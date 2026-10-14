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
#include "entity/arrow_mesh.hpp"
#include "entity/item_entity.hpp"
#include "entity/mob_entity.hpp"
#include "entity/mob_mesh.hpp"
#include "block/chance_drop.hpp"
#include "world/growth.hpp"
#include "world/fire_spread.hpp"
#include "world/beacon.hpp"
#include "world/noise.hpp"
#include "ui/font.hpp"
#include "ui/inventory_ui.hpp"
#include "ui/ui_scale.hpp"

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
// Fire v2 (Phase 66): the ember emission per live fire cell runs every
// Nth frame (0.1 s = the Minecraft tick rhythm); the fixed frame step
// keeps headless runs byte-reproducible.
constexpr int kFireParticleEveryNFrames = 6;
// Dedicated salt for the fire ember seeds (same discipline as the
// growth/chance-drop salt separation, Phase 45/49): ember emissions
// never track any other deterministic stream in the world.
constexpr std::uint64_t kFireParticleRollSalt = 0xE4B6F1C2D9A83574ULL;

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

// Chunk streaming (Phase 14). The demo area is generated synchronously at
// startup (keeps stationary runs job-free, so headless baselines stay
// byte-reproducible); beyond it, columns are generated on worker threads
// as the player approaches the edge of the loaded area.
// Worker count of the streaming pool (generation + lighting jobs).
constexpr int kStreamWorkerCount = 4;
// Columns posted for generation per frame (the main thread then meshes
// and integrates them; 1 keeps the main-thread work well under the frame
// budget while the pool stays pipelined a few columns deep).
constexpr int kStreamColumnsPerFrame = 1;
// Streaming engages when the player's column is within this many columns
// (Chebyshev) of a missing one inside the radius ring. 1 = the player
// must reach the edge of the loaded area; an idle player in the center of
// the 4x4 demo area (2 columns from its edge) never engages.
constexpr int kStreamTriggerDistance = 1;

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

