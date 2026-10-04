#include "core/input.hpp"

#include <SDL3/SDL.h>

namespace mc {

std::optional<GameKey> Input::mapKey(std::uint32_t sdlKeycode) {
    switch (sdlKeycode) {
        // SDL3 key codes are uppercase (SDLK_W, ...).
        case SDLK_W: return GameKey::W;
        case SDLK_A: return GameKey::A;
        case SDLK_S: return GameKey::S;
        case SDLK_D: return GameKey::D;
        case SDLK_SPACE: return GameKey::Space;
        case SDLK_LSHIFT: return GameKey::LeftShift;
        case SDLK_LCTRL: return GameKey::LeftControl;
        case SDLK_LALT: return GameKey::LeftAlt;
        case SDLK_TAB: return GameKey::Tab;
        case SDLK_ESCAPE: return GameKey::Escape;
        case SDLK_Q: return GameKey::Q;
        default: return std::nullopt;
    }
}

std::optional<MouseButton> Input::mapButton(int sdlButton) {
    switch (sdlButton) {
        case SDL_BUTTON_LEFT: return MouseButton::Left;
        case SDL_BUTTON_RIGHT: return MouseButton::Right;
        case SDL_BUTTON_MIDDLE: return MouseButton::Middle;
        default: return std::nullopt;
    }
}

void Input::frameBegin() {
    m_keyPressed.fill(false);
    m_buttonPressed.fill(false);
    m_mouseDelta = Vec2 {0.0f, 0.0f};
}

void Input::pump() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                m_quit = true;
                break;
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
                handleKey(event.key.key, event.type == SDL_EVENT_KEY_DOWN);
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                handleMouseButton(static_cast<int>(event.button.button),
                                  event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
                break;
            case SDL_EVENT_MOUSE_MOTION:
                handleMouseMotion(event.motion.xrel, event.motion.yrel);
                break;
            default:
                break;
        }
    }
}

void Input::handleKey(std::uint32_t sdlKeycode, bool down) {
    auto key = mapKey(sdlKeycode);
    if (!key) {
        return;
    }
    const int idx = static_cast<int>(*key);
    if (down && !m_keyDown[idx]) {
        m_keyPressed[idx] = true; // edge: only on the rising edge
    }
    m_keyDown[idx] = down;
}

void Input::handleMouseButton(int sdlButton, bool down) {
    auto button = mapButton(sdlButton);
    if (!button) {
        return;
    }
    const int idx = static_cast<int>(*button);
    if (down && !m_buttonDown[idx]) {
        m_buttonPressed[idx] = true;
    }
    m_buttonDown[idx] = down;
}

void Input::handleMouseMotion(float dx, float dy) {
    m_mouseDelta.x += dx;
    m_mouseDelta.y += dy;
}

void Input::handleQuit() {
    m_quit = true;
}

} // namespace mc
