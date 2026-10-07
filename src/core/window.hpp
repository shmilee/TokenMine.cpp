// Window + OpenGL context management (SDL3 + GLEW).
//
// This class owns the SDL video subsystem lifecycle and the GL 3.3 core
// context. Game code must not call SDL or GL directly; it goes through the
// Renderer (Phase 1) which is the only GL consumer.
//
// SDL types are forward-declared so this header does not drag SDL headers
// into the rest of the codebase (and so GLEW can be included first in the
// implementation file without macro clashes).
#pragma once

#include <string>

#include "core/config.hpp"
#include "core/error.hpp"

struct SDL_Window;
struct SDL_GLContextState;

namespace mc {

class Window {
public:
    Window() = default;
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    // Create window + GL 3.3 core context. Initializes the SDL video
    // subsystem (SDL_Init is owned here).
    Result<void> init(const Config& config);

    // Destroy context, window, and quit SDL. Safe to call once after init.
    void shutdown();

    // Hide cursor and deliver relative mouse motion (first-person look).
    // Event delivery itself is owned by Input (single consumer of the SDL
    // event queue); Window only manages context, swap and mode.
    void setRelativeMouseMode(bool enabled);
    bool relativeMouseMode() const;

    void swapBuffers();
    void setTitle(const std::string& title);

    // Current DRAWABLE size in pixels (live: queries SDL on every call,
    // so a user resize is picked up on the next frame — the viewport,
    // projection aspect, UI layout and screenshots all follow the window).
    // The initial (config) size is returned until the window exists.
    int width() const;
    int height() const;

    // Current LOGICAL size (SDL_GetWindowSize). Equals width()/height()
    // at dppx 1; on a HiDPI-scaled display it is the unscaled size while
    // width()/height() (SDL_GetWindowSizeInPixels) are the drawable
    // pixels. SDL mouse event coordinates are in this logical space, so
    // the UI hit tests derive from it (Game::uiSize/uiMouse, Phase 34).
    int logicalWidth() const;
    int logicalHeight() const;

    // Opaque handle for the Renderer (Phase 1).
    const SDL_Window* nativeHandle() const { return m_window; }

private:
    SDL_Window* m_window = nullptr;
    SDL_GLContextState* m_glContext = nullptr;
    int m_width = 0;
    int m_height = 0;
    std::string m_title;
    bool m_vsync = true;
    bool m_sdlActive = false;
};

} // namespace mc
