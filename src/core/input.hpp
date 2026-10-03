// Input state, polled once per frame on the game thread.
//
// Input is the single consumer of the SDL event queue (Window only creates
// the context). State is split into:
//   - level-triggered:  isDown()      (held)
//   - edge-triggered:   wasPressed()  (true once, on the frame the key went
//                                     down; cleared by the next frameBegin)
//
// SDL types are mapped to GameKey/MouseButton at the boundary so the rest
// of the codebase (and the unit tests) never touches SDL.
#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "core/math/vec2.hpp"

namespace mc {

enum class GameKey {
    W,
    A,
    S,
    D,
    Space,
    LeftShift,
    LeftControl,
    LeftAlt,
    Tab,
    Escape,
    // Keep in sync with kKeyCount.
};

inline constexpr int kKeyCount = static_cast<int>(GameKey::Escape) + 1;

enum class MouseButton {
    Left,
    Right,
    Middle,
};

inline constexpr int kMouseButtonCount =
    static_cast<int>(MouseButton::Middle) + 1;

class Input {
public:
    // Call once at the start of each frame: clears edge-triggered state and
    // the accumulated mouse delta.
    void frameBegin();

    // Drain the SDL event queue (game thread only, once per frame).
    void pump();

    // Set when the user requested to quit (window close / WM quit).
    bool quitRequested() const { return m_quit; }

    bool isDown(GameKey key) const {
        return m_keyDown[static_cast<int>(key)];
    }
    bool wasPressed(GameKey key) const {
        return m_keyPressed[static_cast<int>(key)];
    }
    bool isDown(MouseButton button) const {
        return m_buttonDown[static_cast<int>(button)];
    }
    bool wasPressed(MouseButton button) const {
        return m_buttonPressed[static_cast<int>(button)];
    }

    // Accumulated relative mouse motion since frameBegin.
    Vec2 mouseDelta() const { return m_mouseDelta; }

    // -- Event entry points --------------------------------------------------
    // Used by pump() and directly by unit tests (SDL key codes are the
    // platform-independent SDLK_* virtual key codes).
    void handleKey(std::uint32_t sdlKeycode, bool down);
    void handleMouseButton(int sdlButton, bool down);
    void handleMouseMotion(float dx, float dy);
    void handleQuit();

private:
    static std::optional<GameKey> mapKey(std::uint32_t sdlKeycode);
    static std::optional<MouseButton> mapButton(int sdlButton);

    std::array<bool, kKeyCount> m_keyDown {};
    std::array<bool, kKeyCount> m_keyPressed {};
    std::array<bool, kMouseButtonCount> m_buttonDown {};
    std::array<bool, kMouseButtonCount> m_buttonPressed {};
    Vec2 m_mouseDelta {0.0f, 0.0f};
    bool m_quit = false;
};

} // namespace mc
