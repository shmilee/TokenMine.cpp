#include "core/window.hpp"

#include <GL/glew.h> // must precede SDL's GL declarations
#include <SDL3/SDL.h>

#include "core/logger.hpp"

namespace mc {

namespace {
// Smallest allowed window size (windowed mode): below this the projection
// aspect and the screen-space UI (hotbar, hearts, F3) stop making sense.
constexpr int kMinWindowWidth = 320;
constexpr int kMinWindowHeight = 200;
} // namespace

Window::~Window() {
    if (m_sdlActive) {
        shutdown();
    }
}

Result<void> Window::init(const Config& config) {
    m_title = config.getString("window.title", "MineCraft");
    m_width = config.getInt("window.width", 1280);
    m_height = config.getInt("window.height", 720);
    const bool fullscreen = config.getBool("window.fullscreen", false);
    m_vsync = config.getBool("render.vsync", true);

    // SDL3: bool return, true = success.
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        return MC_FAIL(std::string("SDL_Init(VIDEO) failed: ") + SDL_GetError());
    }
    m_sdlActive = true;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    // window.resizable (default true): windowed windows follow the user's
    // resize — the game reads the drawable size live every frame (see
    // Window::width/height), so the GL viewport, projection aspect, and
    // the screen-space UI all track the new size without any event
    // handling. false keeps the window at its configured size (some
    // desktop WMs auto-tile resizable windows, which is also why
    // headless/automation runs pin this off to hold a fixed geometry).
    const bool resizable = config.getBool("window.resizable", true);
    Uint32 flags = SDL_WINDOW_OPENGL;
    if (fullscreen) {
        flags |= SDL_WINDOW_FULLSCREEN;
    } else if (resizable) {
        flags |= SDL_WINDOW_RESIZABLE;
    }

    m_window = SDL_CreateWindow(m_title.c_str(), m_width, m_height, flags);
    if (m_window == nullptr) {
        return MC_FAIL(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
    }
    if (resizable) {
        // Keep the window out of the degenerate range where the
        // projection aspect (width / height) and the screen-space UI stop
        // making sense.
        SDL_SetWindowMinimumSize(m_window, kMinWindowWidth, kMinWindowHeight);
    }

    m_glContext = SDL_GL_CreateContext(m_window);
    if (m_glContext == nullptr) {
        return MC_FAIL(std::string("SDL_GL_CreateContext failed: ") + SDL_GetError());
    }
    if (!SDL_GL_MakeCurrent(m_window, m_glContext)) {
        return MC_FAIL(std::string("SDL_GL_MakeCurrent failed: ") + SDL_GetError());
    }
    SDL_GL_SetSwapInterval(m_vsync ? 1 : 0);

    glewExperimental = GL_TRUE;
    const GLenum glewErr = glewInit();
    if (glewErr != GLEW_OK) {
        return MC_FAIL(std::string("glewInit failed: ") +
                       reinterpret_cast<const char*>(glewGetErrorString(glewErr)));
    }

    MC_LOG_INFO("Window created: " + std::to_string(m_width) + "x" +
                std::to_string(m_height) + (fullscreen ? " (fullscreen)" : "") +
                (m_vsync ? " (vsync on)" : " (vsync off)"));
    return {};
}

void Window::shutdown() {
    if (m_glContext != nullptr) {
        SDL_GL_DestroyContext(m_glContext);
        m_glContext = nullptr;
    }
    if (m_window != nullptr) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    if (m_sdlActive) {
        SDL_Quit();
        m_sdlActive = false;
    }
}

void Window::setRelativeMouseMode(bool enabled) {
    if (m_window != nullptr) {
        if (!SDL_SetWindowRelativeMouseMode(m_window, enabled)) {
            MC_LOG_WARN(std::string("Failed to set relative mouse mode: ") +
                        SDL_GetError());
        }
    }
}

bool Window::relativeMouseMode() const {
    return m_window != nullptr && SDL_GetWindowRelativeMouseMode(m_window);
}

int Window::width() const {
    if (m_window == nullptr) {
        return m_width; // initial size until the window exists
    }
    int w = 0;
    int h = 0;
    // Pixel size (the GL drawable size; equals the window size at 1:1
    // scaling, the high-DPI-correct value otherwise).
    SDL_GetWindowSizeInPixels(m_window, &w, &h);
    return w;
}

int Window::height() const {
    if (m_window == nullptr) {
        return m_height; // initial size until the window exists
    }
    int w = 0;
    int h = 0;
    SDL_GetWindowSizeInPixels(m_window, &w, &h);
    return h;
}

void Window::swapBuffers() {
    if (m_window != nullptr) {
        SDL_GL_SwapWindow(m_window);
    }
}

void Window::setTitle(const std::string& title) {
    m_title = title;
    if (m_window != nullptr) {
        SDL_SetWindowTitle(m_window, m_title.c_str());
    }
}

} // namespace mc
