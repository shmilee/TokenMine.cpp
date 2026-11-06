// V3-8: gamepad support (the input boundary layer) — the source-bitmask OR
// semantics, the v1 mapping table, the left stick's digitize, the right
// stick's synthetic mouseDelta, the single-gamepad model, and the legacy
// keyboard/mouse bit-identity fixture (the structural-immunity root: no
// gamepad = bit 1 never set = the pre-V3-8 bool-era readings).
#include <gtest/gtest.h>

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>

#include "core/input.hpp"

using namespace mc;

namespace {

// Connect the (single) gamepad — the SDL-less test process has no device,
// so the name resolves to "unknown" (SDL_GetGamepadNameForID = null).
void connectGamepad(Input& input, int which = 1) {
    input.handleGamepadAdded(which);
}

// The full public state snapshot of one Input (the OR-semantics and the
// zero-change assertions compare snapshots).
struct InputSnapshot {
    std::array<bool, kKeyCount> keyDown {};
    std::array<bool, kMouseButtonCount> buttonDown {};
    Vec2 mouseDelta {0.0f, 0.0f};
};

bool operator==(const InputSnapshot& a, const InputSnapshot& b) {
    return a.keyDown == b.keyDown && a.buttonDown == b.buttonDown &&
           a.mouseDelta == b.mouseDelta;
}

InputSnapshot snapshot(const Input& input) {
    InputSnapshot s;
    for (int i = 0; i < kKeyCount; ++i) {
        s.keyDown[i] = input.isDown(static_cast<GameKey>(i));
    }
    for (int i = 0; i < kMouseButtonCount; ++i) {
        s.buttonDown[i] = input.isDown(static_cast<MouseButton>(i));
    }
    s.mouseDelta = input.mouseDelta();
    return s;
}

// The variant comparison helper (C++20 has no variant == alternative):
// true when the action holds the alternative T with the value.
template <typename T>
bool actionIs(const GamepadButtonAction& action, const T& value) {
    return std::holds_alternative<T>(action) && std::get<T>(action) == value;
}

// The ten V3-8b-mapped buttons (the ruling table — the Xbox standard
// layout normalized; LT is a trigger AXIS not mapped, RT is the
// sprint's axis tested separately).
const std::array<int, 10>& mappedButtons() {
    static const std::array<int, 10> kMapped = {
        SDL_GAMEPAD_BUTTON_SOUTH,
        SDL_GAMEPAD_BUTTON_EAST,
        SDL_GAMEPAD_BUTTON_WEST,
        SDL_GAMEPAD_BUTTON_NORTH,
        SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,
        SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
        SDL_GAMEPAD_BUTTON_BACK,
        SDL_GAMEPAD_BUTTON_START,
        SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
        SDL_GAMEPAD_BUTTON_LEFT_STICK,
    };
    return kMapped;
}

} // namespace

TEST(GamepadTest, ButtonMappingTable) {
    // V3-8: the mapping pure function (the design's table — the v1 is the
    // Xbox standard layout).
    EXPECT_TRUE(actionIs(mapGamepadButton(SDL_GAMEPAD_BUTTON_SOUTH),
                         GameKey::Space));
    // V3-8b: B is the dedicated flight toggle (the sprint moved to
    // the RT trigger axis).
    EXPECT_TRUE(actionIs(mapGamepadButton(SDL_GAMEPAD_BUTTON_EAST),
                         GameKey::FlyToggle));
    EXPECT_TRUE(actionIs(mapGamepadButton(SDL_GAMEPAD_BUTTON_NORTH),
                         GameKey::E));
    EXPECT_TRUE(actionIs(mapGamepadButton(SDL_GAMEPAD_BUTTON_WEST),
                         GameKey::Q));
    EXPECT_TRUE(actionIs(mapGamepadButton(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER),
                         MouseButton::Left));
    EXPECT_TRUE(actionIs(mapGamepadButton(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER),
                         MouseButton::Right));
    EXPECT_TRUE(actionIs(mapGamepadButton(SDL_GAMEPAD_BUTTON_BACK),
                         GameKey::C));
    EXPECT_TRUE(actionIs(mapGamepadButton(SDL_GAMEPAD_BUTTON_START),
                         GameKey::Escape));
    EXPECT_TRUE(actionIs(mapGamepadButton(SDL_GAMEPAD_BUTTON_DPAD_RIGHT),
                         GameKey::Tab));
    // V3-8b: L3 fills the crouch gap (the keyboard's LeftControl's
    // gamepad twin).
    EXPECT_TRUE(actionIs(mapGamepadButton(SDL_GAMEPAD_BUTTON_LEFT_STICK),
                         GameKey::LeftControl));
}

