// Top-level game object: owns configuration, assets, window, input, camera
// and the renderer; runs the main loop.
//
// Phase 1 scope: a basic 3D scene (tiled ground + rotating textured cube)
// with first-person camera controls (WASD + mouse look, click to lock
// pointer, ESC to exit). World/player/physics arrive in later phases and
// plug into update() without changing the loop.
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

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
    Result<void> buildScene();
    void update(double dt);
    void render();

    // Demo geometry / textures.
    Result<void> takeScreenshot(const std::filesystem::path& path);
    static std::vector<MeshVertex> buildCube();
    static std::vector<MeshVertex> buildGroundPlane(float size);
    static std::vector<std::uint32_t> buildCubeIndices();
    static std::vector<std::uint32_t> buildGroundIndices();
    static std::vector<std::uint8_t> makeCheckerPixels(int size, int checker);
    static std::vector<std::uint8_t> makeGroundPixels(int size);

    GameArgs m_args;
    Config m_config;
    AssetManager m_assets;
    Window m_window;
    Input m_input;
    Camera m_camera;
    Renderer m_renderer;

    ShaderProgram m_basicShader;
    Texture m_checkerTexture;
    Texture m_groundTexture;
    Mesh m_cubeMesh;
    Mesh m_groundMesh;

    double m_time = 0.0;
    int m_frameCount = 0;
    double m_fpsSmoothed = 0.0;
    double m_lastTitleUpdate = 0.0;
    bool m_initialized = false;
};

} // namespace mc
