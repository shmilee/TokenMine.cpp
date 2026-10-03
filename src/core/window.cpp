#include "core/window.hpp"

#include <GL/glew.h> // must precede SDL's GL declarations
#include <SDL3/SDL.h>

#include "core/logger.hpp"

namespace mc {

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

    Uint32 flags = SDL_WINDOW_OPENGL;
    if (fullscreen) {
        flags |= SDL_WINDOW_FULLSCREEN;
    }

    m_window = SDL_CreateWindow(m_title.c_str(), m_width, m_height, flags);
    if (m_window == nullptr) {
        return MC_FAIL(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
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