TEST(GamepadTest, MappedButtonsPressAndRelease) {
    // V3-8: each mapped button presses exactly its GameKey/Button (the
    // gamepad's rising edge + level, the release clears the gamepad's
    // source bit — the mouse buttons also get the release edge, the bow's
    // semantics apply to a gamepad-driven RMB as well).
    struct {
        int sdl;
        GameKey key; // the key target when !isMouse
        MouseButton mouse; // the mouse target when isMouse
        bool isMouse;
    } cases[] = {
        {SDL_GAMEPAD_BUTTON_SOUTH, GameKey::Space, MouseButton::Left, false},
        {SDL_GAMEPAD_BUTTON_EAST, GameKey::FlyToggle, MouseButton::Left, false},
        {SDL_GAMEPAD_BUTTON_NORTH, GameKey::E, MouseButton::Left, false},
        {SDL_GAMEPAD_BUTTON_WEST, GameKey::Q, MouseButton::Left, false},
        {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, GameKey::W, MouseButton::Left, true},
        {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, GameKey::W, MouseButton::Right, true},
        {SDL_GAMEPAD_BUTTON_BACK, GameKey::C, MouseButton::Left, false},
        {SDL_GAMEPAD_BUTTON_START, GameKey::Escape, MouseButton::Left, false},
        {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, GameKey::Tab, MouseButton::Left, false},
        {SDL_GAMEPAD_BUTTON_LEFT_STICK, GameKey::LeftControl,
         MouseButton::Left, false},
    };
    for (const auto& c : cases) {
        Input input;
        input.frameBegin();
        connectGamepad(input);
        input.handleGamepadButton(c.sdl, true);
        if (c.isMouse) {
            EXPECT_TRUE(input.isDown(c.mouse)) << c.sdl;
            EXPECT_TRUE(input.wasPressed(c.mouse)) << c.sdl;
        } else {
            EXPECT_TRUE(input.isDown(c.key)) << c.sdl;
            EXPECT_TRUE(input.wasPressed(c.key)) << c.sdl;
        }
        input.frameBegin();
        input.handleGamepadButton(c.sdl, false);
        if (c.isMouse) {
            EXPECT_FALSE(input.isDown(c.mouse)) << c.sdl;
            EXPECT_TRUE(input.wasReleased(c.mouse)) << c.sdl;
        } else {
            EXPECT_FALSE(input.isDown(c.key)) << c.sdl;
        }
    }
}

TEST(GamepadTest, UnmappedButtonsZeroChange) {
    // V3-8: every valid SDL button NOT in the V3-8b table (GUIDE / R3 /
    // DPAD up-down-left / the paddles / the touchpad / the MISC family —
    // LT/RT are trigger AXES, the RT's sprint is tested separately) =
    // zero state change, over a primed state (the unmapped press must
    // not disturb it).
    const auto& kMapped = mappedButtons();
    Input input;
    input.frameBegin();
    connectGamepad(input);
    input.handleGamepadButton(SDL_GAMEPAD_BUTTON_SOUTH, true);
    input.handleGamepadAxis(SDL_GAMEPAD_AXIS_LEFTY, 6881);
    const InputSnapshot before = snapshot(input);
    for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; ++b) {
        if (std::find(kMapped.begin(), kMapped.end(), b) != kMapped.end()) {
            continue;
        }
        input.handleGamepadButton(b, true);
        input.handleGamepadButton(b, false);
        EXPECT_EQ(snapshot(input), before) << b;
    }
}

// ---------------------------------------------------------------------------
// V3-8b: the user's three habit rulings (L3 = crouch / RT = sprint /
// B = the dedicated flight toggle)
// ---------------------------------------------------------------------------

TEST(GamepadTest, RTTriggerSprintDigitize) {
    // V3-8b: the RT trigger = the sprint (the keyboard's LeftShift's
    // gamepad twin), the left-stick's digitize twin (the same 0.2
    // deadzone; the trigger is one-directional so the remap's sign IS
    // the pressed test). The boundary values mirror the stick's
    // pin: 6553/32768 = 0.19998 <= 0.2 inside, 6554 = 0.20001
    // outside.
    Input input;
    input.frameBegin();
    connectGamepad(input);
    // Below / on the deadzone = no sprint.
    for (int v : {0, 3000, 6553}) {
        input.handleGamepadAxis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,
                                static_cast<std::int16_t>(v));
        EXPECT_FALSE(input.isDown(GameKey::LeftShift)) << v;
    }
    // Past the deadzone = the press edge + held.
    input.frameBegin();
    input.handleGamepadAxis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 10000);
    EXPECT_TRUE(input.isDown(GameKey::LeftShift));
    EXPECT_TRUE(input.wasPressed(GameKey::LeftShift));
    input.frameBegin();
    EXPECT_TRUE(input.isDown(GameKey::LeftShift));
    EXPECT_FALSE(input.wasPressed(GameKey::LeftShift));
    // Back inside the deadzone = the release.
    input.frameBegin();
    input.handleGamepadAxis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 0);
    EXPECT_FALSE(input.isDown(GameKey::LeftShift));
    // The LT trigger stays unmapped (the documented deviation) — zero
    // state change even at full press.
    const InputSnapshot before = snapshot(input);
    input.handleGamepadAxis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 32767);
    input.handleGamepadAxis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 0);
    EXPECT_EQ(snapshot(input), before);
}

