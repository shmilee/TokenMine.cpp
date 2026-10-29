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
#include "world/mesh_job.hpp"
#include "world/noise.hpp"
#include "ui/font.hpp"
#include "ui/inventory_ui.hpp"
#include "ui/ui_scale.hpp"
#include "item/armor.hpp"
#include "item/durability.hpp"
#include "block/block_sound.hpp"
#include "renderer/alpha_test.hpp"
#include "renderer/fog.hpp"

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

// Sky objects (sun/moon/stars) sit this far from the world origin — inside
// the far plane (kFar) so they are never clipped.
constexpr double kSkyDistance = 800.0;
constexpr int kStarCount = 300;
// Fixed starfield (deterministic hash; any value works).
constexpr std::uint64_t kStarSeed = 0x5EEDULL;
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

// Phase 83 beacon: the refresh DURATION the beacon applies to its effect
// every frame (the fire-source pattern — standing in range resets it, so
// it only matters once the player leaves: the effect then counts down this
// long before expiring). 0.5 s = the Minecraft beacon re-application rhythm.
constexpr double kBeaconEffectDurationSeconds = 0.5;
// Phase 83 beacon beam: the translucent column geometry — a thin box
// (kBeaconBeamHalfWidth on x/z) from y=0 to y=1, scaled up to
// kBeaconBeamBaseHeight + level * kBeaconBeamLevelHeight per active beacon
// (the beam is taller at higher pyramid levels, Minecraft).
constexpr float kBeaconBeamHalfWidth = 0.15f;
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

// Demo area: 4x4 chunk columns covering world x,z in [-32, 32).
constexpr int kDemoChunkMin = -2;
constexpr int kDemoChunkMax = 1;

// Chunk streaming (Phase 14) + worker meshing (PIPE-1). The demo area is
// generated synchronously at startup; its 16 columns' initial meshing
// goes through the worker path like everything else (postMeshJobs 2
// columns/frame -> meshed within the first ~9 frames). Beyond the demo
// area, columns are generated on worker threads as the player approaches
// the edge of the loaded area.
// Worker count of the streaming pool (generation + mesh + lighting jobs).
constexpr int kStreamWorkerCount = 4;
// Columns posted for generation per frame. 2 = two of the four worker
// threads generate in parallel (the 99.8 multi-core fix: at 1 the pool
// idled ~75% of the time while the ring filled — generation of one
// column is ~1.6 ms, far below one frame). The per-frame lighting jobs
// (99.10: up to TWO — two new columns whose light regions are
// bit-level independent run in parallel on two workers, one box each,
// ~8.4 ms per LGT-1 step-0; a dependent pair falls back to one job,
// boxes sequential — planLightJobs, world/light_job.hpp) amortize to
// the same ~1 box per streamed column: the flush floor drops from the
// sequential 2-box ~16.7 ms to one box plus the FIFO start delay.
// kMaxStreamBacklog bounds the queue either way.
constexpr int kStreamColumnsPerFrame = 2;
// Streaming engages when the player's column is within this many columns
// (squared Euclidean, STRM-1: columnDistanceSq <= trigger², i.e. the
// player's own column + the four axis-adjacent ones — a diagonal missing
// column at distanceSq 2 does NOT engage) of a missing one inside the
// radius disk. 1 = the player must reach the edge of the loaded area; an
// idle player in the center of the 4x4 demo area (2 columns from its
// edge) never engages.
constexpr int kStreamTriggerDistance = 1;
// Far-plane cap on the stream disk radius (STRM-1): the effective radius
// is min(config world.view_distance, kStreamCapRadius). The cap is a
// property of the renderer (kFar), not of the user's view — no config
// key.
//
// Derivation (per-lattice-point check against kFar = 1000 blocks = 62.5
// columns): in a quadrant, a column at integer offset (dx, dz) (both >=
// 1) is potentially renderable iff its nearest corner can reach the eye
// — worst case the eye at the corner of its own column facing the target
// — 16*sqrt((dx-1)² + (dz-1)²) < kFar, i.e. (dx-1)² + (dz-1)² < 62.5².
// Verified over the lattice: every potentially-renderable column has
// d = sqrt(dx²+dz²) < 64 (continuous maximum sqrt(2) + 62.5 = 63.91;
// lattice maximum 63.89, the (41, 49) family), and every column with
// d > 64 keeps its nearest corner more than kFar from the eye (closest:
// 1002.01 blocks, the (40, 50) family) — it can never be drawn, so
// streaming it is pure memory waste. Cap 64: a radius-80 config streams
// the 12853-column disk instead of the old 25921-column square.
constexpr int kStreamCapRadius = 64;
// Unload hysteresis: a loaded column is dropped only when its squared
// distance from the player is strictly FARTHER than (view_distance + 1)²
// (STRM-1). The +1 band stops the edge columns from ping-ponging (load
// at distance R, unload at distance R) as the player's column ticks
// across a boundary.
constexpr int kUnloadHysteresis = 1;
// Re-mesh budget (PIPE-1, was kRemeshBudgetPerFrame = 8 chunks on the
// main thread): the mesher's compute moved to workers — up to this many
// COLUMNS get a snapshot mesh job per frame (postMeshJobs), nearest first
// (world/mesh_job.hpp selection). Each job meshes one whole column (8
// chunks) on a worker against its private snapshot World; the main thread
// keeps only the column-merged GL upload (updateChunkMeshes). Independent
// deque (m_meshJobs) — NOT counted against kMaxStreamBacklog's generation
// posts (the post order gen -> mesh -> light means streamChunks's
// pendingCount check runs before the mesh posts).
//
// Queue timing (why the flush stays at the per-box light floor, 4
// workers, FIFO): a fill frame posts 2 gen (~1.6 ms) + 2 mesh (<= 6 ms)
// + up to 2 light (99.10: one box each ~8.4 ms when the pair is
// independent). Worst case = 6 jobs > 4 workers: the gen jobs take two
// workers (0..1.6), the mesh jobs the other two (0..6 — hiding under
// the light), and the two light jobs (posted last) start on the first
// two freed workers (~1.6) and run IN PARALLEL (1.6..10): flush ≈
// 8.4 + 1.6 ms — the LGT-1 per-box floor plus one gen slot of FIFO
// start delay (was 13 + 1.6 with the single sequential 2-box job). The
// queue only adds latency, never a wrong result: flush() waits for
// EVERY posted job (thread_pool), and planLightJobs proves the two
// light jobs write disjoint chunks. A dependent pair posts ONE 2-box
// job instead (flush ≈ 16.7 + 1.6, the pre-99.10 behavior — the rare
// close pairs, |Δx| ≤ 3 AND |Δz| ≤ 3; the LGT-1 dominant streaming
// pairs have |Δz| 15..60 and are independent). A non-streaming frame
// posts at most 2 mesh jobs: flush <= 6 ms, and the same work no longer
// costs inside the frame.
constexpr int kMaxMeshBacklog = 2;
// Worker-pool backlog cap for the streaming posts: at most this many
// pool jobs (generation + mesh + lighting — the whole queue, 99.10: up
// to 2 of each kind per frame) may be pending (queued or running)
// before streamChunks skips its post for a frame. The per-frame flush()
// drains the whole queue, so an unbounded backlog turns into unbounded
// main-thread blocking (FPS decay the longer the disk takes to fill —
// e.g. the capped radius-64 disk is ~103k chunks). 8 = two frames of
// posts at the 4-worker pool's depth.
constexpr std::size_t kMaxStreamBacklog = 8;

// A column is the full world height: kWorldHeight / kChunkSize chunks.
constexpr int kVerticalChunksPerColumn = kWorldHeight / kChunkSize;

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
constexpr int kCrackStages = 5;
// The overlay cubes sit just outside the target block's faces so they
// never z-fight with the block itself (the crack) and stay visible from
// outside (the outline).
constexpr float kCrackCubeScale = 1.006f;
constexpr float kOutlineScale = 1.004f;
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

double nowSeconds() {
    return static_cast<double>(SDL_GetPerformanceCounter()) /
           SDL_GetPerformanceFrequency();
}

// The column's model matrix: vertex positions bake each chunk's LAYER
// offset (y), so the model translates the column origin in X/Z only
// (baking X/Z into the vertices as well would double-apply it).
Mat4 columnModel(const ColumnPos& col) {
    return Mat4::translate({static_cast<float>(col.x * kChunkSize), 0.0f,
                            static_cast<float>(col.z * kChunkSize)});
}

} // namespace

Game::~Game() {
    // Close audio before the window's SDL_Quit (idempotent: run() already
    // did this on the normal path; this covers an early destruction).
    m_sound.shutdown();
    if (m_initialized) {
        m_window.shutdown();
    }
}

// ---------------------------------------------------------------------------
// Scene construction
// ---------------------------------------------------------------------------

