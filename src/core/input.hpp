// Input state, polled once per frame on the game thread.
//
// Input is the single consumer of the SDL event queue (Window only creates
// the context). State is split into:
//   - level-triggered:  isDown()      (held)
//   - edge-triggered:   wasPressed()  (true once, on the frame the key went
//                                     down; cleared by the next frameBegin)
//                        wasReleased() (true once, on the frame the key went
//                                     up; same clearing; Phase 24)
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
    Q, // drop one from the selected hotbar slot (Phase 11)
    E, // toggle the inventory screen (Phase 15)
    F3, // toggle the F3 debug overlay (Phase 15)
    F5, // save now, when a save slot is active (Phase 15)
    R, // respawn, while dead (Phase 16)
    // CRAFT-7: hotbar selection (Minecraft's top-row digits — select
    // slot 1..9 directly, the Tab cycle stays). Both physical shift keys
    // map to LeftShift (the shift-click quick-move accepts either).
    Digit1,
    Digit2,
    Digit3,
    Digit4,
    Digit5,
    Digit6,
    Digit7,
    Digit8,
    Digit9,
    // Keep in sync with kKeyCount.
};

inline constexpr int kKeyCount = static_cast<int>(GameKey::Digit9) + 1;

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
    // True once, on the frame the button went up (Phase 24; the bow's
    // charge is fired on the right-button release). A button-up for a
    // button that was not down (a stale WM event) does not set the edge.
    bool wasReleased(MouseButton button) const {
        return m_buttonReleased[static_cast<int>(button)];
    }

    // Accumulated relative mouse motion since frameBegin.
    Vec2 mouseDelta() const { return m_mouseDelta; }

    // Last absolute mouse position in window pixels (the UI screens read
    // it; while the pointer is captured SDL reports (0, 0) and the value
    // is stale by design — the UI is only used while the pointer is free).
    Vec2 mousePosition() const { return m_mousePos; }

    // -- Event entry points --------------------------------------------------
    // Used by pump() and directly by unit tests (SDL key codes are the
    // platform-independent SDLK_* virtual key codes).
    void handleKey(std::uint32_t sdlKeycode, bool down);
    void handleMouseButton(int sdlButton, bool down);
    void handleMouseMotion(float dx, float dy, float x, float y);
    void handleQuit();

private:
    static std::optional<GameKey> mapKey(std::uint32_t sdlKeycode);
    static std::optional<MouseButton> mapButton(int sdlButton);

    std::array<bool, kKeyCount> m_keyDown {};
    std::array<bool, kKeyCount> m_keyPressed {};
    std::array<bool, kMouseButtonCount> m_buttonDown {};
    std::array<bool, kMouseButtonCount> m_buttonPressed {};
    std::array<bool, kMouseButtonCount> m_buttonReleased {};
    Vec2 m_mouseDelta {0.0f, 0.0f};
    // Absolute position: updated on every motion event, NOT reset by
    // frameBegin (the cursor sits where it last was).
    Vec2 m_mousePos {0.0f, 0.0f};
    bool m_quit = false;
};

} // namespace mc
