#include "core/game.hpp"

#include <GL/glew.h>
#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <numbers>
#include <sstream>

#include "block/cube_face_table.hpp"
#include "core/json.hpp"
#include "core/png.hpp"
#include "core/bow.hpp"
#include "core/difficulty.hpp"
#include "core/ui_layout.hpp"
#include "core/math/frustum.hpp"
#include "entity/arrow_mesh.hpp"
#include "entity/item_entity.hpp"
#include "entity/item_light.hpp"
#include "entity/item_model_mesh.hpp"
#include "entity/mob_entity.hpp"
#include "entity/mob_mesh.hpp"
#include "entity/mob_swing.hpp"
#include "block/chance_drop.hpp"
#include "world/ambience.hpp"
#include "world/cloud.hpp"
#include "world/growth.hpp"
#include "world/fire_spread.hpp"
#include "world/beacon.hpp"
#include "world/beam_light.hpp"
#include "world/light_job.hpp"
#include "world/lighting.hpp" // kMaxLight
#include "world/mesh_job.hpp"
#include "world/noise.hpp"
#include "ui/font.hpp"
#include "ui/inventory_ui.hpp"
#include "ui/item_tooltip.hpp"
#include "ui/ui_scale.hpp"
#include "item/armor.hpp"
#include "item/durability.hpp"
#include "block/block_sound.hpp"
#include "renderer/alpha_test.hpp"
#include "renderer/fog.hpp"

#include "core/game_local.hpp"

namespace mc {


namespace {

// Phase 64: the six face-adjacent offsets — the fire extinguish's
// "water 6-adjacent" scan (the same neighborhood the fluid sim uses).
constexpr BlockPos kFireNeighbors[6] = {
    {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1},
};

constexpr float kFovDegrees = 70.0f;
constexpr float kNear = 0.1f;
constexpr float kFar = 1000.0f;
// PRE-3 water ripples: the shimmer's phase period in FRAMES (2.0 s at the
// 60 FPS cap — the frame-locked class, hard rule 8; the phase is a pure
// function of m_frameCount, so the animation is byte-reproducible and the
// period is a named constant with no config key). The u_ripplePhase uniform
// sweeps 0..2*pi over the period; the frame MODULO keeps the sin argument
// bounded (float precision loss over a long session).
constexpr int kRipplePeriodFrames = 120;

// Sprite sizes in pixels (billboarded quads — see sky.vert).
constexpr float kStarSpriteSize = 2.0f;
constexpr float kSunSpriteSize = 40.0f;
constexpr float kMoonSpriteSize = 28.0f;
// Sun/moon fade-out: alpha = clamp(elevation / kHorizonFade, 0, 1), so they
// are fully visible a little above the horizon and invisible on/below it.
constexpr float kHorizonFade = 0.1f;
// Day/night time advances by this fixed amount per frame (not the wall
// clock), so headless runs are byte-reproducible frame for frame; the
// real-time speed is exact at 60 FPS.
constexpr double kDayStepPerFrame = 1.0 / 60.0;

// Mobs (Phase 20) and arrows (Phase 21) — every MOVING entity — simulate
// on the SAME fixed per-frame step: a moving body's position at frame N
// must be a pure function of N (the day cycle and the item animation set
// the precedent — real dt varies with load, and a moving body's final
// position would drift by sub-pixels between runs). At the 60 FPS cap the
// real-time speed is exact; at lower caps the body slows down with the
// frame clock (the day cycle's behavior).
constexpr double kMobStepPerFrame = 1.0 / 60.0;
// Arrows share the moving-entity convention (Phase 21).
constexpr double kProjectileStepPerFrame = 1.0 / 60.0;
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
// CRAFT-7: the double-click collect window (real dt, the survival-timer
// class — input-only state, a no-input run never touches it). 0.3 s is
// the Minecraft-style "fast double tap" band: wide enough for xdotool's
// ~50 ms click jitter (the E2E protocol), narrow enough that a deliberate
// pick-and-place (>= a glance) never collects by accident.
constexpr double kDoubleClickSeconds = 0.3;
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
constexpr float kBeaconBeamBaseHeight = 64.0f;
constexpr float kBeaconBeamLevelHeight = 32.0f;
// Phase 83 beacon beam color (bright cyan-white, the classic beam) and its
// translucency (the beam is a light column, not a solid pillar).
constexpr float kBeaconBeamR = 0.6f;
constexpr float kBeaconBeamG = 0.95f;
constexpr float kBeaconBeamB = 1.0f;
constexpr float kBeaconBeamAlpha = 0.5f;

// Reinterpret an (possibly negative) int as 64 bits for hashing (the
// worldgen's u64 — two's-complement is a bijection).
std::uint64_t asU64(int v) {
    return static_cast<std::uint64_t>(static_cast<std::int64_t>(v));
}

// How far the crosshair raycast reaches (blocks; Minecraft's reach).
constexpr float kReach = 5.0f;

// Mobs (Phase 20): the player's attack cadence + the knockback a MELEE hit
// applies (the horizontal speed along the aim + a vertical lift; the
// MobEntity's knockback window decays it). Phase 42: an ARROW hit knocks
// back with core/bow.hpp arrowKnockback — the same base values, scaled by
// the shot's charge power.
constexpr double kSwingInterval = 0.25;  // seconds between attack hits
constexpr float kKnockbackSpeed = 4.0f;  // horizontal knockback (blocks/s)
constexpr float kKnockbackLift = 2.5f;   // vertical knockback (blocks/s)

// Bow (Phase 21): the arrow spawns this far in front of the eye (the
// launch origin never sits inside the camera near plane).
constexpr float kArrowSpawnOffset = 0.3f;

// Arrow pickup (Phase 42): a STUCK arrow within this radius of the
// player's bottom-center returns to the inventory as the launcher's ammo
// item (Minecraft: walk up to a stuck arrow to pick it up; flying arrows
// are not pickable).
constexpr float kArrowPickupRadius = 1.0f;

// A dropped item is picked up when its bottom-center is within this
// distance of the player's bottom-center (and past the item's pickup
// delay). Minecraft pulls items in gradually (magnetism); the fixed
// radius is the v1 — see docs/KNOWN_ISSUES.md.
constexpr float kPickupDistance = 1.0f;

// Dropped-item animation (Phase 10): derived from the frame counter, not
// the wall clock, so headless runs stay byte-reproducible. At 60 FPS the
// pickup bobs with a 1.5 s period (amplitude 0.05 blocks) and spins once
// every 4 s, like Minecraft's item pickup.
constexpr int kItemBobPeriodFrames = 90;
constexpr int kItemSpinPeriodFrames = 240;
constexpr float kItemBobAmplitude = 0.05f;

// Phase 15 UI (screen-space, pixels; the font is 16x16 at scale 1,
// Phase 35).
constexpr int kHotbarSlotSize = 44;
constexpr int kHotbarSlotGap = 2;
constexpr int kHotbarBottomMargin = 8;
constexpr int kHotbarIconPx = 30;
constexpr int kHeldIconPx = 76;
constexpr int kHeldIconOffset = 72; // center of the held icon from the corner
// 3D held hand (Phase 42): the view-anchored hand model constants (see
// drawHeldHand). The offset is in the CAMERA frame (x = right, y = up,
// z = behind the eye), so the hand sits low-right in front of the eye.
constexpr Vec3 kHandOffset {0.42f, -0.38f, -0.85f};
constexpr float kHandCubeScale = 0.28f; // block-cube hand size
constexpr float kHandIconScale = 0.5f;  // icon-quad hand size
constexpr float kHandSpinX = -0.35f; // tilt the top toward the camera
constexpr float kHandSpinY = 0.6f;   // turn a corner toward the camera
// Bow draw (Phase 42, replaces the Phase 24 2D icon pull): the hand pulls
// back toward the face and the bow tilts back with the charge power.
constexpr float kHandDrawPullZ = 0.3f;
constexpr float kHandDrawSpinX = 0.9f;
// Eat channel (Phase 42): the hand rises toward the face with the eat
// progress (replaces the Phase 28 2D icon pull).
constexpr float kHandEatLiftY = 0.35f;
constexpr float kHandEatPullZ = 0.35f;
constexpr int kF3Margin = 8;
constexpr int kF3LineHeight = 18; // 16 px glyph (Phase 35) + 2 px gap
// The icon cubes are drawn at a fixed tilt (deterministic; no bob).
constexpr float kIconTiltYaw = 0.65f;
constexpr float kIconTiltPitch = -0.35f;

// Phase 15 sound cadence (seconds).
constexpr double kDigSoundInterval = 0.3;
constexpr double kStepSoundInterval = 0.4;
constexpr float kStepMinSpeed = 0.5f; // blocks/s horizontal to count as walking

// Phase 16 survival: the red damage veil's lifetime (it fades linearly to
// 0 over this window after a damage event).
constexpr double kHurtFlashSeconds = 0.5;

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
    if (m_pool) {
        m_pool->flush();
    }
    collectLitColumns();
    integrateMeshJobs();
    integrateStreamedColumns();
    unloadFarColumns();

    // Day/night: a fixed per-frame step (see kDayStepPerFrame) keeps
    // headless runs byte-reproducible.
    m_dayCycle.update(kDayStepPerFrame);

    // Furnaces (Phase 40, per-block Phase 44): one fixed 1/60 substep per
    // frame, per state (the day-cycle class — integer substep counters,
    // byte-reproducible). std::map iteration is position-sorted: the tick
    // order is deterministic. No-op on an empty state; runs in every
    // pointer/panel state (the world keeps ticking while the panel is
    // open, as in Minecraft).
    for (auto& [pos, st] : m_furnaces) {
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
        }
    }