Result<void> Game::buildScene() {
    // PIPE-2: the save journal's observer, installed at the TOP of
    // buildScene — before ANY cell-level write: the demo-area generation
    // (the generator callback) writes fresh content (never a journal
    // entry), and the save load's replay (tryLoadSave -> applySave ->
    // applyWorldEdits -> World::setBlock/setFluid, below) seeds the
    // journal automatically with zero extra code.
    m_world.setCellMutationObserver([this](const CellMutation& mu) {
        bool capped = false;
        if (mu.block.valid()) {
            capped = m_saveJournal.recordBlock(mu.pos);
        }
        // The displacement double report: a setBlock into a fluid cell
        // also recorded the drained cell in the fluid table.
        if (mu.fluidCode.has_value()) {
            capped = capped || m_saveJournal.recordFluid(mu.pos);
        }
        if (capped) {
            MC_LOG_INFO("Save journal exceeded kSaveJournalMaxEntries (" +
                        std::to_string(kSaveJournalMaxEntries) +
                        " entries) — the exit save falls back to the "
                        "full world diff");
        }
    });
    // -- shaders -------------------------------------------------------------
    auto vertSrc = m_assets.loadText("shaders/basic.vert");
    MC_TRY(vertSrc);
    auto fragSrc = m_assets.loadText("shaders/basic.frag");
    MC_TRY(fragSrc);

    auto shader = ShaderProgram::create(*vertSrc, *fragSrc);
    MC_TRY(shader);
    m_basicShader = std::move(*shader);

    // -- sky shader (unlit point sprites: sun, moon, stars) -------------------
    auto skyVert = m_assets.loadText("shaders/sky.vert");
    MC_TRY(skyVert);
    auto skyFrag = m_assets.loadText("shaders/sky.frag");
    MC_TRY(skyFrag);
    auto skyShader = ShaderProgram::create(*skyVert, *skyFrag);
    MC_TRY(skyShader);
    m_skyShader = std::move(*skyShader);

    // -- Phase 15 UI: unlit screen-space shader + generated textures --------
    auto uiVert = m_assets.loadText("shaders/ui.vert");
    MC_TRY(uiVert);
    auto uiFrag = m_assets.loadText("shaders/ui.frag");
    MC_TRY(uiFrag);
    auto uiShader = ShaderProgram::create(*uiVert, *uiFrag);
    MC_TRY(uiShader);
    m_uiShader = std::move(*uiShader);

    auto fontPath = m_assets.resolve("textures/font.png");
    MC_TRY(fontPath);
    auto fontTex = Texture::loadPng(*fontPath);
    MC_TRY(fontTex);
    m_fontTexture = std::move(*fontTex);

    auto crackPath = m_assets.resolve("textures/cracks.png");
    MC_TRY(crackPath);
    auto crackTex = Texture::loadPng(*crackPath);
    MC_TRY(crackTex);
    m_crackTexture = std::move(*crackTex);

    // Health heart strip (Phase 16): 48x16, three 16x16 tiles
    // (full/half/empty; tile t occupies u in [t/3, (t+1)/3]).
    auto healthPath = m_assets.resolve("textures/health.png");
    MC_TRY(healthPath);
    auto healthTex = Texture::loadPng(*healthPath);
    MC_TRY(healthTex);
    m_healthTexture = std::move(*healthTex);

    // Hunger drumstick strip (Phase 23): same 48x16 three-tile layout.
    auto hungerPath = m_assets.resolve("textures/hunger.png");
    MC_TRY(hungerPath);
    auto hungerTex = Texture::loadPng(*hungerPath);
    MC_TRY(hungerTex);
    m_hungerTexture = std::move(*hungerTex);

    // SURV-2: the oxygen bubble strip — a SINGLE 16x16 tile (no
    // half-state: the fill count IS the number of drawn icons). It
    // replaces the hunger row on screen while the air is below full.
    auto oxygenPath = m_assets.resolve("textures/oxygen.png");
    MC_TRY(oxygenPath);
    auto oxygenTex = Texture::loadPng(*oxygenPath);
    MC_TRY(oxygenTex);
    m_oxygenTexture = std::move(*oxygenTex);

    // Status effect icon strip (Phase 46): 16 px tiles (tile t occupies u
    // in [t/N, (t+1)/N]); the effect data's 'icon' field indexes a tile
    // (cross-checked against this texture's width at init, with the
    // registry loaded).
    auto effectPath = m_assets.resolve("textures/effects.png");
    MC_TRY(effectPath);
    auto effectTex = Texture::loadPng(*effectPath);
    MC_TRY(effectTex);
    m_effectTexture = std::move(*effectTex);

    // Target-block outline: a slightly enlarged unit cube as 24 line
    // vertices (12 edges x 2 endpoints), centered on the origin.
    {
        const float s = kOutlineScale * 0.5f;
        const float x0 = -s, x1 = s, y0 = -s, y1 = s, z0 = -s, z1 = s;
        const Vec3 kEdges[24] = {
            {x0, y0, z0}, {x1, y0, z0}, {x1, y0, z0}, {x1, y0, z1},
            {x1, y0, z1}, {x0, y0, z1}, {x0, y0, z1}, {x0, y0, z0},
            {x0, y1, z0}, {x1, y1, z0}, {x1, y1, z0}, {x1, y1, z1},
            {x1, y1, z1}, {x0, y1, z1}, {x0, y1, z1}, {x0, y1, z0},
            {x0, y0, z0}, {x0, y1, z0}, {x1, y0, z0}, {x1, y1, z0},
            {x1, y0, z1}, {x1, y1, z1}, {x0, y0, z1}, {x0, y1, z1},
        };
        std::vector<MeshVertex> verts;
        verts.reserve(24);
        for (const Vec3& p : kEdges) {
            MeshVertex v;
            v.position = p;
            verts.push_back(v);
        }
        auto up = m_outlineMesh.upload(verts, {}, MeshMode::Lines);
        MC_TRY(up);
    }

    // Phase 83 beacon beam: a thin translucent column (half-width on x/z,
    // unit height on y) — scaled + positioned per active beacon by the model
    // matrix in drawBeaconBeams. All 8 corners share normal +Y (so every face
    // gets the top-face shade = a uniform bright column, independent of
    // orientation). The baked light (skyLight 0.0 / blockLight 1.0 = the
    // legacy constant-white fallback) is swapped PER DRAW for the beacon
    // block's cell light by the LGT-4 vertex override (beamLightSample,
    // the PRE-6 itemDropLight pattern) — the geometry is never re-baked.
    // Culling is disabled for the beam draw, so the winding only needs to
    // trace each quad (both faces render, alpha-blended).
    {
        const float h = kBeaconBeamHalfWidth;
        const Vec3 kCorners[8] = {
            {-h, 0.0f, -h}, {h, 0.0f, -h}, {h, 0.0f, h}, {-h, 0.0f, h},
            {-h, 1.0f, -h}, {h, 1.0f, -h}, {h, 1.0f, h}, {-h, 1.0f, h},
        };
        const std::uint32_t kIndices[36] = {
            0, 1, 5, 0, 5, 4,  // -Z
            2, 3, 7, 2, 7, 6,  // +Z
            3, 0, 4, 3, 4, 7,  // -X
            1, 2, 6, 1, 6, 5,  // +X
            0, 3, 2, 0, 2, 1,  // -Y
            4, 5, 6, 4, 6, 7,  // +Y
        };
        std::vector<MeshVertex> verts;
        verts.reserve(8);
        for (const Vec3& p : kCorners) {
            MeshVertex v;
            v.position = p;
            v.normal = {0.0f, 1.0f, 0.0f};
            v.uv = {0.0f, 0.0f};
            v.skyLight = 0.0f;
            v.blockLight = 1.0f;
            v.uvLocal = {0.0f, 0.0f};
            v.tiled = 0.0f;
            verts.push_back(v);
        }
        std::vector<std::uint32_t> indices(kIndices, kIndices + 36);
        auto up =
            m_beaconBeamMesh.upload(verts, indices, MeshMode::Triangles);
        MC_TRY(up);
    }

    // Crack overlays: one full cube per stage (kCrackStages tiles of the
    // crack strip), slightly enlarged, UVs = the stage's tile.
    for (int stage = 0; stage < kCrackStages; ++stage) {
        std::vector<MeshVertex> verts;
        std::vector<std::uint32_t> indices;
        verts.reserve(24);
        indices.reserve(36);
        const float u0 = static_cast<float>(stage) / kCrackStages;
        const float u1 = static_cast<float>(stage + 1) / kCrackStages;
        for (const CubeFace& face : kCubeFaces) {
            // Full tile height (the crack art is directionless); the corner
            // UV order matches the verified cube-face mapping.
            const Vec2 faceUv[4] = {
                {u0, 1.0f}, {u1, 1.0f}, {u1, 0.0f}, {u0, 0.0f},
            };
            const std::uint32_t first =
                static_cast<std::uint32_t>(verts.size());
            for (int i = 0; i < 4; ++i) {
                MeshVertex v;
                v.position = {
                    (static_cast<float>(face.corner[i][0]) - 0.5f) *
                        kCrackCubeScale,
                    (static_cast<float>(face.corner[i][1]) - 0.5f) *
                        kCrackCubeScale,
                    (static_cast<float>(face.corner[i][2]) - 0.5f) *
                        kCrackCubeScale};
                v.normal = face.normal;
                v.uv = faceUv[i];
                verts.push_back(v);
            }
            indices.insert(indices.end(),
                           {first, first + 1, first + 2, first, first + 2,
                            first + 3});
        }
        auto up = m_crackMeshes[stage].upload(verts, indices);
        MC_TRY(up);
    }

    // -- day/night cycle (config-driven; --time overrides the start time) -----
    DayCycle::Params dayParams;
    dayParams.dayLengthSeconds =
        m_config.getDouble("world.day_length_seconds", 240.0);
    dayParams.startTimeOfDay =
        m_config.getDouble("world.start_time_of_day", 0.25);
    m_dayCycle = DayCycle(dayParams);

    // -- sky sprites: the starfield (deterministic quads) + the single quad
    //    the sun and moon share (positioned per draw by u_offset) ------------
    // Shared quad corners: view-space [-0.5, 0.5]^2, carried in the vertex
    // "normal" slot (the sky shader reads it as a_corner).
    const Vec3 kQuadCorners[4] = {{-0.5f, -0.5f, 0.0f},
                                  {0.5f, -0.5f, 0.0f},
                                  {0.5f, 0.5f, 0.0f},
                                  {-0.5f, 0.5f, 0.0f}};
    const std::uint32_t kQuadIndices[6] = {0, 1, 2, 0, 2, 3};

    std::vector<MeshVertex> starVerts;
    starVerts.reserve(static_cast<std::size_t>(kStarCount) * 4);
    std::vector<std::uint32_t> starIndices;
    starIndices.reserve(static_cast<std::size_t>(kStarCount) * 6);
    int star = 0;
    for (const Vec3& p :
         DayCycle::makeStarPositions(kStarSeed, kStarCount, kSkyDistance)) {
        for (const Vec3& corner : kQuadCorners) {
            MeshVertex v;
            v.position = p;
            v.normal = corner;
            starVerts.push_back(v);
        }
        const std::uint32_t base = static_cast<std::uint32_t>(star * 4);
        for (const std::uint32_t idx : kQuadIndices) {
            starIndices.push_back(base + idx);
        }
        ++star;
    }
    auto starUpload = m_starMesh.upload(starVerts, starIndices);
    MC_TRY(starUpload);

    std::vector<MeshVertex> pointVerts;
    for (const Vec3& corner : kQuadCorners) {
        MeshVertex v;
        v.normal = corner;
        pointVerts.push_back(v);
    }
    auto pointUpload =
        m_skyPointMesh.upload(pointVerts, {0, 1, 2, 0, 2, 3});
    MC_TRY(pointUpload);

    // -- blocks (data-driven from assets/data/blocks/*.json) ------------------
    auto blocksLoad = m_blocks.loadDirectory(m_assets, "data/blocks");
    MC_TRY(blocksLoad);

    // Phase 75: the campfire FAMILY ids (lit + extinguished), resolved
    // once (the registry is static after init; an absent name leaves the
    // id invalid = the family-dependent paths are structurally inert —
    // the P70 fire/campfire id pattern).
    m_campfireLitId = m_blocks.idForName("campfire");
    m_campfireExtId = m_blocks.idForName("campfire_extinguished");

    // -- texture atlas (16x16 tiles from assets/textures/blocks/*.png) --------
    // The layout is data (data/textures/block_atlas.json): explicit cells
    // per tile on a power-of-two grid, so adding a texture never moves an
    // existing tile's UVs (byte-stable baseline screenshots).
    auto atlasLoad = m_atlas.loadDirectory(m_assets, "textures/blocks");
    MC_TRY(atlasLoad);
    // Mob texture tiles (Phase 20) join the same atlas (separate directory,
    // same data-driven layout file).
    auto mobAtlasLoad = m_atlas.loadDirectory(m_assets, "textures/mobs");
    MC_TRY(mobAtlasLoad);
    auto atlasLayoutText = m_assets.loadText("data/textures/block_atlas.json");
    MC_TRY(atlasLayoutText);
    auto atlasLayout = Json::parse(*atlasLayoutText);
    MC_TRY(atlasLayout);
    auto atlasUpload = m_atlas.upload(*atlasLayout);
    MC_TRY(atlasUpload);

    // Cross-check: every texture a block references must exist in the atlas.
    for (const std::string& name : m_blocks.requiredTextures()) {
        if (!m_atlas.contains(name)) {
            return MC_FAIL("block references missing texture '" + name +
                           "' (no such tile in the atlas)");
        }
    }

    // -- items (Phase 11): implicit block items + explicit data items -------
    // Block items are defined by their blocks (same name); explicit items
    // (tools and materials) come from data files. Build the block items
    // first so an explicit item colliding with a block name is rejected.
    MC_TRY(m_items.buildBlockItems(m_blocks));
    auto itemsLoad = m_items.loadDirectory(m_assets, "data/items");
    MC_TRY(itemsLoad);
    // Strict data check: a block's drops must name a registered item whose
    // max stack fits dropsCount (block items cover the usual case, e.g.
    // grass -> wheat_1 seed; ores drop material items, e.g.
    // iron_ore -> iron_ingot; mature crops drop a harvest material, e.g.
    // wheat_7 -> wheat x2).
    for (const BlockID id : m_blocks.allIds()) {
        const Block& b = m_blocks.at(id);
        if (b.properties.drops.empty()) {
            continue;
        }
        const ItemID dropId = m_items.idForName(b.properties.drops);
        if (!dropId.valid()) {
            return MC_FAIL("block '" + b.name + "' drops unknown item '" +
                           b.properties.drops + "'");
        }
        if (b.properties.dropsCount > m_items.at(dropId).maxStack) {
            return MC_FAIL("block '" + b.name + "' drops " +
                           std::to_string(b.properties.dropsCount) +
                           " of '" + b.properties.drops +
                           "' but its max stack is " +
                           std::to_string(m_items.at(dropId).maxStack));
        }
    }

    // Strict data check (Phase 48): a block's grows_to must name an
    // EXISTING block (the next growth stage). The pairing + range are
    // enforced at data load; this is the existence cross-check (a typo in
    // the target name fails init loudly — the ammo discipline).
    for (const BlockID id : m_blocks.allIds()) {
        const Block& b = m_blocks.at(id);
        if (b.properties.growsTo.empty()) {
            continue;
        }
        const BlockID toId = m_blocks.idForName(b.properties.growsTo);
        if (!toId.valid()) {
            return MC_FAIL("block '" + b.name + "' grows to unknown block '" +
                           b.properties.growsTo + "'");
        }
    }

    // Strict data check (Phase 50): a block's grows_on must name an
    // EXISTING block (the soil the crop needs below it) — the same
    // existence cross-check as grows_to (a typo fails init loudly).
    for (const BlockID id : m_blocks.allIds()) {
        const Block& b = m_blocks.at(id);
        if (b.properties.growsOn.empty()) {
            continue;
        }
        const BlockID onId = m_blocks.idForName(b.properties.growsOn);
        if (!onId.valid()) {
            return MC_FAIL("block '" + b.name + "' grows on unknown block '" +
                           b.properties.growsOn + "'");
        }
    }

    // Strict data check (Phase 53): each plant_on name must reference an
    // EXISTING block (the soil the block can be placed on) — the same
    // existence cross-check as grows_on (a typo fails init loudly).
    for (const BlockID id : m_blocks.allIds()) {
        const Block& b = m_blocks.at(id);
        for (const std::string& soil : b.properties.plantOn) {
            const BlockID onId = m_blocks.idForName(soil);
            if (!onId.valid()) {
                return MC_FAIL("block '" + b.name + "' plant_on names "
                               "unknown block '" + soil + "'");
            }
        }
    }

    // GEN-2: resolve the tree specs (the sapling `tree` names -> BlockIDs,
    // the m_campfireLitId init-resolved pattern). The registry load
    // already validated the trunk / leaves names EXIST and the parse
    // validated the geometry, so the ids resolve (invalid would be a
    // logic error — the defensive guard keeps the spec out of the cache).
    m_treeSpecs.clear();
    for (const BlockID id : m_blocks.allIds()) {
        const Block& b = m_blocks.at(id);
        if (!b.hasTree()) {
            continue;
        }
        TreeSpec spec = treeSpecFromBlock(b);
        spec.trunk = m_blocks.idForName(b.properties.tree->trunk);
        spec.leaves = m_blocks.idForName(b.properties.tree->leaves);
        if (!spec.trunk.valid() || !spec.leaves.valid()) {
            return MC_FAIL("block '" + b.name + "' tree names unknown "
                           "block(s) '" + b.properties.tree->trunk +
                           "' / '" + b.properties.tree->leaves + "'");
        }
        m_treeSpecs[b.id] = std::move(spec);
    }

    // Strict data check (Phase 76): a block's ignites_to must name an
    // EXISTING block (the relight target) — the same existence
    // cross-check as grows_on (a typo fails init loudly).
    for (const BlockID id : m_blocks.allIds()) {
        const Block& b = m_blocks.at(id);
        if (b.properties.ignitesTo.empty()) {
            continue;
        }
        const BlockID toId = m_blocks.idForName(b.properties.ignitesTo);
        if (!toId.valid()) {
            return MC_FAIL("block '" + b.name + "' ignites_to names unknown "
                           "block '" + b.properties.ignitesTo + "'");
        }
    }

    // Strict data check (Phase 92): a block's stair_rotates_to must name an
    // EXISTING block (the next stair facing variant) — the same existence
    // cross-check as grows_on / ignites_to (a typo fails init loudly).
    for (const BlockID id : m_blocks.allIds()) {
        const Block& b = m_blocks.at(id);
        if (b.properties.stairRotatesTo.empty()) {
            continue;
        }
        const BlockID toId = m_blocks.idForName(b.properties.stairRotatesTo);
        if (!toId.valid()) {
            return MC_FAIL("block '" + b.name + "' stair_rotates_to names "
                           "unknown block '" + b.properties.stairRotatesTo +
                           "'");
        }
    }

    // CRAFT-3: a block's sound_material must name a material whose three
    // event wavs exist in the sound directory (the grows_to discipline —
    // a typo fails init loudly, naming the missing piece). File-based,
    // not the loaded bank: the check holds when the audio is disabled
    // (the headless degradation path keeps init strict).
    MC_TRY(checkSoundMaterials(m_blocks, m_assets));

    // Strict data check (Phase 51): a fluid block's water_reaction names
    // two EXISTING blocks (the source-water and flowing-water products of
    // the reaction — the ammo discipline; a typo fails init loudly). The
    // pairing is enforced at data load; this is the existence check.
    for (const BlockID id : m_blocks.allIds()) {
        const Block& b = m_blocks.at(id);
        if (b.properties.waterReactionSource.empty()) {
            continue;
        }
        const BlockID sourceId =
            m_blocks.idForName(b.properties.waterReactionSource);
        const BlockID flowingId =
            m_blocks.idForName(b.properties.waterReactionFlowing);
        if (!sourceId.valid() || !flowingId.valid()) {
            return MC_FAIL("block '" + b.name + "' water_reaction names "
                           "unknown block(s) '" +
                           b.properties.waterReactionSource + "' / '" +
                           b.properties.waterReactionFlowing + "'");
        }
    }

    // Strict data check (Phase 49): a block's chance_drop item must name a
    // registered item whose max stack fits chanceDropCount (the drops
    // discipline — a typo in the extra-drop name fails init loudly).
    for (const BlockID id : m_blocks.allIds()) {
        const Block& b = m_blocks.at(id);
        if (b.properties.chanceDropItem.empty() &&
            b.properties.chanceDrops.empty()) {
            continue;
        }
        if (!b.properties.chanceDropItem.empty()) {
            const ItemID cdId = m_items.idForName(b.properties.chanceDropItem);
            if (!cdId.valid()) {
                return MC_FAIL("block '" + b.name + "' chance_drop names unknown "
                               "item '" + b.properties.chanceDropItem + "'");
            }
            if (b.properties.chanceDropCount > m_items.at(cdId).maxStack) {
                return MC_FAIL("block '" + b.name + "' chance_drop yields " +
                               std::to_string(b.properties.chanceDropCount) +
                               " of '" + b.properties.chanceDropItem +
                               "' but its max stack is " +
                               std::to_string(m_items.at(cdId).maxStack));
            }
        }
        // Phase 52: the chance_drops LIST — the same discipline per entry
        // (the item must exist and its max stack fit the entry's count).
        for (std::size_t i = 0; i < b.properties.chanceDrops.size(); ++i) {
            const ChanceDrop& e = b.properties.chanceDrops[i];
            const ItemID id2 = m_items.idForName(e.item);
            if (!id2.valid()) {
                return MC_FAIL("block '" + b.name + "' chance_drops[" +
                               std::to_string(i) + "] names unknown item '" +
                               e.item + "'");
            }
            if (e.count > m_items.at(id2).maxStack) {
                return MC_FAIL("block '" + b.name + "' chance_drops[" +
                               std::to_string(i) + "] yields " +
                               std::to_string(e.count) + " of '" + e.item +
                               "' but its max stack is " +
                               std::to_string(m_items.at(id2).maxStack));
            }
        }
        continue;
    }

    // -- mobs (Phase 20): data-driven types + spawn config ------------------
    // Types come from data/mobs/*.json (one file per mob); the spawn
    // cadence/ring/cap from data/mob_spawning.json; the enable flag from
    // the config (mobs.enabled, default true). Cross-checks (same
    // discipline as block drops): the spawning list, every model texture
    // and every drop must resolve.
    MC_TRY(m_mobTypes.loadDirectory(m_assets, "data/mobs"));
    auto spawnText = m_assets.loadText("data/mob_spawning.json");
    MC_TRY(spawnText);
    auto spawnJson = Json::parse(*spawnText);
    MC_TRY(spawnJson);
    auto spawnCfg = MobSpawningConfig::parse(*spawnJson);
    MC_TRY(spawnCfg);
    m_spawnConfig = *spawnCfg;
    m_mobsEnabled = m_config.getBool("mobs.enabled", true);
    // ENT-1: the limb-swing enable flag (config mobs.limb_swing, default
    // true; false = the v0.9.0 static gait — the mobs.enabled pattern,
    // the E2E control group). Pure render layer: it gates nothing else.
    m_limbSwing = m_config.getBool("mobs.limb_swing", true);
    // Difficulty (Phase 25): strict — an unknown value fails init with a
    // message naming the field (the config default is "normal", which is
    // also the no-behavior-change baseline: scales 1.0 / cadence base).
    auto difficulty = parseDifficulty(
        m_config.getString("mobs.difficulty", "normal"));
    MC_TRY(difficulty);
    m_difficulty = *difficulty;
    for (const SpawnMobEntry& entry : m_spawnConfig.mobs) {
        if (m_mobTypes.findByName(entry.id) == nullptr) {
            return MC_FAIL("mob_spawning.json: unknown mob '" + entry.id +
                           "'");
        }
    }
    for (const MobID mid : m_mobTypes.allIds()) {
        const Mob& mob = m_mobTypes.at(mid);
        for (const MobBox& box : mob.model.boxes) {
            if (!m_atlas.contains(box.texture)) {
                return MC_FAIL("mob '" + mob.name + "': model texture '" +
                               box.texture + "' not in the atlas");
            }
        }
        for (const MobDrop& drop : mob.properties.drops) {
            const ItemID dropId = m_items.idForName(drop.item);
            if (!dropId.valid()) {
                return MC_FAIL("mob '" + mob.name + "' drops unknown item '" +
                               drop.item + "'");
            }
            // The ROLLED count can never exceed countMax (Phase 38), so
            // the max stack is checked against the range's top.
            if (drop.countMax > m_items.at(dropId).maxStack) {
                return MC_FAIL("mob '" + mob.name + "' drops up to " +
                               std::to_string(drop.countMax) +
                               " of '" + drop.item +
                               "' but its max stack is " +
                               std::to_string(m_items.at(dropId).maxStack));
            }
        }
    }

    // -- status effects (Phase 46) ------------------------------------------
    // The data-driven effect definitions (data/effects/*.json) + the
    // cross-checks: a mob's on_hit_effect must name a registered effect
    // (the ammo/milk pattern) and every effect's icon must be a valid
    // tile of the effects.png strip (the block-texture pattern).
    MC_TRY(m_effects.loadDirectory(m_assets, "data/effects"));
    const int effectTiles =
        static_cast<int>(m_effectTexture.width() / TextureAtlas::kTileSize);
    for (const int effId : m_effects.allIds()) {
        const Effect& eff = m_effects.at(effId);
        if (eff.icon < 0 || eff.icon >= effectTiles) {
            return MC_FAIL("effect '" + eff.name + "' icon " +
                           std::to_string(eff.icon) + " is outside the "
                           "effects.png tile range [0, " +
                           std::to_string(effectTiles - 1) + "]");
        }
    }
    // Phase 57: at most ONE effect may ignite the player from the lava
    // (the fire). Zero is legal (no fire system in a modded asset set).
    // Phase 58: the MOB burn ticks on the fixed 1/60 frame grid (hard rule
    // 8), so the ignites effect's tick interval must be a whole number of
    // frames (the shipped fire: 0.2 s = 12 frames) — a non-grid interval
    // would silently quantize the mob's burn rate.
    for (const int effId : m_effects.allIds()) {
        const Effect& eff = m_effects.at(effId);
        if (!eff.ignitesOnLava) {
            continue;
        }
        if (m_fireEffectId != -1) {
            return MC_FAIL("only one effect may have 'ignites_on_lava' "
                           "(found '" + m_effects.at(m_fireEffectId).name +
                           "' and '" + eff.name + "')");
        }
        // The tolerance is in FRAME units (1e-4 frames = 1.7 us per tick):
        // the float interval 0.2f * 60 lands at 12.00000018, while any
        // genuinely non-grid interval (0.33 s = 19.8 frames) misses by a
        // large fraction of a frame.
        const double frames = eff.tickInterval * 60.0;
        if (frames < 1.0 ||
            std::abs(frames - std::round(frames)) > 1e-4) {
            return MC_FAIL("effect '" + eff.name +
                           "' has 'ignites_on_lava' but its tick_interval "
                           "is not a whole number of 1/60 s frames (the "
                           "mob burn grid)");
        }
        m_fireEffectId = effId;
    }
    // Strict data check (Phase 83, a list in Phase 91): every effect a
    // block's beacon applies must name an EXISTING effect (the beacon
    // applies them to in-range players). The empty-string rejection is at
    // data load; this is the existence cross-check (a typo in an effect
    // name fails init loudly — the grows_to / on_hit_effect discipline).
    // Any registered effect is a valid beacon target (the shipped data uses
    // ["absorption", "speed"] — Phase 91's multi-effect "enchanting
    // table").
    for (const BlockID id : m_blocks.allIds()) {
        const Block& b = m_blocks.at(id);
        for (const std::string& effName : b.properties.beaconEffects) {
            if (m_effects.findByName(effName) == nullptr) {
                return MC_FAIL("block '" + b.name + "' beacon_effects names "
                               "unknown effect '" + effName + "'");
            }
        }
    }
    for (const MobID mid : m_mobTypes.allIds()) {
        const Mob& mob = m_mobTypes.at(mid);
        if (!mob.properties.hitEffectName.empty()) {
            const Effect* eff =
                m_effects.findByName(mob.properties.hitEffectName);
            if (eff == nullptr) {
                return MC_FAIL("mob '" + mob.name + "' on_hit_effect "
                               "references the unregistered effect '" +
                               mob.properties.hitEffectName + "'");
            }
            // SURV-6: the grant's tier range (0-based < tier_max).
            if (mob.properties.hitEffectTier >= eff->tierMax) {
                return MC_FAIL("mob '" + mob.name + "' on_hit_effect grants "
                               "'" + eff->name + "' at tier " +
                               std::to_string(mob.properties.hitEffectTier) +
                               ", but the effect has tier_max " +
                               std::to_string(eff->tierMax) +
                               " (tiers are 0-based, in [0, " +
                               std::to_string(eff->tierMax - 1) + "])");
            }
        }
        // SURV-5: the arrow effect must name an EXISTING effect (the
        // on_hit_effect pattern). Unlike the BOW's arrow_effect it does
        // NOT require `affects_mobs`: the skeleton's arrow hits the
        // PLAYER (its wither is player-facing), and a wither arrow that
        // happens to strike another mob passes the mob-side gate on its
        // own (the wither carries affects_mobs too).
        if (!mob.properties.arrowEffectName.empty()) {
            const Effect* eff =
                m_effects.findByName(mob.properties.arrowEffectName);
            if (eff == nullptr) {
                return MC_FAIL("mob '" + mob.name + "' arrow_effect "
                               "references the unregistered effect '" +
                               mob.properties.arrowEffectName + "'");
            }
            // SURV-6: the grant's tier range (0-based < tier_max).
            if (mob.properties.arrowEffectTier >= eff->tierMax) {
                return MC_FAIL("mob '" + mob.name + "' arrow_effect grants "
                               "'" + eff->name + "' at tier " +
                               std::to_string(mob.properties.arrowEffectTier) +
                               ", but the effect has tier_max " +
                               std::to_string(eff->tierMax) +
                               " (tiers are 0-based, in [0, " +
                               std::to_string(eff->tierMax - 1) + "])");
            }
        }
    }
    for (const ItemID iid : m_items.allIds()) {
        const Item& it = m_items.at(iid);
        // A grant must name an EXISTING effect (the ammo pattern) AND obey
        // the SURV-4 duration/instant equivalence: a grant of duration 0 is
        // an INSTANT effect (instant_hp != 0 — settled on apply, no
        // lingering); a grant of positive duration is a LINGERING effect
        // (instant_hp == 0). Both directions are named: a 0 on a lingering
        // effect is a dead grant; a positive duration on an instant effect
        // would leave it lingering (an instant should not).
        auto checkGrant = [&](const std::string& field,
                              const std::string& effName,
                              double duration,
                              int tier) -> Result<void> {
            const Effect* eff = m_effects.findByName(effName);
            if (eff == nullptr) {
                return MC_FAIL("item '" + it.name + "' " + field +
                               " references the unregistered effect '" +
                               effName + "'");
            }
            // SURV-6: the grant's tier is 0-based and must be < the
            // effect's tier_max (a single-tier effect rejects any
            // non-zero tier — named).
            if (tier >= eff->tierMax) {
                return MC_FAIL("item '" + it.name + "' " + field +
                               " grants '" + effName + "' at tier " +
                               std::to_string(tier) + ", but the effect "
                               "has tier_max " +
                               std::to_string(eff->tierMax) + " (tiers "
                               "are 0-based, in [0, " +
                               std::to_string(eff->tierMax - 1) + "])");
            }
            const bool instant = eff->instantHp != 0.0f;
            if (duration == 0.0 && !instant) {
                return MC_FAIL("item '" + it.name + "' " + field +
                               " grants '" + effName +
                               "' with duration 0, but the effect is not "
                               "instant (instant_hp 0) — a 0-duration grant "
                               "must name an instant effect");
            }
            if (duration > 0.0 && instant) {
                return MC_FAIL("item '" + it.name + "' " + field +
                               " grants the instant effect '" + effName +
                               "' with a positive duration — an instant "
                               "effect settles on apply and must be granted "
                               "duration 0");
            }
            return {};
        };
        if (!it.grantsEffect.empty()) {
            MC_TRY(checkGrant("grants_effect", it.grantsEffect,
                              it.grantsEffectDuration, it.grantsEffectTier));
        }
        // Phase 56: the list form, entry by entry (indexed name, the
        // chance_drops[i] pattern).
        for (std::size_t i = 0; i < it.grantsEffects.size(); ++i) {
            const Item::GrantEffect& g = it.grantsEffects[i];
            MC_TRY(checkGrant("grants_effects[" + std::to_string(i) + "]",
                              g.name, g.duration, g.tier));
        }
        // Phase 67: the fast_against list, entry by entry (the grows_on
        // discipline: a typo must fail init loudly, not silently break
        // at the normal speed).
        for (std::size_t i = 0; i < it.fastAgainst.size(); ++i) {
            const std::string& bn = it.fastAgainst[i];
            if (!m_blocks.idForName(bn).valid()) {
                return MC_FAIL("item '" + it.name + "' fast_against[" +
                               std::to_string(i) +
                               "] references the unknown block '" + bn +
                               "'");
            }
        }
        // SURV-5: the launcher's arrow effect must name an EXISTING
        // effect (the ammo pattern) AND carry `affects_mobs` — a bow's
        // arrow only ever hits MOBS (a player's bow arrow is unflagged
        // hurtPlayer, no self-damage), so a player-only effect on a bow
        // would be a dead grant (the named rejection).
        if (!it.arrowEffectName.empty()) {
            const Effect* eff = m_effects.findByName(it.arrowEffectName);
            if (eff == nullptr) {
                return MC_FAIL("item '" + it.name + "' arrow_effect "
                               "references the unregistered effect '" +
                               it.arrowEffectName + "'");
            }
            if (!eff->affectsMobs) {
                return MC_FAIL("item '" + it.name + "' arrow_effect names "
                               "effect '" + it.arrowEffectName +
                               "' which does not accept mobs "
                               "('affects_mobs' false) — a bow's arrow "
                               "only hits mobs");
            }
            // SURV-6: the grant's tier range (0-based < tier_max).
            if (it.arrowEffectTier >= eff->tierMax) {
                return MC_FAIL("item '" + it.name + "' arrow_effect grants "
                               "'" + it.arrowEffectName + "' at tier " +
                               std::to_string(it.arrowEffectTier) +
                               ", but the effect has tier_max " +
                               std::to_string(eff->tierMax) +
                               " (tiers are 0-based, in [0, " +
                               std::to_string(eff->tierMax - 1) + "])");
            }
        }
    }

    // -- fire (Phase 64, the fire visuals): the burn target --------------
    // The ignition (Phase 60) and the lava burn pass (Phase 58) burn a
    // `flamable` block into the FIRE block (a light source that burns out
    // after fire.lifetime_seconds and is extinguished by water) — not air.
    // A registry with ANY flamable block must therefore provide the
    // "fire" block (the single named reference — without it the burn
    // consumers would have no target). Zero flamable blocks (a stripped
    // asset set) is legal: the burn paths stay inert and the fire block
    // is optional.
    bool anyFlamable = false;
    for (const BlockID bid : m_blocks.allIds()) {
        if (m_blocks.at(bid).properties.flamable) {
            anyFlamable = true;
            break;
        }
    }
    m_fireId = m_blocks.idForName("fire");
    if (anyFlamable && !m_fireId.valid()) {
        return MC_FAIL("the flamable blocks require the 'fire' block (the "
                       "burn target of the ignition and the lava burn "
                       "pass) — register it in data/blocks");
    }
    m_fireLifetime = m_config.getDouble("fire.lifetime_seconds", 4.0);
    if (m_fireLifetime <= 0.0) {
        return MC_FAIL("config fire.lifetime_seconds must be > 0 (got " +
                       std::to_string(m_fireLifetime) + ")");
    }
    // Fire v2 (Phase 66): the spread roll cadence + per-neighbor chance
    // (Minecraft values: the 0.1 s tick rhythm, the normal-difficulty
    // per-neighbor 5%) and the ember-emission gate.
    m_fireSpreadInterval =
        m_config.getInt("fire.spread_interval_frames", 6);
    if (m_fireSpreadInterval < 1) {
        return MC_FAIL("config fire.spread_interval_frames must be >= 1 "
                       "(got " +
                       std::to_string(m_fireSpreadInterval) + ")");
    }
    m_fireSpreadChance = m_config.getDouble("fire.spread_chance", 0.05);
    if (m_fireSpreadChance < 0.0 || m_fireSpreadChance > 1.0) {
        return MC_FAIL("config fire.spread_chance must be in [0, 1] (got " +
                       std::to_string(m_fireSpreadChance) + ")");
    }
    m_fireParticles = m_config.getBool("fire.particles", true);
    // CRAFT-5: the lit-furnace flame gate (the fire.particles pattern;
    // off = zero emissions = bit-identical to v1.21.0).
    m_furnaceFlames = m_config.getBool("render.furnace_flames", true);

    // Phase 83 beacon: the effect range (Chebyshev blocks from the beacon
    // cell; the Minecraft beacon reaches the whole build area, 50 is a
    // generous default) and the beam visibility gate (the E2E control, the
    // fire.particles pattern — a no-input run without beacons is inert
    // either way).
    m_beaconRange = m_config.getInt("beacon.range_blocks", 50);
    if (m_beaconRange <= 0) {
        return MC_FAIL("config beacon.range_blocks must be > 0 (got " +
                       std::to_string(m_beaconRange) + ")");
    }
    m_beaconBeam = m_config.getBool("beacon.beam", true);

    // SAVE-1: the auto-save interval (save.auto_save_seconds — a CODE
    // default, the fire/mobs/beacon config-key discipline: the committed
    // config.json carries no save section). Strict read: absent = the
    // default, present = a finite number >= 0 (0 = off = the pre-SAVE-1
    // behavior; a negative / non-finite / non-number value is named and
    // rejected).
    {
        auto interval = autoSaveIntervalSeconds(m_config);
        MC_TRY(interval);
        m_autoSaveInterval = *interval;
    }

    // -- item icons (Phase 15 UI) ------------------------------------------
    // Each explicit item may name a 2D icon under assets/textures/items/
    // (strict: the file must exist — same cross-check pattern as the block
    // textures). Block items have no icon (the UI renders a 3D block cube).
    m_itemIcons.resize(m_items.count());
    for (const ItemID id : m_items.allIds()) {
        const Item& item = m_items.at(id);
        if (item.icon.empty()) {
            continue;
        }
        const std::string rel = "textures/items/" + item.icon + ".png";
        if (!m_assets.exists(rel)) {
            return MC_FAIL("item '" + item.name + "' references missing "
                           "icon texture '" + rel + "'");
        }
        auto iconPath = m_assets.resolve(rel);
        MC_TRY(iconPath);
        auto iconTex = Texture::loadPng(*iconPath);
        MC_TRY(iconTex);
        m_itemIcons[static_cast<std::size_t>(id.value)] = std::move(*iconTex);
    }

    // -- item model textures (CRAFT-2) ------------------------------------
    // Each model item names one texture file under
    // assets/textures/items/ (its icon or a dedicated one) for the box-
    // model render path. The file-existence cross-check is the icon-file
    // discipline (the grows_to pattern: it could not be validated per
    // parse because it is a filesystem check). The model geometry itself
    // is pure data — the mesh is built lazily on first render
    // (itemModelMesh).
    m_itemModelTextures.resize(m_items.count());
    m_itemModelMeshes.resize(m_items.count());
    // PRE-5: the per-item cutout census (parallel to the model textures):
    // decode the model's PNG ONCE and run isPureCutoutImage over its alpha
    // channel — a pure cutout (alpha set ⊆ {0, 255}) routes the item's
    // world drop to the CUTOUT pass (visible through the leaf holes); an
    // icon with a translucent core stays in the BLEND pass. (The shipped
    // icons are all pure cutout — the census is content data, and a
    // future translucent icon re-routes itself with zero code changes.)
    m_itemCutout.assign(m_items.count(), false);
    for (const ItemID id : m_items.allIds()) {
        const Item& item = m_items.at(id);
        if (!item.model.has_value()) {
            continue;
        }
        const std::string rel =
            "textures/items/" + item.model->texture + ".png";
        if (!m_assets.exists(rel)) {
            return MC_FAIL("item '" + item.name +
                           "' model references missing texture '" + rel +
                           "'");
        }
        auto modelPath = m_assets.resolve(rel);
        MC_TRY(modelPath);
        std::ifstream in(*modelPath, std::ios::binary);
        if (!in) {
            return MC_FAIL("cannot open item model texture file: '" +
                           modelPath->string() + "'");
        }
        const std::string bytes((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());
        auto image = Png::decode(bytes);
        MC_TRY(image);
        m_itemCutout[static_cast<std::size_t>(id.value)] =
            isPureCutoutImage(image->rgba.data(), image->width,
                              image->height);
        auto modelTex = Texture::createFromPixels(
            image->width, image->height, image->rgba.data());
        MC_TRY(modelTex);
        m_itemModelTextures[static_cast<std::size_t>(id.value)] =
            std::move(*modelTex);
    }

    // -- launcher ammo (Phase 24): the 'ammo' field names an item that
    // must exist (same cross-check pattern as the icon file check; it
    // could not be validated per-file, the ammo item may be defined in a
    // later file).
    for (const ItemID id : m_items.allIds()) {
        const Item& item = m_items.at(id);
        if (!item.ammo.empty() && m_items.findByName(item.ammo) == nullptr) {
            return MC_FAIL("item '" + item.name + "' references unknown "
                           "ammo item '" + item.ammo + "'");
        }
        // Phase 40: the furnace product (same cross-check pattern as the
        // ammo — the product may be defined in a later file).
        if (!item.smeltableTo.empty() &&
            m_items.findByName(item.smeltableTo) == nullptr) {
            return MC_FAIL("item '" + item.name + "' references unknown "
                           "smelting product '" + item.smeltableTo + "'");
        }
        // Phase 43: the drinkable's returned container (same cross-check
        // pattern — the container may be defined in a later file).
        if (!item.returnsItem.empty() &&
            m_items.findByName(item.returnsItem) == nullptr) {
            return MC_FAIL("item '" + item.name + "' references unknown "
                           "container item '" + item.returnsItem + "'");
        }
        // Phase 47 (kinds in Phase 51): the fluid-bucket PAIR (the ammo
        // pattern extended to pairs). The EMPTY side's companion is
        // DERIVED per named fluid: there must be exactly ONE full bucket
        // that pours that fluid back into this item (the parse keeps each
        // side locally consistent; only the round trip needs both files).
        // The FULL side's companion must exist and be an empty bucket
        // that collects this fluid.
        for (const std::string& fluid : item.collectsFluids) {
            // Exactly ONE full bucket must pour this fluid back into this
            // item (zero = a dead field, two = an ambiguous scoop target).
            int matches = 0;
            for (const ItemID otherId : m_items.allIds()) {
                const Item& other = m_items.at(otherId);
                if (other.poursFluid == fluid &&
                    other.poursBecomes == item.name) {
                    ++matches;
                }
            }
            if (matches != 1) {
                return MC_FAIL("item '" + item.name + "' collects fluid '" +
                               fluid + "' with " + std::to_string(matches) +
                               " full bucket(s) pouring it back (exactly "
                               "one is required — the scoop target must "
                               "be unique)");
            }
        }
        if (!item.poursBecomes.empty()) {
            const Item* other = m_items.findByName(item.poursBecomes);
            const bool collectsThisFluid =
                other != nullptr &&
                std::find(other->collectsFluids.begin(),
                          other->collectsFluids.end(),
                          item.poursFluid) != other->collectsFluids.end();
            if (other == nullptr || !collectsThisFluid) {
                return MC_FAIL("item '" + item.name + "' pours back to '" +
                               item.poursBecomes +
                               "', but that item does not collect fluid '" +
                               item.poursFluid +
                               "' (a bucket pair must be symmetric: the "
                               "empty bucket names this fluid)");
            }
        }
    }

    // -- mob arrow ammo (Phase 42): a ranged mob's 'ammo' field names the
    // item a stuck arrow returns to the player's inventory on pickup (the
    // same cross-check pattern as the launcher ammo above).
    for (const MobID id : m_mobTypes.allIds()) {
        const Mob& def = m_mobTypes.at(id);
        if (!def.properties.ammoItem.empty() &&
            m_items.findByName(def.properties.ammoItem) == nullptr) {
            return MC_FAIL("mob '" + def.name + "' references unknown "
                           "arrow ammo item '" + def.properties.ammoItem +
                           "'");
        }
        // Phase 43: the milking pair (both names must exist — the parsing
        // guarantees the pair is complete, only the names are checked
        // here, the ammo pattern).
        if (!def.properties.milkWith.empty()) {
            if (m_items.findByName(def.properties.milkWith) == nullptr) {
                return MC_FAIL("mob '" + def.name + "' milking pair "
                               "references unknown item '" +
                               def.properties.milkWith + "'");
            }
            if (m_items.findByName(def.properties.milkGives) == nullptr) {
                return MC_FAIL("mob '" + def.name + "' milking pair "
                               "references unknown item '" +
                               def.properties.milkGives + "'");
            }
        }
    }

    // -- dye families (Phase 41): the recolor interaction resolves its
    // target block purely by NAME (dyeVariantName), so the data must be
    // COMPLETE: every dyeable block names an existing dye item color,
    // and for every dye family (distinct 'dye_base' values) every dye
    // item color has its variant block (a missing variant would be a
    // dead dye — same cross-check pattern as the ammo/smelt product).
    {
        std::vector<std::string> dyeItemColors;
        for (const ItemID id : m_items.allIds()) {
            if (!m_items.at(id).dyeColor.empty()) {
                dyeItemColors.push_back(m_items.at(id).dyeColor);
            }
        }
        std::vector<std::string> dyeBases;
        for (const BlockID id : m_blocks.allIds()) {
            const BlockProperties& p = m_blocks.at(id).properties;
            if (p.dyeBase.empty()) {
                continue;
            }
            dyeBases.push_back(p.dyeBase);
            if (p.dyeColor.empty()) {
                return MC_FAIL("dyeable block '" +
                               m_blocks.at(id).name + "' (dye_base '" +
                               p.dyeBase + "') requires a 'dye_color' "
                               "property");
            }
            bool colorKnown = false;
            for (const std::string& c : dyeItemColors) {
                if (c == p.dyeColor) {
                    colorKnown = true;
                    break;
                }
            }
            if (!colorKnown) {
                return MC_FAIL("dyeable block '" + m_blocks.at(id).name +
                               "' has dye color '" + p.dyeColor +
                               "' but no dye item of that color exists");
            }
        }
        for (const std::string& base : dyeBases) {
            for (const std::string& color : dyeItemColors) {
                const std::string variant = dyeVariantName(base, color);
                if (m_blocks.findByName(variant) == nullptr) {
                    return MC_FAIL("dye family '" + base + "' is missing "
                                   "the variant block '" + variant +
                                   "' (dye color '" + color + "')");
                }
            }
        }
    }

    // -- recipes (Phase 12): data-driven crafting (UI/trigger = Phase 15) --
    auto recipesLoad = m_recipes.loadDirectory(m_assets, m_items,
                                               "data/recipes");
    MC_TRY(recipesLoad);

    // -- hotbar (starting inventory contents, data-driven) -------------------
    // assets/data/hotbar.json: { "items": [name, ...] } — at most
    // Inventory::kHotbarSize entries; slot i starts with a full stack of
    // the item (placing consumes the stacks; blocks are no longer infinite).
    auto hotbarText = m_assets.loadText("data/hotbar.json");
    MC_TRY(hotbarText);
    auto hotbarJson = Json::parse(*hotbarText);
    MC_TRY(hotbarJson);
    const Json* list = hotbarJson->find("items");
    if (list == nullptr || !list->isArray() || list->size() == 0) {
        return MC_FAIL("hotbar.json: 'items' array is required and must "
                       "not be empty");
    }
    if (list->size() > static_cast<std::size_t>(Inventory::kHotbarSize)) {
        return MC_FAIL("hotbar.json: at most " +
                       std::to_string(Inventory::kHotbarSize) +
                       " items (hotbar size)");
    }
    for (std::size_t i = 0; i < list->size(); ++i) {
        const Json& entry = list->at(i);
        if (!entry.isString()) {
            return MC_FAIL("hotbar.json: entry #" + std::to_string(i) +
                           " must be an item name string");
        }
        const ItemID iid = m_items.idForName(entry.asString());
        if (!iid.valid()) {
            return MC_FAIL("hotbar.json: unknown item '" + entry.asString() +
                           "'");
        }
        m_inventory.setSlot(i, ItemStack {iid, m_items.at(iid).maxStack});
    }

    // GEN-2: the starter STORAGE fill (hotbar.json's `inventory` section,
    // the strict parse in item/inventory — the hidden-starter pattern:
    // the saplings sit in the storage, NOT the hotbar, so a no-input run
    // renders nothing new; the slot ranges are disjoint, so no overwrite
    // of the `items` fill is possible). A SAVE LOAD still replaces the
    // whole inventory (the starter is a fresh-world gift — the applySave
    // contract).
    auto starterSlots = parseStarterSlots(*hotbarJson, m_items);
    MC_TRY(starterSlots);
    for (const StarterSlot& s : *starterSlots) {
        m_inventory.setSlot(s.slot, ItemStack {s.item, s.count});
    }

    // -- world generation (Phase 4: seeded, data-driven) ---------------------
    auto biomesLoad = m_biomes.loadDirectory(m_assets, "data/biomes");
    MC_TRY(biomesLoad);
    auto worldgen = WorldgenParams::load(m_assets);
    MC_TRY(worldgen);
    const std::int64_t seed = m_config.getInt64("world.seed", 0);
    auto gen = WorldGenerator::create(seed, *worldgen, m_biomes, m_blocks);
    MC_TRY(gen);
    m_generator = std::move(*gen);

    m_world.setChunkGenerator(makeChunkGenerator());
    for (int cx = kDemoChunkMin; cx <= kDemoChunkMax; ++cx) {
        for (int cz = kDemoChunkMin; cz <= kDemoChunkMax; ++cz) {
            for (int cy = 0; cy * kChunkSize < kWorldHeight; ++cy) {
                m_world.getOrCreateChunk({cx, cy, cz});
            }
        }
    }

    // Lighting: full propagation over the generated area. Runs once at
    // startup (a few tens of ms); break/place then use the cheap local
    // update, and chunks that appear later (streamed-in or placed into)
    // get a worker lighting job for their column box (postLightJobs).
    Lighting::propagateAll(m_world, m_blocks);

    // Chunk streaming (Phase 14, STRM-1): the worker pool and the radius
    // (config world.view_distance, in chunk columns around the player).
    // The raw config value is kept here for reporting; the geometric
    // consumers clamp it to kStreamCapRadius (far-plane cap).
    m_pool = std::make_unique<ThreadPool>(kStreamWorkerCount);
    m_streamRadius = m_config.getInt("world.view_distance", 8);

    // Spawn the player on dry land: the seed may put the origin underwater
    // or a tree may stand on the column, so check nearby candidates before
    // falling back to (0, 0).
    // Player tuning (Phase 32): the config's player.* keys override the
    // Player::Params defaults (a missing key = the struct default; present
    // keys are validated, so a mistyped value fails startup instead of
    // silently falling back).
    const auto playerParams = Player::Params::fromConfig(m_config);
    MC_TRY(playerParams);
    // UI scale (Phase 34): same missing-key / validated-value contract.
    const auto uiScale = uiScaleFromConfig(m_config);
    MC_TRY(uiScale);
    m_uiScale = *uiScale;
    const Player::Params& playerDims = *playerParams; // room check
    Vec3 spawnPos {0.5f,
                   static_cast<float>(m_generator->surfaceHeight(0, 0)) + 1.0f,
                   0.5f};
    constexpr int kSpawnCandidates[][2] = {
        {0, 0}, {4, 0}, {-4, 0}, {8, 0}, {-8, 0},
        {0, 4}, {0, -4}, {4, 4}, {-4, -4}, {8, 8},
    };
    int spawnX = 0;
    int spawnZ = 0;
    for (const auto& c : kSpawnCandidates) {
        const int h = m_generator->surfaceHeight(c[0], c[1]);
        if (h <= m_generator->params().seaLevel) {
            continue;
        }
        const Vec3 cand {static_cast<float>(c[0]) + 0.5f,
                         static_cast<float>(h) + 1.0f,
                         static_cast<float>(c[1]) + 0.5f};
        if (Player::hasRoomAt(m_world, m_blocks, cand, playerDims.width,
                              playerDims.height)) {
            spawnPos = cand;
            spawnX = c[0];
            spawnZ = c[1];
            break;
        }
    }
    // The WORLD spawn (Phase 25): the center of the hostile spawn-protection
    // circle. Captured BEFORE the save load — a loaded save moves the
    // player, but the protection stays anchored to the world spawn (the
    // Minecraft semantics; there are no beds in v1).
    m_spawnPoint = spawnPos;
    m_player.emplace(m_world, m_blocks, spawnPos, *playerParams);
    m_player->pitch = -0.25f; // look slightly down at the landscape
    m_camera.position = m_player->eyePosition();
    m_camera.yaw = m_player->yaw;
    m_camera.pitch = m_player->pitch;

    // -- save load (Phase 13): a saved state REPLACES the fresh world ----
    // Runs after generation + spawn: the cell edits apply on top of the
    // generated terrain, and the saved player/inventory/entities/time win
    // over the fresh ones (--time only affects fresh worlds).
    MC_TRY(tryLoadSave());

    // Crop-growth tracking set (Phase 48): rebuilt from the world after
    // the save load (block edits restored the planted crops). A fresh
    // world scans to nothing — worldgen places no crops (v1).
    rescanCrops();
    // GEN-2: the sapling tracking set (the rescanCrops twin): a fresh
    // world scans to nothing — worldgen places no saplings (stateful
    // blocks are rejected at generator init, the ENT-2 invariant).
    rescanSaplings();
    rescanMoistureBlocks();
    rescanBeacons();
    rescanFlammableChunks();

    // The loaded columns mirror (streaming + unload decide on this set,
    // not per-frame chunk scans). The lighting pipeline has covered every
    // chunk that exists now (the fresh demo area, or the loaded save's
    // world — applySave ends with a full propagation); anything that
    // appears later is NEW (tracked by m_lightPendingColumns).
    for (const ChunkPos& c : m_world.chunkPositions()) {
        m_loadedColumns.insert({c.x, c.z});
    }

    // -- audio (Phase 15) ----------------------------------------------------
    // Graceful: with no audio device the bank stays disabled and every
    // play() is a no-op (the game is otherwise unaffected).
    m_sound.init(m_assets, "sounds",
                 m_config.getDouble("audio.volume", 0.7));

    MC_LOG_INFO("Scene built: " + std::to_string(m_blocks.count()) +
                " blocks, " + std::to_string(m_atlas.tileCount()) +
                " atlas tiles, " + std::to_string(m_world.chunkCount()) +
                " chunks (seed=" + std::to_string(m_generator->seed()) +
                ", spawn x=" + std::to_string(spawnX) + " z=" +
                std::to_string(spawnZ) + ", surface y=" +
                std::to_string(m_generator->surfaceHeight(spawnX, spawnZ)) +
                ", biome=" + m_generator->biomeNameAt(spawnX, spawnZ) + ")");
    return {};
}

// ---------------------------------------------------------------------------
// Chunk meshing (PIPE-1): the mesher's COMPUTE runs on workers (postMeshJobs
// snapshots a column's 3x3 neighborhood into a throwaway World; the worker
// runs the UNCHANGED ChunkMesher against it — face culling across chunk
// boundaries works identically because the snapshot IS the mesher's full
// read set); integrateMeshJobs lands the results after the pool flush and
// clears the dirty flags. The game thread keeps the column-merged GL
// upload (rebuildColumnMesh — the GL context is main-thread only).
// ---------------------------------------------------------------------------


Result<void> Game::updateChunkMeshes() {
    // The merged GL mesh of every column whose chunk meshes landed this
    // frame (integrateMeshJobs) or earlier (a column can be pending across
    // frames — its chunk data exists, the upload is what lags). Phase 99.9:
    // one draw per column instead of one per chunk. Chunks without CPU
    // geometry (unlit — their mesh job waits for the light — or not meshed
    // yet) are simply absent from the merged column mesh: the same
    // pop-in visibility the old main-thread budgeted path had.
    for (const ColumnPos& col : m_columnRebuildPending) {
        MC_TRY(rebuildColumnMesh(col));
    }
    m_columnRebuildPending.clear();
    return {};
}

void Game::integrateMeshJobs() {
    if (m_meshJobs.empty()) {
        return;
    }
    // Runs right after pool->flush() (happens-before via the pool's
    // mutex) and BEFORE integrateStreamedColumns: a stream insert marks
    // face-sharing neighbors dirty, and that re-dirtying must not be
    // swallowed by a mesh integrate's clear-dirty (it re-queues the
    // column with the new neighbor in the next frame's snapshot).
    // The mesh is a pure function of the job's snapshot, so FIFO
    // integration preserves the selection order (nearest first) —
    // deterministic, frame-locked.
    std::deque<std::unique_ptr<MeshJob>> inFlight;
    for (auto& job : m_meshJobs) {
        if (!job->done) {
            inFlight.push_back(std::move(job));
            continue;
        }
        for (std::size_t cy = 0; cy < job->meshes.size(); ++cy) {
            const ChunkPos cpos{job->column.x,
                                static_cast<int>(cy), job->column.z};
            m_chunkMeshData[cpos] = std::move(job->meshes[cy]);
            // Clear the dirty flag at INTEGRATE, not post: no main-thread
            // World write can land between this frame's post and the next
            // flush (the read-only worker window), and any edit AFTER
            // this integrate re-marks the chunk and re-queues the column
            // (self-healing).
            m_world.clearChunkDirty(cpos);
        }
        m_columnRebuildPending.insert(job->column);
        m_meshPending.erase(job->column);
        MC_LOG_DEBUG("Meshed column (" + std::to_string(job->column.x) +
                     ", " + std::to_string(job->column.z) + ") on worker");
    }
    m_meshJobs = std::move(inFlight);
}

Result<void> Game::rebuildColumnMesh(const ColumnPos& col) {
    // Concatenate the column's chunk geometries. Each chunk's Y offset
    // (its layer in the column) is baked into the vertex positions; the
    // X/Z column origin stays in the model matrix (columnModel) — baking
    // it there too would double-apply the translation. Chunks without
    // CPU geometry (unlit, or not meshed yet) are simply absent — the same
    // visibility the old per-chunk loop had.
    std::vector<MeshVertex> opaqueVerts;
    std::vector<std::uint32_t> opaqueIdx;
    // PRE-5: the third (cutout) bucket, merged like the other two.
    std::vector<MeshVertex> cutoutVerts;
    std::vector<std::uint32_t> cutoutIdx;
    std::vector<MeshVertex> transparentVerts;
    std::vector<std::uint32_t> transparentIdx;
    for (int cy = 0; cy < kVerticalChunksPerColumn; ++cy) {
        const ChunkPos cp{col.x, cy, col.z};
        const auto it = m_chunkMeshData.find(cp);
        if (it == m_chunkMeshData.end()) {
            continue;
        }
        const ChunkMeshData& d = it->second;
        const float oy = static_cast<float>(cy * kChunkSize);
        if (!d.opaqueEmpty()) {
            const std::uint32_t base =
                static_cast<std::uint32_t>(opaqueVerts.size());
            opaqueVerts.reserve(opaqueVerts.size() + d.opaqueVertices.size());
            for (const MeshVertex& v : d.opaqueVertices) {
                MeshVertex w = v;
                w.position.y += oy;
                opaqueVerts.push_back(std::move(w));
            }
            opaqueIdx.reserve(opaqueIdx.size() + d.opaqueIndices.size());
            for (const std::uint32_t i : d.opaqueIndices) {
                opaqueIdx.push_back(i + base);
            }
        }
        if (!d.cutoutEmpty()) {
            const std::uint32_t base =
                static_cast<std::uint32_t>(cutoutVerts.size());
            cutoutVerts.reserve(cutoutVerts.size() + d.cutoutVertices.size());
            for (const MeshVertex& v : d.cutoutVertices) {
                MeshVertex w = v;
                w.position.y += oy;
                cutoutVerts.push_back(std::move(w));
            }
            cutoutIdx.reserve(cutoutIdx.size() + d.cutoutIndices.size());
            for (const std::uint32_t i : d.cutoutIndices) {
                cutoutIdx.push_back(i + base);
            }
        }
        if (!d.transparentEmpty()) {
            const std::uint32_t base =
                static_cast<std::uint32_t>(transparentVerts.size());
            transparentVerts.reserve(
                transparentVerts.size() + d.transparentVertices.size());
            for (const MeshVertex& v : d.transparentVertices) {
                MeshVertex w = v;
                w.position.y += oy;
                transparentVerts.push_back(std::move(w));
            }
            transparentIdx.reserve(
                transparentIdx.size() + d.transparentIndices.size());
            for (const std::uint32_t i : d.transparentIndices) {
                transparentIdx.push_back(i + base);
            }
        }
    }
    ColumnMesh& cm = m_columnMeshes[col];
    cm.opaque.reset();
    cm.cutout.reset();
    cm.transparent.reset();
    if (!opaqueIdx.empty()) {
        cm.opaque.emplace();
        auto r = cm.opaque->upload(opaqueVerts, opaqueIdx);
        MC_TRY(r);
    }
    if (!cutoutIdx.empty()) {
        cm.cutout.emplace();
        auto r = cm.cutout->upload(cutoutVerts, cutoutIdx);
        MC_TRY(r);
    }
    if (!transparentIdx.empty()) {
        cm.transparent.emplace();
        auto r = cm.transparent->upload(transparentVerts, transparentIdx);
        MC_TRY(r);
    }
    return {};
}
// ---------------------------------------------------------------------------
// Chunk streaming (Phase 14): worker-generated columns around the player
// ---------------------------------------------------------------------------

void Game::integrateStreamedColumns() {
    if (m_streamJobs.empty()) {
        return;
    }
    // Runs right after pool->flush(): completed jobs are fully published
    // (happens-before via the pool's mutex). insertChunk marks the chunk
    // and its face-sharing neighbors dirty, so updateChunkMeshes (later
    // this frame) re-meshes them with correct boundary culling.
    std::deque<std::unique_ptr<StreamJob>> inFlight;
    for (auto& job : m_streamJobs) {
        if (!job->done) {
            inFlight.push_back(std::move(job));
            continue;
        }
        for (int cy = 0; cy < kVerticalChunksPerColumn; ++cy) {
            const ChunkPos cpos {job->column.x, cy, job->column.z};
            m_world.insertChunk(cpos, std::move(job->chunks[cy]));
            // Burn-pass gate (Phase 99.8): a freshly generated column can
            // carry flamables (wood, leaves, grass) — note the chunks.
            const Chunk* c = m_world.findChunk(cpos);
            if (c != nullptr) {
                const BlockID* blocks = c->data();
                for (int i = 0; i < Chunk::kVolume; ++i) {
                    if (m_blocks.at(blocks[i]).properties.flamable) {
                        m_flammableChunks.insert(cpos);
                        break;
                    }
                }
            }
        }
        m_streamPending.erase(job->column);
        m_loadedColumns.insert(job->column);
        // New terrain needs light propagation (the old m_litChunks full
        // scan detected it; the explicit column set replaces it) — and
        // stays UNMESHED until that light lands (m_unlitColumns: the
        // chunk meshes once, lit, instead of dark-then-re-meshed).
        m_lightPendingColumns.insert(job->column);
        m_unlitColumns.insert(job->column);
        // STRM-2: re-apply this column's retained diff (the session
        // changes it held before an earlier unload — the pure-fluid
        // churn, the fire burn, the water-lava reaction: the writes that
        // bypass the game's setBlock choke point and so never earned the
        // m_editedColumns protection). BLOCKS FIRST, then fluids — the
        // applyWorldEdits order (a block displaces the cell's fluid, so a
        // re-applied placement re-drains the generated fluid before the
        // fluid edits land; a level-0 fluid edit on an already-dry cell
        // is the expected no-op). The observer re-journals every write:
        // the column is in the journal again and out of the retained
        // map — the one-of invariant, and the next unload snapshots it
        // again (the self-similar loop). Deliberately NOT inserted into
        // m_editedColumns: that would keep the column's chunks resident
        // for the whole session (the memory regression this feature
        // replaces) — a fluid-only column must stay unloadable.
        // Lighting: the column just joined m_lightPendingColumns /
        // m_unlitColumns above (new terrain, the forced propagation), so
        // this frame's postLightJobs covers the re-applied cells and the
        // meshes form once, lit (no lit-pop-in, the 99.4 discipline).
        if (auto rit = m_retainedColumns.find(job->column);
            rit != m_retainedColumns.end()) {
            for (const SavedBlockEdit& e : rit->second.blocks) {
                const BlockID id = m_blocks.idForName(e.blockName);
                if (!id.valid()) {
                    // Structurally unreachable (same session, same
                    // registry as the snapshot — the name resolved when
                    // the diff was produced): name it and skip the ONE
                    // edit — never crash, never drop the column
                    // silently.
                    MC_LOG_ERROR("STRM-2: retained block edit at (" +
                                 std::to_string(e.pos.x) + ", " +
                                 std::to_string(e.pos.y) + ", " +
                                 std::to_string(e.pos.z) +
                                 ") names unknown block '" + e.blockName +
                                 "' — skipped");
                    continue;
                }
                // The cell's y was valid when the write was journal-
                // recorded, and the id is valid: setBlock cannot fail.
                m_world.setBlock(e.pos.x, e.pos.y, e.pos.z, id);
            }
            for (const SavedFluidEdit& e : rit->second.fluids) {
                // A positive level targets the cell the block pass left
                // (air — a cell holds fluid XOR a block), so setFluid
                // cannot fail for it; the level-0 drain on an already-
                // dry cell is the expected false no-op.
                m_world.setFluid(e.pos.x, e.pos.y, e.pos.z, e.kind, e.level);
            }
            m_retainedColumns.erase(rit);
        }
        // Per-column detail at DEBUG (a radius-80 session logs 25921 of
        // these lines; the periodic INFO progress line in streamChunks is
        // the human-readable signal).
        MC_LOG_DEBUG("Streamed column (" + std::to_string(job->column.x) +
                     ", " + std::to_string(job->column.z) +
                     ") integrated (" + std::to_string(m_world.chunkCount()) +
                     " chunks loaded)");
    }
    m_streamJobs = std::move(inFlight);
}

void Game::collectLitColumns() {
    if (m_lightJobs.empty()) {
        return;
    }
    // Runs right after pool->flush(): done jobs have finished writing
    // light (happens-before via the pool's mutex), so their columns can
    // be meshed this frame with the propagated light. Undone jobs stay
    // in the deque (their columns stay unlit/unmeshed).
    std::deque<std::unique_ptr<LightJob>> inFlight;
    for (auto& job : m_lightJobs) {
        if (!job->done) {
            inFlight.push_back(std::move(job));
            continue;
        }
        for (const ColumnPos& col : job->columns) {
            if (job->fluid) {
                // 99.11: the fluid catch-up's bookkeeping. The column
                // already left m_fluidLightPending at POST time (the
                // budget must not re-take an in-flight column) — and
                // this frame's fluid tick runs AFTER this reap, so no
                // re-churn can have re-added it: the erase is the
                // contract belt (a processed column never lingers).
                // Fluid columns are NOT unlit columns (they were lit
                // long before the fluid touched them): m_unlitColumns —
                // the streaming lit-gate — is not touched.
                m_fluidLightPending.erase(col);
            } else {
                m_unlitColumns.erase(col);
            }
        }
        // 99.10: apply the workers' net-changed chunks to the World's
        // dirty set HERE (the main-thread-writer window — no worker is
        // in flight): the jobs collected them job-locally, because two
        // parallel light jobs would race on the shared set. 99.11:
        // markChunkDirty silently skips a missing chunk (World
        // contract) — the defensive path for a fluid job whose column
        // was unloaded in the meantime (timing keeps it unreachable:
        // the flush barrier completes every job before this frame's
        // unloadFarColumns runs).
        for (const ChunkPos& pos : job->dirtyChunks) {
            m_world.markChunkDirty(pos);
        }
    }
    m_lightJobs = std::move(inFlight);
}

void Game::unloadFarColumns() {
    if (!m_player) {
        return;
    }
    // Safe point: the pool flushed at the top of update and the posts run
    // at its end — no worker touches the World while this erases (the
    // main-thread-writer window, same as integrateStreamedColumns).
    const Vec3 p = m_player->position();
    const ColumnPos player{
        floorDiv(static_cast<int>(std::floor(p.x)), kChunkSize),
        floorDiv(static_cast<int>(std::floor(p.z)), kChunkSize)};
    const int keepRadius = std::min(m_streamRadius, kStreamCapRadius);
    const int keepDist = keepRadius + kUnloadHysteresis;
    // The unload predicate is squared Euclidean (STRM-1): a column
    // survives while its distanceSq is <= (keepRadius + 1)² — the +1
    // hysteresis band around the capped disk.
    const std::int64_t keepDistSq =
        static_cast<std::int64_t>(keepDist) * keepDist;
    // The per-block state maps (furnace/campfire/table/crop/beacon/fire)
    // are keyed by BlockPos: their COLUMNS must survive (the save records
    // the states and cross-checks them against the block edits — an
    // unloaded column would break the load). The maps are player-scale
    // (small).
    std::set<ColumnPos> protectedColumns = m_editedColumns;
    protectedColumns.insert(m_streamPending.begin(), m_streamPending.end());
    // PIPE-1: in-flight MESH columns too (same paradigm — their
    // integrate must find the column still loaded; unloading it would
    // strand the 8 ChunkMeshData outputs in m_chunkMeshData).
    protectedColumns.insert(m_meshPending.begin(), m_meshPending.end());
    const auto protectStates = [this, &protectedColumns](
                                   const auto& states) {
        for (const auto& [pos, st] : states) {
            protectedColumns.insert({
                floorDiv(pos.x, kChunkSize), floorDiv(pos.z, kChunkSize)});
        }
    };
    protectStates(m_furnaces);
    protectStates(m_campfires);
    protectStates(m_tables);
    protectStates(m_crops);
    protectStates(m_saplings);
    protectStates(m_beacons);
    protectStates(m_fires);

    std::vector<ChunkPos> toErase;
    // The loaded COLUMNS (the incremental mirror), not a full chunk scan:
    // a full-ring world is ~9k-200k chunks, the column set is ~2k-26k.
    for (const ColumnPos& col : m_loadedColumns) {
        if (columnDistanceSq(player, col) <= keepDistSq) {
            continue; // inside the loaded disk (1-column hysteresis band)
        }
        // The 4x4 demo area is protected: respawn teleports to the spawn
        // point inside it (an unloaded spawn column would read void).
        if (col.x >= kDemoChunkMin && col.x <= kDemoChunkMax &&
            col.z >= kDemoChunkMin && col.z <= kDemoChunkMax) {
            continue;
        }
        if (protectedColumns.count(col) > 0) {
            continue; // player edits / tracked state / in-flight job
        }
        for (int cy = 0; cy < kVerticalChunksPerColumn; ++cy) {
            toErase.push_back({col.x, cy, col.z});
        }
    }
    if (toErase.empty()) {
        return;
    }
    // The unloaded COLUMNS (toErase holds all eight chunks of each — the
    // merged column mesh and every member's CPU geometry go with them).
    std::set<ColumnPos> columns;
    for (const ChunkPos& pos : toErase) {
        columns.insert({pos.x, pos.z});
    }
    // STRM-2: snapshot the journal diff (live != fresh, restricted to the
    // column's journal cells) of every unloaded column BEFORE its chunks
    // are erased — the snapshot reads the live chunks. A column with no
    // journal entries in EITHER table costs two map finds and nothing
    // else (the structural fast path: most unloaded columns are pristine
    // generated terrain); an all-revert column yields an empty diff and
    // is not stored. On re-stream the diff is re-applied
    // (integrateStreamedColumns) and merged into the exit save
    // (collectSave) — the one-of invariant with the journal below is
    // established HERE: the snapshot precedes the eviction.
    for (const ColumnPos& col : columns) {
        const auto jb = m_saveJournal.blocks.find(col);
        const auto jf = m_saveJournal.fluids.find(col);
        if (jb == m_saveJournal.blocks.end() &&
            jf == m_saveJournal.fluids.end()) {
            continue;
        }
        RetainedColumnDiff diff = retainColumnDiff(
            m_world, makeChunkGenerator(), m_blocks, col,
            jb == m_saveJournal.blocks.end() ? nullptr : &jb->second,
            jf == m_saveJournal.fluids.end() ? nullptr : &jf->second);
        if (!diff.blocks.empty() || !diff.fluids.empty()) {
            m_retainedColumns.emplace(col, std::move(diff));
        }
    }
    for (const ChunkPos& pos : toErase) {
        m_chunkMeshData.erase(pos);
        m_world.eraseChunk(pos); // marks the boundary neighbors dirty
        m_flammableChunks.erase(pos);
    }
    for (const ColumnPos& col : columns) {
        m_columnMeshes.erase(col); // frees the GL buffers (Mesh destructor)
        m_columnRebuildPending.erase(col);
        // PIPE-2 / STRM-2: the journal entries of an unloaded column go
        // with it — the session's diff they carried was just snapshotted
        // into m_retainedColumns above (when non-empty), so nothing is
        // lost: the column re-enters as FRESH streamed terrain plus its
        // retained diff.
        m_saveJournal.evictColumn(col);
    }
    for (const ChunkPos& pos : toErase) {
        m_loadedColumns.erase({pos.x, pos.z});
        // Defensive: an unlit column cannot be unloaded (it is either
        // inside the ring, edited, or in flight — all protected above),
        // but the set must not leak entries for dead columns.
        m_unlitColumns.erase({pos.x, pos.z});
        // 99.11: the fluid catch-up pending set must not accumulate
        // entries for dead columns either (a coast tick's changed cell
        // can outlive its column's unload window while the budget lags
        // behind the churn): evict them here, the same no-leak belt. An
        // in-flight fluid job of an unloaded column cannot exist at
        // this point (the flush barrier completed it before the reap
        // above); its reap applied dirtyOut while the chunks still
        // existed, and markChunkDirty skips any that did not.
        m_fluidLightPending.erase({pos.x, pos.z});
    }
    m_unloadedColumnsTotal +=
        static_cast<std::size_t>(toErase.size() / kVerticalChunksPerColumn);
    // Per-event detail at DEBUG (walking at radius 80 produces one of
    // these every ~4 s; the periodic INFO progress line carries the
    // count).
    MC_LOG_DEBUG("Unloaded " +
                 std::to_string(toErase.size() / kVerticalChunksPerColumn) +
                 " far columns (" + std::to_string(m_world.chunkCount()) +
                 " chunks loaded)");
}

void Game::streamChunks() {
    if (!m_pool || !m_generator || !m_player) {
        return;
    }
    // While the player is dead the body is frozen and the death screen
    // owns the input: stop requesting new columns. The radius disk
    // around the corpse would otherwise keep filling to the capped view
    // distance (the radius-64 disk holds 12853 columns ≈ 103k chunks:
    // hours of streaming and several GB of allocations behind the death
    // veil). In-flight jobs
    // (at most kStreamColumnsPerFrame) still drain through the
    // post->flush window and get lit by postLightJobs as usual, and the
    // preserved hysteresis state resumes the fill on respawn (R).
    if (m_player->isDead()) {
        return;
    }
    // The player's column (bottom-center position).
    const Vec3 p = m_player->position();
    const ColumnPos player{
        floorDiv(static_cast<int>(std::floor(p.x)), kChunkSize),
        floorDiv(static_cast<int>(std::floor(p.z)), kChunkSize)};

    // The loaded columns (the incremental mirror built by buildScene /
    // integrate / unload / first-touch creation — the old code rebuilt
    // this from a full per-frame chunk scan + sort).
    const std::vector<ColumnPos> loaded(m_loadedColumns.begin(),
                                        m_loadedColumns.end());

    // Hysteresis (see chunk_streaming): engage when the player reaches
    // the edge of the loaded area; while engaged, keep filling the full
    // radius disk (an exploring player ends up with the capped
    // view_distance of terrain around them); disengage only when the
    // disk around the player is fully loaded (nearestMissingDistanceSq
    // returns the (radius + 1)² sentinel). A never-engaged idle player
    // in the center of the loaded area stays job-free (keeps headless
    // baselines byte-reproducible). The effective radius is the config
    // clamped to the far-plane cap (kStreamCapRadius); m_streamRadius
    // itself keeps the raw config value for the logs and the F3 overlay.
    const int radius = std::min(m_streamRadius, kStreamCapRadius);
    const std::int64_t nearestMissingSq =
        nearestMissingDistanceSq(player, loaded, radius);
    if (!m_streamingEngaged) {
        const std::int64_t triggerSq = static_cast<std::int64_t>(
            kStreamTriggerDistance) * kStreamTriggerDistance;
        if (nearestMissingSq > triggerSq) {
            return;
        }
        m_streamingEngaged = true;
        MC_LOG_INFO("Chunk streaming engaged (radius " +
                    std::to_string(m_streamRadius) + " columns)");
    } else if (nearestMissingSq >= missingSentinelDistanceSq(radius)) {
        m_streamingEngaged = false; // the radius disk is fully loaded
        // One explicit completion line: after this, streaming is quiet
        // (no per-frame work) until the player reaches the loaded edge
        // again — silence in the log = the disk is full, not a stall.
        MC_LOG_INFO("Streaming complete: " +
                    std::to_string(m_loadedColumns.size()) + "/" +
                    std::to_string(diskColumnCount(radius)) +
                    " columns (" + std::to_string(m_world.chunkCount()) +
                    " chunks, " +
                    std::to_string(m_unloadedColumnsTotal) +
                    " unloaded) — quiet until the player reaches the "
                    "loaded edge");
    }

    // Backlog cap: the per-frame flush() at the top of update waits for
    // EVERY in-flight pool job, so an unbounded queue makes the main
    // thread block longer and longer (FPS decay while the ring fills).
    // Generation is the heavy post; the per-frame lighting jobs (<= 2,
    // 99.10) stay uncapped (one box each — cheap per integrated column).
    // Skipping the post this frame (not the column) is safe: the loop
    // re-runs next frame and m_streamPending only holds actually-posted
    // columns.
    if (m_pool->pendingCount() >= kMaxStreamBacklog) {
        return;
    }
    int posted = 0;
    const std::vector<ColumnPos> missing =
        missingColumns(player, loaded, radius);
    for (const ColumnPos& col : missing) {
        if (posted >= kStreamColumnsPerFrame) {
            break;
        }
        if (m_streamPending.count(col) > 0) {
            continue; // a generation job for this column is in flight
        }
        auto job = std::make_unique<StreamJob>();
        job->column = col;
        job->chunks = std::make_unique<Chunk[]>(kVerticalChunksPerColumn);
        m_streamPending.insert(col);
        StreamJob* rawJob = job.get();
        m_streamJobs.push_back(std::move(job));
        // The worker fills the job's own scratch chunks with the pure
        // generator (no World access, no shared mutable state — the
        // generator's members are immutable after create()).
        m_pool->post([this, rawJob] {
            for (int cy = 0; cy < kVerticalChunksPerColumn; ++cy) {
                m_generator->generateChunk(rawJob->chunks[cy],
                                           {rawJob->column.x, cy,
                                            rawJob->column.z});
            }
            rawJob->done = true;
        });
        ++posted;
    }
    // Periodic INFO progress (the per-column lines are DEBUG): every
    // 300 frames while streaming is engaged — the human-readable
    // "is it still loading / did it stop" signal for a long session.
    // Frame-based (deterministic cadence).
    if (m_frameCount % 300 == 0) {
        MC_LOG_INFO("Streaming: " +
                    std::to_string(m_loadedColumns.size()) + "/" +
                    std::to_string(diskColumnCount(radius)) +
                    " columns (" + std::to_string(m_world.chunkCount()) +
                    " chunks, " +
                    std::to_string(m_unloadedColumnsTotal) +
                    " unloaded since session start)");
    }
}

void Game::postLightJobs() {
    if (!m_pool) {
        return;
    }
    // Columns that appeared since the last post (integrated at the top of
    // update, or first-touch completed by the setBlock choke point).
    // Everything else is already covered by the lighting pipeline (init/
    // load propagateAll + local updates). The old code found these by a
    // full per-frame chunk scan against m_litChunks; the explicit pending
    // set replaces it. Columns are complete (the ensureColumn invariant),
    // so each box is the full column.
    if (m_lightPendingColumns.empty()) {
        return;
    }
    // 99.10 (hard rule 7, revised): up to TWO lighting jobs per frame —
    // and only when the two pending columns' light regions are bit-level
    // independent (planLightJobs: the WRITE footprint is the column box
    // ±kLightUpdateRadius = ±1 column, and the block-light BFS READS one
    // more column ring as its boundary seed, so a column gap >=
    // kLightParallelColumnGap on either axis separates write from write
    // AND write from read). Two independent jobs run in parallel on two
    // workers and touch DISJOINT chunks only — the post->flush window
    // contract holds (the main thread only reads the World in it, and no
    // other job writes the live World after the mesh snapshots). The
    // confluence half: independent regions cannot influence each other
    // in either order, so the final light is a pure function of the
    // pre-insertion world regardless of which job finishes first.
    //
    // A dependent pair (or 3+ pending columns) posts ONE job running its
    // boxes sequentially in set order — the pre-99.10 behavior,
    // bit-identical (the latent sky-rescan clobber of OVERLAPPING
    // regions is unchanged by design — LGT-1 disposition, its fix left
    // for 100+).
    //
    // The jobs collect their net-changed chunk positions in
    // LightJob::dirtyChunks instead of calling World::markChunkDirty:
    // two parallel markChunkDirty calls would race on the World's shared
    // dirty set; collectLitColumns applies them at the next flush (the
    // main-thread-writer window). The job also carries its COLUMNS and a
    // done flag: at the next flush, collectLitColumns drops them from
    // m_unlitColumns so their chunks get a mesh job once, lit (PIPE-1
    // selection rule).
    const LightJobPlan plan = planLightJobs(m_lightPendingColumns);
    m_lightPendingColumns.clear();
    for (const std::vector<ColumnPos>& columns : plan.jobs) {
        auto job = std::make_unique<LightJob>();
        job->columns = std::set<ColumnPos>(columns.begin(), columns.end());
        job->boxes.reserve(columns.size());
        for (const ColumnPos& col : columns) {
            job->boxes.push_back(columnLightBox(col));
        }
        LightJob* rawJob = job.get();
        m_lightJobs.push_back(std::move(job));
        m_pool->post([this, rawJob] {
            for (const auto& box : rawJob->boxes) {
                // newTerrain = true: the boxes cover freshly inserted
                // columns — the sky propagation must run even when the
                // rescan writes nothing (a column born dark still
                // receives light from its lit neighbors). dirtyOut: the
                // net change is collected job-locally (see above).
                Lighting::updateRegion(m_world, m_blocks, box[0], box[1],
                                       true, &rawJob->dirtyChunks);
            }
            rawJob->done = true;
        });
    }
}

void Game::postFluidLightJobs() {
    if (!m_pool) {
        return;
    }
    if (m_fluidLightPending.empty()) {
        return; // a settled world: the fast exit (structural immunity)
    }
    // The streaming job columns of THIS window (postLightJobs ran
    // earlier in the frame; the previous window's jobs — of either kind
    // — were reaped at the top after the flush barrier, so nothing is in
    // flight when this runs). Window exclusivity (see the header
    // comment + the 99.11 note in world/light_job.hpp): a fluid column
    // whose light region is not lightRegionsIndependent of one of these
    // columns is skipped this frame — it stays pending and is retried
    // next frame. The 99.10 parallel safety is per-plan; a shared
    // window would let two workers write one chunk's light arrays.
    std::set<ColumnPos> streaming;
    for (const std::unique_ptr<LightJob>& job : m_lightJobs) {
        if (!job->fluid) {
            streaming.insert(job->columns.begin(), job->columns.end());
        }
    }
    const LightJobPlan plan =
        planFluidLightJobs(m_fluidLightPending, streaming);
    if (plan.jobs.empty()) {
        return; // every candidate was blocked: nothing to post
    }
    // The taken columns leave the pending set at POST time — before the
    // jobs run — so the budget never re-takes an in-flight column. A
    // re-churn of the column (the next fluid tick) re-adds it after the
    // job lands: a new world state, a new re-light, correctly sequenced
    // by the flush barrier.
    for (const std::vector<ColumnPos>& columns : plan.jobs) {
        for (const ColumnPos& col : columns) {
            m_fluidLightPending.erase(col);
        }
    }
    for (const std::vector<ColumnPos>& columns : plan.jobs) {
        auto job = std::make_unique<LightJob>();
        job->fluid = true;
        job->columns = std::set<ColumnPos>(columns.begin(), columns.end());
        job->boxes.reserve(columns.size());
        for (const ColumnPos& col : columns) {
            job->boxes.push_back(columnLightBox(col));
        }
        LightJob* rawJob = job.get();
        m_lightJobs.push_back(std::move(job));
        m_pool->post([this, rawJob] {
            for (const auto& box : rawJob->boxes) {
                // newTerrain = true: unlike the old synchronous fluid
                // re-light (default false), the catch-up forces the sky
                // propagation even when the rescan writes nothing — the
                // 99.9 no-op skip would strand a fluid-opened horizontal
                // light path dark (a cave film's vertical values are
                // unchanged while its attenuation path changes). The
                // streaming jobs pay the same cost (99.10).
                // dirtyOut: job-local net-changed collection — the fluid
                // columns' chunks are re-meshed with the new light at
                // the next flush (collectLitColumns), the 99.4
                // lit-pop-in discipline.
                Lighting::updateRegion(m_world, m_blocks, box[0], box[1],
                                       true, &rawJob->dirtyChunks);
            }
            rawJob->done = true;
        });
    }
}

void Game::postMeshJobs() {
    // PIPE-1: worker meshing. Runs in update AFTER streamChunks (gen
    // posts) and BEFORE the light posts (99.11: postLightJobs THEN
    // postFluidLightJobs) — the post order gen -> mesh -> light is the
    // contract: after this function's posts, the ONLY jobs that write
    // the live World in the coming window are this frame's lighting
    // jobs (streaming + fluid catch-up), and nextMeshColumns's
    // exclusions (unlit + its footprint) keep every snapshot's light
    // final for the streaming ones; a fluid re-light of a column whose
    // mesh was snapshotted this frame re-marks the chunk dirty through
    // its dirtyOut at the next flush, so the mesh re-queues after it
    // (self-healing — one frame of stale light, the 99.4 pattern). The
    // snapshot is taken NOW — after every main-thread World write of
    // this frame (interaction, fluids, fire, growth), before the
    // player's next move — so it is the frame's final block/fluid
    // state (a pure function of the World at this point).
    if (!m_pool) {
        return;
    }
    const std::set<ChunkPos>& dirtySet = m_world.dirtyChunks();
    if (dirtySet.empty()) {
        return;
    }
    // The column projection of the dirty set (the set is sorted -> the
    // projection is deterministic before the distance sort).
    std::set<ColumnPos> dirtyColumns;
    for (const ChunkPos& pos : dirtySet) {
        dirtyColumns.insert({pos.x, pos.z});
    }
    if (dirtyColumns.empty()) {
        return;
    }
    const Vec3 p =
        m_player ? m_player->position() : Vec3{0.0, 0.0, 0.0};
    const ColumnPos player{
        floorDiv(static_cast<int>(std::floor(p.x)), kChunkSize),
        floorDiv(static_cast<int>(std::floor(p.z)), kChunkSize)};
    const std::vector<ColumnPos> chosen = nextMeshColumns(
        dirtyColumns, player, m_unlitColumns, m_lightPendingColumns,
        m_meshPending, kMaxMeshBacklog);
    for (const ColumnPos& col : chosen) {
        auto job = std::make_unique<MeshJob>();
        job->column = col;
        job->meshes.resize(kVerticalChunksPerColumn);
        // The snapshot: the column's 3x3 column neighborhood, all layers
        // (the mesher's full read set — its faces cull and bake light
        // against at most the 6 neighbor columns' cells). Missing
        // neighbors stay missing: the snapshot World's void reads match
        // the live world's bit-for-bit (the same World class).
        // insertSnapshotChunk carries the cell content + light + fluid +
        // stair facing with no live-World bookkeeping (the snapshot's
        // indexes are never consulted).
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dz = -1; dz <= 1; ++dz) {
                for (int cy = 0; cy < kVerticalChunksPerColumn; ++cy) {
                    const ChunkPos cp{col.x + dx, cy, col.z + dz};
                    const Chunk* src = m_world.findChunk(cp);
                    if (src != nullptr) {
                        job->snap.insertSnapshotChunk(cp, *src);
                    }
                }
            }
        }
        m_meshPending.insert(col);
        MeshJob* rawJob = job.get();
        m_meshJobs.push_back(std::move(job));
        // The worker meshes the job's OWN snapshot (no live World touch —
        // the generation-job pattern). m_blocks / m_atlas are immutable
        // after init (the render pass reads them concurrently). The chunk
        // mesh is a pure function of the snapshot, so the result is
        // deterministic (frame-locked FIFO integration).
        m_pool->post([this, rawJob] {
            const ChunkMesher mesher(rawJob->snap, m_blocks,
                                     m_atlas.layout());
            for (std::size_t cy = 0; cy < rawJob->meshes.size(); ++cy) {
                rawJob->meshes[cy] = mesher.mesh({rawJob->column.x,
                                                  static_cast<int>(cy),
                                                  rawJob->column.z});
            }
            rawJob->done = true;
        });
    }
}

