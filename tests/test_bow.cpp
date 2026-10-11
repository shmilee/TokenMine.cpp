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

// -- arrow mob-hit knockback (Phase 42) ---------------------------------------

TEST(ArrowKnockbackTest, FullPowerMatchesLegacyConstantsBitExactly) {
    // A full-power shot (power 1.0) reproduces the Phase 21/27 melee-style
    // constants (4.0 horizontal along the arrow, 2.5 lift) — 16.0 keeps
    // the normalization exact in binary (4 / 16 = 0.25).
    const Vec3 kb = arrowKnockback({16.0f, -3.0f, 0.0f}, 1.0f);
    EXPECT_FLOAT_EQ(kb.x, kArrowKnockbackSpeed);
    EXPECT_FLOAT_EQ(kb.y, kArrowKnockbackLift);
    EXPECT_FLOAT_EQ(kb.z, 0.0f); // the vertical component is dropped
}

TEST(ArrowKnockbackTest, ScalesWithChargePower) {
    // A half-power shot knocks back at half the impulse in both axes
    // (Minecraft: knockback grows with the shot's strength).
    const Vec3 kb = arrowKnockback({16.0f, 0.0f, 0.0f}, 0.5f);
    EXPECT_FLOAT_EQ(kb.x, 2.0f);
    EXPECT_FLOAT_EQ(kb.y, 1.25f);
    const Vec3 quarter = arrowKnockback({16.0f, 0.0f, 0.0f}, 0.25f);
    EXPECT_FLOAT_EQ(quarter.x, 1.0f);
    EXPECT_FLOAT_EQ(quarter.y, 0.625f);
}

TEST(ArrowKnockbackTest, PushesAlongHorizontalTravelDirection) {
    // The horizontal push follows the arrow's horizontal velocity
    // (normalized to the constant magnitude); a diagonal shot splits it.
    // (3, 4) normalizes to (0.6, 0.8): 4 * 0.6 = 2.4, 4 * 0.8 = 3.2.
    const Vec3 kb = arrowKnockback({3.0f, 9.0f, 4.0f}, 1.0f);
    EXPECT_NEAR(kb.x, 2.4f, 1e-5f);
    EXPECT_NEAR(kb.z, 3.2f, 1e-5f);
    EXPECT_FLOAT_EQ(kb.y, kArrowKnockbackLift);
    // A shot heading in -x pushes in -x.
    const Vec3 back = arrowKnockback({-16.0f, 0.0f, 0.0f}, 1.0f);
    EXPECT_FLOAT_EQ(back.x, -kArrowKnockbackSpeed);
}

TEST(ArrowKnockbackTest, StraightDownKeepsLiftOnly) {
    // A (near-)vertically falling arrow has no horizontal push — the
    // lift stays (the 1e-4 guard mirrors the Phase 21 code).
    const Vec3 kb = arrowKnockback({0.0f, -12.0f, 0.0f}, 1.0f);
    EXPECT_FLOAT_EQ(kb.x, 0.0f);
    EXPECT_FLOAT_EQ(kb.z, 0.0f);
    EXPECT_FLOAT_EQ(kb.y, kArrowKnockbackLift);
    const Vec3 half = arrowKnockback({0.0f, -12.0f, 0.0f}, 0.5f);
    EXPECT_FLOAT_EQ(half.y, 1.25f);
}
