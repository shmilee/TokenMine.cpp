#include "core/game.hpp"

#include <GL/glew.h>
#include <SDL3/SDL.h>

#include <cmath>
#include <cstring>
#include <fstream>

#include "core/json.hpp"
#include "core/png.hpp"

namespace mc {

namespace {

constexpr float kFovDegrees = 70.0f;
constexpr float kNear = 0.1f;
constexpr float kFar = 1000.0f;
constexpr Vec4 kSkyColor {0.47f, 0.71f, 0.95f, 1.0f};

// Demo area: 4x4 chunk columns covering world x,z in [-32, 32).
constexpr int kDemoChunkMin = -2;
constexpr int kDemoChunkMax = 1;

// How far the crosshair raycast reaches (blocks; Minecraft's reach).
constexpr float kReach = 5.0f;

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

    // -- hotbar (temporary selection list, data-driven; Phase 11 replaces it
    //    with the item/inventory system) ------------------------------------
    auto hotbarText = m_assets.loadText("data/hotbar.json");
    MC_TRY(hotbarText);
    auto hotbarJson = Json::parse(*hotbarText);
    MC_TRY(hotbarJson);
    const Json* list = hotbarJson->find("blocks");
    if (list == nullptr || !list->isArray()) {
        return MC_FAIL("hotbar.json: 'blocks' array is required");
    }
    m_hotbar.clear();
    for (std::size_t i = 0; i < list->size(); ++i) {
        const Json& entry = list->at(i);
        if (!entry.isString()) {
            return MC_FAIL("hotbar.json: entry #" + std::to_string(i) +
                           " must be a block name string");
        }
        const BlockID id = m_blocks.idForName(entry.asString());
        if (!id.valid()) {
            return MC_FAIL("hotbar.json: unknown block '" + entry.asString() +
                           "'");
        }
        m_hotbar.push_back(id);
    }
    if (m_hotbar.empty()) {
        return MC_FAIL("hotbar.json: 'blocks' must not be empty");
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

    m_world.setChunkGenerator([this](Chunk& chunk, const ChunkPos& pos) {
        // The generator is stateless and only writes into the given chunk
        // (never back into the World), which the World contract requires.
        m_generator->generateChunk(chunk, pos);
    });
    for (int cx = kDemoChunkMin; cx <= kDemoChunkMax; ++cx) {
        for (int cz = kDemoChunkMin; cz <= kDemoChunkMax; ++cz) {
            for (int cy = 0; cy * kChunkSize < kWorldHeight; ++cy) {
                m_world.getOrCreateChunk({cx, cy, cz});
            }
        }
    }

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
// Chunk meshing (CPU, game thread; Phase 14 moves it to worker threads)
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

    m_window.shutdown();
    m_initialized = false;
    MC_LOG_INFO("Game shut down cleanly");
    return 0;
}

void Game::update(double dt) {
    handleInteraction();

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
}

// ---------------------------------------------------------------------------
// Pointer lock + block interaction (Phase 6)
// ---------------------------------------------------------------------------

void Game::handleInteraction() {
    const bool locked = m_window.relativeMouseMode();
    if (!locked) {
        // Left click captures the pointer (standard first-person behavior).
        if (m_input.wasPressed(MouseButton::Left)) {
            m_window.setRelativeMouseMode(true);
        }
        return;
    }

    if (m_input.wasPressed(GameKey::Escape)) {
        m_window.setRelativeMouseMode(false);
        return;
    }
    if (m_input.wasPressed(MouseButton::Left)) {
        breakBlock();
    }
    if (m_input.wasPressed(MouseButton::Right)) {
        placeBlock();
    }
    if (m_input.wasPressed(GameKey::Tab)) {
        cycleHotbar();
    }
}

std::optional<BlockHit> Game::targetBlock() const {
    return raycastBlock(m_world, m_camera.position, m_camera.forward(),
                        kReach);
}

void Game::breakBlock() {
    const std::optional<BlockHit> hit = targetBlock();
    if (!hit) {
        return;
    }
    const BlockID id = m_world.getBlock(hit->block.x, hit->block.y,
                                        hit->block.z);
    // Breakability is data-driven: water/bedrock set destroyable=false.
    if (!m_blocks.at(id).properties.destroyable) {
        return;
    }
    // setBlock marks the chunk (and face-sharing neighbors) dirty; the mesh
    // rebuild happens on the next frame.
    m_world.setBlock(hit->block.x, hit->block.y, hit->block.z, BlockID::air());
}

void Game::placeBlock() {
    if (m_hotbar.empty()) {
        return;
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
    m_world.setBlock(target.x, target.y, target.z,
                     m_hotbar[m_hotbarSelected]);
}

void Game::cycleHotbar() {
    if (m_hotbar.empty()) {
        return;
    }
    m_hotbarSelected = (m_hotbarSelected + 1) % m_hotbar.size();
}

void Game::render() {
    const int w = m_window.width();
    const int h = m_window.height();

    m_renderer.beginFrame(w, h);
    m_renderer.clear(kSkyColor);

    const Mat4 projection = Mat4::perspective(
        kFovDegrees * static_cast<float>(M_PI) / 180.0f,
        static_cast<float>(w) / static_cast<float>(h), kNear, kFar);
    const Mat4 view = m_camera.viewMatrix();

    // Shared uniforms (program state persists across draws).
    m_basicShader.bind();
    m_basicShader.setUniform1i("u_texture", 0);
    m_basicShader.setUniform1i("u_useTexture", 1);
    m_basicShader.setUniform3f("u_lightDir", -0.4f, 0.8f, -0.3f);

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

    m_renderer.enableBlending();
    for (const ChunkPos& pos : positions) {
        const auto it = m_chunkMeshes.find(pos);
        if (it == m_chunkMeshes.end() || !it->second.transparent.has_value()) {
            continue;
        }
        m_renderer.draw(*it->second.transparent, m_basicShader, view,
                        projection, chunkModel(pos));
    }
    m_renderer.disableBlending();

    m_renderer.endFrame();
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