// ---------------------------------------------------------------------------
// Save / load (Phase 13): seed + modifications + full game state
// ---------------------------------------------------------------------------

World::ChunkGenerator Game::makeChunkGenerator() const {
    // The generator is stateless and only writes into the given chunk
    // (never back into the World), which the World contract requires.
    // The SAME callback serves world creation and the save diff reference.
    return [this](Chunk& chunk, const ChunkPos& pos) {
        m_generator->generateChunk(chunk, pos);
    };
}

std::filesystem::path Game::savePath() const {
    return "data/saves/" + *m_args.saveSlot + ".json";
}

Result<SaveGame> Game::collectSave() const {
    SaveGame save;
    save.seed = m_generator->seed();
    save.time = m_dayCycle.timeOfDay();
    save.playerPos = m_player->position();
    // SAVE-2 (format 22): the player's velocity — the game always has
    // the real value ([0,0,0] at rest: the ENT-7 stop epsilon makes
    // rest exactly zero, so a no-input save carries the zero triple).
    save.playerVelocity = m_player->velocity();
    save.playerYaw = m_player->yaw;
    save.playerPitch = m_player->pitch;
    save.playerHealth = m_player->health();
    // Phase 72: the absorption pool (format 12; 0 when no layer is
    // active — the game never writes a pool without its effects).
    save.playerAbsorption = m_player->absorption();
    save.playerHunger = m_player->hunger();
    save.playerSaturation = m_player->saturation();
    // SURV-1: the equipped armor (format 16) — the piece names by slot
    // ("" = empty; the count is always 1 by the armor items' max_stack,
    // so the name is the whole state). SURV-3 (format 18): the pieces'
    // accumulated damage by slot (0 = full — the "damage" key is
    // written only when > 0, which flips the entry to its object shape).
    for (int slot = 0; slot < kArmorSlotCount; ++slot) {
        const ItemStack& worn = m_player->armorSlot(slot);
        save.playerArmor[static_cast<std::size_t>(slot)] =
            worn.empty() ? std::string {} : m_items.at(worn.item).name;
        save.playerArmorDamage[static_cast<std::size_t>(slot)] =
            worn.empty() ? 0.0f : worn.damage;
    }
    // SURV-2: the player's oxygen (format 17, always written — the air
    // units left, kOxygenMax while breathing). The drownTicks
    // accumulator is deliberately NOT carried (it restarts at 0 on
    // load — the tick-phase convention, the effects pattern).
    save.playerOxygen = m_drown.oxygen;
    save.selectedSlot = static_cast<int>(m_selectedSlot);
    save.slots.resize(m_inventory.size());
    for (std::size_t i = 0; i < save.slots.size(); ++i) {
        save.slots[i] = m_inventory.at(i);
    }
    // The crafting grids (format 3): items left in a grid are the player's
    // — saving them keeps a mid-craft state loadable. CRAFT-1: the E
    // panel's grid is the only global one — the table's grids are
    // per-block (the "tables" section below).
    save.invGrid.resize(CraftingGrid::kSize);
    for (int i = 0; i < CraftingGrid::kSize; ++i) {
        save.invGrid[static_cast<std::size_t>(i)] = m_inventoryGrid.at(i);
    }
    // The per-block furnace states (format 7; Phase 44): map iteration is
    // position-sorted (the save's order); every furnace block has an entry
    // (the setBlock invariant), empty states included.
    for (const auto& [pos, st] : m_furnaces) {
        save.furnaces.push_back({pos, st});
    }
    // The burning fire blocks (format 10; Phase 64): positions + the
    // real-dt seconds remaining (the map is position-sorted — the
    // serializer's write order; a reload continues burning).
    for (const auto& [pos, remaining] : m_fires) {
        save.fires.push_back(SavedFire {pos, remaining});
    }
    // The campfire cooking states (format 11; Phase 71): map iteration is
    // position-sorted (the save's order); every campfire block has an
    // entry (the setBlock invariant), empty states included.
    for (const auto& [pos, st] : m_campfires) {
        save.campfires.push_back({pos, st});
    }
    // The per-block crafting-table grids (format 19; CRAFT-1): map
    // iteration is position-sorted (the save's order); every
    // crafting_table block has an entry (the setBlock invariant), empty
    // grids included.
    for (const auto& [pos, grid] : m_tables) {
        save.tables.push_back(SavedTable {pos, grid});
    }
    // PIPE-2: the journal path is the default; it falls back to the
    // legacy full-world path when the journal is invalid (the
    // kSaveJournalMaxEntries cap was exceeded — logged at the crossing)
    // or --save-full-diff was given (the in-process equivalence oracle
    // and escape hammer). Both paths produce the same file format.
    const bool journalSave = m_saveJournal.valid && !m_args.saveFullDiff;
    // The per-instance block states (format 15; Phase 94 stair facing,
    // generalized in ENT-2).
    if (journalSave) {
        // PIPE-2: derived from the JOURNAL instead of the O(N) world
        // scan: worldgen places no stateful block (the init cross-check in
        // WorldGenerator::create + the content test pin it), so every
        // stateful cell was block-edited this session — it is in the block
        // journal. The live state read + the sentinel fallback are the same
        // logic the old scan had; a journal cell whose live block is no
        // longer stateful (broken again) is skipped, exactly as the old
        // scan's hasState gate. STRM-2: the retained (unloaded) columns'
        // block edits are never stateful either — a stateful placement
        // goes through the setBlock choke point (m_editedColumns
        // protection: the column never unloads) and the sapling stage
        // writes track m_saplings (protected the same way) — so the
        // journal-only derivation stays complete.
        std::vector<SavedState> states;
        for (const auto& entry : m_saveJournal.blocks) {
            for (const BlockPos& p : entry.second) {
                const BlockID id = m_world.getBlock(p.x, p.y, p.z);
                if (id == BlockID::air() || !m_blocks.at(id).hasState()) {
                    continue;
                }
                const std::uint8_t state =
                    m_world.getBlockState(p.x, p.y, p.z);
                // The state is always set for a placed stateful block (the
                // setBlock invariant); the sentinel (unset) is a
                // hand-edited stateful block in a format-13 file — fall
                // back to the block's state default.
                const std::uint8_t s =
                    (state != Chunk::kBlockStateUnset)
                        ? state
                        : static_cast<std::uint8_t>(
                              m_blocks.at(id).stateDefault());
                states.push_back(SavedState {p, s});
            }
        }
        std::sort(states.begin(), states.end(),
                  [](const SavedState& a, const SavedState& b) {
                      return a.pos < b.pos;
                  });
        save.states = std::move(states);
    } else {
        // Legacy (pre-PIPE-2): scan the world for the stateful cells (the
        // block hasState()) + record their per-instance state (the World's
        // per-cell byte — the project's first per-block state).
        // Position-sorted (the chunk/scan order). Every stateful block has
        // an entry (the setBlock invariant sets the state on placement).
        for (const ChunkPos& cp : m_world.chunkPositions()) {
            const Chunk* chunk = m_world.findChunk(cp);
            if (chunk == nullptr) {
                continue;
            }
            const int bx = cp.x * kChunkSize;
            const int by = cp.y * kChunkSize;
            const int bz = cp.z * kChunkSize;
            for (int ly = 0; ly < kChunkSize; ++ly) {
                for (int lz = 0; lz < kChunkSize; ++lz) {
                    for (int lx = 0; lx < kChunkSize; ++lx) {
                        const BlockID id = chunk->get(lx, ly, lz);
                        if (id == BlockID::air() ||
                            !m_blocks.at(id).hasState()) {
                            continue;
                        }
                        const std::uint8_t state =
                            chunk->stateAt(lx, ly, lz);
                        const BlockPos p {bx + lx, by + ly, bz + lz};
                        // The state is always set for a placed stateful
                        // block (the setBlock invariant); the sentinel
                        // (unset) is a hand-edited stateful block in a
                        // format-13 file — fall back to the block's state
                        // default.
                        const std::uint8_t s =
                            (state != Chunk::kBlockStateUnset)
                                ? state
                                : static_cast<std::uint8_t>(
                                      m_blocks.at(id).stateDefault());
                        save.states.push_back(SavedState {p, s});
                    }
                }
            }
        }
    }
    // The player's active status effects (format 8; Phase 46): names +
    // the real-dt seconds remaining (the serializer name-sorts; the tick
    // phase is deliberately not saved — a reload restarts the next tick
    // at the full interval).
    for (const ActiveEffect& a : m_playerEffects.list()) {
        // SURV-6: the LIVE instance's tier rides the save (the
        // aggregate's 3rd field — an active tier-1 effect saved as
        // tier 0 would silently downgrade on load; the gate-133 S2b
        // final save asserts the tier round trip).
        save.playerEffects.push_back(
            SavedEffect {m_effects.at(a.effect).name, a.remaining,
                         a.tier});
    }
    // The player's death-related statistics (format 21; SURV-7): the
    // game's counter map IS the save's (name-sorted by construction;
    // zero counters are absent from the map).
    save.playerStats = m_stats;
    for (const std::unique_ptr<Entity>& e : m_entities) {
        // Persisted entities are ItemEntities. Mobs (Phase 20) and arrows
        // (Phase 21) are EPHEMERAL by design — the save format expresses
        // them by not storing them (a mob/arrow alive at save time is
        // simply gone on load), so they are skipped, not an error. Any
        // OTHER kind means a code path this save format cannot express —
        // fail loudly.
        const ItemEntity* item = dynamic_cast<const ItemEntity*>(e.get());
        if (item == nullptr) {
            if (dynamic_cast<const MobEntity*>(e.get()) == nullptr &&
                dynamic_cast<const Projectile*>(e.get()) == nullptr) {
                return MC_FAIL(
                    "cannot save: an entity of a kind this build does not "
                    "persist exists");
            }
            continue;
        }
        SavedItemEntity se;
        se.item = item->itemName();
        se.count = item->count();
        se.pos = item->position();
        se.age = item->age();
        se.pickupDelay = item->pickupDelayRemaining();
        se.damage = item->damage(); // SURV-3 (the stack's accumulated)
        save.entities.push_back(std::move(se));
    }
    // The modifications: every cell that differs from what the seed
    // generates (block + fluid), sorted by position.
    if (journalSave) {
        // PIPE-2: the incremental diff — only the journal's EXISTING
        // chunks are regenerated + compared (the full-world O(N) scan is
        // gone). Byte-identical to diffWorld for the same world (the
        // equivalence the unit tests pin; --save-full-diff keeps the
        // legacy path as the in-process oracle). Phase 37: large journal
        // footprints diff on kSaveDiffWorkers threads.
        auto diff = journalDiff(m_world, makeChunkGenerator(), m_blocks,
                                m_saveJournal.blocks, m_saveJournal.fluids,
                                save.blockEdits, save.fluidEdits,
                                kSaveDiffWorkers);
        if (!diff.ok()) {
            return diff.error();
        }
    } else {
        // Legacy (pre-PIPE-2, and the fallback/oracle path): the full
        // world diff. Phase 37: large worlds diff on kSaveDiffWorkers
        // threads (P33: ~14 s exit on a ~100k-chunk world single-
        // threaded); the position-sorted output is byte-identical
        // either way.
        auto diff = diffWorld(m_world, makeChunkGenerator(), m_blocks,
                              save.blockEdits, save.fluidEdits,
                              kSaveDiffWorkers);
        if (!diff.ok()) {
            return diff.error();
        }
    }
    // STRM-2: the retained (unloaded) columns' diffs merge into the save
    // in BOTH paths (the journal diff and the legacy/oracle full diff
    // cover EXISTING chunks only — a retained column is unloaded, and its
    // journal entries were evicted at unload; without the merge the exit
    // save would silently drop the session's far-column fluid changes).
    // Duplicate cells are structurally impossible (the one-of invariant);
    // a session that never unloaded a modified column passes an empty
    // map (the no-op — the pre-STRM-2 file shape, bit-identical).
    mergeRetained(save, m_retainedColumns);
    return save;
}

