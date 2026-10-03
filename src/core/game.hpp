// Top-level game object: owns configuration, assets, window, input, camera
// and the renderer; runs the main loop.
//
// Phase 2 scope: data-driven block system (registry from JSON + texture
// atlas) demonstrated by a block showcase row (each block rendered with its
// own top/side/bottom textures, transparent blocks in a blending pass), plus
// first-person camera controls. The voxel world arrives in Phase 3.
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
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

private:
    // One showcase instance of a registered block.
    struct ShowcaseCube {
        BlockID block;
        Mesh mesh;
        Vec3 position {0.0f, 0.5f, 0.0f};
        bool spin = false;
    };

    Result<void> buildScene();
    void update(double dt);
    void render();

    // Demo geometry / textures.
    Result<void> takeScreenshot(const std::filesystem::path& path);
    // Full-cube mesh whose per-face UVs cover the atlas tiles named by the
    // block model. (Superseded by chunk meshing in Phase 3.)
    static std::vector<MeshVertex> buildBlockCube(const BlockModel& model,
                                                  const AtlasLayout& layout);
    static std::vector<MeshVertex> buildGroundPlane(float size);
    static std::vector<std::uint32_t> buildCubeIndices();
    static std::vector<std::uint32_t> buildGroundIndices();
    static std::vector<std::uint8_t> makeGroundPixels(int size);
    void drawShowcaseCube(const ShowcaseCube& cube, const Mat4& view,
                          const Mat4& projection);

    GameArgs m_args;
    Config m_config;
    AssetManager m_assets;
    Window m_window;
    Input m_input;
    Camera m_camera;
    Renderer m_renderer;

    // Data-driven content.
    BlockRegistry m_blocks;
    TextureAtlas m_atlas;

    ShaderProgram m_basicShader;
    Texture m_groundTexture;
    Mesh m_groundMesh;
    std::vector<ShowcaseCube> m_showcase;

    double m_time = 0.0;
    int m_frameCount = 0;
    double m_fpsSmoothed = 0.0;
    double m_lastTitleUpdate = 0.0;
    bool m_initialized = false;
};

} // namespace mc
