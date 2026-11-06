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
//
// V3-8: gamepad support (the input boundary layer, on par with keyboard and
// mouse). The held state is a per-action SOURCE BITMASK (bit 0 =
// kInputSourceKeyboard, bit 1 = kInputSourceGamepad):
//   - isDown = any source holds (the OR semantics — the two sources are the
//     union of the same action; there is no keyboard-priority override, the
//     keyboard mapping is untouched and the gamepad never rebinds a key);
//   - each source has its OWN rising edge into wasPressed (a second source
//     pressing an already-down action sets the edge again — documented
//     semantics);
//   - a release clears only that source's bit (the other source still
//     holding keeps the action down);
//   - wasReleased (mouse buttons, the bow) fires only on a source's
//     falling edge that clears the WHOLE mask (the other source still
//     holding = not released = the bow keeps its draw).
// No gamepad connected = bit 1 is never set = the keyboard/mouse path is
// bit-identical to the pre-V3-8 bool behavior (the structural-immunity
// root; the legacy fixture unit test pins it).
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <variant>

#include <SDL3/SDL.h>

#include "core/math/vec2.hpp"

namespace mc {

// V3-9 (demo replay / record): the input event RECORD — the raw arguments
// of the public entry points, zero mapping layer (the replay re-injects
// them as-is through the SAME handle* closure that pump()'s SDL events
// and the unit tests use; the demo recorder's tap carries them). The
// field meanings per Kind (the unused fields stay zero):
//   KeyDown/KeyUp      u32 = the SDL virtual keycode
//   MouseDown/MouseUp  code = the SDL button (SDL_BUTTON_LEFT/RIGHT/
//                        MIDDLE; a button outside the map is recorded
//                        faithfully — the replay's handleMouseButton
//                        no-ops it, as the original did)
//   MouseMove          dx/dy = the relative delta (pixels),
//                        ax/ay = the absolute position (pixels)
//   Wheel              code = the vertical ticks (SDL convention:
//                        positive = up / away)
//   GamepadButton      u32 = the tracked device's which,
//                        code = the SDL_GAMEPAD_BUTTON_* value,
//                        down = the press state
//   GamepadAxis        u32 = the tracked device's which,
//                        code = the SDL_GAMEPAD_AXIS_* value,
//                        value = the raw Sint16 deflection
struct InputEvent {
    enum class Kind : std::uint8_t {
        KeyDown,
        KeyUp,
        MouseDown,
        MouseUp,
        MouseMove,
        Wheel,
        GamepadButton,
        GamepadAxis,
    } kind = Kind::KeyDown;
    std::uint32_t u32 = 0;
    std::int32_t code = 0;
    std::int16_t value = 0;
    float dx = 0.0f;
    float dy = 0.0f;
    float ax = 0.0f;
    float ay = 0.0f;
    bool down = false;
};

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
    C, // UX-4: cycle the camera First -> ThirdBack -> ThirdFront (locked
        // pointer only; V2-1: the Minecraft F5 order)
    T, // UX-7: skip the tutorial (locked pointer only)
    G, // V3-11a: the creative mode toggle (the C-key pattern — the
       // game's creative gamemode switch, no other consumer)
    FlyToggle, // V3-8b: the gamepad's DEDICATED flight toggle (the B/EAST
               // button — a single press flips the fly; the keyboard's
               // Space double-tap is untouched). Gamepad-only: no
               // keyboard key maps to it (mapKey never returns it).
    // Keep in sync with kKeyCount.
};

inline constexpr int kKeyCount = static_cast<int>(GameKey::FlyToggle) + 1; // G sits before FlyToggle (the enum tail = FlyToggle)

enum class MouseButton {
    Left,
    Right,
    Middle,
};

inline constexpr int kMouseButtonCount =
    static_cast<int>(MouseButton::Middle) + 1;

// V3-8: the input source bits (the m_keyDown / m_buttonDown masks — the
// sources of one action are OR'd, see the file header).
inline constexpr std::uint8_t kInputSourceKeyboard = 0x1;
inline constexpr std::uint8_t kInputSourceGamepad = 0x2;

// V3-8: the gamepad's axis deadzone (the standard digital threshold — both
// the left stick's digitize and the right stick's deadzone).
inline constexpr float kGamepadAxisDeadzone = 0.2f;
// V3-8: the look speed — the right stick's full deflection =
// kGamepadLookPixelsPerUnit pixels of synthetic mouseDelta per event, the
// mouseDelta pixels-equivalent scale (the player's mouseSensitivity then
// applies as for the mouse; the future config-ization anchor).
inline constexpr float kGamepadLookPixelsPerUnit = 100.0f;
// V3-8: the SDL axis raw range (Sint16, -32768..32767) — the normalization
// divisor to the float [-1, 1] scale (32768 keeps -32768 at exactly -1.0).
inline constexpr float kGamepadAxisFullScale = 32768.0f;

