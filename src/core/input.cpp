#include "core/input.hpp"

#include <cmath>

#include <SDL3/SDL.h>

#include "core/logger.hpp"

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
        // UX-4: the camera cycle (V2-1: First -> ThirdBack -> ThirdFront;
        // Minecraft's modern camera key; F5 is the manual save in this
        // project).
        case SDLK_C: return GameKey::C;
        // UX-7: the tutorial skip (locked pointer only, the C-key
        // pattern). T is otherwise unused in the input layer.
        case SDLK_T: return GameKey::T;
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

GamepadButtonAction mapGamepadButton(int sdlButton) {
    // V3-8: the v1 mapping table (the Xbox standard layout). The
    // intentionally-unmapped buttons return std::monostate (the
    // documented deviation: LT/RT are trigger axes, not buttons; GUIDE /
    // L3 / R3 / DPAD up-down-left / the paddles / the touchpad stay
    // unmapped in v1).
    switch (sdlButton) {
        case SDL_GAMEPAD_BUTTON_SOUTH:
            return GameKey::Space; // A: jump (double-tap = the fly, the
                                    // Space's existing semantics inherit)
        case SDL_GAMEPAD_BUTTON_EAST:
            return GameKey::LeftShift; // B: sprint
        case SDL_GAMEPAD_BUTTON_NORTH:
            return GameKey::E; // X: the inventory screen
        case SDL_GAMEPAD_BUTTON_WEST:
            return GameKey::Q; // Y: drop
        case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
            return MouseButton::Left; // LB: mine / attack
        case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
            return MouseButton::Right; // RB: use / place
        case SDL_GAMEPAD_BUTTON_BACK:
            return GameKey::C; // camera cycle (UX-4)
        case SDL_GAMEPAD_BUTTON_START:
            return GameKey::Escape; // close panel / ESC
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
            return GameKey::Tab; // the hotbar cycle
        default:
            return std::monostate {};
    }
}

float remapAxisOutsideDeadzone(float v, float dz) {
    // V3-8: sign(v) x min(1, (|v| - dz) / (1 - dz)); inside or on the
    // deadzone (|v| <= dz) = 0.
    const float a = std::fabsf(v);
    if (a <= dz) {
        return 0.0f;
    }
    const float s = v < 0.0f ? -1.0f : 1.0f;
    const float r = (a - dz) / (1.0f - dz);
    return s * (r < 1.0f ? r : 1.0f);
}

void Input::frameBegin() {
    m_keyPressed.fill(false);
    m_buttonPressed.fill(false);
    m_buttonReleased.fill(false);
    m_mouseDelta = Vec2 {0.0f, 0.0f};
    m_wheelDelta = 0; // V2-19: the wheel is per-frame input state
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
            case SDL_EVENT_MOUSE_WHEEL:
                // V2-19: the third-person zoom's input (the vertical
                // ticks only — the Game's consumer reads the per-frame
                // accumulated total).
                handleMouseWheel(static_cast<int>(event.wheel.y));
                break;
            case SDL_EVENT_GAMEPAD_ADDED:
                // V3-8: the connection state is the gamepad's own decision
                // (the single-gamepad model: a second one is ignored).
                handleGamepadAdded(static_cast<int>(event.gdevice.which));
                break;
            case SDL_EVENT_GAMEPAD_REMOVED:
                // V3-8: only the tracked gamepad's REMOVED disconnects
                // (handleGamepadRemoved no-ops on any other device).
                handleGamepadRemoved(static_cast<int>(event.gdevice.which));
                break;
            case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                // V3-8: single-gamepad model — only the tracked device's
                // events are consumed (a second gamepad's events are
                // ignored; the documented deviation, v1).
                if (event.gaxis.which == m_gamepadWhich) {
                    handleGamepadAxis(static_cast<int>(event.gaxis.axis),
                                      static_cast<std::int16_t>(
                                          event.gaxis.value));
                }
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            case SDL_EVENT_GAMEPAD_BUTTON_UP:
                if (event.gbutton.which == m_gamepadWhich) {
                    handleGamepadButton(static_cast<int>(event.gbutton.button),
                                        event.gbutton.down);
                }
                break;
            // V3-8: SDL_EVENT_GAMEPAD_REMAPPED = the mapping updated (v1:
            // the standard-layout table is built-in, no re-read).
            default:
                break;
        }
    }
}

void Input::tapEvent(const InputEvent& ev) const {
    // V3-9: null tap (every normal run) = the branch alone.
    if (m_eventTap) {
        m_eventTap(ev);
    }
}

void Input::handleKey(std::uint32_t sdlKeycode, bool down) {
    // V3-9: the tap records the RAW argument (before the map — an
    // unmapped keycode records and re-injects as the same no-op).
    tapEvent(InputEvent {down ? InputEvent::Kind::KeyDown
                              : InputEvent::Kind::KeyUp,
                         sdlKeycode, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, false});
    auto key = mapKey(sdlKeycode);
    if (!key) {
        return;
    }
    setKeySource(*key, kInputSourceKeyboard, down);
}