TEST(GamepadTest, L3CrouchHold) {
    // V3-8b: L3 (the left-stick press, a BUTTON in the SDL gamepad
    // vocabulary) = the crouch (the keyboard's LeftControl's gamepad
    // twin, held while the stick is pressed).
    Input input;
    input.frameBegin();
    connectGamepad(input);
    input.handleGamepadButton(SDL_GAMEPAD_BUTTON_LEFT_STICK, true);
    EXPECT_TRUE(input.isDown(GameKey::LeftControl));
    EXPECT_TRUE(input.wasPressed(GameKey::LeftControl));
    input.frameBegin();
    input.handleGamepadButton(SDL_GAMEPAD_BUTTON_LEFT_STICK, false);
    EXPECT_FALSE(input.isDown(GameKey::LeftControl));
}

TEST(GamepadTest, FlyToggleKeyIsGamepadOnly) {
    // V3-8b: GameKey::FlyToggle has NO keyboard twin (the keyboard's
    // fly stays the Space double-tap — the black-box proof: pressing
    // every virtual keycode never lands FlyToggle) and the B press
    // lands exactly its edge.
    Input kb;
    kb.frameBegin();
    for (int k = 1; k < 256; ++k) {
        kb.handleKey(static_cast<std::uint32_t>(k), true);
        EXPECT_FALSE(kb.isDown(GameKey::FlyToggle)) << k;
        kb.handleKey(static_cast<std::uint32_t>(k), false);
    }
    Input input;
    input.frameBegin();
    connectGamepad(input);
    input.handleGamepadButton(SDL_GAMEPAD_BUTTON_EAST, true);
    EXPECT_TRUE(input.wasPressed(GameKey::FlyToggle));
    EXPECT_FALSE(input.isDown(GameKey::Space));
    EXPECT_FALSE(input.isDown(GameKey::LeftShift));
    input.frameBegin();
    input.handleGamepadButton(SDL_GAMEPAD_BUTTON_EAST, false);
    EXPECT_FALSE(input.isDown(GameKey::FlyToggle));
}

TEST(GamepadTest, LeftStickDeadzoneBoundary) {
    // V3-8: the deadzone boundary (|v| <= dz = no key; the design: ±0.19
    // → nothing): 6225/32768 = 0.18997 < 0.2 inside; 6553/32768 =
    // 0.19998 <= 0.2 still inside (the <=); 6554/32768 = 0.20001 > 0.2
    // outside.
    Input input;
    input.frameBegin();
    connectGamepad(input);
    for (int v : {6225, -6225, 6553, -6553}) {
        input.handleGamepadAxis(SDL_GAMEPAD_AXIS_LEFTY,
                                static_cast<std::int16_t>(v));
        input.handleGamepadAxis(SDL_GAMEPAD_AXIS_LEFTX,
                                static_cast<std::int16_t>(v));
        EXPECT_FALSE(input.isDown(GameKey::W)) << v;
        EXPECT_FALSE(input.isDown(GameKey::S)) << v;
        EXPECT_FALSE(input.isDown(GameKey::A)) << v;
        EXPECT_FALSE(input.isDown(GameKey::D)) << v;
    }
    input.handleGamepadAxis(SDL_GAMEPAD_AXIS_LEFTY, 6554);
    EXPECT_TRUE(input.isDown(GameKey::W));
    EXPECT_TRUE(input.wasPressed(GameKey::W));
    input.frameBegin();
    EXPECT_FALSE(input.wasPressed(GameKey::W)); // the edge was on the press
    input.handleGamepadAxis(SDL_GAMEPAD_AXIS_LEFTY, -6554);
    EXPECT_FALSE(input.isDown(GameKey::W));
    EXPECT_TRUE(input.isDown(GameKey::S));
    EXPECT_TRUE(input.wasPressed(GameKey::S));
}

