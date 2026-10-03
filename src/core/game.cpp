#include "core/game.hpp"

#include <GL/glew.h>
#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>

#include "core/png.hpp"

namespace mc {

namespace {

constexpr float kFovDegrees = 70.0f;
constexpr float kNear = 0.1f;
constexpr float kFar = 1000.0f;
constexpr Vec4 kSkyColor {0.47f, 0.71f, 0.95f, 1.0f};

double nowSeconds() {
    return static_cast<double>(SDL_GetPerformanceCounter()) /
           SDL_GetPerformanceFrequency();
}

// Deterministic hash for procedural texture variation (not game logic).
std::uint32_t hash2(std::uint32_t x, std::uint32_t y) {
    std::uint32_t h = x * 374761393u + y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
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

std::vector<MeshVertex> Game::buildCube() {
    // 24 vertices: 4 per face, each with its own normal. UVs span 0..1 per
    // face so a single texture tile covers each face.
    const float s = 0.5f;
    struct Face {
        Vec3 n;
        Vec3 c[4]; // corners CCW viewed from outside
    };
    const Face faces[6] = {
        {{0, 1, 0}, {{-s, s, -s}, {s, s, -s}, {s, s, s}, {-s, s, s}}},     // +Y
        {{0, -1, 0}, {{-s, -s, s}, {s, -s, s}, {s, -s, -s}, {-s, -s, -s}}}, // -Y
        {{0, 0, 1}, {{-s, -s, s}, {s, -s, s}, {s, s, s}, {-s, s, s}}},     // +Z
        {{0, 0, -1}, {{s, -s, -s}, {-s, -s, -s}, {-s, s, -s}, {s, s, -s}}}, // -Z
        {{1, 0, 0}, {{s, -s, s}, {s, -s, -s}, {s, s, -s}, {s, s, s}}},     // +X
        {{-1, 0, 0}, {{-s, -s, -s}, {-s, -s, s}, {-s, s, s}, {-s, s, -s}}}, // -X
    };
    const Vec2 uvs[4] = {{0, 1}, {1, 1}, {1, 0}, {0, 0}};

    std::vector<MeshVertex> verts;
    verts.reserve(24);
    for (const Face& f : faces) {
        for (int i = 0; i < 4; ++i) {
            MeshVertex v;
            v.position = f.c[i];
            v.normal = f.n;
            v.uv = uvs[i];
            v.light = 1.0f;
            verts.push_back(v);
        }
    }
    return verts;
}

std::vector<std::uint32_t> Game::buildCubeIndices() {
    std::vector<std::uint32_t> idx;
    idx.reserve(36);
    for (std::uint32_t face = 0; face < 6; ++face) {
        const std::uint32_t base = face * 4;
        idx.insert(idx.end(), {base, base + 1, base + 2, base, base + 2,
                               base + 3});
    }
    return idx;
}

std::vector<MeshVertex> Game::buildGroundPlane(float size) {
    const float h = size * 0.5f;
    const float uvScale = size * 0.5f; // tile the texture at 1x1 world units
    std::vector<MeshVertex> verts = {
        {{-h, 0.0f, -h}, {0, 1, 0}, {0.0f, uvScale}, 1.0f},
        {{h, 0.0f, -h}, {0, 1, 0}, {uvScale, uvScale}, 1.0f},
        {{h, 0.0f, h}, {0, 1, 0}, {uvScale, 0.0f}, 1.0f},
        {{-h, 0.0f, h}, {0, 1, 0}, {0.0f, 0.0f}, 1.0f},
    };
    return verts;
}

std::vector<std::uint32_t> Game::buildGroundIndices() {
    // Winding chosen so the normal points +Y (up): the triangles must be
    // counter-clockwise when viewed from above, otherwise back-face
    // culling discards them.
    return {0, 2, 1, 0, 3, 2};
}

std::vector<std::uint8_t> Game::makeCheckerPixels(int size, int checker) {
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(size) * size * 4);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const bool on = ((x / checker) + (y / checker)) % 2 == 0;
            std::uint8_t* p = pixels.data() + (y * size + x) * 4;
            if (on) {
                p[0] = 190;
                p[1] = 185;
                p[2] = 180;
            } else {
                p[0] = 120;
                p[1] = 115;
                p[2] = 110;
            }
            p[3] = 255;
        }
    }
    return pixels;
}