Result<void> Game::writeSave(bool byAutoSave) const {
    auto saved = collectSave();
    MC_TRY(saved);
    auto serialized = serializeSave(*saved, m_items);
    MC_TRY(serialized);
    const std::filesystem::path path = savePath();
    // SAVE-1: the ATOMIC write (same-directory temp + rename) shared by
    // the exit save, the F5 save and the periodic auto-save — the slot
    // file holds either the previous complete save or this one, never a
    // half-written one (a kill mid-write cannot corrupt it).
    MC_TRY(writeAtomicFile(path, serialized->dump(2) + '\n'));
    MC_LOG_INFO((byAutoSave ? "World auto-saved to '" : "World saved to '") +
                path.string() + "' (" +
                std::to_string(saved->blockEdits.size()) +
                " block edits, " + std::to_string(saved->fluidEdits.size()) +
                " fluid edits, " + std::to_string(saved->entities.size()) +
                " entities, " + std::to_string(saved->slots.size()) +
                " slots)");
    return {};
}

Result<SaveGame> Game::readSaveFile(const std::filesystem::path& path) const {
    std::ifstream in(path);
    if (!in) {
        return MC_FAIL("cannot open save file '" + path.string() + "'");
    }
    std::stringstream ss;
    ss << in.rdbuf();
    auto parsed = Json::parse(ss.str());
    if (!parsed.ok()) {
        return MC_FAIL("invalid JSON in save file '" + path.string() + "': " +
                       parsed.error().message);
    }
    auto save =
        parseSave(*parsed, m_items, m_blocks, m_effects, m_generator->seed());
    if (!save.ok()) {
        return MC_FAIL("save file '" + path.string() + "': " +
                       save.error().message);
    }
    // SURV-7: the stats vocabulary check (every "kill.<name>" names a
    // REGISTERED mob; "deathCount" is fixed). The mob registry is not a
    // parseSave parameter (180+ call sites), so the check runs here, at
    // the single game-load choke point (the named rejection is the same
    // either way — the rescan-pattern twin).
    MC_TRY(validateStats(save->playerStats, m_mobTypes));
    return *save;
}

