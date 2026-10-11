// Eating channel tests (Phase 28, core/food.hpp): the 1.6 s eat animation
// state machine — hold to complete (consume + restore), release early to
// cancel (nothing consumed), plus the progress indicator mapping.
#include <gtest/gtest.h>

#include "core/food.hpp"

using namespace mc;

namespace {

// One update at a fixed real-dt step (the game's 60 fps frame).
constexpr double kDt = 1.0 / 60.0;

// 1.6 s of 1/60 s frames is 96 frames mathematically, but the IEEE-double
// sum of 96 steps of (1/60) lands a hair SHORT of the double 1.6 (the
// constant rounds up), so the boundary crosses on the 97th update. The
// tests pin the exact frame — the arithmetic is deterministic IEEE double.
constexpr int kFullEatFrames = 97;

} // namespace

TEST(EatChannelTest, StartsInactiveAndIdle) {
    EatChannel c;
    EXPECT_FALSE(c.active);
    EXPECT_FLOAT_EQ(c.progress(), 0.0f);
    EXPECT_EQ(c.update(kDt, true), EatEvent::None); // not started: no event
}

TEST(EatChannelTest, CompletesWhenHeldToTheEnd) {
    EatChannel c;
    c.start();
    ASSERT_TRUE(c.active);
    // One frame short of the full 1.6 s: still running, no event.
    for (int i = 1; i < kFullEatFrames; ++i) {
        EXPECT_EQ(c.update(kDt, true), EatEvent::None) << "frame " << i;
    }
    EXPECT_TRUE(c.active);
    // The final frame crosses the boundary: exactly one completion.
    EXPECT_EQ(c.update(kDt, true), EatEvent::Completed);
    EXPECT_FALSE(c.active);
    EXPECT_EQ(c.update(kDt, true), EatEvent::None); // terminal, fired once
}

TEST(EatChannelTest, EarlyReleaseCancels) {
    EatChannel c;
    c.start();
    // Halfway through the eat: releasing aborts — no completion event.
    for (int i = 1; i < kFullEatFrames / 2; ++i) {
        EXPECT_EQ(c.update(kDt, true), EatEvent::None);
    }
    EXPECT_EQ(c.update(kDt, false), EatEvent::Cancelled);
    EXPECT_FALSE(c.active);
    EXPECT_EQ(c.update(kDt, false), EatEvent::None); // terminal, fired once
}

TEST(EatChannelTest, ReReleaseWithoutHoldCancels) {
    // The button is already up on the first update after start (a stale
    // event or the lock was lost): the channel cancels immediately rather
    // than hanging.
    EatChannel c;
    c.start();
    EXPECT_EQ(c.update(kDt, false), EatEvent::Cancelled);
    EXPECT_FALSE(c.active);
}

TEST(EatChannelTest, ProgressRunsZeroToOne) {
    EatChannel c;
    c.start();
    EXPECT_LT(c.progress(), 0.01f); // just started
    for (int i = 0; i < kFullEatFrames / 2; ++i) { // 48 frames
        c.update(kDt, true);
    }
    // Halfway (48/97 frames ~ 0.5 of the clock): progress ~0.5, clamped
    // inside [0, 1].
    EXPECT_GT(c.progress(), 0.48f);
    EXPECT_LT(c.progress(), 0.52f);
    for (int i = 48; i < kFullEatFrames + 3; ++i) {
        if (c.update(kDt, true) == EatEvent::Completed) {
            break;
        }
    }
    EXPECT_FALSE(c.active);
    EXPECT_FLOAT_EQ(c.progress(), 0.0f); // completed: the indicator resets
}

TEST(EatChannelTest, ConstantsMatchMinecraft) {
    // The two data-free constants: the eat time (Minecraft's 1.6 s) and
    // the saturation factor (the cooked-food 2x rule of thumb).
    EXPECT_DOUBLE_EQ(kEatDurationSeconds, 1.6);
    EXPECT_FLOAT_EQ(kSaturationFactor, 2.0f);
}

// -- drinkable channel (Phase 43: the data-driven duration) ------------------

TEST(EatChannelTest, CustomDurationRunsAtItsOwnClock) {
    // A drinkable container arms the shared channel with its DATA-DRIVEN
    // drink_time (here 0.5 s): it completes at its own duration, not the
    // eat's 1.6 s. 0.5 s is 30 frames mathematically, but the IEEE-double
    // sum of 30 steps of (1/60) lands a hair SHORT of the double 0.5, so
    // the boundary crosses on the 31st update (the 97th-frame phenomenon
    // of the eat test, pinned the same way).
    EatChannel c;
    c.start(0.5);
    ASSERT_TRUE(c.active);
    for (int i = 1; i < 31; ++i) {
        EXPECT_EQ(c.update(kDt, true), EatEvent::None) << "frame " << i;
    }
    EXPECT_EQ(c.update(kDt, true), EatEvent::Completed);
    EXPECT_FALSE(c.active);
}

TEST(EatChannelTest, CustomDurationProgressNormalizesByOwnTotal) {
    // progress() divides by the channel's OWN duration (`total`): halfway
    // through a 0.5 s drink is ~0.5, not ~0.15 of the eat clock.
    EatChannel c;
    c.start(0.5);
    for (int i = 0; i < 15; ++i) { // 15 frames = 0.25 s
        c.update(kDt, true);
    }
    EXPECT_GT(c.progress(), 0.48f);
    EXPECT_LT(c.progress(), 0.52f);
}

TEST(EatChannelTest, CustomDurationEarlyReleaseStillCancels) {
    EatChannel c;
    c.start(0.5);
    for (int i = 1; i < 15; ++i) {
        EXPECT_EQ(c.update(kDt, true), EatEvent::None);
    }
    EXPECT_EQ(c.update(kDt, false), EatEvent::Cancelled);
    EXPECT_FALSE(c.active);
    EXPECT_EQ(c.update(kDt, false), EatEvent::None); // terminal, fired once
}

TEST(EatChannelTest, DefaultStartIsStillTheEatDuration) {
    // Backward compatibility: start() with no argument is the 1.6 s eat
    // (the existing tests pin the 97th-frame boundary).
    EatChannel c;
    c.start();
    EXPECT_DOUBLE_EQ(c.total, kEatDurationSeconds);
}