TEST(GamepadTest, LeftStickDigitize) {
    // V3-8: the left stick's digitize outside the deadzone (the design:
    // +Y→W, −Y→S, +X→D, −X→A): 6881/32768 = 0.20999 > 0.2.
    Input input;
    input.frameBegin();
    connectGamepad(input);
    input.handleGamepadAxis(SDL_GAMEPAD_AXIS_LEFTY, 6881);
    EXPECT_TRUE(input.isDown(GameKey::W));
    EXPECT_TRUE(input.wasPressed(GameKey::W)); // the stick's rising edge
    input.frameBegin();
    input.handleGamepadAxis(SDL_GAMEPAD_AXIS_LEFTY, -6881);
    EXPECT_FALSE(input.isDown(GameKey::W));
    EXPECT_TRUE(input.isDown(GameKey::S));
    EXPECT_TRUE(input.wasPressed(GameKey::S));
    input.frameBegin();
    input.handleGamepadAxis(SDL_GAMEPAD_AXIS_LEFTX, 6881);
    EXPECT_TRUE(input.isDown(GameKey::D));
    EXPECT_TRUE(input.wasPressed(GameKey::D));
    input.frameBegin();
    input.handleGamepadAxis(SDL_GAMEPAD_AXIS_LEFTX, -6881);
    EXPECT_FALSE(input.isDown(GameKey::D));
    EXPECT_TRUE(input.isDown(GameKey::A));
    EXPECT_TRUE(input.wasPressed(GameKey::A));
}

TEST(GamepadTest, LeftStickDiagonal) {
    // V3-8: a diagonal = both keys (W + D), each with its own rising edge
    // (no re-edge for the one already down).
    Input input;
    input.frameBegin();
    connectGamepad(input);
    input.handleGamepadAxis(SDL_GAMEPAD_AXIS_LEFTY, 6881);
    input.frameBegin();
    input.handleGamepadAxis(SDL_GAMEPAD_AXIS_LEFTX, 6881);
    EXPECT_TRUE(input.isDown(GameKey::W));
    EXPECT_TRUE(input.isDown(GameKey::D));
    EXPECT_TRUE(input.wasPressed(GameKey::D));
    EXPECT_FALSE(input.wasPressed(GameKey::W)); // already down (no re-edge)
    input.frameBegin();
    // The full return to center clears both.
    input.handleGamepadAxis(SDL_GAMEPAD_AXIS_LEFTY, 0);
    input.handleGamepadAxis(SDL_GAMEPAD_AXIS_LEFTX, 0);
    EXPECT_FALSE(input.isDown(GameKey::W));
    EXPECT_FALSE(input.isDown(GameKey::D));
}

TEST(GamepadTest, LeftStickReturnCenter) {
    // V3-8: the axis returning into the deadzone clears the direction
    // (the source-bit's falling — the same logic as a button up).
    Input input;
    input.frameBegin();
    connectGamepad(input);
    input.handleGamepadAxis(SDL_GAMEPAD_AXIS_LEFTY, 6881);
    EXPECT_TRUE(input.isDown(GameKey::W));
    input.frameBegin();
    input.handleGamepadAxis(SDL_GAMEPAD_AXIS_LEFTY, 0);
    EXPECT_FALSE(input.isDown(GameKey::W));
    EXPECT_FALSE(input.wasPressed(GameKey::W));
}

TEST(GamepadTest, LeftStickDirectionSwitch) {
    // V3-8: the direction switch (the design: up → right: W clears, D
    // sets) — the full recompute of the four directions' source bits is
    // idempotent (a repeated event changes nothing, sets no edges).
    Input input;
    input.frameBegin();
    connectGamepad(input);
    input.handleGamepadAxis(SDL_GAMEPAD_AXIS_LEFTY, 6881); // W
    input.frameBegin();
    input.handleGamepadAxis(SDL_GAMEPAD_AXIS_LEFTX, 6881); // W + D
    EXPECT_TRUE(input.isDown(GameKey::W));
    EXPECT_TRUE(input.isDown(GameKey::D));
    EXPECT_TRUE(input.wasPressed(GameKey::D));
    input.frameBegin();
    input.handleGamepadAxis(SDL_GAMEPAD_AXIS_LEFTY, 0); // D only
    EXPECT_FALSE(input.isDown(GameKey::W));
    EXPECT_TRUE(input.isDown(GameKey::D));
    EXPECT_FALSE(input.wasPressed(GameKey::D)); // no re-edge while held
    input.handleGamepadAxis(SDL_GAMEPAD_AXIS_LEFTY, 0); // idempotent
    EXPECT_TRUE(input.isDown(GameKey::D));
    EXPECT_FALSE(input.wasPressed(GameKey::D));
}

TEST(GamepadTest, RightStickInsideDeadzoneZeroDelta) {
    // V3-8: the right stick inside the deadzone = zero synthetic
    // mouseDelta (the no-input discipline holds per stick too).
    Input input;
    input.frameBegin();
    connectGamepad(input);
    for (int v : {0, 6225, -6225, 6553, -6553}) {
        input.handleGamepadAxis(SDL_GAMEPAD_AXIS_RIGHTX,
                                static_cast<std::int16_t>(v));
        input.handleGamepadAxis(SDL_GAMEPAD_AXIS_RIGHTY,
                                static_cast<std::int16_t>(v));
    }
    EXPECT_EQ(input.mouseDelta(), (Vec2 {0.0f, 0.0f}));
}