Result<void> Game::applySave(const SaveGame& save) {
    // 1) Cell edits onto the generated world (blocks first, then fluids).
    MC_TRY(applyWorldEdits(save, m_blocks, m_world));
    // The edited columns differ from the pristine generated terrain — the
    // save's diff must survive a re-save even when the player is currently
    // far away: protect them from the unload pass (applyWorldEdits writes
    // the World directly, bypassing the setBlock choke point's marking).
    for (const SavedBlockEdit& e : save.blockEdits) {
        m_editedColumns.insert({floorDiv(e.pos.x, kChunkSize),
                                floorDiv(e.pos.z, kChunkSize)});
    }
    for (const SavedFluidEdit& e : save.fluidEdits) {
        m_editedColumns.insert({floorDiv(e.pos.x, kChunkSize),
                                floorDiv(e.pos.z, kChunkSize)});
    }
    // 2) Lighting: a full re-propagation is exact for any edit count (the
    //    incremental local updates converge to the same values).
    Lighting::propagateAll(m_world, m_blocks);
    // 3) Player. restorePosition teleports (velocity zeroed) and re-pins
    //    the fall tracking to the restored Y (Phase 36: a save taken
    //    mid-air has no leave-the-ground transition on load, so without
    //    the pin the saved fall would land damage-free). When the save
    //    was taken at rest the one-frame gravity re-settle is
    //    position-exact (the floor plane is an integer).
    m_player->restorePosition(save.playerPos);
    // SAVE-2 (format 22): re-apply the saved velocity (pre-22 files
    // carry the zero triple = the legacy zeroed restore, bit-for-bit).
    // restorePosition's teleport already zeroed it, so this single
    // assignment is the whole motion restore.
    m_player->setVelocity(save.playerVelocity);
    m_player->yaw = save.playerYaw;
    m_player->pitch = save.playerPitch;
    // Hunger (Phase 23): format 1-3 saves have none (-1 = full).
    m_player->setHunger(save.playerHunger < 0.0f
                            ? m_player->maxHunger()
                            : save.playerHunger);
    // Saturation (Phase 28): format 1-4 saves have none (-1 = no buffer).
    m_player->setSaturation(
        save.playerSaturation < 0.0f ? 0.0f : save.playerSaturation);
    // Health is applied in 4e) AFTER the status effects: the Phase 56
    // absorption layer raises the effective max, and a saved health above
    // the BASE max is legal exactly when the saved effects carry a layer.
    m_camera.position = m_player->eyePosition();
    m_camera.yaw = m_player->yaw;
    m_camera.pitch = m_player->pitch;
    // 4) Inventory (replaces the hotbar.json fill) + selection.
    for (std::size_t i = 0; i < m_inventory.size(); ++i) {
        m_inventory.setSlot(i, save.slots[i]);
    }
    m_selectedSlot = static_cast<std::size_t>(save.selectedSlot);
    // 4b) Crafting grids (format 3; format 1/2 saves load them empty).
    // CRAFT-1: the E panel's grid is the only global one — the table's
    // grids are per-block (the 4c-quinque section below).
    m_inventoryGrid.clear();
    for (int i = 0; i < CraftingGrid::kSize; ++i) {
        if (i < static_cast<int>(save.invGrid.size())) {
            m_inventoryGrid.setSlot(i, save.invGrid[static_cast<std::size_t>(i)]);
        }
    }
    // 4c) The per-block furnace states (format 7; Phase 44). Format-6
    // saves load their ONE global state already migrated (exactly one
    // furnace block: assigned there; otherwise: discarded — save.cpp);
    // format 1-5 load no entries at all. The parser derived progressFor
    // on each state; the copy keeps it.
    m_furnaces.clear();
    for (const SavedFurnace& sf : save.furnaces) {
        m_furnaces.emplace(sf.pos, sf.state);
    }
    // 4c-bis) The burning fire blocks (format 10; Phase 64): positions +
    // the real-dt seconds remaining (a reload continues burning — like
    // the player's fire effect). The entries are parse-validated
    // (remaining > 0, one per fire block).
    m_fires.clear();
    for (const SavedFire& sf : save.fires) {
        m_fires.emplace(sf.pos, sf.remainingSeconds);
    }
    // Migration (format 9 and earlier): a fire block in the world's block
    // edits WITHOUT a fires entry is only possible in a hand-edited old
    // file (the fire block did not exist before format 10) — it starts at
    // the full lifetime (the empty-furnace-state migration pattern).
    if (m_fireId.valid()) {
        for (const SavedBlockEdit& e : save.blockEdits) {
            if (e.blockName == "fire") {
                m_fires.emplace(e.pos, m_fireLifetime);
            }
        }
    }
    // 4c-ter) The campfire cooking states (format 11; Phase 71). Format-
    // 11 saves carry one entry per campfire block (parse-validated,
    // cross-checked in both directions); format 10 and earlier carry none.
    m_campfires.clear();
    for (const SavedCampfire& sc : save.campfires) {
        m_campfires.emplace(sc.pos, sc.state);
    }
    // Migration (format 10 and earlier): a campfire-FAMILY block in the
    // world's block edits WITHOUT a campfires entry is only possible in
    // a hand-edited old file (the campfire block did not exist before
    // format 11, the extinguished one before format 13) — it starts
    // empty (the empty-furnace-state migration pattern).
    for (const SavedBlockEdit& e : save.blockEdits) {
        if ((e.blockName == "campfire" ||
             e.blockName == "campfire_extinguished") &&
            m_campfires.count(e.pos) == 0) {
            m_campfires.emplace(e.pos, CampfireCookState {});
        }
    }
    // 4c-quinque) The per-block crafting-table grids (format 19;
    // CRAFT-1). Format-19 saves carry one entry per crafting_table block
    // (parse-validated, cross-checked in both directions); format 18 and
    // earlier carry the ONE global "table_grid", migrated at parse
    // (sole attribution to the single table block, otherwise discarded —
    // the Phase 44 precedent, save.cpp). The parse-time invariant
    // guarantees an entry for EVERY table block in the world's block
    // edits, so the map is complete after this copy (the furnace load
    // path, the twin — no rescan needed).
    m_tables.clear();
    for (const SavedTable& st : save.tables) {
        m_tables.emplace(st.pos, st.grid);
    }
    // 4c-quater) The per-instance block states (format 15; Phase 94 stair
    // facing, generalized in ENT-2). Format-15 saves carry one entry per
    // stateful block (parse-validated, cross-checked in both directions);
    // format 14 carries the "stairs" entries (parsed + migrated into the
    // states, value-preserving); format 13 and earlier carry none (the
    // per-instance state did not exist — the cells keep the sentinel, and
    // the consumers fall back to the block's state default on load).
    // The block/fluid replay above ran through the World (not the game's
    // choke point), so the saved value is authoritative: it overwrites
    // whatever the replay left.
    for (const SavedState& ss : save.states) {
        m_world.setBlockState(ss.pos.x, ss.pos.y, ss.pos.z, ss.state);
    }
    // 4d) The player's active status effects (format 8; Phase 46). The
    // names are parse-validated against the registry (idForName cannot
    // fail); the tick phase starts at 0 (it is not saved — the next tick
    // lands at the full interval).
    m_playerEffects.clearAll();
    for (const SavedEffect& se : save.playerEffects) {
        // SURV-6: the saved tier (absent in format-19 files = 0 — the
        // migration; range-checked against the registry at parse).
        m_playerEffects.apply(m_effects.idForName(se.name), se.remaining,
                              se.tier);
    }
    // 4d-bis) The death-related statistics (format 21; SURV-7): the map
    // IS the state (vocabulary validated in readSaveFile; a pre-21 load
    // migrates to EMPTY — the system did not exist).
    m_stats = save.playerStats;
    // 4e) The player's health + absorption (Phase 16; the two SEPARATE
    // pools are set here in Phase 72, AFTER the effects of 4d). A
    // format-1 save has no health (-1 = full); a saved 0 reloads the
    // player dead (the death screen offers R). No syncAbsorption here:
    //   - a format-12 save carries BOTH pools, parse-validated (health
    //     in range, absorption in [0, 64] and <= the effects' total) and
    //     authoritative — the pool may sit BELOW the total (damage
    //     drains it between sync points; reloading must keep the drain);
    //   - a format <= 11 save holds the SHARED P56 health (the value
    //     included the layer): the migration splits it — the excess
    //     above the base max becomes the pool (it is <= the effects'
    //     total by the old model's own invariant), the health keeps the
    //     base part. A hand-edited format-11 health above base + total
    //     is a contract violation, named and rejected.
    const float baseMax = m_player->maxHealth();
    const float effectsTotal = m_playerEffects.totalAbsorption(m_effects);
    if (save.playerAbsorption < 0.0f) {
        const float h =
            save.playerHealth < 0.0f ? baseMax : save.playerHealth;
        if (h > baseMax + effectsTotal + 1e-4f) {
            return MC_FAIL("save 'player.health' is above the base max plus "
                           "the effects' absorption total (format 11 and "
                           "below: a hand-edited shared health)");
        }
        m_player->setAbsorption(std::max(0.0f, h - baseMax));
        m_player->setHealth(std::min(h, baseMax));
    } else {
        if (save.playerHealth >= 0.0f &&
            save.playerHealth > baseMax + 1e-4f) {
            return MC_FAIL("save 'player.health' is above the base max "
                           "(format 12: the absorption is a separate pool)");
        }
        m_player->setAbsorption(save.playerAbsorption);
        m_player->setHealth(save.playerHealth < 0.0f ? baseMax
                                                     : save.playerHealth);
    }
    // 4f) The player's armor (format 16; SURV-1). The names are
    // parse-validated (registered items whose data armor_slot matches
    // the save's key — idForName cannot fail); format 1-15 saves carry
    // no armor (all the slots stay empty, the default). SURV-3 (format
    // 18): the pieces' accumulated damage rides the slot (parse-
    // validated against the item's data durability) and the setter's
    // durability argument (the damage()'s wear step needs it — the
    // player holds no ItemRegistry, the points pattern).
    for (int slot = 0; slot < kArmorSlotCount; ++slot) {
        const std::string& name =
            save.playerArmor[static_cast<std::size_t>(slot)];
        if (name.empty()) {
            m_player->setArmorSlot(slot, ItemStack {}, 0);
            continue;
        }
        const Item& item = m_items.at(m_items.idForName(name));
        const float dmg =
            save.playerArmorDamage[static_cast<std::size_t>(slot)];
        m_player->setArmorSlot(slot, ItemStack {item.id, 1, dmg},
                               item.armorPoints, item.durability);
    }
    m_prevHealth = m_player->health(); // no hurt event from the load itself
    // 4g) The player's oxygen (format 17; SURV-2, the drowning
    // system). A format-17 save carries the value (parse-validated
    // [0, kOxygenMax]); format 1-16 loads full air (the sentinel —
    // the system did not exist, the player had full air). The
    // drownTicks accumulator is never saved — it restarts at 0 (the
    // tick-phase convention, the effects pattern).
    m_drown.oxygen =
        save.playerOxygen < 0 ? kOxygenMax : save.playerOxygen;
    m_drown.drownTicks = 0;
    // 5) Dropped items (with their lifetime/pickup timers restored;
    // SURV-3: the stack's accumulated damage too).
    for (const SavedItemEntity& e : save.entities) {
        const Item& item = m_items.at(m_items.idForName(e.item));
        auto entity = std::make_unique<ItemEntity>(
            m_world, m_blocks, e.item, item.maxStack, e.count, e.pos,
            e.damage);
        entity->setAge(e.age);
        entity->setPickupDelay(e.pickupDelay);
        m_entities.push_back(std::move(entity));
    }
    // 6) Day-cycle time (wins over --time for a loaded save).
    m_dayCycle.setTimeOfDay(save.time);
    return {};
}

