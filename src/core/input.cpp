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
        case SDLK_LSHIFT:
        case SDLK_RSHIFT: // CRAFT-7: either shift key drives quick-move
            return GameKey::LeftShift;
        case SDLK_LCTRL: return GameKey::LeftControl;
        case SDLK_LALT: return GameKey::LeftAlt;
        case SDLK_TAB: return GameKey::Tab;
        case SDLK_ESCAPE: return GameKey::Escape;
        case SDLK_Q: return GameKey::Q;
        case SDLK_E: return GameKey::E;
        case SDLK_F3: return GameKey::F3;
        case SDLK_F5: return GameKey::F5;
        case SDLK_R: return GameKey::R;
        // CRAFT-7: the top-row digits (the numpad digits are left
        // unmapped — Minecraft binds the top row).
        case SDLK_1: return GameKey::Digit1;
        case SDLK_2: return GameKey::Digit2;
        case SDLK_3: return GameKey::Digit3;
        case SDLK_4: return GameKey::Digit4;
        case SDLK_5: return GameKey::Digit5;
        case SDLK_6: return GameKey::Digit6;
        case SDLK_7: return GameKey::Digit7;
        case SDLK_8: return GameKey::Digit8;
        case SDLK_9: return GameKey::Digit9;
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
    m_buttonReleased.fill(false);
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
                handleMouseMotion(event.motion.xrel, event.motion.yrel,
                                  event.motion.x, event.motion.y);
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
    } else if (!down && m_buttonDown[idx]) {
        m_buttonReleased[idx] = true; // release edge (Phase 24)
    }
    m_buttonDown[idx] = down;
}

void Input::handleMouseMotion(float dx, float dy, float x, float y) {
    m_mouseDelta.x += dx;
    m_mouseDelta.y += dy;
    m_mousePos = Vec2 {x, y};
}

void Input::handleQuit() {
    m_quit = true;
}

} // namespace mc