TEST(GamepadTest, RightStickSynthesizesMouseDelta) {
    // V3-8: the right stick's synthetic mouseDelta (the design's formula):
    // stick right = mouse right = xrel+; stick up (SDL +1) = mouse up =
    // yrel−; 16384/32768 = 0.5 (exact) → remap = (0.5 - 0.2)/(1 - 0.2)
    // ≈ 0.375 → ≈ 37.5 px (the kGamepadLookPixelsPerUnit 100 gain).
    Input input;
    input.frameBegin();
    connectGamepad(input);
    input.handleGamepadAxis(SDL_GAMEPAD_AXIS_RIGHTX, 16384);
    EXPECT_NEAR(input.mouseDelta().x, 37.5f, 1e-3f);
    EXPECT_FLOAT_EQ(input.mouseDelta().y, 0.0f);
    input.frameBegin();
    input.handleGamepadAxis(SDL_GAMEPAD_AXIS_RIGHTX, -16384);
    EXPECT_NEAR(input.mouseDelta().x, -37.5f, 1e-3f);
    input.frameBegin();
    input.handleGamepadAxis(SDL_GAMEPAD_AXIS_RIGHTY, 16384); // stick UP
    EXPECT_NEAR(input.mouseDelta().y, -37.5f, 1e-3f);
    input.frameBegin();
    input.handleGamepadAxis(SDL_GAMEPAD_AXIS_RIGHTY, -16384); // stick DOWN
    EXPECT_NEAR(input.mouseDelta().y, 37.5f, 1e-3f);
    // The accumulation (two events in one frame) + the per-frame reset
    // (the mouse-delta discipline).
    input.handleGamepadAxis(SDL_GAMEPAD_AXIS_RIGHTX, 16384);
    input.handleGamepadAxis(SDL_GAMEPAD_AXIS_RIGHTX, 16384);
    EXPECT_NEAR(input.mouseDelta().x, 75.0f, 2e-3f);
    input.frameBegin();
    EXPECT_EQ(input.mouseDelta(), (Vec2 {0.0f, 0.0f}));
}

TEST(GamepadTest, RemapAxisFormulaPinned) {
    // V3-8: the remap pure function — sign(v) x min(1, (|v| - dz) /
    // (1 - dz)); |v| <= dz = 0 (the design's formula). The design's
    // worked example "v=0.7, dz=0.2" is pinned here at the formula-
    // correct 0.625 ((0.7 - 0.2)/(1 - 0.2) = 5/8); 0.875 is the v=0.9
    // value.
    const float dz = kGamepadAxisDeadzone;
    EXPECT_FLOAT_EQ(remapAxisOutsideDeadzone(0.0f, dz), 0.0f);
    EXPECT_FLOAT_EQ(remapAxisOutsideDeadzone(dz, dz), 0.0f); // |v| <= dz
    EXPECT_FLOAT_EQ(remapAxisOutsideDeadzone(-dz, dz), 0.0f);
    EXPECT_NEAR(remapAxisOutsideDeadzone(0.7f, dz), 0.625f, 1e-6f);
    EXPECT_NEAR(remapAxisOutsideDeadzone(-0.7f, dz), -0.625f, 1e-6f);
    EXPECT_NEAR(remapAxisOutsideDeadzone(0.9f, dz), 0.875f, 1e-6f);
    EXPECT_NEAR(remapAxisOutsideDeadzone(1.0f, dz), 1.0f, 1e-6f);
    EXPECT_NEAR(remapAxisOutsideDeadzone(-1.0f, dz), -1.0f, 1e-6f);
    // The min(1, ...) clamp (an out-of-range input cannot come from SDL;
    // the function stays total).
    EXPECT_FLOAT_EQ(remapAxisOutsideDeadzone(1.5f, dz), 1.0f);
    EXPECT_FLOAT_EQ(remapAxisOutsideDeadzone(-1.5f, dz), -1.0f);
    // The constant pins (no magic numbers): the design's §2 values.
    EXPECT_FLOAT_EQ(kGamepadAxisDeadzone, 0.2f);
    EXPECT_FLOAT_EQ(kGamepadLookPixelsPerUnit, 100.0f);
    EXPECT_EQ(kInputSourceKeyboard, 0x1);
    EXPECT_EQ(kInputSourceGamepad, 0x2);
}

TEST(GamepadTest, LeftTriggerUnmappedZeroChange) {
    // V3-8b: the LT trigger stays unmapped (the documented deviation)
    // = zero state change at any value (the RT moved to the sprint —
    // RTTriggerSprintDigitize covers it).
    Input input;
    input.frameBegin();
    connectGamepad(input);
    const InputSnapshot before = snapshot(input);
    for (int v : {0, 32767, -32767, 16384}) {
        input.handleGamepadAxis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER,
                                static_cast<std::int16_t>(v));
    }
    EXPECT_EQ(snapshot(input), before);
}