Result<void> Game::tryLoadSave() {
    if (!m_args.saveSlot.has_value() || m_args.newWorld) {
        return {};
    }
    const std::filesystem::path path = savePath();
    if (!std::filesystem::exists(path)) {
        MC_LOG_INFO("No save at '" + path.string() + "' — starting fresh");
        return {};
    }
    auto save = readSaveFile(path);
    MC_TRY(save);
    MC_TRY(applySave(*save));
    MC_LOG_INFO("Loaded save '" + path.string() + "' (" +
                std::to_string(save->blockEdits.size()) +
                " block edits, " + std::to_string(save->fluidEdits.size()) +
                " fluid edits, " + std::to_string(save->entities.size()) +
                " entities)");
    return {};
}

void Game::rescanCrops() {
    // Rebuild the growth tracking set from the world: every cell whose
    // block carries a non-empty growsTo. The block id IS the stage, so a
    // full rescan is exact (no per-instance state to restore). Runs at the
    // end of buildScene — a fresh world finds nothing (worldgen places no
    // crops), a loaded save finds the player's planted ones (restored by
    // the block edits).
    m_crops.clear();
    for (const ChunkPos& cp : m_world.chunkPositions()) {
        const Chunk* chunk = m_world.findChunk(cp);
        if (chunk == nullptr) {
            continue;
        }
        // A chunk is 16x16x16 (kChunkSize on every axis); the world's
        // height is stacked across ChunkPos.y (see FluidSim's origin math).
        // Local coordinates are [0, kChunkSize) on all three axes.
        const int baseX = cp.x * kChunkSize;
        const int baseY = cp.y * kChunkSize;
        const int baseZ = cp.z * kChunkSize;
        for (int ly = 0; ly < kChunkSize; ++ly) {
            for (int lz = 0; lz < kChunkSize; ++lz) {
                for (int lx = 0; lx < kChunkSize; ++lx) {
                    const BlockID id = chunk->get(lx, ly, lz);
                    if (m_blocks.at(id).properties.growsTo.empty()) {
                        continue;
                    }
                    const BlockPos wp {baseX + lx, baseY + ly, baseZ + lz};
                    m_crops[wp] = id;
                }
            }
        }
    }
}

