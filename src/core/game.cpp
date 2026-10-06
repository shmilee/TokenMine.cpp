#include "core/game.hpp"

#include <GL/glew.h>
#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
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
#include "world/noise.hpp"
#include "ui/font.hpp"
#include "ui/inventory_ui.hpp"

namespace mc {

namespace {

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

// Mobs (Phase 20): the player's attack cadence + the knockback a hit
// applies (the horizontal speed along the aim + a vertical lift; the
// MobEntity's knockback window decays it).
constexpr double kSwingInterval = 0.25;  // seconds between attack hits
constexpr float kKnockbackSpeed = 4.0f;  // horizontal knockback (blocks/s)
constexpr float kKnockbackLift = 2.5f;   // vertical knockback (blocks/s)

// Bow (Phase 21): the arrow spawns this far in front of the eye (the
// launch origin never sits inside the camera near plane), and a hit arrow
// knocks back with the melee constants above (along the arrow's horizontal
// velocity).
constexpr float kArrowSpawnOffset = 0.3f;

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

// Phase 15 UI (screen-space, pixels; the font is 8x8 at scale 1).
constexpr int kHotbarSlotSize = 44;
constexpr int kHotbarSlotGap = 2;
constexpr int kHotbarBottomMargin = 8;
constexpr int kHotbarIconPx = 30;
constexpr int kHeldIconPx = 76;
constexpr int kHeldIconOffset = 72; // center of the held icon from the corner
// Bow draw (Phase 24): the held icon is pulled this many pixels toward
// the screen center at full charge (the v1 draw indicator).
constexpr float kBowDrawPullPx = 64.0f;
constexpr int kF3Margin = 8;
constexpr int kF3LineHeight = 14;
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
    // grass -> dirt; ores drop material items, e.g. iron_ore -> iron_ingot).
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
    for (const std::string& name : m_spawnConfig.mobs) {
        if (m_mobTypes.findByName(name) == nullptr) {
            return MC_FAIL("mob_spawning.json: unknown mob '" + name + "'");
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
            if (drop.count > m_items.at(dropId).maxStack) {
                return MC_FAIL("mob '" + mob.name + "' drops " +
                               std::to_string(drop.count) +
                               " of '" + drop.item +
                               "' but its max stack is " +
                               std::to_string(m_items.at(dropId).maxStack));
            }
        }
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
    const Player::Params playerDims; // default dimensions for the room check
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
    m_player.emplace(m_world, m_blocks, spawnPos);
    m_player->pitch = -0.25f; // look slightly down at the landscape
    m_camera.position = m_player->eyePosition();
    m_camera.yaw = m_player->yaw;
    m_camera.pitch = m_player->pitch;

    // -- save load (Phase 13): a saved state REPLACES the fresh world ----
    // Runs after generation + spawn: the cell edits apply on top of the
    // generated terrain, and the saved player/inventory/entities/time win
    // over the fresh ones (--time only affects fresh worlds).
    MC_TRY(tryLoadSave());

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
    // generates (block + fluid), sorted by position.
    auto diff = diffWorld(m_world, makeChunkGenerator(), m_blocks,
                          save.blockEdits, save.fluidEdits);
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
    auto save = parseSave(*parsed, m_items, m_blocks, m_generator->seed());
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
    // 3) Player. teleportTo zero's the velocity; when the save was taken
    //    at rest the one-frame gravity re-settle is position-exact (the
    //    floor plane is an integer).
    m_player->teleportTo(save.playerPos);
    m_player->yaw = save.playerYaw;
    m_player->pitch = save.playerPitch;
    // Health (Phase 16): a format-1 save has none (-1 = full); a saved 0
    // reloads the player dead (the death screen offers R).
    m_player->setHealth(save.playerHealth < 0.0f
                            ? m_player->maxHealth()
                            : save.playerHealth);
    // Hunger (Phase 23): format 1-3 saves have none (-1 = full).
    m_player->setHunger(save.playerHunger < 0.0f
                            ? m_player->maxHunger()
                            : save.playerHunger);
    // Saturation (Phase 28): format 1-4 saves have none (-1 = no buffer).
    m_player->setSaturation(
        save.playerSaturation < 0.0f ? 0.0f : save.playerSaturation);
    m_prevHealth = m_player->health(); // no hurt event from the load itself
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
            m_window.setTitle("MineCraft — " +
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
        const std::vector<BlockPos> changed = FluidSim::tick(m_world);
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
        // Food (Phase 23): a selected food item is EATEN, not used — the
        // click is consumed (no interact, no placement). At full hunger the
        // eat is a no-op but the click is still consumed (Minecraft: you
        // cannot place while "eating" a full bar).
        if (tryEat()) {
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
    const double duration =
        breakDurationSeconds(block.properties.hardness, selectedBreakSpeed());
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
    if (stack.empty()) {
        return 1.0f;  // bare hand
    }
    return m_items.at(stack.item).attackDamage;  // data (default 1.0)
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
        m_inventory.take(static_cast<std::size_t>(slot), 1);
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
    m_entities.push_back(std::make_unique<Projectile>(
        m_world, m_blocks, o, f, item.shootSpeed * power,
        item.shootDamage * power));
    m_sound.play("shoot");
    resetBowDraw();
}

void Game::resetBowDraw() {
    m_bowDrawing = false;
    m_bowDrawSeconds = 0.0;
}

void Game::breakBlock(const BlockHit& hit) {
    const BlockID id = m_world.getBlock(hit.block.x, hit.block.y, hit.block.z);
    // setBlock marks the chunk (and face-sharing neighbors) dirty; the mesh
    // rebuild happens on the next frame.
    m_world.setBlock(hit.block.x, hit.block.y, hit.block.z, BlockID::air());
    // Relight the region around the change (a broken occluder lets light
    // in; the broken cell stops blocking).
    Lighting::updateLocal(m_world, m_blocks, hit.block);

    // Drops are data-driven: `properties.drops` names an item (validated
    // against the ItemRegistry at init). The item spawns at the block
    // center with zero velocity, so its rest position is exact and headless
    // runs stay byte-reproducible (see ItemEntity).
    const Block& block = m_blocks.at(id);
    if (!block.properties.drops.empty()) {
        const Item& dropItem = m_items.at(m_items.idForName(block.properties.drops));
        const Vec3 dropPos {static_cast<float>(hit.block.x) + 0.5f,
                            static_cast<float>(hit.block.y) + 0.5f,
                            static_cast<float>(hit.block.z) + 0.5f};
        // dropsCount (validated at init to fit the item's max stack).
        m_entities.push_back(std::make_unique<ItemEntity>(
            m_world, m_blocks, dropItem.name, dropItem.maxStack,
            block.properties.dropsCount, dropPos));
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
    m_world.setBlock(target.x, target.y, target.z, item.block);
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
        // The channel finished: eat one from the captured slot (its
        // validity was just re-checked) — hunger plus the hidden
        // saturation buffer (core/food.hpp), then the eat sound.
        const Item& item = m_items.at(m_eatItem);
        m_inventory.take(m_eatSlot, 1);
        m_player->restoreHunger(static_cast<float>(item.restoreHunger));
        m_player->restoreSaturation(
            static_cast<float>(item.restoreHunger) * kSaturationFactor);
        m_sound.play("eat");
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

float Game::selectedBreakSpeed() const {
    const ItemStack& stack = m_inventory.at(m_selectedSlot);
    if (stack.empty()) {
        return 1.0f; // bare hands
    }
    const Item& item = m_items.at(stack.item);
    return item.type == ItemType::Tool ? item.breakSpeed : 1.0f;
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
    for (auto& entity : m_entities) {
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
    // damage + the melee-style knockback along the arrow's horizontal
    // velocity, and the arrow is consumed (the despawn pass removes it
    // this frame). Arrows fly through the player and every other entity
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
            const Vec3 v = arrow->velocity();
            Vec3 kb {v.x, 0.0f, v.z};
            const float len = std::sqrt(kb.x * kb.x + kb.z * kb.z);
            if (len > 1e-4f) {
                kb.x *= kKnockbackSpeed / len;
                kb.z *= kKnockbackSpeed / len;
            }
            kb.y = kKnockbackLift;
            mob->addKnockback(kb);
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
            // in list order at the same rest position.
            for (const MobDrop& drop : mob->def().properties.drops) {
                mobDrops.push_back({drop.item, drop.count, mob->position()});
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
    // Deterministic type pick among the data-listed mobs (the seed mixes
    // the world seed with the frame, so the choice is a pure function of
    // the run). Note: the pick happens BEFORE the light gate, so an
    // attempt whose type cannot spawn at the current light is simply
    // wasted (e.g. a zombie pick by day) — the cadence, not the type,
    // decides when attempts happen.
    const std::uint64_t pick =
        splitmix64(static_cast<std::uint64_t>(m_generator->seed()) ^
                   (std::uint64_t{0x9E3779B97F4A7C15} *
                    static_cast<std::uint64_t>(m_frameCount)));
    const Mob& def = m_mobTypes.at(
        m_mobTypes.idForName(m_spawnConfig.mobs[static_cast<std::size_t>(
            pick % m_spawnConfig.mobs.size())]));

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
    // The AI's landed attacks damage the player (Phase 16's damage path
    // owns the flash/sound/death — updateSurvival polls the health).
    // Hostile damage is scaled by the difficulty (Phase 25); passive mobs
    // (attack_damage 0 in the data) pass the scale untouched.
    const float damageScale = def.properties.hostile
        ? hostileDamageScale(m_difficulty)
        : 1.0f;
    mob->setHitCallback([this, damageScale](float amount) {
        if (!m_player->isDead()) {
            m_player->damage(amount * damageScale);
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
        MobEntity* raw = mob.get();
        mob->setShootCallback([this, raw, arrowSpeed, arrowDamage]() {
            if (m_player->isDead() || raw->dead()) {
                return;
            }
            const Vec3 eye = raw->eyePosition();
            Vec3 toPlayer = m_player->eyePosition() - eye;
            const float dist = std::sqrt(dot(toPlayer, toPlayer));
            if (dist < 1e-4f) {
                return;
            }
            // Arc compensation (the Minecraft skeleton leads the target):
            // the arrow drops 0.5 * g * t^2 while it flies t = dist/speed
            // seconds, so aim ABOVE the player's eye by exactly that drop.
            // Without it a straight-line aim only lands within ~6 blocks
            // (drop > eye height past that) and a skeleton standing at its
            // preferred range would never hit.
            const float flightTime = dist / arrowSpeed;
            toPlayer.y += 0.5f * Projectile::kGravity * flightTime *
                           flightTime;
            const Vec3 dir =
                toPlayer * (1.0f / std::sqrt(dot(toPlayer, toPlayer)));
            auto arrow = std::make_unique<Projectile>(
                m_world, m_blocks, eye + dir * kArrowSpawnOffset, dir,
                arrowSpeed, arrowDamage, /*hurtPlayer=*/true);
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
    const int w = m_window.width();
    const int h = m_window.height();
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
    const Vec2 mp = m_input.mousePosition();
    const int px = static_cast<int>(mp.x);
    const int py = static_cast<int>(mp.y);
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
        m_window.setRelativeMouseMode(false);
        m_sound.play("death");
    }
    m_prevHealth = hp;
    if (dead && m_input.wasPressed(GameKey::R)) {
        m_player->respawn();
        m_prevHealth = m_player->health(); // no hurt flash on respawn
    }
}

Mat4 Game::uiProjection(int w, int h) const {
    // Screen pixels, y-down (y = 0 is the window top). The depth range
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
            block.properties.hardness, selectedBreakSpeed());
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
            const std::string& tex =
                cubeFaceTexture(block, face.textureSlot);
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
                v.normal = face.normal;
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
    const int w = m_window.width();
    const int h = m_window.height();
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
    auto addText = [&](const std::string& text, int x, int y, int scale,
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

    const Vec2 mouse = m_input.mousePosition();
    const int mx = static_cast<int>(mouse.x);
    const int my = static_cast<int>(mouse.y);

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
                addText(count, sx + kHotbarSlotSize - 4 - textWidth(count),
                        y0 + kHotbarSlotSize - 4 - kGlyphSize, 1, kText);
            }
        }
        // Health bar (Phase 16) + hunger bar (Phase 23): 10 hearts above
        // the hotbar, 2 HP each, three-state tiles (full / half / empty);
        // the hunger is 10 drumsticks (2 each, same tiles) on the SAME row
        // to the right of the hearts. Both strips share the icon metrics.
        {
            constexpr int kIconPx = 16;
            constexpr int kIconGap = 2;
            constexpr int kIconCount = 10;
            constexpr int kStripGap = 6; // between the hearts and hunger
            const int stripW = kIconCount * kIconPx +
                               (kIconCount - 1) * kIconGap;
            // The hearts stay centered (the Phase 16 layout); the hunger
            // strip starts just past their right edge.
            const int hx0 = (w - stripW) / 2;
            const int gx0 = hx0 + stripW + kStripGap;
            const int hy = y0 - kIconPx - 6;
            // One pass per strip: tile t occupies u in [t/3, (t+1)/3].
            auto drawStrip = [&](int x0, const Texture& tex, float value) {
                UiBatch& batch =
                    batchFor(Vec4 {1.0f, 1.0f, 1.0f, 1.0f}, true, &tex);
                for (int i = 0; i < kIconCount; ++i) {
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
            drawStrip(hx0, m_healthTexture, m_player->health());
            drawStrip(gx0, m_hungerTexture, m_player->hunger());
        }
        // Crosshair: a 2px thin cross at the screen center.
        UiBatch& cross = batchFor(kCrosshair, false, nullptr);
        addQuad(cross, w / 2 - 5, h / 2 - 1, 11, 2);
        addQuad(cross, w / 2 - 1, h / 2 - 5, 2, 11);
        // Held item: a large icon in the bottom-right corner. While the
        // bow is drawn (Phase 24) the icon is pulled toward the screen
        // center by the charge power; while food is being eaten (Phase 28)
        // by the eat channel's progress — the v1 draw/eat indicators (no
        // 3D hand model; see docs/KNOWN_ISSUES.md).
        float heldX = static_cast<float>(w - kHeldIconOffset);
        float heldY = static_cast<float>(h - kHeldIconOffset);
        float heldPull = 0.0f;
        if (m_bowDrawing) {
            heldPull = bowPower(m_bowDrawSeconds);
        } else if (m_eatChannel.active) {
            heldPull = m_eatChannel.progress();
        }
        if (heldPull > 0.0f) {
            const Vec2 toCenter {static_cast<float>(w / 2) - heldX,
                                 static_cast<float>(h / 2) - heldY};
            const float len =
                std::sqrt(toCenter.x * toCenter.x + toCenter.y * toCenter.y);
            if (len > 1e-4f) {
                const float pull = kBowDrawPullPx * heldPull;
                heldX += toCenter.x / len * pull;
                heldY += toCenter.y / len * pull;
            }
        }
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
        addText(died, (w - textWidth(died, 2)) / 2, h / 2 - 24, 2,
                Vec4 {1.0f, 0.35f, 0.35f, 1.0f});
        addText(hint, (w - textWidth(hint)) / 2, h / 2 + 8, 1, kText);
    }

    // -- crafting panel (while open): the E inventory (2x2 grid + the
    // shapeless recipe list) or the crafting table (3x3 grid). ---------------
    if (panelOpen()) {
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

        // Panel background + titles.
        addQuad(batchFor(kPanel, false, nullptr), layout.panel.x,
                layout.panel.y, layout.panel.w, layout.panel.h);
        addText(table ? "Crafting Table" : "Inventory", layout.title.x,
                layout.title.y + 6, 2, kText);
        if (!table) {
            addText("Crafting", layout.craftHeader.x,
                    layout.craftHeader.y + 6, 2, kText);
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
                     static_cast<float>(r.y + r.h / 2), 34);
            if (!stack.empty() &&
                stack.count < m_items.at(stack.item).maxStack) {
                const std::string count = std::to_string(stack.count);
                addText(count, r.x + r.w - 4 - textWidth(count),
                        r.y + r.h - 4 - kGlyphSize, 1, kText);
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
                     static_cast<float>(r.y + r.h / 2), 34);
            if (!stack.empty() &&
                stack.count < m_items.at(stack.item).maxStack) {
                const std::string count = std::to_string(stack.count);
                addText(count, r.x + r.w - 4 - textWidth(count),
                        r.y + r.h - 4 - kGlyphSize, 1, kText);
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
                         static_cast<float>(r.y + r.h / 2), 34);
                if (out.count < m_items.at(out.item).maxStack) {
                    const std::string count = std::to_string(out.count);
                    addText(count, r.x + r.w - 4 - textWidth(count),
                            r.y + r.h - 4 - kGlyphSize, 1, kText);
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
                     static_cast<float>(r.x + 14),
                     static_cast<float>(r.y + r.h / 2), 28);
            addText(m_items.at(recipe.output).name + " x" +
                        std::to_string(recipe.outputCount),
                    r.x + 32, r.y + 6, 1, outColor);
            std::string needs;
            for (const auto& [item, count] : recipe.ingredients) {
                if (!needs.empty()) {
                    needs += ", ";
                }
                needs += m_items.at(item).name + " x" +
                         std::to_string(count);
            }
            addText("needs: " + needs, r.x + 32, r.y + 24, 1, kTextDim);
        }

        // The hovered slot/cell's item name + the carried stack at the
        // cursor.
        if (hoverSlot >= 0) {
            const ItemStack& stack = m_inventory.at(hoverSlot);
            if (!stack.empty()) {
                addText(m_items.at(stack.item).name, mx + 12, my + 12, 1,
                        kText);
            }
        } else if (hoverCell >= 0) {
            const ItemStack& stack =
                ui.grid().at(layout.gridSlots[static_cast<std::size_t>(hoverCell)]);
            if (!stack.empty()) {
                addText(m_items.at(stack.item).name, mx + 12, my + 12, 1,
                        kText);
            }
        }
        if (ui.carrying()) {
            drawIcon(ui.carry(), static_cast<float>(mx),
                     static_cast<float>(my), 34);
        }
        // Close hint, centered at the panel bottom.
        const std::string hint = "E to close";
        addText(hint, layout.panel.x + (layout.panel.w - textWidth(hint, 1)) / 2,
                layout.panel.y + layout.panel.h - 16, 1, kTextDim);
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
            info.hunger = m_player->hunger();
            info.maxHunger = m_player->maxHunger();
        }
        info.difficulty = difficultyName(m_difficulty);
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
