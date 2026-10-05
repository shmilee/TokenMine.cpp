// Bow charge model tests (Phase 24): the pure draw-time -> power mapping
// shared by the game (core/bow.hpp) — an instant release fires at the
// minimum power, the charge fills linearly, and a full draw (or longer)
// is exactly the item's full data values.
#include <gtest/gtest.h>

#include "core/bow.hpp"

using namespace mc;

TEST(BowChargeTest, InstantReleaseIsMinimumPower) {
    // A press and release in the same frame still fires (Minecraft's
    // quick click) — at the 10 % minimum power.
    EXPECT_FLOAT_EQ(bowPower(0.0), kBowMinPower);
    // Negative draw times are clamped to the minimum too.
    EXPECT_FLOAT_EQ(bowPower(-1.0), kBowMinPower);
}

TEST(BowChargeTest, FillsLinearlyToFullAtFullDraw) {
    // The mapping is clamp(t / kBowFullDrawSeconds, 0.1, 1.0) (Minecraft:
    // power = draw time / 1.6 s, floored at 0.1): half the draw time is
    // exactly half power, a quarter is a quarter.
    EXPECT_NEAR(bowPower(kBowFullDrawSeconds * 0.5), 0.5f, 1e-6f);
    EXPECT_NEAR(bowPower(kBowFullDrawSeconds * 0.25), 0.25f, 1e-6f);
    // And the floor only kicks in below 0.16 s (0.1 * 1.6).
    EXPECT_FLOAT_EQ(bowPower(0.08), kBowMinPower);
    EXPECT_NEAR(bowPower(0.16), 0.1f, 1e-6f);
}

TEST(BowChargeTest, FullDrawIsFullPower) {
    EXPECT_FLOAT_EQ(bowPower(kBowFullDrawSeconds), 1.0f);
}

TEST(BowChargeTest, OverchargeIsCapped) {
    // Holding past the full-draw time stays at full power.
    EXPECT_FLOAT_EQ(bowPower(kBowFullDrawSeconds * 10.0), 1.0f);
}