Mat4 chunkModel(const ChunkPos& pos) {
    const BlockPos o = chunkOrigin(pos);
    return Mat4::translate({static_cast<float>(o.x), static_cast<float>(o.y),
                            static_cast<float>(o.z)});
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
    // orientation) and blockLight 1.0 (full brightness regardless of world
    // lighting). Culling is disabled for the beam draw, so the winding only
    // needs to trace each quad (both faces render, alpha-blended).
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
    // Strict data check (Phase 83): a block's beacon_effect must name an
    // EXISTING effect (the beacon applies it to in-range players). The
    // empty-string rejection is at data load; this is the existence
    // cross-check (a typo in the effect name fails init loudly — the
    // grows_to / on_hit_effect discipline). The effect must not be
    // negative-removable-irrelevant: any registered effect is a valid beacon
    // target (the shipped data uses "absorption").
    for (const BlockID id : m_blocks.allIds()) {
        const Block& b = m_blocks.at(id);
        if (b.properties.beaconEffect.empty()) {
            continue;
        }
        if (m_effects.findByName(b.properties.beaconEffect) == nullptr) {
            return MC_FAIL("block '" + b.name + "' beacon_effect names "
                           "unknown effect '" + b.properties.beaconEffect +
                           "'");
        }
    }
    for (const MobID mid : m_mobTypes.allIds()) {
        const Mob& mob = m_mobTypes.at(mid);
        if (!mob.properties.hitEffectName.empty() &&
            m_effects.findByName(mob.properties.hitEffectName) == nullptr) {
            return MC_FAIL("mob '" + mob.name + "' on_hit_effect references "
                           "the unregistered effect '" +
                           mob.properties.hitEffectName + "'");
        }
    }
    for (const ItemID iid : m_items.allIds()) {
        const Item& it = m_items.at(iid);
        if (!it.grantsEffect.empty() &&
            m_effects.findByName(it.grantsEffect) == nullptr) {
            return MC_FAIL("item '" + it.name + "' grants_effect references "
                           "the unregistered effect '" + it.grantsEffect + "'");
        }
        // Phase 56: the list form, entry by entry (indexed name, the
        // chance_drops[i] pattern).
        for (std::size_t i = 0; i < it.grantsEffects.size(); ++i) {
            const Item::GrantEffect& g = it.grantsEffects[i];
            if (m_effects.findByName(g.name) == nullptr) {
                return MC_FAIL("item '" + it.name + "' grants_effects[" +
                               std::to_string(i) +
                               "] references the unregistered effect '" +
                               g.name + "'");
            }
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

    // Chunk streaming (Phase 14): the worker pool and the radius (config
    // world.view_distance, in chunk columns around the player).
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
    rescanBeacons();

    // The lighting pipeline has covered every chunk that exists now (the
    // fresh demo area, or the loaded save's world — applySave ends with a
    // full propagation). Anything that appears later is new.
    const std::vector<ChunkPos> litNow = m_world.chunkPositions();
    m_litChunks.insert(litNow.begin(), litNow.end());

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
// Chunk meshing (CPU geometry + GPU upload, game thread). Meshing stays on
// the game thread in Phase 14: it reads the live World (face culling
// across chunk boundaries), and the streaming budget
// (kStreamColumnsPerFrame) bounds its per-frame cost to a few chunks
// (measured ~0.01 ms/chunk on this machine).
// ---------------------------------------------------------------------------

Result<void> Game::updateChunkMeshes() {
    const ChunkMesher mesher(m_world, m_blocks, m_atlas.layout());
    const std::vector<ChunkPos> positions = m_world.chunkPositions();
    for (const ChunkPos& pos : positions) {
        // The chunk exists (it came from chunkPositions), so this does not
        // create or rehash; the reference stays valid for this iteration
        // (no World mutation happens while we hold it).
        Chunk& chunk = m_world.getOrCreateChunk(pos);
        if (!chunk.dirty()) {
            continue;
        }
        const ChunkMeshData data = mesher.mesh(pos);

        ChunkMesh& cm = m_chunkMeshes[pos];
        cm.opaque.reset();
        cm.transparent.reset();
        if (!data.opaqueEmpty()) {
            cm.opaque.emplace();
            auto r = cm.opaque->upload(data.opaqueVertices, data.opaqueIndices);
            MC_TRY(r);
        }
        if (!data.transparentEmpty()) {
            cm.transparent.emplace();
            auto r = cm.transparent->upload(data.transparentVertices,
                                            data.transparentIndices);
            MC_TRY(r);
        }
        chunk.clearDirty();
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
            m_world.insertChunk({job->column.x, cy, job->column.z},
                                std::move(job->chunks[cy]));
        }
        m_streamPending.erase(job->column);
        MC_LOG_INFO("Streamed column (" + std::to_string(job->column.x) +
                    ", " + std::to_string(job->column.z) +
                    ") integrated (" + std::to_string(m_world.chunkCount()) +
                    " chunks loaded)");
    }
    m_streamJobs = std::move(inFlight);
}

void Game::streamChunks() {
    if (!m_pool || !m_generator || !m_player) {
        return;
    }
    // While the player is dead the body is frozen and the death screen
    // owns the input: stop requesting new columns. The radius square
    // around the corpse would otherwise keep filling to view_distance
    // (a radius-80 square holds ~200k chunks: hours of streaming and
    // several GB of allocations behind the death veil). In-flight jobs
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

    // The loaded columns (the projection of the existing chunks).
    std::set<ColumnPos> loadedSet;
    std::vector<ColumnPos> loaded;
    for (const ChunkPos& c : m_world.chunkPositions()) {
        if (loadedSet.insert({c.x, c.z}).second) {
            loaded.push_back({c.x, c.z});
        }
    }

    // Hysteresis (see chunk_streaming): engage when the player reaches
    // the edge of the loaded area; while engaged, keep filling the full
    // radius ring (an exploring player ends up with view_distance of
    // terrain around them); disengage only when the ring around the
    // player is fully loaded (nearestMissingDistance returns the radius
    // + 1 sentinel). A never-engaged idle player in the center of the
    // loaded area stays job-free (keeps headless baselines
    // byte-reproducible).
    const int nearestMissing =
        nearestMissingDistance(player, loaded, m_streamRadius);
    if (!m_streamingEngaged) {
        if (nearestMissing > kStreamTriggerDistance) {
            return;
        }
        m_streamingEngaged = true;
        MC_LOG_INFO("Chunk streaming engaged (radius " +
                    std::to_string(m_streamRadius) + " columns)");
    } else if (nearestMissing > m_streamRadius) {
        m_streamingEngaged = false; // the radius ring is fully loaded
    }

    int posted = 0;
    const std::vector<ColumnPos> missing =
        missingColumns(player, loaded, m_streamRadius);
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
}

void Game::postLightJobs() {
    if (!m_pool) {
        return;
    }
    // Chunks that appeared since the last frame: streamed-in columns
    // (integrated at the top of update) or the placement path's
    // synchronous getOrCreateChunk. Everything else is already covered by
    // the lighting pipeline (init/load propagateAll + local updates).
    const std::vector<ChunkPos> positions = m_world.chunkPositions();
    std::map<ColumnPos, std::array<BlockPos, 2>> boxes;
    for (const ChunkPos& c : positions) {
        if (!m_litChunks.insert(c).second) {
            continue;
        }
        const ColumnPos col{c.x, c.z};
        const BlockPos lo{c.x * kChunkSize, c.y * kChunkSize,
                          c.z * kChunkSize};
        const BlockPos hi{lo.x + kChunkSize - 1, lo.y + kChunkSize - 1,
                          lo.z + kChunkSize - 1};
        auto it = boxes.find(col);
        if (it == boxes.end()) {
            boxes.emplace(col, std::array<BlockPos, 2> {lo, hi});
            continue;
        }
        // Merge into the column's box (partial columns: the placement
        // path creates a single chunk).
        BlockPos& blo = it->second[0];
        BlockPos& bhi = it->second[1];
        blo.x = std::min(blo.x, lo.x);
        blo.y = std::min(blo.y, lo.y);
        blo.z = std::min(blo.z, lo.z);
        bhi.x = std::max(bhi.x, hi.x);
        bhi.y = std::max(bhi.y, hi.y);
        bhi.z = std::max(bhi.z, hi.z);
    }
    if (boxes.empty()) {
        return;
    }
    // ONE job per frame, running the region updates sequentially. The
    // region of one box overlaps the region of an adjacent new column
    // (each expands by kLightUpdateRadius), so two lighting jobs must
    // never run concurrently — they would race on the shared light
    // arrays. A single job is serialized by the pool and stays in the
    // worker window (the main thread only reads the World until the next
    // flush). The worker writes light arrays and the dirty flag, which
    // the main thread re-reads at the next frame's meshing (after the
    // flush).
    std::vector<std::array<BlockPos, 2>> jobBoxes;
    jobBoxes.reserve(boxes.size());
    for (const auto& [col, box] : boxes) {
        jobBoxes.push_back(box);
    }
    m_pool->post([this, jobBoxes = std::move(jobBoxes)] {
        for (const auto& box : jobBoxes) {
            Lighting::updateRegion(m_world, m_blocks, box[0], box[1]);
        }
    });
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
    save.playerYaw = m_player->yaw;
    save.playerPitch = m_player->pitch;
    save.playerHealth = m_player->health();
    // Phase 72: the absorption pool (format 12; 0 when no layer is
    // active — the game never writes a pool without its effects).
    save.playerAbsorption = m_player->absorption();
    save.playerHunger = m_player->hunger();
    save.playerSaturation = m_player->saturation();
    save.selectedSlot = static_cast<int>(m_selectedSlot);
    save.slots.resize(m_inventory.size());
    for (std::size_t i = 0; i < save.slots.size(); ++i) {
        save.slots[i] = m_inventory.at(i);
    }
    // The crafting grids (format 3): items left in a grid are the player's
    // — saving them keeps a mid-craft state loadable.
    for (int i = 0; i < CraftingGrid::kSize; ++i) {
        save.invGrid.push_back(m_inventoryGrid.at(i));
        save.tableGrid.push_back(m_tableGrid.at(i));
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
    // The player's active status effects (format 8; Phase 46): names +
    // the real-dt seconds remaining (the serializer name-sorts; the tick
    // phase is deliberately not saved — a reload restarts the next tick
    // at the full interval).
    for (const ActiveEffect& a : m_playerEffects.list()) {
        save.playerEffects.push_back(
            SavedEffect {m_effects.at(a.effect).name, a.remaining});
    }
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
        save.entities.push_back(std::move(se));
    }
    // The modifications: every cell that differs from what the seed
    // generates (block + fluid), sorted by position. Phase 37: large
    // worlds diff on kSaveDiffWorkers threads (P33: ~14 s exit on a
    // ~100k-chunk world single-threaded); the position-sorted output is
    // byte-identical either way.
    auto diff = diffWorld(m_world, makeChunkGenerator(), m_blocks,
                          save.blockEdits, save.fluidEdits, kSaveDiffWorkers);
    if (!diff.ok()) {
        return diff.error();
    }
    return save;
}

Result<void> Game::writeSave() const {
    auto saved = collectSave();
    MC_TRY(saved);
    auto serialized = serializeSave(*saved, m_items);
    MC_TRY(serialized);
    const std::filesystem::path path = savePath();
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream out(path);
    if (!out) {
        return MC_FAIL("cannot open save file for writing: '" + path.string() +
                       "'");
    }
    out << serialized->dump(2) << '\n';
    out.close();
    if (!out) {
        return MC_FAIL("write error for save file '" + path.string() + "'");
    }
    MC_LOG_INFO("World saved to '" + path.string() + "' (" +
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
    return *save;
}

Result<void> Game::applySave(const SaveGame& save) {
    // 1) Cell edits onto the generated world (blocks first, then fluids).
    MC_TRY(applyWorldEdits(save, m_blocks, m_world));
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
    m_inventoryGrid.clear();
    m_tableGrid.clear();
    for (int i = 0; i < CraftingGrid::kSize; ++i) {
        if (i < static_cast<int>(save.invGrid.size())) {
            m_inventoryGrid.setSlot(i, save.invGrid[static_cast<std::size_t>(i)]);
        }
        if (i < static_cast<int>(save.tableGrid.size())) {
            m_tableGrid.setSlot(i, save.tableGrid[static_cast<std::size_t>(i)]);
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
    // 4d) The player's active status effects (format 8; Phase 46). The
    // names are parse-validated against the registry (idForName cannot
    // fail); the tick phase starts at 0 (it is not saved — the next tick
    // lands at the full interval).
    m_playerEffects.clearAll();
    for (const SavedEffect& se : save.playerEffects) {
        m_playerEffects.apply(m_effects.idForName(se.name), se.remaining);
    }
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
    m_prevHealth = m_player->health(); // no hurt event from the load itself
    // 5) Dropped items (with their lifetime/pickup timers restored).
    for (const SavedItemEntity& e : save.entities) {
        const Item& item = m_items.at(m_items.idForName(e.item));
        auto entity = std::make_unique<ItemEntity>(
            m_world, m_blocks, e.item, item.maxStack, e.count, e.pos);
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
                    if (m_blocks.at(id).properties.beaconEffect.empty()) {
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

        // Refresh the title (shows live FPS) at most twice per second.
        if (frameStart - m_lastTitleUpdate >= 0.5) {
            m_lastTitleUpdate = frameStart;
            m_window.setTitle("TokenMine.cpp — " +
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
    // Chunk streaming (Phase 14) — frame pipeline, the race-free part:
    // every worker job from the previous frame (generation, lighting)
    // completes at this flush; from here until the posts at the end of
    // this function the main thread is the sole writer of the World, and
    // after the posts the main thread only reads it (meshing already
    // happened; rendering reads no World data beyond the chunk list).
    if (m_pool) {
        m_pool->flush();
    }
    integrateStreamedColumns();

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
        tickFurnace(st, m_items);
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
    // levels shift the light (water attenuates it), so re-light the touched
    // region before meshing; a settled world changes nothing.
    if (m_frameCount % kFluidTickEveryNFrames == 0) {
        const std::vector<BlockPos> changed =
            FluidSim::tick(m_world, m_blocks);
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
        if (!changed.empty()) {
            BlockPos min = changed.front();
            BlockPos max = changed.front();
            for (const BlockPos& p : changed) {
                min.x = std::min(min.x, p.x);
                min.y = std::min(min.y, p.y);
                min.z = std::min(min.z, p.z);
                max.x = std::max(max.x, p.x);
                max.y = std::max(max.y, p.y);
                max.z = std::max(max.z, p.z);
            }
            Lighting::updateRegion(m_world, m_blocks, min, max);
        }
    }
    // Fire (Phase 64): the per-frame real-dt burn-out countdown (a fire
    // at zero becomes air — no char, like the Phase 58 lava burn) + the
    // water extinguish. Runs AFTER the fluids tick so the newly burned
    // cells are registered and the new water is visible; the block changes
    // it makes are meshed on the next frame (the effects' setBlock
    // pattern). A no-op while the world holds no fire.
    updateFires(dt);
    // Phase 66 (Fire v2): the spread roll (fixed frame step, every
    // fire.spread_interval_frames frames). Runs AFTER the countdown so a
    // fire that just burned out does not spread on its last frame, and
    // the new fires' lights/meshes land on the next frame like any other
    // setBlock. A no-op while the world holds no fire.
    updateFireSpread();

    const auto meshes = updateChunkMeshes();
    if (!meshes.ok()) {
        MC_LOG_ERROR("Chunk meshing failed: " + meshes.error().describe());
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

    // Other entities (Phase 10): per-entity physics, then item merging and
    // despawn (see updateEntities).
    updateEntities(dt);

    // Break particles (Phase 15) + the footstep sound timer.
    m_particles.update(dt);
    updateSoundTimers(dt);

    // Streaming decision + generation jobs: posted after every
    // main-thread World write of this frame (interaction, fluids) and
    // after meshing. The workers run from here until the next flush while
    // the main thread only reads the World (rendering); generation jobs
    // touch no shared state at all (pure generator into the job's own
    // scratch chunks).
    streamChunks();
    // Lighting for chunks that appeared this frame (streamed-in columns,
    // or the placement path's synchronous creation): one worker job
    // (adjacent regions overlap, so the updates run sequentially inside
    // it). The affected chunks render one frame dark (light 0) and are
    // re-meshed with the correct light the frame after the job completes
    // — deterministic, frame-locked by the flush.
    postLightJobs();
}

// ---------------------------------------------------------------------------
// Pointer lock + block interaction (Phase 6)
// ---------------------------------------------------------------------------

void Game::handleInteraction(double dt) {
    const bool locked = m_window.relativeMouseMode();
    if (!locked) {
        // Left click captures the pointer (standard first-person behavior)
        // — but never while dead (the death screen owns the input; R
        // respawns first).
        if (!m_player->isDead() && m_input.wasPressed(MouseButton::Left)) {
            m_window.setRelativeMouseMode(true);
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
        // Milking (Phase 43): a mob whose data pairs 'milk_with' with the
        // selected item's name, under the crosshair (melee targeting:
        // kReach + block occlusion), is MILKED — one held item consumed,
        // one 'milk_gives' item added, the mob unharmed (v1: no damage,
        // death, drop or cooldown). The click is consumed either way
        // (no interact, no placement).
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
                openPanel(OpenPanel::Table);
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
                } else if (b.properties.flamable) {
                    // (b) m_fireId is valid here: a flamable target
                    // exists, and the init cross-check requires the fire
                    // block whenever the registry holds one.
                    setBlock(hit->block.x, hit->block.y, hit->block.z,
                             m_fireId);
                    Lighting::updateLocal(m_world, m_blocks, hit->block);
                    m_sound.play("ignite");
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
    while (m_breakProgress >= m_lastDigProgress + kDigSoundInterval) {
        m_sound.play("dig");
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
    // Phase 46: the active weakness effects subtract their melee penalty
    // (clamped at 0 — a weakened bare hand deals no damage).
    return std::max(0.0f, base -
                               m_playerEffects.totalMeleePenalty(m_effects));
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
    m_entities.push_back(std::make_unique<Projectile>(
        m_world, m_blocks, o, f, item.shootSpeed * power,
        item.shootDamage * power, /*hurtPlayer=*/false, power, item.ammo));
    m_sound.play("shoot");
    resetBowDraw();
}

void Game::resetBowDraw() {
    m_bowDrawing = false;
    m_bowDrawSeconds = 0.0;
}

void Game::setBlock(int x, int y, int z, BlockID id) {
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
    // Phase 83: the beacon tracking set follows the cell (like the crops):
    // a block with a non-empty beaconEffect is a beacon (tracked; its power
    // level is re-derived from the world each frame, so no per-instance
    // state is stored), a cell that stops holding one drops out.
    {
        const BlockPos bpos {x, y, z};
        if (!m_blocks.at(id).properties.beaconEffect.empty()) {
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
    m_sound.play("break");
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
    // Relight the region around the change (a placed occluder blocks light;
    // a placed glowstone is a new light source).
    Lighting::updateLocal(m_world, m_blocks, target);
    // Placing consumes one from the selected stack (the inventory is the
    // source of blocks — no more infinite placement).
    m_inventory.take(m_selectedSlot, 1);
    m_sound.play("place");
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
    // 'milk_gives' item is added; the mob is NOT affected (no damage,
    // death, drop or cooldown — the v1 rule, see docs/KNOWN_ISSUES). The
    // click is consumed whenever the crosshair is on the mob (Minecraft:
    // a failed milk — e.g. a full inventory — still eats the click).
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
    // Init cross-checks guarantee the exists; the guard is a defensive
    // no-op.
    const ItemID gives = m_items.idForName(props.milkGives);
    if (!gives.valid()) {
        return true;
    }
    if (m_inventory.isFull(gives, m_items)) {
        return true; // full: no milk, click consumed (Minecraft)
    }
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
    // init cross-check). The POUR side: the placement-target cell (the
    // face neighbor of the first non-air block) must be air without fluid
    // and not overlap the player: a level-8 source of the item's fluid is
    // placed and the item swaps to the empty bucket. Both play "splash"
    // and consume the click (a bucket never places a block); a no-action
    // case falls through to the normal right-click (placeBlock is a
    // no-op for a material — nothing happens, e.g. on a flowing cell, a
    // source of a fluid the bucket does not name, or an occupied target).
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
    if (!hit) {
        return false; // no block face to pour against: fall through
    }
    // The pour cell is the face neighbor of the hit block (the
    // placement-target convention): air, without fluid (setFluid only
    // fills air cells), inside the Y range and not overlapping the
    // player (canPlaceAt's tests; a material item is not placeable, so
    // the solid-block rejection there is irrelevant here).
    const BlockPos target {
        hit->block.x + static_cast<int>(hit->normal.x),
        hit->block.y + static_cast<int>(hit->normal.y),
        hit->block.z + static_cast<int>(hit->normal.z)};
    if (!canPlaceAt(m_world, m_blocks, target, m_player->aabbMin(),
                    m_player->aabbMax()) ||
        m_world.getBlock(target.x, target.y, target.z) != BlockID::air() ||
        m_world.getFluid(target.x, target.y, target.z) != 0) {
        return false;
    }
    m_world.setFluid(target.x, target.y, target.z, kind, kMaxFluidLevel);
    Lighting::updateLocal(m_world, m_blocks, target);
    finish();
    MC_LOG_DEBUG("poured " + item.poursFluid + " at (" +
                 std::to_string(target.x) + ", " +
                 std::to_string(target.y) + ", " +
                 std::to_string(target.z) + ") (" + item.name + " -> " +
                 item.poursBecomes + ")");
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
        // Apply the beacon's NAMED effect (init-validated to exist). The
        // duration refreshes every frame while in range (the fire-source
        // pattern — standing in the beam keeps the effect alive).
        const int eff = m_effects.idForName(m_blocks.at(bid).properties.beaconEffect);
        if (eff != -1) {
            m_playerEffects.apply(eff, kBeaconEffectDurationSeconds);
        }
    }
    if (m_playerEffects.totalAbsorption(m_effects) != absorbBefore) {
        syncAbsorption();
    }
}

void Game::updateEffects(double dt) {
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
            m_player->restoreSaturation(
                static_cast<float>(item.restoreHunger) * kSaturationFactor);
            // Food that grants a status effect on a full eat (Phase 49,
            // golden apple; the list form Phase 56): applied exactly where
            // the mob melee-hit effect is (name resolved against the
            // EffectRegistry, cross-checked at init — the ammo pattern).
            // The list entries apply IN ORDER (the data's order).
            if (!item.grantsEffect.empty()) {
                m_playerEffects.apply(m_effects.idForName(item.grantsEffect),
                                      item.grantsEffectDuration);
            }
            for (const Item::GrantEffect& g : item.grantsEffects) {
                m_playerEffects.apply(m_effects.idForName(g.name), g.duration);
            }
            syncAbsorption(); // Phase 72: the pool takes the new total
            m_sound.play("eat");
        } else {
            if (!item.returnsItem.empty()) {
                m_inventory.add(m_items.idForName(item.returnsItem), 1,
                                m_items);
            }
            clearStatusEffects();
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
    // re-picked up on the next frame.
    const Vec3 pos = m_player->position();
    m_entities.push_back(std::make_unique<ItemEntity>(m_world, m_blocks,
                                                      item.name, item.maxStack,
                                                      /*count=*/1, pos));
    m_sound.play("pickup");
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
        if (m_inventory.add(iid, item->count(), m_items) < 0) {
            continue; // inventory full: the item stays on the ground
        }
        gone[i] = true;
        m_sound.play("pickup");
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
                            bool transparentPass) {
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
        if (it.type != ItemType::Block) {
            // Tools have no in-world cube (the held-item view arrives with
            // the UI in Phase 15).
            continue;
        }
        const Block& block = m_blocks.at(it.block);
        if (block.properties.transparent != transparentPass) {
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
        m_renderer.draw(mesh, m_basicShader, view, projection, model);
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
    mob->setHitCallback([this, damageScale, hitEffectName,
                         hitEffectDuration](float amount) {
        if (m_player->isDead()) {
            return;
        }
        m_player->damage(amount * damageScale);
        if (!hitEffectName.empty()) {
            m_playerEffects.apply(m_effects.idForName(hitEffectName),
                                  hitEffectDuration);
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
        MobEntity* raw = mob.get();
        mob->setShootCallback(
            [this, raw, arrowSpeed, arrowDamage, arrowAmmo]() {
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
            auto arrow = std::make_unique<Projectile>(
                m_world, m_blocks, eye + dir * kArrowSpawnOffset, dir,
                arrowSpeed, arrowDamage, /*hurtPlayer=*/true,
                /*power=*/1.0f, arrowAmmo);
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
    for (const auto& entity : m_entities) {
        const MobEntity* mob =
            dynamic_cast<const MobEntity*>(entity.get());
        if (mob == nullptr) {
            continue;
        }
        const Mesh& mesh = mobMesh(mob->typeId());
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
    // skyLight = blockLight = 1: the shader's light is
    // max(sky*dayFactor, block) = 1, so the icon stays fully bright at any
    // time of day (like the 2D icon). The normal is +Y (NOT the geometric
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
    if (!m_beaconBeam || m_beaconActive.empty()) {
        return;
    }
    m_basicShader.bind();
    m_basicShader.setUniform1i("u_useTexture", 0);
    m_basicShader.setUniform3f("u_color", kBeaconBeamR, kBeaconBeamG,
                               kBeaconBeamB);
    m_basicShader.setUniform1f("u_alpha", kBeaconBeamAlpha);
    m_basicShader.setUniform1f("u_dayFactor", m_dayCycle.dayFactor());
    m_renderer.enableBlending();
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
        m_renderer.draw(m_beaconBeamMesh, m_basicShader, view, projection,
                        model);
    }
    m_renderer.setCulling(true);
    m_renderer.disableBlending();
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
    if (!isBlock && !hasIcon) {
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

void Game::handlePanelClicks() {
    // The layout and the mouse hit tests both live in UI pixels (Phase
    // 34): InventoryLayout / FurnaceLayout are pure geometry, so passing
    // the UI space scales the panel identically to the rendered one.
    const UiSize space = uiSize();
    const int w = space.width;
    const int h = space.height;

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
        if (m_input.wasPressed(MouseButton::Left)) {
            m_furnaceUi.leftClick(slot, fslot);
            return;
        }
        if (m_input.wasPressed(MouseButton::Right)) {
            m_furnaceUi.rightClick(slot, fslot);
        }
        return;
    }

    const bool table = m_openPanel == OpenPanel::Table;
    const PanelKind kind =
        table ? PanelKind::CraftingTable : PanelKind::Inventory;
    std::vector<const Recipe*> shapeless;
    if (!table) {
        shapeless = shapelessRecipes();
    }
    const InventoryLayout layout =
        InventoryLayout::compute(w, h, static_cast<int>(shapeless.size()),
                                 kind);
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
    if (m_input.wasPressed(MouseButton::Left)) {
        // Craft rows first (they sit beside the slots, so the hit test is
        // unambiguous): a click crafts when the inventory can satisfy the
        // recipe (all-or-nothing, see craft()).
        if (!table) {
            const int row = layout.recipeRowAt(px, py);
            if (row >= 0) {
                const Recipe& recipe = *shapeless[static_cast<std::size_t>(row)];
                if (canCraft(m_inventory, recipe, m_items)) {
                    auto crafted = craft(m_inventory, recipe, m_items);
                    if (crafted.ok()) {
                        m_sound.play("craft");
                    } else {
                        MC_LOG_ERROR("Craft failed: " + crafted.error().describe());
                    }
                }
                return;
            }
        }
        if (ui.leftClick(layout.slotAt(px, py), gridSlot, onOutput)) {
            m_sound.play("craft");
        }
        return;
    }
    if (m_input.wasPressed(MouseButton::Right)) {
        if (ui.rightClick(layout.slotAt(px, py), gridSlot, onOutput)) {
            m_sound.play("craft");
        }
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
        m_playerEffects.clearAll();
        syncAbsorption(); // Phase 72: the pool dies with the player
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
}

void Game::drawParticles(const Mat4& view, const Mat4& projection,
                         bool transparentPass) {
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
        if (block.properties.transparent != transparentPass) {
            continue;
        }
        for (const CubeFace& face : kCubeFaces) {
            // Cross blocks (Phase 45) carry no cube faces: the particle
            // cube uses the cross texture on every face (the same
            // single-lookup the item-mesh and chunk-mesher cross branches
            // use — cubeFaceTexture would return the empty top/side/bottom
            // name and the atlas lookup would throw).
            const std::string& tex =
                block.model.shape == BlockModel::Shape::Cross
                    ? block.model.textures.cross
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
            drawStrip(gx0, hungerIcons, m_hungerTexture, m_player->hunger(),
                      Vec4 {1.0f, 1.0f, 1.0f, 1.0f});
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
        const std::string died = "You died!";
        const std::string hint = "Press R to respawn";
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
        addText("Furnace", layout.title.x, layout.title.y + titleOffset,
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
        const std::string hint = "E to close";
        addText(hint,
                layout.panel.x + static_cast<int>(std::lround(
                    (layout.panel.w - textWidth(hint, s)) / 2.0)),
                layout.panel.y + layout.panel.h - L(20), s, kTextDim);
    }

    // -- crafting panel (while open): the E inventory (2x2 grid + the
    // shapeless recipe list) or the crafting table (3x3 grid). ---------------
    if (panelOpen() && m_openPanel != OpenPanel::Furnace) {
        const bool table = m_openPanel == OpenPanel::Table;
        const PanelKind kind =
            table ? PanelKind::CraftingTable : PanelKind::Inventory;
        std::vector<const Recipe*> shapeless;
        if (!table) {
            shapeless = shapelessRecipes();
        }
        const InventoryLayout layout =
            InventoryLayout::compute(w, h,
                                     static_cast<int>(shapeless.size()),
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
        addText(table ? "Crafting Table" : "Inventory", layout.title.x,
                layout.title.y + titleOffset,
                InventoryLayout::kTitleFontScale * s, kText);
        if (!table) {
            addText("Crafting", layout.craftHeader.x,
                    layout.craftHeader.y + titleOffset,
                    InventoryLayout::kTitleFontScale * s, kText);
        }

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

        // Crafting rows (E panel only): output icon + "name xN" (white when
        // craftable, dim otherwise) + the ingredient list.
        for (std::size_t i = 0; i < layout.recipes.size(); ++i) {
            const Recipe& recipe = *shapeless[static_cast<std::size_t>(i)];
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
            addText("needs: " + needs, r.x + L(32), r.y + L(28), s,
                    kTextDim);
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
        const std::string hint = "E to close";
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
            info.hunger = m_player->hunger();
            info.maxHunger = m_player->maxHunger();
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
    // the block atlas, so re-bind it.
    if (!iconDraws.empty()) {
        m_atlas.texture().bind();
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

    m_atlas.texture().bind();

    // Chunk passes: all opaque geometry first, then the transparent
    // (blended) geometry, so blending composites over the opaque scene.
    const std::vector<ChunkPos> positions = m_world.chunkPositions();
    for (const ChunkPos& pos : positions) {
        const auto it = m_chunkMeshes.find(pos);
        if (it == m_chunkMeshes.end() || !it->second.opaque.has_value()) {
            continue;
        }
        m_renderer.draw(*it->second.opaque, m_basicShader, view, projection,
                        chunkModel(pos));
    }
    // Dropped items (Phase 10): the opaque ones with the chunk pass; the
    // transparent ones (leaves, glass) join the blending pass below.
    drawItemEntities(view, projection, false);
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
    drawParticles(view, projection, false);

    m_renderer.enableBlending();
    // Sky objects (unlit billboarded quads at kSkyDistance): drawn after
    // the opaque pass so the depth test occludes them behind terrain, and
    // before the transparent pass (they never overlap chunk geometry in
    // depth — they sit behind everything in the demo area).
    drawSkyObjects(view, projection, h);
    for (const ChunkPos& pos : positions) {
        const auto it = m_chunkMeshes.find(pos);
        if (it == m_chunkMeshes.end() || !it->second.transparent.has_value()) {
            continue;
        }
        m_renderer.draw(*it->second.transparent, m_basicShader, view,
                        projection, chunkModel(pos));
    }
    drawItemEntities(view, projection, true);
    drawParticles(view, projection, true);
    // Beacon beams (Phase 83): the translucent light columns from active
    // beacons, blended + depth-occluded like the other world geometry.
    // No-op without an active beacon (baseline byte-identical).
    drawBeaconBeams(view, projection);
    // The break crack + target outline (Phase 15): world-space, blended,
    // depth test still on (the overlay cubes sit just outside the block).
    drawWorldOverlay(view, projection);
    m_renderer.disableBlending();

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