void Input::handleMouseButton(int sdlButton, bool down) {
    tapEvent(InputEvent {down ? InputEvent::Kind::MouseDown
                              : InputEvent::Kind::MouseUp,
                         0, sdlButton, 0, 0.0f, 0.0f, 0.0f, 0.0f, false});
    auto button = mapButton(sdlButton);
    if (!button) {
        return;
    }
    setButtonSource(*button, kInputSourceKeyboard, down);
}

void Input::handleMouseMotion(float dx, float dy, float x, float y) {
    tapEvent(InputEvent {InputEvent::Kind::MouseMove, 0, 0, 0, dx, dy, x, y,
                         false});
    m_mouseDelta.x += dx;
    m_mouseDelta.y += dy;
    m_mousePos = Vec2 {x, y};
}

void Input::handleMouseWheel(int deltaY) {
    tapEvent(InputEvent {InputEvent::Kind::Wheel, 0, deltaY, 0, 0.0f, 0.0f,
                         0.0f, 0.0f, false});
    m_wheelDelta += deltaY;
}

void Input::handleQuit() {
    m_quit = true;
}

void Input::setKeySource(GameKey key, std::uint8_t source, bool down) {
    // V3-8: the per-source bit operation (see the file header for the OR
    // semantics). With the gamepad never connected (bit 1 never set) this
    // is bit-identical to the pre-V3-8 bool assignment (the legacy fixture
    // pins it).
    const int idx = static_cast<int>(key);
    const std::uint8_t mask = m_keyDown[idx];
    if (down) {
        if ((mask & source) == 0) {
            m_keyPressed[idx] = true; // this source's rising edge
        }
        m_keyDown[idx] = static_cast<std::uint8_t>(mask | source);
    } else {
        m_keyDown[idx] = static_cast<std::uint8_t>(mask & ~source);
    }
}

void Input::setButtonSource(MouseButton button, std::uint8_t source,
                            bool down) {
    // V3-8: the per-source bit operation for mouse buttons. wasReleased
    // (the bow) fires only on a source's falling edge that clears the
    // WHOLE mask — the other source still holding = not released.
    const int idx = static_cast<int>(button);
    const std::uint8_t mask = m_buttonDown[idx];
    if (down) {
        if ((mask & source) == 0) {
            m_buttonPressed[idx] = true; // this source's rising edge
        }
        m_buttonDown[idx] = static_cast<std::uint8_t>(mask | source);
    } else {
        const std::uint8_t next = static_cast<std::uint8_t>(mask & ~source);
        if ((mask & source) != 0 && next == 0) {
            m_buttonReleased[idx] = true; // the release edge (Phase 24)
        }
        m_buttonDown[idx] = next;
    }
}

void Input::updateGamepadMovement() {
    // V3-8: the left stick's digitize (SDL convention: LEFTX +1 = right,
    // LEFTY +1 = up): +Y→W, −Y→S, +X→D, −X→A; outside the deadzone only,
    // a diagonal = both keys. The remap is monotone and zero inside the
    // deadzone, so its sign IS the digitize test. A full recompute of the
    // four directions' gamepad bits from the cached pair (idempotent).
    const float x = remapAxisOutsideDeadzone(
        static_cast<float>(m_gamepadLeftX) / kGamepadAxisFullScale,
        kGamepadAxisDeadzone);
    const float y = remapAxisOutsideDeadzone(
        static_cast<float>(m_gamepadLeftY) / kGamepadAxisFullScale,
        kGamepadAxisDeadzone);
    setKeySource(GameKey::W, kInputSourceGamepad, y > 0.0f);
    setKeySource(GameKey::S, kInputSourceGamepad, y < 0.0f);
    setKeySource(GameKey::A, kInputSourceGamepad, x < 0.0f);
    setKeySource(GameKey::D, kInputSourceGamepad, x > 0.0f);
}

void Input::handleGamepadButton(int sdlButton, bool down) {
    // V3-8: not connected = no tracked device = the event is ignored
    // (pump() pre-filters the which; this guard makes the handler
    // self-contained for the unit tests).
    if (m_gamepadWhich == 0) {
        return;
    }
    // V3-9: the tap after the not-connected guard (a tracked device's
    // which is recorded — the replay's synthetic connect, demo.hpp).
    tapEvent(InputEvent {InputEvent::Kind::GamepadButton, m_gamepadWhich,
                         sdlButton, 0, 0.0f, 0.0f, 0.0f, 0.0f, down});
    const GamepadButtonAction action = mapGamepadButton(sdlButton);
    if (const GameKey* key = std::get_if<GameKey>(&action)) {
        setKeySource(*key, kInputSourceGamepad, down);
    } else if (const MouseButton* button = std::get_if<MouseButton>(&action)) {
        setButtonSource(*button, kInputSourceGamepad, down);
    }
    // std::monostate = the button is not mapped in v1 (the documented
    // deviation) — zero state change.
}