std::vector<std::uint8_t> Game::makeGroundPixels(int size) {
    // Green with deterministic per-pixel variation (procedural "grass").
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(size) * size * 4);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const std::uint32_t h = hash2(static_cast<std::uint32_t>(x),
                                          static_cast<std::uint32_t>(y));
            const int v = static_cast<int>(h % 40);
            std::uint8_t* p = pixels.data() + (y * size + x) * 4;
            p[0] = static_cast<std::uint8_t>(50 + v);
            p[1] = static_cast<std::uint8_t>(130 + v);
            p[2] = static_cast<std::uint8_t>(45 + v / 2);
            p[3] = 255;
        }
    }
    return pixels;
}

Result<void> Game::buildScene() {
    // -- shaders -------------------------------------------------------------
    auto vertSrc = m_assets.loadText("shaders/basic.vert");
    MC_TRY(vertSrc);
    auto fragSrc = m_assets.loadText("shaders/basic.frag");
    MC_TRY(fragSrc);

    auto shader = ShaderProgram::create(*vertSrc, *fragSrc);
    MC_TRY(shader);
    m_basicShader = std::move(*shader);

    // -- textures --------------------------------------------------------------
    auto checkerPixels = makeCheckerPixels(8, 1);
    auto checker = Texture::createFromPixels(8, 8, checkerPixels.data());
    MC_TRY(checker);
    m_checkerTexture = std::move(*checker);

    auto groundPixels = makeGroundPixels(8);
    auto groundTex = Texture::createFromPixels(8, 8, groundPixels.data());
    MC_TRY(groundTex);
    m_groundTexture = std::move(*groundTex);
    m_groundTexture.setRepeat(true);

    // -- meshes ----------------------------------------------------------------
    const auto cubeVerts = buildCube();
    const auto cubeIdx = buildCubeIndices();
    auto cubeUpload = m_cubeMesh.upload(cubeVerts, cubeIdx);
    MC_TRY(cubeUpload);

    const auto groundVerts = buildGroundPlane(64.0f);
    const auto groundIdx = buildGroundIndices();
    auto groundUpload = m_groundMesh.upload(groundVerts, groundIdx);
    MC_TRY(groundUpload);

    MC_LOG_INFO("Scene built: 1 cube (" + std::to_string(m_cubeMesh.vertexCount()) +
                " verts), 1 ground (" +
                std::to_string(m_groundMesh.vertexCount()) + " verts)");
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
        if (m_input.quitRequested() || m_input.wasPressed(GameKey::Escape)) {
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
    m_time += dt;

    // Pointer lock: left click toggles first-person mouse look.
    if (m_input.wasPressed(MouseButton::Left)) {
        m_window.setRelativeMouseMode(!m_window.relativeMouseMode());
    }

    m_camera.update(dt, m_input);
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

    // Ground plane (tiled texture).
    m_groundTexture.bind();
    m_basicShader.bind();
    m_basicShader.setUniform1i("u_texture", 0);
    m_basicShader.setUniform1i("u_useTexture", 1);
    m_basicShader.setUniform3f("u_lightDir", -0.4f, 0.8f, -0.3f);
    m_renderer.draw(m_groundMesh, m_basicShader, view, projection,
                    Mat4::identity());

    // Rotating cube on the ground.
    m_checkerTexture.bind();
    m_basicShader.bind();
    m_basicShader.setUniform1i("u_texture", 0);
    m_basicShader.setUniform1i("u_useTexture", 1);
    m_basicShader.setUniform3f("u_lightDir", -0.4f, 0.8f, -0.3f);
    const Mat4 model =
        Mat4::multiply(Mat4::rotateY(static_cast<float>(m_time)),
                       Mat4::translate(0.0f, 0.5f, 0.0f));
    m_renderer.draw(m_cubeMesh, m_basicShader, view, projection, model);

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
