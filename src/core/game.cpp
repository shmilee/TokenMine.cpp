#include "core/game.hpp"

#include <GL/glew.h>
#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <numbers>
#include <sstream>

#include "core/json.hpp"
#include "core/png.hpp"

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

// A dropped item is picked up when its bottom-center is within this
// distance of the player's bottom-center (and past the item's pickup
// delay). Minecraft pulls items in gradually (magnetism); the fixed
// radius is the v1 — see KNOWN_ISSUES.md.
constexpr float kPickupDistance = 1.0f;

// Dropped-item animation (Phase 10): derived from the frame counter, not
// the wall clock, so headless runs stay byte-reproducible. At 60 FPS the
// pickup bobs with a 1.5 s period (amplitude 0.05 blocks) and spins once
// every 4 s, like Minecraft's item pickup.
constexpr int kItemBobPeriodFrames = 90;
constexpr int kItemSpinPeriodFrames = 240;
constexpr float kItemBobAmplitude = 0.05f;

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
    auto atlasLoad = m_atlas.loadDirectory(m_assets, "textures/blocks");
    MC_TRY(atlasLoad);
    auto atlasUpload = m_atlas.upload();
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
        m_inventory.set(i, ItemStack {iid, m_items.at(iid).maxStack});
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
    save.selectedSlot = static_cast<int>(m_selectedSlot);
    save.slots.resize(m_inventory.size());
    for (std::size_t i = 0; i < save.slots.size(); ++i) {
        save.slots[i] = m_inventory.at(i);
    }
    for (const std::unique_ptr<Entity>& e : m_entities) {
        // The game only ever spawns ItemEntities; anything else means a
        // code path this save format cannot express — fail loudly.
        const ItemEntity* item = dynamic_cast<const ItemEntity*>(e.get());
        if (item == nullptr) {
            return MC_FAIL("cannot save: an entity of a kind this build "
                           "does not persist exists");
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
    m_camera.position = m_player->eyePosition();
    m_camera.yaw = m_player->yaw;
    m_camera.pitch = m_player->pitch;
    // 4) Inventory (replaces the hotbar.json fill) + selection.
    for (std::size_t i = 0; i < m_inventory.size(); ++i) {
        m_inventory.set(i, save.slots[i]);
    }
    m_selectedSlot = static_cast<std::size_t>(save.selectedSlot);
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
        // ESC exits only while the pointer is free; while captured it
        // releases the pointer instead (handled in update).
        if (m_input.quitRequested() ||
            (!m_window.relativeMouseMode() &&
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

    handleInteraction(dt);

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
    m_player->update(dt, m_input, m_window.relativeMouseMode());
    m_camera.position = m_player->eyePosition();
    m_camera.yaw = m_player->yaw;
    m_camera.pitch = m_player->pitch;

    // Other entities (Phase 10): per-entity physics, then item merging and
    // despawn (see updateEntities).
    updateEntities(dt);

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
        // Left click captures the pointer (standard first-person behavior).
        if (m_input.wasPressed(MouseButton::Left)) {
            m_window.setRelativeMouseMode(true);
        }
        resetBreaking();
        return;
    }

    if (m_input.wasPressed(GameKey::Escape)) {
        m_window.setRelativeMouseMode(false);
        resetBreaking();
        return;
    }
    if (m_input.wasPressed(MouseButton::Right)) {
        placeBlock();
    }
    if (m_input.wasPressed(GameKey::Tab)) {
        cycleHotbar();
    }
    if (m_input.wasPressed(GameKey::Q)) {
        dropSelectedItem();
    }
    // Breaking is hold-driven (Phase 11): progress accumulates while the
    // button stays down on the same target block.
    if (m_input.isDown(MouseButton::Left)) {
        updateBreaking(dt);
    } else {
        resetBreaking();
    }
}

std::optional<BlockHit> Game::targetBlock() const {
    return raycastBlock(m_world, m_camera.position, m_camera.forward(),
                        kReach);
}

void Game::resetBreaking() {
    m_breakTarget.reset();
    m_breakProgress = 0.0;
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
    if (m_breakProgress >= duration) {
        breakBlock(*hit);
        resetBreaking();
    }
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
}

void Game::cycleHotbar() {
    m_selectedSlot =
        (m_selectedSlot + 1) % static_cast<std::size_t>(Inventory::kHotbarSize);
    // The selected item (and its break speed) changed: restart any break
    // in progress (Minecraft behavior).
    resetBreaking();
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
    for (auto& entity : m_entities) {
        entity->update(dt);
    }
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
    const Vec3 playerPos = m_player->position();
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
    }
    // Despawn: merged-away / picked-up + aged-out / impact / below the
    // kill plane.
    for (std::size_t i = 0; i < n; ++i) {
        if (!gone[i] && m_entities[i]->shouldDespawn()) {
            gone[i] = true;
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
    m_renderer.disableBlending();

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
