// PRE-2: the distance-fog range pure function (the streaming disc edge
// ramp) + the render.fog config key semantics (code default true, the
// fire.particles getBool precedent — config.json is untouched).

#include <gtest/gtest.h>

#include <string>

#include "core/config.hpp"
#include "renderer/fog.hpp"

using mc::Config;
using mc::FogRange;
using mc::Json;
using mc::kFogStartScale;
using mc::fogDistanceRange;

// Shipped config (view_distance 80): min(80, 64) = 64 columns = 1024
// blocks; the far plane (1000) is INSIDE the disc, so end = 1000 and
// start = 0.75 * 1000 = 750.
TEST(FogRangeTest, ShippedVd80Cap) {
    const FogRange r = fogDistanceRange(1024.0f, 1000.0f);
    EXPECT_FLOAT_EQ(r.end, 1000.0f);
    EXPECT_FLOAT_EQ(r.start, 750.0f);
}

// view_distance 8: min(8, 64) = 8 columns = 128 blocks < far plane.
TEST(FogRangeTest, Vd8) {
    const FogRange r = fogDistanceRange(128.0f, 1000.0f);
    EXPECT_FLOAT_EQ(r.end, 128.0f);
    EXPECT_FLOAT_EQ(r.start, 96.0f);
}

// view_distance 16: 256 blocks < far plane.
TEST(FogRangeTest, Vd16) {
    const FogRange r = fogDistanceRange(256.0f, 1000.0f);
    EXPECT_FLOAT_EQ(r.end, 256.0f);
    EXPECT_FLOAT_EQ(r.start, 192.0f);
}

// A mid radius inside the far plane.
TEST(FogRangeTest, MidRadius) {
    const FogRange r = fogDistanceRange(500.0f, 1000.0f);
    EXPECT_FLOAT_EQ(r.end, 500.0f);
    EXPECT_FLOAT_EQ(r.start, 375.0f);
}

// start is 0.75 * end BIT-for-bit: 0.75 is exact in binary, and the test
// recomputes the identical IEEE multiply the function performs.
TEST(FogRangeTest, StartIsExactlyThreeQuartersOfEnd) {
    for (float radius : {1.0f, 33.0f, 123.45f, 500.0f, 1024.0f, 16384.0f}) {
        const FogRange r = fogDistanceRange(radius, 1000.0f);
        EXPECT_TRUE(r.start == kFogStartScale * r.end) << "radius " << radius;
    }
}

// end never exceeds min(radius, far): a radius beyond the far plane is
// cut off by the far plane; a radius inside it is kept.
TEST(FogRangeTest, EndAtMostMinOfRadiusAndFar) {
    const FogRange r = fogDistanceRange(2048.0f, 1000.0f);
    EXPECT_FLOAT_EQ(r.end, 1000.0f);
    const FogRange r2 = fogDistanceRange(128.0f, 1000.0f);
    EXPECT_FLOAT_EQ(r2.end, 128.0f);
}

// far < radius: the far plane wins.
TEST(FogRangeTest, FarPlaneSmallerThanRadius) {
    const FogRange r = fogDistanceRange(2000.0f, 500.0f);
    EXPECT_FLOAT_EQ(r.end, 500.0f);
    EXPECT_FLOAT_EQ(r.start, 375.0f);
}

// Both bounds are monotone non-decreasing in the radius (saturating at
// the far plane once the disc exceeds it).
TEST(FogRangeTest, MonotonicInRadius) {
    float prevEnd = 0.0f;
    float prevStart = 0.0f;
    for (float radius : {64.0f, 128.0f, 256.0f, 512.0f, 1024.0f, 4096.0f}) {
        const FogRange r = fogDistanceRange(radius, 1000.0f);
        EXPECT_GE(r.end, prevEnd);
        EXPECT_GE(r.start, prevStart);
        prevEnd = r.end;
        prevStart = r.start;
    }
}

// (0, far) -> (0, 0): a degenerate range the caller turns off (end >
// start is false). A negative radius (an out-of-range config value) is
// degenerate the same way — the fog must never enable without a real
// disc edge.
TEST(FogRangeTest, ZeroAndNegativeRadiusDegenerate) {
    const FogRange r = fogDistanceRange(0.0f, 1000.0f);
    EXPECT_FLOAT_EQ(r.end, 0.0f);
    EXPECT_FLOAT_EQ(r.start, 0.0f);
    EXPECT_FALSE(r.end > r.start);
    const FogRange r2 = fogDistanceRange(-128.0f, 1000.0f);
    EXPECT_FALSE(r2.end > r2.start);
}

// render.fog: the key is absent from config.json by design — the code
// default (true, the fire.particles / mobs.limb_swing precedent) applies.
TEST(FogConfigTest, MissingKeyDefaultsToOn) {
    Config cfg;
    EXPECT_TRUE(cfg.getBool("render.fog", true));
}

// An explicit false turns the fog off (the A/B "off" control).
TEST(FogConfigTest, ExplicitFalseTurnsOff) {
    Config cfg;
    cfg.set("render.fog", Json(false));
    EXPECT_FALSE(cfg.getBool("render.fog", true));
}
