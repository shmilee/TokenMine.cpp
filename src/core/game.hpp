// Top-level game object: owns configuration, assets, window and runs the
// main loop. Phase 0 scope: window + clear + FPS. World, player, renderer,
// entity systems are added here in later phases without changing the loop.
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "core/asset_manager.hpp"
#include "core/config.hpp"
#include "core/error.hpp"
#include "core/logger.hpp"
#include "core/window.hpp"

namespace mc {

struct GameArgs {
    std::filesystem::path configPath = "config/config.json";
    int frames = 0;        // 0 = run until closed
    std::int64_t seed = -1; // -1 = use config value
    LogLevel logLevel = LogLevel::Info;
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
    void update(double dt);
    void render();

    GameArgs m_args;
    Config m_config;
    AssetManager m_assets;
    Window m_window;

    int m_frameCount = 0;
    double m_fpsSmoothed = 0.0;
    double m_lastTitleUpdate = 0.0;
    bool m_initialized = false;
};

} // namespace mc