    // Campfire water extinguish (Phase 75): a LIT campfire with a WATER
    // fluid (any level) in one of its 6 neighbors goes out instantly —
    // water beats fire (the Phase 57/64 rule, the fire-block twin). The
    // cooking state SURVIVES (the items stay on the unlit logs: the
    // take-back works, the clocks pause); there is no re-light in v2
    // (documented deviation). A frame-state check (deterministic; the
    // fluid state is world state), run BEFORE the cook tick so an
    // extinguished campfire does not cook on its extinguish frame.
    // Fast-exit: a no-input run has no campfires (worldgen never emits
    // one) and the fresh world has no water next to one.
    if (!m_campfires.empty() && m_campfireLitId.valid() &&
        m_campfireExtId.valid()) {
        std::vector<BlockPos> goOut;
        for (const auto& entry : m_campfires) {
            const BlockPos& pos = entry.first;
            if (m_world.getBlock(pos.x, pos.y, pos.z) != m_campfireLitId) {
                continue; // already out (or the family id is stale)
            }
            for (const BlockPos& d : kFireNeighbors) {
                if (m_world.getFluidOfKind(pos.x + d.x, pos.y + d.y,
                                           pos.z + d.z,
                                           FluidKind::Water) > 0) {
                    goOut.push_back(pos);
                    break;
                }
            }
        }
        // Apply AFTER the scan (setBlock touches the world state the scan
        // reads — the snapshot discipline, like the fire put-out pass).
        for (const BlockPos& pos : goOut) {
            setBlock(pos.x, pos.y, pos.z, m_campfireExtId);
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
    for (auto& [pos, st] : m_campfires) {
        if (m_world.getBlock(pos.x, pos.y, pos.z) != m_campfireLitId) {
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
    // open, as in Minecraft).
    {
        const std::vector<GrowthStep> steps = cropGrowthSteps(
            m_world, m_blocks, m_crops,
            static_cast<std::uint64_t>(m_generator->seed()),
            static_cast<std::uint64_t>(m_frameCount));
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
            m_world, m_blocks, m_saplings,
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
        if (m_fireId.valid()) {
            for (const BlockPos& p : changed) {
                if (m_world.getBlock(p.x, p.y, p.z) == m_fireId) {
                    m_fires.emplace(p, m_fireLifetime);
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
    if (!m_moistureBlocks.empty() &&
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

    // Column-merged GL upload (PIPE-1: the mesher's compute runs on the
    // workers — postMeshJobs / integrateMeshJobs feed m_chunkMeshData;
    // this frame's integrates' clear-dirty is the only input here).
    {
        const auto meshes = updateChunkMeshes();
        if (!meshes.ok()) {
            MC_LOG_ERROR("Chunk meshing failed: " + meshes.error().describe());
        }
    }

    // The player owns the first-person state; the camera is just its view
    // (eye position + orientation). Mouse look is only active while the
    // pointer is captured, so an unlocked cursor never rotates the view.
    // Paused while the inventory is open (Phase 15) or the player is dead
    // (Phase 16: the death screen freezes the body; R respawns).
    if (!panelOpen() && !m_player->isDead()) {
        m_player->update(dt, m_input, m_window.relativeMouseMode());
    }
    m_camera.position = m_player->eyePosition();
    m_camera.yaw = m_player->yaw;
    m_camera.pitch = m_player->pitch;

    // SURV-2: the oxygen / drowning pass (the frame-locked class — driven
    // by m_frameCount, not dt, like the day cycle; the pure step + the
    // head predicate live in player/drown, the unit tests own them — the
    // campfire pattern). Runs in every pointer/panel state (the player's
    // physics pauses behind a panel, the air does not — the world keeps
    // ticking, the Minecraft behavior); after a death it is naturally
    // inert (Player::damage no-ops on the frozen body, and the death
    // transition in updateSurvival refills the state for the respawn).
    {
        const float dmg = drownStep(m_drown, headInFluid(*m_player, m_world),
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
    updateBeacons();
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
    streamChunks();
    // Worker meshing (PIPE-1): snapshot + post for the nearest eligible
    // dirty columns. AFTER streamChunks and BEFORE postLightJobs — the
    // post order gen -> mesh -> light keeps this frame's lighting job the
    // only live-World writer after the snapshots (see postMeshJobs).
    postMeshJobs();
    // Lighting for chunks that appeared this frame (streamed-in columns,
    // or the placement path's synchronous creation): up to two worker
    // jobs (99.10) — two bit-level-independent column boxes run in
    // parallel (planLightJobs), a dependent pair stays one job with the
    // boxes sequential (the pre-99.10 behavior). The affected chunks
    // render one frame dark (light 0) and are re-meshed with the correct
    // light the frame after the jobs complete — deterministic,
    // frame-locked by the flush.
    postLightJobs();
    // 99.11: the fluid catch-up light jobs — the budgeted worker drain
    // of m_fluidLightPending (filled by the fluid tick above; the old
    // synchronous bounding-box re-light there was the frame's entire
    // cost in the shipped ocean, step-0: 130..661 s per 300-frame
    // window). LAST of the posts: it reads the streaming job columns of
    // this window from m_lightJobs for the cross-kind exclusion, and
    // posts nothing while a candidate is not lightRegionsIndependent of
    // them (the 99.10 parallel safety is per-plan). The reap + the
    // dirtyOut application land at the next frame's collectLitColumns —
    // the fluid columns' chunks re-mesh with the new light then (99.4).
    postFluidLightJobs();
}

// ---------------------------------------------------------------------------
// Pointer lock + block interaction (Phase 6)
// ---------------------------------------------------------------------------

void Game::handleInteraction(double dt) {
    // CRAFT-3: the listener for the spatialized sounds (the material dig /
    // place / break at the block, the pickup at the item) — the camera
    // state at the frame start. The camera was synced from the player at
    // the END of the previous frame (the player physics run after this in
    // the frame), so the listener is one frame stale: zero cost, zero
    // non-determinism — audio is outside the frame state (hard rule 8).
    // The 2D paths (step / shoot / eat / ui_*, the held-item breaks)
    // ignore the listener entirely.
    m_sound.setListener(m_camera.position, m_camera.yaw);
    const bool locked = m_window.relativeMouseMode();
    if (!locked) {
        // Left click captures the pointer (standard first-person behavior)
        // — but never while dead (the death screen owns the input; R
        // respawns first).
        if (!m_player->isDead() && m_input.wasPressed(MouseButton::Left)) {
            m_window.setRelativeMouseMode(true);
            // Debug line: the E2E input scripts verify the capture landed
            // by waiting for this (a covering window can steal the click
            // silently — P25-class pollution, 2026-10-21 STRM-1 E2E).
            MC_LOG_DEBUG("Pointer captured");
        }
        resetBreaking();
        m_swingCooldown = 0.0;
        resetBowDraw();
        resetEatChannel();
        return;
    }

    if (m_input.wasPressed(GameKey::Escape)) {
        m_window.setRelativeMouseMode(false);
        resetBreaking();
        m_swingCooldown = 0.0;
        resetBowDraw();
        resetEatChannel();
        return;
    }
    if (m_input.wasPressed(MouseButton::Right)) {
        // Campfire cooking (Phase 71): a campfire under the crosshair
        // owns the click BEFORE the item channels — Minecraft places the
        // held food on the fire instead of starting to eat (essential:
        // every smeltable item in this project is a food, so without the
        // precedence the cooking slot could never be filled). The click
        // is consumed either way (a campfire never places through the
        // right button).
        if (const std::optional<BlockHit> hit = targetBlock(); hit) {
            const Block& b = m_blocks.at(
                m_world.getBlock(hit->block.x, hit->block.y,
                                 hit->block.z));
            if (b.properties.interact == kInteractCampfire) {
                tryCampfireCook(hit->block);
                return;
            }
        }
        // Food (Phase 23): a selected food item is EATEN, not used — the
        // click is consumed (no interact, no placement). At full hunger the
        // eat is a no-op but the click is still consumed (Minecraft: you
        // cannot place while "eating" a full bar).
        if (tryEat()) {
            return;
        }
        // Drinkable container (Phase 43): a selected item with a
        // 'drink_time' is DRUNK, not used — the same hold channel as the
        // eat (data-driven duration); completing it consumes one, returns
        // the empty container and clears the status effects (v1: the hurt
        // flash). The click is consumed either way (no interact, no
        // placement — a milk bucket in hand never places).
        if (tryDrink()) {
            return;
        }
        // Launcher (Phase 24): the right button is the DRAW. Starting it
        // consumes the click entirely (no interact, no placement — a bow
        // in hand never places). The draw starts only when ammo is
        // available (Minecraft: no draw without arrows).
        if (selectedShootSpeed() > 0.0f) {
            if (ammoAvailable()) {
                m_bowDrawing = true;
                m_bowDrawSeconds = 0.0;
                m_sound.play("bow");
            }
            return;
        }
        // Milking (Phase 43, SURV-10): a mob whose data pairs
        // 'milk_with' with the selected item's name, under the crosshair
        // (melee targeting: kReach + block occlusion), is MILKED — one
        // held item consumed, one 'milk_gives' item added, the mob
        // unharmed (no damage, death or drop) but put on the 5-minute
        // Minecraft milking cooldown (re-milking early does nothing; the
        // click is still consumed, the failed-milk rule).
        if (tryMilk()) {
            return;
        }
        // Data-driven interaction (Phase 17): a block whose `interact`
        // property names a known GUI opens it instead of placing.
        if (const std::optional<BlockHit> hit = targetBlock(); hit) {
            const Block& b = m_blocks.at(
                m_world.getBlock(hit->block.x, hit->block.y,
                                 hit->block.z));
            if (b.properties.interact == kInteractCraftingTable) {
                // CRAFT-1: the panel is bound to THIS block's grid
                // (per-block table states — the Phase 44 furnace pattern).
                openTablePanel(hit->block);
                return;
            }
            if (b.properties.interact == kInteractFurnace) {
                // Phase 44: the panel is bound to THIS block's state.
                openFurnacePanel(hit->block);
                return;
            }
            // The campfire (Phase 71) is handled at the TOP of this
            // handler, before the item channels (see above).
        }
        // Dye recoloring (Phase 41): a selected dye item right-clicked on
        // a dye-family block converts the block to the dyed variant
        // (dyeVariantName) and consumes one dye — instant (no channel,
        // Minecraft) and the click is consumed (no placement). A block
        // already in that color is left alone and the click falls through
        // (the dye is not a block item, so placeBlock is a no-op — the
        // documented v1 rule: a duplicate dye is not consumed).
        if (const std::optional<BlockHit> hit = targetBlock(); hit) {
            const Block& b = m_blocks.at(
                m_world.getBlock(hit->block.x, hit->block.y,
                                 hit->block.z));
            if (!b.properties.dyeBase.empty()) {
                const ItemStack& held = m_inventory.at(m_selectedSlot);
                if (!held.empty() &&
                    !m_items.at(held.item).dyeColor.empty()) {
                    const std::string variant =
                        dyeVariantName(b.properties.dyeBase,
                                       m_items.at(held.item).dyeColor);
                    if (variant != b.name) {
                        // Game init (family completeness) guarantees the
                        // variant block exists; the null guard is a
                        // defensive no-op.
                        if (const Block* v = m_blocks.findByName(variant);
                            v != nullptr) {
                            // Game::setBlock (Phase 42): also releases any
                            // arrow stuck in the recolored cell.
                            setBlock(hit->block.x, hit->block.y,
                                     hit->block.z, v->id);
                            Lighting::updateLocal(m_world, m_blocks,
                                                  hit->block);
                            m_inventory.take(m_selectedSlot, 1);
                            m_sound.play("cloth");
                        }
                        return;
                    }
                }
            }
        }
        // Fluid bucket (Phase 47): a selected bucket side collects a
        // source cell or pours one — the click is consumed either way
        // (a bucket never places a block). Runs AFTER the block
        // interactions: a furnace or crafting table under the crosshair
        // still opens its panel even when a bucket is selected (Minecraft).
        if (tryFluidBucket()) {
            return;
        }
        // Ignition (Phase 60, fire visuals Phase 64, the relight target
        // Phase 76): a selected item with the 'ignites_flamable' flag
        // acts on the block under the crosshair — (a) a block with a
        // non-empty `ignites_to` is set to THAT block in place (the
        // RELIGHT: campfire_extinguished -> campfire, its cooking state
        // survives the setBlock choke point and resumes — Minecraft:
        // flint and steel relights an unlit campfire), (b) otherwise a
        // `flamable` block burns — setBlock(FIRE), NO drops (the Phase 58
        // lava-burn semantics). Neither consumes the item (Minecraft: the
        // flint and steel survives its uses) and both play "ignite". The
        // fire block is a light source that burns out after
        // fire.lifetime_seconds (the setBlock choke point starts its
        // countdown) and is put out by water. Runs AFTER the panel
        // interactions: a furnace or crafting table under the crosshair
        // still opens its panel even when an igniter is selected. Other
        // targets fall through (the item is a material, so placeBlock is
        // a no-op).
        if (const ItemStack& held = m_inventory.at(m_selectedSlot);
            !held.empty() && m_items.at(held.item).ignitesFlamable) {
            if (const std::optional<BlockHit> hit = targetBlock(); hit) {
                const Block& b = m_blocks.at(
                    m_world.getBlock(hit->block.x, hit->block.y,
                                     hit->block.z));
                if (!b.properties.ignitesTo.empty()) {
                    // (a) the relight target (Phase 76): set the named
                    // block in place. The name is init-validated (the
                    // grows_to discipline), so idForName cannot fail.
                    const BlockID toId =
                        m_blocks.idForName(b.properties.ignitesTo);
                    setBlock(hit->block.x, hit->block.y, hit->block.z,
                             toId);
                    Lighting::updateLocal(m_world, m_blocks, hit->block);
                    m_sound.play("ignite");
                    // SURV-3: the successful relight wears the held
                    // igniter one point (the action completed — a
                    // flint and steel that breaks is simply gone).
                    if (m_items.at(held.item).durability > 0.0f) {
                        if (applyDurability(m_inventory.at(m_selectedSlot),
                                            kIgniteWear, m_items)) {
                            m_sound.play("break"); // it broke on the
                                                   // successful ignition
                        }
                    }
                } else if (b.properties.flamable) {
                    // (b) m_fireId is valid here: a flamable target
                    // exists, and the init cross-check requires the fire
                    // block whenever the registry holds one.
                    setBlock(hit->block.x, hit->block.y, hit->block.z,
                             m_fireId);
                    Lighting::updateLocal(m_world, m_blocks, hit->block);
                    m_sound.play("ignite");
                    // SURV-3: the successful burn wears the held igniter
                    // one point (the action completed).
                    if (m_items.at(held.item).durability > 0.0f) {
                        if (applyDurability(m_inventory.at(m_selectedSlot),
                                            kIgniteWear, m_items)) {
                            m_sound.play("break"); // it broke on the
                                                   // successful ignition
                        }
                    }
                }
            }
        }
        // Armor (SURV-1): a held piece equips (an occupied slot swaps
        // it with the held one), an empty hand unequips the first worn
        // slot (the fixed order head -> chest -> legs -> feet) — the
        // click is consumed whenever it acts (armor is a material,
        // placeBlock would be a no-op; the channel owns the right
        // button, so it runs BEFORE the sneak-rotate: equipping is an
        // item action even while sneaking — Minecraft). Runs AFTER the
        // item channels + the igniter: a campfire under the crosshair
        // still cooks (it owns the click at the top of the handler),
        // food still eats, a bucket still scoops, an igniter still
        // lights (the channel-ownership precedence, unchanged).
        if (tryArmor()) {
            return;
        }
        // Block-state rotation (Phase 92 stair ring, per-instance in
        // Phase 94, generalized in ENT-2): a SNEAKING right-click on a
        // stateful block (hasState()) rotates it IN PLACE — the per-instance
        // state (the World's per-cell byte, the project's first per-block
        // state) advances (state + 1) % stateCount (a stair: (facing + 1) %
        // 4, the shipped behavior unchanged). The click is consumed (no
        // placement). Minecraft: sneak + right-click rotates a stair. Runs
        // AFTER the item channels + panel interactions: a furnace/crafting
        // table under the crosshair still opens its panel; a stateful block
        // (no panel) rotates. (Phase 92 used the block-ID ring — the 4
        // variants; Phase 94 replaced it with the per-instance facing;
        // ENT-2 generalizes the gate to hasState.)
        if (m_player->isCrouching()) {
            if (const std::optional<BlockHit> hit = targetBlock(); hit) {
                const Block& b = m_blocks.at(
                    m_world.getBlock(hit->block.x, hit->block.y,
                                     hit->block.z));
                if (b.hasState()) {
                    const int cur = m_world.getBlockState(hit->block.x,
                                                          hit->block.y,
                                                          hit->block.z);
                    // Unset (the sentinel, e.g. a hand-edited load) falls
                    // back to the block's state default — the same rule as
                    // the collision / mesh consumers.
                    const int s =
                        (cur != Chunk::kBlockStateUnset) ? cur
                                                         : b.stateDefault();
                    m_world.setBlockState(hit->block.x, hit->block.y,
                                          hit->block.z,
                                          static_cast<std::uint8_t>(
                                              (s + 1) % b.stateCount()));
                    // CRAFT-3: the rotated block's material timbre at the
                    // block center (no material = the generic "place").
                    m_sound.play(
                        blockSoundName(b, "place"),
                        Vec3{static_cast<float>(hit->block.x) + 0.5f,
                             static_cast<float>(hit->block.y) + 0.5f,
                             static_cast<float>(hit->block.z) + 0.5f});
                    return; // the rotation consumes the click (no placement)
                }
            }
        }
        placeBlock();
    }
    if (m_input.wasPressed(GameKey::Tab)) {
        cycleHotbar();
    }
    if (m_input.wasPressed(GameKey::Q)) {
        dropSelectedItem();
    }
    // Left button (Phases 11/20/21/24): a launcher item selected (the bow
    // — data 'shoot_speed' > 0) MELEE-attacks a mob under the crosshair
    // (one hit per kSwingInterval while held — the item's data
    // attack_damage, the bow's 1) but NEVER mines (Minecraft: the bow is
    // a weapon, not a tool; the shot is the right-button draw).
    // Otherwise: on a mob under the crosshair the swing attacks it
    // (damage + knockback, the block behind it is NOT mined); on a block
    // it mines (hold-to-break progress). Leaving either target resets
    // the other's state.
    if (m_input.isDown(MouseButton::Left)) {
        // A swing cancels an in-progress eat (Phase 28, Minecraft) — the
        // food is not consumed; the swing itself proceeds normally.
        if (m_eatChannel.active) {
            resetEatChannel();
        }
        if (selectedShootSpeed() > 0.0f) {
            resetBreaking();
            if (const std::optional<MobHit> mob = targetMob()) {
                updateAttacking(dt, *mob->mob);
            } else {
                m_swingCooldown = 0.0;
            }
        } else if (const std::optional<MobHit> mob = targetMob()) {
            resetBreaking();
            updateAttacking(dt, *mob->mob);
        } else {
            m_swingCooldown = 0.0;
            updateBreaking(dt);
        }
    } else {
        resetBreaking();
        m_swingCooldown = 0.0;
    }
    // Draw charge (Phase 24): with a launcher selected, the held right
    // button accumulates draw time (real dt, like the survival timers)
    // and the release fires one arrow with the charge's power.
    if (selectedShootSpeed() > 0.0f) {
        updateBowDraw(dt);
    }
    // Eat channel (Phase 28): the held right button fills the 1.6 s eat
    // (real dt, like the survival timers); completion consumes the food.
    // No-op while inactive (food and launcher are exclusive in the
    // selected slot, so the two channels never overlap).
    updateEatChannel(dt);
}

std::optional<BlockHit> Game::targetBlock() const {
    return raycastBlock(m_world, m_camera.position, m_camera.forward(),
                        kReach);
}

void Game::resetBreaking() {
    m_breakTarget.reset();
    m_breakProgress = 0.0;
    m_lastDigProgress = 0.0;
}

void Game::updateBreaking(double dt) {
    const std::optional<BlockHit> hit = targetBlock();
    if (!hit) {
        resetBreaking();
        return;
    }
    const Block& block =
        m_blocks.at(m_world.getBlock(hit->block.x, hit->block.y,
                                     hit->block.z));
    // Breakability is data-driven: water/bedrock set destroyable=false.
    if (!block.properties.destroyable) {
        resetBreaking();
        return;
    }
    if (!m_breakTarget.has_value() || *m_breakTarget != hit->block) {
        m_breakTarget = hit->block;
        m_breakProgress = 0.0;
    }
    const double duration = breakDurationSeconds(
        block.properties.hardness, selectedBreakSpeed(block));
    if (duration <= 0.0) {
        // Data-driven instant break (hardness 0 while destroyable).
        breakBlock(*hit);
        resetBreaking();
        return;
    }
    m_breakProgress += dt;
    // Mining tick (Phase 15): a dig sound every kDigSoundInterval of held
    // progress (the crack overlay renders the same progress, see render).
    // CRAFT-3: the broken block's material timbre at the block center
    // (no material = the generic "dig").
    const Vec3 digPos {static_cast<float>(hit->block.x) + 0.5f,
                       static_cast<float>(hit->block.y) + 0.5f,
                       static_cast<float>(hit->block.z) + 0.5f};
    while (m_breakProgress >= m_lastDigProgress + kDigSoundInterval) {
        m_sound.play(blockSoundName(block, "dig"), digPos);
        m_lastDigProgress += kDigSoundInterval;
    }
    if (m_breakProgress >= duration) {
        breakBlock(*hit);
        resetBreaking();
    }
}

std::optional<Game::MobHit> Game::targetMob() const {
    // The block occludes: the nearer of the block hit and the mob AABB
    // hits wins (a mob behind the aimed block is not hit; a mob in front
    // of the block is).
    float bestT = kReach;
    if (const std::optional<BlockHit> b = targetBlock()) {
        bestT = b->distance;
    }
    MobEntity* best = nullptr;
    const Vec3 o = m_camera.position;
    const Vec3 d = m_camera.forward();
    for (const auto& entity : m_entities) {
        const MobEntity* mob =
            dynamic_cast<const MobEntity*>(entity.get());
        if (mob == nullptr || mob->dead()) {
            continue;
        }
        const float t =
            raycastAabb(o, d, mob->aabbMin(), mob->aabbMax());
        if (t >= 0.0f && t < bestT) {
            bestT = t;
            best = const_cast<MobEntity*>(mob);
        }
    }
    if (best == nullptr) {
        return std::nullopt;
    }
    return MobHit {best, bestT};
}

void Game::updateAttacking(double dt, MobEntity& mob) {
    if (m_swingCooldown > 0.0) {
        m_swingCooldown -= dt;
        return;
    }
    // A swing landed: data damage (selected item) + knockback along the
    // aim (horizontal) with a lift. Removal + drops happen in the
    // updateEntities despawn pass (same frame).
    m_swingCooldown = kSwingInterval;
    mob.damage(selectedAttackDamage());
    // SURV-3 (durability): the held TOOL wears two points per melee hit
    // that LANDS (the event table: pickaxe / axe / shovel +2). A bow's
    // melee swing does not wear (it is a material, not a tool — its wear
    // is the shot, the fireBow site), and a bare hand has no item. The
    // hit completed (the damage + knockback landed) — a tool that breaks
    // on this swing is simply gone.
    {
        ItemStack& held = m_inventory.at(m_selectedSlot);
        if (!held.empty()) {
            const Item& heldItem = m_items.at(held.item);
            if (heldItem.type == ItemType::Tool &&
                heldItem.durability > 0.0f) {
                if (applyDurability(held, kToolMeleeWear, m_items)) {
                    m_sound.play("break"); // the tool broke on this hit
                }
            }
        }
    }
    const Vec3 f = m_camera.forward();
    Vec3 kb {f.x, 0.0f, f.z};
    const float len = std::sqrt(kb.x * kb.x + kb.z * kb.z);
    if (len > 1e-4f) {
        kb.x *= kKnockbackSpeed / len;
        kb.z *= kKnockbackSpeed / len;
    }
    kb.y = kKnockbackLift;
    mob.addKnockback(kb);
    m_sound.play("hurt");
}

float Game::selectedAttackDamage() const {
    const ItemStack& stack = m_inventory.at(m_selectedSlot);
    const float base =
        stack.empty() ? 1.0f  // bare hand
                      : m_items.at(stack.item).attackDamage;  // data (1.0)
    // Phase 46: the active weakness effects subtract their melee penalty;
    // Phase 84: the strength effects ADD their melee bonus (the pair — the
    // net is clamped at 0, so a fully weakened hand deals no damage even
    // with no strength).
    return std::max(0.0f, base -
                               m_playerEffects.totalMeleePenalty(m_effects) +
                               m_playerEffects.totalMeleeBonus(m_effects));
}

float Game::selectedShootSpeed() const {
    const ItemStack& stack = m_inventory.at(m_selectedSlot);
    if (stack.empty()) {
        return 0.0f;  // no launcher
    }
    return m_items.at(stack.item).shootSpeed;  // data (0 = not a launcher)
}

bool Game::ammoAvailable() const {
    const ItemStack& stack = m_inventory.at(m_selectedSlot);
    if (stack.empty()) {
        return false; // not a launcher (selectedShootSpeed guards callers)
    }
    const Item& item = m_items.at(stack.item);
    if (item.ammo.empty()) {
        return true; // no 'ammo' data = unlimited (pre-Phase-24 semantics)
    }
    const ItemID ammo = m_items.idForName(item.ammo); // init cross-checked
    for (std::size_t i = 0; i < m_inventory.size(); ++i) {
        if (m_inventory.at(i).item == ammo) {
            return true;
        }
    }
    return false;
}

void Game::updateBowDraw(double dt) {
    if (!m_bowDrawing) {
        return;
    }
    if (m_input.isDown(MouseButton::Right)) {
        // The charge fills in real dt, capped at the full-draw time
        // (core/bow.hpp). A press and a release in the same frame still
        // fire (at the minimum power), like a Minecraft quick click.
        m_bowDrawSeconds =
            std::min(m_bowDrawSeconds + dt, kBowFullDrawSeconds);
    } else if (m_input.wasReleased(MouseButton::Right)) {
        fireBow();
    } else {
        // The button is up without a release edge this frame (a stale
        // event, or the lock was lost): cancel the draw rather than hang
        // it.
        resetBowDraw();
    }
}

void Game::fireBow() {
    const Item& item = m_items.at(m_inventory.at(m_selectedSlot).item);
    // Ammo (Phase 24): one from the lowest-index slot that holds the
    // launcher's data 'ammo' item. The inventory can run out between the
    // draw start and the release — then the shot is cancelled silently.
    // Phase 77 (Infinity): an `infinite` launcher is still gated on
    // holding at least one (found below) but consumes NONE — the one
    // arrow is never spent (Minecraft's bow Infinity enchantment,
    // data-driven here).
    if (!item.ammo.empty()) {
        const ItemID ammo = m_items.idForName(item.ammo);
        int slot = -1;
        for (std::size_t i = 0; i < m_inventory.size(); ++i) {
            if (m_inventory.at(i).item == ammo) {
                slot = static_cast<int>(i);
                break;
            }
        }
        if (slot < 0) {
            resetBowDraw();
            return;
        }
        if (!item.infinite) {
            m_inventory.take(static_cast<std::size_t>(slot), 1);
        }
    }
    // The arrow spawns in front of the eye along the aim (the launch
    // origin never sits inside the near plane), at the item's data
    // speed/damage scaled by the charge's power (core/bow.hpp — a full
    // draw is the item's full data values). The updateEntities pass runs
    // its swept arrow-vs-mob hit test; the despawn pass removes it on
    // age / kill plane, and a block impact STICKS it (Phase 24). Arrows
    // are not saved (ephemeral, like mobs).
    const float power = bowPower(m_bowDrawSeconds);
    const Vec3 f = m_camera.forward();
    const Vec3 o = m_camera.position + f * kArrowSpawnOffset;
    // Phase 42: the arrow carries the shot's charge (power — scales the
    // mob-hit knockback) and the launcher's ammo item (a stuck arrow
    // picked up returns it to the inventory).
    // SURV-5: the launcher's arrow effect — the data name resolves to the
    // registry id here (init cross-checked the name AND its
    // affects_mobs flag, so idForName cannot fail); -1 on a plain bow =
    // the pre-SURV-5 projectile, bit-identical.
    int effectId = -1;
    double effectDuration = 0.0;
    int effectTier = 0;
    if (!item.arrowEffectName.empty()) {
        effectId = m_effects.idForName(item.arrowEffectName);
        effectDuration = item.arrowEffectDuration;
        effectTier = item.arrowEffectTier;
    }
    m_entities.push_back(std::make_unique<Projectile>(
        m_world, m_blocks, o, f, item.shootSpeed * power,
        item.shootDamage * power, /*hurtPlayer=*/false, power, item.ammo,
        effectId, effectDuration, effectTier));
    // SURV-3 (durability): the SHOT wears the launcher one point (any
    // charge; Infinity included — the arrow is not consumed, the bow
    // still is). The shot completed (the arrow is flying) — a bow that
    // breaks on this shot is simply gone.
    if (item.durability > 0.0f) {
        if (applyDurability(m_inventory.at(m_selectedSlot),
                            kLauncherShotWear, m_items)) {
            m_sound.play("break"); // the bow broke on this shot
        }
    }
    m_sound.play("shoot");
    resetBowDraw();
}

void Game::resetBowDraw() {
    m_bowDrawing = false;
    m_bowDrawSeconds = 0.0;
}

void Game::setBlock(int x, int y, int z, BlockID id) {
    // A first-touch column must be COMPLETE before the edit: the World's
    // setBlock creates only the target chunk, and the streaming loader
    // counts a column as loaded when ANY chunk exists — a partial column
    // would keep its other chunks void forever (a hole around the edit).
    // Completing it is the documented synchronous generation (the
    // placement path, ~1.6 ms for a fresh column; a no-op cost of 8 map
    // lookups when the column already exists).
    const ChunkPos cp = worldToChunk({x, y, z});
    if (!m_world.columnComplete(cp.x, cp.z)) {
        // First touch of a new column: complete it synchronously and
        // register the column for streaming (loaded), unload (tracked),
        // and the lighting job (new terrain).
        m_world.ensureColumn(cp.x, cp.z);
        m_loadedColumns.insert({cp.x, cp.z});
        m_lightPendingColumns.insert({cp.x, cp.z});
        m_unlitColumns.insert({cp.x, cp.z});
    }
    // The single choke point for every in-game block change (break, place,
    // dye recolor). Phase 42: in addition to the raw setBlock, any STUCK
    // arrow hung on the changed cell is released (Minecraft: arrows fall
    // out when their block is broken or replaced) — it rejoins flight
    // physics with zero velocity (gravity) and its cumulative age.
    // Phase 44: the per-block furnace state follows the cell — a furnace
    // block always owns a state (created on placement, empty), and a
    // cell that stops holding a furnace drops its state (the invariant
    // the format-7 save cross-checks against the block edits).
    m_world.setBlock(x, y, z, id);
    // Burn-pass gate (Phase 99.8): a flamable placement/rotation makes the
    // chunk "ever held a flamable" (over-approximation — removals do not
    // prune; see m_flammableChunks).
    if (m_blocks.at(id).properties.flamable) {
        m_flammableChunks.insert(cp);
    }
    // The column now DIFFERS from pristine generated terrain (break /
    // place / rotate / growth / burn / campfire — everything goes through
    // this choke point): the save records the diff, so the column's chunks
    // must survive the unload pass (player-edited columns are never
    // dropped — a reloaded save would otherwise lose the edits).
    m_editedColumns.insert({floorDiv(x, kChunkSize),
                            floorDiv(z, kChunkSize)});
    {
        const BlockPos fpos {x, y, z};
        const bool furnace =
            m_blocks.at(id).properties.interact == kInteractFurnace;
        if (furnace) {
            m_furnaces.emplace(fpos, FurnaceState {});
        } else {
            m_furnaces.erase(fpos);
        }
    }
    // Phase 48: the crop-growth tracking set follows the cell (like the
    // furnace states): a block with a non-empty growsTo is tracked, a cell
    // that stops holding one drops out — maturity included (the final
    // stage has no growsTo and never rolls again).
    {
        const BlockPos cpos {x, y, z};
        if (!m_blocks.at(id).properties.growsTo.empty()) {
            m_crops[cpos] = id;
        } else {
            m_crops.erase(cpos);
        }
    }
    // GEN-2: the sapling tracking set follows the cell (the m_crops twin,
    // the `tree` predicate): a sapling placement/break registers/drops the
    // cell; a GROWN tree's cells (trunk / leaves) never match, so the
    // completion drops the entry through this same path (the root cell
    // becoming the trunk). The stage is NOT touched here — the ENT-2
    // block below it seeds/clears the state byte independently.
    {
        const BlockPos spos {x, y, z};
        if (m_blocks.at(id).hasTree()) {
            m_saplings[spos] = id;
        } else {
            m_saplings.erase(spos);
        }
    }
    // GEN-9: the moisture tracking set follows the cell (the m_saplings
    // twin, the model.moisture_top predicate): a farmland placement/break
    // registers/drops the cell, seeded with its wetness NOW so the first
    // mesh (and the first refresh) see the correct state. The WETNESS is
    // derived at mesh time — this entry is only the change detector for
    // the fluid-tick re-mesh trigger (refreshMoistureStates).
    {
        const BlockPos mpos {x, y, z};
        if (!m_blocks.at(id).model.moistureTop.empty()) {
            m_moistureBlocks[mpos] = soilIsWet(m_world, mpos);
            // Seed-time re-mesh: the cell's chunk may hold a STALE mesh
            // (baked before this placement, or before the water neighbor
            // existed — the sweep's flip gate then never fires, because
            // the tracked state already equals the live one). Forcing one
            // re-mesh here makes the top face derive from the now-current
            // fluid field; the sweep covers the later chunk-load case.
            m_world.markChunkDirty(worldToChunk(mpos));
        } else {
            m_moistureBlocks.erase(mpos);
        }
    }
    // Phase 83 (a list in Phase 91): the beacon tracking set follows the
    // cell (like the crops): a block with a non-empty beaconEffects list is
    // a beacon (tracked; its power level is re-derived from the world each
    // frame, so no per-instance state is stored), a cell that stops holding
    // one drops out.
    {
        const BlockPos bpos {x, y, z};
        if (!m_blocks.at(id).properties.beaconEffects.empty()) {
            m_beacons[bpos] = id;
        } else {
            m_beacons.erase(bpos);
        }
    }
    // Phase 64: the fire remaining life follows the cell (like the
    // furnace states): a cell that holds a fire block always owns a life
    // (created with the FULL lifetime on placement/burn — emplace keeps
    // an existing countdown, so a same-block re-set never resets it), and
    // a cell that stops holding a fire drops its life (the invariant the
    // format-10 save cross-checks). m_fireId is invalid only in an asset
    // set without the fire block (then this is a no-op, like the other
    // registry-keyed bookkeeping).
    {
        const BlockPos ppos {x, y, z};
        if (m_fireId.valid() && id == m_fireId) {
            m_fires.emplace(ppos, m_fireLifetime);
        } else {
            m_fires.erase(ppos);
        }
    }
    // Phase 71: the campfire cooking state follows the cell (like the
    // furnace states): a campfire block always owns a state (created on
    // placement, empty), and a cell that stops holding a campfire drops
    // its state (the invariant the format-11 save cross-checks against
    // the block edits).
    {
        const BlockPos cpos {x, y, z};
        if (m_blocks.at(id).properties.interact == kInteractCampfire) {
            m_campfires.emplace(cpos, CampfireCookState {});
        } else {
            m_campfires.erase(cpos);
        }
    }
    // CRAFT-1: the table's grid follows the cell (like the furnace
    // states): a crafting_table block always owns a grid (created on
    // placement, empty), and a cell that stops holding one drops its grid
    // (the invariant the format-19 save cross-checks against the block
    // edits). BREAKING a table loses the items in it — the container
    // contents do not drop (the furnace-break semantics: only the block's
    // own `drops` spawn; the Minecraft rule).
    {
        const BlockPos tpos {x, y, z};
        if (m_blocks.at(id).properties.interact == kInteractCraftingTable) {
            m_tables.emplace(tpos, CraftingGrid {});
        } else {
            m_tables.erase(tpos);
        }
    }
    // ENT-2: the per-instance block state follows the cell (the project's
    // first per-block state — Phase 94's stair facing, generalized). A
    // stateful block (hasState(): the Stairs shape or an explicit
    // state_count) owns a state — set to the block's state DEFAULT on
    // placement (the placeBlock override can set a placement-specific value
    // afterward), and a cell that stops holding a stateful block keeps its
    // byte (the World API rejects the sentinel write — no change, no dirty
    // mark; the consumers are shape/state-gated, so the stale value is
    // inert). The collision (solidTopHeight) + the mesh (emitStairs) + the
    // save read it from the World.
    {
        const Block& b = m_blocks.at(id);
        if (b.hasState()) {
            m_world.setBlockState(x, y, z,
                                  static_cast<std::uint8_t>(b.stateDefault()));
        } else {
            // No state: the sentinel write is rejected (World validates
            // < 255) — zero change, zero dirty marks.
            m_world.setBlockState(x, y, z, Chunk::kBlockStateUnset);
        }
    }
    const float half = Projectile::kSize * 0.5f;
    for (auto& entity : m_entities) {
        auto* arrow = dynamic_cast<Projectile*>(entity.get());
        if (arrow == nullptr || !arrow->stuck()) {
            continue;
        }
        const Vec3 p = arrow->position();
        const float fx = static_cast<float>(x);
        const float fy = static_cast<float>(y);
        const float fz = static_cast<float>(z);
        // The collision resolver clamps a stuck arrow EXACTLY onto the
        // face it struck: its box touches the STUCK cell's boundary and
        // its center sits strictly outside that face. The stuck cell is
        // the changed cell iff, per axis, the box range meets the cell's
        // closed range (inAxis) and in at least one axis the box touches
        // a face from the OUTSIDE (touchAxis — the center strictly beyond
        // the face). A cell the box merely occupies from inside (no face
        // contact) does not qualify, so placing into the air cell around
        // a wall-stuck arrow does not release it.
        const auto inAxis = [half](float c, float lo, float hi) {
            return c - half <= hi && c + half >= lo;
        };
        const auto touchAxis = [half](float c, float lo, float hi) {
            return (c + half >= lo && c < lo) ||
                   (c - half <= hi && c > hi);
        };
        if (inAxis(p.x, fx, fx + 1.0f) && inAxis(p.y, fy, fy + 1.0f) &&
            inAxis(p.z, fz, fz + 1.0f) &&
            (touchAxis(p.x, fx, fx + 1.0f) ||
             touchAxis(p.y, fy, fy + 1.0f) ||
             touchAxis(p.z, fz, fz + 1.0f))) {
            arrow->release();
            MC_LOG_DEBUG("arrow released from changed block at (" +
                         std::to_string(p.x) + ", " + std::to_string(p.y) +
                         ", " + std::to_string(p.z) + ")");
        }
    }
}

void Game::breakBlock(const BlockHit& hit) {
    const BlockID id = m_world.getBlock(hit.block.x, hit.block.y, hit.block.z);
    // setBlock marks the chunk (and face-sharing neighbors) dirty; the mesh
    // rebuild happens on the next frame.
    setBlock(hit.block.x, hit.block.y, hit.block.z, BlockID::air());
    // Relight the region around the change (a broken occluder lets light
    // in; the broken cell stops blocking).
    Lighting::updateLocal(m_world, m_blocks, hit.block);

    // Drops are data-driven: `properties.drops` names an item (validated
    // against the ItemRegistry at init). The item spawns at the block
    // center with zero velocity, so its rest position is exact and headless
    // runs stay byte-reproducible (see ItemEntity).
    const Block& block = m_blocks.at(id);
    const Vec3 dropPos {static_cast<float>(hit.block.x) + 0.5f,
                        static_cast<float>(hit.block.y) + 0.5f,
                        static_cast<float>(hit.block.z) + 0.5f};
    if (!block.properties.drops.empty()) {
        const Item& dropItem = m_items.at(m_items.idForName(block.properties.drops));
        // dropsCount (validated at init to fit the item's max stack).
        m_entities.push_back(std::make_unique<ItemEntity>(
            m_world, m_blocks, dropItem.name, dropItem.maxStack,
            block.properties.dropsCount, dropPos));
    }
    // Chance secondary drop (Phase 49): an OPTIONAL extra item rolled per
    // break, independent of (and in addition to) the primary drop. The roll
    // is a pure function of (world seed, position, chance) — headless
    // byte-reproducible (no wall-clock, no per-instance RNG). Spawns at the
    // same block center (the deterministic rest position, like the primary).
    if (!block.properties.chanceDropItem.empty()) {
        if (rollChanceDrop(static_cast<std::uint64_t>(m_generator->seed()),
                           hit.block.x, hit.block.y, hit.block.z,
                           block.properties.chanceDropChance)) {
            const Item& cdItem =
                m_items.at(m_items.idForName(block.properties.chanceDropItem));
            m_entities.push_back(std::make_unique<ItemEntity>(
                m_world, m_blocks, cdItem.name, cdItem.maxStack,
                block.properties.chanceDropCount, dropPos));
        }
    }
    // Chance secondary drop LIST (Phase 52): each entry rolls independently
    // (the entry-indexed salt keeps them decorrelated — block/chance_drop)
    // and each passing entry spawns its own item at the block center.
    for (std::size_t i = 0; i < block.properties.chanceDrops.size(); ++i) {
        const ChanceDrop& cd = block.properties.chanceDrops[i];
        if (rollChanceDropEntry(static_cast<std::uint64_t>(m_generator->seed()),
                                hit.block.x, hit.block.y, hit.block.z,
                                static_cast<int>(i), cd.chance)) {
            const Item& cdItem = m_items.at(m_items.idForName(cd.item));
            m_entities.push_back(std::make_unique<ItemEntity>(
                m_world, m_blocks, cdItem.name, cdItem.maxStack,
                cd.count, dropPos));
        }
    }

    // Break particles (Phase 15): a deterministic burst (world seed mixed
    // with an increasing per-break counter: the same input sequence gives
    // the same burst). The light is baked here — updateLocal already ran
    // above — because the render pass must not read the light arrays
    // (worker lighting jobs may be running while it renders).
    const Chunk* chunk = m_world.findChunk(worldToChunk(hit.block));
    float sky = 1.0f;
    float blockLight = 0.0f;
    if (chunk != nullptr) {
        const BlockPos local =
            worldToChunkLocal(worldToChunk(hit.block), hit.block);
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
    m_particles.spawnBreak(hit.block.x, hit.block.y, hit.block.z, id, sky,
                           blockLight, rngSeed);
    // SURV-3 (durability): the held TOOL wears one point per NON-instant
    // block broken (hardness 0 = the "naturally breaks instantly"
    // approximation = 0 wear — the documented deviation; hand-mining and
    // non-tool items wear nothing — the event table's "tool +1" gate).
    // The break COMPLETES regardless (the block is already air +
    // dropped + sounded — the wear is the action's epilogue): a tool
    // that breaks on this block is simply gone from the slot.
    if (block.properties.hardness != 0.0f) {
        ItemStack& held = m_inventory.at(m_selectedSlot);
        if (!held.empty()) {
            const Item& heldItem = m_items.at(held.item);
            if (heldItem.type == ItemType::Tool &&
                heldItem.durability > 0.0f) {
                if (applyDurability(held, kToolBlockWear, m_items)) {
                    // The tool BROKE on this block: the item-break cue
                    // (the existing "break" wav — the sound table has no
                    // dedicated item-break asset, the design's reuse).
                    // CRAFT-3: the held item breaks at the PLAYER (gain 1,
                    // panned centered = the 2D no-origin call, the
                    // documented choice).
                    m_sound.play("break");
                }
            }
        }
    }
    // CRAFT-3: the broken BLOCK's material timbre at the block center
    // (dropPos — no material = the generic "break").
    m_sound.play(blockSoundName(block, "break"), dropPos);
}

void Game::placeBlock() {
    // The selected inventory slot must hold a placeable block item.
    const ItemStack& stack = m_inventory.at(m_selectedSlot);
    if (stack.empty()) {
        return;
    }
    const Item& item = m_items.at(stack.item);
    if (item.type != ItemType::Block) {
        return; // tools (and future non-block items) are not placeable
    }
    const std::optional<BlockHit> hit = targetBlock();
    if (!hit) {
        return;
    }
    // The target cell is the face neighbor of the hit block. The normal is a
    // unit axis vector, so the int cast is exact.
    const BlockPos target {
        hit->block.x + static_cast<int>(hit->normal.x),
        hit->block.y + static_cast<int>(hit->normal.y),
        hit->block.z + static_cast<int>(hit->normal.z)};
    if (!canPlaceAt(m_world, m_blocks, target, m_player->aabbMin(),
                    m_player->aabbMax())) {
        return;
    }
    // Planting placement restriction (Phase 53): if the placed block names a
    // plant_on soil list, the cell DIRECTLY BELOW the target must hold one of
    // the listed blocks (the soil). Absent = unrestricted (the generic
    // placeBlock rule). This is a placement-time gate, independent of the
    // growth-time grows_on gate (Phase 50). A wrong-soil placement is a
    // silent no-op (the block is not consumed, no sound — same as any other
    // rejected placement).
    if (!canPlantAt(m_world, m_blocks, target,
                    m_blocks.at(item.block).properties.plantOn)) {
        return;
    }
    // Game::setBlock (Phase 42): also releases any arrow stuck in the
    // newly occupied cell (placing into a cell an arrow hangs in).
    setBlock(target.x, target.y, target.z, item.block);
    // Phase 97: stair placement AUTO-ORIENTS (Minecraft): the per-instance
    // facing (Phase 94, the state in ENT-2) is set from the player's yaw —
    // the stair's low side faces where the player looks. setBlock above
    // seeded the state with the block's state default; this overrides it
    // for stairs. Any stair-shaped block (the base stone_stairs or a
    // Phase-89 variant) gets the same placement rule (the variant's baked
    // facing only matters outside the game, e.g. unit fixtures). This slot
    // is the future placement-state-rule extension point (ENT-2): other
    // stateful shapes keep the choke point's default.
    if (m_blocks.at(item.block).model.shape == BlockModel::Shape::Stairs) {
        m_world.setBlockState(target.x, target.y, target.z,
                              static_cast<std::uint8_t>(
                                  stairFacingFromYaw(m_player->yaw)));
    }
    // Relight the region around the change (a placed occluder blocks light;
    // a placed glowstone is a new light source).
    Lighting::updateLocal(m_world, m_blocks, target);
    // Placing consumes one from the selected stack (the inventory is the
    // source of blocks — no more infinite placement).
    m_inventory.take(m_selectedSlot, 1);
    // CRAFT-3: the PLACED block's material timbre at the new cell's
    // center (no material = the generic "place").
    m_sound.play(
        blockSoundName(m_blocks.at(item.block), "place"),
        Vec3{static_cast<float>(target.x) + 0.5f,
             static_cast<float>(target.y) + 0.5f,
             static_cast<float>(target.z) + 0.5f});
}

bool Game::tryEat() {
    // Only the selected slot can feed the player (Minecraft). The item is
    // food when its data marks restore_hunger (Item::restoreHunger > 0);
    // there is no hardcoded item list.
    if (!m_player || m_player->isDead()) {
        return false;
    }
    const ItemStack& stack = m_inventory.at(m_selectedSlot);
    if (stack.empty()) {
        return false;
    }
    const Item& item = m_items.at(stack.item);
    if (item.restoreHunger <= 0) {
        return false; // not food: fall through to the normal right-click
    }
    // Food is selected: the click is consumed either way.
    if (m_player->hunger() >= m_player->maxHunger()) {
        return true; // full: the eat is a no-op (nothing placed, no sound)
    }
    // Eat channel (Phase 28): the eat is no longer instant — holding the
    // right button for 1.6 s eats (updateEatChannel owns the completion:
    // consume + restore + sound). An extra press while channeling is
    // ignored (Minecraft: the eat continues).
    if (m_eatChannel.active) {
        return true;
    }
    m_eatChannel.start();
    m_eatSlot = m_selectedSlot;
    m_eatItem = stack.item;
    m_eatStartHealth = m_player->health();
    return true;
}

bool Game::tryDrink() {
    // Drinkable container (Phase 43): only the selected slot can feed
    // the player (Minecraft). The item is drinkable when its data marks
    // drink_time (Item::drinkTime > 0); there is no hardcoded item list.
    if (!m_player || m_player->isDead()) {
        return false;
    }
    const ItemStack& stack = m_inventory.at(m_selectedSlot);
    if (stack.empty()) {
        return false;
    }
    const Item& item = m_items.at(stack.item);
    if (item.drinkTime <= 0.0f) {
        return false; // not drinkable: fall through to the normal right-click
    }
    // A drinkable is selected: the click is consumed either way. No room
    // for the returned container (Minecraft: you cannot drink when the
    // empty bucket would have nowhere to go): a no-op, click consumed.
    // isFull(item) = "not even one more unit fits" (a partial stack of
    // the item or an empty slot exists = false), the exact probe add(1)
    // needs.
    if (!item.returnsItem.empty()) {
        const ItemID ret = m_items.idForName(item.returnsItem);
        if (ret.valid() && m_inventory.isFull(ret, m_items)) {
            return true; // no room for the container: no-op (no sound)
        }
    }
    // Drink channel (Phase 43): the shared eat channel armed with the
    // item's DATA-DRIVEN duration (updateEatChannel owns the completion:
    // consume + return container + clear status effects). An extra press
    // while channeling is ignored (the drink continues). The "milk"
    // sound plays on START (Minecraft: the glug is up front, even when
    // the drink is cancelled — same as the bow draw sound).
    if (m_eatChannel.active) {
        return true;
    }
    m_eatChannel.start(static_cast<double>(item.drinkTime));
    m_eatSlot = m_selectedSlot;
    m_eatItem = stack.item;
    m_eatStartHealth = m_player->health();
    m_sound.play("milk");
    return true;
}

bool Game::tryMilk() {
    // Milking (Phase 43, cow): the crosshair must be ON a milkable mob
    // (mob data 'milk_with' == the selected item's name; melee targeting
    // — kReach with block occlusion, see targetMob) and the player must
    // hold the mob's 'milk_with' item. One held item is consumed and one
    // 'milk_gives' item is added; the mob is NOT damaged, killed or made
    // to drop. SURV-10: a milked mob is on the 5-minute Minecraft milking
    // cooldown — re-milking it early does nothing (the click is still
    // consumed, the failed-milk rule below). The click is consumed
    // whenever the crosshair is on the mob (Minecraft: a failed milk —
    // e.g. a full inventory — still eats the click).
    if (!m_player || m_player->isDead()) {
        return false;
    }
    const ItemStack& stack = m_inventory.at(m_selectedSlot);
    if (stack.empty()) {
        return false;
    }
    const Item& item = m_items.at(stack.item);
    const std::optional<MobHit> mob = targetMob();
    if (!mob) {
        return false;
    }
    const MobProperties& props = mob->mob->def().properties;
    if (props.milkWith.empty() || props.milkWith != item.name) {
        return false; // not a milkable mob, or the wrong item in hand
    }
    // SURV-10: the milking cooldown (Minecraft's 5-minute rule) — the
    // same mob cannot be milked again until it expires. No milk, no
    // sound, the click is consumed (the failed-milk rule).
    if (!mob->mob->milkReady()) {
        return true;
    }
    // Init cross-checks guarantee the exists; the guard is a defensive
    // no-op.
    const ItemID gives = m_items.idForName(props.milkGives);
    if (!gives.valid()) {
        return true;
    }
    if (m_inventory.isFull(gives, m_items)) {
        return true; // full: no milk, click consumed (Minecraft)
    }
    mob->mob->startMilkCooldown();
    m_inventory.take(m_selectedSlot, 1);
    m_inventory.add(gives, 1, m_items);
    m_sound.play("milk");
    MC_LOG_DEBUG("milked mob '" + mob->mob->def().name + "' (" +
                 item.name + " -> " + props.milkGives + ")");
    return true;
}

bool Game::tryFluidBucket() {
    // Fluid bucket (Phase 47; kinds in Phase 51 — water and lava). The
    // selected item must be a bucket side (item data 'collects_fluid' or
    // 'pours_fluid'; the pair symmetry is cross-checked at init). The
    // COLLECT side: the bucket-target ray (kReach; the first cell that is
    // a non-air block or carries fluid — a wall occludes, Minecraft) must
    // land on a level-8 (source) cell of a fluid the bucket names: the
    // source is removed and one held item is swapped for the full bucket
    // of THAT fluid (the unique item that pours it back — derived, the
    // init cross-check). The POUR side (bucketPourTarget, FLUID-4): the
    // face neighbor of the first non-air block — or the fluid ray's own
    // hit cell when no block is in reach — must be air without fluid
    // (the legacy placement) or a FLOWING cell of the bucket's own kind
    // (pour onto a water surface: it becomes the source), in the Y range
    // and not overlapping the player: a level-8 source of the item's
    // fluid is placed and the item swaps to the empty bucket. Both play
    // "splash" and consume the click (a bucket never places a block); a
    // no-action case falls through to the normal right-click (placeBlock
    // is a no-op for a material — nothing happens, e.g. on an
    // already-source cell of the bucket's own kind, a water-on-lava
    // reaction pour with no dry-air placement cell (FLUID-5), or a
    // solid target).
    if (!m_player || m_player->isDead()) {
        return false;
    }
    const ItemStack& stack = m_inventory.at(m_selectedSlot);
    if (stack.empty()) {
        return false;
    }
    const Item& item = m_items.at(stack.item);
    if (item.collectsFluids.empty() && item.poursFluid.empty()) {
        return false; // not a bucket side: fall through
    }
    const bool collect = !item.collectsFluids.empty();
    if (collect) {
        const std::optional<BlockPos> cell =
            raycastFluid(m_world, m_camera.position, m_camera.forward(),
                         kReach);
        // A source cell is level kMaxFluidLevel (flowing cells are lower
        // and cannot be collected, Minecraft). A block cell reads dry.
        const std::optional<FluidKind> kind =
            cell ? m_world.getFluidKind(cell->x, cell->y, cell->z)
                 : std::nullopt;
        const bool isSource =
            kind &&
            m_world.getFluidOfKind(cell->x, cell->y, cell->z, *kind) ==
                kMaxFluidLevel;
        // The source's fluid must be one the bucket names (Minecraft: one
        // empty bucket scoops every fluid it names).
        const std::string kindName =
            kind ? fluidKindName(*kind) : std::string();
        const bool named =
            kind && std::find(item.collectsFluids.begin(),
                              item.collectsFluids.end(),
                              kindName) != item.collectsFluids.end();
        if (!cell || !isSource || !named) {
            return false; // no named source in reach: fall through
        }
        // The full bucket of that fluid (init guarantees exactly one).
        const Item* full = m_items.fullBucketFor(kindName, item.name);
        if (full == nullptr) {
            return true; // defensive (init cross-check); consume the click
        }
        const ItemID becomes = full->id;
        if (m_inventory.isFull(becomes, m_items)) {
            return true; // no room: nothing happens (Minecraft)
        }
        auto finish = [&]() {
            m_inventory.take(m_selectedSlot, 1);
            m_inventory.add(becomes, 1, m_items);
            m_sound.play("splash");
        };
        m_world.setFluid(cell->x, cell->y, cell->z, *kind, 0);
        // The fluid diff is save-recorded (no setBlock choke point here):
        // keep the column (never unloaded).
        m_editedColumns.insert({floorDiv(cell->x, kChunkSize),
                                floorDiv(cell->z, kChunkSize)});
        Lighting::updateLocal(m_world, m_blocks, *cell);
        finish();
        MC_LOG_DEBUG("scooped " + kindName + " at (" +
                     std::to_string(cell->x) + ", " +
                     std::to_string(cell->y) + ", " +
                     std::to_string(cell->z) + ") (" + item.name + " -> " +
                     full->name + ")");
        return true;
    }
    // The pour side: the item's fluid kind (init guarantees the name is
    // in the vocabulary).
    const FluidKind kind = *fluidKindForName(item.poursFluid);
    const ItemID becomes = m_items.idForName(item.poursBecomes);
    if (!becomes.valid()) {
        return true; // defensive (init cross-check); consume the click
    }
    if (m_inventory.isFull(becomes, m_items)) {
        return true; // no room for the companion: nothing happens (Minecraft)
    }
    auto finish = [&]() {
        m_inventory.take(m_selectedSlot, 1);
        m_inventory.add(becomes, 1, m_items);
        m_sound.play("splash");
    };
    const std::optional<BlockHit> hit = targetBlock();
    std::optional<BlockPos> fluidHit;
    if (!hit) {
        // FLUID-4: no block face in reach — open water beyond block
        // reach is the fluid ray's own hit cell (a wall in reach would
        // have produced the block hit, so the fluid hit here is
        // unoccluded by definition).
        fluidHit = raycastFluid(m_world, m_camera.position,
                                m_camera.forward(), kReach);
    }
    // The pour cell (bucketPourTarget): the face neighbor of the hit
    // block (the placement-target convention — the block ray passes
    // through fluids, so a floor under a 1-deep pool makes the pool's
    // top water cell the candidate), or the fluid hit when no block is
    // in reach. The candidate is either AIR without fluid (the legacy
    // placement, bit-identical) or a FLOWING cell of the bucket's own
    // kind (FLUID-4 pour onto a water surface — it becomes the source);
    // canPlaceAt keeps the Y range / solid / player-overlap tests.
    const std::optional<BlockPos> target = bucketPourTarget(
        m_world, m_blocks, hit, fluidHit, kind, m_player->aabbMin(),
        m_player->aabbMax());

    if (!target) {
        return false; // no acceptable pour cell: fall through
    }
    const BlockPos& cell = *target;
    m_world.setFluid(cell.x, cell.y, cell.z, kind, kMaxFluidLevel);
    // The fluid diff is save-recorded (no setBlock choke point here):
    // keep the column (never unloaded).
    m_editedColumns.insert({floorDiv(cell.x, kChunkSize),
                            floorDiv(cell.z, kChunkSize)});
    Lighting::updateLocal(m_world, m_blocks, cell);
    finish();
    MC_LOG_DEBUG("poured " + item.poursFluid + " at (" +
                 std::to_string(cell.x) + ", " +
                 std::to_string(cell.y) + ", " +
                 std::to_string(cell.z) + ") (" + item.name + " -> " +
                 item.poursBecomes + ")");
    return true;
}

bool Game::tryArmor() {
    // Armor (SURV-1): the right-click channel of the armor pieces. The
    // decision + every state write is the pure armorRightClickUse (item/
    // armor.hpp — the campfire pattern): a HELD piece equips onto its
    // data slot (an occupied slot SWAPS the piece with the held one), an
    // EMPTY hand unequips the first occupied slot in the fixed order
    // head -> chest -> legs -> feet. The click is consumed whenever a
    // piece is equipped, swapped or unequipped; otherwise it falls
    // through (the sneak-rotate / place no-op).
    if (!m_player || m_player->isDead()) {
        return false;
    }
    const ArmorUseAction action =
        armorRightClickUse(*m_player, m_inventory, m_items, m_selectedSlot);
    if (action == ArmorUseAction::None) {
        return false;
    }
    m_sound.play("cloth"); // the Minecraft armor sound (no new wav)
    return true;
}

void Game::clearStatusEffects() {
    // Phase 43 (completed by Phase 46): a completed drinkable container
    // resets the red damage veil (m_hurtFlash) and removes every NEGATIVE
    // active status effect — Minecraft's milk semantics (the positive
    // effects, like regeneration, survive the drink). The fall-damage
    // tracking (Player::fallStartY) is deliberately NOT touched —
    // Minecraft's fall damage is not a status effect, so drinking
    // mid-air must not reduce the fall damage owed on landing.
    m_hurtFlash = 0.0;
    m_playerEffects.clearNegative(m_effects);
    syncAbsorption(); // Phase 72: the pool drops with the cleared effects
}

void Game::updateFires(double dt) {
    if (m_fires.empty()) {
        return;
    }
    // std::map iteration is position-sorted: the put-out order is
    // deterministic. The countdown is a REAL-dt timer (hard rule 8, the
    // survival-timer class — like the player's fire effect): a headless
    // run at a fixed frame rate is byte-reproducible, and the burn-out
    // moment is wall-clock-stable.
    std::vector<BlockPos> putOut;
    for (auto& [pos, remaining] : m_fires) {
        remaining -= dt;
        if (remaining <= 0.0) {
            putOut.push_back(pos);
            continue;
        }
        // Water extinguish (Phase 64): a fire with a WATER fluid (any
        // level) in one of its 6 neighbors is put out instantly — water
        // beats fire (the Phase 57 water-beats-lava rule). Lava does not
        // extinguish: the fire beside the pool is the lava's product and
        // keeps burning (Minecraft — fire and lava coexist).
        bool wet = false;
        for (const BlockPos& d : kFireNeighbors) {
            if (m_world.getFluidOfKind(pos.x + d.x, pos.y + d.y,
                                       pos.z + d.z, FluidKind::Water) > 0) {
                wet = true;
                break;
            }
        }
        if (wet) {
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
        m_particles.spawnBreak(pos.x, pos.y, pos.z, m_fireId, sky, blockLight,
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
    if (m_fireParticles && m_frameCount % kFireParticleEveryNFrames == 0) {
        for (const auto& [pos, remaining] : m_fires) {
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
            m_particles.spawnFire(pos.x, pos.y, pos.z, m_fireId, sky,
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
    if (!m_furnaceFlames || m_furnaces.empty() || !m_fireId.valid() ||
        (m_frameCount + 1) % kFurnaceParticleEveryNFrames != 0) {
        return;
    }
    for (const auto& [pos, st] : m_furnaces) {
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
        m_particles.spawnFire(pos.x, pos.y + 1, pos.z, m_fireId, sky,
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
    if (!m_mobFlames || !m_fireId.valid() ||
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
        m_particles.spawnFire(center.x, center.y, center.z, m_fireId, sky,
                              1.0f, rngSeed);
    }
}

void Game::updateFireSpread() {
    // Phase 66 (Fire v2): the spread roll runs every
    // fire.spread_interval_frames frame (the fixed frame step, hard rule
    // 8) — no fires, no fire block (a stripped asset set), or a chance of
    // 0 all fast-exit inside fireSpreadIgnitions.
    if (m_fires.empty() || !m_fireId.valid() ||
        m_frameCount % m_fireSpreadInterval != 0) {
        return;
    }
    // Position-ordered fire list (the m_fires map keys, std::map order).
    std::vector<BlockPos> firePositions;
    firePositions.reserve(m_fires.size());
    for (const auto& [pos, remaining] : m_fires) {
        (void)remaining;
        firePositions.push_back(pos);
    }
    const std::vector<BlockPos> ignitions = fireSpreadIgnitions(
        m_world, m_blocks, firePositions, m_generator->seed(),
        static_cast<std::uint64_t>(m_frameCount), m_fireSpreadChance);
    // Apply (AFTER the roll — setBlock erases nothing here, but it
    // EMPLACES the new fires' full lives through the choke point; never
    // feed a map into a pure function while mutating it). No sound: a
    // spreading fire line would otherwise spam the one-shot "ignite"
    // (documented Fire v2 scope).
    for (const BlockPos& pos : ignitions) {
        setBlock(pos.x, pos.y, pos.z, m_fireId);
        Lighting::updateLocal(m_world, m_blocks, pos);
    }
}

void Game::updateBeacons() {
    // Phase 83: re-derive the power level of every tracked beacon and
    // refresh its NAMED effect on the player while the player is in range.
    // The level is a pure function of the world (world/beacon) — no stored
    // state. A no-op (fast-exit) while the world holds no beacon OR the
    // player is dead (structural immunity: a fresh no-input world has no
    // beacon, so m_beacons is empty and this never runs its body).
    m_beaconActive.clear();
    if (m_beacons.empty() || m_player->isDead()) {
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
    for (const auto& [pos, bid] : m_beacons) {
        const int level = beaconLevel(m_world, m_blocks, pos.x, pos.y, pos.z);
        if (level < 1) {
            continue;
        }
        // An active beacon feeds the beam renderer (drawn in the blending
        // pass; the beam is gated on the `beacon.beam` config there).
        m_beaconActive.push_back(BeaconBeam {pos.x, pos.y, pos.z, level});
        // In range? Chebyshev distance from the player's feet to the beacon
        // cell's centre (the voxel-cube "within N blocks" rule).
        const float dx = feet.x - (pos.x + 0.5f);
        const float dy = feet.y - (pos.y + 0.5f);
        const float dz = feet.z - (pos.z + 0.5f);
        const float dist = std::max(std::fabs(dx),
                                    std::max(std::fabs(dy), std::fabs(dz)));
        if (dist > static_cast<float>(m_beaconRange)) {
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
    const int rawSky = m_world.getSkyLight(cx, cy, cz);
    const AmbienceContext ctx =
        ambienceContext(rawSky, m_dayCycle.dayFactor());
    // The roll: the pure function of (world seed, frame, context) — same
    // inputs, same answer across runs (the event sequence is
    // deterministic; the log line below is the E2E evidence).
    if (!ambienceRoll(static_cast<std::uint64_t>(m_generator->seed()),
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

void Game::updateEffects(double dt) {
    // Phase 84: push the effect-derived move-speed + damage-reduction onto
    // the player every frame (the player's movement + damage read them; the
    // identity 1.0 / 0.0 when no such effect is active). Runs even for a
    // dead player so the values reset cleanly on respawn.
    m_player->setSpeedMultiplier(
        m_playerEffects.totalSpeedMultiplier(m_effects));
    m_player->setDamageReduction(
        m_playerEffects.totalDamageReduction(m_effects));
    if (m_player->isDead()) {
        return;
    }
    // Phase 72: the absorption pool is synced only when the effects'
    // total ACTUALLY changes this frame (an expiry, a water extinguish).
    // A per-frame SET would fight the damage drain: the pool must persist
    // between sync points (the Phase 56 bonus sync was idempotent, the
    // pool is not — damage lowers it below the total).
    const float absorptionBefore = m_playerEffects.totalAbsorption(m_effects);
    // Phase 57: the fire system (the data-flagged effect, resolved at
    // init). Runs EVEN with no active effects — the ignition is a source:
    // while the player is in the lava OR overlapping a fire block
    // (Phase 74: the fire block ignites the player the way the lava
    // does — the P70 direct contact damage is gone; both sources share
    // this one gate) the fire is (re-)applied EVERY frame (the duration
    // resets, the tick phase is kept — standing in the fire source burns
    // continuously, Minecraft); after leaving, the effect counts down
    // fireDurationSeconds and burns until it expires. Water
    // EXTINGUISHES instantly (and beats both: in water, no fire).
    if (m_fireEffectId != -1) {
        if (m_player->inLava() || m_player->isInFire()) {
            m_playerEffects.apply(
                m_fireEffectId,
                m_player->params().fireDurationSeconds);
        }
        if (m_player->inWater()) {
            m_playerEffects.clearExtinguishable(m_effects);
        }
    }
    if (m_playerEffects.count() == 0) {
        return;
    }
    // The ticks are REAL dt (the survival-timer class, like hunger and
    // the eat channel). The HP changes apply against the player HERE
    // (not in StatusEffects): the damage tick respects the effect's
    // min_health floor (poison keeps the player at >= 1 HP and never
    // kills — a plain damage effect with floor 0 may), the heal tick
    // clamps at max health. A poison tick that drops health shows the
    // red hurt veil like any other damage (updateSurvival polls the
    // health each frame; the v1 has no separate poison tint).
    m_playerEffects.update(
        dt, m_effects, [this](const Effect& def) {
            if (def.tick == Effect::Tick::Damage) {
                if (m_player->health() - def.tickAmount >= def.minHealth) {
                    m_player->damage(def.tickAmount);
                }
            } else if (def.tick == Effect::Tick::Heal) {
                m_player->heal(def.tickAmount);
            }
        });
    // Phase 72: an expired (or extinguished) absorption layer drops the
    // pool to the remaining total (the health is untouched — the pool is
    // not HP). Unchanged totals leave the pool alone (the drain since
    // the last sync point persists).
    if (m_playerEffects.totalAbsorption(m_effects) != absorptionBefore) {
        syncAbsorption();
    }
}

void Game::syncAbsorption() {
    m_player->setAbsorption(m_playerEffects.totalAbsorption(m_effects));
}

void Game::applyPlayerEffect(const std::string& name, double duration,
                             int tier) {
    // SURV-4 (potions): the single choke point for applying a NAMED effect
    // to the player. Every grant source (a food's grants_effect(s), a
    // drinkable's, a mob's on_hit_effect) is init-cross-checked to name an
    // EXISTING effect, so idForName cannot fail here (the dead check in the
    // call sites guards both). The apply uses the RESET semantics (an
    // already-active instance is re-timed to `duration`, the tick phase
    // kept — drinking the same potion twice resets, it does not stack).
    // SURV-6: the grant's `tier` rides along (a re-apply at a DIFFERENT
    // tier replaces the instance's tier — no amplifier stacking; tier 0
    // is the pre-SURV-6 value, bit-identical through this choke point).
    // Then settle any INSTANT action the effect data carries: a positive
    // instant_hp heals (clamped at the base max — the healing potion), a
    // negative one deals damage with the armor EXEMPTED (the harming
    // potion — Minecraft's magic/potion damage, the SURV-2 drowning
    // bypassArmor twin; the resistance effect still applies, as in the
    // damage pipeline). Effects without an instant action settle nothing
    // (the pre-SURV-4 apply sites are bit-identical through the helper).
    // The caller owns the surrounding syncAbsorption() (the pool is a pure
    // function of the effects at every sync point).
    const int id = m_effects.idForName(name);
    if (id == -1) {
        return; // init guarantees existence; this is a defensive no-op.
    }
    m_playerEffects.apply(id, duration, tier);
    const Effect& def = m_effects.at(id);
    if (def.instantHp > 0.0f) {
        m_player->heal(def.instantHp);
    } else if (def.instantHp < 0.0f) {
        m_player->damage(-def.instantHp, true);
    }
}

void Game::updateEatChannel(double dt) {
    if (!m_eatChannel.active) {
        return;
    }
    // Interrupts (Phase 28, Minecraft): damage, death, pointer unlock, a
    // panel, or the food leaving its slot (selection change, Q-drop, or
    // the stack emptied some other way). Any of them cancels the channel
    // — the food is NOT consumed.
    const ItemStack& stack = m_inventory.at(m_selectedSlot);
    if (m_player->isDead() || !m_window.relativeMouseMode() ||
        panelOpen() || m_player->health() < m_eatStartHealth ||
        m_selectedSlot != m_eatSlot || stack.item != m_eatItem) {
        resetEatChannel();
        return;
    }
    const EatEvent ev =
        m_eatChannel.update(dt, m_input.isDown(MouseButton::Right));
    if (ev == EatEvent::Completed) {
        // The channel finished: consume one from the captured slot (its
        // validity was just re-checked). Food restores hunger plus the
        // hidden saturation buffer (core/food.hpp) and plays the eat
        // sound; a drinkable container (Phase 43) returns its empty
        // container to the inventory (start checked the capacity — the
        // add cannot fail) and clears the player's status effects (the
        // v1 set: the hurt flash; see clearStatusEffects).
        const Item& item = m_items.at(m_eatItem);
        m_inventory.take(m_eatSlot, 1);
        if (item.restoreHunger > 0) {
            m_player->restoreHunger(static_cast<float>(item.restoreHunger));
            // SURV-9: the per-food satiety (the 'saturation' field, the
            // Minecraft values; absent = the legacy 2x rule).
            m_player->restoreSaturation(foodSatiety(item));
            // Food that grants a status effect on a full eat (Phase 49,
            // golden apple; the list form Phase 56): applied through the
            // single choke point (applyPlayerEffect — the name is
            // cross-checked at init, the list entries IN ORDER, the instant
            // settlement for the SURV-4 effects). Bit-identical to the old
            // direct apply for the pre-SURV-4 food (no instant effects).
            if (!item.grantsEffect.empty()) {
                applyPlayerEffect(item.grantsEffect,
                                  item.grantsEffectDuration,
                                  item.grantsEffectTier);
            }
            for (const Item::GrantEffect& g : item.grantsEffects) {
                applyPlayerEffect(g.name, g.duration, g.tier);
            }
            syncAbsorption(); // Phase 72: the pool takes the new total
            m_sound.play("eat");
        } else {
            if (!item.returnsItem.empty()) {
                m_inventory.add(m_items.idForName(item.returnsItem), 1,
                                m_items);
            }
            // SURV-4 (potions): a drinkable that GRANTS effects applies them
            // on a completed drink — the milk's clear is now the NO-GRANT
            // default, not the universal behavior. The grant entries apply
            // IN ORDER (the data's order), each through the single choke
            // point (applyPlayerEffect — the RESET semantics + the instant
            // settlement). A drinkable WITHOUT grants (milk / water) keeps
            // the original clear-negative behavior, bit-identical.
            if (!item.grantsEffect.empty() || !item.grantsEffects.empty()) {
                if (!item.grantsEffect.empty()) {
                    applyPlayerEffect(item.grantsEffect,
                                      item.grantsEffectDuration,
                                      item.grantsEffectTier);
                }
                for (const Item::GrantEffect& g : item.grantsEffects) {
                    applyPlayerEffect(g.name, g.duration, g.tier);
                }
                syncAbsorption(); // Phase 72: the pool takes the new total
            } else {
                clearStatusEffects(); // milk / water: the original clear
            }
            MC_LOG_DEBUG("drank '" + item.name + "'" +
                         (item.returnsItem.empty()
                              ? std::string()
                              : " (returned '" + item.returnsItem + "')"));
        }
        resetEatChannel();
    } else if (ev == EatEvent::Cancelled) {
        // Early release: nothing is consumed, no sound.
        resetEatChannel();
    }
}

void Game::resetEatChannel() {
    m_eatChannel.reset();
    m_eatItem = ItemID::invalid();
    m_eatStartHealth = -1.0f;
}

void Game::cycleHotbar() {
    m_selectedSlot =
        (m_selectedSlot + 1) % static_cast<std::size_t>(Inventory::kHotbarSize);
    // The selected item (and its break speed) changed: restart any break
    // in progress (Minecraft behavior), cancel an in-progress bow draw
    // (the launcher changed; Phase 24) and an in-progress eat (the food
    // left its slot; Phase 28).
    resetBreaking();
    resetBowDraw();
    resetEatChannel();
}

void Game::dropSelectedItem() {
    const ItemStack& stack = m_inventory.at(m_selectedSlot);
    if (stack.empty()) {
        return;
    }
    const Item& item = m_items.at(stack.item);
    if (m_inventory.take(m_selectedSlot, 1) == 0) {
        return;
    }
    // Spawn at the player's feet (bottom-center) with zero velocity — the
    // same determinism convention as block drops. The item's pickup delay
    // (see ItemEntity::kPickupDelaySeconds) keeps it from being
    // re-picked up on the next frame. SURV-3: the dropped unit carries
    // the stack's accumulated damage (captured BEFORE the take above —
    // an emptied stack would lose it; the take keeps the remainder's).
    const float droppedDamage = stack.damage;
    const Vec3 pos = m_player->position();
    m_entities.push_back(std::make_unique<ItemEntity>(m_world, m_blocks,
                                                      item.name, item.maxStack,
                                                      /*count=*/1, pos,
                                                      droppedDamage));
    // CRAFT-3: the timbre is the generic "pickup" (items have no
    // sound_material — YAGNI, documented), only the spatialization is
    // added (at the drop position, the player's feet).
    m_sound.play("pickup", pos);
}

float Game::selectedBreakSpeed(const Block& target) const {
    const ItemStack& stack = m_inventory.at(m_selectedSlot);
    if (stack.empty()) {
        return 1.0f; // bare hands
    }
    // Phase 67: the fast_against list (axes vs wood) is part of the
    // item's data — effectiveBreakSpeed applies it against `target`.
    return effectiveBreakSpeed(m_items.at(stack.item), target);
}

// ---------------------------------------------------------------------------
// Entities (Phase 10)
// ---------------------------------------------------------------------------

void Game::updateEntities(double dt) {
    // Mob spawning (Phase 20): a deterministic frame cadence from the data
    // config (first_spawn_frame keeps the 60-frame no-input baseline
    // mob-free). The scan is a pure function of world + player, so the
    // same runs spawn the same mobs.
    // The cadence is the data base scaled by the difficulty (Phase 25:
    // check_every_frames / hostileSpawnRateScale — 30 -> 60/30/20 for
    // easy/normal/hard). Normal = the base itself (baseline unchanged).
    if (m_mobsEnabled &&
        m_frameCount >= m_spawnConfig.firstSpawnFrame &&
        m_frameCount %
                checkEveryFramesFor(m_spawnConfig.checkEveryFrames,
                                    m_difficulty) ==
            0) {
        attemptMobSpawn();
    }
    // The mobs need the player's state for their AI (the player updates
    // before the entities in the frame; a dead player is not chased).
    // Mobs step on the fixed kMobStepPerFrame (see its comment); the other
    // entities keep the real dt (they rest — a dropped item's position is
    // an exact boundary regardless of the dt history).
    const Vec3 playerPos = m_player->position();
    const bool playerAlive = !m_player->isDead();
    // Arrow pre-step positions (Phase 21): captured before the update so
    // the swept arrow-vs-mob hit test below can run over each arrow's
    // per-step displacement.
    std::vector<std::pair<Projectile*, Vec3>> arrowSteps;
    for (auto& entity : m_entities) {
        if (auto* arrow = dynamic_cast<Projectile*>(entity.get())) {
            arrowSteps.push_back({arrow, arrow->position()});
        }
    }
    // Phase 73: INDEX loop (not range-for): the skeleton's shoot
    // callback (invoked inside mob->update) pushes a new arrow into
    // m_entities — a vector reallocation would DANGLE the range-for's
    // reference (latent P27 UB, exposed the first time a skeleton
    // finally got an unobstructed line of sight). The index re-fetches
    // m_entities[i] from the current storage every iteration, so the
    // deferred arrow is reached and takes its spawn-frame step — the
    // original intent. (No pass ERASES from m_entities mid-loop, so the
    // index is stable enough; removals run in the post-loop despawn
    // pass.)
    for (std::size_t i = 0; i < m_entities.size(); ++i) {
        auto& entity = m_entities[i];
        if (auto* mob = dynamic_cast<MobEntity*>(entity.get())) {
            mob->setPlayer(playerPos, playerAlive);
            mob->update(kMobStepPerFrame);
            // SURV-5: the mob's active status effects tick on the SAME
            // fixed mob step (frame-locked, hard rule 8) — the damage
            // floors use the effect's min_health_mob (poison: 0, the mob
            // can die; a killing tick flows through the existing
            // despawn/drop path below). count == 0 fast-exits (every
            // effect-free mob's frame is bit-identical to before).
            mob->updateStatusEffects(kMobStepPerFrame, m_effects);
        } else if (dynamic_cast<Projectile*>(entity.get()) != nullptr) {
            // Moving entity: the fixed step (kProjectileStepPerFrame).
            entity->update(kProjectileStepPerFrame);
        } else {
            entity->update(dt);
        }
    }
    // Arrow hits (Phase 21): a swept test from each arrow's pre-step
    // position to its new one against every live mob's AABB — raycastAabb
    // with the per-step displacement as the direction, where t in [0, 1]
    // is a hit inside this step (t = 0: the arrow starts inside the mob).
    // The first mob hit (entity index order) takes the launcher's data
    // damage + the charge-scaled knockback along the arrow's horizontal
    // velocity (Phase 42, core/bow.hpp arrowKnockback), and the arrow is
    // consumed (the despawn pass removes it this frame). Arrows fly
    // through the player and every other entity
    // (collidesWithEntities = false) and never damage the player
    // (single-player v1, docs/KNOWN_ISSUES.md).
    for (auto& [arrow, prev] : arrowSteps) {
        if (arrow->dead()) {
            continue;  // a mob hit this step already consumed it
        }
        if (arrow->stuck()) {
            continue;  // stuck in a block (Phase 24): no motion, no hits
        }
        const Vec3 step = arrow->position() - prev;
        // Mob-shot arrows (Phase 27) can hit the player: the same swept
        // test as the mobs, run FIRST (the arrow's target). The player's
        // own bow arrows are unflagged and keep flying through the player
        // (no self-damage, single-player v1).
        if (arrow->hurtPlayer()) {
            const float tp = raycastAabb(prev, step, m_player->aabbMin(),
                                         m_player->aabbMax());
            if (tp >= 0.0f && tp <= 1.0f && !m_player->isDead()) {
                m_player->damage(arrow->damage());
                if (arrow->effectId() >= 0) {
                    // SURV-5: the arrow's effect lands on the player
                    // (the skeleton's wither) through the SAME single
                    // choke point as the spider's on_hit_effect (the
                    // name is init-cross-checked; the instant settlement
                    // is a no-op for a lingering effect like the wither).
                    applyPlayerEffect(m_effects.at(arrow->effectId()).name,
                                      arrow->effectDuration(),
                                      arrow->effectTier());
                    syncAbsorption(); // the pool takes the new total
                }
                arrow->kill();
                MC_LOG_DEBUG("mob arrow hit player for " +
                             std::to_string(arrow->damage()));
                continue;  // consumed: no mob test this step
            }
        }
        for (auto& entity : m_entities) {
            auto* mob = dynamic_cast<MobEntity*>(entity.get());
            if (mob == nullptr || mob->dead() || mob == arrow->source()) {
                continue;  // the shooter's own arrow never self-hits
            }
            const float t =
                raycastAabb(prev, step, mob->aabbMin(), mob->aabbMax());
            if (t < 0.0f || t > 1.0f) {
                continue;
            }
            mob->damage(arrow->damage());
            // Phase 42: the knockback scales with the shot's charge
            // (arrowKnockback in core/bow.hpp — a full-power shot
            // reproduces the legacy melee-style constants bit-exactly).
            mob->addKnockback(arrowKnockback(arrow->velocity(),
                                             arrow->power()));
            if (arrow->effectId() >= 0 && !mob->dead()) {
                // SURV-5: the arrow's effect lands on the mob (the poison
                // bow's arrow). applyEffect runs the gates (affects_mobs —
                // init-cross-checked for a bow's arrow — and the undead
                // exemption: a poisoned zombie / skeleton is ignored).
                // A mob KILLED by this arrow's damage takes no effect
                // (it despawns with its drops this frame).
                mob->applyEffect(arrow->effectId(), arrow->effectDuration(),
                                 m_effects, arrow->effectTier());
            }
            m_sound.play("hurt");
            arrow->kill();
            break;
        }
    }
    // Entity-vs-entity separation (Phase 19; the player-vs-item part is
    // the Phase 16 pass, now generalized): the pass runs over a fixed-order
    // body list — the player first (immovable: Minecraft pushes items out
    // of the player), then the entity list by index — so the pair order
    // and the exact-center tie are deterministic (an item dropped exactly
    // at the player's feet goes to the player's -x side, the Phase 16
    // direction). Overlapping pairs are pushed apart along the
    // minimum-penetration axis (50/50 between two pushable bodies, full
    // span against an immovable one); projectiles never participate
    // (collidesWithEntities). Repeated until stable, capped (see
    // separateEntityList).
    std::vector<Entity*> bodies;
    bodies.reserve(m_entities.size() + 1);
    bodies.push_back(m_player.operator->());
    for (auto& entity : m_entities) {
        bodies.push_back(entity.get());
    }
    separateEntityList(bodies);
    // Item merge: index order, `j` absorbed into `i` (j > i). A removed
    // entity is skipped as a future merge target, so each item is absorbed
    // at most once (deterministic result for a given spawn order).
    const std::size_t n = m_entities.size();
    std::vector<bool> gone(n, false);
    for (std::size_t i = 0; i < n; ++i) {
        if (gone[i]) {
            continue;
        }
        auto* a = dynamic_cast<ItemEntity*>(m_entities[i].get());
        if (a == nullptr) {
            continue;
        }
        for (std::size_t j = i + 1; j < n; ++j) {
            if (gone[j]) {
                continue;
            }
            auto* b = dynamic_cast<ItemEntity*>(m_entities[j].get());
            if (b != nullptr && b->canMergeInto(*a)) {
                a->absorb(*b);
                gone[j] = true;
            }
        }
    }
    // Pickup (Phase 11): items within kPickupDistance of the player's
    // bottom-center that are past their pickup delay go into the inventory
    // (deterministic index order; if the inventory has no room the item
    // stays where it is and is retried next frame).
    const float pickupSq = kPickupDistance * kPickupDistance;
    for (std::size_t i = 0; i < n; ++i) {
        if (gone[i]) {
            continue;
        }
        auto* item = dynamic_cast<ItemEntity*>(m_entities[i].get());
        if (item == nullptr || !item->pickupReady()) {
            continue;
        }
        const Vec3 d = item->position() - playerPos;
        if (lengthSq(d) > pickupSq) {
            continue;
        }
        // Validity is guaranteed: drops are validated at init and only
        // spawn from that validated data.
        const ItemID iid = m_items.idForName(item->itemName());
        // SURV-3: the picked-up stack's damage returns with it (the
        // Inventory::add merge rule: same-item AND same-damage only).
        if (m_inventory.add(iid, item->count(), m_items, item->damage()) < 0) {
            continue; // inventory full: the item stays on the ground
        }
        gone[i] = true;
        // CRAFT-3: the generic "pickup" timbre (items have no
        // sound_material — YAGNI, documented), spatialized at the picked
        // item's position.
        m_sound.play("pickup", item->position());
    }
    // Arrow pickup (Phase 42): a STUCK arrow within kArrowPickupRadius of
    // the player's bottom-center returns to the inventory as the
    // launcher's ammo item (Minecraft: walk up to a stuck arrow to pick
    // it up). Flying arrows are not pickable (they are in the air). Same
    // deterministic index order as the item pickup above; if the
    // inventory has no room the arrow stays stuck and is retried next
    // frame.
    {
        const float arrowPickupSq =
            kArrowPickupRadius * kArrowPickupRadius;
        for (std::size_t i = 0; i < n; ++i) {
            if (gone[i]) {
                continue;
            }
            auto* arrow = dynamic_cast<Projectile*>(m_entities[i].get());
            if (arrow == nullptr || !arrow->stuck() ||
                arrow->ammoItem().empty()) {
                continue;
            }
            const Vec3 d = arrow->position() - playerPos;
            if (lengthSq(d) > arrowPickupSq) {
                continue;
            }
            // Validity is guaranteed: the ammo item exists (init
            // cross-check, the launcher's 'ammo' field / the mob's 'ammo').
            const ItemID aid = m_items.idForName(arrow->ammoItem());
            if (m_inventory.add(aid, 1, m_items) < 0) {
                continue; // inventory full: the arrow stays stuck
            }
            gone[i] = true;
            m_sound.play("pickup");
            const Vec3 p = arrow->position();
            MC_LOG_DEBUG("picked up stuck arrow (" +
                         arrow->ammoItem() + ") at (" +
                         std::to_string(p.x) + ", " + std::to_string(p.y) +
                         ", " + std::to_string(p.z) + ")");
        }
    }
    // Despawn: merged-away / picked-up + aged-out / impact / below the
    // kill plane + dead mobs (Phase 20: their data-defined drops spawn
    // here, AFTER the list rebuild — pushing during the scan would lose
    // them) + mobs beyond the despawn distance from the player.
    const float despawnSq =
        m_spawnConfig.despawnDistance * m_spawnConfig.despawnDistance;
    struct PendingDrop {
        std::string item;
        int count;
        Vec3 pos;
    };
    std::vector<PendingDrop> mobDrops;
    int mobDeaths = 0;
    for (std::size_t i = 0; i < n; ++i) {
        if (gone[i]) {
            continue;
        }
        MobEntity* mob = dynamic_cast<MobEntity*>(m_entities[i].get());
        if (mob != nullptr && !mob->dead()) {
            const Vec3 d = mob->position() - playerPos;
            if (lengthSq(d) > despawnSq) {
                gone[i] = true;
                continue;
            }
        }
        if (!m_entities[i]->shouldDespawn()) {
            continue;
        }
        gone[i] = true;
        if (mob != nullptr && mob->dead()) {
            // SURV-7: the per-mob kill counter — every DEAD mob, drop-
            // less included (the death is the event, not the drop; a
            // distance despawn does NOT count — the mob was never dead).
            ++m_stats["kill." + mob->def().name];
        }
        if (mob != nullptr && mob->dead() &&
            !mob->def().properties.drops.empty()) {
            // Every data entry becomes its own item entity (Phase 28: a
            // mob can drop several items, e.g. leather + beef), spawned
            // in list order at the same rest position. Phase 38: a range
            // entry is rolled once with the mob's seeded stream (a
            // deterministic death frame → a reproducible roll); a roll of
            // 0 drops nothing for that entry (the skeleton's 0-2 bones).
            for (const MobDrop& drop : mob->def().properties.drops) {
                const int n = mob->rollDropCount(drop.countMin,
                                                 drop.countMax);
                if (n > 0) {
                    mobDrops.push_back({drop.item, n, mob->position()});
                }
            }
            ++mobDeaths;
        }
    }
    if (std::none_of(gone.begin(), gone.end(), [](bool g) { return g; })) {
        return;
    }
    std::vector<std::unique_ptr<Entity>> kept;
    kept.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        if (!gone[i]) {
            kept.push_back(std::move(m_entities[i]));
        } else if (auto* mob = dynamic_cast<MobEntity*>(m_entities[i].get())) {
            // ENT-1: free the mob's dynamic swing mesh (if it ever swung
            // — the key is absent otherwise) so its GL buffers die with
            // the entity. The pointer key is valid here: the pointee is
            // destroyed only AFTER this rebuild (unique_ptr move).
            m_swingMeshes.erase(mob);
        }
    }
    m_entities = std::move(kept);
    for (const PendingDrop& drop : mobDrops) {
        // Validity is guaranteed: drops are validated at init. The drop
        // spawns at the mob's bottom-center with zero velocity (the same
        // deterministic rest-position convention as broken blocks).
        const Item& it = m_items.at(m_items.idForName(drop.item));
        m_entities.push_back(std::make_unique<ItemEntity>(
            m_world, m_blocks, it.name, it.maxStack, drop.count, drop.pos));
    }
    if (mobDeaths > 0) {
        m_sound.play("death");
    }
}

void Game::drawItemEntities(const Mat4& view, const Mat4& projection,
                            RenderPass pass) {
    // PRE-6 / LGT-4: dropped items take the light of the cell they sit in
    // (the AABB-center cell, itemDropLight) via the vertex shader's
    // per-draw override (u_useLightOverride / u_lightOverrideSky /
    // u_lightOverrideBlock — the PRE-6 drop names, renamed neutral when
    // the beacon beams joined the pattern) — a stone cube dropped in an
    // unlit cave no longer glows like daylight.
    // render.drop_light (code default true, the render.fog /
    // render.water_ripples getBool precedent — config.json is untouched)
    // gates it: OFF = the three uniforms are NEVER set,
    // u_useLightOverride stays its link-time 0 and the vertex branch never
    // runs, so the pipeline is the v0.29.0 one bit-for-bit (the gate 113
    // S1 off control). The program's uniform state persists per program
    // object across draws (Renderer::draw unbinds after every draw), so
    // each drop draw MUST reset u_useLightOverride = 0 afterwards — a
    // leak would light the next draw (mobs, chunks, arrows) with this
    // drop's cell light.
    const bool dropLightOn = m_config.getBool("render.drop_light", true);
    for (const auto& entity : m_entities) {
        const ItemEntity* item =
            dynamic_cast<const ItemEntity*>(entity.get());
        if (item == nullptr) {
            continue;
        }
        const ItemID iid = m_items.idForName(item->itemName());
        if (!iid.valid()) {
            continue; // cannot happen (item data is validated at init)
        }
        const Item& it = m_items.at(iid);
        // CRAFT-2: a model item (a tool / material with a 'model'
        // section) renders its box mesh — replacing the legacy INVISIBLE
        // non-block drop. The box samples the per-item model texture
        // (the icon PNG: it carries transparent pixels, so the model
        // joins the BLENDING pass only — unblended they would draw as an
        // opaque black square, the icon-hand pattern), double-sided
        // (culling OFF for the batch). No model = the legacy paths
        // bit-level unchanged (the block cube below / the invisible
        // non-block drop).
        if (it.model.has_value()) {
            const std::size_t idx = static_cast<std::size_t>(iid.value);
            // PRE-5: the model item routes by its init-time icon census
            // (m_itemCutout) — a PURE cutout icon (alpha set ⊆ {0, 255})
            // draws in the CUTOUT pass (discard + depth write, visible
            // through the leaf holes); an icon with a translucent core
            // stays in the BLEND pass. Never the opaque pass (the icon
            // carries transparent pixels — unblended it would draw as an
            // opaque black square, the CRAFT-2 pattern).
            const RenderPass want =
                (idx < m_itemCutout.size() && m_itemCutout[idx])
                    ? RenderPass::Cutout
                    : RenderPass::Blend;
            if (pass != want) {
                continue;
            }
            if (idx >= m_itemModelTextures.size() ||
                !m_itemModelTextures[idx].valid()) {
                continue; // cannot happen (the texture loads at init)
            }
            const Mesh& mesh = itemModelMesh(iid);
            if (!mesh.valid()) {
                continue;
            }
            // Bob + spin from the frame counter (see kItemBobPeriodFrames):
            // deterministic in headless runs; at 60 FPS the pickup bobs
            // with a 1.5 s period and spins around once every 4 s (the
            // block-cube-drop convention).
            const Vec3 p = item->position();
            const float t = static_cast<float>(m_frameCount);
            const float bob =
                kItemBobAmplitude *
                std::sin(2.0f * std::numbers::pi * t /
                         static_cast<float>(kItemBobPeriodFrames));
            const float spin = 2.0f * std::numbers::pi * t /
                               static_cast<float>(kItemSpinPeriodFrames);
            const Vec3 center {p.x, p.y + item->height() * 0.5f + bob, p.z};
            // The model geometry is canonical unit space (-0.5..0.5); the
            // block cube bakes kSize into its geometry, the model applies
            // it in the matrix — the same world footprint.
            const Mat4 model = Mat4::multiply(
                Mat4::translate(center),
                Mat4::multiply(Mat4::rotateY(spin),
                               Mat4::scale(ItemEntity::kSize)));
            // The program must be BIND first: Renderer::draw unbinds the
            // program after every draw, and a uniform call on an unbound
            // program is GL_INVALID_OPERATION (the residual error then
            // poisons the lazy mesh-upload's glGetError — same pattern as
            // drawArrows).
            m_basicShader.bind();
            m_basicShader.setUniform1i("u_useTexture", 1);
            // PRE-6: this drop's cell light (the vertex override; the
            // reset below restores the 0 every other draw expects).
            if (dropLightOn) {
                const std::pair<float, float> light =
                    itemDropLight(m_world, *item);
                m_basicShader.setUniform1i("u_useLightOverride", 1);
                m_basicShader.setUniform1f("u_lightOverrideSky", light.first);
                m_basicShader.setUniform1f("u_lightOverrideBlock",
                                           light.second);
            }
            m_renderer.setCulling(false); // double-sided box
            m_itemModelTextures[idx].bind();
            m_renderer.draw(mesh, m_basicShader, view, projection, model);
            // PRE-6: the reset (draw unbound the program — re-bind): the
            // override must not leak into the draws that follow.
            if (dropLightOn) {
                m_basicShader.bind();
                m_basicShader.setUniform1i("u_useLightOverride", 0);
            }
            m_atlas.texture().bind(); // restore for the passes that follow
            m_renderer.setCulling(true);
            continue;
        }
        if (it.type != ItemType::Block) {
            // Tools have no in-world cube (the held-item view arrives with
            // the UI in Phase 15).
            continue;
        }
        const Block& block = m_blocks.at(it.block);
        // PRE-5: the block-cube drop routes by the block's properties —
        // opaque blocks stay in the opaque pass, `cutout` blocks (leaves)
        // join the cutout pass, the remaining transparent blocks (glass)
        // the blend pass.
        const RenderPass want =
            !block.properties.transparent
                ? RenderPass::Opaque
                : (block.properties.cutout ? RenderPass::Cutout
                                           : RenderPass::Blend);
        if (pass != want) {
            continue;
        }
        const Mesh& mesh = itemMesh(it.block);
        if (!mesh.valid()) {
            continue;
        }
        // Bob + spin from the frame counter (see kItemBobPeriodFrames):
        // deterministic in headless runs; at 60 FPS the pickup bobs with a
        // 1.5 s period and spins around once every 4 s.
        const Vec3 p = item->position();
        const float t = static_cast<float>(m_frameCount);
        const float bob =
            kItemBobAmplitude *
            std::sin(2.0f * std::numbers::pi * t /
                     static_cast<float>(kItemBobPeriodFrames));
        const float spin = 2.0f * std::numbers::pi * t /
                           static_cast<float>(kItemSpinPeriodFrames);
        const Vec3 center {p.x, p.y + item->height() * 0.5f + bob, p.z};
        const Mat4 model =
            Mat4::multiply(Mat4::translate(center), Mat4::rotateY(spin));
        // PRE-6: the block-cube path needs the program bound before the
        // per-draw uniform (Renderer::draw binds on its own, but the
        // override must be set first — the model path's pattern).
        if (dropLightOn) {
            const std::pair<float, float> light =
                itemDropLight(m_world, *item);
            m_basicShader.bind();
            m_basicShader.setUniform1i("u_useLightOverride", 1);
            m_basicShader.setUniform1f("u_lightOverrideSky", light.first);
            m_basicShader.setUniform1f("u_lightOverrideBlock", light.second);
        }
        m_renderer.draw(mesh, m_basicShader, view, projection, model);
        // PRE-6: the reset (draw unbound the program — re-bind).
        if (dropLightOn) {
            m_basicShader.bind();
            m_basicShader.setUniform1i("u_useLightOverride", 0);
        }
    }
}

const Mesh& Game::itemMesh(BlockID id) {
    auto it = m_itemMeshes.find(id);
    if (it != m_itemMeshes.end()) {
        return it->second;
    }
    Mesh& mesh = m_itemMeshes[id];
    const ItemMeshGeometry geom =
        makeItemCubeGeometry(m_blocks.at(id), m_atlas.layout());
    auto r = mesh.upload(geom.vertices, geom.indices);
    if (!r.ok()) {
        MC_LOG_ERROR("Item mesh build failed for block '" +
                     m_blocks.at(id).name + "': " + r.error().describe());
    }
    return mesh;
}

const Mesh& Game::itemModelMesh(ItemID id) {
    // CRAFT-2: the cached box-model mesh for a model item. Only model
    // items are looked up (the callers gate on item.model / the texture);
    // a non-model id yields the invalid cached mesh (the callers skip
    // invalid). The geometry is static per item (makeItemModelGeometry
    // is a pure function of the parsed data), so build-once + cache is
    // byte-reproducible across runs. The cache is sized at init, before
    // any frame renders.
    const std::size_t idx = static_cast<std::size_t>(id.value);
    assert(idx < m_itemModelMeshes.size());
    Mesh& mesh = m_itemModelMeshes[idx];
    if (!mesh.valid()) {
        const Item& item = m_items.at(id);
        if (item.model.has_value()) {
            // PRE-5: the geometry carries the cutout identity mark (the
            // init-time icon census) on its vertices (location 8).
            const ItemMeshGeometry geom = makeItemModelGeometry(
                item.model.value(),
                idx < m_itemCutout.size() && m_itemCutout[idx]);
            auto r = mesh.upload(geom.vertices, geom.indices);
            if (!r.ok()) {
                MC_LOG_ERROR("Item model mesh build failed for item '" +
                             item.name + "': " + r.error().describe());
            }
        }
    }
    return mesh;
}

void Game::attemptMobSpawn() {
    // The live-mob census (count + bottom-centers for the clearance rule).
    int mobCount = 0;
    std::vector<Vec3> mobPositions;
    for (const auto& entity : m_entities) {
        if (dynamic_cast<const MobEntity*>(entity.get()) != nullptr) {
            ++mobCount;
            mobPositions.push_back(entity->position());
        }
    }
    if (mobCount >= m_spawnConfig.maxMobs) {
        MC_LOG_DEBUG("mob spawn f" + std::to_string(m_frameCount) +
                     ": cap reached (" + std::to_string(mobCount) +
                     " live), attempt skipped");
        return;
    }
    // Deterministic weighted type pick among the data-listed mobs (the
    // seed mixes the world seed with the frame, so the choice is a pure
    // function of the run). ONE stream value per attempt, reduced into
    // the total-weight domain and walked over the cumulative list (the
    // Phase 39 weights; all weights 1 — the shipped data — is exactly
    // the legacy pick % list.size()). Note: the pick happens BEFORE the
    // light gate, so an attempt whose type cannot spawn at the current
    // light is simply wasted (e.g. a zombie pick by day) — the cadence,
    // not the type, decides when attempts happen.
    const std::uint64_t pick =
        splitmix64(static_cast<std::uint64_t>(m_generator->seed()) ^
                   (std::uint64_t{0x9E3779B97F4A7C15} *
                    static_cast<std::uint64_t>(m_frameCount)));
    const Mob& def = m_mobTypes.at(m_mobTypes.idForName(
        m_spawnConfig
            .mobs[m_spawnConfig.pickIndex(pick %
                                          static_cast<std::uint64_t>(
                                              m_spawnConfig.totalWeight()))]
            .id));

    // Player anchor: the player's column + the floor block under its feet
    // (the highest solid cell at or below the feet in that column).
    const Vec3 p = m_player->position();
    const int px = static_cast<int>(std::floor(p.x));
    const int pz = static_cast<int>(std::floor(p.z));
    int floorY = -1;
    for (int y = static_cast<int>(std::floor(p.y)); y >= 0; --y) {
        if (m_blocks.at(m_world.getBlock(px, y, pz)).properties.solid) {
            floorY = y;
            break;
        }
    }
    if (floorY < 0) {
        return;  // the player is over the void: no spawning
    }
    // Spawn protection (Phase 25): hostiles are gated by the difficulty's
    // radius around the world spawn point; passive mobs pass 0 (exempt).
    const float protection = def.properties.hostile
        ? spawnProtectionRadius(m_difficulty)
        : 0.0f;
    auto cell = findMobSpawnCell(m_world, m_blocks, m_spawnConfig.minRadius,
                                 m_spawnConfig.maxRadius, px, pz, floorY,
                                 def.model.width, def.model.height,
                                 m_dayCycle.dayFactor(),
                                 def.properties.spawnMaxLight,
                                 m_player->aabbMin(), m_player->aabbMax(),
                                 mobPositions, protection, m_spawnPoint.x,
                                 m_spawnPoint.z, m_nightSkyLevel);
    if (!cell) {
        MC_LOG_DEBUG("mob spawn f" + std::to_string(m_frameCount) +
                     ": pick '" + def.name + "' found no valid cell "
                     "(light gate / clearance / protection)");
        return;
    }
    // Spawn at the cell center, feet on the floor top; the per-mob seed is
    // the world seed mixed with the (increasing) spawn counter, so the same
    // runs produce the same mobs with the same AI streams.
    const Vec3 spawn {static_cast<float>(cell->x) + 0.5f,
                      static_cast<float>(cell->y) + 1.0f,
                      static_cast<float>(cell->z) + 0.5f};
    MC_LOG_DEBUG("mob spawn f" + std::to_string(m_frameCount) +
                 ": '" + def.name + "' at (" + std::to_string(spawn.x) +
                 ", " + std::to_string(spawn.y) + ", " +
                 std::to_string(spawn.z) + ")");
    ++m_spawnCounter;
    const std::uint64_t seed =
        static_cast<std::uint64_t>(m_generator->seed()) ^
        (std::uint64_t{0x9E3779B97F4A7C15} * m_spawnCounter);
    auto mob = std::make_unique<MobEntity>(m_world, m_blocks, def, spawn,
                                           seed);
    // Phase 58: the fire system reaches the mobs (Minecraft's lava ignites
    // every entity). Arm from the SAME fire definition + config duration
    // as the player's ignition; an asset set without the fire effect
    // leaves every mob inert (structural, the P57 pattern).
    if (m_fireEffectId != -1) {
        const Effect& fire = m_effects.at(m_fireEffectId);
        mob->armFire(fire.tickInterval, fire.tickAmount,
                     m_player->params().fireDurationSeconds);
    }
    // The AI's landed attacks damage the player (Phase 16's damage path
    // owns the flash/sound/death — updateSurvival polls the health).
    // Hostile damage is scaled by the difficulty (Phase 25); passive mobs
    // (attack_damage 0 in the data) pass the scale untouched.
    const float damageScale = def.properties.hostile
        ? hostileDamageScale(m_difficulty)
        : 1.0f;
    // Phase 46: a landed melee hit also applies the mob's on_hit_effect
    // (the spider's poison) — the effect name is init-cross-checked, so
    // idForName cannot fail here (the dead check guards both).
    const std::string hitEffectName = def.properties.hitEffectName;
    const double hitEffectDuration = def.properties.hitEffectDuration;
    const int hitEffectTier = def.properties.hitEffectTier;
    mob->setHitCallback(
        [this, damageScale, hitEffectName, hitEffectDuration,
         hitEffectTier](float amount) {
            if (m_player->isDead()) {
                return;
            }
            m_player->damage(amount * damageScale);
            if (!hitEffectName.empty()) {
                // SURV-4: through the single choke point (the name is
                // init-cross-checked; the instant settlement is a no-op for
                // the pre-SURV-4 hit effects — the spider's poison has
                // none). SURV-6: the data's tier rides along (0 = the
                // pre-SURV-6 apply, bit-identical).
                applyPlayerEffect(hitEffectName, hitEffectDuration,
                                  hitEffectTier);
                syncAbsorption(); // Phase 72 (the pool takes the new total)
            }
        });
    // Ranged mobs (Phase 27, the skeleton): the AI's shot spawns an arrow
    // from the mob's head at the player's eye — the same Projectile the
    // bow fires (it sticks in blocks, ages out, and renders), flagged
    // hurtPlayer so the update pass sweeps it against the player too.
    // Damage = the data attack_damage × the difficulty scale (the melee
    // callback above uses the same scale). The raw pointer stays valid:
    // the mob lives in m_entities until the despawn pass removes it, and
    // the callback only runs while the mob is alive (runAi).
    if (def.properties.shootRange > 0.0f) {
        const float arrowDamage = def.properties.attackDamage * damageScale;
        const float arrowSpeed = def.properties.shootSpeed;
        // Phase 42: the mob's arrows are full-power shots (power 1.0 =
        // the legacy knockback constants) and return the mob's data 'ammo'
        // item when a stuck one is picked up (registry: a ranged mob must
        // define it; init: the item exists).
        const std::string arrowAmmo = def.properties.ammoItem;
        // SURV-5: the mob's arrow effect (the skeleton's wither) — the
        // data pair captured like the hit effect above; the id resolves
        // at SHOT time (init cross-checked the name's existence, so
        // idForName cannot fail; empty = a pre-SURV-5 mob, the arrow
        // carries no effect, bit-identical).
        const std::string arrowEffectName = def.properties.arrowEffectName;
        const double arrowEffectDuration = def.properties.arrowEffectDuration;
        const int arrowEffectTier = def.properties.arrowEffectTier;
        MobEntity* raw = mob.get();
        mob->setShootCallback(
            [this, raw, arrowSpeed, arrowDamage, arrowAmmo, arrowEffectName,
             arrowEffectDuration, arrowEffectTier]() {
            if (m_player->isDead() || raw->dead()) {
                return;
            }
            const Vec3 eye = raw->eyePosition();
            const Vec3 target = m_player->eyePosition();
            Vec3 toPlayer = target - eye;
            const float dist = std::sqrt(dot(toPlayer, toPlayer));
            if (dist < 1e-4f) {
                return;
            }
            // Phase 38 LOS gate: a wall between the two makes the arrow
            // stick and vanish on impact, so don't fire at all. The AI
            // already consumed the cooldown, so the skeleton retries after
            // the full interval (Minecraft's behavior: it keeps nocking
            // but never releases).
            if (!hasLineOfSight(m_world, eye, target)) {
                MC_LOG_DEBUG("skeleton shot blocked (no line of sight) from ("
                             + std::to_string(eye.x) + ", " +
                             std::to_string(eye.y) + ", " +
                             std::to_string(eye.z) + ") dist=" +
                             std::to_string(dist));
                return;
            }
            // Phase 38 leading: aim where the player WILL be when the
            // arrow arrives (computeAimDirection iterates the prediction
            // twice; for a stationary target it reproduces the Phase 27
            // arc-only aim bit-exactly, so standing players are hit exactly
            // as before).
            const Vec3 dir = computeAimDirection(eye, target,
                                                 m_player->velocity(),
                                                 arrowSpeed);
            // SURV-5: the arrow's effect (the wither on the skeleton) —
            // resolved to the registry id at shot time (init
            // cross-checked the name's existence; empty = -1 = no
            // effect, the pre-SURV-5 arrow).
            int effectId = -1;
            double effectDuration = 0.0;
            int effectTier = 0;
            if (!arrowEffectName.empty()) {
                effectId = m_effects.idForName(arrowEffectName);
                effectDuration = arrowEffectDuration;
                effectTier = arrowEffectTier;
            }
            auto arrow = std::make_unique<Projectile>(
                m_world, m_blocks, eye + dir * kArrowSpawnOffset, dir,
                arrowSpeed, arrowDamage, /*hurtPlayer=*/true,
                /*power=*/1.0f, arrowAmmo, effectId, effectDuration,
                effectTier);
            arrow->setSource(raw);  // the hit test skips its shooter
            m_entities.push_back(std::move(arrow));
            m_sound.play("shoot");
            MC_LOG_DEBUG("skeleton shot: from (" + std::to_string(eye.x) +
                         ", " + std::to_string(eye.y) + ", " +
                         std::to_string(eye.z) + ") dist=" +
                         std::to_string(dist) + " dmg=" +
                         std::to_string(arrowDamage));
        });
    }
    m_entities.push_back(std::move(mob));
}

void Game::drawMobs(const Mat4& view, const Mat4& projection) {
    // Mobs are always opaque (the model boxes carry no transparency), so
    // they draw with the opaque item pass. Dead mobs are removed in the
    // same frame's update, so a dead one is never rendered.
    //
    // ENT-1: an actively swinging mob (config on + the type has swing
    // data + the phase != 0) draws its per-entity DYNAMIC mesh (the
    // frame's vertices — the swing parts rotated, the rest verbatim);
    // every other mob takes the STATIC per-type cache (the fast path =
    // structural immunity: phase 0 is the identity pose, so the static
    // mesh is byte-identical to a phase-0 frame).
    for (const auto& entity : m_entities) {
        const MobEntity* mob =
            dynamic_cast<const MobEntity*>(entity.get());
        if (mob == nullptr) {
            continue;
        }
        const Mob& def = m_mobTypes.at(mob->typeId());
        const Mesh& mesh =
            limbSwingActive(m_limbSwing, !def.model.swings.empty(),
                            mob->swingPhase())
                ? swingMobMesh(*mob, def)
                : mobMesh(mob->typeId());
        if (!mesh.valid()) {
            continue;
        }
        // Bottom-center origin: translate to the mob, rotate by its facing
        // (camera yaw convention — the -Z face of a box is the front).
        const Mat4 model = Mat4::multiply(
            Mat4::translate(mob->position()), Mat4::rotateY(mob->facingYaw()));
        m_renderer.draw(mesh, m_basicShader, view, projection, model);
    }
}

const Mesh& Game::mobMesh(MobID id) {
    auto it = m_mobMeshes.find(id);
    if (it != m_mobMeshes.end()) {
        return it->second;
    }
    Mesh& mesh = m_mobMeshes[id];
    const ItemMeshGeometry geom =
        makeMobModelGeometry(m_mobTypes.at(id).model, m_atlas.layout());
    auto r = mesh.upload(geom.vertices, geom.indices);
    if (!r.ok()) {
        MC_LOG_ERROR("Mob mesh build failed for mob '" +
                     m_mobTypes.at(id).name + "': " + r.error().describe());
    }
    return mesh;
}

const Mesh& Game::swingMobMesh(const MobEntity& mob, const Mob& def) {
    // The per-type swing geometry (built once, lazily) + this frame's
    // vertices (the pure buildSwingFrame into the shared scratch) + the
    // per-entity dynamic mesh (allocated on the first swing frame; its
    // VBO is rewritten every frame afterwards — the topology is static,
    // so the IBO/VAO are never touched again).
    const SwingModel& sm = swingModel(def.id);
    buildSwingFrame(sm, mob.swingPhase(), m_swingFrameVerts);
    Mesh& mesh = m_swingMeshes[&mob];
    if (!mesh.valid()) {
        auto r = mesh.uploadDynamic(sm.base, sm.indices);
        if (!r.ok()) {
            MC_LOG_ERROR("Mob swing mesh build failed for mob '" + def.name +
                         "': " + r.error().describe());
        }
    }
    mesh.updateVertices(m_swingFrameVerts);
    return mesh;
}

const SwingModel& Game::swingModel(MobID id) {
    auto it = m_swingModels.find(id);
    if (it != m_swingModels.end()) {
        return it->second;
    }
    SwingModel& sm = m_swingModels[id];
    sm = buildSwingModel(m_mobTypes.at(id).model, m_atlas.layout());
    return sm;
}

void Game::drawArrows(const Mat4& view, const Mat4& projection) {
    // The arrows render in the basic shader's solid-color path (v1 has no
    // arrow texture): u_useTexture is per-program state shared with every
    // textured draw of the frame, so it is switched here and restored
    // immediately (the next pass, the particles, samples the atlas).
    // PRE-9: each arrow is THREE parts (the brown shaft, the gray tip,
    // the white fletching) — three single-color draws per arrow (the
    // classic MC arrow colors).
    buildArrowMeshes();
    if (!m_arrowShaftMesh.valid() || !m_arrowTipMesh.valid() ||
        !m_arrowFletchMesh.valid()) {
        return;
    }
    // The program is UNBOUND here: Renderer::draw binds/unbinds around
    // every draw, and the last textured draw of the opaque pass left it
    // off. Setting a uniform on an unbound program is
    // GL_INVALID_OPERATION, so bind it for the switch (uniforms persist
    // on the program object; the next pass re-binds per draw).
    m_basicShader.bind();
    m_basicShader.setUniform1i("u_useTexture", 0);
    m_basicShader.setUniform1f("u_alpha", 1.0f); // arrows are opaque
    for (const auto& entity : m_entities) {
        const Projectile* arrow =
            dynamic_cast<const Projectile*>(entity.get());
        if (arrow == nullptr) {
            continue;
        }
        // Collision-box center (the box is kSize tall; the part meshes
        // are centered on the origin) oriented along the travel direction
        // (local -Z). A stuck arrow (Phase 24) keeps the pre-impact
        // direction for the orientation — the collision resolver zeroed
        // the blocked axis of the live velocity. Dead arrows are removed
        // in this frame's update, so a dead one is never rendered.
        const Vec3 c = arrow->position() +
                       Vec3 {0.0f, Projectile::kSize * 0.5f, 0.0f};
        const Vec3 dir =
            arrow->stuck() ? arrow->stuckDir() : arrow->velocity();
        const auto [yaw, pitch] = arrowOrientation(dir);
        const Mat4 model = Mat4::multiply(
            Mat4::multiply(Mat4::translate(c), Mat4::rotateY(yaw)),
            Mat4::rotateX(pitch));
        // Three parts, one color each (re-bind before each: the draw
        // unbinds, and u_color is per-part).
        m_basicShader.bind();
        m_basicShader.setUniform3f("u_color", kArrowShaftColorR,
                                   kArrowShaftColorG, kArrowShaftColorB);
        m_renderer.draw(m_arrowShaftMesh, m_basicShader, view, projection,
                        model);
        m_basicShader.bind();
        m_basicShader.setUniform3f("u_color", kArrowTipColorR,
                                   kArrowTipColorG, kArrowTipColorB);
        m_renderer.draw(m_arrowTipMesh, m_basicShader, view, projection, model);
        m_basicShader.bind();
        m_basicShader.setUniform3f("u_color", kArrowFletchColorR,
                                   kArrowFletchColorG, kArrowFletchColorB);
        m_renderer.draw(m_arrowFletchMesh, m_basicShader, view, projection,
                        model);
    }
    // Re-bind for the restore: the last arrow draw (if any) unbound the
    // program.
    m_basicShader.bind();
    m_basicShader.setUniform1i("u_useTexture", 1);
    m_basicShader.unbind();
}

void Game::buildArrowMeshes() {
    // Idempotent: build the three arrow-part meshes once (the pure
    // makeArrowGeometry — the brown shaft box, the gray tip pyramid, the
    // white fletching box).
    if (m_arrowShaftMesh.valid() && m_arrowTipMesh.valid() &&
        m_arrowFletchMesh.valid()) {
        return;
    }
    const ArrowGeometry geom = makeArrowGeometry();
    const auto rShaft =
        m_arrowShaftMesh.upload(geom.shaft.vertices, geom.shaft.indices);
    const auto rTip =
        m_arrowTipMesh.upload(geom.tip.vertices, geom.tip.indices);
    const auto rFletch = m_arrowFletchMesh.upload(geom.fletching.vertices,
                                                  geom.fletching.indices);
    if (!rShaft.ok()) {
        MC_LOG_ERROR("Arrow shaft mesh build failed: " +
                     rShaft.error().describe());
    }
    if (!rTip.ok()) {
        MC_LOG_ERROR("Arrow tip mesh build failed: " + rTip.error().describe());
    }
    if (!rFletch.ok()) {
        MC_LOG_ERROR("Arrow fletching mesh build failed: " +
                     rFletch.error().describe());
    }
}

const Mesh& Game::handIconMesh() {
    if (m_handIconMesh.valid()) {
        return m_handIconMesh;
    }
    // A unit quad in the local XY plane (z = 0), +Z-facing, CCW from the
    // front. Full-texture UVs (the per-item icon is bound at draw time);
    // the top edge (y = +0.5) is v = 0 so the icon is NOT flipped relative
    // to the 2D UI icon (the UI quad maps its top screen row to v = 0).
    // skyLight = blockLight = 1: the shader's LGT-3 light is
    // max(sky - (8.0/15.0) * (1 - dayFactor), block) = max(< 1, 1.0) =
    // 1.0 — the block-light side dominates at EVERY time of day (the
    // moonlit sky term is never >= 1.0), so the icon stays fully bright
    // at night like the 2D icon. The normal is +Y (NOT the geometric
    // +Z): the vertex shader derives faceShade from the model-rotated
    // normal, and +Y keeps it in the top-face bucket (1.0) through every
    // hand spin state — a geometric +Z normal would land in the 0.6 bucket
    // and dim the icon to 61 %.
    MeshVertex tl;
    tl.position = {-0.5f, 0.5f, 0.0f};
    tl.normal = {0.0f, 1.0f, 0.0f};
    tl.uv = {0.0f, 0.0f};
    tl.skyLight = 1.0f;
    tl.blockLight = 1.0f;
    MeshVertex tr;
    tr.position = {0.5f, 0.5f, 0.0f};
    tr.normal = {0.0f, 1.0f, 0.0f};
    tr.uv = {1.0f, 0.0f};
    tr.skyLight = 1.0f;
    tr.blockLight = 1.0f;
    MeshVertex br;
    br.position = {0.5f, -0.5f, 0.0f};
    br.normal = {0.0f, 1.0f, 0.0f};
    br.uv = {1.0f, 1.0f};
    br.skyLight = 1.0f;
    br.blockLight = 1.0f;
    MeshVertex bl;
    bl.position = {-0.5f, -0.5f, 0.0f};
    bl.normal = {0.0f, 1.0f, 0.0f};
    bl.uv = {0.0f, 1.0f};
    bl.skyLight = 1.0f;
    bl.blockLight = 1.0f;
    std::vector<MeshVertex> verts = {tl, tr, br, bl};
    std::vector<std::uint32_t> idx = {0, 3, 2, 0, 2, 1}; // CCW from +Z
    auto r = m_handIconMesh.upload(verts, idx);
    if (!r.ok()) {
        MC_LOG_ERROR("Hand icon mesh build failed: " + r.error().describe());
    }
    return m_handIconMesh;
}

// Phase 78: the four-slot icon row (Minecraft shows every cooking item
// on the campfire). One 0.35 icon per occupied slot, 0.3 apart in a 1x4
// row across the cell's upper half (total span 1.25 — a 0.5 icon leaves
// no room for four, so the icons shrink to fit; the small neighbour
// overlap is intentional adjacency).
inline constexpr float kCampfireIconScale = 0.35f;
inline constexpr float kCampfireIconSpacing = 0.3f;

void Game::drawCampfireCookItems(const Mat4& view, const Mat4& projection) {
    // Phase 71 (all four slots render in Phase 78): the items cooking on
    // each campfire. No occupied slot: nothing draws (a no-input run has
    // no campfire at all — worldgen never emits one — and a placed-but-
    // empty campfire draws nothing either: baseline byte-identical).
    for (const auto& [pos, st] : m_campfires) {
        bool any = false;
        for (const ItemStack& slot : st.slots) {
            if (!slot.empty()) {
                any = true;
                break;
            }
        }
        if (!any) {
            continue;
        }
        // The icons float in the upper half of the cell (the campfire is
        // a cross block — the food sits above the flame). Crossed quads
        // (the cross-block convention: view-independent; culling stays
        // ON, so from any horizontal angle at least one quad's front
        // face shows). The quad's +Y normal keeps it in the top-face
        // brightness bucket through the 90° cross rotation (the
        // handIconMesh convention). Slot i is centred at the cell centre
        // + (i - 1.5) * the spacing.
        m_basicShader.bind();
        m_basicShader.setUniform1i("u_useTexture", 1);
        m_renderer.enableBlending();
        for (std::size_t i = 0; i < kCampfireSlotCount; ++i) {
            const ItemStack& slot = st.slots[i];
            if (slot.empty()) {
                continue;
            }
            const std::size_t iconIdx =
                static_cast<std::size_t>(slot.item.value);
            if (iconIdx >= m_itemIcons.size() ||
                !m_itemIcons[iconIdx].valid()) {
                continue; // cannot happen (item data is validated at init)
            }
            const float dx =
                (static_cast<float>(i) - 1.5f) * kCampfireIconSpacing;
            const Vec3 center {static_cast<float>(pos.x) + 0.5f + dx,
                               static_cast<float>(pos.y) + 0.75f,
                               static_cast<float>(pos.z) + 0.5f};
            const Mat4 base =
                Mat4::multiply(Mat4::translate(center),
                               Mat4::scale(kCampfireIconScale));
            // The icon texture carries transparent pixels (the
            // drawHeldHand pattern): blending on for the two draws, the
            // atlas restored after the row.
            m_itemIcons[iconIdx].bind();
            m_renderer.draw(handIconMesh(), m_basicShader, view, projection,
                            base);
            m_renderer.draw(handIconMesh(), m_basicShader, view, projection,
                            Mat4::multiply(
                                base, Mat4::rotateY(0.5f * std::numbers::pi)));
        }
        m_atlas.texture().bind(); // restore for the passes that follow
        m_renderer.disableBlending();
    }
}

void Game::drawBeaconBeams(const Mat4& view, const Mat4& projection) {
    // Phase 83: the translucent light column rising from every ACTIVE
    // (level>=1) beacon to the sky. Drawn in the blending pass (the depth
    // test occludes it behind terrain like the other world geometry). A
    // no-input run has no beacon -> m_beaconActive empty -> this is a no-op
    // (baseline byte-identical, structural immunity); the `beacon.beam`
    // config gate is the E2E control (the fire.particles pattern). Culling
    // is off for the beam (both faces of each quad render, alpha-blended).
    //
    // LGT-4: the beam RECEIVES the world light. Each active beacon's draw
    // feeds the vertex per-draw override (u_useLightOverride /
    // u_lightOverrideSky / u_lightOverrideBlock — the PRE-6 itemDropLight
    // pattern, renamed neutral) with beamLightSample: the light of the
    // AIR CELL directly above the beacon block (the beam's lowest cell —
    // the block's own cell is opaque and reads (0, 0)): the beam is
    // moonlit at deep night (the LGT-3 affine retreat with the
    // kNightSkyLevel 7 floor), bright next to glowstone / torches, and
    // full white at open-sky noon = the legacy constant look there. The
    // override is uniform-gated per draw, so the baked vertex light
    // (sky 0 / block 1.0) is the fallback a non-override draw sees — no
    // config gate (the beam draw itself is structurally inert without
    // beacons), and the sample is deterministic frame state (the light
    // field + dayFactor, hard rule 8: no real-dt).
    if (!m_beaconBeam || m_beaconActive.empty()) {
        return;
    }
    m_basicShader.bind();
    m_basicShader.setUniform1i("u_useTexture", 0);
    m_basicShader.setUniform3f("u_color", kBeaconBeamR, kBeaconBeamG,
                               kBeaconBeamB);
    m_basicShader.setUniform1f("u_alpha", kBeaconBeamAlpha);
    m_basicShader.setUniform1f("u_dayFactor", m_dayCycle.dayFactor());
    // Blending is the caller's (the blend section's) state — do NOT
    // enable/disable it here (10-26 user-report fix: the trailing
    // disableBlending leaked past this draw and killed the crack/outline
    // overlay's blending — the mined block rendered through the crack
    // cube's (0,0,0,0) background as a solid black box).
    m_renderer.setCulling(false);
    for (const BeaconBeam& b : m_beaconActive) {
        // The unit-height beam box, scaled to the level's height and
        // positioned on the beacon cell's top face (y = cell y + 1).
        const float height =
            kBeaconBeamBaseHeight + b.level * kBeaconBeamLevelHeight;
        const Mat4 model = Mat4::multiply(
            Mat4::translate(static_cast<float>(b.x) + 0.5f,
                            static_cast<float>(b.y) + 1.0f,
                            static_cast<float>(b.z) + 0.5f),
            Mat4::scale(1.0f, height, 1.0f));
        // LGT-4: this beacon's light (the vertex override: the air cell
        // directly above the beacon block — the beam's lowest cell; the
        // block's own cell is opaque and always reads (0, 0), the gate
        // 119 E2E defect). The reset below restores the 0 every other
        // draw expects — a leak would light the draws that follow with
        // this beacon's cell light, the PRE-6 / UXFIX-1 discipline. The
        // program must be BIND first: Renderer::draw unbinds it after
        // every draw (the top-of-function bind was already consumed by
        // the previous iteration).
        m_basicShader.bind();
        const std::pair<float, float> light =
            beamLightSample(m_world, b.x, b.y, b.z);
        m_basicShader.setUniform1i("u_useLightOverride", 1);
        m_basicShader.setUniform1f("u_lightOverrideSky", light.first);
        m_basicShader.setUniform1f("u_lightOverrideBlock", light.second);
        m_renderer.draw(m_beaconBeamMesh, m_basicShader, view, projection,
                        model);
        // LGT-4: the reset (draw unbound the program — re-bind): the
        // override must not leak into the draws that follow.
        m_basicShader.bind();
        m_basicShader.setUniform1i("u_useLightOverride", 0);
    }
    m_renderer.setCulling(true);
    // (blending state left to the caller — see drawBeaconBeams head)
}

void Game::drawClouds(const Mat4& view, const Mat4& projection) {
    // PRE-4: the drifting translucent cloud layer (world/cloud).
    // render.clouds (code default true, the render.fog getBool precedent —
    // config.json is untouched) gates the draw; OFF = zero mesh, zero
    // draw (the v0.27.0 pipeline bit-identical — the gate's "new-off"
    // control). The clouds are UNLIT WHITE quads through the basic
    // program's solid-color path (the beam pattern: u_useTexture = 0 +
    // u_alpha, culling off so both faces of each quad render
    // alpha-blended; the depth test stays on, so terrain occludes the
    // layer like any other world geometry).
    if (!m_config.getBool("render.clouds", true)) {
        return;
    }
    // The rebuild key: the camera's cloud cell (the floor of the eye's
    // x/z on the kCloudCellBlocks grid — floorDiv, hard rule 1) + the
    // frame-locked drift time step (m_frameCount / kCloudFramesPerCell,
    // hard rule 8). A no-input run is static from the first draw;
    // walking rebuilds once per 16 blocks (the 99.x discipline — no
    // per-frame VBO upload).
    const Vec3 eye = m_camera.position;
    const int cellX =
        floorDiv(static_cast<int>(std::floor(eye.x)), kCloudCellBlocks);
    const int cellZ =
        floorDiv(static_cast<int>(std::floor(eye.z)), kCloudCellBlocks);
    const int t = cloudTimeStep(m_frameCount);
    if (cloudMeshNeedsRebuild(m_cloudMeshValid, m_cloudMeshCellX,
                              m_cloudMeshCellZ, m_cloudMeshT, cellX, cellZ,
                              t)) {
        std::vector<MeshVertex> verts;
        std::vector<std::uint32_t> indices;
        buildCloudMesh(cellX, cellZ, t, verts, indices);
        if (verts.empty()) {
            // Unreachable at 50% density over 576 cells — defensive: no
            // geometry, no draw, and the cache stays invalid so the next
            // frame retries (the key is untouched).
            m_cloudMeshValid = false;
            return;
        }
        const auto up = m_cloudMesh.upload(verts, indices,
                                           MeshMode::Triangles);
        if (!up.ok()) {
            MC_LOG_ERROR("Cloud mesh upload failed: " + up.error().describe());
            m_cloudMeshValid = false;
            return;
        }
        m_cloudMeshValid = true;
        m_cloudMeshCellX = cellX;
        m_cloudMeshCellZ = cellZ;
        m_cloudMeshT = t;
    }
    if (!m_cloudMeshValid) {
        return;
    }
    // The unlit-white draw: the beam's uniform recipe (u_dayFactor is
    // read by the vertex shader — with skyLight = 0 the clouds render
    // full-brightness white at every time of day, the documented
    // deviation "the clouds do not darken at night").
    m_basicShader.bind();
    m_basicShader.setUniform1i("u_useTexture", 0);
    m_basicShader.setUniform3f("u_color", 1.0f, 1.0f, 1.0f);
    m_basicShader.setUniform1f("u_alpha", kCloudAlpha);
    m_basicShader.setUniform1f("u_dayFactor", m_dayCycle.dayFactor());
    // Blending is the caller's (the blend section's) state — do NOT
    // enable/disable it here (10-26 user-report fix: the trailing
    // disableBlending leaked into drawWorldOverlay, whose crack cube
    // then drew unblended — its (0,0,0,0) background painted the mined
    // block solid black).
    m_renderer.setCulling(false);
    // The mesh is WORLD-SPACE (absolute quad coordinates) — the identity
    // model matrix (the beam scales a unit box per beacon; the clouds
    // carry their own positions).
    m_renderer.draw(m_cloudMesh, m_basicShader, view, projection,
                    Mat4::identity());
    m_renderer.setCulling(true);
}

void Game::drawHeldHand(const Mat4& view, const Mat4& projection) {
    // Phase 42: the 3D held-item hand (the Minecraft hand). Rendered only
    // while the pointer is locked and no panel is open (the same gate as
    // the hotbar/held icon) — a no-input run is never locked, so it renders
    // the pre-Phase-42 image (byte-identical baseline). Pure rendering: it
    // reads the selected slot + the bow/eat channel state and draws one
    // model; it mutates nothing.
    if (!m_window.relativeMouseMode() || panelOpen()) {
        return;
    }
    const ItemStack& stack = m_inventory.at(m_selectedSlot);
    if (stack.empty()) {
        return; // empty hand: nothing to hold
    }
    const Item& it = m_items.at(stack.item);
    bool isBlock = it.type == ItemType::Block;
    std::size_t iconIdx = static_cast<std::size_t>(stack.item.value);
    const bool hasIcon =
        !isBlock && iconIdx < m_itemIcons.size() &&
        m_itemIcons[iconIdx].valid();
    // CRAFT-2: a model item renders its box mesh INSTEAD of the icon
    // quad (the block-cube hand is untouched). The bow-draw and eat-lift
    // camera transforms below apply to the model branch exactly as to
    // the icon branch (the eat lift is inherited — zero extra code).
    const bool hasModel =
        !isBlock && it.model.has_value() &&
        iconIdx < m_itemModelTextures.size() &&
        m_itemModelTextures[iconIdx].valid();
    if (!isBlock && !hasIcon && !hasModel) {
        return; // no hand model for this item (v1: no per-item 3D)
    }
    const float scale = isBlock ? kHandCubeScale : kHandIconScale;
    // Animation offsets (camera frame): the bow draw pulls the hand back
    // and tilts the bow toward the face with the charge; the eat channel
    // raises the hand toward the face with the progress.
    Vec3 offset = kHandOffset;
    float spinX = kHandSpinX;
    if (m_bowDrawing) {
        const float p = bowPower(m_bowDrawSeconds);
        offset.z += kHandDrawPullZ * p;
        spinX += kHandDrawSpinX * p;
    } else if (m_eatChannel.active) {
        const float p = m_eatChannel.progress();
        offset.y += kHandEatLiftY * p;
        offset.z += kHandEatPullZ * p;
    }
    // Camera -> world (yaw-then-pitch FPS convention, see Camera::forward):
    // C = T(pos) * Ry(yaw) * Rx(pitch). The hand sits in the camera frame
    // (x = right, y = up, z = behind the eye), spun + tilted, then scaled.
    const Mat4 item = Mat4::multiply(
        Mat4::rotateX(spinX),
        Mat4::multiply(Mat4::rotateY(kHandSpinY), Mat4::scale(scale)));
    const Mat4 place = Mat4::multiply(Mat4::translate(offset), item);
    const Mat4 cam = Mat4::multiply(
        Mat4::translate(m_camera.position),
        Mat4::multiply(Mat4::rotateY(m_camera.yaw),
                       Mat4::rotateX(m_camera.pitch)));
    const Mat4 model = Mat4::multiply(cam, place);

    // The basic shader's texture state is set at the top of the render
    // pass (atlas bound, u_useTexture = 1, u_dayFactor = world light). The
    // block-cube hand uses the atlas as-is; the icon hand binds its per-item
    // texture (and the atlas is restored for the passes that follow).
    // The program must be BIND first: Renderer::draw unbinds the program
    // after every draw, and a uniform call on an unbound program is
    // GL_INVALID_OPERATION (the residual error then poisons the lazy
    // mesh-upload's glGetError — same pattern as drawArrows).
    m_basicShader.bind();
    m_basicShader.setUniform1i("u_useTexture", 1);
    // Blending is enabled for the hand: the opaque pass runs with blending
    // off, but the icon texture carries transparent (alpha 0) pixels —
    // unblended they would draw as an opaque black square behind the icon
    // (translucent tiles such as glass cubes need it too).
    m_renderer.enableBlending();
    if (isBlock) {
        m_atlas.texture().bind();
        m_renderer.draw(itemMesh(it.block), m_basicShader, view, projection,
                        model);
    } else if (hasModel) {
        // CRAFT-2: the box-model hand — the per-item model texture bound
        // at draw time (the icon-hand's binding path), culling OFF for
        // the double-sided box (every face renders from any spin state),
        // the canonical unit geometry scaled by kHandIconScale in the
        // model matrix (the icon-quad footprint).
        m_itemModelTextures[iconIdx].bind();
        m_renderer.setCulling(false);
        m_renderer.draw(itemModelMesh(it.id), m_basicShader, view,
                        projection, model);
        m_renderer.setCulling(true);
        m_atlas.texture().bind(); // restore for the passes that follow
    } else {
        m_itemIcons[iconIdx].bind();
        m_renderer.draw(handIconMesh(), m_basicShader, view, projection,
                        model);
        m_atlas.texture().bind(); // restore for the passes that follow
    }
    m_renderer.disableBlending();
}

// ---------------------------------------------------------------------------
// Phase 15: UI keys, inventory, sounds, overlay and particles
// ---------------------------------------------------------------------------

void Game::handleUiKeys() {
    if (m_input.wasPressed(GameKey::F3)) {
        m_debugOverlay = !m_debugOverlay;
    }
    // CRAFT-7: the top-row digits select the hotbar slot directly
    // (Minecraft; the Tab cycle stays). In the GLOBAL key section
    // (handleUiKeys, not the locked-world section): it must work with a
    // panel open, as in Minecraft — the world section early-returns
    // while the pointer is free (panel open).
    for (int d = 0; d < 9; ++d) {
        if (m_input.wasPressed(static_cast<GameKey>(
                static_cast<int>(GameKey::Digit1) + d))) {
            m_selectedSlot = static_cast<std::size_t>(d);
        }
    }
    if (m_input.wasPressed(GameKey::F5)) {
        if (m_args.saveSlot.has_value()) {
            auto saved = writeSave();
            if (!saved.ok()) {
                MC_LOG_ERROR("F5 save failed: " + saved.error().describe());
            }
        } else {
            MC_LOG_WARN("F5: no save slot (pass --save <slot>)");
        }
    }
    if (m_input.wasPressed(GameKey::E)) {
        if (panelOpen()) {
            closePanel();
        } else if (m_window.relativeMouseMode()) {
            openPanel(OpenPanel::Inventory);
        }
    }
    if (panelOpen() && m_input.wasPressed(GameKey::Escape)) {
        closePanel();
    }
}

void Game::openPanel(OpenPanel panel) {
    m_window.setRelativeMouseMode(false); // free the cursor for clicking
    m_openPanel = panel;
    resetBreaking();
    resetBowDraw(); // a panel interrupts a draw (Phase 24)
    resetEatChannel(); // a panel interrupts an eat too (Phase 28)
    m_sound.play("ui_open");
}

void Game::closePanel() {
    m_openPanel = OpenPanel::None;
    m_sound.play("ui_close");
}

// Phase 44: the furnace panel opens for ONE block — bind the UI to that
// block's state (the setBlock invariant guarantees a furnace block owns
// one; furnaceAt's default-construct is defensive). The state is stable
// while the panel is open: block interaction is paused and no other code
// path mutates or removes a furnace block in the meantime.
void Game::openFurnacePanel(const BlockPos& pos) {
    m_openFurnacePos = pos;
    m_furnaceUi.bind(furnaceAt(pos));
    openPanel(OpenPanel::Furnace);
}

FurnaceState& Game::furnaceAt(const BlockPos& pos) {
    auto it = m_furnaces.find(pos);
    if (it == m_furnaces.end()) {
        it = m_furnaces.emplace(pos, FurnaceState {}).first;
    }
    return it->second;
}

// CRAFT-1: the table panel opens for ONE block — bind the UI to that
// block's grid (the setBlock invariant guarantees a table block owns
// one; tableAt's default-construct is defensive). The grid is stable
// while the panel is open: block interaction is paused and no other code
// path mutates or removes a table block in the meantime (the Phase 44
// furnace argument, the twin).
void Game::openTablePanel(const BlockPos& pos) {
    m_tableUi.bind(tableAt(pos));
    openPanel(OpenPanel::Table);
}

CraftingGrid& Game::tableAt(const BlockPos& pos) {
    auto it = m_tables.find(pos);
    if (it == m_tables.end()) {
        it = m_tables.emplace(pos, CraftingGrid {}).first;
    }
    return it->second;
}

CampfireCookState& Game::campfireAt(const BlockPos& pos) {
    auto it = m_campfires.find(pos);
    if (it == m_campfires.end()) {
        it = m_campfires.emplace(pos, CampfireCookState {}).first;
    }
    return it->second;
}

void Game::tryCampfireCook(const BlockPos& pos) {
    // The click is ALWAYS consumed (the caller returns) — a campfire is
    // never placed through this branch, whatever the hand holds.
    CampfireCookState& st = campfireAt(pos);
    const ItemStack& held = m_inventory.at(m_selectedSlot);
    // Phase 76: the relight — an ignites_flamable item (the flint and
    // steel) on a campfire-family block with an `ignites_to` target sets
    // the target in place (unlit -> lit: the cooking state survives the
    // setBlock choke point and resumes; light 0 -> 15, local re-light).
    // It runs HERE because this branch consumes the click BEFORE the
    // general igniter branch; the item is NOT consumed (the P60
    // semantics) and the "ignite" sound plays. An igniter on a LIT
    // campfire (no ignites_to) falls into the place/take logic below and
    // no-ops (it is not smeltable).
    if (!held.empty() && m_items.at(held.item).ignitesFlamable) {
        const Block& b = m_blocks.at(
            m_world.getBlock(pos.x, pos.y, pos.z));
        if (!b.properties.ignitesTo.empty()) {
            // The name is init-validated (the grows_to discipline), so
            // idForName cannot fail.
            const BlockID toId = m_blocks.idForName(b.properties.ignitesTo);
            setBlock(pos.x, pos.y, pos.z, toId);
            Lighting::updateLocal(m_world, m_blocks, pos);
            m_sound.play("ignite");
        }
        return;
    }
    // Phase 75: only a LIT campfire cooks — placing food on the unlit
    // logs does nothing (the fire must be there, Minecraft); the
    // take-back works on EITHER (the items are in the unlit campfire
    // too). There is no panel (v1): the slot choice is by index — first
    // empty (place) / first occupied (take), deterministic.
    const bool lit =
        m_world.getBlock(pos.x, pos.y, pos.z) == m_campfireLitId;
    if (!held.empty()) {
        const Item& it = m_items.at(held.item);
        if (lit && !it.smeltableTo.empty()) {
            for (std::size_t i = 0; i < kCampfireSlotCount; ++i) {
                if (st.slots[i].empty()) {
                    // Place one unit in the slot (Minecraft: a smeltable
                    // item on an empty campfire spot). One is consumed
                    // from the hand; the cooking starts at the next tick.
                    st.slots[i] = ItemStack {held.item, 1};
                    st.progressSubsteps[i] = 0;
                    m_inventory.take(m_selectedSlot, 1);
                    m_sound.play("place");
                    break;
                }
            }
            // All four slots full (or a non-smeltable / unlit target):
            // nothing happens (documented v1 simplification — no panel).
        }
        // The take needs a FREE hand (Minecraft).
        return;
    }
    // Empty hand: take the first occupied slot back (smelted or not) —
    // the selected slot is empty, so the write is a direct set.
    for (std::size_t i = 0; i < kCampfireSlotCount; ++i) {
        if (!st.slots[i].empty()) {
            m_inventory.setSlot(m_selectedSlot, st.slots[i]);
            st.slots[i] = ItemStack {};
            st.progressSubsteps[i] = 0;
            m_sound.play("pickup");
            break;
        }
    }
}

// The shapeless recipes in registry order: the E panel's craft list
// (shaped recipes are crafted in the grids, not from the list).
std::vector<const Recipe*> Game::shapelessRecipes() const {
    std::vector<const Recipe*> out;
    for (const Recipe& r : m_recipes.all()) {
        if (r.type == RecipeType::Shapeless) {
            out.push_back(&r);
        }
    }
    return out;
}

// CRAFT-4: the shaped recipes in registry order: the table panel's hint
// list (display-only — the player crafts them in the 3x3 grid following
// the mini pattern).
std::vector<const Recipe*> Game::shapedRecipes() const {
    std::vector<const Recipe*> out;
    for (const Recipe& r : m_recipes.all()) {
        if (r.type == RecipeType::Shaped) {
            out.push_back(&r);
        }
    }
    return out;
}

void Game::handlePanelClicks() {
    // The layout and the mouse hit tests both live in UI pixels (Phase
    // 34): InventoryLayout / FurnaceLayout are pure geometry, so passing
    // the UI space scales the panel identically to the rendered one.
    const UiSize space = uiSize();
    const int w = space.width;
    const int h = space.height;

    // CRAFT-7: the interaction dispatch — shift-click quick-moves, a
    // fast second left click on the same player-inventory slot collects.
    // The double-click window is REAL dt (the survival-timer class, hard
    // rule 8: input-only state — a no-input run never touches it), and
    // any non-slot click / other button / shift resets the pairing.
    const bool shift = m_input.isDown(GameKey::LeftShift);
    const bool leftPressed = m_input.wasPressed(MouseButton::Left);
    const bool rightPressed = m_input.wasPressed(MouseButton::Right);
    const double now = nowSeconds();
    auto isDoubleClick = [&](int slot) {
        return !shift && slot >= 0 && slot == m_lastLeftSlot &&
               m_lastLeftClickTime >= 0.0 &&
               (now - m_lastLeftClickTime) <= kDoubleClickSeconds;
    };
    auto armDoubleClick = [&](int slot) {
        m_lastLeftSlot = slot;
        m_lastLeftClickTime = slot >= 0 ? now : -1.0;
    };
    auto disarmDoubleClick = [&]() {
        m_lastLeftSlot = -1;
        m_lastLeftClickTime = -1.0;
    };

    // Phase 40: the furnace panel. Same click model as the crafting
    // panels: the layout hit tests are disjoint (at most one of a
    // 27-slot / input / fuel / output is active; a left click with all of
    // them inactive drops the carry). No craft action — the output fills
    // by itself.
    if (m_openPanel == OpenPanel::Furnace) {
        const FurnaceLayout layout = FurnaceLayout::compute(w, h);
        const std::pair<int, int> mp = uiMouse();
        const int slot = layout.slotAt(mp.first, mp.second);
        const int fslot = layout.furnaceSlotAt(mp.first, mp.second);
        if (leftPressed) {
            if (shift) {
                m_furnaceUi.quickMove(slot, fslot);
            } else if (fslot < 0 && isDoubleClick(slot)) {
                m_furnaceUi.doubleCollect(slot);
            } else {
                m_furnaceUi.leftClick(slot, fslot);
            }
            if (fslot < 0 && !shift) {
                armDoubleClick(slot);
            } else {
                disarmDoubleClick();
            }
            return;
        }
        if (rightPressed) {
            if (shift) {
                m_furnaceUi.quickMove(slot, fslot);
            } else {
                m_furnaceUi.rightClick(slot, fslot);
            }
            disarmDoubleClick();
        }
        return;
    }

    const bool table = m_openPanel == OpenPanel::Table;
    const PanelKind kind =
        table ? PanelKind::CraftingTable : PanelKind::Inventory;
    // The hint list per kind: shapeless craft rows (E panel) or shaped
    // display-only hint rows (table panel, CRAFT-4).
    std::vector<const Recipe*> hints =
        table ? shapedRecipes() : shapelessRecipes();
    const InventoryLayout layout =
        InventoryLayout::compute(w, h, static_cast<int>(hints.size()), kind);
    const std::pair<int, int> mp = uiMouse();
    const int px = mp.first;
    const int py = mp.second;
    InventoryUI& ui = table ? m_tableUi : m_inventoryUi;
    // The layout hit tests are disjoint: at most one of slot / grid cell /
    // output is active (the UI dispatch checks output first, then grid,
    // then slot; all-inactive drops the carry on a left click).
    const int cell = layout.gridCellAt(px, py);
    const int gridSlot =
        cell >= 0 ? layout.gridSlots[static_cast<std::size_t>(cell)] : -1;
    const bool onOutput = layout.inOutput(px, py);
    const int slot = layout.slotAt(px, py);
    if (leftPressed) {
        // Craft rows first (they sit beside the slots, so the hit test is
        // unambiguous): an E-panel row crafts when the inventory can
        // satisfy the recipe (all-or-nothing, see craft()). A table-panel
        // hint row (CRAFT-4) is display-only: the click is CONSUMED (no
        // craft, and no carry drop like a click outside every slot).
        const int row = layout.recipeRowAt(px, py);
        if (row >= 0) {
            if (table) {
                disarmDoubleClick();
                return;
            }
            const Recipe& recipe = *hints[static_cast<std::size_t>(row)];
            if (canCraft(m_inventory, recipe, m_items)) {
                auto crafted = craft(m_inventory, recipe, m_items);
                if (crafted.ok()) {
                    m_sound.play("craft");
                } else {
                    MC_LOG_ERROR("Craft failed: " + crafted.error().describe());
                }
            }
            disarmDoubleClick();
            return;
        }
        if (shift) {
            ui.quickMove(slot, gridSlot, onOutput);
        } else if (gridSlot < 0 && !onOutput && isDoubleClick(slot)) {
            ui.doubleCollect(slot);
        } else {
            if (ui.leftClick(slot, gridSlot, onOutput)) {
                m_sound.play("craft");
            }
        }
        if (gridSlot < 0 && !onOutput && !shift) {
            armDoubleClick(slot);
        } else {
            disarmDoubleClick();
        }
        return;
    }
    if (rightPressed) {
        if (shift) {
            ui.quickMove(slot, gridSlot, onOutput);
        } else {
            if (ui.rightClick(slot, gridSlot, onOutput)) {
                m_sound.play("craft");
            }
        }
        disarmDoubleClick();
    }
}

void Game::updateSoundTimers(double dt) {
    if (!m_player || !m_window.relativeMouseMode() || panelOpen()) {
        return;
    }
    const Vec3 v = m_player->velocity();
    const bool moving =
        std::abs(v.x) > kStepMinSpeed || std::abs(v.z) > kStepMinSpeed;
    if (moving && m_player->onGround()) {
        m_stepTimer += dt;
        if (m_stepTimer >= kStepSoundInterval) {
            m_stepTimer = 0.0;
            m_sound.play("step");
        }
    } else {
        m_stepTimer = 0.0;
    }
}

void Game::updateAutoSave(double dt) {
    // SAVE-1: the real-dt auto-save timer (hard rule 8 — the accumulator
    // is NOT frame state: never saved, never touches the deterministic
    // frame; it is the audio-timer class, the survival-timer rhythm).
    // The slot gate is structural: without --save the timer never arms.
    if (!m_args.saveSlot.has_value()) {
        return;
    }
    if (!autoSaveStep(m_autoSaveAccum, dt, m_autoSaveInterval)) {
        return;
    }
    auto saved = writeSave(true);
    if (saved.ok()) {
        m_autoSaveAccum = 0.0;
    } else {
        // A failed write keeps the accumulator armed (>= the interval):
        // the NEXT frame retries (no backoff — the documented
        // simplification; the save path's error is the signal).
        MC_LOG_ERROR("Auto-save failed: " + saved.error().describe());
    }
}

void Game::updateSurvival(double dt) {
    if (m_hurtFlash > 0.0) {
        m_hurtFlash = std::max(0.0, m_hurtFlash - dt);
    }
    const float hp = m_player->health();
    const bool dead = m_player->isDead();
    if (m_prevHealth >= 0.0f && hp < m_prevHealth) {
        // A health drop this frame: the red veil restarts and the hurt
        // sound plays (suppressed when the drop was lethal — the death
        // sound follows instead).
        m_hurtFlash = kHurtFlashSeconds;
        if (!dead) {
            m_sound.play("hurt");
        }
    }
    if (dead && m_prevHealth >= 0.0f && m_prevHealth > 0.0f) {
        // Alive -> dead transition: the death screen owns the input, so
        // release the pointer (it was locked while the fall happened).
        // Phase 46: the active status effects die with the player (the
        // respawn starts clean — Minecraft parity).
        m_window.setRelativeMouseMode(false);
        m_sound.play("death");
        // SURV-7: the death counter (the alive->dead transition — one
        // increment per life lost, whatever the cause; frame-locked).
        m_stats["deathCount"]++;
        m_playerEffects.clearAll();
        syncAbsorption(); // Phase 72: the pool dies with the player
        // SURV-2: the air refills with the death (the respawn
        // convergence point — the next life starts on the dry spawn
        // with full oxygen; while dead the pass is inert and the
        // state's decay is harmless).
        m_drown = DrownState {};
    }
    m_prevHealth = hp;
    if (dead && m_input.wasPressed(GameKey::R)) {
        m_player->respawn();
        m_prevHealth = m_player->health(); // no hurt flash on respawn
    }
}

Game::UiSize Game::uiSize() const {
    const double scale = static_cast<double>(m_uiScale);
    return UiSize {
        std::max(1, static_cast<int>(std::lround(
            static_cast<double>(m_window.logicalWidth()) / scale))),
        std::max(1, static_cast<int>(std::lround(
            static_cast<double>(m_window.logicalHeight()) / scale)))};
}

std::pair<int, int> Game::uiMouse() const {
    const Vec2 p = m_input.mousePosition();
    const double scale = static_cast<double>(m_uiScale);
    return {static_cast<int>(std::lround(p.x / scale)),
            static_cast<int>(std::lround(p.y / scale))};
}

Mat4 Game::uiProjection(int w, int h) const {
    // UI pixels (y = 0 is the window top), stretched onto the drawable by
    // the ortho range [0, w] x [0, h]. The depth range
    // must cover the 3D block-cube icons (the 76px held icon spans
    // |z| up to ~66px after its tilt); everything 2D sits at z = 0 and
    // the UI pass runs with the depth test off, so the wide range is
    // harmless.
    return Mat4::ortho(0.0f, static_cast<float>(w), static_cast<float>(h),
                       0.0f, -128.0f, 128.0f);
}

void Game::drawUiBatch(const std::vector<MeshVertex>& vertices,
                       const Mat4& projection, const Texture& texture,
                       bool textured, const Vec4& color) {
    if (vertices.empty()) {
        return;
    }
    Mesh mesh;
    auto up = mesh.upload(vertices);
    if (!up.ok()) {
        MC_LOG_ERROR("UI mesh upload failed: " + up.error().describe());
        return;
    }
    m_uiShader.bind();
    m_uiShader.setUniform1i("u_texture", 0);
    m_uiShader.setUniform1i("u_useTexture", textured ? 1 : 0);
    m_uiShader.setUniform4f("u_color", color.x, color.y, color.z, color.w);
    if (textured) {
        texture.bind();
    }
    m_renderer.draw(mesh, m_uiShader, Mat4::identity(), projection,
                    Mat4::identity());
}

void Game::drawWorldOverlay(const Mat4& view, const Mat4& projection) {
    // Only while playing (pointer captured, no inventory): the crosshair
    // aim is what the overlay describes. The crack is drawn over the
    // block being broken; the outline over whatever block the ray hits.
    // Both are world-space draws through the UI shader (unlit).
    const bool playing =
        m_window.relativeMouseMode() && !panelOpen();
    if (!playing) {
        return;
    }
    // The overlay is self-contained blending: the crack/outline are
    // alpha-composited over the block faces. The blend section's state
    // used to be assumed, but per-draw state leaks (10-26 user report:
    // a disableBlending from an earlier draw reached here and the crack
    // cube's (0,0,0,0) background painted the mined block solid black).
    m_renderer.enableBlending();
    if (m_breakTarget.has_value() && m_breakProgress > 0.0) {
        const BlockPos& b = *m_breakTarget;
        const Block& block =
            m_blocks.at(m_world.getBlock(b.x, b.y, b.z));
        const double duration = breakDurationSeconds(
            block.properties.hardness, selectedBreakSpeed(block));
        const int stage =
            duration > 0.0
                ? std::clamp(
                      static_cast<int>(std::floor(
                          (m_breakProgress / duration) * kCrackStages)),
                      0, kCrackStages - 1)
                : kCrackStages - 1;
        const Vec3 center {static_cast<float>(b.x) + 0.5f,
                           static_cast<float>(b.y) + 0.5f,
                           static_cast<float>(b.z) + 0.5f};
        m_uiShader.bind();
        m_uiShader.setUniform1i("u_texture", 0);
        m_uiShader.setUniform1i("u_useTexture", 1);
        m_uiShader.setUniform4f("u_color", 1.0f, 1.0f, 1.0f, 1.0f);
        m_crackTexture.bind();
        m_renderer.draw(m_crackMeshes[static_cast<std::size_t>(stage)],
                        m_uiShader, view, projection,
                        Mat4::translate(center));
    }
    if (const std::optional<BlockHit> hit = targetBlock()) {
        const Vec3 center {static_cast<float>(hit->block.x) + 0.5f,
                           static_cast<float>(hit->block.y) + 0.5f,
                           static_cast<float>(hit->block.z) + 0.5f};
        m_uiShader.bind();
        m_uiShader.setUniform1i("u_texture", 0);
        m_uiShader.setUniform1i("u_useTexture", 0);
        m_uiShader.setUniform4f("u_color", 0.0f, 0.0f, 0.0f, 0.85f);
        m_renderer.draw(m_outlineMesh, m_uiShader, view, projection,
                        Mat4::translate(center));
    }
    m_renderer.disableBlending();
}

void Game::drawParticles(const Mat4& view, const Mat4& projection,
                         RenderPass pass) {
    if (m_particles.empty()) {
        return;
    }
    // One dynamic batch per pass: all particles of matching blocks as a
    // single indexed mesh (a 0.12-block cube per particle, the broken
    // block's own faces). Light values were baked at spawn time.
    std::vector<MeshVertex> verts;
    std::vector<std::uint32_t> indices;
    const float half = 0.06f;
    for (const Particle& p : m_particles.particles()) {
        const Block& block = m_blocks.at(p.block);
        // PRE-5: the particle routes like its block — opaque blocks'
        // break particles stay in the opaque pass, `cutout` blocks'
        // (leaves, cross plants: the fire embers) join the cutout pass,
        // the remaining transparent blocks' the blend pass.
        const RenderPass want =
            !block.properties.transparent
                ? RenderPass::Opaque
                : (block.properties.cutout ? RenderPass::Cutout
                                           : RenderPass::Blend);
        if (pass != want) {
            continue;
        }
        for (const CubeFace& face : kCubeFaces) {
            // Cross blocks (Phase 45) carry no cube faces: the particle
            // cube uses the cross texture on every face (the same
            // single-lookup the item-mesh and chunk-mesher cross branches
            // use — cubeFaceTexture would return the empty top/side/bottom
            // name and the atlas lookup would throw). Fence / glass pane
            // (Phase 98/99) are the same rule for the thin side-only
            // shapes: their top/bottom names are empty by design.
            const std::string& tex =
                block.model.shape == BlockModel::Shape::Cross
                    ? block.model.textures.cross
                    : (block.model.shape == BlockModel::Shape::Fence ||
                       block.model.shape == BlockModel::Shape::GlassPane)
                          ? block.model.textures.side
                          : cubeFaceTexture(block, face.textureSlot);
            const Vec2 u0 = AtlasLayout::uvMin(m_atlas.layout(), tex);
            const Vec2 u1 = AtlasLayout::uvMax(m_atlas.layout(), tex);
            const Vec2 faceUv[4] = {
                {u0.x, u1.y}, {u1.x, u1.y}, {u1.x, u0.y}, {u0.x, u0.y},
            };
            const std::uint32_t first =
                static_cast<std::uint32_t>(verts.size());
            for (int i = 0; i < 4; ++i) {
                MeshVertex v;
                v.position = {
                    p.position.x +
                        (static_cast<float>(face.corner[i][0]) - 0.5f) * 2.0f * half,
                    p.position.y +
                        (static_cast<float>(face.corner[i][1]) - 0.5f) * 2.0f * half,
                    p.position.z +
                        (static_cast<float>(face.corner[i][2]) - 0.5f) * 2.0f * half};
                // Cross particles (the fire embers, Phase 66) take the
                // full-brightness face shade, like every other cross
                // rendering (the chunk-mesher and item-mesh cross
                // branches): the cube face normals would darken the
                // side faces to 0.6x and the embers would read as dim
                // specks against the bright flame body.
                v.normal =
                    block.model.shape == BlockModel::Shape::Cross
                        ? Vec3 {0.0f, 1.0f, 0.0f}
                        : face.normal;
                v.uv = faceUv[i];
                v.skyLight = p.skyLight;
                v.blockLight = p.blockLight;
                verts.push_back(v);
            }
            indices.insert(indices.end(),
                           {first, first + 1, first + 2, first, first + 2,
                            first + 3});
        }
    }
    if (verts.empty()) {
        return;
    }
    Mesh mesh;
    auto up = mesh.upload(verts, indices);
    if (!up.ok()) {
        MC_LOG_ERROR("Particle mesh upload failed: " + up.error().describe());
        return;
    }
    m_renderer.draw(mesh, m_basicShader, view, projection, Mat4::identity());
}

// ---------------------------------------------------------------------------
// Phase 15: the screen-space UI pass
// ---------------------------------------------------------------------------

void Game::renderUi() {
    // All UI geometry lives in the UI-pixel space (logical window size /
    // ui.scale, Phase 34): uiProjection stretches it onto the drawable,
    // so a larger ui.scale upsamples every element (font, hotbar, panels,
    // F3, death screen) via the GPU. At dppx 1 with ui.scale 1.0 the
    // space is exactly the drawable — the no-input baseline is unchanged.
    const UiSize space = uiSize();
    const int w = space.width;
    const int h = space.height;
    const bool locked = m_window.relativeMouseMode();
    const Mat4 ortho = uiProjection(w, h);

    // Palette (Minecraft-like: dark translucent panels, gray borders, the
    // selected slot in orange).
    const Vec4 kSlotBg {0.0f, 0.0f, 0.0f, 0.45f};
    const Vec4 kBorder {1.0f, 1.0f, 1.0f, 0.30f};
    const Vec4 kSelected {1.0f, 0.82f, 0.0f, 1.0f};
    const Vec4 kHover {1.0f, 1.0f, 1.0f, 0.85f};
    const Vec4 kPanel {0.08f, 0.08f, 0.11f, 0.88f};
    const Vec4 kText {1.0f, 1.0f, 1.0f, 0.92f};
    const Vec4 kTextDim {1.0f, 1.0f, 1.0f, 0.55f};
    const Vec4 kCrosshair {1.0f, 1.0f, 1.0f, 0.85f};
    // SURV-3 (durability bar): the strip drawn under a DAMAGED item
    // icon — 2 px tall at the icon's bottom edge, filled left-to-right
    // with the REMAINING fraction (1 − damage/durability) in three
    // tiers (green >= 1/2, yellow >= 1/4, red < 1/4 — the documented
    // simplification of Minecraft's continuous gradient). Damage 0
    // (full durability) draws NOTHING (the no-input baseline's
    // structural immunity: a fresh run carries full-durability items).
    constexpr int kDurabilityBarPx = 2;
    constexpr float kDurabilityGreenTier = 0.5f;
    constexpr float kDurabilityYellowTier = 0.25f;
    const Vec4 kDurabilityGreen {0.16f, 0.69f, 0.13f, 1.0f};
    const Vec4 kDurabilityYellow {0.93f, 0.78f, 0.12f, 1.0f};
    const Vec4 kDurabilityRed {0.78f, 0.12f, 0.12f, 1.0f};

    std::vector<UiBatch> batches;
    auto batchFor = [&](const Vec4& color, bool textured,
                        const Texture* tex) -> UiBatch& {
        for (auto& b : batches) {
            if (b.textured == textured && b.texture == tex &&
                b.color.x == color.x && b.color.y == color.y &&
                b.color.z == color.z && b.color.w == color.w) {
                return b;
            }
        }
        UiBatch b;
        b.color = color;
        b.textured = textured;
        b.texture = tex;
        batches.push_back(std::move(b));
        return batches.back();
    };
    // A screen-space quad with an explicit UV rect (image space, v-down:
    // v=0 is the texture's TOP row — textures upload without a flip).
    // Textured batches (item icons, hearts) sample their rect; untextured
    // batches ignore the UVs (u_useTexture=0).
    auto addQuadUv = [&batches](UiBatch& b, int x, int y, int qw, int qh,
                                 const Vec2& uvMin, const Vec2& uvMax) {
        const float x0 = static_cast<float>(x);
        const float y0 = static_cast<float>(y);
        const float x1 = x0 + static_cast<float>(qw);
        const float y1 = y0 + static_cast<float>(qh);
        const auto put = [&](float px, float py, float u, float pv) {
            MeshVertex vert;
            vert.position = {px, py, 0.0f};
            vert.uv = {u, pv};
            b.vertices.push_back(vert);
        };
        put(x0, y0, uvMin.x, uvMin.y);
        put(x1, y0, uvMax.x, uvMin.y);
        put(x1, y1, uvMax.x, uvMax.y);
        put(x0, y0, uvMin.x, uvMin.y);
        put(x1, y1, uvMax.x, uvMax.y);
        put(x0, y1, uvMin.x, uvMax.y);
    };
    auto addQuad = [&batches, &addQuadUv](UiBatch& b, int x, int y, int qw,
                                          int qh) {
        // Full-rect UVs: a single-tile textured quad (item icons) samples
        // the whole texture. (P15 bug fix: the old code left the UVs at
        // (0, 0) and the icon rendered as its top-left pixel.)
        addQuadUv(b, x, y, qw, qh, Vec2 {0.0f, 0.0f}, Vec2 {1.0f, 1.0f});
    };
    auto addBorder = [&](UiBatch& b, const UIRect& r, int thickness) {
        addQuad(b, r.x, r.y, r.w, thickness);
        addQuad(b, r.x, r.y + r.h - thickness, r.w, thickness);
        addQuad(b, r.x, r.y, thickness, r.h);
        addQuad(b, r.x + r.w - thickness, r.y, thickness, r.h);
    };
    auto addText = [&](const std::string& text, int x, int y, double scale,
                       const Vec4& color) {
        makeTextVertices(text, x, y, scale,
                         batchFor(color, true, &m_fontTexture).vertices);
    };

    // PRE-12 (tooltip): the hover tooltip lines are collected into
    // separate batches drawn AFTER the 2D batches and the 3D block-cube
    // icons. A cursor-adjacent tooltip sits over slot rects/icons; the
    // shared kText batch (inserted at the panel title, before the slot
    // backgrounds) would draw UNDER them and vanish at the panel's fit
    // scale (Phase 15's "only the name" tooltip was effectively hidden
    // for the same reason).
    std::vector<UiBatch> tooltipBatches;
    auto appendTooltip = [&](const ItemTooltip& tip, int x, int y,
                             double s, int lineGap) {
        UiBatch title;
        title.color = kText;
        title.textured = true;
        title.texture = &m_fontTexture;
        makeTextVertices(tip.title, x, y, s, title.vertices);
        tooltipBatches.push_back(std::move(title));
        if (!tip.durability.empty()) {
            UiBatch dur;
            dur.color = kTextDim;
            dur.textured = true;
            dur.texture = &m_fontTexture;
            makeTextVertices(tip.durability, x, y + lineGap, s, dur.vertices);
            tooltipBatches.push_back(std::move(dur));
        }
    };

    // Fluid overlay (Phase 99.5): when the camera's cell holds a fluid,
    // the fluid geometry around the eye self-culls (same-kind faces hide
    // each other — a level-8 cell in a deep pool has no faces at all), so
    // a fully submerged player saw "air" (the 99.x user report). A flat
    // translucent tint under the HUD — Minecraft's fluid overlay —
    // restores the submerged read. This is the FIRST batch (drawn first =
    // under every HUD element). No-op out of fluid (the no-input baseline
    // camera is in air: byte-identical).
    {
        const Vec3 eye = m_camera.position;
        const std::optional<FluidKind> submerged = m_world.getFluidKind(
            static_cast<int>(std::floor(eye.x)),
            static_cast<int>(std::floor(eye.y)),
            static_cast<int>(std::floor(eye.z)));
        if (submerged) {
            // Visual tuning constants (overlay strength + hue): water ≈
            // MC's blue depth tint, lava ≈ its orange glow.
            const Vec4 tint = *submerged == FluidKind::Water
                ? (Vec4 {0.20f, 0.42f, 0.85f, 0.45f})
                : (Vec4 {0.95f, 0.42f, 0.05f, 0.60f});
            auto& b = batchFor(tint, false, nullptr);
            addQuad(b, 0, 0, w, h);
        }
    }

    // Item icons: 2D textures (tools, materials) are batched quads like
    // everything else; block items render a lit 3D cube, which is
    // RECORDED and drawn only after all 2D batches — the slot backgrounds
    // are 2D batches, and drawing the cubes first would put them under
    // the backgrounds.
    struct UiIconDraw {
        ItemStack stack;
        float cx, cy;
        int sizePx;
    };
    std::vector<UiIconDraw> iconDraws;
    auto drawIcon = [&](const ItemStack& stack, float cx, float cy,
                        int sizePx) {
        if (stack.empty()) {
            return;
        }
        const Item& item = m_items.at(stack.item);
        if (item.type == ItemType::Block) {
            if (itemMesh(item.block).valid()) {
                iconDraws.push_back(UiIconDraw {stack, cx, cy, sizePx});
            }
            return;
        }
        const std::size_t idx = static_cast<std::size_t>(stack.item.value);
        if (idx >= m_itemIcons.size() || !m_itemIcons[idx].valid()) {
            return;
        }
        UiBatch& b = batchFor(Vec4 {1.0f, 1.0f, 1.0f, 1.0f}, true,
                              &m_itemIcons[idx]);
        addQuad(b, static_cast<int>(cx) - sizePx / 2,
                static_cast<int>(cy) - sizePx / 2, sizePx, sizePx);
        // SURV-3: the durability bar (the 2D-icon path only — block
        // items are not in the durability's allowed set, so the 3D-cube
        // path never carries one).
        if (stack.damage > 0.0f && item.durability > 0.0f) {
            const float remaining = 1.0f - stack.damage / item.durability;
            const Vec4 bar =
                remaining >= kDurabilityGreenTier
                    ? kDurabilityGreen
                    : (remaining >= kDurabilityYellowTier
                           ? kDurabilityYellow : kDurabilityRed);
            const int barW =
                static_cast<int>(std::lround(sizePx * remaining));
            if (barW > 0) {
                auto& bb = batchFor(bar, false, nullptr);
                addQuad(bb, static_cast<int>(cx) - sizePx / 2,
                        static_cast<int>(cy) + sizePx / 2 -
                            kDurabilityBarPx,
                        barW, kDurabilityBarPx);
            }
        }
    };

    const std::pair<int, int> mouse = uiMouse();
    const int mx = mouse.first;
    const int my = mouse.second;

    // -- hotbar + crosshair + held item (while captured, no panel open) --
    if (locked && !panelOpen()) {
        const int n = Inventory::kHotbarSize;
        const int total = n * kHotbarSlotSize + (n - 1) * kHotbarSlotGap;
        const int x0 = (w - total) / 2;
        const int y0 = h - kHotbarBottomMargin - kHotbarSlotSize;
        for (int i = 0; i < n; ++i) {
            const int sx = x0 + i * (kHotbarSlotSize + kHotbarSlotGap);
            const UIRect slot {sx, y0, kHotbarSlotSize, kHotbarSlotSize};
            addQuad(batchFor(kSlotBg, false, nullptr), slot.x, slot.y,
                    slot.w, slot.h);
            if (static_cast<std::size_t>(i) == m_selectedSlot) {
                addBorder(batchFor(kSelected, false, nullptr), slot, 2);
            } else {
                addBorder(batchFor(kBorder, false, nullptr), slot, 1);
            }
            const ItemStack& stack = m_inventory.at(i);
            drawIcon(stack, static_cast<float>(sx + kHotbarSlotSize / 2),
                     static_cast<float>(y0 + kHotbarSlotSize / 2),
                     kHotbarIconPx);
            if (!stack.empty() &&
                stack.count < m_items.at(stack.item).maxStack) {
                const std::string count = std::to_string(stack.count);
                addText(count,
                        sx + kHotbarSlotSize - 4 -
                            static_cast<int>(textWidth(count)),
                        y0 + kHotbarSlotSize - 4 - kGlyphSize, 1.0, kText);
            }
        }
        // Health bar (Phase 16) + hunger bar (Phase 23): hearts above the
        // hotbar, 2 points per icon, three-state tiles (full / half /
        // empty); the hunger strip (drumsticks, same tiles) sits on the
        // SAME row to the right of the hearts. Both strips share the icon
        // metrics; the icon count follows the configured max (the default
        // 20 = the 10-icon Phase 16/23 layout, bit-identical).
        {
            // The UI icon metrics come from the single source of truth
            // (core/ui_layout.hpp — the pure layout is unit-test-owned).
            constexpr int kIconPx = kUiIconPx;
            constexpr int kIconGap = kUiIconGap;
            constexpr int kStripGap = kUiStripGap;
            const float baseMaxHealth = m_player->params().maxHealth;
            const int heartIcons =
                static_cast<int>(std::ceil(baseMaxHealth / 2.0f));
            const int hungerIcons =
                static_cast<int>(std::ceil(m_player->maxHunger() / 2.0f));
            const int oxygen = m_drown.oxygen;
            const float absorbPool = m_player->absorption();
            const int absIcons = absorbPool > 0.0f
                ? static_cast<int>(std::ceil(absorbPool / 2.0f))
                : 0;
            const int absW =
                absIcons > 0
                    ? absIcons * kIconPx + (absIcons - 1) * kIconGap
                    : 0;
            // The heart row (Phase 16/23; PRE-11: the narrow-window
            // fallback): the pure heartRowLayout — the right strip is
            // the oxygen bar while the air is below full (it REPLACES
            // the hunger strip at its position, SURV-2), so the layout
            // reserves the strip that renders THIS frame. Legacy
            // centered when the row fits (bit-exact); shifted left
            // until the right strip fits otherwise.
            const int rightIcons =
                oxygen < kOxygenMax ? kOxygenBarIconCount : hungerIcons;
            const HeartRowLayout heartRow =
                heartRowLayout(w, heartIcons, absIcons, rightIcons);
            const int hx0 = heartRow.heartsX;
            const int gx0 = heartRow.rightX;
            const int hy = y0 - kIconPx - kStripGap;
            // The absorption pool (Phase 72, the golden apple): extra
            // RED hearts just LEFT of the normal strip (the Minecraft
            // layout). The strip shows the POOL itself — damage empties
            // it before any normal heart is (the two-component model).
            // Pool 0 = nothing drawn (the no-input baseline is
            // bit-identical: a fresh run has no effects, hence no pool).
            // One pass per strip: tile t occupies u in [t/3, (t+1)/3].
            auto drawStrip = [&](int x0, int icons, const Texture& tex,
                                 float value, const Vec4& tint) {
                UiBatch& batch = batchFor(tint, true, &tex);
                for (int i = 0; i < icons; ++i) {
                    // Icon i covers the interval [2i, 2i+2); three states
                    // (a sub-half fill renders as half — see docs/KNOWN_ISSUES).
                    const float fill =
                        std::clamp(value - 2.0f * i, 0.0f, 2.0f);
                    const int tile =
                        fill >= 1.99f ? 0 : (fill >= 0.99f ? 1 : 2);
                    addQuadUv(batch, x0 + i * (kIconPx + kIconGap), hy,
                              kIconPx, kIconPx,
                              Vec2 {static_cast<float>(tile) / 3.0f, 0.0f},
                              Vec2 {static_cast<float>(tile + 1) / 3.0f,
                                    1.0f});
                }
            };
            if (absIcons > 0) {
                // The pool strip: red-tinted hearts, value = the pool
                // (0..the effects' total; a drained pool draws no strip).
                drawStrip(hx0 - kStripGap - absW, absIcons, m_healthTexture,
                          absorbPool, Vec4 {1.0f, 0.35f, 0.35f, 1.0f});
            }
            drawStrip(hx0, heartIcons, m_healthTexture, m_player->health(),
                      Vec4 {1.0f, 1.0f, 1.0f, 1.0f});
            // SURV-2: the oxygen bubble row REPLACES the hunger row at
            // its position (gx0, hy — the same row, right of the hearts)
            // while the air is below full (Minecraft: the hunger bar
            // hides while the air bar shows; out of the fluid the air
            // refills instantly = the row disappears the same frame).
            // Single-tile texture, no half-state: icon i is drawn while
            // oxygen > i * kOxygenPerIcon, so the fill count is
            // ceil(oxygen / kOxygenPerIcon) of kOxygenBarIconCount.
            // (oxygen is computed above — the layout reserves the strip
            // that renders this frame.)
            if (oxygen < kOxygenMax) {
                UiBatch& batch =
                    batchFor(Vec4 {1.0f, 1.0f, 1.0f, 1.0f}, true,
                             &m_oxygenTexture);
                for (int i = 0; i < kOxygenBarIconCount; ++i) {
                    if (oxygen > i * kOxygenPerIcon) {
                        addQuadUv(batch, gx0 + i * (kIconPx + kIconGap), hy,
                                  kIconPx, kIconPx, Vec2 {0.0f, 0.0f},
                                  Vec2 {1.0f, 1.0f});
                    }
                }
            } else {
                drawStrip(gx0, hungerIcons, m_hungerTexture,
                          m_player->hunger(), Vec4 {1.0f, 1.0f, 1.0f, 1.0f});
            }
            // Status effect icons (Phase 46; PRE-10: the dedicated row):
            // one 16 px effects.png tile per ACTIVE effect in application
            // order (tile t occupies u in [t/N, (t+1)/N]; the width gives
            // N). PRE-10: the icons no longer share the heart row — a
            // long icon row (>= 5 effects) drew OVER the absorption strip
            // (the Phase-56 KNOWN_ISSUES overlap, triggerable with shipped
            // content: golden apple + spider + beacon + ...). The
            // Minecraft stack is three separate rows: hearts (hy),
            // effects (the row above), armor (the row above that). The
            // effect row is left-aligned on the heart row's LEFT edge —
            // the absorption strip's left edge when the pool is up, the
            // plain hearts' left edge otherwise. No effects = nothing
            // drawn (the no-input baseline is untouched).
            const int effX0 = heartRow.rowLeft;
            const int ey = hy - kIconPx - kStripGap;
            if (m_playerEffects.count() > 0) {
                const int n = static_cast<int>(
                    m_effectTexture.width() / kIconPx);
                UiBatch& batch =
                    batchFor(Vec4 {1.0f, 1.0f, 1.0f, 1.0f}, true,
                             &m_effectTexture);
                int ex = effX0;
                for (const ActiveEffect& a : m_playerEffects.list()) {
                    const Effect& def = m_effects.at(a.effect);
                    addQuadUv(batch, ex, ey, kIconPx, kIconPx,
                              Vec2 {static_cast<float>(def.icon) / n, 0.0f},
                              Vec2 {static_cast<float>(def.icon + 1) / n,
                                    1.0f});
                    ex += kIconPx + kIconGap;
                }
            }
            // Armor row (SURV-1; PRE-10: the top row of the three-row
            // stack): the four equipment slots ABOVE the effect row
            // (hy − 2×(16 + kStripGap)) — every HUD row is to itself, so
            // no icon count can collide with another strip. Fixed
            // positions head -> feet left to right, left-aligned on the
            // hearts' left edge hx0, one 16 px per-item icon (the
            // independent PNG through the shared drawIcon path) per
            // OCCUPIED slot only. No armor = nothing drawn (the
            // no-input baseline is bit-identical: a fresh run wears
            // nothing).
            const int ay = hy - 2 * (kIconPx + kStripGap);
            for (int slot = 0; slot < kArmorSlotCount; ++slot) {
                const ItemStack& worn = m_player->armorSlot(slot);
                if (worn.empty()) {
                    continue;
                }
                const int ax = hx0 + slot * (kIconPx + kIconGap);
                drawIcon(worn, static_cast<float>(ax + kIconPx / 2),
                         static_cast<float>(ay + kIconPx / 2), kIconPx);
            }
        }
        // Crosshair: a 2px thin cross at the screen center.
        UiBatch& cross = batchFor(kCrosshair, false, nullptr);
        addQuad(cross, w / 2 - 5, h / 2 - 1, 11, 2);
        addQuad(cross, w / 2 - 1, h / 2 - 5, 2, 11);
        // Held item: a large icon in the bottom-right corner. Phase 42:
        // the icon stays put — the draw/eat animation is the 3D hand
        // (drawHeldHand), which pulls back / rises with the charge (the
        // Phase 24/28 2D icon pull is gone).
        const float heldX = static_cast<float>(w - kHeldIconOffset);
        const float heldY = static_cast<float>(h - kHeldIconOffset);
        drawIcon(m_inventory.at(m_selectedSlot), heldX, heldY, kHeldIconPx);
    }

    // -- death screen (Phase 16, while the player is dead) ------------------
    // The player physics is frozen; the pointer was released on death, so
    // R (any pointer state) respawns.
    if (m_player && m_player->isDead()) {
        addQuad(batchFor(Vec4 {0.0f, 0.0f, 0.0f, 0.55f}, false, nullptr), 0,
                0, w, h);
        // PRE-1: the user-visible strings are localized to Chinese
        // (the CJK glyphs live in the font atlas rows 0..3); the F3
        // debug overlay stays English (a developer tool).
        const std::string died = "你死了！";
        const std::string hint = "按 R 重生";
        // 16 px glyphs (Phase 35): "died" at scale 2 = 32 px tall, hint
        // 16 px — the block spans h/2-40 .. h/2+28, roughly centered.
        addText(died, w / 2 - static_cast<int>(textWidth(died, 2.0)) / 2,
                h / 2 - 40, 2.0, Vec4 {1.0f, 0.35f, 0.35f, 1.0f});
        addText(hint, w / 2 - static_cast<int>(textWidth(hint)) / 2,
                h / 2 + 12, 1.0, kText);
    }

    // -- furnace panel (Phase 40/44): the smelting GUI (right-clicked
    // furnace) — rendering the ONE furnace block the panel is bound to
    // (its per-block state). The 27 inventory slots + the smelting
    // section (input, progress bar, output, fuel). No crafting of any
    // kind happens here — the output fills by itself (the furnace tick).
    if (m_openPanel == OpenPanel::Furnace) {
        const FurnaceState& f = furnaceAt(m_openFurnacePos);
        const FurnaceLayout layout = FurnaceLayout::compute(w, h);
        const double s = layout.scale;
        const auto L = [s](int v) {
            return static_cast<int>(std::lround(v * s));
        };
        const int slotIcon = L(34); // icons inside slot-sized rects
        const int titleOffset = L(4); // 24 px title text in the 40 px row

        // Panel background + title.
        addQuad(batchFor(kPanel, false, nullptr), layout.panel.x,
                layout.panel.y, layout.panel.w, layout.panel.h);
        addText("熔炉", layout.title.x, layout.title.y + titleOffset,
                InventoryLayout::kTitleFontScale * s, kText);

        const int hoverSlot = layout.slotAt(mx, my);
        const int hoverFslot = layout.furnaceSlotAt(mx, my);

        // The 27 inventory slots (drawn like the crafting panels').
        for (std::size_t i = 0; i < layout.slots.size(); ++i) {
            const UIRect& r = layout.slots[i];
            addQuad(batchFor(kSlotBg, false, nullptr), r.x, r.y, r.w, r.h);
            if (i == m_selectedSlot) {
                addBorder(batchFor(kSelected, false, nullptr), r, 2);
            } else if (static_cast<int>(i) == hoverSlot) {
                addBorder(batchFor(kHover, false, nullptr), r, 2);
            } else {
                addBorder(batchFor(kBorder, false, nullptr), r, 1);
            }
            const ItemStack& stack = m_inventory.at(i);
            drawIcon(stack, static_cast<float>(r.x + r.w / 2),
                     static_cast<float>(r.y + r.h / 2), slotIcon);
            if (!stack.empty() &&
                stack.count < m_items.at(stack.item).maxStack) {
                const std::string count = std::to_string(stack.count);
                addText(count,
                        r.x + r.w - L(4) - static_cast<int>(textWidth(count, s)),
                        r.y + r.h - L(4) - L(kGlyphSize), s, kText);
            }
        }

        // The smelting section slots (0=input, 1=fuel, 2=output).
        const UIRect* frects[3] = {&layout.input, &layout.fuel,
                                   &layout.output};
        const ItemStack* fslots[3] = {&f.input, &f.fuel,
                                      &f.output};
        for (int i = 0; i < 3; ++i) {
            const UIRect& r = *frects[i];
            addQuad(batchFor(kSlotBg, false, nullptr), r.x, r.y, r.w, r.h);
            if (i == hoverFslot) {
                addBorder(batchFor(kHover, false, nullptr), r, 2);
            } else {
                addBorder(batchFor(kBorder, false, nullptr), r, 1);
            }
            const ItemStack& stack = *fslots[i];
            drawIcon(stack, static_cast<float>(r.x + r.w / 2),
                     static_cast<float>(r.y + r.h / 2), slotIcon);
            if (!stack.empty() &&
                stack.count < m_items.at(stack.item).maxStack) {
                const std::string count = std::to_string(stack.count);
                addText(count,
                        r.x + r.w - L(4) - static_cast<int>(textWidth(count, s)),
                        r.y + r.h - L(4) - L(kGlyphSize), s, kText);
            }
        }

        // The progress bar (Minecraft's arrow): a dark track + a fill
        // proportional to the current smelt progress (0 when the input is
        // empty / not smeltable — the tick keeps it that way).
        {
            const UIRect& r = layout.progress;
            addQuad(batchFor(kSlotBg, false, nullptr), r.x, r.y, r.w, r.h);
            addBorder(batchFor(kBorder, false, nullptr), r, 1);
            double frac = 0.0;
            if (!f.input.empty() &&
                !m_items.at(f.input.item).smeltableTo.empty()) {
                const int total =
                    smeltSubstepsFor(m_items.at(f.input.item));
                if (total > 0) {
                    frac = static_cast<double>(f.progressSubsteps) /
                           static_cast<double>(total);
                }
            }
            const int fill = static_cast<int>(std::lround(r.w * frac));
            if (fill > 0) {
                addQuad(batchFor(kText, false, nullptr), r.x, r.y, fill,
                        r.h);
            }
        }

        // The hovered slot's item name + the carried stack at the cursor +
        // the close hint.
        if (hoverSlot >= 0) {
            const ItemStack& stack = m_inventory.at(hoverSlot);
            if (!stack.empty()) {
                // PRE-12 (tooltip): name + " xN" (count > 1) + the
                // d/max durability readout line (the pure
                // ui/item_tooltip; the Phase-15 "only the name" residue).
                appendTooltip(itemTooltip(m_items.at(stack.item), stack),
                              mx + L(12), my + L(12), s, L(16));
            }
        }
        if (m_furnaceUi.carrying()) {
            drawIcon(m_furnaceUi.carry(), static_cast<float>(mx),
                     static_cast<float>(my), slotIcon);
        }
        const std::string hint = "按 E 关闭"; // PRE-1
        addText(hint,
                layout.panel.x + static_cast<int>(std::lround(
                    (layout.panel.w - textWidth(hint, s)) / 2.0)),
                layout.panel.y + layout.panel.h - L(20), s, kTextDim);
    }

    // -- crafting panel (while open): the E inventory (2x2 grid + the
    // shapeless recipe craft list) or the crafting table (3x3 grid + the
    // shaped recipe hint list, CRAFT-4). ------------------------------------
    if (panelOpen() && m_openPanel != OpenPanel::Furnace) {
        const bool table = m_openPanel == OpenPanel::Table;
        const PanelKind kind =
            table ? PanelKind::CraftingTable : PanelKind::Inventory;
        // The hint list per kind (same source handlePanelClicks uses).
        std::vector<const Recipe*> hints =
            table ? shapedRecipes() : shapelessRecipes();
        const InventoryLayout layout =
            InventoryLayout::compute(w, h, static_cast<int>(hints.size()),
                                     kind);
        InventoryUI& ui = table ? m_tableUi : m_inventoryUi;

        // Panel fit factor (Phase 35): below the native width the whole
        // panel scales down; the text (font scale) and icons (quad sizes)
        // follow the SAME factor so everything stays proportional.
        const double s = layout.scale;
        const auto L = [s](int v) {
            return static_cast<int>(std::lround(v * s));
        };
        const int slotIcon = L(34); // icons inside slot-sized rects
        const int rowIcon = L(28);  // icons inside the recipe rows
        const int titleOffset = L(4); // 24 px title text in the 40 px row

        // Panel background + titles.
        addQuad(batchFor(kPanel, false, nullptr), layout.panel.x,
                layout.panel.y, layout.panel.w, layout.panel.h);
        addText(table ? "工作台" : "背包", layout.title.x,
                layout.title.y + titleOffset,
                InventoryLayout::kTitleFontScale * s, kText);
        // The list header ("合成") now serves both panels (CRAFT-4: the
        // table panel's hint column).
        addText("合成", layout.craftHeader.x,
                layout.craftHeader.y + titleOffset,
                InventoryLayout::kTitleFontScale * s, kText);

        const int hoverSlot = layout.slotAt(mx, my);
        const int hoverCell = layout.gridCellAt(mx, my);
        const bool hoverOutput = layout.inOutput(mx, my);
        for (std::size_t i = 0; i < layout.slots.size(); ++i) {
            const UIRect& r = layout.slots[i];
            addQuad(batchFor(kSlotBg, false, nullptr), r.x, r.y, r.w, r.h);
            if (i == m_selectedSlot) {
                addBorder(batchFor(kSelected, false, nullptr), r, 2);
            } else if (static_cast<int>(i) == hoverSlot) {
                addBorder(batchFor(kHover, false, nullptr), r, 2);
            } else {
                addBorder(batchFor(kBorder, false, nullptr), r, 1);
            }
            const ItemStack& stack = m_inventory.at(i);
            drawIcon(stack, static_cast<float>(r.x + r.w / 2),
                     static_cast<float>(r.y + r.h / 2), slotIcon);
            if (!stack.empty() &&
                stack.count < m_items.at(stack.item).maxStack) {
                const std::string count = std::to_string(stack.count);
                addText(count,
                        r.x + r.w - L(4) - static_cast<int>(textWidth(count, s)),
                        r.y + r.h - L(4) - L(kGlyphSize), s, kText);
            }
        }

        // Crafting grid cells + the output slot (the matched recipe's
        // result — shaped first, then shapeless; see matchGridRecipe).
        for (std::size_t i = 0; i < layout.gridCells.size(); ++i) {
            const UIRect& r = layout.gridCells[i];
            addQuad(batchFor(kSlotBg, false, nullptr), r.x, r.y, r.w, r.h);
            if (static_cast<int>(i) == hoverCell) {
                addBorder(batchFor(kHover, false, nullptr), r, 2);
            } else {
                addBorder(batchFor(kBorder, false, nullptr), r, 1);
            }
            const ItemStack& stack = ui.grid().at(layout.gridSlots[i]);
            drawIcon(stack, static_cast<float>(r.x + r.w / 2),
                     static_cast<float>(r.y + r.h / 2), slotIcon);
            if (!stack.empty() &&
                stack.count < m_items.at(stack.item).maxStack) {
                const std::string count = std::to_string(stack.count);
                addText(count,
                        r.x + r.w - L(4) - static_cast<int>(textWidth(count, s)),
                        r.y + r.h - L(4) - L(kGlyphSize), s, kText);
            }
        }
        {
            const UIRect& r = layout.output;
            addQuad(batchFor(kSlotBg, false, nullptr), r.x, r.y, r.w, r.h);
            if (hoverOutput) {
                addBorder(batchFor(kHover, false, nullptr), r, 2);
            } else {
                addBorder(batchFor(kBorder, false, nullptr), r, 1);
            }
            const GridMatch m = ui.match();
            if (m.matched()) {
                const ItemStack out {m.recipe->output,
                                     m.recipe->outputCount};
                drawIcon(out, static_cast<float>(r.x + r.w / 2),
                         static_cast<float>(r.y + r.h / 2), slotIcon);
                if (out.count < m_items.at(out.item).maxStack) {
                    const std::string count = std::to_string(out.count);
                    addText(count,
                            r.x + r.w - L(4) -
                                static_cast<int>(textWidth(count, s)),
                            r.y + r.h - L(4) - L(kGlyphSize), s, kText);
                }
            }
        }

        // Crafting rows: the E panel's shapeless CRAFT rows (output icon +
        // "name xN" white when craftable, dim otherwise + the ingredient
        // list) and the table panel's shaped HINT rows (CRAFT-4: the mini
        // pattern + output icon + "name xN", white when the inventory can
        // supply the pattern, dim otherwise — display-only, the click is
        // consumed in handlePanelClicks).
        if (table) {
            const int pcell = L(14); // the mini pattern's cell
            const int pgap = L(2);    // between mini pattern cells
            for (std::size_t i = 0; i < layout.recipes.size(); ++i) {
                const Recipe& recipe = *hints[static_cast<std::size_t>(i)];
                const UIRect& r = layout.recipes[i];
                const bool can = canSupply(m_inventory, patternBag(recipe));
                // The 3x3 mini pattern, top-left aligned exactly as defined
                // (a valid placement the grid match accepts): dark cell
                // background + the keyed item's icon; a space = empty cell.
                for (int pr = 0; pr < CraftingGrid::kColumns; ++pr) {
                    for (int pc = 0; pc < CraftingGrid::kColumns; ++pc) {
                        const int cx = r.x + L(3) + pc * (pcell + pgap);
                        const int cy = r.y + L(3) + pr * (pcell + pgap);
                        addQuad(batchFor(kSlotBg, false, nullptr), cx, cy,
                                pcell, pcell);
                        const std::string* row =
                            pr < static_cast<int>(recipe.pattern.size())
                                ? &recipe.pattern[static_cast<std::size_t>(pr)]
                                : nullptr;
                        if (row != nullptr &&
                            pc < static_cast<int>(row->size()) &&
                            row->at(static_cast<std::size_t>(pc)) != ' ') {
                            const char ch =
                                row->at(static_cast<std::size_t>(pc));
                            const ItemID item = recipe.key.at(ch);
                            drawIcon(ItemStack {item, 1},
                                     static_cast<float>(cx + pcell / 2),
                                     static_cast<float>(cy + pcell / 2),
                                     L(12));
                        }
                    }
                }
                const Vec4 outColor = can ? kText : kTextDim;
                drawIcon(ItemStack {recipe.output, recipe.outputCount},
                         static_cast<float>(r.x + L(55)),
                         static_cast<float>(r.y + r.h / 2), rowIcon);
                // The label fits the row: the " xN" suffix is kept, a long
                // item id is trimmed so it never bleeds into the other
                // column (deterministic, see fitLabel).
                const std::string suffix =
                    " x" + std::to_string(recipe.outputCount);
                const std::string label =
                    fitLabel(m_items.at(recipe.output).name, suffix, s,
                             std::max(0, r.w - L(89) - L(4))) +
                    suffix;
                addText(label, r.x + L(89), r.y + L(18), s, outColor);
            }
        } else {
            for (std::size_t i = 0; i < layout.recipes.size(); ++i) {
                const Recipe& recipe = *hints[static_cast<std::size_t>(i)];
                const UIRect& r = layout.recipes[i];
                const bool can = canCraft(m_inventory, recipe, m_items);
                const Vec4 outColor = can ? kText : kTextDim;
                drawIcon(ItemStack {recipe.output, recipe.outputCount},
                         static_cast<float>(r.x + L(14)),
                         static_cast<float>(r.y + r.h / 2), rowIcon);
                addText(m_items.at(recipe.output).name + " x" +
                            std::to_string(recipe.outputCount),
                        r.x + L(32), r.y + L(8), s, outColor);
                std::string needs;
                for (const auto& [item, count] : recipe.ingredients) {
                    if (!needs.empty()) {
                        needs += ", ";
                    }
                    needs += m_items.at(item).name + " x" +
                             std::to_string(count);
                }
                addText("需要：" + needs, r.x + L(32), r.y + L(28), s,
                        kTextDim); // PRE-1 (item ids stay English data)
            }
        }

        // The hovered slot/cell's item name + the carried stack at the
        // cursor.
        if (hoverSlot >= 0) {
            const ItemStack& stack = m_inventory.at(hoverSlot);
            if (!stack.empty()) {
                // PRE-12 (tooltip): name + " xN" (count > 1) + the
                // d/max durability readout line (the pure
                // ui/item_tooltip; the Phase-15 "only the name" residue).
                // Drawn via the tooltip batches (after the 2D batches and
                // the 3D block-cube icons — see appendTooltip).
                appendTooltip(itemTooltip(m_items.at(stack.item), stack),
                              mx + L(12), my + L(12), s, L(16));
            }
        } else if (hoverCell >= 0) {
            const ItemStack& stack =
                ui.grid().at(layout.gridSlots[static_cast<std::size_t>(hoverCell)]);
            if (!stack.empty()) {
                appendTooltip(itemTooltip(m_items.at(stack.item), stack),
                              mx + L(12), my + L(12), s, L(16));
            }
        }
        if (ui.carrying()) {
            drawIcon(ui.carry(), static_cast<float>(mx),
                     static_cast<float>(my), slotIcon);
        }
        // Close hint, centered at the panel bottom.
        const std::string hint = "按 E 关闭"; // PRE-1
        addText(hint,
                layout.panel.x + static_cast<int>(std::lround(
                    (layout.panel.w - textWidth(hint, s)) / 2.0)),
                layout.panel.y + layout.panel.h - L(20), s, kTextDim);
    }

    // -- F3 debug overlay (while toggled) ------------------------------------
    if (m_debugOverlay) {
        DebugInfo info;
        info.fps = static_cast<int>(std::lround(m_fpsSmoothed));
        info.frame = m_frameCount;
        info.drawCalls = m_renderer.stats().drawCalls;
        info.vertices = m_renderer.stats().vertices;
        info.chunks = static_cast<int>(m_world.chunkCount());
        if (m_player) {
            info.posX = m_player->position().x;
            info.posY = m_player->position().y;
            info.posZ = m_player->position().z;
            info.yaw = m_player->yaw;
            info.pitch = m_player->pitch;
        }
        if (m_player) {
            info.columnX =
                floorDiv(static_cast<int>(std::floor(m_player->position().x)),
                         kChunkSize);
            info.columnZ =
                floorDiv(static_cast<int>(std::floor(m_player->position().z)),
                         kChunkSize);
        }
        info.seed = m_generator ? m_generator->seed() : 0;
        info.timeOfDay = m_dayCycle.timeOfDay();
        info.dayFactor = m_dayCycle.dayFactor();
        info.streamingEngaged = m_streamingEngaged;
        info.streamPending = static_cast<int>(m_streamPending.size());
        info.streamRadius = m_streamRadius;
        info.pointerLocked = locked;
        info.inventoryOpen = panelOpen();
        info.particles = m_particles.size();
        info.entities = m_entities.size();
        if (m_player) {
            info.health = m_player->health();
            info.maxHealth = m_player->maxHealth();
            info.absorption = m_player->absorption();
            info.armorPoints = m_player->armorPoints(); // SURV-1
            info.oxygen = m_drown.oxygen; // SURV-2
            info.hunger = m_player->hunger();
            info.maxHunger = m_player->maxHunger();
        }
        // SURV-3: the held item's line — "(none)" on an empty hand, the
        // item name otherwise, plus " damage/durability" when the item
        // has one (the d/max readout; a full-durability held item shows
        // 0.0).
        {
            const ItemStack& held = m_inventory.at(m_selectedSlot);
            if (held.empty()) {
                info.heldItem = "(none)";
            } else {
                const Item& hi = m_items.at(held.item);
                char buf[128];
                if (hi.durability > 0.0f) {
                    std::snprintf(buf, sizeof(buf), "%s %.1f/%.0f",
                                  hi.name.c_str(), held.damage,
                                  hi.durability);
                } else {
                    std::snprintf(buf, sizeof(buf), "%s", hi.name.c_str());
                }
                info.heldItem = buf;
            }
        }
        info.difficulty = difficultyName(m_difficulty);
        // Phase 46: the active status effects, "name I 12.3s; name 4.5s"
        // (application order); empty when the player has none. PRE-12
        // (the SURV-6 display boundary): every effect carries its
        // amplifier roman (tier 0 = "I").
        for (const ActiveEffect& a : m_playerEffects.list()) {
            char buf[80];
            std::snprintf(buf, sizeof(buf), "%s %s %.1fs",
                          m_effects.at(a.effect).name.c_str(),
                          effectTierRoman(a.tier), a.remaining);
            if (!info.effects.empty()) {
                info.effects += "; ";
            }
            info.effects += buf;
        }
        // PRE-12 (the SURV-7 display boundary): the player stats
        // (deathCount + kill.<name>; the map holds non-zero entries in
        // key order).
        info.stats = m_stats;
        int y = kF3Margin;
        for (const std::string& line : debugOverlayLines(info)) {
            addText(line, kF3Margin, y, 1, kText);
            y += kF3LineHeight;
        }
    }

    // Upload + draw every 2D batch (backgrounds, borders, text, 2D item
    // icons). The 3D block-cube icons are drawn after this loop.
    for (auto& b : batches) {
        drawUiBatch(b.vertices, ortho, b.textured ? *b.texture
                                                  : m_fontTexture,
                    b.textured, b.color);
    }

    // 3D block-cube icons last: on top of every 2D element (the slot
    // backgrounds would otherwise cover them). The batch draws above may
    // have bound the font/icon textures to unit 0; the cube UVs live in
    // the block atlas, so re-bind it. The cube draws through the BASIC
    // shader, whose per-draw uniforms (u_useTexture / u_color / u_alpha)
    // persist from the last world draw — the world's solid-color draws
    // (beacon beam, the clouds) leave u_useTexture = 0 + white u_color +
    // u_alpha < 1, which the cube would otherwise inherit and render as
    // a translucent white box (10-26 user report). Set the textured
    // recipe explicitly.
    if (!iconDraws.empty()) {
        m_atlas.texture().bind();
        m_basicShader.bind();
        m_basicShader.setUniform1i("u_useTexture", 1);
        m_basicShader.setUniform3f("u_color", 1.0f, 1.0f, 1.0f);
        m_basicShader.setUniform1f("u_alpha", 1.0f);
        for (const UiIconDraw& d : iconDraws) {
            const Mesh& mesh = itemMesh(m_items.at(d.stack.item).block);
            if (!mesh.valid()) {
                continue;
            }
            const float s =
                static_cast<float>(d.sizePx) / ItemEntity::kSize;
            const Mat4 model = Mat4::multiply(
                Mat4::multiply(
                    Mat4::multiply(
                        Mat4::translate({d.cx, d.cy, 0.0f}),
                        Mat4::rotateY(kIconTiltYaw)),
                    Mat4::rotateX(kIconTiltPitch)),
                Mat4::scale(s));
            m_renderer.draw(mesh, m_basicShader, Mat4::identity(), ortho,
                            model);
        }
    }

    // PRE-12: the hover tooltip — LAST of the UI passes, above the 2D
    // batches and the 3D block-cube icons (it sits cursor-adjacent, over
    // slot rects and icons). No hovered stack = zero batches = the
    // no-input baseline is untouched.
    for (auto& tb : tooltipBatches) {
        drawUiBatch(tb.vertices, ortho, tb.textured ? *tb.texture
                                                    : m_fontTexture,
                    tb.textured, tb.color);
    }

    // PRE-7: the effect tints — a full-screen veil per ACTIVE player
    // effect carrying a data-driven tint (poison = the Minecraft purple
    // veil). Constant alpha (the pulsing poison veil is a documented
    // deviation). No active tinted effect = zero quads: the no-input
    // baseline carries no effects, so nothing is drawn there (the
    // structural immunity).
    {
        const std::vector<EffectTint> tints =
            activeEffectTints(m_playerEffects, m_effects);
        for (const EffectTint& t : tints) {
            UiBatch veil;
            veil.color = {t.r, t.g, t.b, t.a};
            addQuad(veil, 0, 0, w, h);
            drawUiBatch(veil.vertices, ortho, m_fontTexture, false,
                        veil.color);
        }
    }

    // Hurt flash (Phase 16): a full-screen red veil that decays over
    // kHurtFlashSeconds after a damage event. Drawn last (over every UI
    // element); the timer is 0 in the no-input baseline, so nothing is
    // drawn there.
    if (m_hurtFlash > 0.0) {
        const float a =
            0.35f * static_cast<float>(m_hurtFlash / kHurtFlashSeconds);
        UiBatch flash;
        flash.color = {1.0f, 0.0f, 0.0f, a};
        addQuad(flash, 0, 0, w, h);
        drawUiBatch(flash.vertices, ortho, m_fontTexture, false,
                    flash.color);
    }
}

void Game::render() {
    const int w = m_window.width();
    const int h = m_window.height();

    m_renderer.beginFrame(w, h);
    // The sky background is the day-cycle color (deep night blue at
    // midnight, orange at dawn/dusk, the classic blue by day).
    const Vec3 sky = m_dayCycle.skyColor();
    m_renderer.clear({sky.x, sky.y, sky.z, 1.0f});

    const Mat4 projection = Mat4::perspective(
        kFovDegrees * static_cast<float>(M_PI) / 180.0f,
        static_cast<float>(w) / static_cast<float>(h), kNear, kFar);
    const Mat4 view = m_camera.viewMatrix();

    // Shared uniforms (program state persists across draws). Lighting is
    // baked into the vertices by the mesher (Phase 7) — no light uniforms
    // except the day/night scale for sky light.
    m_basicShader.bind();
    m_basicShader.setUniform1i("u_texture", 0);
    m_basicShader.setUniform1i("u_useTexture", 1);
    m_basicShader.setUniform1f("u_dayFactor", m_dayCycle.dayFactor());
    // Solid-color alpha default 1.0 (opaque world + arrows; the beacon beam
    // lowers it for its own draws — Phase 83). The textured path ignores it.
    m_basicShader.setUniform1f("u_alpha", 1.0f);
    // One atlas tile in UV space, for the tiled greedy chunk quads
    // (Phase 18). Unread on the legacy (tiled = 0) path.
    m_basicShader.setUniform2f(
        "u_tileSize", 1.0f / static_cast<float>(m_atlas.layout().columns),
        1.0f / static_cast<float>(m_atlas.layout().rows));
    // PRE-2 distance fog: the streaming disc edge (raw chunk
    // cross-sections) dissolves into the horizon — a linear ramp toward
    // the same-frame sky color from kFogStartScale * end to end, where
    // end = min(far plane, effective stream radius in blocks). The
    // radius is m_streamRadius capped to kStreamCapRadius (STRM-1)
    // times kChunkSize — the same clamp the stream disk itself uses.
    // render.fog (code default true, the fire.particles getBool
    // precedent) gates it; a degenerate range (end <= start — a
    // zero/negative view distance) falls back to off. Four per-frame
    // uniform sets, zero per-draw cost; the streaming code is untouched.
    const bool fogConfigOn = m_config.getBool("render.fog", true);
    const float rEffBlocks = static_cast<float>(
                                 std::min(m_streamRadius, kStreamCapRadius)) *
                             kChunkSize;
    const FogRange fog = fogDistanceRange(rEffBlocks, kFar);
    const bool fogOn = fogConfigOn && fog.end > fog.start;
    m_basicShader.setUniform1i("u_fogEnabled", fogOn ? 1 : 0);
    m_basicShader.setUniform1f("u_fogStart", fog.start);
    m_basicShader.setUniform1f("u_fogEnd", fog.end);
    m_basicShader.setUniform3f("u_fogColor", sky.x, sky.y, sky.z);
    // PRE-3 water ripples: the frame-locked phase of the water-top UV
    // wander (the white-noise tile shimmers as its sample window moves —
    // the vertex computes the offset, the fragment's tiled branch wraps
    // it inside the water tile). render.water_ripples (code default true,
    // the render.fog getBool precedent) gates it; OFF leaves the v0.26.0
    // pipeline bit-identical — the vertex branch never executes
    // (u_ripplesEnabled = 0 short-circuits before the a_water read, so
    // v_rippleOff = (0,0) for every vertex and fract(x + (0,0)) = x
    // bit-for-bit) and every non-water-top vertex carries water = 0.
    // u_tileSize (the third ripple-related uniform) is the same program
    // uniform already set above for the tiled greedy quads — no second
    // set needed. Two new per-frame uniform sets, zero per-draw cost.
    const bool rippleOn = m_config.getBool("render.water_ripples", true);
    const float ripplePhase =
        2.0f * static_cast<float>(m_frameCount % kRipplePeriodFrames) /
        static_cast<float>(kRipplePeriodFrames) *
        (static_cast<float>(M_PI) * 2.0f);
    m_basicShader.setUniform1i("u_ripplesEnabled", rippleOn ? 1 : 0);
    m_basicShader.setUniform1f("u_ripplePhase", ripplePhase);
    // PRE-5 alpha cutout: the render.alpha_test gate (code default true,
    // the render.fog / render.clouds getBool precedent — config.json is
    // untouched). ON = the fragment's `alpha < 0.5 -> discard` branch
    // runs on every basic-program draw (bucket-independent: the cutout
    // holes AND the glass frame AND the item-icon backgrounds), and the
    // blend section below drops its depth write (setDepthMask(false));
    // OFF = the branch is never taken and the depth mask is never
    // touched, so the pipeline is the v0.28.0 one bit-for-bit (the S1
    // three-way gate). One per-frame uniform set, zero per-draw cost.
    const bool alphaTestOn = m_config.getBool("render.alpha_test", true);
    m_basicShader.setUniform1i("u_discardEnabled", alphaTestOn ? 1 : 0);

    m_atlas.texture().bind();

    // Frustum culling (Phase 99.9): the chunk passes must not submit draw
    // calls for chunks the camera cannot see. At view distance 80 the
    // loaded ring holds ~200k chunks while the frustum covers a few
    // hundred; drawing them all is what collapsed the vd80 frame to
    // seconds. The test is conservative (a box is culled only when fully
    // outside one plane), so a culled chunk had zero screen coverage —
    // the rendered pixels are unchanged for any camera (byte-identical
    // no-input baseline). Entities/particles are player-scale and stay
    // unculled here (their own draw calls are few).
    const Mat4 mvp = Mat4::multiply(projection, view);
    const Frustum frustum = Frustum::fromMVP(mvp);
    const auto columnVisible = [&frustum](const ColumnPos& col) {
        const float x0 = static_cast<float>(col.x * kChunkSize);
        const float z0 = static_cast<float>(col.z * kChunkSize);
        return frustum.intersectsAabb(
            Vec3{x0, 0.0f, z0},
            Vec3{x0 + kChunkSize, static_cast<float>(kWorldHeight),
                 z0 + kChunkSize});
    };

    // Column passes: all opaque geometry first, then the PRE-5 cutout
    // geometry (discard + depth write), then the translucent (blended)
    // geometry (no depth write with alpha_test on), so blending composites
    // over the opaque + cutout scene. Phase 99.9: one merged mesh per
    // COLUMN (its eight chunks) instead of
    // one per chunk — the per-chunk submission was ~10-30 us/draw on the
    // Intel iGPU and grew with the view-distance ring (the vd80 render
    // collapse). The column list is m_loadedColumns (the incremental
    // mirror): no per-frame chunkPositions() snapshot + sort of the ~200k
    // loaded chunks.
    const std::vector<ColumnPos> columns(m_loadedColumns.begin(),
                                         m_loadedColumns.end());
    for (const ColumnPos& col : columns) {
        if (!columnVisible(col)) {
            continue;
        }
        const auto it = m_columnMeshes.find(col);
        if (it == m_columnMeshes.end() || !it->second.opaque.has_value()) {
            continue;
        }
        m_renderer.draw(*it->second.opaque, m_basicShader, view, projection,
                        columnModel(col));
    }

    // Dropped items (Phase 10): the opaque ones with the chunk pass; the
    // cutout ones (leaves, the pure-cutout model items, PRE-5) join the
    // cutout pass below; the remaining transparent ones (glass) the
    // blending pass.
    drawItemEntities(view, projection, RenderPass::Opaque);
    // Mobs (Phase 20): opaque box models, with the opaque pass (the depth
    // test then occludes them behind terrain exactly like the chunks).
    drawMobs(view, projection);
    // Arrows (Phase 21): the bow's in-flight projectiles, solid color.
    drawArrows(view, projection);
    // 3D held hand (Phase 42): the selected item's view model, in the
    // opaque pass (the depth test occludes it behind terrain like
    // everything else). No-op unless the pointer is locked and no panel is
    // open (a no-input run never renders it — baseline byte-identical).
    drawHeldHand(view, projection);
    // Campfire cooking items (Phase 71): the item icon floating above each
    // campfire with a non-empty slot — opaque pass, depth-occluded. No-op
    // without a cooking campfire (baseline byte-identical).
    drawCampfireCookItems(view, projection);
    // Opaque break particles (Phase 15) belong to the opaque pass so the
    // transparent geometry blends over them.
    drawParticles(view, projection, RenderPass::Opaque);

    m_renderer.enableBlending();
    // Sky objects (unlit billboarded quads at kSkyDistance): drawn after
    // the opaque pass so the depth test occludes them behind terrain, and
    // before the transparent pass (they never overlap chunk geometry in
    // depth — they sit behind everything in the demo area).
    drawSkyObjects(view, projection, h);

    // PRE-5 CUTOUT section (always after the sky, before the blend
    // section — the v0.28.0 relative order for a water/glass-free scene,
    // which is what the OFF immunity rests on): the leaves + cross-plant
    // column geometry, the cutout item drops and the cutout particles.
    // With render.alpha_test ON the discard is active (u_discardEnabled,
    // set above) AND the depth write is on (the GL default — the cutout
    // surface occludes what is behind its SOLID pixels, the holes write
    // no depth = the visibility fix). With it OFF the section is the old
    // transparent section bit-for-bit (no discard, depth write on).
    for (const ColumnPos& col : columns) {
        if (!columnVisible(col)) {
            continue;
        }
        const auto it = m_columnMeshes.find(col);
        if (it == m_columnMeshes.end() || !it->second.cutout.has_value()) {
            continue;
        }
        m_renderer.draw(*it->second.cutout, m_basicShader, view, projection,
                        columnModel(col));
    }
    drawItemEntities(view, projection, RenderPass::Cutout);
    drawParticles(view, projection, RenderPass::Cutout);

    // PRE-5 BLEND section: the translucent geometry (glass, the pane, the
    // fluids, the translucent items/particles, the beams, the clouds, the
    // world overlay) with the depth WRITE off — a translucent surface
    // composites over what is behind it and must not occlude it. With
    // render.alpha_test OFF the mask is never touched (the GL default
    // stays on) and the pipeline is the v0.28.0 one bit-for-bit (for a
    // water/glass-free scene; the mixed-scene order deviation is the
    // documented PRE-5 one).
    if (alphaTestOn) {
        m_renderer.setDepthMask(false);
    }
    for (const ColumnPos& col : columns) {
        if (!columnVisible(col)) {
            continue;
        }
        const auto it = m_columnMeshes.find(col);
        if (it == m_columnMeshes.end() ||
            !it->second.transparent.has_value()) {
            continue;
        }
        m_renderer.draw(*it->second.transparent, m_basicShader, view,
                        projection, columnModel(col));
    }
    drawItemEntities(view, projection, RenderPass::Blend);
    drawParticles(view, projection, RenderPass::Blend);
    // Beacon beams (Phase 83): the translucent light columns from active
    // beacons, blended + depth-occluded like the other world geometry.
    // No-op without an active beacon (baseline byte-identical).
    drawBeaconBeams(view, projection);
    // PRE-4 clouds: the drifting translucent layer at y = kCloudHeight,
    // blended + depth-occluded like the other world geometry. Gated on
    // render.clouds (code default true); OFF = zero draw (the v0.27.0
    // pipeline bit-identical). The cached mesh rebuilds only when the
    // camera's cloud cell or the drift step changes.
    drawClouds(view, projection);
    // The break crack + target outline (Phase 15): world-space, blended,
    // depth test still on (the overlay cubes sit just outside the block).
    drawWorldOverlay(view, projection);
    m_renderer.disableBlending();
    if (alphaTestOn) {
        m_renderer.setDepthMask(true); // restore the GL default
    }

    // Phase 15 UI pass: screen space (y-down ortho), depth test + culling
    // off. Every element is gated on a pointer/key state, so a no-input
    // run renders exactly the Phase 14 image (byte-identical baseline).
    m_renderer.setDepthTest(false);
    m_renderer.setCulling(false);
    m_renderer.enableBlending();
    renderUi();
    m_renderer.disableBlending();
    m_renderer.setDepthTest(true);
    m_renderer.setCulling(true);

    m_renderer.endFrame();
}

void Game::drawSkyObjects(const Mat4& view, const Mat4& projection,
                          int viewportHeight) {
    // View-space length of one pixel at the sprite's depth (see sky.vert).
    const float pxToWorld = 2.0f *
                             std::tan(kFovDegrees * static_cast<float>(M_PI) /
                                      360.0f) /
                             static_cast<float>(viewportHeight);

    const float starAlpha = m_dayCycle.starAlpha();
    if (starAlpha > 0.0f) {
        m_skyShader.bind();
        m_skyShader.setUniformMat4("u_view", view);
        m_skyShader.setUniformMat4("u_proj", projection);
        m_skyShader.setUniform3f("u_offset", 0.0f, 0.0f, 0.0f);
        m_skyShader.setUniform2f("u_sizePixels", kStarSpriteSize,
                                 kStarSpriteSize);
        m_skyShader.setUniform1f("u_pxToWorld", pxToWorld);
        m_skyShader.setUniform4f("u_color", 1.0f, 1.0f, 1.0f, starAlpha);
        m_renderer.drawSky(m_starMesh, m_skyShader, view, projection);
    }

    // Sun and moon share the single quad, offset along the celestial
    // direction. Both fade out as they cross the horizon.
    const Vec3 sun = m_dayCycle.sunDirection();
    const float sunAlpha = std::clamp(sun.y / kHorizonFade, 0.0f, 1.0f);
    if (sunAlpha > 0.0f) {
        m_skyShader.bind();
        m_skyShader.setUniformMat4("u_view", view);
        m_skyShader.setUniformMat4("u_proj", projection);
        const Vec3 sunPos = sun * static_cast<float>(kSkyDistance);
        m_skyShader.setUniform3f("u_offset", sunPos.x, sunPos.y, sunPos.z);
        m_skyShader.setUniform2f("u_sizePixels", kSunSpriteSize,
                                 kSunSpriteSize);
        m_skyShader.setUniform1f("u_pxToWorld", pxToWorld);
        m_skyShader.setUniform4f("u_color", 1.0f, 0.93f, 0.78f, sunAlpha);
        m_renderer.drawSky(m_skyPointMesh, m_skyShader, view, projection);
    }

    const Vec3 moon = m_dayCycle.moonDirection();
    const float moonAlpha = std::clamp(moon.y / kHorizonFade, 0.0f, 1.0f);
    if (moonAlpha > 0.0f) {
        m_skyShader.bind();
        m_skyShader.setUniformMat4("u_view", view);
        m_skyShader.setUniformMat4("u_proj", projection);
        const Vec3 moonPos = moon * static_cast<float>(kSkyDistance);
        m_skyShader.setUniform3f("u_offset", moonPos.x, moonPos.y, moonPos.z);
        m_skyShader.setUniform2f("u_sizePixels", kMoonSpriteSize,
                                 kMoonSpriteSize);
        m_skyShader.setUniform1f("u_pxToWorld", pxToWorld);
        m_skyShader.setUniform4f("u_color", 0.85f, 0.88f, 1.0f, moonAlpha);
        m_renderer.drawSky(m_skyPointMesh, m_skyShader, view, projection);
    }
}

// Capture the front buffer (valid right after swapBuffers) into a PNG.
// Called on the game thread with the GL context current.
Result<void> Game::takeScreenshot(const std::filesystem::path& path) {
    const int w = m_window.width();
    const int h = m_window.height();
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(w) * h * 4);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    const GLenum glErr = glGetError();
    if (glErr != GL_NO_ERROR) {
        return MC_FAIL("glReadPixels failed: " +
                       std::to_string(static_cast<unsigned>(glErr)));
    }

    // glReadPixels delivers rows bottom-to-top; PNG needs top-to-bottom.
    PngImage image;
    image.width = w;
    image.height = h;
    image.rgba.resize(pixels.size());
    for (int y = 0; y < h; ++y) {
        std::memcpy(image.rgba.data() + static_cast<std::size_t>(y) * w * 4,
                    pixels.data() + static_cast<std::size_t>(h - 1 - y) * w * 4,
                    static_cast<std::size_t>(w) * 4);
    }

    // The framebuffer alpha is a blend-state byproduct (e.g. water's texture
    // alpha), not part of the displayed image: screenshots are opaque.
    for (std::size_t i = 3; i < image.rgba.size(); i += 4) {
        image.rgba[i] = 255;
    }

    auto png = Png::encode(image);
    MC_TRY(png);
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return MC_FAIL("cannot write screenshot file: '" + path.string() + "'");
    }
    out.write(png->data(), static_cast<std::streamsize>(png->size()));
    MC_LOG_INFO("Screenshot saved: " + path.string() + " (" +
                std::to_string(w) + "x" + std::to_string(h) + ")");
    return {};
}

} // namespace mc