void Game::rescanSaplings() {
    // GEN-2: the rescanCrops twin over the `tree` predicate. The STAGE is
    // not rescanned — it is the world's per-instance state byte (the
    // save's format-15 segment restored it with the block edits); this
    // set only decides which cells the growth engine rolls.
    m_saplings.clear();
    for (const ChunkPos& cp : m_world.chunkPositions()) {
        const Chunk* chunk = m_world.findChunk(cp);
        if (chunk == nullptr) {
            continue;
        }
        const int baseX = cp.x * kChunkSize;
        const int baseY = cp.y * kChunkSize;
        const int baseZ = cp.z * kChunkSize;
        for (int ly = 0; ly < kChunkSize; ++ly) {
            for (int lz = 0; lz < kChunkSize; ++lz) {
                for (int lx = 0; lx < kChunkSize; ++lx) {
                    const BlockID id = chunk->get(lx, ly, lz);
                    if (!m_blocks.at(id).hasTree()) {
                        continue;
                    }
                    const BlockPos wp {baseX + lx, baseY + ly, baseZ + lz};
                    m_saplings[wp] = id;
                }
            }
        }
    }
}

void Game::rescanMoistureBlocks() {
    // GEN-9: the rescanSaplings twin over the model.moisture_top
    // predicate. The WETNESS is not stored — it is derived at mesh time
    // from the live fluid field; the map value is SEEDED with the first
    // evaluation so a just-loaded farm only re-meshes on a real flip
    // (not on the load itself). Runs at the end of buildScene — a fresh
    // world scans to nothing (worldgen places no farmland), a loaded save
    // finds the player's placed ones (restored by the block edits).
    m_moistureBlocks.clear();
    for (const ChunkPos& cp : m_world.chunkPositions()) {
        const Chunk* chunk = m_world.findChunk(cp);
        if (chunk == nullptr) {
            continue;
        }
        const int baseX = cp.x * kChunkSize;
        const int baseY = cp.y * kChunkSize;
        const int baseZ = cp.z * kChunkSize;
        for (int ly = 0; ly < kChunkSize; ++ly) {
            for (int lz = 0; lz < kChunkSize; ++lz) {
                for (int lx = 0; lx < kChunkSize; ++lx) {
                    const BlockID id = chunk->get(lx, ly, lz);
                    if (m_blocks.at(id).model.moistureTop.empty()) {
                        continue;
                    }
                    const BlockPos wp {baseX + lx, baseY + ly, baseZ + lz};
                    m_moistureBlocks[wp] = soilIsWet(m_world, wp);
                }
            }
        }
    }
}