void Input::handleGamepadAxis(int sdlAxis, std::int16_t value) {
    if (m_gamepadWhich == 0) {
        return; // V3-8: not connected (see handleGamepadButton).
    }
    // V3-9: the tap (see handleGamepadButton) — the raw axis code +
    // value, no deadzone remap (the replay re-runs the remap).
    tapEvent(InputEvent {InputEvent::Kind::GamepadAxis, m_gamepadWhich,
                         sdlAxis, value, 0.0f, 0.0f, 0.0f, 0.0f, false});
    switch (sdlAxis) {
        case SDL_GAMEPAD_AXIS_LEFTX:
            m_gamepadLeftX = value;
            updateGamepadMovement();
            break;
        case SDL_GAMEPAD_AXIS_LEFTY:
            m_gamepadLeftY = value;
            updateGamepadMovement();
            break;
        case SDL_GAMEPAD_AXIS_RIGHTX:
            // V3-8: the look — stick right = mouse right = xrel+.
            m_mouseDelta.x +=
                remapAxisOutsideDeadzone(static_cast<float>(value) /
                                             kGamepadAxisFullScale,
                                         kGamepadAxisDeadzone) *
                kGamepadLookPixelsPerUnit;
            break;
        case SDL_GAMEPAD_AXIS_RIGHTY:
            // V3-8: the look — stick up = SDL +1 = mouse up = yrel−.
            m_mouseDelta.y +=
                -remapAxisOutsideDeadzone(static_cast<float>(value) /
                                              kGamepadAxisFullScale,
                                          kGamepadAxisDeadzone) *
                kGamepadLookPixelsPerUnit;
            break;
        default:
            // V3-8: the triggers (LEFT_TRIGGER / RIGHT_TRIGGER) are not
            // mapped in v1 (the documented deviation) — zero state change.
            break;
    }
}

void Input::handleGamepadAdded(int which) {
    // V3-8: the single-gamepad model — the first connected gamepad is
    // tracked; a second one (while the first is still connected) is
    // ignored (the documented deviation, v1).
    if (m_gamepadWhich != 0) {
        return;
    }
    m_gamepadWhich = static_cast<std::uint32_t>(which);
    m_gamepadLeftX = 0;
    m_gamepadLeftY = 0;
    // SDL_GetGamepadNameForID: NULL when SDL has no mapping for the
    // device (safe to call — a null subsystem returns null, verified).
    const char* name = SDL_GetGamepadNameForID(m_gamepadWhich);
    m_gamepadName = name != nullptr ? std::string(name) : std::string("unknown");
    // V3-8a (11-06 real-device acceptance): OPEN the pad — SDL 3.x
    // delivers BUTTON/AXIS events only after an explicit open (real-
    // device verified: zero events pre-open, all post-open; the
    // gate-193 synthetic-event unit tests could never surface this).
    // Opening also stabilizes the shanwan clone (unopened + button
    // press = USB disconnect, the 11-06 log chain). A failed open
    // (permissions, ...) keeps the tracking state — the REMOVED below
    // must still match — but is logged loudly: without the open no
    // button can arrive.
    m_gamepad = SDL_OpenGamepad(static_cast<SDL_JoystickID>(which));
    if (m_gamepad == nullptr) {
        MC_LOG_WARN("Gamepad open failed: " + std::string(SDL_GetError()));
    }
    MC_LOG_INFO("Gamepad connected: " + m_gamepadName);
}

void Input::handleGamepadRemoved(int which) {
    // V3-8: only the tracked gamepad's REMOVED disconnects (any other
    // device = a no-op, the single-gamepad model; a negative which
    // widens to a huge Uint32 that cannot match — a no-op as well).
    if (m_gamepadWhich == 0 ||
        static_cast<std::uint32_t>(which) != m_gamepadWhich) {
        return;
    }
    // V3-8a: close the opened handle. SDL tore the device down
    // internally on REMOVED already, so this is belt-and-braces —
    // SDL_CloseGamepad looks the handle up in its internal list and
    // no-ops safely on a stale pointer (keeps open/close symmetric on
    // the hot-plug path).
    if (m_gamepad != nullptr) {
        SDL_CloseGamepad(m_gamepad);
        m_gamepad = nullptr;
    }
    // Clear every gamepad source bit (the keys + the mouse buttons the
    // gamepad was driving; the other source's bits stay untouched — the
    // keyboard-held action stays down). The disconnect is NOT a release
    // event (no wasReleased edges — the bow's stale-up branch cancels the
    // draw rather than hanging it).
    for (int i = 0; i < kKeyCount; ++i) {
        m_keyDown[i] = static_cast<std::uint8_t>(
            m_keyDown[i] & ~kInputSourceGamepad);
    }
    for (int i = 0; i < kMouseButtonCount; ++i) {
        m_buttonDown[i] = static_cast<std::uint8_t>(
            m_buttonDown[i] & ~kInputSourceGamepad);
    }
    m_gamepadLeftX = 0;
    m_gamepadLeftY = 0;
    m_gamepadWhich = 0;
    MC_LOG_INFO("Gamepad disconnected");
}

} // namespace mc
