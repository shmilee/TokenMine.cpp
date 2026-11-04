#include "core/game.hpp"

#include <algorithm>
#include <cmath>

#include "core/third_person.hpp"
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

void Game::update(double dt) {
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
    DimCtx activeCtx = playerInNether() ? netherCtx() : overworldCtx();
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
    if (m_tutorialToastFrames > 0) {
        --m_tutorialToastFrames;
    }

    // V3-1 Phase B (D3 time-capsule ruling): the overworld's world pass
    // (the per-block states + the growth family) is FROZEN while the
    // player is in the nether — the inactive dimension ticks nothing,
    // its state is a pure function of its last active frame (the S2
    // return-leg gate's "OW savenorm ≡ pre-entry except the global
    // time" rests on it; the nether's twin pass runs in the nether
    // section below). In the overworld (the default) this is the exact
    // pre-Phase-B sequence — the bit-identical red line.
    if (!playerInNether()) {
        updateOverworldWorldPass();
    }

    // Phase 15 UI keys: F3/F5 work in any pointer state; E opens the
    // inventory (releasing the pointer) or closes it.
    handleUiKeys();
    // While the inventory is open, player physics and block interaction
    // are paused; the world keeps ticking (day cycle, fluids, entities,
    // streaming) — see docs/KNOWN_ISSUES.
    if (panelOpen()) {
        handlePanelClicks();
    } else {
        handleInteraction(dt);
    }

    // V3-1 Phase B (D3 time-capsule ruling, the section-1 twin): the
    // overworld's fluid / fire / moisture pass is FROZEN in the nether;
    // the nether's twin (the fluid tick over the static source lava sea
    // — zero churn, the recon's risk-surface ③ resolution) runs
    // instead.
    if (playerInNether()) {
        updateNetherFluidTick();
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
    }

    // The player owns the first-person state; the camera is just its view
    // (eye position + orientation). Mouse look is only active while the
    // pointer is captured, so an unlocked cursor never rotates the view.
    // Paused while the inventory is open (Phase 15) or the player is dead
    // (Phase 16: the death screen freezes the body; R respawns).
    if (!panelOpen() && !m_player->isDead()) {
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
        const Vec3 tutPosBefore = m_player->position();
        const float tutVyBefore = m_player->velocity().y;
        const bool tutGroundedBefore = m_player->onGround();
        m_player->update(dt, m_input, m_window.relativeMouseMode());
        const Vec3 tutPosAfter = m_player->position();
        if (tutPosAfter.x != tutPosBefore.x ||
            tutPosAfter.z != tutPosBefore.z) {
            emitTutorialEvent("walk", 1);
        }
        if (tutGroundedBefore && m_player->velocity().y > tutVyBefore &&
            !m_player->inWater() && !m_player->inLava()) {
            emitTutorialEvent("jump", 1);
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
    // UX-4: first person keeps the legacy assignment bit-exactly (the
    // camera is the eye). Third person orbits kThirdPersonDistance from
    // the eye on the view's own sphere and clamps against the first
    // occluding block face (the pure read-only resolve in
    // core/third_person — the campfire pattern). V2-1: the front view is
    // the SAME resolve executed at the flipped view angles
    // (thirdPersonFrontAngles) — the camera in front of the player,
    // looking at it. The mode is session state (never saved, defaults
    // to First), so a no-input run stays on the legacy path.
    m_camera.yaw = m_player->yaw;
    m_camera.pitch = m_player->pitch;
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
            thirdPersonOrbitCenter(m_player->eyePosition(), m_player->yaw,
                                   m_player->pitch),
            m_player->yaw, m_player->pitch, m_thirdPersonDistance);
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
            thirdPersonFrontAngles(m_player->yaw, m_player->pitch);
        m_camera.yaw = fa.yaw;
        m_camera.pitch = fa.pitch;
        // V2-19: the same wheel-zoomed session distance (the front view
        // zooms identically — one value per session, the design's
        // single-value sharing).
        m_camera.position = resolveThirdPersonCamera(
            currentWorld(), m_blocks,
            thirdPersonOrbitCenter(m_player->eyePosition(), m_player->yaw,
                                   m_player->pitch),
            fa.yaw, fa.pitch, m_thirdPersonDistance);
    } else {
        m_camera.position = m_player->eyePosition(); // First: legacy
    }

    // SURV-2: the oxygen / drowning pass (the frame-locked class — driven
    // by m_frameCount, not dt, like the day cycle; the pure step + the
    // head predicate live in player/drown, the unit tests own them — the
    // campfire pattern). Runs in every pointer/panel state (the player's
    // physics pauses behind a panel, the air does not — the world keeps
    // ticking, the Minecraft behavior); after a death it is naturally
    // inert (Player::damage no-ops on the frozen body, and the death
    // transition in updateSurvival refills the state for the respawn).
    {
        const float dmg = drownStep(m_drown, headInFluid(*m_player, currentWorld()),
                                    static_cast<unsigned>(m_frameCount));
        if (dmg > 0.0f) {
            // Drowning BYPASSES the armor reduction (the wiki damage
            // table: Drowning = "Armor: No effect"); the resistance
            // effect and the absorption pool still apply (damage()'s
            // pipeline, the bypass flag skips the armor step only).
            m_player->damage(dmg, /*bypassArmor=*/true);
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
    // refresh its NAMED effect on the player while the player is in range.
    // The level is a pure function of the world (world/beacon) — no stored
    // state. A no-op (fast-exit) while the world holds no beacon OR the
    // player is dead (structural immunity: a fresh no-input world has no
    // beacon, so m_states.beacons is empty and this never runs its body).
    m_states.beaconActive.clear();
    if (m_states.beacons.empty() || m_player->isDead()) {
        return;
    }
    // The absorption pool is a pure function of the effect total at the
    // sync points. updateEffects only detects a change if the total moves
    // DURING its own ticks; the beacon applies its effect HERE (before
    // updateEffects), so capture the total around the applies and sync the
    // pool if the beacon raised it (the fire-source pattern: standing in
    // range keeps the layer at its full total; damage still drains it
    // between sync points, Phase 72).
    const float absorbBefore = m_playerEffects.totalAbsorption(m_effects);
    const Vec3 feet = m_player->position();
    for (const auto& [pos, bid] : m_states.beacons) {
        const int level = beaconLevel(m_world, m_blocks, pos.x, pos.y, pos.z);
        if (level < 1) {
            continue;
        }
        // An active beacon feeds the beam renderer (drawn in the blending
        // pass; the beam is gated on the `beacon.beam` config there).
        m_states.beaconActive.push_back(BeaconBeam {pos.x, pos.y, pos.z, level});
        // In range? Chebyshev distance from the player's feet to the beacon
        // cell's centre (the voxel-cube "within N blocks" rule).
        const float dx = feet.x - (pos.x + 0.5f);
        const float dy = feet.y - (pos.y + 0.5f);
        const float dz = feet.z - (pos.z + 0.5f);
        const float dist = std::max(std::fabs(dx),
                                    std::max(std::fabs(dy), std::fabs(dz)));
        if (dist > static_cast<float>(m_states.beaconRange)) {
            continue;
        }
        // Apply the beacon's EFFECT LIST (Phase 91; each name is
        // init-validated to exist). The duration of every effect refreshes
        // each frame while in range (the fire-source pattern — standing in
        // the beam keeps the effects alive). A single-effect beacon (the
        // Phase 83 `beacon_effect`) is a 1-element list.
        for (const std::string& effName :
             m_blocks.at(bid).properties.beaconEffects) {
            const int eff = m_effects.idForName(effName);
            if (eff != -1) {
                m_playerEffects.apply(eff, kBeaconEffectDurationSeconds);
            }
        }
    }
    if (m_playerEffects.totalAbsorption(m_effects) != absorbBefore) {
        syncAbsorption();
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
    // (the dayFactor's 0.5 cutoff).
    const Vec3 feet = m_player->position();
    const int cx = static_cast<int>(std::floor(feet.x));
    const int cy = static_cast<int>(std::floor(feet.y));
    const int cz = static_cast<int>(std::floor(feet.z));
    // V3-1 Phase B: the CURRENT world (the nether's skyless void reads
    // raw sky 0 = the cave context — the nether's ambience is the cave
    // bed, the Minecraft "no sky" rule's twin).
    const int rawSky = currentWorld().getSkyLight(cx, cy, cz);
    const AmbienceContext ctx =
        ambienceContext(rawSky, m_dayCycle.dayFactor());
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
        m_sound.play("portal_trigger");
        MC_LOG_INFO("Portal entry: travel countdown armed");
    }
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

void Game::emitTutorialEvent(const std::string& name, int n) {
    // UX-7 / V2-3: the single tutorial-event choke point (the thin
    // adapter — the state machine's logic is unit-test-owned in
    // core/tutorial). Gated: disabled or done = ZERO execution (no
    // event ever fires without input — the no-input run's structural
    // immunity — and a done/disabled tutorial never re-runs the
    // machine, the skip key no-ops through this same gate).
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
} // namespace mc
