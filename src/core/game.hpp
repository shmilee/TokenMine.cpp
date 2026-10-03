// Top-level game object: owns configuration, assets, window, input, camera,
// renderer and the voxel world; runs the main loop.
//
// Phase 6 scope: the Phase 5 world + walking Player, plus block
// interaction: a DDA voxel raycast from the eye selects a block (reach 5);
// left click breaks it (if properties.destroyable), right click places the
// currently selected hotbar block on the hit face (if the target cell is
// placeable and does not overlap the player). Hotbar selection is a
// temporary data-driven list (assets/data/hotbar.json, Tab to cycle) until
// the item/inventory system lands in Phase 11. Each chunk owns up to two
// GPU meshes (opaque + transparent/blended); a chunk's meshes are rebuilt
// lazily when it is marked dirty (first meshing or a setBlock on/next to it).
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
#include "player/player.hpp"
#include "renderer/mesh.hpp"
#include "renderer/renderer.hpp"
#include "renderer/shader.hpp"
#include "renderer/texture.hpp"
#include "renderer/texture_atlas.hpp"
#include "world/biome.hpp"
#include "world/chunk.hpp"
#include "world/chunk_mesher.hpp"
#include "world/day_cycle.hpp"
#include "world/lighting.hpp"
#include "world/raycast.hpp"
#include "world/world.hpp"
#include "world/world_generator.hpp"

namespace mc {

struct GameArgs {
    std::filesystem::path configPath = "config/config.json";
    int frames = 0;        // 0 = run until closed
    std::int64_t seed = -1; // -1 = use config value
    double time = -1;      // -1 = use config start_time_of_day (else [0, 1))
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

    // Draw the sun, moon and starfield (unlit billboarded quads, blended).
    // Called between the opaque and transparent chunk passes.
    void drawSkyObjects(const Mat4& view, const Mat4& projection,
                        int viewportHeight);

    // Pointer-lock state machine + block interaction (called from update
    // while the pointer is captured).
    void handleInteraction();
    // The block the crosshair is aiming at (raycast from the eye, within
    // kReach), if any.
    std::optional<BlockHit> targetBlock() const;
    void breakBlock();
    void placeBlock();
    void cycleHotbar();

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

    // The first-person actor (built in buildScene after the world exists;
    // it holds references to the world and the block registry).
    std::optional<Player> m_player;

    // Temporary block-selection list (assets/data/hotbar.json, Tab cycles);
    // replaced by the item/inventory system in Phase 11.
    std::vector<BlockID> m_hotbar;
    std::size_t m_hotbarSelected = 0;

    ShaderProgram m_basicShader;

    // Day/night cycle (config-driven; advanced by a fixed per-frame step so
    // headless runs stay byte-reproducible — see kDayStepPerFrame).
    DayCycle m_dayCycle;
    // Unlit billboarded sky sprites (sun, moon, stars), drawn with
    // m_skyShader between the opaque and transparent chunk passes.
    ShaderProgram m_skyShader;
    Mesh m_starMesh;     // kStarCount quads on the upper hemisphere
    Mesh m_skyPointMesh; // one quad at the origin (offset per draw)

    int m_frameCount = 0;
    double m_fpsSmoothed = 0.0;
    double m_lastTitleUpdate = 0.0;
    bool m_initialized = false;

    // Chunk count the lighting was last fully propagated for; a new chunk
    // (out-of-area placement) starts with no light, so the next frame
    // re-propagates the whole world before meshing.
    std::size_t m_lastLightedChunks = 0;
};

} // namespace mc
