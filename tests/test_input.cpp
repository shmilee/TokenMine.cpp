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

TEST(InputTest, MouseDeltaAccumulatesAndResets) {
    Input input;
    input.frameBegin();
    input.handleMouseMotion(5.0f, -3.0f);
    input.handleMouseMotion(2.0f, 1.0f);
    EXPECT_EQ(input.mouseDelta(), (Vec2{7.0f, -2.0f}));

    input.frameBegin();
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
