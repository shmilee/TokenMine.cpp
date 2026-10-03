// Top-level game object: owns configuration, assets, window, input, camera,
// renderer and the voxel world; runs the main loop.
//
// Phase 4 scope: chunk-based voxel world (World/Chunk, 16^3 sections,
// face-culled chunk meshing) filled by the seeded, data-driven
// WorldGenerator (terrain + biomes + caves + ore veins + trees). Each chunk
// owns up to two GPU meshes (opaque + transparent/blended); a chunk's
// meshes are rebuilt lazily when it is marked dirty (first meshing or a
// setBlock on/next to it).
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "block/block_registry.hpp"
#include "core/asset_manager.hpp"
#include "core/camera.hpp"
#include "core/config.hpp"
#include "core/error.hpp"
#include "core/input.hpp"
#include "core/logger.hpp"
#include "core/window.hpp"
#include "renderer/mesh.hpp"
#include "renderer/renderer.hpp"
#include "renderer/shader.hpp"
#include "renderer/texture.hpp"
#include "renderer/texture_atlas.hpp"
#include "world/biome.hpp"
#include "world/chunk.hpp"
#include "world/chunk_mesher.hpp"
#include "world/world.hpp"
#include "world/world_generator.hpp"

namespace mc {

struct GameArgs {
    std::filesystem::path configPath = "config/config.json";
    int frames = 0;        // 0 = run until closed
    std::int64_t seed = -1; // -1 = use config value
    LogLevel logLevel = LogLevel::Info;
    std::filesystem::path screenshotPath; // if set: save after the last frame
};

class Game {
public:
    Game() = default;
    ~Game();

    Result<void> init(const GameArgs& args);
    // Runs the main loop. Returns the process exit code (0 = clean exit).
    int run();

    const Config& config() const { return m_config; }
    AssetManager& assets() { return m_assets; }
    const World& world() const { return m_world; }

private:
    // GPU geometry of one chunk: opaque + transparent (blended) sub-meshes.
    struct ChunkMesh {
        std::optional<Mesh> opaque;
        std::optional<Mesh> transparent;
    };

    Result<void> buildScene();
    void update(double dt);
    void render();
    // Rebuilds the GPU mesh of every dirty (or never-meshed) chunk.
    Result<void> updateChunkMeshes();
    Result<void> takeScreenshot(const std::filesystem::path& path);

    GameArgs m_args;
    Config m_config;
    AssetManager m_assets;
    Window m_window;
    Input m_input;
    Camera m_camera;
    Renderer m_renderer;

    // Data-driven content.
    BlockRegistry m_blocks;
    BiomeRegistry m_biomes;
    TextureAtlas m_atlas;

    // Voxel world + per-chunk GPU meshes.
    World m_world;
    std::unordered_map<ChunkPos, ChunkMesh> m_chunkMeshes;

    // Seeded world generator (built in buildScene). Optional: the game only
    // touches it after init succeeded.
    std::optional<WorldGenerator> m_generator;

    ShaderProgram m_basicShader;

    int m_frameCount = 0;
    double m_fpsSmoothed = 0.0;
    double m_lastTitleUpdate = 0.0;
    bool m_initialized = false;
};

} // namespace mc
