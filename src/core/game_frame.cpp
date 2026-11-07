#include "core/game.hpp"
#include "core/game_local.hpp" // V3-10b: the pearl's constants (kPearlTeleportOffset)

#include <algorithm>
#include <cmath>

#include "core/third_person.hpp"
#include "entity/breath_cloud.hpp"
#include "entity/item_light.hpp"
#include "world/beacon.hpp"
#include "world/fire_spread.hpp"
#include "world/growth.hpp"
#include "world/light_job.hpp"
#include "world/noise.hpp"

namespace mc {

namespace {

// Day/night time advances by this fixed amount per frame (not the wall
// clock), so headless runs are byte-reproducible frame for frame; the
// real-time speed is exact at 60 FPS.
constexpr double kDayStepPerFrame = 1.0 / 60.0;

// The fluid simulation ticks every Nth frame (20 ticks/s at 60 FPS); the
// fixed frame step keeps headless runs byte-reproducible.
constexpr int kFluidTickEveryNFrames = 3;

// FLUID-6: the honey viscosity gate — the honey kind simulates once
// every kHoneyTickPeriodFluidTicks fluid ticks (1/3 the rate of water
// and lava = the Minecraft high-viscosity honey; falling and spreading
// are both 1/3 speed under the single gate).
constexpr int kHoneyTickPeriodFluidTicks = 3;

// GEN-9: the farmland-moisture convergence sweep cadence (re-evaluate
// every tracked moisture cell's wetness from the live fluid field and
// re-mesh on a flip; 10 frames = 0.167 s — visually immediate for a
// pour/drain, and the bound on the worker-mesh-staleness the sweep
// exists to fix: a farmland chunk meshed before its water neighbor
// streamed in converges within one sweep once the neighborhood exists).
constexpr int kMoistureRefreshEveryNFrames = 10;

// Fire v2 (Phase 66): the ember emission per live fire cell runs every
// Nth frame (0.1 s = the Minecraft tick rhythm); the fixed frame step
// keeps headless runs byte-reproducible.
constexpr int kFireParticleEveryNFrames = 6;

// Dedicated salt for the fire ember seeds (same discipline as the
// growth/chance-drop salt separation, Phase 45/49): ember emissions
// never track any other deterministic stream in the world.
constexpr std::uint64_t kFireParticleRollSalt = 0xE4B6F1C2D9A83574ULL;

// CRAFT-5: the lit-furnace flame emission runs every Nth frame. N = 36
// (0.6 s) is >2x spawnFire's 0.3-0.5 s max ember life, so at any
// <=60 fps the previous batch is provably dead before the next emission
// (30 frames would sit exactly on the 0.5 s life boundary at 60 fps —
// one run's straggler ember broke the same-side RAW, gate 138). The
// fixed frame step keeps headless runs byte-reproducible.
constexpr int kFurnaceParticleEveryNFrames = 36;

// V3-15: the portal's ambient steam — the emission pass runs every Nth
// frame (N = 6, the fire-ember step: the 0.8-1.4 s steam life spans ~8
// passes, so several steams per portal cell are alive at once = the
// lingering column). The scan is the 8-block box around the player's
// feet (the player's dimension — the portalId is per dimension); each
// portal cell emits on its deterministic roll (1 in 10, the fire's
// seed contract with the dedicated salt).
constexpr int kPortalSteamEveryNFrames = 6;
constexpr std::uint64_t kPortalSteamRollSalt = 0x9C41F2A7E5B60D33ULL;
constexpr int kPortalSteamEmitModulus = 100;
constexpr int kPortalSteamEmitBelow = 10;

// Dedicated salt for the furnace flame seeds (the P66 discipline — the
// furnace stream never tracks the fire-ember stream).
constexpr std::uint64_t kFurnaceParticleRollSalt = 0x4F7A2C9E1B5D8360ULL;

// PRE-8: the burning-mob flame emission cadence — the CRAFT-5 value
// (36 frames, >2x the 0.5 s max ember life: at any <=60 fps the
// previous batch is provably dead before the next emission, so the age-0
// batch is the only live one in any <=900-frame screenshot).
constexpr int kMobFlameEveryNFrames = 36;

// Dedicated salt for the mob-flame seeds (the P66/CRAFT-5 discipline —
// the mob stream never tracks the furnace or the fire-ember stream).
constexpr std::uint64_t kMobFlameRollSalt = 0x6B3E91C4A7F25D08ULL;

// Phase 83 beacon: the refresh DURATION the beacon applies to its effect
// every frame (the fire-source pattern — standing in range resets it, so
// it only matters once the player leaves: the effect then counts down this
// long before expiring). 0.5 s = the Minecraft beacon re-application rhythm.
constexpr double kBeaconEffectDurationSeconds = 0.5;

// Reinterpret an (possibly negative) int as 64 bits for hashing (the
// worldgen's u64 — two's-complement is a bijection).
std::uint64_t asU64(int v) {
    return static_cast<std::uint64_t>(static_cast<std::int64_t>(v));
}

} // namespace

// ---------------------------------------------------------------------------
// V3-9: the per-frame camera sync — the pre-V3-9 update()'s camera block,
// EXTRACTED VERBATIM (the mode's base view: first person the legacy eye
// assignment bit-exactly; third person the orbit resolve at the session's
// zoomed distance; the front view at the flipped angles — the UX-4 /
// V2-1 / V2-19 comments follow the code). update() calls it at the same
// point (the bit-identical normal run); the replay's still frames (the
// pause menu / the finished panel / the observation) call it too so the
// audience orbit stays live without the simulation.
// ---------------------------------------------------------------------------
void Game::syncCamera() {
    // UX-4: first person keeps the legacy assignment bit-exactly (the
    // camera is the eye). Third person orbits kThirdPersonDistance from
    // the eye on the view's own sphere and clamps against the first
    // occluding block face (the pure read-only resolve in
    // core/third_person — the campfire pattern). V2-1: the front view is
    // the SAME resolve executed at the flipped view angles
    // (thirdPersonFrontAngles) — the camera in front of the player,
    // looking at it. The mode is session state (never saved, defaults
    // to First), so a no-input run stays on the legacy path.
    // V3-12b: the camera follows the LOCAL player (the local presentation — each machine views its own player; solo: local == primary, the pre-V3-12b bit-identical value).
    Player* lp = localPlayer();
    m_camera.yaw = lp->yaw;
    m_camera.pitch = lp->pitch;
    if (m_cameraMode == CameraMode::ThirdBack) {
        // 2.0.0: orbit kThirdPersonOrbitHeight ABOVE the eye along the
        // view's up axis (the Minecraft third-person convention) so the
        // screen-center crosshair no longer sits on the player's head.
        // The aim rays still leave from eyePosition() in both modes
        // (UX-8g red line, untouched); the screen-center ray is the
        // view's own forward — parallel to the eye ray, offset by the
        // lift along up_cam = the fixed third-person parallax
        // (documented deviation, the MC third-person look).
        // V2-19: the wheel-zoomed session distance (default
        // kThirdPersonDistance — a no-input run resolves bit-identically;
        // the orbit center is distance-INDEPENDENT — the fixed 0.4 lift —
        // so it keeps its pre-V2-19 signature).
        m_camera.position = resolveThirdPersonCamera(
            currentWorld(), m_blocks,
            thirdPersonOrbitCenter(lp->eyePosition(), lp->yaw,
                                   lp->pitch),
            lp->yaw, lp->pitch, m_thirdPersonDistance);
    } else if (m_cameraMode == CameraMode::ThirdFront) {
        // V2-1: front view = the back camera's resolve run at the flipped
        // view angles (thirdPersonFrontAngles): the camera sits
        // kThirdPersonDistance IN FRONT of the eye, looking at the
        // player (the player's front is visible). The orbit center still
        // uses the PLAYER's angles (up_cam is invariant under the flip —
        // the same 0.4 lift, so the head stays 29-96 px below the screen
        // center as in the back view). The occlusion / fluid / embedded
        // clamps apply symmetrically (the same geometry, mirrored about
        // the orbit center). The aim rays still leave from eyePosition()
        // (UX-8g); the direction is now the camera's own forward = the
        // player's backward, and the reach is extended (aimReach — see
        // the V2-1 note in game_interaction, the MC front-view
        // convention).
        const ThirdPersonAngles fa =
            thirdPersonFrontAngles(lp->yaw, lp->pitch);
        m_camera.yaw = fa.yaw;
        m_camera.pitch = fa.pitch;
        // V2-19: the same wheel-zoomed session distance (the front view
        // zooms identically — one value per session, the design's
        // single-value sharing).
        m_camera.position = resolveThirdPersonCamera(
            currentWorld(), m_blocks,
            thirdPersonOrbitCenter(lp->eyePosition(), lp->yaw,
                                   lp->pitch),
            fa.yaw, fa.pitch, m_thirdPersonDistance);
    } else {
        m_camera.position = lp->eyePosition(); // First: legacy
    }

    // V3-9: the AUDIENCE orbit (replay only, RENDER-ONLY): the
    // observation view = the mode's base angles + the orbit offset the
    // audience's mouse / gamepad sticks accumulate, the camera
    // re-resolved around the eye at the offset angles (the third-
    // person geometry — the single trig source; the session's zoomed
    // distance, the base's own). A ZERO orbit skips the branch
    // entirely (the pre-V3-9 camera path, bit-exactly — the First's
    // legacy eye, the Third* base resolves). The orbit never touches
    // the simulation: the aim rays read aimForward (the player's base
    // view), the offset is session state (never saved), the player's
    // yaw / pitch stay the demo's.
    if (m_demoMode == DemoMode::Replay &&
        (m_demoOrbitYaw != 0.0f || m_demoOrbitPitch != 0.0f)) {
        const float oyaw = m_camera.yaw + m_demoOrbitYaw;
        const float opitch = std::clamp(
            m_camera.pitch + m_demoOrbitPitch, -kAudienceOrbitMaxPitch,
            kAudienceOrbitMaxPitch);
        m_camera.position = resolveThirdPersonCamera(
            currentWorld(), m_blocks,
            thirdPersonOrbitCenter(lp->eyePosition(), oyaw, opitch),
            oyaw, opitch, m_thirdPersonDistance);
        m_camera.yaw = oyaw;
        m_camera.pitch = opitch;
    }
}

void Game::update(double dt) {
    // V3-12b (R2): the shared sim clock's advance (first of the frame —
    // every double-tap window's read sees this frame's dt; solo: an
    // unread accumulator, the bit-level red line).
    m_simTime += dt;
    // Chunk streaming (Phase 14) + worker meshing (PIPE-1) — frame
    // pipeline, the race-free part: every worker job from the previous
    // frame (generation, mesh, lighting) completes at this flush; from
    // here until the posts at the end of this function the main thread is
    // the sole writer of the World, and after the posts the main thread
    // only reads it (the mesh COMPUTE already moved to the workers —
    // updateChunkMeshes below does the GL upload only; rendering reads no
    // World data beyond the chunk list).
    // integrateMeshJobs runs BEFORE integrateStreamedColumns: a stream
    // insert re-marks neighbor columns dirty, and that re-dirtying must
    // survive the mesh integrate's clear-dirty (see integrateMeshJobs).
    // V3-1 Phase B: the drain covers BOTH dimensions (the overworld's
    // in-flight jobs + the nether's — at most one frame's worth of the
    // source dimension's jobs linger after a travel, then its sets stay
    // empty and every drain fast-exits: a no-nether session runs the
    // exact pre-Phase-B sequence, the bit-identical red line). The
    // unload pass runs for the ACTIVE dimension only (the player's
    // coordinates are its space; the frozen dimension's disk is
    // neither added to nor subtracted from — zero post, zero churn).
    if (m_streaming.pool) {
        m_streaming.pool->flush();
    }
    collectLitColumns(overworldCtx());
    integrateMeshJobs(overworldCtx());
    integrateStreamedColumns(overworldCtx());
    if (m_nether.world) {
        collectLitColumns(netherCtx());
        integrateMeshJobs(netherCtx());
        integrateStreamedColumns(netherCtx());
    }
    // V3-10a: the End's drain (the nether's block verbatim — at most one
    // frame's worth of the source dimension's jobs linger after a
    // travel, then every drain fast-exits; a no-End session runs the
    // exact pre-V3-10a sequence, the bit-identical red line).
    if (m_end.world) {
        collectLitColumns(endCtx());
        integrateMeshJobs(endCtx());
        integrateStreamedColumns(endCtx());
    }
    DimCtx activeCtx =
        playerInNether() ? netherCtx()
        : playerInEnd() ? endCtx() : overworldCtx();
    unloadFarColumns(activeCtx);

    // Day/night: a fixed per-frame step (see kDayStepPerFrame) keeps
    // headless runs byte-reproducible. V3-1 Phase B: the day cycle +
    // weather are GLOBAL (the single instance keeps advancing in every
    // dimension — the Minecraft time-continuity semantics, the design's
    // D7 ruling: the nether's render simply does not consume them).
    m_dayCycle.update(kDayStepPerFrame);

    // V2-4: the weather state machine (the fixed 1/60 frame step, the
    // day-cycle class — hard rule 8): the frame-locked CLEAR/RAIN
    // segments + the thunder schedule (the pure logic in core/weather,
    // the unit tests own it). Runs FIRST — before every raining consumer
    // (the campfire put-out, the crop growth, the fire put-out, the
    // player's fire-effect extinguish, the mobs' setRaining feed).
    // render.weather OFF = this never runs (the state machine does not
    // run + zero drops + zero interaction = the v2.3.0 pipeline bit-
    // identical).
    updateWeather();

    // UX-7 / V2-3: the tutorial completion-toast countdown (the
    // frame-locked class — the kTutorialToastFrames window armed by the
    // done flip in emitTutorialEvent or the T skip of the LAST series,
    // decremented once per frame; the render draws while > 0). The
    // decrement runs BEFORE this frame's interaction events, so a toast
    // armed by an event later in this same update still draws this
    // frame (120 frames total). A no-input run never arms it (zero
    // events — the structural immunity).
    // V3-12b: the ×N pass (the per-player toast countdown — solo: one
    // session, the pre-V3-12b decrement bit-identical).
    for (std::size_t i = 0; i < m_sessions.size(); ++i) {
        PlayerSession& s = *m_sessions[i];
        if (s.tutorialToastFrames > 0) {
            --s.tutorialToastFrames;
        }
    }

    // V3-1 Phase B (D3 time-capsule ruling): the overworld's world pass
    // (the per-block states + the growth family) is FROZEN while the
    // player is in the nether — the inactive dimension ticks nothing,
    // its state is a pure function of its last active frame (the S2
    // return-leg gate's "OW savenorm ≡ pre-entry except the global
    // time" rests on it; the nether's twin pass runs in the nether
    // section below). In the overworld (the default) this is the exact
    // pre-Phase-B sequence — the bit-identical red line. V3-10a: the
    // End freezes it too (the triple-capsule — the nether AND the
    // overworld freeze while the player is in the End; neither has a
    // world pass of its own, the End's frozen content is the frame's).
    if (!playerInNether() && !playerInEnd()) {
        updateOverworldWorldPass();
    }

    // Phase 15 UI keys: F3/F5 work in any pointer state; E opens the
    // inventory (releasing the pointer) or closes it.
    handleUiKeys();
    // While the inventory is open, player physics and block interaction
    // are paused; the world keeps ticking (day cycle, fluids, entities,
    // streaming) — see docs/KNOWN_ISSUES.
    // V3-12b: the ×N input pass (the per-player panel OR interaction —
    // solo: one player, the pre-V3-12b sequence bit-identical; the
    // panel gate is the player's OWN panel, the D11's remote-no-panel
    // falls out of the loop).
    for (std::size_t i = 0; i < m_players.size(); ++i) {
        Player& p = *m_players[i];
        PlayerSession& s = *m_sessions[i];
        const Input& in = inputFor(i);
        if (s.panel.openPanel != OpenPanel::None) {
            handlePanelClicks(p, s, in);
        } else {
            handleInteraction(p, s, in, dt);
        }
    }

    // V3-14d: the ride pass (the dismount conditions + the carry snap +
    // the steer push) — AFTER the input handlers (a Space / LeftControl
    // press or a panel-open dismounts THIS frame, before the player's
    // update) and BEFORE the entities' update (the steer is pushed
    // before the mount's own update). One null check in every no-ride
    // session (the structural-immunity root).
    updateRide();

    // V3-10b: the pearl's throw cooldown (the frame-locked bookkeeping
    // — one decrement per frame; a no-throw session keeps it 0, the
    // structural-immunity root, the portal-cooldown's shape).
    // V3-12b: the ×N pass (the per-player pearl cooldown — solo: one
    // session, the pre-V3-12b decrement bit-identical).
    for (std::size_t i = 0; i < m_sessions.size(); ++i) {
        PlayerSession& s = *m_sessions[i];
        if (s.pearlThrowCooldownFrames > 0) {
            --s.pearlThrowCooldownFrames;
        }
    }

    // V3-1 Phase B (D3 time-capsule ruling, the section-1 twin): the
    // overworld's fluid / fire / moisture pass is FROZEN in the nether;
    // the nether's twin (the fluid tick over the static source lava sea
    // — zero churn, the recon's risk-surface ③ resolution) runs
    // instead. V3-10a: the End's twin (10a's worldgen holds no fluid —
    // a player-poured bucket settles the same way, the static zero-
    // churn discipline; the evaporation rule stays nether-scoped).
    if (playerInNether()) {
        updateNetherFluidTick();
    } else if (playerInEnd()) {
        updateEndFluidTick();
    } else {
        updateOverworldFluidPass(dt);
    }

    // Column-merged GL upload (PIPE-1: the mesher's compute runs on the
    // workers — postMeshJobs / integrateMeshJobs feed the dimension's
    // chunk mesh data; this frame's integrates' clear-dirty is the only
    // input here). V3-1 Phase B: BOTH dimensions upload (the fast exit
    // on the empty pending set keeps a no-nether session at the exact
    // pre-Phase-B cost).
    {
        auto meshes = updateChunkMeshes(overworldCtx());
        if (!meshes.ok()) {
            MC_LOG_ERROR("Chunk meshing failed: " + meshes.error().describe());
        }
        if (m_nether.world) {
            meshes = updateChunkMeshes(netherCtx());
            if (!meshes.ok()) {
                MC_LOG_ERROR("Chunk meshing failed: " +
                             meshes.error().describe());
            }
        }
        // V3-10a: the End's upload (the nether's block verbatim — the
        // fast exit on the empty pending set keeps a no-End session at
        // the exact pre-V3-10a cost).
        if (m_end.world) {
            meshes = updateChunkMeshes(endCtx());
            if (!meshes.ok()) {
                MC_LOG_ERROR("Chunk meshing failed: " +
                             meshes.error().describe());
            }
        }
    }

    // The player owns the first-person state; the camera is just its view
    // (eye position + orientation). Mouse look is only active while the
    // pointer is captured, so an unlocked cursor never rotates the view.
    // Paused while the inventory is open (Phase 15) or the player is dead
    // (Phase 16: the death screen freezes the body; R respawns).
    // V3-12b: the ×N physics pass (the per-player body — solo: one
    // player, the pre-V3-12b sequence bit-identical; the tutorial's
    // walk/jump events anchor the PRIMARY per D12).
    for (std::size_t i = 0; i < m_players.size(); ++i) {
        Player& p = *m_players[i];
        PlayerSession& s = *m_sessions[i];
        if (s.panel.openPanel != OpenPanel::None || p.isDead()) {
            continue;
        }
        // UX-7: the tutorial walk / jump events — pure comparisons of
        // the player's own state across this frame's update (frame-
        // locked, no real-dt, hard rule 8):
        //   - walk: the frame-boundary HORIZONTAL displacement (the
        //     ENT-1 swing's move rule: x or z strictly differs between
        //     frames — one event per moved frame at most);
        //   - jump: the JUMP IMPULSE GRANT frame — the velocity's rise
        //     out of a grounded, non-fluid frame (the on-ground Space
        //     branch sets velocity.y = jumpSpeed; the in-fluid Space is
        //     the swim-up, not a jump — the inWater/inLava tick-state
        //     gate; a knockback lift applied BEFORE the player update
        //     is already in the baseline, so it never fakes a rise).
        const Vec3 tutPosBefore = p.position();
        const float tutVyBefore = p.velocity().y;
        const bool tutGroundedBefore = p.onGround();
        // R18: the pointer-lock SIM mirror — solo syncs from the SDL
        // state (the original read's value, bit-identical); net reads
        // the FrameState-carried flag (the local fact in the sim).
        if (m_players.size() == 1) {
            s.inputLocked = m_window.relativeMouseMode();
        }
        p.update(dt, inputFor(i), s.inputLocked);
        if (i == 0) {
            const Vec3 tutPosAfter = p.position();
            if (tutPosAfter.x != tutPosBefore.x ||
                tutPosAfter.z != tutPosBefore.z) {
                emitTutorialEvent("walk", 1);
            }
            if (tutGroundedBefore && p.velocity().y > tutVyBefore &&
                !p.inWater() && !p.inLava()) {
                emitTutorialEvent("jump", 1);
            }
        }
    }
    // V3-1 Phase A: the portal entry countdown (the frame-locked class —
    // the pure step is core/portal's portalEntryStep, the unit tests own
    // it). Runs in EVERY pointer/panel state: the player's physics pauses
    // behind a panel, but a panel opened mid-countdown auto-CLOSES (the
    // Minecraft portal rule — the GUI cannot stay open in a portal), so
    // the section must see the player even while the panel is up. A
    // no-input run without portals never arms it (the structural-immunity
    // root: the AABB overlap is the only trigger, worldgen places no
    // portal blocks — the stateful worldgen rejection).
    updatePortalEntry();
    // V3-10a: the End portal's entry countdown (the updatePortalEntry's
    // twin — the player's AABB over an end_portal cell in the CURRENT
    // dimension arms the same 60-frame step, the SHARED cooldown; a
    // no-end-portal run never arms it, the structural-immunity root).
    updateEndPortalEntry();
    // V3-10d: the dragon boss's gameplay pass (the spawn schedule / the
    // crystal census + fire + heal / the contact + cloud damage / the
    // ascension ritual — the updateEndPortalEntry's dimension twin,
    // one playerInEnd() gate = the no-End session's structural root).
    updateDragonGameplay();
    // UX-4 / V2-1 / V2-19: the per-frame camera sync (the mode's base
    // view — extracted VERBATIM into syncCamera(); the V3-9 replay's
    // still frames call it too for the live audience orbit).
    syncCamera();

    // SURV-2: the oxygen / drowning pass (the frame-locked class — driven
    // by m_frameCount, not dt, like the day cycle; the pure step + the
    // head predicate live in player/drown, the unit tests own them — the
    // campfire pattern). Runs in every pointer/panel state (the player's
    // physics pauses behind a panel, the air does not — the world keeps
    // ticking, the Minecraft behavior); after a death it is naturally
    // inert (Player::damage no-ops on the frozen body, and the death
    // transition in updateSurvival refills the state for the respawn).
    for (std::size_t i = 0; i < m_players.size(); ++i) {
        Player& p = *m_players[i];
        PlayerSession& s = *m_sessions[i];
        const float dmg = drownStep(s.drown, headInFluid(p, currentWorld()),
                                    static_cast<unsigned>(m_frameCount));
        if (dmg > 0.0f) {
            // Drowning BYPASSES the armor reduction (the wiki damage
            // table: Drowning = "Armor: No effect"); the resistance
            // effect and the absorption pool still apply (damage()'s
            // pipeline, the bypass flag skips the armor step only).
            p.damage(dmg, /*bypassArmor=*/true,
                     {Player::DamageSource::Drown, {}});
        }
    }

    // Survival events (Phase 16): poll the player's health/death state —
    // a drop drives the hurt flash + sound, a death releases the pointer,
    // R (while dead) respawns. Runs even while paused so the flash decays.
    updateSurvival(dt);
    // Phase 83 beacon: re-derive every beacon's power level and refresh the
    // NAMED effect on the player while in range (BEFORE the effect ticks,
    // so the absorption pool — a pure function of the effect total at the
    // sync points inside updateEffects — sees the beacon's freshly applied
    // layer this frame, the fire-source refresh pattern). No-op without a
    // beacon (structural immunity).
    if (!playerInNether()) {
        updateBeacons();
    }
    // Status-effect ticks (Phase 46): the real-dt durations + the periodic
    // damage/heal (see updateEffects). Runs after the survival poll so a
    // lethal tick's death is handled on the NEXT frame's poll (a poison
    // tick cannot kill — its min_health floor — so the one-frame delay is
    // only reachable by a future floor-less damage effect).
    updateEffects(dt);
    // PRE-4 ambience: the frame-locked day/night/cave ambient sound (the
    // roll once every kAmbienceIntervalFrames — audio outside the frame
    // state, so this touches no screenshot/save). Runs in every
    // pointer/panel state (the world keeps ticking — the ambience with
    // it, the Minecraft behavior).
    updateAmbience();
    // V3-1 Phase A: the portal ambient sound (the visible-portal per-
    // frame roll, the bat squeak's cadence — the same runs-in-every-state
    // ambience discipline). A no-input run without portals never arms it
    // (the tracking set is empty — the structural-immunity root).
    updatePortalAmbience();

    // Other entities (Phase 10): per-entity physics, then item merging and
    // despawn (see updateEntities).
    updateEntities(dt);

    // Break particles (Phase 15) + the footstep sound timer.
    m_particles.update(dt);
    updateSoundTimers(dt);
    // SAVE-1: the auto-save pass (real dt, the survival-timer class).
    // Runs in the frame's sole-World-writer window — the F5 save's
    // precedent (handleUiKeys above) — AFTER every state update of this
    // frame, so the snapshot is the frame's settled state. Structurally
    // inert without a --save slot or with the interval off (0).
    updateAutoSave(dt);

    // Streaming decision + generation jobs: posted after every
    // main-thread World write of this frame (interaction, fluids) and
    // after meshing. The workers run from here until the next flush while
    // the main thread only reads the World (rendering); generation jobs
    // touch no shared state at all (pure generator into the job's own
    // scratch chunks).
    streamChunks(activeCtx);
    // Worker meshing (PIPE-1): snapshot + post for the nearest eligible
    // dirty columns. AFTER streamChunks and BEFORE postLightJobs — the
    // post order gen -> mesh -> light keeps this frame's lighting job the
    // only live-World writer after the snapshots (see postMeshJobs).
    // V3-1 Phase B: the ACTIVE dimension only (the frozen dimension's
    // dirty set does not grow — its world is a time capsule, so there
    // is nothing to mesh; the posts that matter are the player's).
    postMeshJobs(activeCtx);
    // Lighting for chunks that appeared this frame (streamed-in columns,
    // or the placement path's synchronous creation): up to two worker
    // jobs (99.10) — two bit-level-independent column boxes run in
    // parallel (planLightJobs), a dependent pair stays one job with the
    // boxes sequential (the pre-99.10 behavior). The affected chunks
    // render one frame dark (light 0) and are re-meshed with the correct
    // light the frame after the jobs complete — deterministic,
    // frame-locked by the flush.
    postLightJobs(activeCtx);
    // 99.11: the fluid catch-up light jobs — the budgeted worker drain
    // of m_fluidLightPending (filled by the fluid tick above; the old
    // synchronous bounding-box re-light there was the frame's entire
    // cost in the shipped ocean, step-0: 130..661 s per 300-frame
    // window). LAST of the posts: it reads the streaming job columns of
    // this window from m_streaming.lightJobs for the cross-kind exclusion, and
    // posts nothing while a candidate is not lightRegionsIndependent of
    // them (the 99.10 parallel safety is per-plan). The reap + the
    // dirtyOut application land at the next frame's collectLitColumns —
    // the fluid columns' chunks re-mesh with the new light then (99.4).
    postFluidLightJobs(activeCtx);
}

// ---------------------------------------------------------------------------
// Pointer lock + block interaction (Phase 6)
// ---------------------------------------------------------------------------

void Game::updateFires(double dt) {
    if (m_states.fires.empty()) {
        return;
    }
    // std::map iteration is position-sorted: the put-out order is
    // deterministic. The countdown is a REAL-dt timer (hard rule 8, the
    // survival-timer class — like the player's fire effect): a headless
    // run at a fixed frame rate is byte-reproducible, and the burn-out
    // moment is wall-clock-stable.
    std::vector<BlockPos> putOut;
    for (auto& [pos, remaining] : m_states.fires) {
        remaining -= dt;
        if (remaining <= 0.0) {
            putOut.push_back(pos);
            continue;
        }
        // Extinguish (Phase 64): the shared fire-family rule (world/
        // fire_spread) — a WATER fluid (any level) in one of the 6
        // neighbors puts the fire out instantly, water beats fire (the
        // Phase 57 water-beats-lava rule). Lava does not extinguish: the
        // fire beside the pool is the lava's product and keeps burning
        // (Minecraft — fire and lava coexist). V2-4: the rain disjunct
        // (the rain wets every fire, no roof-shade check = the documented
        // deviation — off/clear = false = the water-only rule bit-
        // identical).
        if (extinguishedByWaterOrRain(m_world, pos, m_weather.raining)) {
            putOut.push_back(pos);
        }
    }
    // Apply (AFTER the iteration — the setBlock calls below erase the
    // entries through the choke point; never iterate a map while it is
    // being structurally modified). Phase 66 (Fire v2): each death
    // (burn-out or water extinguish) spawns one break-style burst of the
    // FIRE texture — the puff of a dying fire (the Phase 65 cross-branch
    // fix renders it). The seed rides the shared per-break counter, so an
    // identical input sequence gets an identical burst.
    for (const BlockPos& pos : putOut) {
        setBlock(pos.x, pos.y, pos.z, BlockID::air());
        Lighting::updateLocal(m_world, m_blocks, pos);
        const Chunk* chunk = m_world.findChunk(worldToChunk(pos));
        float sky = 0.0f;
        float blockLight = 1.0f; // the fire's own emission dominates
        if (chunk != nullptr) {
            const BlockPos local =
                worldToChunkLocal(worldToChunk(pos), pos);
            sky = static_cast<float>(chunk->skyLightAt(local.x, local.y,
                                                       local.z)) /
                  15.0f;
            blockLight = static_cast<float>(
                             chunk->blockLightAt(local.x, local.y, local.z)) /
                         15.0f;
        }
        const std::uint64_t rngSeed =
            m_generator->seed() +
            (++m_particleBreakCounter) * 0x9E3779B97F4A7C15ULL;
        m_particles.spawnBreak(pos.x, pos.y, pos.z, m_states.fireId, sky, blockLight,
                               rngSeed);
    }
    // Phase 66 (Fire v2): the ember emission. FIXED-frame step (hard rule
    // 8, the day-cycle class — the spawn is a pure function of the world
    // seed, the cell and the frame; the motion then integrates the real
    // dt like every other particle, so a run with LIVE embers is
    // screenshot-identical only after they have died — the E2E scene
    // either waits that out or uses the fire.particles control). Only
    // fires that SURVIVED this frame's countdown emit; the put-outs got
    // their death burst above. Position-sorted map iteration ->
    // deterministic emission order.
    if (m_states.fireParticles && m_frameCount % kFireParticleEveryNFrames == 0) {
        for (const auto& [pos, remaining] : m_states.fires) {
            (void)remaining;
            const Chunk* chunk = m_world.findChunk(worldToChunk(pos));
            float sky = 0.0f;
            float blockLight = 1.0f; // the fire's own emission dominates
            if (chunk != nullptr) {
                const BlockPos local =
                    worldToChunkLocal(worldToChunk(pos), pos);
                sky = static_cast<float>(
                          chunk->skyLightAt(local.x, local.y, local.z)) /
                      15.0f;
                blockLight = static_cast<float>(
                                 chunk->blockLightAt(local.x, local.y,
                                                     local.z)) /
                             15.0f;
            }
            // The seed advances with the frame (PITS #4: splitmix64 is a
            // pure finalizer) and is per-cell via the position hash (a
            // dedicated salt — the emission stream never tracks any other
            // deterministic roll in the world).
            const std::uint64_t rngSeed =
                m_generator->seed() +
                hash3(asU64(pos.x), asU64(pos.y), asU64(pos.z),
                      kFireParticleRollSalt) +
                static_cast<std::uint64_t>(m_frameCount) *
                    0x9E3779B97F4A7C15ULL;
            m_particles.spawnFire(pos.x, pos.y, pos.z, m_states.fireId, sky,
                                  blockLight, rngSeed);
        }
    }
}

void Game::updateFurnaceFlames() {
    // CRAFT-5: the lit-furnace mouth flames — rising ember batches (the
    // P66 spawnFire reused: 2 particles, negative gravity, 0.3-
    // 0.5 s life) emitted from the cell ABOVE the furnace block, every
    // kFurnaceParticleEveryNFrames frame (fixed frame step, hard rule 8 —
    // the spawn is a pure function of the world seed, the position and
    // the frame). The gate is (m_frameCount + 1) % N because the frame
    // counter increments AFTER update/render: a 900-frame run renders
    // its last frame with m_frameCount = 899 (900 % 36 = 0), so the
    // screenshot carries that frame's age-0 batch (deterministic at any
    // fps) while the previous batch (36 frames = 0.6 s at 60 fps > the
    // 0.5 s max life) is already dead. Motion then integrates the real
    // dt like every other particle. render.furnace_flames off (or no
    // furnaces, or no fire block for the texture) = zero emissions =
    // bit-identical to v1.22.0. Position-sorted map iteration ->
    // deterministic order.
    if (!m_states.furnaceFlames || m_states.furnaces.empty() || !m_states.fireId.valid() ||
        (m_frameCount + 1) % kFurnaceParticleEveryNFrames != 0) {
        return;
    }
    for (const auto& [pos, st] : m_states.furnaces) {
        if (st.fuelSubsteps <= 0) {
            continue; // unlit: no flame
        }
        // The embers live in the AIR cell above the furnace (spawnFire
        // centers there): read the light of THAT cell — the furnace cell
        // itself is opaque and self-dark (no light_emission), which
        // would bake invisible black embers.
        const BlockPos above {pos.x, pos.y + 1, pos.z};
        const Chunk* chunk = m_world.findChunk(worldToChunk(above));
        float sky = 0.0f;
        float blockLight = 1.0f; // a burning furnace glows through its
                                 // mouth — the P66 fallback
        if (chunk != nullptr) {
            const BlockPos local =
                worldToChunkLocal(worldToChunk(above), above);
            sky = static_cast<float>(
                      chunk->skyLightAt(local.x, local.y, local.z)) /
                  15.0f;
            blockLight = static_cast<float>(
                             chunk->blockLightAt(local.x, local.y,
                                                 local.z)) /
                         15.0f;
        }
        const std::uint64_t rngSeed =
            m_generator->seed() +
            hash3(asU64(pos.x), asU64(pos.y), asU64(pos.z),
                  kFurnaceParticleRollSalt) +
            static_cast<std::uint64_t>(m_frameCount) *
                0x9E3779B97F4A7C15ULL;
        m_particles.spawnFire(pos.x, pos.y + 1, pos.z, m_states.fireId, sky,
                              blockLight, rngSeed);
    }
}

void Game::updatePortalSteam() {
    // V3-15: the nether portal's ambient steam — the rising
    // translucent cubes (the portal block's own texture routes the
    // particle to the blend pass, the translucent-box precedent).
    // Frame-locked (hard rule 8): the pass step, the box and the roll
    // are pure functions of (seed, cell, frame) — an identical run
    // emits identical steams.
    if (!m_states.portalSteam ||
        m_frameCount % kPortalSteamEveryNFrames != 0 ||
        !m_states.portalId.valid()) {
        return;
    }
    const World& world = currentWorld();
    const Vec3 f = m_player->position();
    const int fx = static_cast<int>(std::floor(f.x));
    const int fy = static_cast<int>(std::floor(f.y));
    const int fz = static_cast<int>(std::floor(f.z));
    // The y range is asymmetric (feet-4 .. feet+5): the portal's top
    // frame row stands 4 above the ground at the portal base, and the
    // steam reads from the whole surface, not the player's eye line.
    for (int x = fx - 4; x < fx + 4; ++x) {
        for (int z = fz - 4; z < fz + 4; ++z) {
            for (int y = fy - 4; y < fy + 6; ++y) {
                if (world.getBlock(x, y, z) != m_states.portalId) {
                    continue;
                }
                // The deterministic roll (the fire's seed contract —
                // the dedicated salt never tracks another stream).
                const std::uint64_t roll =
                    m_generator->seed() +
                    hash3(asU64(x), asU64(y), asU64(z),
                          kPortalSteamRollSalt) +
                    static_cast<std::uint64_t>(m_frameCount) *
                        0x9E3779B97F4A7C15ULL;
                if (static_cast<int>(roll % kPortalSteamEmitModulus) >=
                    kPortalSteamEmitBelow) {
                    continue;
                }
                const Chunk* chunk =
                    world.findChunk(worldToChunk({x, y, z}));
                float sky = 0.0f;
                float blockLight = 1.0f; // the portal's own light 11
                if (chunk != nullptr) {
                    const BlockPos local =
                        worldToChunkLocal(worldToChunk({x, y, z}),
                                          {x, y, z});
                    sky = static_cast<float>(
                              chunk->skyLightAt(local.x, local.y,
                                                local.z)) /
                          15.0f;
                    blockLight = static_cast<float>(
                                     chunk->blockLightAt(local.x,
                                                         local.y,
                                                         local.z)) /
                                 15.0f;
                }
                m_particles.spawnSteam(x, y, z, m_states.portalId, sky,
                                       blockLight, roll);
            }
        }
    }
}

void Game::updateMobFlames() {
    // PRE-8: the burning-mob flames — one rising ember batch per BURNING
    // mob (MobEntity::burning = the P57/P73 fire system's state: a mob in
    // the lava or strictly overlapping a fire-block cell re-ignites every
    // step; the lingering burn after leaving counts down), every
    // kMobFlameEveryNFrames frame (fixed frame step, hard rule 8). The
    // (m_frameCount + 1) % N gate is the CRAFT-5 boundary lesson: a
    // 900-frame run renders its last frame with m_frameCount = 899
    // (900 % 36 = 0), so the screenshot carries that frame's age-0 batch
    // while the 36-frame-old predecessor (0.6 s > the 0.5 s max life) is
    // already dead — the 30-frame variant sat on the life boundary and a
    // straggler ember broke the same-side RAW (gate 138). The spawn is a
    // pure function of the world seed, the mob's cell and the frame, so
    // identical runs emit identical particles; motion then integrates
    // the real dt like every other particle (the live-particle class —
    // E2E compares the OFF side and the cluster bounds, never the ON
    // side's bytes, the P66 discipline).
    //
    // The batch is SELF-LIT: a burning mob is engulfed in its own fire,
    // so the embers take block light 1.0 (visible at deep night —
    // Minecraft's look) with the center cell's SKY as the ambient term.
    // CRAFT-5's furnace-mouth embers instead live in the AIR cell above
    // the furnace and are environment-lit (the documented difference:
    // a furnace's flame pokes out, a mob's flame covers it).
    //
    // render.mob_flames off (or no burning mob, or no fire block for the
    // texture) = zero emissions = bit-identical to v1.28.0 (the
    // structural immunity: the no-input fresh baseline never burns a
    // mob). The m_entities vector order is the run's own, so the batch
    // sequence is deterministic for identical runs.
    if (!m_states.mobFlames || !m_states.fireId.valid() ||
        (m_frameCount + 1) % kMobFlameEveryNFrames != 0) {
        return;
    }
    for (const auto& entity : m_entities) {
        const MobEntity* mob =
            dynamic_cast<const MobEntity*>(entity.get());
        if (mob == nullptr || !mob->burning()) {
            continue;
        }
        // The AABB-center cell (the PRE-6 entity-cell convention,
        // hard rule 1 floors; the World getter's documented fallback
        // covers out-of-range / never-created chunks).
        const BlockPos center =
            entityCenterCell(mob->position(), mob->height());
        // Self-lit: the combustion is the block term (1.0 — the embers
        // stay bright at deep night, Minecraft's look); the center cell's
        // sky is the ambient term.
        const float sky =
            static_cast<float>(m_world.getSkyLight(center.x, center.y,
                                                   center.z)) /
            static_cast<float>(kMaxLight);
        const std::uint64_t rngSeed =
            m_generator->seed() +
            hash3(asU64(center.x), asU64(center.y), asU64(center.z),
                  kMobFlameRollSalt) +
            static_cast<std::uint64_t>(m_frameCount) *
                0x9E3779B97F4A7C15ULL;
        m_particles.spawnFire(center.x, center.y, center.z, m_states.fireId, sky,
                              1.0f, rngSeed);
    }
}

void Game::updateFireSpread() {
    // Phase 66 (Fire v2): the spread roll runs every
    // fire.spread_interval_frames frame (the fixed frame step, hard rule
    // 8) — no fires, no fire block (a stripped asset set), or a chance of
    // 0 all fast-exit inside fireSpreadIgnitions.
    if (m_states.fires.empty() || !m_states.fireId.valid() ||
        m_frameCount % m_states.fireSpreadInterval != 0) {
        return;
    }
    // Position-ordered fire list (the m_states.fires map keys, std::map order).
    std::vector<BlockPos> firePositions;
    firePositions.reserve(m_states.fires.size());
    for (const auto& [pos, remaining] : m_states.fires) {
        (void)remaining;
        firePositions.push_back(pos);
    }
    const std::vector<BlockPos> ignitions = fireSpreadIgnitions(
        m_world, m_blocks, firePositions, m_generator->seed(),
        static_cast<std::uint64_t>(m_frameCount), m_states.fireSpreadChance);
    // Apply (AFTER the roll — setBlock erases nothing here, but it
    // EMPLACES the new fires' full lives through the choke point; never
    // feed a map into a pure function while mutating it). No sound: a
    // spreading fire line would otherwise spam the one-shot "ignite"
    // (documented Fire v2 scope).
    for (const BlockPos& pos : ignitions) {
        setBlock(pos.x, pos.y, pos.z, m_states.fireId);
        Lighting::updateLocal(m_world, m_blocks, pos);
    }
}

void Game::updateBeacons() {
    // Phase 83: re-derive the power level of every tracked beacon and
    // refresh its NAMED effect on the player(s) in range.
    // V3-12b: the ×N pass (each player in range gets the effects —
    // solo: one player, the pre-V3-12b sequence bit-identical). The
    // level is a pure function of the world (world/beacon) — no stored
    // state. A no-op (fast-exit) while the world holds no beacon (the
    // structural immunity: a fresh no-input world has no beacon, so
    // m_states.beacons is empty and this never runs its body).
    m_states.beaconActive.clear();
    if (m_states.beacons.empty()) {
        return;
    }
    for (std::size_t i = 0; i < m_players.size(); ++i) {
        Player& p = *m_players[i];
        PlayerSession& s = *m_sessions[i];
        if (p.isDead()) {
            continue;
        }
        // The absorption pool is a pure function of the effect total at
        // the sync points. updateEffects only detects a change if the
        // total moves DURING its own ticks; the beacon applies its
        // effect HERE (before updateEffects), so capture the total
        // around the applies and sync the pool if the beacon raised it
        // (the fire-source pattern: standing in range keeps the layer at
        // its full total; damage still drains it between sync points,
        // Phase 72).
        const float absorbBefore = s.effects.totalAbsorption(m_effects);
        const Vec3 feet = p.position();
        for (const auto& [pos, bid] : m_states.beacons) {
            const int level = beaconLevel(m_world, m_blocks, pos.x, pos.y, pos.z);
            if (level < 1) {
                continue;
            }
            // An active beacon feeds the beam renderer (drawn in the
            // blending pass; the beam is gated on the `beacon.beam`
            // config there).
            m_states.beaconActive.push_back(
                BeaconBeam {pos.x, pos.y, pos.z, level});
            // In range? Chebyshev distance from the player's feet to the
            // beacon cell's centre (the voxel-cube "within N blocks"
            // rule).
            const float dx = feet.x - (pos.x + 0.5f);
            const float dy = feet.y - (pos.y + 0.5f);
            const float dz = feet.z - (pos.z + 0.5f);
            const float dist = std::max(std::fabs(dx),
                                        std::max(std::fabs(dy),
                                                 std::fabs(dz)));
            if (dist > static_cast<float>(m_states.beaconRange)) {
                continue;
            }
            // Apply the beacon's EFFECT LIST (Phase 91; each name is
            // init-validated to exist). The duration of every effect
            // refreshes each frame while in range (the fire-source
            // pattern — standing in the beam keeps the effects alive).
            // A single-effect beacon (the Phase 83 `beacon_effect`) is a
            // 1-element list.
            for (const std::string& effName :
                 m_blocks.at(bid).properties.beaconEffects) {
                const int eff = m_effects.idForName(effName);
                if (eff != -1) {
                    s.effects.apply(eff, kBeaconEffectDurationSeconds);
                }
            }
        }
        if (s.effects.totalAbsorption(m_effects) != absorbBefore) {
            syncAbsorptionFor(p, s);
        }
    }
}

void Game::updateAmbience() {
    // PRE-4: the day/night/cave ambience (the frame-locked schedule,
    // world/ambience). audio.ambience (code default true, the render.fog
    // getBool precedent — config.json is untouched) gates the pass; OFF =
    // zero events, zero sounds (the gate's "new-off" control, the
    // v0.27.0 pipeline bit-identical).
    if (!m_config.getBool("audio.ambience", true)) {
        return;
    }
    // The roll cadence: once every kAmbienceIntervalFrames (the
    // m_frameCount % N gate — the fire.spread_interval_frames pattern;
    // a pure function of the frame, hard rule 8).
    if (m_frameCount % kAmbienceIntervalFrames != 0) {
        return;
    }
    // The context (world/ambience's pure decision): the player's FEET
    // cell (position = the AABB bottom-center, the floor on each axis) —
    // raw sky light 0 = no sky path = cave (priority over day/night, the
    // Minecraft "no sky" rule); otherwise the renderer's day/night split
    // (the dayFactor's 0.5 cutoff). T4 (v3.7.0): the nether is its own
    // context at the TOP of the priority (a nether cave is a nether —
    // the recorded deviation 5); the overworld (inNether = false) is
    // the pre-T4 decision, bit-identical (the unit-test pin).
    const Vec3 feet = m_player->position();
    const int cx = static_cast<int>(std::floor(feet.x));
    const int cy = static_cast<int>(std::floor(feet.y));
    const int cz = static_cast<int>(std::floor(feet.z));
    // V3-1 Phase B: the CURRENT world (the nether's skyless void reads
    // raw sky 0 — the nether context (T4) owns the ambience now, the
    // cave bed is the overworld's no-sky rule only).
    const int rawSky = currentWorld().getSkyLight(cx, cy, cz);
    // V3-10a: the three-way (the End's own context — the Nether's T4
    // shape; the two flags are mutually exclusive by construction).
    const AmbienceContext ctx = ambienceContext(
        rawSky, m_dayCycle.dayFactor(), playerInNether(), playerInEnd());
    // The roll: the pure function of (world seed, frame, context) — same
    // inputs, same answer across runs (the event sequence is
    // deterministic; the log line below is the E2E evidence). V3-1 Phase
    // B: the CURRENT dimension's seed (the nether's derived one — the
    // stream never tracks the overworld's in the nether, the salt
    // discipline).
    if (!ambienceRoll(static_cast<std::uint64_t>(currentSeed()),
                      static_cast<std::uint64_t>(m_frameCount), ctx)) {
        return;
    }
    const char* name = ambienceSoundName(ctx);
    // The ambience is GLOBAL — the 2D centered path (never spatialized,
    // the CRAFT-3 listener is inert for it). Audio never enters the frame
    // state (hard rule 8 / the CRAFT-3 discipline): no screenshot or save
    // is affected, and the disabled-bank degradation (headless) makes
    // play() a silent no-op — the log line still fires.
    m_sound.play(name);
    MC_LOG_INFO(std::string("Ambience: ") + name);
}

void Game::updatePortalEntry() {
    // V3-1 Phase B: the portal entry countdown (the frame-locked class —
    // hard rule 8, the day-cycle step): the player's AABB overlapping a
    // portal cell (the overlapsBlock precedent — the fire-contact twin)
    // arms kPortalEntryFrames through the pure portalEntryStep (the
    // unit tests own the step); leaving cancels. The GUI mutual
    // exclusion: a panel opened mid-countdown auto-CLOSES (the Minecraft
    // portal rule — the panel cannot stay open in a portal; the close
    // returns the items + frees the pointer, the closePanel cleanup).
    // Completion (kPortalTravelEnabled = true, Phase B) runs the real
    // dimension switch: the pure travel-destination decision
    // (core/portal's portalTravelDestination — the mapped coordinate,
    // the ±16 portal search / the generation fallback, the bottom-center
    // landing, the orthogonal yaw turn) + the travelPortal pipeline (the
    // catch-up disk, the rebind, the flip, the cooldown, the two sounds,
    // the "Traveled to <dim>" log). The cooldown (kPortalCooldownFrames,
    // the Minecraft 15 s rule) blocks re-arming so the player — left
    // INSIDE the destination portal by the bottom-center landing — is not
    // ping-ponged back on the next countdown (the Phase A no-cooldown pin
    // is superseded: the destination-inside-portal geometry makes the
    // cooldown mandatory). Dimension-agnostic: the same countdown runs
    // the overworld -> nether AND the nether -> overworld legs (the
    // player's world binding makes overlapsBlock read the current
    // dimension's portal cells).
    static_assert(kPortalTravelEnabled,
                  "Phase B ships the real travel pipeline (travelPortal): "
                  "kPortalTravelEnabled is true. Flipping it back to the "
                  "Phase A no-op stub requires removing the pipeline — "
                  "not a TODO, a build gate.");
    if (m_portalCooldownFrames > 0) {
        --m_portalCooldownFrames; // the cooldown always runs (frame-locked)
    }
    if (!m_states.portalId.valid() || m_player->isDead()) {
        m_portalEntryFrames = 0;
        return; // the structural-inert / dead-body no-op (the death
                // screen owns the frozen body)
    }
    const bool inPortal = m_player->overlapsBlock(m_states.portalId);
    if (inPortal && panelOpen()) {
        closePanel(); // the GUI cannot stay open in a portal (Minecraft)
    }
    // The cooldown blocks re-arm (the ping-pong guard) — but the panel
    // mutual exclusion above still applies (the GUI rule is independent
    // of the timer, the Minecraft twin).
    const bool canArm = m_portalCooldownFrames == 0;
    const PortalEntryStep step =
        portalEntryStep(canArm && inPortal, m_portalEntryFrames);
    m_portalEntryFrames = step.framesRemaining;
    if (step.completed) {
        // The completion frame: resolve the SOURCE frame (the portal the
        // player's AABB overlaps — the travel's axis + the mapping's
        // anchor) and run the dimension switch (the single-frame,
        // frame-locked pipeline — see travelPortal).
        const std::optional<PortalFrame> source = playerPortalFrame();
        if (source) {
            // The travel sound (the fade, 2D centered — the switch is
            // global, not a point in space; the thunder / ambience
            // precedent). The trigger sound played on the ARM frame below.
            travelPortal(*source);
        }
        return;
    }
    // The ARM frame (the first in-portal frame of the countdown): the
    // portal TRIGGER sound (the Minecraft block.portal.trigger — the
    // entry cue), spatialized at the player's feet (the player is inside
    // the portal, so the pan is centered — the 2D-centered class).
    // (The step completes on the kPortalEntryFrames-th frame, so the arm
    // and the completion are distinct frames: this branch never co-fires
    // with the travel above.)
    if (step.framesRemaining == kPortalEntryFrames - 1) {
        // V3-14d: the portal arm DISMOUNTS the rider (the mount cannot
        // cross dimensions — the strider stays behind in the nether;
        // the player continues the 60-frame countdown on foot, the
        // recon's risk ② double-gate's second half: the ride pass's
        // dimension-agnostic conditions plus this arm frame).
        dismountRide();
        m_sound.play("portal_trigger");
        MC_LOG_INFO("Portal entry: travel countdown armed");
    }
}

// V3-14d: the carry's height — the ride point on the mount (the
// strider's BODY box top: 0.55 + 0.5 = 1.05 above the feet,
// strider.json's model boxes — the YAGNI core constant: the first
// mount's pose is fixed by its model, a second rideable species
// re-opens the data-fication, the MC's per-species saddle point).
static constexpr float kRideCarryHeight = 1.05f;

const Input& Game::inputFor(std::size_t i) const {
    // V3-12b: the per-player SIM input source. SOLO (m_players.size()==1,
    // the only reachable state with no net session): the live pump — the
    // pre-V3-12b value, bit-identical. NET (size > 1): the player's
    // preloaded ring input (the preloadNetInputs' target — the N−D
    // frame state, the unified delay rule's no local fast-path).
    if (m_netActive && m_players.size() > 1) {
        return m_netInputs[i];
    }
    (void)i;
    return m_input;
}

void Game::updateRide() {
    // V3-12b: the ×N ride pass (the per-player dismount conditions +
    // the carry snap + the steer push — solo: one player, the
    // pre-V3-12b sequence bit-identical). No ride per player = one null
    // check each (the structural-immunity root).
    for (std::size_t i = 0; i < m_players.size(); ++i) {
        Player& p = *m_players[i];
        PlayerSession& s = *m_sessions[i];
        if (s.ridingMount == nullptr) {
            continue;
        }
        MobEntity& mount = *s.ridingMount;
        // The dismount conditions (every exit funnel here): the mount
        // dies / the player dies (the death screen owns the body) / the
        // flight toggle (G / the gamepad's B — the carry and the fly
        // cannot stack) / a panel opens (the pointer frees, the panel
        // owns the input — same-frame, handlePanelClicks ran first) /
        // the mount is IN WATER (the dismounts_underwater structural
        // twin — the LAVA SURFACE stays ridden: onLavaSurface is the
        // strider's home, the Minecraft ruling).
        if (mount.dead() || p.isDead() || p.isFlying() ||
            s.panel.openPanel != OpenPanel::None ||
            (mount.inFluid() && !mount.onLavaSurface())) {
            dismountRideFor(p, s);
            continue;
        }
        // The steer push (the player's key wish in the CAMERA frame —
        // the player update's flatFwd / right twins, yaw 0 = -Z): W
        // forward, S back, A/D side (the WASD-direct ruling — the MC
        // fungus-stick turn is the documented deviation 2; any wish =
        // forward drive, no wish = the kRideDecel drift-stop).
        const float cy = std::cos(p.yaw);
        const float sy = std::sin(p.yaw);
        const Input& in = inputFor(i);
        Vec3 dir {0.0f, 0.0f, 0.0f};
        if (in.isDown(GameKey::W)) {
            dir = dir + Vec3 {-sy, 0.0f, -cy};
        }
        if (in.isDown(GameKey::S)) {
            dir = dir - Vec3 {-sy, 0.0f, -cy};
        }
        if (in.isDown(GameKey::D)) {
            dir = dir + Vec3 {cy, 0.0f, -sy};
        }
        if (in.isDown(GameKey::A)) {
            dir = dir - Vec3 {cy, 0.0f, -sy};
        }
        mount.setRiderSteer(dir, lengthSq(dir) > 0.0f);
        // The carry snap (the player's pose = the mount's back position;
        // the horizontal velocity is pushed for the swing phase's
        // displacement drive + the dismount's drift — the vertical stays
        // 0: the strider's feet-on-plane snap owns the height, the
        // player's integrate is the no-op while carried).
        p.setPosition(mount.position() + Vec3 {0.0f, kRideCarryHeight, 0.0f});
        const Vec3 mv = mount.velocity();
        p.setVelocity(Vec3 {mv.x, 0.0f, mv.z});
        p.setCarried(true);
    }
}

void Game::dismountRide() {
    // The primary's dismount (the portal arm's choke — D9's
    // primary-anchored portals; solo: the only player, the pre-V3-12b
    // behavior).
    dismountRideFor(*m_player, *m_sessions[0]);
}

void Game::dismountRideFor(Player& p, PlayerSession& s) {
    // V3-14d: the dismount exit (every dismount funnels here — the
    // Space / LeftControl keys, the ride pass's conditions, the portal
    // arm): the mount's AI returns to its wander (the m_ridden release
    // + the steer zero), the player's carry bypass lifts (the next
    // update runs the normal physics: gravity from the back position).
    // The fall bookkeeping is safe by construction: the last carried
    // frame re-anchored m_fallStartY to the back position (the player
    // update's fly twin), so the ~1.05-block dismount drop never
    // damages. The player KEEPS the mount's horizontal velocity (the
    // drift — the knockback-free project's natural exit).
    if (s.ridingMount == nullptr) {
        return;
    }
    MC_LOG_DEBUG("dismounted the '" + s.ridingMount->def().name + "'");
    s.ridingMount->setRidden(false);
    s.ridingMount->setRiderSteer(Vec3::zero(), false);
    s.ridingMount = nullptr;
    p.setCarried(false);
}

// The portal frame the player's AABB currently overlaps (the travel's
// SOURCE): the player's cells (the strict-overlap convention, the
// overlapsBlock twin — the per-axis cell ranges from the AABB's min/max)
// scanned in deterministic (x, y, z) order for a portal block; the first
// hit's VALID frame (portalValidFrame — a cell whose ring is broken is
// an orphan, not a source) is returned. nullopt = the player overlaps a
// portal cell whose frame is invalid (structurally unreachable for a
// tracked portal — the rescan keeps the tracking set valid; the belt
// returns nullopt and the travel is a no-op).
std::optional<PortalFrame> Game::playerPortalFrame() const {
    if (!m_states.portalId.valid()) {
        return std::nullopt;
    }
    const World& world =
        playerInNether() ? *m_nether.world : m_world;
    const Vec3 mn = m_player->aabbMin();
    const Vec3 mx = m_player->aabbMax();
    const int x0 = static_cast<int>(std::floor(mn.x));
    const int x1 = static_cast<int>(std::ceil(mx.x)) - 1;
    const int y0 = static_cast<int>(std::floor(mn.y));
    const int y1 = static_cast<int>(std::ceil(mx.y)) - 1;
    const int z0 = static_cast<int>(std::floor(mn.z));
    const int z1 = static_cast<int>(std::ceil(mx.z)) - 1;
    for (int x = x0; x <= x1; ++x) {
        for (int y = y0; y <= y1; ++y) {
            for (int z = z0; z <= z1; ++z) {
                if (!(mn.x < x + 1 && mx.x > x)) {
                    continue;
                }
                if (!(mn.y < y + 1 && mx.y > y)) {
                    continue;
                }
                if (!(mn.z < z + 1 && mx.z > z)) {
                    continue;
                }
                if (world.getBlock(x, y, z) != m_states.portalId) {
                    continue;
                }
                return portalValidFrame(world, BlockPos {x, y, z},
                                        m_states.obsidianId,
                                        m_states.fireId,
                                        m_states.portalId);
            }
        }
    }
    return std::nullopt;
}

void Game::travelPortal(const PortalFrame& sourceFrame) {
    // D6 (the single-frame frame-locked travel pipeline — no loading
    // screen, no transition frame; the design's deviation 7). Runs in the
    // update's main-thread-writer window (updatePortalEntry's call site),
    // so the synchronous catch-up + the generated-portal writes are safe
    // (no worker in flight — the flush barrier ran at the top of update).
    const Dimension source = m_playerDimension;
    const Dimension target =
        (source == Dimension::Overworld) ? Dimension::Nether
                                         : Dimension::Overworld;
    // 1. The source cell (the position's floor — the feet convention) +
    //    the 8:1 mapping (the pure overworldToNether / netherToOverworld).
    const Vec3 p = m_player->position();
    const BlockPos src {static_cast<int>(std::floor(p.x)),
                        static_cast<int>(std::floor(p.y)),
                        static_cast<int>(std::floor(p.z))};
    const BlockPos mapped =
        (source == Dimension::Overworld)
            ? overworldToNether(src.x, src.y, src.z)
            : netherToOverworld(src.x, src.y, src.z);
    // 2. The target dimension (the nether is LAZY — engage it now; the
    //    overworld always exists). A stripped asset set leaves the
    //    nether unengaged (ensureNether's named error): abort the travel
    //    (the countdown clears — re-entry re-counts, the Phase A twin).
    if (target == Dimension::Nether) {
        ensureNether();
        if (!m_nether.world) {
            m_portalEntryFrames = 0;
            return;
        }
    }
    World& targetWorld =
        (target == Dimension::Overworld) ? m_world : *m_nether.world;
    // 3. The destination decision (the pure portalTravelDestination — the
    //    ±16 portal search over the target's tracking set, the generation
    //    fallback, the bottom-center landing + hitbox probe, the
    //    orthogonal yaw turn).
    std::vector<BlockPos> cells;
    cells.reserve(portalsOf(target).size());
    for (const auto& [pos, id] : portalsOf(target)) {
        (void)id;
        cells.push_back(pos); // the std::map is position-sorted
    }
    const PortalTravelQuery q {
        mapped, sourceFrame.axis, m_player->yaw, cells, targetWorld,
        m_blocks, m_states.obsidianId, m_states.fireId, m_states.portalId,
        m_player->params().width, m_player->params().height};
    const PortalTravelPlan plan = portalTravelDestination(q);
    // 4. The generation fallback: write the frame (the 2x3 bedrock
    //    platform, the corners-included obsidian ring, the portal interior
    //    in the frame's axis state) through the TARGET's choke point
    //    (the journal / editedColumns / tracking-set protection rides
    //    along — the player is still in the SOURCE dimension, so the
    //    dimension-explicit setBlockInWorld is required, not the player-
    //    facing setBlock). A found portal writes nothing (it is live).
    if (plan.generated) {
        for (const BlockPos& plat : portalPlatformPlan(plan.frame)) {
            setBlockInWorld(targetWorld, plat.x, plat.y, plat.z,
                            m_states.bedrockId);
        }
        for (const BlockPos& ring : portalFrameRing(plan.frame)) {
            setBlockInWorld(targetWorld, ring.x, ring.y, ring.z,
                            m_states.obsidianId);
        }
        for (const BlockPos& interior : portalFrameInterior(plan.frame)) {
            setBlockInWorld(targetWorld, interior.x, interior.y, interior.z,
                            m_states.portalId);
            targetWorld.setBlockState(interior.x, interior.y, interior.z,
                                      static_cast<std::uint8_t>(plan.frame.axis));
        }
    }
    // 5. The synchronous catch-up disk (3x3 columns around the landing —
    //    the first-touch precedent amplified: ensureColumn + ONE
    //    synchronous relight, the ~ms one-time cost, the design's D6
    //    step 3).
    {
        const int cx = floorDiv(static_cast<int>(std::floor(plan.destination.x)),
                                kChunkSize);
        const int cz = floorDiv(static_cast<int>(std::floor(plan.destination.z)),
                                kChunkSize);
        catchUpDisk(target, cx, cz);
    }
    // 6. The player's world rebind (D3: only the player crosses — setWorld
    //    is the mechanical change; every read the player's physics does
    //    now resolves against the target world) + the landing (restorePosition
    //    = the teleport + the fall pin — the Phase 36 load twin: the
    //    in-air landing takes no free-fall damage) + the yaw (kept or
    //    ortho-turned) + the camera sync.
    m_player->setWorld(targetWorld);
    m_player->restorePosition(plan.destination);
    m_player->yaw = plan.yaw;
    m_camera.position = m_player->eyePosition();
    m_camera.yaw = m_player->yaw;
    m_camera.pitch = m_player->pitch;
    // 7. The dimension flip (the streaming center = the player's position
    //    re-engages the TARGET dimension's StreamingState next frame — the
    //    nether's streamer starts un-engaged, so the player's own column
    //    missing-distance 0 engages it immediately; the SOURCE dimension's
    //    streaming freezes — zero post, the steady-state zero contention).
    m_playerDimension = target;
    // 8. The cooldown (the destination inside-portal ping-pong guard) +
    //    the countdown reset + the travel sound (the fade, 2D centered —
    //    the switch is global) + the info log (the E2E evidence). The log
    //    carries the landing's CELL (the floor of the destination — the
    //    bottom center is the cell's center, so the floor is exact) in
    //    the TARGET dimension's coordinates + the source cell.
    m_portalCooldownFrames = kPortalCooldownFrames;
    m_portalEntryFrames = 0;
    m_sound.play("portal_travel");
    const int destX = static_cast<int>(std::floor(plan.destination.x));
    const int destY = static_cast<int>(std::floor(plan.destination.y));
    const int destZ = static_cast<int>(std::floor(plan.destination.z));
    MC_LOG_INFO(std::string("Traveled to ") +
                (target == Dimension::Nether ? "nether" : "overworld") +
                " at (" + std::to_string(destX) + ", " +
                std::to_string(destY) + ", " + std::to_string(destZ) +
                ") from (" + std::to_string(src.x) + ", " +
                std::to_string(src.y) + ", " + std::to_string(src.z) + ")");
}

void Game::updateEndPortalEntry() {
    // V3-10a: the End portal's entry countdown (the updatePortalEntry's
    // shape — the frame-locked class, hard rule 8): the player's AABB
    // overlapping an end_portal cell in the CURRENT dimension (the
    // overlapsBlock precedent — the nether's travel reads the same way)
    // arms kPortalEntryFrames through the SAME pure portalEntryStep;
    // leaving cancels. The GUI mutual exclusion is the nether's twin
    // (a panel opened mid-countdown auto-CLOSES). Completion runs
    // travelEnd (the 1:1 independent-origin pipeline — NO 8:1 mapping,
    // the fixed entry platform / the overworld-spawn return). The
    // cooldown is the SHARED m_portalCooldownFrames (the ping-pong
    // guard is dimension-agnostic — a destination inside-portal
    // landing is blocked exactly as the nether's is). The trigger sound
    // reuses "portal_trigger" (zero new wav, the documented deviation);
    // the travel sound reuses "portal_travel".
    if (!m_states.endPortalId.valid() || m_player->isDead()) {
        m_endPortalEntryFrames = 0;
        return; // the structural-inert / dead-body no-op
    }
    const bool inPortal = m_player->overlapsBlock(m_states.endPortalId);
    if (inPortal && panelOpen()) {
        closePanel(); // the GUI cannot stay open in a portal (Minecraft)
    }
    const bool canArm = m_portalCooldownFrames == 0;
    const PortalEntryStep step =
        portalEntryStep(canArm && inPortal, m_endPortalEntryFrames);
    m_endPortalEntryFrames = step.framesRemaining;
    if (step.completed) {
        travelEnd();
        return;
    }
    // The ARM frame (the first in-portal frame of the countdown): the
    // trigger sound + the dismount (a rider cannot cross dimensions —
    // the nether's arm frame's twin) + the log.
    if (step.framesRemaining == kPortalEntryFrames - 1) {
        dismountRide();
        m_sound.play("portal_trigger");
        MC_LOG_INFO("End portal entry: travel countdown armed");
    }
}

void Game::travelEnd() {
    // V3-10a: the End's single-frame frame-locked travel pipeline (the
    // travelPortal's shape, the 1:1 independent origin — the 8:1 mapping
    // does NOT apply: the End has its own coordinates, the Minecraft
    // scale-1.0 ruling). The OW->End leg lands on the entry platform
    // (the 5x5 obsidian at (40,52,0) — the PER-ENTRY rewrite: the
    // platform's cells are journaled the first time, idempotent
    // thereafter — the recon's risk ⑦); the End->OW leg (the origin
    // exit) returns to the overworld's SPAWN (the bed-validated respawn
    // precedent — the broken-bed world-spawn fallback). Runs in the
    // update's main-thread-writer window (the flush barrier ran at the
    // top), so the platform writes + the synchronous catch-up are safe.
    const Dimension source = m_playerDimension;
    const Dimension target =
        (source == Dimension::End) ? Dimension::Overworld : Dimension::End;
    const Vec3 p = m_player->position();
    const BlockPos src {static_cast<int>(std::floor(p.x)),
                        static_cast<int>(std::floor(p.y)),
                        static_cast<int>(std::floor(p.z))};
    // 1. The target dimension (the End is LAZY — engage it now; the
    //    overworld always exists). A stripped asset set leaves the End
    //    unengaged (ensureEnd's named error): abort the travel (the
    //    countdown clears — re-entry re-counts).
    if (target == Dimension::End) {
        ensureEnd();
        if (!m_end.world) {
            m_endPortalEntryFrames = 0;
            return;
        }
        // 2. The entry platform (the per-entry rewrite — the
        //    portalPlatformPlan's twin): the 3 layers above cleared
        //    FIRST (the landing safety — the void above is air already,
        //    the clear is the idempotent belt over 10b's future
        //    content), the 5x5 obsidian square second (the feet land on
        //    its top, y 53). Through the TARGET's choke point (the
        //    journal / editedColumns / tracking-set protection ride
        //    along — the player is still in the SOURCE dimension, so the
        //    dimension-explicit setBlockInWorld is required). A
        //    stripped asset set's invalid obsidianId makes the writes
        //    structural no-ops (the landing is the island's endstone,
        //    the platform's absence is a belt-only loss).
        const EndEntryPlatformPlan plan = endEntryPlatformPlan();
        for (const BlockPos& c : plan.clearAir) {
            setBlockInWorld(*m_end.world, c.x, c.y, c.z, BlockID::air());
        }
        if (m_states.obsidianId.valid()) {
            for (const BlockPos& c : plan.setObsidian) {
                setBlockInWorld(*m_end.world, c.x, c.y, c.z,
                                m_states.obsidianId);
            }
        }
    }
    World& targetWorld = (target == Dimension::End) ? *m_end.world : m_world;
    // 3. The destination decision (the pure fixed point — the nether's
    //    ±16 search / generation fallback has no twin: the entry is the
    //    platform's top center facing WEST (the +pi/2 camera
    //    convention, the Minecraft-exact landing), the exit is the
    //    overworld's bed-validated spawn (the respawnPlayer's D8
    //    ruling: the End death's return twin — the coordinate memory is
    //    re-pinned by the spawn-restore below).
    const Vec3 destination = (target == Dimension::End)
        ? endEntryPosition()
        : bedRespawnValid(m_world, m_blocks, m_player->spawnPos(),
                          m_player->params().width,
                          m_player->params().height)
              ? m_player->spawnPos()
              : m_mobs.spawnPoint;
    const float yaw =
        (target == Dimension::End) ? endEntryYaw() : m_player->yaw;
    // 4. The synchronous catch-up disk (3x3 columns around the landing —
    //    the nether's step 5 verbatim).
    {
        const int cx =
            floorDiv(static_cast<int>(std::floor(destination.x)),
                     kChunkSize);
        const int cz =
            floorDiv(static_cast<int>(std::floor(destination.z)),
                     kChunkSize);
        catchUpDisk(target, cx, cz);
    }
    // 5. The player's world rebind (D3: only the player crosses) + the
    //    landing (restorePosition = the teleport + the fall pin) + the
    //    yaw + the camera sync. The exit leg re-pins the spawn memory
    //    (the respawnPlayer's twin — the return's coordinate is the
    //    respawn's coordinate: a later End death lands on the same spot).
    m_player->setWorld(targetWorld);
    if (target == Dimension::Overworld) {
        m_player->setSpawnPos(destination);
    }
    m_player->restorePosition(destination);
    m_player->yaw = yaw;
    m_camera.position = m_player->eyePosition();
    m_camera.yaw = m_player->yaw;
    m_camera.pitch = m_player->pitch;
    // 6. The dimension flip (the nether's step 7 verbatim — the
    //    target's streamer engages on the player's own column).
    m_playerDimension = target;
    // 6b. V3-10a: the flip frame's ambient set (the End's kAmbientEnd /
    //     the return's config floor — the u_ambient's per-dimension
    //     shape, zero per-frame cost).
    applyDimensionAmbient();
    // 7. The SHARED cooldown (the ping-pong guard) + the countdown reset
    //    + the travel sound (the "portal_travel" reuse — zero new wav,
    //    the deviation) + the info log (the E2E evidence — the landing's
    //    cell in the TARGET dimension's coordinates + the source cell).
    m_portalCooldownFrames = kPortalCooldownFrames;
    m_endPortalEntryFrames = 0;
    m_sound.play("portal_travel");
    const int destX = static_cast<int>(std::floor(destination.x));
    const int destY = static_cast<int>(std::floor(destination.y));
    const int destZ = static_cast<int>(std::floor(destination.z));
    MC_LOG_INFO(std::string("Traveled to ") +
                (target == Dimension::End ? "end" : "overworld (end exit)") +
                " at (" + std::to_string(destX) + ", " +
                std::to_string(destY) + ", " + std::to_string(destZ) +
                ") from (" + std::to_string(src.x) + ", " +
                std::to_string(src.y) + ", " + std::to_string(src.z) + ")");
}

namespace {
// V3-10b: the 53-bit uniform in [0, 1) from an advancing splitmix64
// stream (the particle.cpp's local twin — the repo's splitmix64 is a
// pure finalizer: the golden-step advance before the mix, the PITS 4
// discipline).
double uniform01(std::uint64_t& state) {
    state += 0x9E3779B97F4A7C15ULL;
    return static_cast<double>(splitmix64(state) >> 11) /
           static_cast<double>(1ull << 53);
}

// V3-10b: the pearl's landing offset (the particle.cpp's sphere
// direction's 2-draw pure form — u1 the polar cosine, u2 the azimuth,
// the FIXED kPearlTeleportOffset radius on the sphere's surface, the
// MC "2.5 blocks" read; the deterministic stream contract: same seed
// + same counter, same offset every run).
Vec3 pearlOffset(std::uint64_t state) {
    const double u1 = uniform01(state);
    const double u2 = uniform01(state);
    const double theta = 2.0 * 3.14159265358979323846 * u2;
    const double cosPhi = 2.0 * u1 - 1.0;
    const double sinPhi =
        std::sqrt(std::max(0.0, 1.0 - cosPhi * cosPhi));
    return Vec3 {
        static_cast<float>(sinPhi * std::cos(theta) * kPearlTeleportOffset),
        static_cast<float>(cosPhi * kPearlTeleportOffset),
        static_cast<float>(sinPhi * std::sin(theta) * kPearlTeleportOffset)};
}

} // namespace

void Game::pearlLanding(Projectile* pearl) {
    // V3-10b: the pearl's landing seam (the stuck pass's consumer — the
    // fireball's consume-at-impact twin, the zero-damage shape): the
    // thrower is teleported to the landing point + the uniform 2.5
    // offset, the fall bookkeeping re-anchored (the pearl CANCELS the
    // fall damage — the restorePosition's fall pin, the MC twin), a
    // rider dismounts first (the thrower's MC 1.10 twin), the teleport
    // cue plays AT the landing (the CRAFT-3 spatialized origin), the
    // pearl despawns. The probe's TOTAL failure (a sealed pocket — all
    // 4 levels solid) swallows the pearl WITHOUT the teleport (the
    // documented deviation: the MC always teleports; the pocket is
    // rare and the pearl's loss is the visible tell). Runs in the
    // entities' update's main-thread-writer window (the stuck pass —
    // the catch-up disk is safe: the flush barrier ran at the top of
    // update).
    const Vec3 landing = pearl->position();
    const Vec3 offset = pearlOffset(currentSeed() +
                                    (++m_particleBreakCounter) *
                                        0x9E3779B97F4A7C15ULL);
    World& world = currentWorld();
    // The catch-up disk AROUND THE LANDING (the probe's premise: the
    // terrain the offset reaches is generated — the travelEnd's twin,
    // the landing's chunk disk, the offset's 2.5 bound inside the
    // 3x3 columns).
    {
        const int cx = floorDiv(static_cast<int>(std::floor(landing.x)),
                                kChunkSize);
        const int cz = floorDiv(static_cast<int>(std::floor(landing.z)),
                                kChunkSize);
        catchUpDisk(m_playerDimension, cx, cz);
    }
    // The target: the landing + offset (the pearl's position IS the
    // box's bottom-center, the player's feet convention — the stuck
    // resolver put the pearl's bottom face on the surface it struck,
    // so the feet land ON it), the hitbox probe (the strict
    // inequality, hard rule 6) + the +1..+3 up-step (the portal's
    // landing-probe twin — the sealed-pocket total failure swallows).
    // V3-12b: the thrower (the pearl's landing teleports ITS thrower —
    // solo: index 0 = the only player, the pre-V3-12b value).
    const std::size_t pi =
        (pearl->throwerIndex() >= 0) ? static_cast<std::size_t>(
            pearl->throwerIndex()) : 0;
    Player& thrower = *m_players[pi];
    PlayerSession& tsession = *m_sessions[pi];
    Vec3 target = landing + offset;
    const float width = thrower.params().width;
    const float height = thrower.params().height;
    bool roomy = false;
    for (int up = 0; up <= kPortalLandingProbeUp && !roomy; ++up) {
        const Vec3 probe {target.x, target.y + static_cast<float>(up),
                          target.z};
        roomy = Player::hasRoomAt(world, m_blocks, probe, width, height);
        if (roomy) {
            target = probe;
        }
    }
    pearl->kill();
    if (!roomy) {
        MC_LOG_DEBUG("pearl swallowed (the sealed landing pocket — no "
                     "teleport, the documented deviation)");
        return;
    }
    // The dismount (a rider throws — the ride pass's condition would
    // otherwise drop the rider on the flip's frame; the MC 1.10
    // dismounts-underwater-throw's pearl twin).
    dismountRideFor(thrower, tsession);
    thrower.restorePosition(target);
    // The camera's jump cut is the LOCAL machine's act (the thrower's
    // view — solo: the only player, the pre-V3-12b sync; net: the
    // remote thrower's teleport does not move this machine's view).
    if (&thrower == localPlayer()) {
        m_camera.position = thrower.eyePosition();
        m_camera.yaw = thrower.yaw;
        m_camera.pitch = thrower.pitch;
    }
    const Item& pearlItem =
        m_items.at(m_items.idForName("ender_pearl"));
    if (!pearlItem.teleportSound.empty()) {
        m_sound.play(pearlItem.teleportSound, target);
    }
    MC_LOG_DEBUG("pearl teleport to (" + std::to_string(target.x) + ", " +
                 std::to_string(target.y) + ", " +
                 std::to_string(target.z) + ")");
}

namespace {
// V3-10c: the strict AABB-fluid overlap at an arbitrary feet position
// (the Entity::overlapsFluid's static probe form — the same per-axis
// strict convention: a face resting exactly on a cell boundary is NOT
// an overlap). The teleport's landing probe uses it over its candidate
// (the End is structurally fluid-free — the base-rock ocean is SOLID —
// so this is the belt over a player-poured bucket, the contract's
// completeness, not the expected path).
bool aabbHasFluid(const World& world, Vec3 feet, float width, float height) {
    const float half = width * 0.5f;
    const float xn = feet.x - half, xx = feet.x + half;
    const float yn = feet.y, yy = feet.y + height;
    const float zn = feet.z - half, zx = feet.z + half;
    for (int y = static_cast<int>(std::floor(yn));
         y <= static_cast<int>(std::floor(yy)); ++y) {
        if (yn >= y + 1 || yy <= y) {
            continue;
        }
        for (int z = static_cast<int>(std::floor(zn));
             z <= static_cast<int>(std::floor(zx)); ++z) {
            if (zn >= z + 1 || zx <= z) {
                continue;
            }
            for (int x = static_cast<int>(std::floor(xn));
                 x <= static_cast<int>(std::floor(xx)); ++x) {
                if (xn >= x + 1 || xx <= x) {
                    continue;
                }
                if (world.getFluidKind(x, y, z)) {
                    return true;
                }
            }
        }
    }
    return false;
}

} // namespace

bool Game::executeMobTeleport(
    MobEntity* mob, const std::array<TeleportCandidate, kTeleportAttempts>&
                        candidates) {
    // V3-10c: the teleport's GAME side (the callback the mob's roll
    // invokes — the stream contract is the ROLL's, this is pure world
    // I/O). Runs in the update's main-thread-writer window (the flush
    // barrier ran at the top of update), so the catch-up disk + the
    // landing are safe (the pearlLanding's twin). The candidates are
    // probed IN ORDER: the first with a landing (the downward
    // hasRoomAt / fluid probe) wins; the total failure SWALLOWS (the
    // mob stays put — the pearl's sealed-pocket precedent, the
    // documented deviation over the Minecraft's always-land).
    World& world = currentWorld();
    const float w = mob->width();
    const float h = mob->height();
    for (const TeleportCandidate& cand : candidates) {
        // 1. The catch-up disk around the candidate (the probe's
        //    premise — the recon's R3: an unloaded chunk's hasRoomAt is
        //    a read hazard; the pearlLanding's verbatim twin).
        {
            const int cx = floorDiv(static_cast<int>(std::floor(cand.pos.x)),
                                    kChunkSize);
            const int cz = floorDiv(static_cast<int>(std::floor(cand.pos.z)),
                                    kChunkSize);
            catchUpDisk(m_playerDimension, cx, cz);
        }
        // 2. The landing probe (the Minecraft read: the candidate point
        //    must be OPEN — the body's box in the air with no fluid —
        //    and GROUND must exist within kTeleportProbeDown below it:
        //    the enderman falls the gap naturally (the fall re-anchor
        //    in finishTeleport cancels the damage — the pearl twin).
        //    The ground bound is the void-ditch guard: an open point
        //    over a >16 gap (the plateau's underside void, the island's
        //    edge) has no floor in reach — the candidate is rejected
        //    (a tunneling "first roomy level" probe would land the mob
        //    IN the void below solid terrain — the gate's catch: the
        //    enderman dropped under the plateau and despawned). The
        //    support = ANY solid cell under the box's footprint (the
        //    resting convention: one supporting corner holds the box).
        const float half = w * 0.5f;
        const int x0 = static_cast<int>(std::floor(cand.pos.x - half));
        const int x1 = static_cast<int>(std::floor(cand.pos.x + half));
        const int z0 = static_cast<int>(std::floor(cand.pos.z - half));
        const int z1 = static_cast<int>(std::floor(cand.pos.z + half));
        const int feetCell = static_cast<int>(std::floor(cand.pos.y));
        bool ground = false;
        for (int down = 1; down <= kTeleportProbeDown && !ground; ++down) {
            const int yL = feetCell - down;
            if (yL < 0) {
                break;
            }
            for (int z = z0; z <= z1 && !ground; ++z) {
                for (int x = x0; x <= x1 && !ground; ++x) {
                    ground = m_blocks.at(world.getBlock(x, yL, z))
                                  .properties.solid;
                }
            }
        }
        const Vec3 landing = cand.pos;
        if (!ground ||
            !Player::hasRoomAt(world, m_blocks, landing, w, h) ||
            aabbHasFluid(world, landing, w, h)) {
            continue; // sealed / over-void / in-fluid — the next
        }
        // 3. The landing: the move + the mob-side re-anchor (the fall
        //    pin + the stuck window / path drop — the pearl's
        //    fall-cancel twin) + the landing cue (the CRAFT-3
        //    spatialized, the pearl's twin) + the DEBUG line (the E2E
        //    evidence — the run-to-run verbatim string, the pearl's
        //    precedent's shape over the from/to pair).
        const Vec3 from = mob->position();
        mob->teleportTo(landing);
        mob->finishTeleport();
        m_sound.play(kTeleportSound, landing);
        MC_LOG_DEBUG("mob '" + mob->def().name + "' teleported to (" +
                     std::to_string(landing.x) + ", " +
                     std::to_string(landing.y) + ", " +
                     std::to_string(landing.z) + ") from (" +
                     std::to_string(from.x) + ", " + std::to_string(from.y) +
                     ", " + std::to_string(from.z) + ") f" +
                     std::to_string(m_frameCount));
        return true;
    }
    MC_LOG_DEBUG("mob '" + mob->def().name + "' teleport attempt " +
                 "swallowed (no roomy landing among the " +
                 std::to_string(kTeleportAttempts) + " candidates) f" +
                 std::to_string(m_frameCount));
    return false; // the total failure — the swallow (the mob stays put)
}

// ---------------------------------------------------------------------------
// V3-10d: the ender dragon boss (the updateDragonGameplay's single
// choke point + the spawn / the shatter seams — see the declarations).
// ---------------------------------------------------------------------------

namespace {
// The crystal slots' CELL indices (the kEndCrystalSlots centers' floor —
// the world reads' integer form, the single derivation).
struct CrystalCell {
    int x, y, z;
};
constexpr CrystalCell kCrystalCells[6] = {
    {50, 59, 50}, {-50, 59, 50}, {50, 59, -50}, {-50, 59, -50},
    {2, 54, 0},    {-2, 54, 0},
};
// The egg's CELL (the kEndEggPos center's floor).
constexpr CrystalCell kEggCell {0, 54, 0};
} // namespace

void Game::spawnDragon() {
    // V3-10d: the dragon's singleton spawn (the attemptMobSpawn's twin
    // over the FIXED point — the pool is the enderman's, the boss is
    // the worldgen arena's resident). The seed's kSaltDragon xor (the
    // singleton never walks the pool's spawnCounter — the R2's stream
    // isolation). The entity lands in m_end.entities (the dimension's
    // list — the player is in the End by the caller's gate).
    if (!m_end.world) {
        return; // belt (the caller's playerInEnd gate implies built)
    }
    const Mob* def = m_mobs.mobTypes.findByName("ender_dragon");
    if (def == nullptr) {
        return; // cannot happen (init resolved the End pool's
                // vocabulary — the dragon is a data resident, not a
                // pool member, but the belt names nothing: the spawn
                // schedule's gate is the ids' validity)
    }
    const std::uint64_t seed =
        static_cast<std::uint64_t>(currentSeed()) ^
        static_cast<std::uint64_t>(kSaltDragon);
    auto mob = std::make_unique<MobEntity>(
        *m_end.world, m_blocks, *def, kDragonSpawnPos, seed);
    // The dragon's two FEEDS + the ANCHORS (the entity never reads the
    // core layer — the game pushes the end-origin's geometry).
    mob->setDragonAnchors(kEndPerchPos, kDragonOrbitCenter);
    mob->setCrystalsAlive(0, false);
    // The AI's landed cues (the attemptMobSpawn's wiring family):
    // the shoot callback = the DRAGON FIREBALL (damage 0 / no hurt /
    // the leavesCloud flag — the cloud is the damage, not the impact;
    // the dragon_shoot cue + the eye-to-player aim, the skeleton's
    // seam shape).
    {
        const float fireballSpeed = def->properties.fireballSpeed;
        MobEntity* raw = mob.get();
        mob->setShootCallback(
            [this, raw, fireballSpeed]() {
                if (m_player->isDead() || raw->dead()) {
                    return;
                }
                const Vec3 eye = raw->eyePosition();
                const Vec3 target = m_player->eyePosition();
                const Vec3 toPlayer = target - eye;
                const float dist = std::sqrt(dot(toPlayer, toPlayer));
                if (dist < 1e-4f) {
                    return;
                }
                if (!hasLineOfSight(*m_end.world, m_blocks, eye, target)) {
                    return; // the wall's no-fire gate (the skeleton's
                            // read — the crystals' towers are the
                            // arena's only occluders)
                }
                auto fireball = std::make_unique<Projectile>(
                    *m_end.world, m_blocks,
                    eye + toPlayer / dist * 0.5f, toPlayer / dist,
                    fireballSpeed, /*damage=*/0.0f,
                    /*hurtPlayer=*/false, /*power=*/1.0f,
                    /*ammoItem=*/"", /*effectId=*/-1,
                    /*effectDuration=*/0.0, /*effectTier=*/0,
                    /*explodeRadius=*/0.0f, /*explodeSource=*/"",
                    /*teleports=*/false, /*leavesCloud=*/true);
                MC_LOG_DEBUG("dragon fired fireball f" +
                             std::to_string(m_frameCount));
                m_sound.play("dragon_shoot", eye);
                m_end.entities.push_back(std::move(fireball));
            });
    }
    // The ambient (the dragon_ambient's data interval) + the two
    // dragon cues (the growl telegraph + the breath cloud's trigger —
    // the cloud spawns at the PLAYER's feet, the data breath's
    // duration/radius).
    if (!def->properties.ambientSoundName.empty()) {
        const std::string ambientName = def->properties.ambientSoundName;
        mob->setAmbientSoundCallback(
            [this, ambientName](Vec3 origin) {
                m_sound.play(ambientName, origin);
            });
    }
    mob->setGrowlCallback([this](Vec3 origin) {
        MC_LOG_DEBUG("dragon growl (breath telegraph) f" +
                     std::to_string(m_frameCount));
        m_sound.play("dragon_growl", origin);
    });
    {
        const double breathDuration = def->properties.breathDuration;
        const float breathRadius = def->properties.breathRadius;
        mob->setBreathCallback([this, breathDuration, breathRadius]() {
            if (m_player->isDead()) {
                return;
            }
            const Vec3 feet = m_player->position();
            MC_LOG_DEBUG("dragon breath cloud at (" +
                         std::to_string(feet.x) + ", " +
                         std::to_string(feet.y) + ", " +
                         std::to_string(feet.z) + ") f" +
                         std::to_string(m_frameCount));
            m_end.entities.push_back(std::make_unique<BreathCloudEntity>(
                *m_end.world, m_blocks, feet, breathDuration,
                breathRadius));
        });
    }
    MobEntity* raw = mob.get();
    m_end.entities.push_back(std::move(mob));
    m_end.m_dragon = raw;
    MC_LOG_DEBUG("ender dragon spawned at (" +
                 std::to_string(kDragonSpawnPos.x) + ", " +
                 std::to_string(kDragonSpawnPos.y) + ", " +
                 std::to_string(kDragonSpawnPos.z) + ") f" +
                 std::to_string(m_frameCount));
    // V3-10d2: the --dragon-damage seam (PER-SPAWN — the 10d1 one-shot
    // read re-armed on every (re)spawn so one run can walk the full
    // kill -> egg -> summon -> re-kill cycle deterministically; absent
    // = every pre-seam run bit-identical, the 10d1 behavior on a single
    // spawn is unchanged — the application frame is the spawn frame).
    if (m_args.dragonDamage.has_value() && !raw->dead()) {
        MC_LOG_INFO("--dragon-damage: " +
                    std::to_string(*m_args.dragonDamage) +
                    " to the End dragon (spawn " +
                    std::to_string(m_end.m_dragonDeaths + 1) + ")");
        raw->damage(static_cast<float>(*m_args.dragonDamage));
    }
}

bool Game::shatterCrystalAt(int x, int y, int z) {
    // V3-10d: the crystal's shatter (the breakBlock's explode hook's
    // caller-over — the player's break AND the arrow's sweep both flow
    // through here, the single choke point). Returns false when the
    // cell holds no live crystal (the sweep's miss path).
    World& world = currentWorld();
    const BlockID id = world.getBlock(x, y, z);
    if (id != m_states.endCrystalId || !id.valid()) {
        return false;
    }
    // The heal-source's self-harm (the R4's frame ruling: the damage
    // lands BEFORE the blast — the crystal's heal state is the
    // m_dragonHealSlot's THIS-WINDOW read, the beam's source).
    if (m_end.m_dragon != nullptr && !m_end.m_dragon->dead() &&
        m_end.m_dragonHealSlot >= 0) {
        const CrystalCell& healed = kCrystalCells[m_end.m_dragonHealSlot];
        if (healed.x == x && healed.y == y && healed.z == z) {
            const float dmg =
                m_end.m_dragon->def().properties.crystalBreakDamage;
            if (dmg > 0.0f) {
                m_end.m_dragon->damage(dmg);
            }
            m_end.m_dragonHealSlot = -1;
        }
    }
    // The break (the setBlock's journal choke) + the relight + the
    // BLAST (the data explode_radius over the cell center — the
    // no-chain structural property: the blast's setBlock never
    // re-enters this function, the other crystals in range just
    // shatter to air without their own blasts, the MC JE read).
    setBlock(x, y, z, BlockID::air());
    Lighting::updateLocal(world, m_blocks, {x, y, z});
    const Block& crystal = m_blocks.at(id);
    if (crystal.properties.explodeRadius > 0.0f) {
        explodeAt(Vec3 {static_cast<float>(x) + 0.5f,
                        static_cast<float>(y) + 0.5f,
                        static_cast<float>(z) + 0.5f},
                  crystal.properties.explodeRadius, "");
    }
    MC_LOG_DEBUG("end crystal shattered at (" + std::to_string(x) + ", " +
                 std::to_string(y) + ", " + std::to_string(z) + ") f" +
                 std::to_string(m_frameCount));
    return true;
}

void Game::updateDragonGameplay() {
    // V3-10d: the boss's per-frame gameplay pass (the declaration's
    // five sections — the single playerInEnd() gate: a no-End session
    // is one bool check, the structural-immunity root).
    if (!playerInEnd()) {
        return;
    }
    // The system's validity belt (the ids' init resolution — an asset
    // set without the dragon's blocks is structurally inert, the P70
    // pattern).
    if (!m_states.endCrystalId.valid() || !m_states.dragonEggId.valid()) {
        return;
    }
    World& world = *m_end.world;

    // -- 1. the spawn schedule (the first-visit's 60-frame delay) ------
    MobEntity* dragon = m_end.m_dragon;
    // The pointer's liveness belt (the despawn pass's rebuild moves the
    // unique_ptrs — a removed dragon leaves a stale pointer; the scan
    // is the few-entity list, negligible): null on the removal frame.
    if (dragon != nullptr) {
        bool alive = false;
        for (const auto& e : m_end.entities) {
            if (e.get() == dragon) {
                alive = true;
                break;
            }
        }
        if (!alive) {
            m_end.m_dragon = nullptr;
            dragon = nullptr;
        }
    }
    // V3-10d2: the FIRST-VISIT read (the 10d1 gate + the defeated's
    // closure): after a defeat the egg's absence is a broken egg, not a
    // spawn signal — the summon sequence's explicit delay is the only
    // re-spawn path (the MC read, the gate-217 probe's fix).
    if (dragon == nullptr && m_end.m_dragonSpawnDelay < 0 &&
        m_mobs.mobsEnabled && !m_end.m_dragonDefeated &&
        world.getBlock(kEggCell.x, kEggCell.y, kEggCell.z) !=
            m_states.dragonEggId) {
        m_end.m_dragonSpawnDelay = kDragonAscentFlyFrames; // the 1-s read
    }
    if (m_end.m_dragonSpawnDelay >= 0) {
        --m_end.m_dragonSpawnDelay;
        if (m_end.m_dragonSpawnDelay == 0) {
            m_end.m_dragonSpawnDelay = -1;
            spawnDragon();
        }
    }
    // The --dragon-damage seam moved to spawnDragon's tail (V3-10d2's
    // per-spawn read — the 10d1 one-shot's same-frame behavior on a
    // single spawn, re-armed on every re-spawn for the re-kill cycle).
    // Re-capture AFTER the spawn/seam: the same-frame spawn+kill (the
    // gate's seam) must still book its death this frame (the R14's
    // 0-frame-gap ruling — the egg lands on the m_dead frame). A
    // non-spawn frame re-reads the identical pointer.
    dragon = m_end.m_dragon;

    // -- 1b. V3-10d2: the egg session respawn + the summon sequence --
    const BlockID eggCell =
        world.getBlock(kEggCell.x, kEggCell.y, kEggCell.z);
    // The egg's REWRITE (the player-broken egg's 60-s session timer —
    // D7: a reload resets the timer, the egg's presence is the
    // persisted state; the summon's removal is NOT a player break and
    // clears the pending timer instead). The deadline's frame rewrites
    // the cell (air-only belt — an occupied cell keeps its occupant).
    if (m_end.m_eggRespawnFrame >= 0 &&
        m_frameCount >= m_end.m_eggRespawnFrame) {
        m_end.m_eggRespawnFrame = -1;
        if (eggCell == BlockID::air()) {
            setBlock(kEggCell.x, kEggCell.y, kEggCell.z,
                     m_states.dragonEggId);
            Lighting::updateLocal(world, m_blocks,
                                  {kEggCell.x, kEggCell.y, kEggCell.z});
            MC_LOG_DEBUG("dragon egg respawned at (0, 54, 0) f" +
                         std::to_string(m_frameCount));
        }
    }
    // The SUMMON sequence (the MC's re-summon read): all 4 EXIT-FACE
    // crystals present (kEndSummonFaceSlots — the ±X pair are the
    // arena's exit crystals, the ±Z pair worldgen-absent = only a
    // player placement fills them) + no egg + no dragon = the
    // countdown arms; any loss (a face crystal broken / the egg back /
    // a dragon live) cancels it (the mid-sequence break = the MC
    // rule). At zero: the 4 face crystals detonate (shatterCrystalAt's
    // single choke — the self-harm's dragon-less no-op, the blast's
    // no-chain structural property), the pending egg timer clears, the
    // MISSING tower crystals restore (the worldgen state's idempotent
    // rewrite — loaded chunks only, an unloaded tower chunk is
    // worldgen-fresh = intact by the 10d1 census read), and the
    // EXPLICIT 60-frame spawn delay brings the dragon back (the
    // first-visit gate stays defeated-closed — the summon is the only
    // re-spawn path, the "re-kill" of the cycle).
    int faceCrystals = 0;
    for (const Vec3& slot : kEndSummonFaceSlots) {
        const int cx = static_cast<int>(std::floor(slot.x));
        const int cy = static_cast<int>(std::floor(slot.y));
        const int cz = static_cast<int>(std::floor(slot.z));
        // The face slots read the LIVE cell (unlike the arena census's
        // unloaded-intact read): the ±Z pair is worldgen-ABSENT, so an
        // unloaded chunk reads air = not present — the summon only
        // arms at the exit, where the summoning player's own column
        // keeps the cells loaded (the context's structural read).
        if (world.getBlock(cx, cy, cz) == m_states.endCrystalId) {
            ++faceCrystals;
        }
    }
    const bool summonContext =
        faceCrystals == 4 && eggCell == BlockID::air() && dragon == nullptr;
    if (m_end.m_dragonSummonCountdown < 0) {
        if (summonContext) {
            m_end.m_dragonSummonCountdown = kDragonSummonCountdownFrames;
            MC_LOG_DEBUG("dragon summon sequence armed (4 face crystals) f" +
                         std::to_string(m_frameCount));
        }
    } else if (!summonContext) {
        m_end.m_dragonSummonCountdown = -1;
        MC_LOG_DEBUG("dragon summon sequence cancelled f" +
                     std::to_string(m_frameCount));
    } else {
        --m_end.m_dragonSummonCountdown;
        if (m_end.m_dragonSummonCountdown == 0) {
            m_end.m_dragonSummonCountdown = -1; // complete (the armed
                                                // state's exit — the
                                                // post-sequence context
                                                // loss is not a cancel)
            MC_LOG_DEBUG("dragon summon sequence complete f" +
                         std::to_string(m_frameCount));
            for (const Vec3& slot : kEndSummonFaceSlots) {
                shatterCrystalAt(static_cast<int>(std::floor(slot.x)),
                                 static_cast<int>(std::floor(slot.y)),
                                 static_cast<int>(std::floor(slot.z)));
            }
            // The summon's egg removal is not a player break (the MC's
            // no-egg-after-summon read) — clear the pending respawn.
            m_end.m_eggRespawnFrame = -1;
            // The explicit re-spawn (the first-visit gate's defeated
            // closure makes this the summon's own 60-frame delay —
            // the "crystals, then the dragon" read).
            m_end.m_dragonSpawnDelay = kDragonAscentFlyFrames;
            for (const Vec3& t : kEndTowerSlots) {
                const int cx = static_cast<int>(std::floor(t.x));
                const int cz = static_cast<int>(std::floor(t.z));
                const ChunkPos cp =
                    worldToChunk(BlockPos {cx, 59, cz});
                if (world.hasChunk(cp) &&
                    world.getBlock(cx, 59, cz) != m_states.endCrystalId) {
                    setBlock(cx, 59, cz, m_states.endCrystalId);
                    Lighting::updateLocal(world, m_blocks,
                                          {cx, 59, cz});
                    MC_LOG_DEBUG("tower crystal restored at (" +
                                 std::to_string(cx) + ", 59, " +
                                 std::to_string(cz) + ") f" +
                                 std::to_string(m_frameCount));
                }
            }
        }
    }

    // -- 2. the crystal census + fire + heal cadence -------------------
    // UNLOADED-CHUNK INDEPENDENCE (the gate-216 streaming probe): a
    // crystal can only break through the loaded-chunk paths (the
    // player's break / the arrow sweep both read the chunk's cell), so
    // a slot in an unloaded chunk is WORLDGEN-FRESH = intact. Counting
    // it live keeps the census at the arena's true state from frame 0
    // (a raw getBlock read would drift 0->6 as the chunks stream in,
    // faking crystal breaks into the strafe trigger and the perch
    // probability).
    int liveCrystals = 0;
    for (const CrystalCell& c : kCrystalCells) {
        const ChunkPos cp = worldToChunk(BlockPos {c.x, c.y, c.z});
        if (!world.hasChunk(cp) ||
            world.getBlock(c.x, c.y, c.z) == m_states.endCrystalId) {
            ++liveCrystals;
        }
    }
    // The first census is never a "change" (the -1 sentinel would
    // otherwise read as a crystal break on the spawn's frame — the
    // spurious strafe the gate-216 probe caught).
    const bool crystalsChanged =
        m_end.m_lastDragonCrystalCount >= 0 &&
        m_end.m_lastDragonCrystalCount != liveCrystals;
    m_end.m_lastDragonCrystalCount = liveCrystals;

    if (dragon != nullptr && !dragon->dead()) {
        dragon->setCrystalsAlive(liveCrystals, crystalsChanged);
        // The state transition's E2E evidence line (the 10c provoked
        // line's twin — the boss combat's three-state cycle is the
        // gate's assertion target; the game pass owns the log, the
        // entity stays log-free as every other does).
        const MobState st = dragon->state();
        const int s = static_cast<int>(st);
        if (m_end.m_dragonLastState != s) {
            const char* name =
                st == MobState::Wander
                    ? "orbit"
                    : (st == MobState::RangedAttack
                           ? "strafe"
                           : (st == MobState::Attack ? "perch" : "state"));
            MC_LOG_DEBUG("dragon entered " + std::string(name) + " f" +
                         std::to_string(m_frameCount));
            m_end.m_dragonLastState = s;
        }
    }
    // The crystal fire (each live crystal's cell+1y = a permanent end
    // fire block — the D9 closed set's zero-churn read: written once,
    // never re-touched; the break clears it with the crystal).
    for (const CrystalCell& c : kCrystalCells) {
        if (world.getBlock(c.x, c.y, c.z) == m_states.endCrystalId &&
            world.getBlock(c.x, c.y + 1, c.z) == BlockID::air()) {
            setBlock(c.x, c.y + 1, c.z, m_states.fireId);
        }
    }
    // The heal cadence (the 1 HP / 30 frames from the NEAREST live
    // crystal in 32 — the data-order tie-break over equal distances).
    m_end.m_dragonHealFrame = (m_end.m_dragonHealFrame + 1) % 30;
    m_end.m_dragonHealSlot = -1;
    if (dragon != nullptr && !dragon->dead() && m_end.m_dragonHealFrame == 0
        && liveCrystals > 0) {
        const Vec3 dpos = dragon->position();
        int best = -1;
        float bestDist = 32.0f;
        for (int i = 0; i < 6; ++i) {
            const CrystalCell& c = kCrystalCells[i];
            // The same unloaded-chunk predicate as the census above
            // (an intact crystal in an unloaded chunk is a valid
            // heal source — the beam's render is load-independent).
            const ChunkPos cp = worldToChunk(BlockPos {c.x, c.y, c.z});
            if (world.hasChunk(cp) &&
                world.getBlock(c.x, c.y, c.z) != m_states.endCrystalId) {
                continue;
            }
            const Vec3 center {static_cast<float>(c.x) + 0.5f,
                               static_cast<float>(c.y) + 0.5f,
                               static_cast<float>(c.z) + 0.5f};
            const float d = std::sqrt(dot(center - dpos, center - dpos));
            if (d < bestDist) { // the strict < = the data-order
                                 // tie-break (earlier slot wins)
                bestDist = d;
                best = i;
            }
        }
        if (best >= 0 && dragon->heal(1.0f)) {
            m_end.m_dragonHealSlot = best;
        }
    }

    // -- 3. the contact damage pass (the wing sweep) -------------------
    for (auto& entity : m_end.entities) {
        auto* mob = dynamic_cast<MobEntity*>(entity.get());
        if (mob == nullptr || mob->dead()) {
            continue;
        }
        const float contact = mob->def().properties.contactDamage;
        if (contact <= 0.0f) {
            continue; // the data gate (the dragon is the shipped carrier)
        }
        mob->tickContactCooldown(1.0 / 60.0);
        if (!mob->contactDamageReady() || m_player->isDead()) {
            continue;
        }
        // The strict AABB overlap (hard rule 6 — the no-epsilon read).
        const Vec3 mmin = mob->aabbMin(), mmax = mob->aabbMax();
        const Vec3 pmin = m_player->aabbMin(), pmax = m_player->aabbMax();
        const bool overlap = pmin.x < mmax.x && pmax.x > mmin.x &&
                             pmin.y < mmax.y && pmax.y > mmin.y &&
                             pmin.z < mmax.z && pmax.z > mmin.z;
        if (!overlap) {
            continue;
        }
        const float damageScale = mob->def().properties.hostile
                                      ? hostileDamageScale(m_mobs.difficulty)
                                      : 1.0f;
        m_player->damage(contact * damageScale, false,
                         {Player::DamageSource::Mob, mob->def().name});
        mob->latchContactDamage();
    }

    // -- 4. the breath cloud's damage pass (the player's overlap) ------
    if (!m_player->isDead()) {
        const Vec3 pmin = m_player->aabbMin(), pmax = m_player->aabbMax();
        for (auto& entity : m_end.entities) {
            auto* cloud = dynamic_cast<BreathCloudEntity*>(entity.get());
            if (cloud == nullptr) {
                continue;
            }
            const Vec3 c = cloud->position();
            const float r = cloud->radius();
            // The horizontal disc (the player's feet cell vs the cloud's
            // center — the standing read) + the y band (the cloud's
            // hover height's 1-block tolerance).
            const float hdx = c.x - pmin.x;
            const float hdz = c.z - pmin.z;
            const bool inDisc = std::sqrt(hdx * hdx + hdz * hdz) <= r;
            const bool inBand = c.y < pmax.y + 1.0f && c.y > pmin.y - 1.0f;
            if (!inDisc || !inBand) {
                continue;
            }
            // The 30-frame re-grant (the effect's 0.6 duration outlives
            // the gap — a continuous cloud = a continuous effect).
            if (m_frameCount % 30 == 0) {
                applyPlayerEffect("dragon_breath", 0.6, 0);
                syncAbsorption();
            }
        }
    }

    // -- 5. the ascension ritual driver (the dead boss's corpse) -------
    if (dragon != nullptr && dragon->dead()) {
        const int f = dragon->ascensionFrames();
        if (f < 0) {
            // The m_dead frame (the R14's 0-frame-gap bookkeeping — the
            // egg's world-state marker + the kill stat + the death line
            // all run HERE, the ritual's anchor, not the removal frame).
            dragon->setAscensionFrames(0);
            m_end.m_dragonDeathPos = dragon->position();
            // The egg (the end segment's journal's +1 cell — the
            // defeat's WORLD-STATE marker, the format-32 no-bump's
            // persistence ruling).
            setBlock(kEggCell.x, kEggCell.y, kEggCell.z,
                     m_states.dragonEggId);
            Lighting::updateLocal(world, m_blocks,
                                  {kEggCell.x, kEggCell.y, kEggCell.z});
            ++m_stats["kill." + dragon->def().name];
            emitTutorialEvent("kill." + dragon->def().name, 1);
            MC_LOG_DEBUG("mob '" + dragon->def().name + "' died at (" +
                         std::to_string(dragon->position().x) + ", " +
                         std::to_string(dragon->position().y) + ", " +
                         std::to_string(dragon->position().z) + ") f" +
                         std::to_string(m_frameCount));
            // V3-10d2: the RE-KILL's XP (death #1 = the 10d1's 200
            // burst, #2+ = one 8-value orb — the MC 500/12000 ratio's
            // 1/24 over the project's 200, the D5's read) + the
            // defeated's latch (the spawn gate's closure).
            ++m_end.m_dragonDeaths;
            m_end.m_dragonDefeated = true;
            const bool rekill = m_end.m_dragonDeaths > 1;
            const int burstTotal = rekill
                ? kDragonRekillXpValue
                : (kDragonXpOrbs - 1) * kDragonXpOrbValue +
                      kDragonXpLastOrbValue; // the 22 x 9 + 2 = 200
            MC_LOG_INFO("dragon death: egg placed, " +
                        std::to_string(burstTotal) + " XP burst " +
                        std::string(rekill ? "scheduled (re-kill)"
                                           : "scheduled"));
            if (rekill) {
                m_end.m_dragonXpRemaining = 1;
                m_end.m_dragonXpLastValue = kDragonRekillXpValue;
            } else {
                m_end.m_dragonXpRemaining = kDragonXpOrbs;
                m_end.m_dragonXpLastValue = kDragonXpLastOrbValue;
            }
            m_end.m_dragonXpFrame = 0;
        } else if (f < kDragonAscentTotalFrames) {
            const int nf = f + 1;
            dragon->setAscensionFrames(nf);
            if (nf <= kDragonAscentFlyFrames) {
                // The flight to the perch (the 60-frame lerp — the
                // corpse's pure-geometry path, the R14's read).
                const float t =
                    static_cast<float>(nf) / kDragonAscentFlyFrames;
                const Vec3 from = m_end.m_dragonDeathPos;
                const Vec3 to = kEndPerchPos;
                dragon->teleportTo(Vec3 {from.x + (to.x - from.x) * t,
                                         from.y + (to.y - from.y) * t,
                                         from.z + (to.z - from.z) * t});
            } else {
                // The perch hover (the 180-frame still — the rise is
                // the particle stream's job below).
                dragon->teleportTo(kEndPerchPos);
            }
            // The ascension's particle stream (the crystal-texture's
            // rising embers — the spawnFire's deterministic seed: the
            // world seed x the frame, one per 4 frames).
            if (nf % 4 == 0) {
                const Vec3 p = dragon->position();
                m_particles.spawnFire(
                    static_cast<int>(std::floor(p.x)),
                    static_cast<int>(std::floor(p.y)),
                    static_cast<int>(std::floor(p.z)),
                    m_states.endCrystalId, 0.0f, 15.0f,
                    static_cast<std::uint64_t>(currentSeed()) ^
                        (std::uint64_t{0x9E3779B97F4A7C15} *
                         static_cast<std::uint64_t>(m_frameCount)));
            }
            // The XP burst (the 200's 23-orb release — the every-5-
            // frames cadence over the perch, the MC 155/5 read).
            if (nf > kDragonAscentFlyFrames &&
                m_end.m_dragonXpRemaining > 0) {
                m_end.m_dragonXpFrame =
                    (m_end.m_dragonXpFrame + 1) % kDragonXpReleaseFrames;
                if (m_end.m_dragonXpFrame == 0) {
                    // The last orb's value is the RITUAL's (the 10d1's
                    // 2, the 10d2 re-kill's 8 — m_dragonXpLastValue's
                    // bookkeeping set).
                    const int value =
                        m_end.m_dragonXpRemaining == 1
                            ? m_end.m_dragonXpLastValue
                            : kDragonXpOrbValue;
                    MC_LOG_DEBUG("dragon XP orb " +
                                 std::to_string(kDragonXpOrbs -
                                                 m_end.m_dragonXpRemaining +
                                                 1) + "/" +
                                 std::to_string(kDragonXpOrbs) +
                                 " (value " + std::to_string(value) +
                                 ") f" + std::to_string(m_frameCount));
                    spawnExpOrb(value, kEndPerchPos);
                    --m_end.m_dragonXpRemaining;
                }
            }
        } else {
            // The ritual's end (the 240-frame removal — the despawn
            // pass drops the corpse this frame; the death cue's final-
            // frame play).
            MC_LOG_DEBUG("dragon ascension complete (wail + removal) f" +
                         std::to_string(m_frameCount));
            m_sound.play("dragon_death", kEndPerchPos);
        }
    }
}

void Game::updatePortalAmbience() {
    // V3-1 Phase A: the portal ambient sound (the bat squeak's per-frame
    // roll, spatialized at the portal cell's center — the 0.5 volume
    // class is BAKED into the sample's master amplitude, the squeak's
    // 0.1-class twin one step up). Every tracked portal within
    // kPortalAmbientRadiusBlocks of the player's feet rolls once per
    // frame: the (seed, frame, cell) pure stream (the fire-particle-roll
    // pattern, the dedicated kPortalAmbientSalt) at
    // 1 / (kPortalAmbientIntervalSeconds × 60). Audio never enters the
    // frame state (hard rule 8 / the CRAFT-3 discipline — the sound is
    // heard, never seen or saved); a no-input run without portals never
    // arms it (the empty tracking set's fast exit, the structural-
    // immunity root).
    const std::map<BlockPos, BlockID>& portals =
        portalsOf(playerInNether() ? Dimension::Nether : Dimension::Overworld);
    if (portals.empty()) {
        return;
    }
    const Vec3 feet = m_player->position();
    const float r2 =
        kPortalAmbientRadiusBlocks * kPortalAmbientRadiusBlocks;
    for (const auto& [pos, id] : portals) {
        (void)id; // the tracking value is the id — the predicate, not data
        // The squared 3D distance, feet to the portal cell's center.
        const float dx = static_cast<float>(pos.x) + 0.5f - feet.x;
        const float dy = static_cast<float>(pos.y) + 0.5f - feet.y;
        const float dz = static_cast<float>(pos.z) + 0.5f - feet.z;
        if (dx * dx + dy * dy + dz * dz > r2) {
            continue; // out of the visible range
        }
        // V3-1 Phase B: the CURRENT dimension's seed (the portal hum
        // stream in the nether runs on the derived seed — the same portal
        // cell in both dimensions never hums in lockstep, the salt
        // discipline).
        if (portalAmbientRoll(static_cast<std::uint64_t>(currentSeed()),
                              static_cast<std::uint64_t>(m_frameCount), pos)) {
            m_sound.play("portal_ambient",
                         Vec3 {static_cast<float>(pos.x) + 0.5f,
                               static_cast<float>(pos.y) + 0.5f,
                               static_cast<float>(pos.z) + 0.5f});
            MC_LOG_INFO("Portal ambience at (" + std::to_string(pos.x) +
                        ", " + std::to_string(pos.y) + ", " +
                        std::to_string(pos.z) + ")");
        }
    }
}

void Game::updateWeather() {
    // V2-4: the weather state machine's frame step (the fixed 1/60 day-
    // cycle class, hard rule 8 — the pure logic in core/weather, the unit
    // tests own it). render.weather OFF = the machine never runs (the
    // state stays at its initial clear — zero interaction + zero drops +
    // the weatherDayFactor's IEEE identities = the v2.3.0 pipeline bit-
    // identical, the gate's "new-off" control).
    if (!m_weatherEnabled) {
        return;
    }
    const WeatherEvent ev =
        weatherStep(m_weather, m_weatherStream, m_weatherThunderStream);
    // The verbatim log lines (the design's E2E evidence — the frame-
    // locked event sequence in the log, the PRE-4 ambience-log pattern).
    if (ev.enteredRain) {
        MC_LOG_INFO("Weather: rain started");
    }
    if (ev.exitedRain) {
        MC_LOG_INFO("Weather: rain stopped");
    }
    if (ev.thunder) {
        // The thunder frame arms the sky pulse (the presentation state —
        // read by weatherDayFactor; never saved, the render-only class).
        m_weatherThunderFrame = m_frameCount;
        // The thunder is GLOBAL — the 2D centered path (never
        // spatialized, the distant-thunder decision, the ambience
        // pattern). Audio never enters the frame state (hard rule 8):
        // the disabled-bank degradation (headless) makes play() a silent
        // no-op — the log line still fires.
        m_sound.play("thunder");
        MC_LOG_INFO("Thunder");
    }
}

void Game::emitTutorialEvent(const std::string& name, int n, Player* actor) {
    // UX-7 / V2-3: the single tutorial-event choke point (the thin
    // adapter — the state machine's logic is unit-test-owned in
    // core/tutorial). Gated: disabled or done = ZERO execution (no
    // event ever fires without input — the no-input run's structural
    // immunity — and a done/disabled tutorial never re-runs the
    // machine, the skip key no-ops through this same gate).
    // V3-12b (D12): the tutorial anchors the PRIMARY — a non-primary
    // actor's event is a no-op in the net session (the solo actor is
    // always the primary = the gate inert, bit-identical).
    if (m_players.size() > 1 && actor != nullptr && actor != m_player) {
        return;
    }
    if (!m_tutorialEnabled || m_tutorial.done) {
        return;
    }
    const TutorialState before = m_tutorial;
    m_tutorial = tutorialApply(m_tutorial, name, n, m_tutorialSeries);
    if (m_tutorial.done) {
        // The completion flip (this event completed the last step of
        // the LAST series): arm the toast window + the one-shot info
        // log (the E2E evidence — the PRE-4 ambience-log pattern:
        // frame-locked event sequence in the log, audio outside the
        // frame state).
        m_tutorialToastFrames = kTutorialToastFrames;
        const int totalSeries = static_cast<int>(m_tutorialSeries.size());
        MC_LOG_INFO("Tutorial: completed (series " +
                    std::to_string(totalSeries) + "/" +
                    std::to_string(totalSeries) + ", step " +
                    std::to_string(m_tutorialSeries.back().size()) +
                    "/" + std::to_string(m_tutorialSeries.back().size()) +
                    ")");
    } else if (before.seriesIndex != m_tutorial.seriesIndex) {
        // The series transition flip (this event completed the last
        // step of a NON-final series): the one-shot info log, NO toast
        // (the design decision: the target line's step change is the
        // cue — zero new text).
        MC_LOG_INFO("Tutorial: series complete (" +
                    std::to_string(before.seriesIndex + 1) + "/" +
                    std::to_string(m_tutorialSeries.size()) + ", step " +
                    std::to_string(m_tutorialSeries[before.seriesIndex]
                                       .size()) +
                    "/" + std::to_string(m_tutorialSeries[before.seriesIndex]
                                            .size()) +
                    ")");
    }
}


// ---------------------------------------------------------------------------
// V3-1 Phase B: the dimension-split world passes (the time-capsule rule —
// see update's gates). updateOverworldWorldPass / updateOverworldFluidPass
// are the pre-Phase-B code MOVED VERBATIM (the overworld sequence in the
// overworld is unchanged — the bit-identical red line); updateNetherFluidTick
// is the nether's twin (the fluid tick over the nether world — the static
// source lava sea's zero churn; no fire registration: the nether holds no
// fire state, the D9 closed-set ruling, the permanent-fire documented
// deviation).
// ---------------------------------------------------------------------------

void Game::updateOverworldWorldPass() {
    // Furnaces (Phase 40, per-block Phase 44): one fixed 1/60 substep per
    // frame, per state (the day-cycle class — integer substep counters,
    // byte-reproducible). std::map iteration is position-sorted: the tick
    // order is deterministic. No-op on an empty state; runs in every
    // pointer/panel state (the world keeps ticking while the panel is
    // open, as in Minecraft).
    for (auto& [pos, st] : m_states.furnaces) {
        // CRAFT-5: snapshot before the tick — the presentation events
        // (the lit crackle, the smelt-complete ding) are a pure function
        // of (before, after); audio is outside the frame state (hard
        // rule 8), so the sound + log never touch determinism.
        const FurnaceState before = st;
        tickFurnace(st, m_items);
        const FurnaceTickEvents ev = furnaceTickEvents(before, st);
        if (ev.lit) {
            m_sound.play("ignite", Vec3 {
                static_cast<float>(pos.x) + 0.5f,
                static_cast<float>(pos.y) + 0.5f,
                static_cast<float>(pos.z) + 0.5f});
            MC_LOG_INFO("Furnace lit at (" + std::to_string(pos.x) +
                        ", " + std::to_string(pos.y) + ", " +
                        std::to_string(pos.z) + ")");
        }
        if (ev.smeltCompleted) {
            m_sound.play("smelt", Vec3 {
                static_cast<float>(pos.x) + 0.5f,
                static_cast<float>(pos.y) + 0.5f,
                static_cast<float>(pos.z) + 0.5f});
            MC_LOG_INFO("Furnace smelt complete: " +
                        m_items.at(st.output.item).name + " at (" +
                        std::to_string(pos.x) + ", " +
                        std::to_string(pos.y) + ", " +
                        std::to_string(pos.z) + ")");
            // V2-3: the smelt.<item> tutorial event — the smelt-complete
            // consumption point (the product the furnace just output),
            // one fixed 1/60 substep per frame per state (hard rule 8).
            emitTutorialEvent("smelt." + m_items.at(st.output.item).name, 1);
            // V2-11 (XP): the smelt-complete grant — one orb worth the
            // PRODUCT's data smelt_xp (the smelt_xp default 1 unifies
            // every smelt — the documented simplification of Minecraft's
            // fractional table), at the furnace block's center raised
            // +0.5 (the cell above the block — the CRAFT-5 flame-spawn
            // position; the orb falls onto the furnace's face and rests,
            // the item convention). smelt_xp 0 = no orb (the data gate
            // — spawnExpOrb's belt makes it structural). Frame-locked
            // event boundary (hard rule 8: the smelt event fires on the
            // fixed 1/60 substep, the pool mutation lands on the frame).
            if (m_items.at(st.output.item).smeltXp > 0) {
                spawnExpOrb(m_items.at(st.output.item).smeltXp,
                            Vec3 {static_cast<float>(pos.x) + 0.5f,
                                  static_cast<float>(pos.y) + 1.0f,
                                  static_cast<float>(pos.z) + 0.5f});
            }
        }
    }

    // Campfire water extinguish (Phase 75): a LIT campfire with a WATER
    // fluid (any level) in one of its 6 neighbors goes out instantly —
    // water beats fire (the Phase 57/64 rule, the fire-block twin). V2-4:
    // the condition is the shared fire-family predicate extended with the
    // rain disjunct (extinguishedByWaterOrRain — the rain puts a lit
    // campfire out like the water does, no roof-shade check = the
    // documented deviation; the world seed never changes, so a no-input
    // run's first rain still lands only after kClearMin frames). The
    // cooking state SURVIVES (the items stay on the unlit logs: the
    // take-back works, the clocks pause); there is no re-light in v2
    // (documented deviation). A frame-state check (deterministic; the
    // fluid state is world state), run BEFORE the cook tick so an
    // extinguished campfire does not cook on its extinguish frame.
    // Fast-exit: a no-input run has no campfires (worldgen never emits
    // one) and the fresh world has no water next to one.
    if (!m_states.campfires.empty() && m_states.campfireLitId.valid() &&
        m_states.campfireExtId.valid()) {
        std::vector<BlockPos> goOut;
        for (const auto& entry : m_states.campfires) {
            const BlockPos& pos = entry.first;
            if (m_world.getBlock(pos.x, pos.y, pos.z) != m_states.campfireLitId) {
                continue; // already out (or the family id is stale)
            }
            if (extinguishedByWaterOrRain(m_world, pos, m_weather.raining)) {
                goOut.push_back(pos);
            }
        }
        // Apply AFTER the scan (setBlock touches the world state the scan
        // reads — the snapshot discipline, like the fire put-out pass).
        for (const BlockPos& pos : goOut) {
            setBlock(pos.x, pos.y, pos.z, m_states.campfireExtId);
            // Light 15 -> 0: re-light the touched region (the P64
            // fire-put-out pattern).
            Lighting::updateLocal(m_world, m_blocks, pos);
        }
    }

    // Campfires (Phase 71, the four-slot tick in Phase 75): one fixed
    // 1/60 substep per frame, per state (the day-cycle class — integer
    // substep counters, byte-reproducible; the furnace pace via
    // smeltSubstepsFor). std::map iteration is position-sorted: the tick
    // order is deterministic. Only LIT campfires cook (an extinguished
    // one holds its items, no progress). No-op on an empty state; runs
    // in every pointer/panel state (the world keeps ticking).
    for (auto& [pos, st] : m_states.campfires) {
        if (m_world.getBlock(pos.x, pos.y, pos.z) != m_states.campfireLitId) {
            continue;
        }
        tickCampfireCook(st, m_items);
    }

    // Crop growth (Phase 48): one fixed 1/60 substep per frame (the
    // day-cycle class — byte-reproducible). The pure roll lives in
    // world/growth (hash of world seed + frame + position); the position-
    // ordered set makes the step list deterministic. Advances go through
    // setBlock (chunk dirty marking + tracking-set maintenance). Runs in
    // every pointer/panel state (the world keeps ticking while a panel is
    // open, as in Minecraft). V2-4: the live rain state releases the
    // dry_slowdown gate (cropGrowthSteps' raining argument — off/clear =
    // false = the pre-V2-4 pace bit-identical).
    {
        const std::vector<GrowthStep> steps = cropGrowthSteps(
            m_world, m_blocks, m_states.crops,
            static_cast<std::uint64_t>(m_generator->seed()),
            static_cast<std::uint64_t>(m_frameCount), m_weather.raining);
        for (const GrowthStep& s : steps) {
            setBlock(s.pos.x, s.pos.y, s.pos.z, s.to);
            // Cross-shaped crops are non-opaque: the light field is
            // unchanged, but the local update is a harmless no-op kept for
            // consistency with every other block change.
            Lighting::updateLocal(m_world, m_blocks, s.pos);
        }
    }

    // GEN-2 sapling growth (the crop tick's twin, one fixed 1/60 substep
    // per frame — the day-cycle class, byte-reproducible): the pure roll
    // + gates live in world/growth (seed + frame + position); the steps
    // are state-only (Advance — the ENT-2 byte: NO light update, a state
    // change never alters the light, the documented constraint) or a full
    // tree placement (Complete — placeGrownTree relights the bbox ONCE).
    // Runs in every pointer/panel state, like the crops.
    {
        const std::vector<SaplingStep> steps = saplingGrowthSteps(
            m_world, m_blocks, m_states.saplings,
            static_cast<std::uint64_t>(m_generator->seed()),
            static_cast<std::uint64_t>(m_frameCount));
        for (const SaplingStep& s : steps) {
            if (s.action == SaplingAction::Advance) {
                m_world.setBlockState(s.pos.x, s.pos.y, s.pos.z,
                                      static_cast<std::uint8_t>(s.stage));
                continue; // the state change marks the chunk dirty (mesh);
                // the light is untouched (the ENT-2 constraint).
            }
            placeGrownTree(s.pos, s.height);
        }
    }

}

void Game::updateNetherFluidTick() {
    // The nether's fluid pass (the overworld section's dimension-bound
    // twin): the fluid tick over the nether world on the SAME frame-
    // locked cadence (every kFluidTickEveryNFrames, the honey gate's
    // twin). The static source lava sea (fillNetherChunk's full-level-8
    // cells) changes NOTHING (no level gradient, no flow proposal —
    // the recon's risk-surface ③ resolution: "全源静态 = 零 churn"): a
    // settled nether's walk re-reads its cells, re-goes-cold and
    // returns empty — no changed columns, no light jobs, the fast
    // exit. A player-poured lava source (the bucket, the nether allows
    // it) settles into the sea the same way it would in the overworld.
    if (m_frameCount % kFluidTickEveryNFrames == 0) {
        // The fluid tick number (frame-locked): the honey gate is its
        // period (the overworld's twin — honey is an overworld content,
        // the nether never holds it; the flag rides the same cadence
        // for the call's signature parity).
        const bool honeyActive =
            (m_frameCount / kFluidTickEveryNFrames) %
                kHoneyTickPeriodFluidTicks == 0;
        const std::vector<BlockPos> changed = FluidSim::tick(
            *m_nether.world, m_blocks, &m_nether.flammableChunks,
            honeyActive);
        // The nether holds no fire STATE (the D9 closed set {blocks,
        // fluids, states}): a burn-pass ignition (a player-placed
        // flamable meeting the sea) lands as a PERMANENT fire block —
        // no lifetime entry, no burn-out countdown (the documented
        // deviation; the nether fire segment is Phase C). So the
        // overworld's fire-registration loop has no twin here.
        for (const ColumnPos& col : changedCellColumns(changed)) {
            m_nether.fluidLightPending.insert(col);
        }
    }
}

void Game::updateEndFluidTick() {
    // V3-10a: the End's fluid pass (the updateNetherFluidTick's
    // dimension-bound twin — the same frame-locked cadence, the same
    // static zero-churn discipline). 10a's worldgen holds NO fluid (the
    // void + the endstone plateau), so a settled End's walk re-reads its
    // cells, re-goes-cold and returns empty — no changed columns, no
    // light jobs, the fast exit. A player-poured bucket settles the same
    // way it would in the overworld. The evaporation rule (the nether's
    // water-vanish) stays NETHER-scoped (the game_interaction predicate
    // — the End's water is plain water, the documented deviation).
    if (m_frameCount % kFluidTickEveryNFrames == 0) {
        // The fluid tick number (frame-locked): the honey gate is its
        // period (the nether's twin — the End holds no honey, the flag
        // rides the same cadence for the call's signature parity).
        const bool honeyActive =
            (m_frameCount / kFluidTickEveryNFrames) %
                kHoneyTickPeriodFluidTicks == 0;
        const std::vector<BlockPos> changed = FluidSim::tick(
            *m_end.world, m_blocks, &m_end.flammableChunks, honeyActive);
        // The End holds no fire STATE (the D9 closed-set twin — a burn-
        // pass ignition lands as a permanent fire block, the nether's
        // documented deviation). So the overworld's fire-registration
        // loop has no twin here.
        for (const ColumnPos& col : changedCellColumns(changed)) {
            m_end.fluidLightPending.insert(col);
        }
    }
}

void Game::updateOverworldFluidPass(double dt) {
    // Fluids tick at 20/s (every third frame at 60fps). Changed fluid
    // levels shift the light (water attenuates it). 99.11: the Phase 64
    // pattern — one SYNCHRONOUS updateRegion over the bounding box of
    // all changed cells, on the main thread — was the deep root of the
    // walking stutter: in the shipped ocean the coast film cascade
    // scatters 713 -> 918 -> 1357 changed cells per tick over hundreds
    // of columns, and that box's rescan cost 130 -> 396 -> 661 s per
    // 300-frame window while the tick itself was 20-38 ms (step-0
    // instrumentation: 99% of the fluid section's cost was the re-light).
    // Now each changed column is queued for the budgeted worker catch-up
    // (postFluidLightJobs: kFluidLightColumnsPerFrame per frame on the
    // 99.10 job infrastructure): the main-thread cost drops to set
    // inserts, and the light lands with a bounded per-column lag (the
    // convergence argument in world/light_job.hpp — each re-light fixes
    // its column exactly, so the drained set equals the full
    // propagation). A settled world changes nothing (the fast exit is
    // unchanged).
    if (m_frameCount % kFluidTickEveryNFrames == 0) {
        // The fluid tick number (frame-locked): the honey gate is its
        // period (FLUID-6 — honey simulates on every 3rd fluid tick).
        const bool honeyActive =
            (m_frameCount / kFluidTickEveryNFrames) %
                kHoneyTickPeriodFluidTicks ==
            0;
        const std::vector<BlockPos> changed = FluidSim::tick(
            m_world, m_blocks, &m_flammableChunks, honeyActive);
        // Phase 64: the burn pass (inside the tick above) turns flamable
        // blocks into FIRE blocks via world.setBlock — it bypasses the
        // Game setBlock choke point, so register the changed cells that
        // now hold fire with their full lifetime here (emplace keeps an
        // existing countdown; the registry without the fire block makes
        // this a no-op).
        if (m_states.fireId.valid()) {
            for (const BlockPos& p : changed) {
                if (m_world.getBlock(p.x, p.y, p.z) == m_states.fireId) {
                    m_states.fires.emplace(p, m_states.fireLifetime);
                }
            }
        }
        // The burn pass's fire cells ride the SAME `changed` vector: a
        // fire is a light-15 source, so its column needs the re-light
        // just like a shifted fluid level (changedCellColumns merges
        // every cell of one column into one entry).
        for (const ColumnPos& col : changedCellColumns(changed)) {
            m_fluidLightPending.insert(col);
        }
    }
    // GEN-9: the farmland wet/dry visual — a PERIODIC re-evaluation of
    // every tracked moisture cell (frame-locked cadence, the m_frameCount
    // discipline). The wetness is derived at MESH time from the live fluid
    // field (the mesher's moisture hook), but the mesh runs on a WORKER
    // over a chunk snapshot: a farmland chunk can be meshed BEFORE the
    // neighboring water chunk has streamed in (the cross-chunk fluid read
    // then sees void and bakes DRY — and nothing would re-mesh it). The
    // sweep is the convergence pass: once the neighborhood exists, the
    // next sweep (<= kMoistureRefreshEveryNFrames frames) sees the water,
    // flips the tracked state, and marks the chunk dirty. It also covers
    // pours/drains (the fluid edit is visible to the next sweep) — the
    // cost is |tracked| x 81 fluid lookups per sweep (zero while the
    // world holds no moisture block: the fresh world and the perf scenes
    // fast-exit on the empty map).
    if (!m_states.moistureBlocks.empty() &&
        m_frameCount % kMoistureRefreshEveryNFrames == 0) {
        refreshMoistureStates();
    }
    // Fire (Phase 64): the per-frame real-dt burn-out countdown (a fire
    // at zero becomes air — no char, like the Phase 58 lava burn) + the
    // water extinguish. Runs AFTER the fluids tick so the newly burned
    // cells are registered and the new water is visible; the block changes
    // it makes are meshed on the next frame (the effects' setBlock
    // pattern). A no-op while the world holds no fire.
    updateFires(dt);
    // CRAFT-5: the lit-furnace mouth flames (frame-locked emission; off
    // by render.furnace_flames = zero emissions).
    updateFurnaceFlames();
    // V3-15: the portal steam (frame-locked emission; off by
    // portal.steam = zero emissions; inert while the box holds no
    // portal).
    updatePortalSteam();
    // PRE-8: the burning-mob flames (frame-locked emission; off by
    // render.mob_flames = zero emissions; inert while no mob burns).
    updateMobFlames();
    // Phase 66 (Fire v2): the spread roll (fixed frame step, every
    // fire.spread_interval_frames frames). Runs AFTER the countdown so a
    // fire that just burned out does not spread on its last frame, and
    // the new fires' lights/meshes land on the next frame like any other
    // setBlock. A no-op while the world holds no fire.
    updateFireSpread();

}

void Game::explodeAt(const Vec3& center, double radius,
                     const std::string& sourceName) {
    // V3-14a: the blast's apply seam — the pure scan (world/explosion)
    // reads the player's dimension; the apply runs the setBlock choke
    // point (journal/relight/bookkeeping), the particles' seed
    // contract, the boom, and the falloff damage.
    World& world = currentWorld();
    const ExplosionScan scan = scanExplosion(world, m_blocks, center,
                                             radius);
    for (const BlockPos& c : scan.broken) {
        const BlockID id = world.getBlock(c.x, c.y, c.z);
        const float sky = static_cast<float>(world.getSkyLight(c.x, c.y,
                                                               c.z)) /
                          static_cast<float>(kMaxLight);
        const float blockLight = static_cast<float>(
                                     world.getBlockLight(c.x, c.y, c.z)) /
                                 static_cast<float>(kMaxLight);
        setBlock(c.x, c.y, c.z, BlockID::air());
        // The debris's seed contract (the breakBlock's twin: the world
        // seed + the shared counter's golden-ratio step — the blast's
        // cell order is the scan's deterministic loop order).
        const std::uint64_t rngSeed =
            currentSeed() +
            (++m_particleBreakCounter) * 0x9E3779B97F4A7C15ULL;
        m_particles.spawnBreak(c.x, c.y, c.z, id, sky, blockLight, rngSeed);
    }
    for (const BlockPos& c : scan.ignited) {
        setBlock(c.x, c.y, c.z, m_states.fireId);
        Lighting::updateLocal(world, m_blocks, c);
    }
    m_sound.play("boom");
    // The center's ember flash (the spawnFire's seed-contract twin —
    // the blast's own counter step, after the debris's).
    if (m_states.fireId.valid()) {
        const int fx = static_cast<int>(std::floor(center.x));
        const int fy = static_cast<int>(std::floor(center.y));
        const int fz = static_cast<int>(std::floor(center.z));
        const float sky = static_cast<float>(world.getSkyLight(fx, fy, fz)) /
                          static_cast<float>(kMaxLight);
        const float blockLight =
            static_cast<float>(world.getBlockLight(fx, fy, fz)) /
            static_cast<float>(kMaxLight);
        const std::uint64_t rngSeed =
            currentSeed() +
            (++m_particleBreakCounter) * 0x9E3779B97F4A7C15ULL;
        m_particles.spawnFire(fx, fy, fz, m_states.fireId, sky, blockLight,
                              rngSeed);
    }
    // The falloff damage: the player first (the creative's gate lives
    // in Player::damage — a creative player survives the blast
    // unharmed), then the mobs (their damage has no cause line — the
    // death's read is the player's only).
    const Vec3 ppos = m_player->position();
    const double pdx = ppos.x - center.x;
    const double pdy = ppos.y - center.y;
    const double pdz = ppos.z - center.z;
    const double pdist = std::sqrt(pdx * pdx + pdy * pdy + pdz * pdz);
    const int pdmg = explosionDamage(pdist, radius, kExplosionBaseDamage);
    if (pdmg > 0) {
        m_player->damage(static_cast<float>(pdmg), false,
                         {Player::DamageSource::Explosion, sourceName});
    }
    for (auto& entity : m_entities) {
        MobEntity* mob = dynamic_cast<MobEntity*>(entity.get());
        if (mob == nullptr) {
            continue;
        }
        const Vec3 mpos = mob->position();
        const double mdx = mpos.x - center.x;
        const double mdy = mpos.y - center.y;
        const double mdz = mpos.z - center.z;
        const double mdist =
            std::sqrt(mdx * mdx + mdy * mdy + mdz * mdz);
        const int mdmg =
            explosionDamage(mdist, radius, kExplosionBaseDamage);
        if (mdmg > 0) {
            mob->damage(static_cast<float>(mdmg));
        }
    }
}

} // namespace mc