// V3-8: the gamepad button's mapped action (the project's two input
// targets: a GameKey or a MouseButton). std::monostate = the button is NOT
// mapped in v1 (the documented deviation: LT/RT are trigger AXES, not
// buttons; GUIDE / L3 / R3 / DPAD up-down-left / the paddles / the touchpad
// are intentionally unmapped).
using GamepadButtonAction =
    std::variant<std::monostate, GameKey, MouseButton>;

// V3-8: the button mapping table (a pure function — the unit tests pin it;
// the v1 table is the Xbox standard layout).
GamepadButtonAction mapGamepadButton(int sdlButton);

// V3-8: remap a normalized axis value from outside the deadzone to the full
// [-1, 1] range: sign(v) x min(1, (|v| - dz) / (1 - dz)); inside or on the
// deadzone (|v| <= dz) = 0. A pure function (the unit tests pin the
// formula).
float remapAxisOutsideDeadzone(float v, float dz);

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
        return m_keyDown[static_cast<int>(key)] != 0;
    }
    bool wasPressed(GameKey key) const {
        return m_keyPressed[static_cast<int>(key)];
    }
    bool isDown(MouseButton button) const {
        return m_buttonDown[static_cast<int>(button)] != 0;
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

    // V2-19: the mouse wheel's VERTICAL ticks accumulated since
    // frameBegin (positive = scroll up / away from the user). Input state
    // like the mouse delta: per-frame, reset by frameBegin — a no-input
    // run reads 0 forever (the structural-immunity root: the Game's
    // third-person wheel zoom only moves its session distance on a
    // nonzero reading).
    int wheelDelta() const { return m_wheelDelta; }
    // V3-11b: the frame's typed text / backspace (the wheelDelta twins).
    // kFrameTextMax = the frame buffer's bound (a synthetic injection
    // can never grow it without limit — public: the gate's tests pin it).
    static constexpr int kFrameTextMax = 256;
    const std::string& frameText() const { return m_frameText; }
    bool frameBackspace() const { return m_frameBackspace; }

    // Last absolute mouse position in window pixels (the UI screens read
    // it; while the pointer is captured SDL reports (0, 0) and the value
    // is stale by design — the UI is only used while the pointer is free).
    Vec2 mousePosition() const { return m_mousePos; }

    // V3-8: the gamepad's connection state (the F3 line + the Game's log).
    // Single-gamepad model: the first connected gamepad is tracked; a
    // second one is ignored until the first disconnects.
    bool gamepadConnected() const { return m_gamepadWhich != 0; }
    // The SDL device name ("unknown" when SDL has no mapping for the
    // device); only valid while gamepadConnected().
    const std::string& gamepadName() const { return m_gamepadName; }
    // V3-8a (11-06 real-device acceptance): true when the tracked pad's
    // OPEN succeeded. SDL 3.x delivers button/axis events only after an
    // explicit SDL_OpenGamepad; a failed open (e.g. permissions) is
    // logged as a warning — without the open no button can ever arrive.
    bool gamepadOpen() const { return m_gamepad != nullptr; }

    // -- Event entry points --------------------------------------------------
    // Used by pump() and directly by unit tests (SDL key codes are the
    // platform-independent SDLK_* virtual key codes).
    void handleKey(std::uint32_t sdlKeycode, bool down);
    void handleMouseButton(int sdlButton, bool down);
    void handleMouseMotion(float dx, float dy, float x, float y);
    // V2-19: one wheel event's vertical delta (SDL delivers ticks, not
    // absolute positions; the consumer accumulates through this handler).
    void handleMouseWheel(int deltaY);
    // V3-11b: one SDL_EVENT_TEXT_INPUT event's characters (the pump's
    // consumer + the unit tests' synthetic injection). RAW capture
    // (UTF-8, bounded) — the consumer (the creative search) filters the
    // vocabulary. NOT part of the tap vocabulary (InputEvent has no
    // string carrier, the demo's 8-type schema stays untouched — a
    // recorded demo can never type; the search is unit- + real-input
    // gated, documented deviation).
    void handleTextInput(const char* text);
    void handleQuit();
    // V3-8: the gamepad's entry points (same pattern as handleKey — the
    // unit tests drive them directly; pump() pre-filters the device: only
    // the tracked gamepad's button/axis events reach the handlers, and a
    // not-connected Input ignores them as well).
    void handleGamepadButton(int sdlButton, bool down);
    void handleGamepadAxis(int sdlAxis, std::int16_t value);
    void handleGamepadAdded(int which);
    void handleGamepadRemoved(int which);

    // V3-9 (demo replay / record): the event TAP — an optional observer
    // of every event reaching the public entry points (the demo
    // recorder's capture point: "record first, inject later" — the tap
    // fires BEFORE the state mutation, so recording is a zero-side-
    // effect observer). The raw arguments are the replay's
    // faithfulness: an unmapped keycode / button still taps (the replay
    // re-injects the same no-op). A null tap (the default — every
    // normal run) = one checked branch per event, bit-identical
    // behavior (the structural-immunity root).
    using EventTap = std::function<void(const InputEvent&)>;
    void setEventTap(EventTap tap) { m_eventTap = std::move(tap); }
    bool hasEventTap() const { return static_cast<bool>(m_eventTap); }

private:
    void tapEvent(const InputEvent& ev) const;

    static std::optional<GameKey> mapKey(std::uint32_t sdlKeycode);
    static std::optional<MouseButton> mapButton(int sdlButton);

    // V3-8: the source-bit operation shared by the keyboard/mouse and the
    // gamepad handlers: down sets THIS source's bit (the wasPressed edge on
    // this source's rising edge); up clears this source's bit ONLY.
    void setKeySource(GameKey key, std::uint8_t source, bool down);
    void setButtonSource(MouseButton button, std::uint8_t source, bool down);
    // V3-8: recompute the four movement directions' gamepad source bits
    // from the CACHED left-stick pair (each AXIS_MOTION event carries one
    // axis only); a full recompute on every event = idempotent (the stick
    // returning into the deadzone clears the direction naturally).
    void updateGamepadMovement();

    // V3-8: the source bitmasks (bit 0 keyboard/mouse, bit 1 gamepad).
    std::array<std::uint8_t, kKeyCount> m_keyDown {};
    std::array<bool, kKeyCount> m_keyPressed {};
    std::array<std::uint8_t, kMouseButtonCount> m_buttonDown {};
    std::array<bool, kMouseButtonCount> m_buttonPressed {};
    std::array<bool, kMouseButtonCount> m_buttonReleased {};
    Vec2 m_mouseDelta {0.0f, 0.0f};
    // V2-19: the frame's accumulated wheel ticks (reset by frameBegin,
    // like the mouse delta — input state, never a persisted level).
    int m_wheelDelta = 0;
    // V3-11b: the frame's TYPED text (the SDL_EVENT_TEXT_INPUT events'
    // raw UTF-8, reset by frameBegin — the wheel delta's twin: input
    // state, never a persisted level). Empty on every no-input frame
    // (SDL's text input is disabled by default + only started by the
    // creative panel — the structural-immunity root). Bounded to
    // kFrameTextMax characters (one frame of typing is small; the cap
    // is the synthetic-injection bound).
    std::string m_frameText;
    // V3-11b: the frame's backspace press (the key's frame state, reset
    // by frameBegin — the search's delete; Backspace maps to no
    // GameKey, so this flag is its only carrier).
    bool m_frameBackspace = false;
    // Absolute position: updated on every motion event, NOT reset by
    // frameBegin (the cursor sits where it last was).
    Vec2 m_mousePos {0.0f, 0.0f};
    bool m_quit = false;
    // V3-8: the single-gamepad model — the tracked device's SDL instance
    // id (0 = none; SDL ids are 1-based; SDL_JoystickID = Uint32). Only
    // this device's REMOVED disconnects; a second device is ignored (the
    // documented deviation, v1).
    std::uint32_t m_gamepadWhich = 0;
    // V3-8: the connected device's SDL name (the F3 line + the log;
    // "unknown" when SDL has no mapping for the device).
    std::string m_gamepadName;
    // V3-8a (11-06 real-device acceptance): the opened gamepad's handle
    // (nullptr = no pad, or the open failed). SDL delivers button/axis
    // events only after an explicit SDL_OpenGamepad (real-device
    // verified: zero events pre-open, all post-open — the gate-193
    // synthetic-event unit tests could never surface this); opening
    // also stabilizes the shanwan clone (unopened + button press = USB
    // disconnect, the 11-06 log chain).
    SDL_Gamepad* m_gamepad = nullptr;
    // V3-8: the cached left-stick raw values (Sint16; each axis event
    // updates its own, then the four directions are recomputed from the
    // pair). 0 = centered.
    std::int16_t m_gamepadLeftX = 0;
    std::int16_t m_gamepadLeftY = 0;
    // V3-9: the event tap (null in every normal run — see setEventTap).
    EventTap m_eventTap;
};

} // namespace mc