TEST(GamepadTest, SourceOrSemanticsKeys) {
    // V3-8: the source-bit OR semantics (keys): keyboard W (bit 0) +
    // stick up (bit 1) → isDown once; the keyboard's release keeps the
    // stick-held W down (the OR); the stick's return to center clears it.
    // Each source has its OWN rising edge into wasPressed (the second
    // source pressing an already-down action sets the edge again — the
    // documented semantics).
    Input input;
    input.frameBegin();
    connectGamepad(input);
    input.handleKey(SDLK_W, true);
    EXPECT_TRUE(input.isDown(GameKey::W));
    EXPECT_TRUE(input.wasPressed(GameKey::W));
    input.frameBegin();
    input.handleGamepadAxis(SDL_GAMEPAD_AXIS_LEFTY, 6881); // stick up
    EXPECT_TRUE(input.isDown(GameKey::W)); // the OR (still down)
    EXPECT_TRUE(input.wasPressed(GameKey::W)); // the gamepad's rising edge
    input.frameBegin();
    input.handleKey(SDLK_W, false); // the keyboard releases, the stick up
    EXPECT_TRUE(input.isDown(GameKey::W)); // the OR keeps it down
    EXPECT_FALSE(input.wasPressed(GameKey::W));
    input.frameBegin();
    input.handleGamepadAxis(SDL_GAMEPAD_AXIS_LEFTY, 0); // the stick centers
    EXPECT_FALSE(input.isDown(GameKey::W)); // cleared
    // The keyboard still works normally afterwards.
    input.handleKey(SDLK_W, true);
    EXPECT_TRUE(input.isDown(GameKey::W));
    EXPECT_TRUE(input.wasPressed(GameKey::W));
}

TEST(GamepadTest, SourceOrSemanticsButtonsBow) {
    // V3-8: the source-bit OR semantics (mouse buttons, the bow):
    // wasReleased fires only when the release clears the WHOLE mask —
    // while RB (the gamepad's Right) is held, the mouse's RMB release =
    // no release edge (the bow keeps its draw).
    Input input;
    input.frameBegin();
    connectGamepad(input);
    input.handleGamepadButton(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, true);
    EXPECT_TRUE(input.isDown(MouseButton::Right));
    EXPECT_TRUE(input.wasPressed(MouseButton::Right));
    input.frameBegin();
    input.handleMouseButton(SDL_BUTTON_RIGHT, true); // the mouse joins the OR
    EXPECT_TRUE(input.isDown(MouseButton::Right));
    EXPECT_TRUE(input.wasPressed(MouseButton::Right)); // the mouse's edge
    input.frameBegin();
    input.handleMouseButton(SDL_BUTTON_RIGHT, false); // the mouse releases
    EXPECT_TRUE(input.isDown(MouseButton::Right)); // RB still holds
    EXPECT_FALSE(input.wasReleased(MouseButton::Right)); // the bow: no release
    input.frameBegin();
    input.handleGamepadButton(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, false);
    EXPECT_FALSE(input.isDown(MouseButton::Right));
    EXPECT_TRUE(input.wasReleased(MouseButton::Right)); // the mask cleared
    input.frameBegin();
    EXPECT_FALSE(input.wasReleased(MouseButton::Right)); // the edge consumed
    // The reverse order: the mouse first, then RB — the RB's release
    // (the mouse still holds) = no release, the mouse's release = it.
    input.handleMouseButton(SDL_BUTTON_RIGHT, true);
    EXPECT_TRUE(input.wasPressed(MouseButton::Right));
    input.frameBegin();
    input.handleGamepadButton(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, true);
    input.frameBegin();
    input.handleGamepadButton(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, false);
    EXPECT_TRUE(input.isDown(MouseButton::Right));
    EXPECT_FALSE(input.wasReleased(MouseButton::Right));
    input.frameBegin();
    input.handleMouseButton(SDL_BUTTON_RIGHT, false);
    EXPECT_FALSE(input.isDown(MouseButton::Right));
    EXPECT_TRUE(input.wasReleased(MouseButton::Right));
}

