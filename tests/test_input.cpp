#include <gtest/gtest.h>

#include <SDL3/SDL.h>

#include "core/input.hpp"

using namespace mc;

TEST(InputTest, KeyLevelAndEdge) {
    Input input;
    input.frameBegin();

    EXPECT_FALSE(input.isDown(GameKey::W));
    input.handleKey(SDLK_W, true);
    EXPECT_TRUE(input.isDown(GameKey::W));
    EXPECT_TRUE(input.wasPressed(GameKey::W)); // edge on this frame

    input.frameBegin();
    EXPECT_TRUE(input.isDown(GameKey::W));  // still held
    EXPECT_FALSE(input.wasPressed(GameKey::W)); // edge consumed

    // Holding does not re-trigger the edge.
    input.handleKey(SDLK_W, true);
    EXPECT_FALSE(input.wasPressed(GameKey::W));

    input.handleKey(SDLK_W, false);
    EXPECT_FALSE(input.isDown(GameKey::W));
}

TEST(InputTest, UnmappedKeysIgnored) {
    Input input;
    input.frameBegin();
    input.handleKey(SDLK_X, true);
    input.handleKey(SDLK_F12, true);
    for (int i = 0; i < kKeyCount; ++i) {
        EXPECT_FALSE(input.isDown(static_cast<GameKey>(i))) << i;
    }
}

TEST(InputTest, AllMappedKeys) {
    struct {
        std::uint32_t sdl;
        GameKey key;
    } cases[] = {
        {SDLK_W, GameKey::W},
        {SDLK_A, GameKey::A},
        {SDLK_S, GameKey::S},
        {SDLK_D, GameKey::D},
        {SDLK_SPACE, GameKey::Space},
        {SDLK_LSHIFT, GameKey::LeftShift},
        {SDLK_LCTRL, GameKey::LeftControl},
        {SDLK_LALT, GameKey::LeftAlt},
        {SDLK_TAB, GameKey::Tab},
        {SDLK_ESCAPE, GameKey::Escape},
        {SDLK_Q, GameKey::Q},
        {SDLK_E, GameKey::E},
        {SDLK_F3, GameKey::F3},
        {SDLK_F5, GameKey::F5},
        {SDLK_R, GameKey::R},
        // UX-4: the camera toggle.
        {SDLK_C, GameKey::C},
        // CRAFT-7: the top-row digits (hotbar selection) and the right
        // shift (it drives the shift-click quick-move, mapped onto the
        // left one).
        {SDLK_RSHIFT, GameKey::LeftShift},
        {SDLK_1, GameKey::Digit1},
        {SDLK_2, GameKey::Digit2},
        {SDLK_3, GameKey::Digit3},
        {SDLK_4, GameKey::Digit4},
        {SDLK_5, GameKey::Digit5},
        {SDLK_6, GameKey::Digit6},
        {SDLK_7, GameKey::Digit7},
        {SDLK_8, GameKey::Digit8},
        {SDLK_9, GameKey::Digit9},
    };
    Input input;
    input.frameBegin();
    for (const auto& c : cases) {
        input.handleKey(c.sdl, true);
        EXPECT_TRUE(input.isDown(c.key)) << c.sdl;
        EXPECT_TRUE(input.wasPressed(c.key));
    }
}

TEST(InputTest, MouseButtons) {
    Input input;
    input.frameBegin();

    input.handleMouseButton(SDL_BUTTON_LEFT, true);
    EXPECT_TRUE(input.isDown(MouseButton::Left));
    EXPECT_TRUE(input.wasPressed(MouseButton::Left));

    input.frameBegin();
    EXPECT_TRUE(input.isDown(MouseButton::Left));
    EXPECT_FALSE(input.wasPressed(MouseButton::Left));

    input.handleMouseButton(SDL_BUTTON_LEFT, false);
    EXPECT_FALSE(input.isDown(MouseButton::Left));

    input.handleMouseButton(SDL_BUTTON_RIGHT, true);
    input.handleMouseButton(SDL_BUTTON_MIDDLE, true);
    EXPECT_TRUE(input.isDown(MouseButton::Right));
    EXPECT_TRUE(input.isDown(MouseButton::Middle));
    // Unmapped button 4+ is ignored.
    input.handleMouseButton(4, true);
}

TEST(InputTest, MouseButtonReleaseEdge) {
    // Phase 24: wasReleased is the mirror of wasPressed — true once, on
    // the frame the button went up (the bow fires on the right release).
    Input input;
    input.frameBegin();
    input.handleMouseButton(SDL_BUTTON_RIGHT, true);
    EXPECT_TRUE(input.wasPressed(MouseButton::Right));
    EXPECT_FALSE(input.wasReleased(MouseButton::Right));

    input.frameBegin();
    EXPECT_TRUE(input.isDown(MouseButton::Right)); // still held
    input.handleMouseButton(SDL_BUTTON_RIGHT, false);
    EXPECT_FALSE(input.isDown(MouseButton::Right));
    EXPECT_FALSE(input.wasPressed(MouseButton::Right));
    EXPECT_TRUE(input.wasReleased(MouseButton::Right)); // edge on this frame

    input.frameBegin();
    EXPECT_FALSE(input.wasReleased(MouseButton::Right)); // edge consumed

    // A stale up-event (the button was never down) sets no release edge.
    input.handleMouseButton(SDL_BUTTON_MIDDLE, false);
    EXPECT_FALSE(input.wasReleased(MouseButton::Middle));
}

TEST(InputTest, MouseDeltaAccumulatesAndResets) {
    Input input;
    input.frameBegin();
    input.handleMouseMotion(5.0f, -3.0f, 0.0f, 0.0f);
    input.handleMouseMotion(2.0f, 1.0f, 0.0f, 0.0f);
    EXPECT_EQ(input.mouseDelta(), (Vec2{7.0f, -2.0f}));

    input.frameBegin();
    EXPECT_EQ(input.mouseDelta(), (Vec2{0.0f, 0.0f}));
}

TEST(InputTest, MousePositionTracksLastAbsolute) {
    Input input;
    input.frameBegin();
    EXPECT_EQ(input.mousePosition(), (Vec2{0.0f, 0.0f}));
    input.handleMouseMotion(1.0f, 1.0f, 100.0f, 200.0f);
    EXPECT_EQ(input.mousePosition(), (Vec2{100.0f, 200.0f}));
    input.handleMouseMotion(1.0f, 1.0f, 130.0f, 250.0f);
    EXPECT_EQ(input.mousePosition(), (Vec2{130.0f, 250.0f}));
    // The absolute position survives frameBegin (the cursor sits where it
    // last was); only the delta resets.
    input.frameBegin();
    EXPECT_EQ(input.mousePosition(), (Vec2{130.0f, 250.0f}));
    EXPECT_EQ(input.mouseDelta(), (Vec2{0.0f, 0.0f}));
}

TEST(InputTest, QuitFlagSticky) {
    Input input;
    input.frameBegin();
    EXPECT_FALSE(input.quitRequested());
    input.handleQuit();
    EXPECT_TRUE(input.quitRequested());
    input.frameBegin(); // frameBegin must not clear quit
    EXPECT_TRUE(input.quitRequested());
}

TEST(InputTest, PumpWithoutVideoSubsystemIsSafe) {
    // No SDL video subsystem initialized in the test process; pump() must
    // simply find no events and not crash.
    Input input;
    input.frameBegin();
    input.pump();
    EXPECT_FALSE(input.quitRequested());
}