void Game::refreshMoistureStates() {
    // GEN-9: re-evaluate the wetness of every tracked moisture cell after
    // a fluid tick that changed cells, and mark the cell's chunk dirty
    // ONLY when the wetness flipped. Position-sorted iteration + the pure
    // soilIsWet predicate = deterministic (hard rule 8). The re-mesh
    // re-derives the top face from the now-updated fluid field (the
    // mesher's moisture hook), so the dirty mark is the whole effect.
    for (auto& [pos, wet] : m_moistureBlocks) {
        const bool nowWet = soilIsWet(m_world, pos);
        if (nowWet == wet) {
            continue;
        }
        wet = nowWet;
        m_world.markChunkDirty(worldToChunk(pos));
    }
}

void Game::placeGrownTree(const BlockPos& saplingPos, int height) {
    // GEN-2: one completed sapling -> a grown tree. The pure geometry
    // (planTree) was already FITS-probed by the growth step, so only
    // verified cells are written — trunk column first (the root cell
    // becomes the trunk base), canopy after. Every write goes through
    // the setBlock choke point (journal + tracking sets + edited-column
    // protection + the ENT-2 state seeding). The rolled height re-uses
    // the step's value (frame-independent — the probe and the placement
    // plan the SAME tree).
    const BlockID saplingId = m_world.getBlock(saplingPos.x, saplingPos.y,
                                               saplingPos.z);
    const auto it = m_treeSpecs.find(saplingId);
    if (it == m_treeSpecs.end()) {
        return; // defensive: an untracked sapling id (a stale step)
    }
    const TreeSpec& spec = it->second;
    int minX = saplingPos.x, minY = saplingPos.y, minZ = saplingPos.z;
    int maxX = saplingPos.x, maxY = saplingPos.y, maxZ = saplingPos.z;
    for (const TreeCell& c : planTree(height, spec)) {
        const int wx = saplingPos.x + c.offset.x;
        const int wy = saplingPos.y + c.offset.y;
        const int wz = saplingPos.z + c.offset.z;
        setBlock(wx, wy, wz, c.trunkCell ? spec.trunk : spec.leaves);
        minX = std::min(minX, wx); maxX = std::max(maxX, wx);
        minY = std::min(minY, wy); maxY = std::max(maxY, wy);
        minZ = std::min(minZ, wz); maxZ = std::max(maxZ, wz);
    }
    // The choke point already dropped the tracking entry when the root
    // cell became trunk (a non-`tree` block); the belt is deterministic.
    m_saplings.erase(saplingPos);
    // ONE relight over the tree's bounding box (the fluid-tick pattern,
    // not per-cell updateLocal): the trunk column + the canopy shadow
    // (LGT-2: the leaves' light_opacity 2 darkens what is under the
    // crown). The player triggered this — a one-time main-thread cost,
    // not a per-frame one (documented).
    Lighting::updateRegion(m_world, m_blocks,
                           BlockPos {minX, minY, minZ},
                           BlockPos {maxX, maxY, maxZ});
}

void Game::rescanFlammableChunks() {
    // The burn pass's gate: every chunk that holds a flamable block.
    m_flammableChunks.clear();
    for (const ChunkPos& cp : m_world.chunkPositions()) {
        const Chunk* c = m_world.findChunk(cp);
        if (c == nullptr) {
            continue;
        }
        const BlockID* blocks = c->data();
        for (int i = 0; i < Chunk::kVolume; ++i) {
            if (m_blocks.at(blocks[i]).properties.flamable) {
                m_flammableChunks.insert(cp);
                break;
            }
        }
    }
}

void Game::rescanBeacons() {
    // Phase 83: rebuild the beacon tracking set from the world — every cell
    // whose block carries a non-empty beaconEffect. A fresh world finds
    // nothing (worldgen places no beacons); a loaded save finds the
    // player's placed ones (restored by the block edits). No per-instance
    // state: the power level is re-derived from the world each frame.
    m_beacons.clear();
    for (const ChunkPos& cp : m_world.chunkPositions()) {
        const Chunk* chunk = m_world.findChunk(cp);
        if (chunk == nullptr) {
            continue;
        }
        const int baseX = cp.x * kChunkSize;
        const int baseY = cp.y * kChunkSize;
        const int baseZ = cp.z * kChunkSize;
        for (int ly = 0; ly < kChunkSize; ++ly) {
            for (int lz = 0; lz < kChunkSize; ++lz) {
                for (int lx = 0; lx < kChunkSize; ++lx) {
                    const BlockID id = chunk->get(lx, ly, lz);
                    if (m_blocks.at(id).properties.beaconEffects.empty()) {
                        continue;
                    }
                    const BlockPos wp {baseX + lx, baseY + ly, baseZ + lz};
                    m_beacons[wp] = id;
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Init / main loop
// ---------------------------------------------------------------------------

Result<void> Game::init(const GameArgs& args) {
    m_args = args;

    // --- configuration ----------------------------------------------------
    if (!std::filesystem::exists(m_args.configPath)) {
        MC_LOG_INFO("Config not found at '" + m_args.configPath.string() +
                    "', creating with defaults");
        std::error_code ec;
        std::filesystem::create_directories(m_args.configPath.parent_path(), ec);
        std::ofstream out(m_args.configPath);
        if (!out) {
            return MC_FAIL("cannot create config file: '" +
                           m_args.configPath.string() + "'");
        }
        out << Config::defaultConfigJson() << '\n';
        out.close();
    }

    auto loaded = Config::load(m_args.configPath);
    MC_TRY(loaded);
    m_config = std::move(*loaded);
    if (m_args.seed >= 0) {
        m_config.set("world.seed", static_cast<long long>(m_args.seed));
    }
    if (m_args.time >= 0.0) {
        m_config.set("world.start_time_of_day", m_args.time);
    }

    const auto seed = m_config.getInt64("world.seed", 0);
    const int viewDistance = m_config.getInt("world.view_distance", 8);
    MC_LOG_INFO("Config loaded from '" + m_config.path().string() + "' (seed=" +
                std::to_string(seed) + ", view_distance=" +
                std::to_string(viewDistance) + ")");

    // --- assets -----------------------------------------------------------
    auto assetRoot = m_assets.setRoot("assets");
    MC_TRY(assetRoot);

    // --- window + GL context ----------------------------------------------
    auto window = m_window.init(m_config);
    MC_TRY(window);

    const char* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    const char* glVersion = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    MC_LOG_INFO(std::string("GL ") + glVersion + " | " + vendor + " | " + renderer);

    // --- renderer + scene ---------------------------------------------------
    auto rendererInit = m_renderer.init();
    MC_TRY(rendererInit);

    auto scene = buildScene();
    MC_TRY(scene);

    m_initialized = true;
    return {};
}

int Game::run() {
    double lastFrame = nowSeconds();
    m_lastTitleUpdate = lastFrame;

    MC_LOG_INFO("Entering main loop (frames=" +
                (m_args.frames > 0 ? std::to_string(m_args.frames)
                                   : std::string("unlimited")) +
                ")");

    while (true) {
        m_input.frameBegin();
        m_input.pump();
        // ESC exits only while the pointer is free AND the inventory is
        // closed AND the player is alive (while captured it releases the
        // pointer, while the inventory is open it closes it, while dead
        // the death screen owns the input — all handled in update).
        if (m_input.quitRequested() ||
            (!m_window.relativeMouseMode() && !panelOpen() &&
             !m_player->isDead() &&
             m_input.wasPressed(GameKey::Escape))) {
            MC_LOG_INFO("Exit requested, shutting down");
            break;
        }

        const double frameStart = nowSeconds();
        double dt = frameStart - lastFrame;
        lastFrame = frameStart;
        if (dt < 0.0) {
            dt = 0.0;
        }
        // Clamp huge deltas (debugger pause, window drag) so one hiccup
        // cannot explode the simulation.
        constexpr double kMaxDt = 0.25;
        if (dt > kMaxDt) {
            dt = kMaxDt;
        }
        if (dt > 0.0) {
            const double fps = 1.0 / dt;
            m_fpsSmoothed =
                (m_fpsSmoothed <= 0.0) ? fps : (m_fpsSmoothed * 0.95 + fps * 0.05);
        }

        update(dt);
        render();
        m_window.swapBuffers();
        ++m_frameCount;

        if (m_args.frames > 0 && m_frameCount >= m_args.frames) {
            MC_LOG_INFO("Frame limit reached (" + std::to_string(m_frameCount) +
                        " frames), exiting");
            if (!m_args.screenshotPath.empty()) {
                takeScreenshot(m_args.screenshotPath);
            }
            break;
        }

        // Optional software FPS cap (in addition to vsync).
        const int fpsCap = m_config.getInt("render.fps_cap", 0);
        if (fpsCap > 0) {
            const double limit = 1.0 / static_cast<double>(fpsCap);
            const double elapsed = nowSeconds() - frameStart;
            if (elapsed < limit) {
                SDL_Delay(static_cast<Uint32>((limit - elapsed) * 1000.0));
            }
        }

        // Refresh the title (shows live FPS + draw calls) at most twice
        // per second. ASCII only: SDL's X11 backend silently REFUSES
        // non-ASCII titles ("Failed to convert title to UTF8") and the
        // meter would never appear (the em dash used to be here — the
        // FPS display was dead for every release).
        if (frameStart - m_lastTitleUpdate >= 0.5) {
            m_lastTitleUpdate = frameStart;
            m_window.setTitle("TokenMine.cpp - " +
                              std::to_string(static_cast<int>(
                                  std::lround(m_fpsSmoothed))) +
                              " FPS | " + std::to_string(m_renderer.stats().drawCalls) +
                              " draw calls");
        }
    }

    // Chunk streaming (Phase 14): drain the worker pool before the save
    // and shutdown — a job exception would surface in the flush, and the
    // reset joins the workers. In-flight generation columns are not
    // integrated (integration happens in update); that is correct, they
    // are pristine generated terrain that a save never records.
    if (m_pool) {
        m_pool->flush();
        m_pool.reset();
    }

    // Audio (Phase 15): close the stream/device before the window's
    // SDL_Quit (idempotent — the destructor does the same).
    m_sound.shutdown();

    // Save (Phase 13): persist the world state for the slot (opt-in via
    // --save, after the final frame — and after any screenshot, which is
    // taken inside the loop). A failed save is logged, not fatal: the
    // session itself ran cleanly.
    if (m_args.saveSlot.has_value()) {
        auto saved = writeSave();
        if (!saved.ok()) {
            MC_LOG_ERROR("Save failed: " + saved.error().describe());
        }
    }

    m_window.shutdown();
    m_initialized = false;
    MC_LOG_INFO("Game shut down cleanly");
    return 0;
}

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
                                 m_spawnPoint.z);
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
    const Mesh& mesh = arrowMesh();
    if (!mesh.valid()) {
        return;
    }
    // The program is UNBOUND here: Renderer::draw binds/unbinds around
    // every draw, and the last textured draw of the opaque pass left it
    // off. Setting a uniform on an unbound program is
    // GL_INVALID_OPERATION, so bind it for the switch (uniforms persist
    // on the program object; the next pass re-binds per draw).
    m_basicShader.bind();
    m_basicShader.setUniform1i("u_useTexture", 0);
    m_basicShader.setUniform3f("u_color", kArrowColorR, kArrowColorG,
                               kArrowColorB);
    m_basicShader.setUniform1f("u_alpha", 1.0f); // arrows are opaque
    for (const auto& entity : m_entities) {
        const Projectile* arrow =
            dynamic_cast<const Projectile*>(entity.get());
        if (arrow == nullptr) {
            continue;
        }
        // Collision-box center (the box is kSize tall; the box mesh is
        // centered on its origin) oriented along the travel direction
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
        m_renderer.draw(mesh, m_basicShader, view, projection, model);
    }
    // Re-bind for the restore: the last arrow draw (if any) unbound the
    // program.
    m_basicShader.bind();
    m_basicShader.setUniform1i("u_useTexture", 1);
    m_basicShader.unbind();
}

const Mesh& Game::arrowMesh() {
    if (m_arrowMesh.valid()) {
        return m_arrowMesh;
    }
    const ItemMeshGeometry geom = makeArrowGeometry();
    auto r = m_arrowMesh.upload(geom.vertices, geom.indices);
    if (!r.ok()) {
        MC_LOG_ERROR("Arrow mesh build failed: " + r.error().describe());
    }
    return m_arrowMesh;
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
            constexpr int kIconPx = 16;
            constexpr int kIconGap = 2;
            constexpr int kStripGap = 6; // between the strips
            const float baseMaxHealth = m_player->params().maxHealth;
            const int heartIcons =
                static_cast<int>(std::ceil(baseMaxHealth / 2.0f));
            const int hungerIcons =
                static_cast<int>(std::ceil(m_player->maxHunger() / 2.0f));
            const int heartW =
                heartIcons * kIconPx + (heartIcons - 1) * kIconGap;
            // The (BASE) hearts stay centered (the Phase 16 layout); the
            // hunger strip starts just past their right edge.
            const int hx0 = (w - heartW) / 2;
            const int gx0 = hx0 + heartW + kStripGap;
            const int hy = y0 - kIconPx - 6;
            // The absorption pool (Phase 72, the golden apple): extra
            // RED hearts just LEFT of the normal strip (the Minecraft
            // layout). The strip shows the POOL itself — damage empties
            // it before any normal heart is (the two-component model).
            // Pool 0 = nothing drawn (the no-input baseline is
            // bit-identical: a fresh run has no effects, hence no pool).
            const float absorbPool = m_player->absorption();
            const int absIcons = absorbPool > 0.0f
                ? static_cast<int>(std::ceil(absorbPool / 2.0f))
                : 0;
            const int absW =
                absIcons * kIconPx + (absIcons - 1) * kIconGap;
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
            const int oxygen = m_drown.oxygen;
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
            // Status effect icons (Phase 46): left-aligned on the heart
            // row (the Minecraft layout), one 16 px effects.png tile per
            // ACTIVE effect in application order (tile t occupies u in
            // [t/N, (t+1)/N]; the width gives N). No effects = nothing
            // drawn (the no-input baseline is untouched).
            if (m_playerEffects.count() > 0) {
                const int n = static_cast<int>(
                    m_effectTexture.width() / kIconPx);
                UiBatch& batch =
                    batchFor(Vec4 {1.0f, 1.0f, 1.0f, 1.0f}, true,
                             &m_effectTexture);
                int ex = x0; // left-aligned on the hotbar's left edge (MC)
                for (const ActiveEffect& a : m_playerEffects.list()) {
                    const Effect& def = m_effects.at(a.effect);
                    addQuadUv(batch, ex, hy, kIconPx, kIconPx,
                              Vec2 {static_cast<float>(def.icon) / n, 0.0f},
                              Vec2 {static_cast<float>(def.icon + 1) / n,
                                    1.0f});
                    ex += kIconPx + kIconGap;
                }
            }
            // Armor row (SURV-1): the four equipment slots ABOVE the
            // heart row (hy − 16 − kStripGap — the row above, so it can
            // never collide with the effect icons / the absorption strip
            // that share the heart row). Fixed positions head -> feet
            // left to right, left-aligned on the hearts' left edge hx0,
            // one 16 px per-item icon (the independent PNG through the
            // shared drawIcon path) per OCCUPIED slot only. No armor =
            // nothing drawn (the no-input baseline is bit-identical: a
            // fresh run wears nothing).
            const int ay = hy - kIconPx - kStripGap;
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
                addText(m_items.at(stack.item).name, mx + L(12), my + L(12),
                        s, kText);
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
                addText(m_items.at(stack.item).name, mx + L(12), my + L(12),
                        s, kText);
            }
        } else if (hoverCell >= 0) {
            const ItemStack& stack =
                ui.grid().at(layout.gridSlots[static_cast<std::size_t>(hoverCell)]);
            if (!stack.empty()) {
                addText(m_items.at(stack.item).name, mx + L(12), my + L(12),
                        s, kText);
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
        // Phase 46: the active status effects, "name 12.3s; name 4.5s"
        // (application order); empty when the player has none.
        for (const ActiveEffect& a : m_playerEffects.list()) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%s %.1fs",
                          m_effects.at(a.effect).name.c_str(),
                          a.remaining);
            if (!info.effects.empty()) {
                info.effects += "; ";
            }
            info.effects += buf;
        }
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