TEST(GamepadTest, DisconnectClearsAllGamepadBits) {
    // V3-8: the disconnect clears every gamepad source bit (the button-
    // held keys + the stick's directions); the keyboard's bits survive;
    // the keyboard still works normally afterwards (a fresh reconnect
    // starts from a reset stick cache).
    Input input;
    input.frameBegin();
    connectGamepad(input);
    input.handleKey(SDLK_D, true); // the keyboard holds D (bit 0)
    input.handleGamepadButton(SDL_GAMEPAD_BUTTON_SOUTH, true); // A → Space
    input.handleGamepadButton(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, true); // LB → Left
    input.handleGamepadAxis(SDL_GAMEPAD_AXIS_LEFTY, 6881); // the stick → W
    EXPECT_TRUE(input.isDown(GameKey::Space));
    EXPECT_TRUE(input.isDown(MouseButton::Left));
    EXPECT_TRUE(input.isDown(GameKey::W));
    input.handleGamepadRemoved(1);
    EXPECT_FALSE(input.gamepadConnected());
    EXPECT_FALSE(input.isDown(GameKey::Space));
    EXPECT_FALSE(input.isDown(MouseButton::Left));
    EXPECT_FALSE(input.isDown(GameKey::W));
    EXPECT_TRUE(input.isDown(GameKey::D)); // the keyboard's bit survives
    // The keyboard still works normally afterwards (the bit-0 path intact).
    input.handleKey(SDLK_W, true);
    EXPECT_TRUE(input.isDown(GameKey::W));
    EXPECT_TRUE(input.wasPressed(GameKey::W));
    input.frameBegin();
    // A reconnect works (the single-gamepad model re-arms).
    input.handleGamepadAdded(2);
    EXPECT_TRUE(input.gamepadConnected());
    input.handleGamepadAxis(SDL_GAMEPAD_AXIS_LEFTY, -6881); // the stick → S
    EXPECT_TRUE(input.isDown(GameKey::S));
    input.frameBegin();
    input.handleGamepadRemoved(2);
    EXPECT_FALSE(input.gamepadConnected());
    EXPECT_FALSE(input.isDown(GameKey::S));
    EXPECT_TRUE(input.isDown(GameKey::W)); // the keyboard's bit again
    input.handleGamepadRemoved(2); // stale: a no-op after the disconnect
    EXPECT_TRUE(input.isDown(GameKey::W));
}

TEST(GamepadTest, SingleGamepadModel) {
    // V3-8: the single-gamepad model — the first connected gamepad is
    // tracked; a second one's ADDED is ignored (the design: the second
    // gamepad is ignored, v1); a second one's REMOVED is a no-op.
    Input input;
    input.frameBegin();
    input.handleGamepadAdded(1);
    EXPECT_TRUE(input.gamepadConnected());
    const std::string name1 = input.gamepadName();
    input.handleGamepadAdded(2); // the second gamepad = ignored
    EXPECT_TRUE(input.gamepadConnected());
    EXPECT_EQ(input.gamepadName(), name1); // the tracking did not move
    input.handleGamepadButton(SDL_GAMEPAD_BUTTON_SOUTH, true);
    EXPECT_TRUE(input.isDown(GameKey::Space));
    input.handleGamepadRemoved(2); // the second one's REMOVED = a no-op
    EXPECT_TRUE(input.gamepadConnected());
    EXPECT_TRUE(input.isDown(GameKey::Space));
    input.handleGamepadRemoved(1); // only the tracked one's REMOVED
    EXPECT_FALSE(input.gamepadConnected());
    EXPECT_FALSE(input.isDown(GameKey::Space));
}

TEST(GamepadTest, HandlersNoOpWhenNotConnected) {
    // V3-8: without a connected gamepad the button/axis handlers are
    // no-ops (the single-gamepad model's guard — no tracked device, no
    // state); a REMOVED of any device is a no-op too.
    Input input;
    input.frameBegin();
    const InputSnapshot before = snapshot(input);
    input.handleGamepadButton(SDL_GAMEPAD_BUTTON_SOUTH, true);
    input.handleGamepadButton(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, true);
    input.handleGamepadAxis(SDL_GAMEPAD_AXIS_LEFTY, 6881);
    input.handleGamepadAxis(SDL_GAMEPAD_AXIS_RIGHTX, 16384);
    input.handleGamepadRemoved(7);
    EXPECT_FALSE(input.gamepadConnected());
    EXPECT_EQ(snapshot(input), before);
}

TEST(GamepadTest, LegacyKeyboardMouseBitIdentical) {
    // V3-8: the LEGACY BIT-IDENTITY fixture (the red line): a pure
    // keyboard + mouse event sequence (no gamepad event at all) reads
    // bit-for-bit the same as the pre-V3-8 bool implementation — no
    // gamepad = bit 1 never set = the structural-immunity root. The
    // expected values are the bool-era readings (the pre-V3-8 unit-test
    // semantics, re-pinned through the source-bitmask refactor).
    Input input;
    input.frameBegin();
    EXPECT_FALSE(input.gamepadConnected());
    // Fresh state: nothing down, nothing pressed, no release.
    for (int i = 0; i < kKeyCount; ++i) {
        EXPECT_FALSE(input.isDown(static_cast<GameKey>(i))) << i;
        EXPECT_FALSE(input.wasPressed(static_cast<GameKey>(i))) << i;
    }
    for (int i = 0; i < kMouseButtonCount; ++i) {
        EXPECT_FALSE(input.isDown(static_cast<MouseButton>(i))) << i;
        EXPECT_FALSE(input.wasReleased(static_cast<MouseButton>(i))) << i;
    }
    // A key's level + edge (the bool era: down = false → the edge on the
    // press, holding = no re-edge, up = the level clears).
    input.handleKey(SDLK_W, true);
    EXPECT_TRUE(input.isDown(GameKey::W));
    EXPECT_TRUE(input.wasPressed(GameKey::W));
    input.frameBegin();
    EXPECT_TRUE(input.isDown(GameKey::W));
    EXPECT_FALSE(input.wasPressed(GameKey::W));
    input.handleKey(SDLK_W, true); // holding = no re-edge
    EXPECT_FALSE(input.wasPressed(GameKey::W));
    input.handleKey(SDLK_W, false);
    EXPECT_FALSE(input.isDown(GameKey::W));
    // An unmapped key is ignored (the bool era: zero state change).
    input.handleKey(SDLK_X, true);
    for (int i = 0; i < kKeyCount; ++i) {
        EXPECT_FALSE(input.isDown(static_cast<GameKey>(i))) << i;
    }
    // A mouse button's level + edge + release (the Phase 24 semantics).
    input.handleMouseButton(SDL_BUTTON_LEFT, true);
    EXPECT_TRUE(input.isDown(MouseButton::Left));
    EXPECT_TRUE(input.wasPressed(MouseButton::Left));
    input.frameBegin();
    EXPECT_FALSE(input.wasPressed(MouseButton::Left));
    input.handleMouseButton(SDL_BUTTON_LEFT, false);
    EXPECT_FALSE(input.isDown(MouseButton::Left));
    EXPECT_TRUE(input.wasReleased(MouseButton::Left));
    input.frameBegin();
    EXPECT_FALSE(input.wasReleased(MouseButton::Left));
    // A stale up (never down) = no release edge (the bool era).
    input.handleMouseButton(SDL_BUTTON_MIDDLE, false);
    EXPECT_FALSE(input.wasReleased(MouseButton::Middle));
    // The mouse delta accumulates + resets (the bool era).
    input.handleMouseMotion(5.0f, -3.0f, 10.0f, 20.0f);
    input.handleMouseMotion(2.0f, 1.0f, 12.0f, 21.0f);
    EXPECT_EQ(input.mouseDelta(), (Vec2 {7.0f, -2.0f}));
    EXPECT_EQ(input.mousePosition(), (Vec2 {12.0f, 21.0f}));
    input.frameBegin();
    EXPECT_EQ(input.mouseDelta(), (Vec2 {0.0f, 0.0f}));
    EXPECT_EQ(input.mousePosition(), (Vec2 {12.0f, 21.0f})); // not reset
    // The wheel (the V2-19 semantics).
    input.handleMouseWheel(1);
    input.handleMouseWheel(-1);
    input.handleMouseWheel(2);
    EXPECT_EQ(input.wheelDelta(), 2);
    input.frameBegin();
    EXPECT_EQ(input.wheelDelta(), 0);
    // The level state survives the frameBegin (the bool era).
    input.handleKey(SDLK_F5, true);
    input.frameBegin();
    EXPECT_TRUE(input.isDown(GameKey::F5));
    input.handleKey(SDLK_F5, false);
    EXPECT_FALSE(input.isDown(GameKey::F5));
    // The quit flag is sticky (the bool era).
    EXPECT_FALSE(input.quitRequested());
    input.handleQuit();
    EXPECT_TRUE(input.quitRequested());
    input.frameBegin();
    EXPECT_TRUE(input.quitRequested());
    // No gamepad event ever happened: the snapshot's keyboard-only
    // invariants hold end to end (bit 1 never set — the structural
    // immunity's root).
    EXPECT_FALSE(input.gamepadConnected());
}

// V3-8a (11-06 real-device acceptance): an OPEN failure (the SDL-less
// test process = no such device) must not break the tracking state:
// the buttons still map into the source bits and the REMOVED still
// matches and clears. (The open's success path is real-device-only —
// the gate-strength note applies as for the rest of V3-8.)
TEST(GamepadTest, OpenFailureKeepsTrackingAndMapping) {
    Input input;
    input.handleGamepadAdded(999); // no device: the open fails (logged)
    EXPECT_TRUE(input.gamepadConnected());
    EXPECT_FALSE(input.gamepadOpen());
    input.handleGamepadButton(SDL_GAMEPAD_BUTTON_SOUTH, true);
    EXPECT_TRUE(input.isDown(GameKey::Space));
    input.handleGamepadButton(SDL_GAMEPAD_BUTTON_SOUTH, false);
    EXPECT_FALSE(input.isDown(GameKey::Space));
    input.handleGamepadRemoved(999);
    EXPECT_FALSE(input.gamepadConnected());
    EXPECT_FALSE(input.gamepadOpen());
}
