// DayCycle: in-game time of day, the day/night sky-light scale (dayFactor),
// the interpolated sky color, the sun/moon directions, and the deterministic
// starfield.
#include <gtest/gtest.h>

#include <cstddef>

#include "core/math/vec3.hpp"
#include "world/day_cycle.hpp"

using mc::DayCycle;

namespace {

// A cycle that starts at a given time of day (no update applied).
DayCycle atTime(double t, double length = 240.0) {
    DayCycle::Params p;
    p.dayLengthSeconds = length;
    p.startTimeOfDay = t;
    return DayCycle(p);
}

} // namespace

TEST(DayCycleTest, StartAtNoonByDefault) {
    DayCycle c; // default: 240 s day, starts at noon
    EXPECT_DOUBLE_EQ(0.25, c.timeOfDay());
    EXPECT_FLOAT_EQ(1.0f, c.dayFactor());
    EXPECT_FLOAT_EQ(0.0f, c.starAlpha());
}

TEST(DayCycleTest, UpdateAdvancesTime) {
    DayCycle c = atTime(0.0);
    for (int i = 0; i < 120; ++i) {
        c.update(1.0); // 120 s = half of the 240 s day
    }
    EXPECT_NEAR(0.5, c.timeOfDay(), 1e-12);
}

TEST(DayCycleTest, DayLengthScalesSpeed) {
    DayCycle c = atTime(0.0, 120.0); // a 120 s day
    c.update(30.0);                  // a quarter of it
    EXPECT_NEAR(0.25, c.timeOfDay(), 1e-12);
}

TEST(DayCycleTest, TimeWrapsAround) {
    DayCycle c = atTime(0.9, 1.0);
    c.update(0.5); // 0.9 + 0.5 = 1.4 -> 0.4
    EXPECT_NEAR(0.4, c.timeOfDay(), 1e-12);
}

TEST(DayCycleTest, ZeroDayLengthIsClamped) {
    DayCycle::Params p;
    p.dayLengthSeconds = 0.0;
    DayCycle c(p);
    c.update(10.0);
    // Must not produce NaN/infinite time: clamped to a 1 s day.
    EXPECT_NO_FATAL_FAILURE(c.timeOfDay());
    EXPECT_GE(c.timeOfDay(), 0.0);
    EXPECT_LT(c.timeOfDay(), 1.0);
}

TEST(DayCycleTest, DayFactorCurve) {
    // Night: 0 before dawn, 0 after dusk.
    EXPECT_FLOAT_EQ(0.0f, atTime(0.0).dayFactor());
    EXPECT_FLOAT_EQ(0.0f, atTime(0.04).dayFactor());
    EXPECT_FLOAT_EQ(0.0f, atTime(0.75).dayFactor());
    EXPECT_FLOAT_EQ(0.0f, atTime(0.99).dayFactor());
    // Full day on the plateau.
    EXPECT_FLOAT_EQ(1.0f, atTime(0.25).dayFactor());
    EXPECT_FLOAT_EQ(1.0f, atTime(0.3).dayFactor());
    // Ramp edges and midpoints (smoothstep(0.5) = 0.5).
    EXPECT_FLOAT_EQ(0.0f, atTime(DayCycle::kDawnStart).dayFactor());
    EXPECT_FLOAT_EQ(0.5f, atTime(0.10).dayFactor());
    EXPECT_FLOAT_EQ(1.0f, atTime(DayCycle::kDawnEnd).dayFactor());
    EXPECT_FLOAT_EQ(1.0f, atTime(DayCycle::kDuskStart).dayFactor());
    EXPECT_FLOAT_EQ(0.5f, atTime(0.50).dayFactor());
    EXPECT_FLOAT_EQ(0.0f, atTime(DayCycle::kDuskEnd).dayFactor());
}

TEST(DayCycleTest, DayFactorMonotonicOnRamps) {
    // Dawn ramp: strictly increasing.
    const float dawn[5] = {atTime(0.05).dayFactor(),
                           atTime(0.07).dayFactor(),
                           atTime(0.09).dayFactor(),
                           atTime(0.11).dayFactor(),
                           atTime(0.13).dayFactor()};
    for (int i = 1; i < 5; ++i) {
        EXPECT_LT(dawn[i - 1], dawn[i]) << "dawn ramp at index " << i;
    }
    // Dusk ramp: strictly decreasing.
    const float dusk[5] = {atTime(0.47).dayFactor(),
                           atTime(0.49).dayFactor(),
                           atTime(0.51).dayFactor(),
                           atTime(0.53).dayFactor(),
                           atTime(0.55).dayFactor()};
    for (int i = 1; i < 5; ++i) {
        EXPECT_GT(dusk[i - 1], dusk[i]) << "dusk ramp at index " << i;
    }
}

TEST(DayCycleTest, StarAlphaComplementsDayFactor) {
    EXPECT_FLOAT_EQ(0.0f, atTime(0.25).starAlpha());
    EXPECT_FLOAT_EQ(1.0f, atTime(0.75).starAlpha());
    EXPECT_FLOAT_EQ(0.5f, atTime(0.10).starAlpha());
    EXPECT_FLOAT_EQ(0.5f, atTime(0.50).starAlpha());
}

TEST(DayCycleTest, SunDirection) {
    // East horizon at t=0, zenith at noon, west at t=0.5, nadir at t=0.75.
    const mc::Vec3 e = atTime(0.0).sunDirection();
    EXPECT_NEAR(1.0f, e.x, 1e-6f);
    EXPECT_NEAR(0.0f, e.y, 1e-6f);
    EXPECT_NEAR(0.0f, e.z, 1e-6f);

    const mc::Vec3 n = atTime(0.25).sunDirection();
    EXPECT_NEAR(0.0f, n.x, 1e-6f);
    EXPECT_NEAR(1.0f, n.y, 1e-6f);
    EXPECT_NEAR(0.0f, n.z, 1e-6f);

    const mc::Vec3 w = atTime(0.5).sunDirection();
    EXPECT_NEAR(-1.0f, w.x, 1e-6f);
    EXPECT_NEAR(0.0f, w.y, 1e-6f);
    EXPECT_NEAR(0.0f, w.z, 1e-6f);

    const mc::Vec3 d = atTime(0.75).sunDirection();
    EXPECT_NEAR(0.0f, d.x, 1e-6f);
    EXPECT_NEAR(-1.0f, d.y, 1e-6f);
    EXPECT_NEAR(0.0f, d.z, 1e-6f);
}

TEST(DayCycleTest, SunDirectionIsUnit) {
    for (double t = 0.0; t < 1.0; t += 0.07) {
        const mc::Vec3 s = atTime(t).sunDirection();
        EXPECT_NEAR(1.0f, mc::length(s), 1e-5f) << "t=" << t;
    }
}

TEST(DayCycleTest, MoonIsOppositeTheSun) {
    for (double t : {0.1, 0.3, 0.55, 0.9}) {
        const mc::Vec3 s = atTime(t).sunDirection();
        const mc::Vec3 m = atTime(t).moonDirection();
        EXPECT_NEAR(s.x, -m.x, 1e-6f) << "t=" << t;
        EXPECT_NEAR(s.y, -m.y, 1e-6f) << "t=" << t;
        EXPECT_NEAR(s.z, -m.z, 1e-6f) << "t=" << t;
    }
}

TEST(DayCycleTest, SkyColorKeyframes) {
    // Noon: the classic day blue (the Phase 1-7 sky color).
    const mc::Vec3 day = atTime(0.25).skyColor();
    EXPECT_NEAR(0.47f, day.x, 1e-6f);
    EXPECT_NEAR(0.71f, day.y, 1e-6f);
    EXPECT_NEAR(0.95f, day.z, 1e-6f);

    // Midnight: deep night blue.
    const mc::Vec3 night = atTime(0.75).skyColor();
    EXPECT_NEAR(0.02f, night.x, 1e-6f);
    EXPECT_NEAR(0.03f, night.y, 1e-6f);
    EXPECT_NEAR(0.08f, night.z, 1e-6f);

    // Mid-dawn / mid-dusk: the orange keyframes.
    const mc::Vec3 dawn = atTime(0.10).skyColor();
    EXPECT_NEAR(0.85f, dawn.x, 1e-6f);
    EXPECT_NEAR(0.45f, dawn.y, 1e-6f);
    EXPECT_NEAR(0.25f, dawn.z, 1e-6f);

    const mc::Vec3 dusk = atTime(0.50).skyColor();
    EXPECT_NEAR(0.85f, dusk.x, 1e-6f);
    EXPECT_NEAR(0.40f, dusk.y, 1e-6f);
    EXPECT_NEAR(0.20f, dusk.z, 1e-6f);
}

TEST(DayCycleTest, SkyColorInterpolatesBetweenKeyframes) {
    // Midway between the dawn orange (t=0.10) and the day blue (t=0.15).
    const mc::Vec3 c = atTime(0.125).skyColor();
    EXPECT_NEAR(0.5f * (0.85f + 0.47f), c.x, 1e-5f);
    EXPECT_NEAR(0.5f * (0.45f + 0.71f), c.y, 1e-5f);
    EXPECT_NEAR(0.5f * (0.25f + 0.95f), c.z, 1e-5f);
}

TEST(DayCycleTest, SkyColorContinuousAtWrap) {
    // The t -> 1 and t -> 0 limits must be the same deep-night color.
    const mc::Vec3 night {0.02f, 0.03f, 0.08f};
    const mc::Vec3 end = atTime(0.9999).skyColor();
    const mc::Vec3 start = atTime(0.0).skyColor();
    EXPECT_NEAR(end.x, start.x, 1e-6f);
    EXPECT_NEAR(end.y, start.y, 1e-6f);
    EXPECT_NEAR(end.z, start.z, 1e-6f);
    for (const mc::Vec3 c : {end, start}) {
        EXPECT_NEAR(night.x, c.x, 1e-6f);
        EXPECT_NEAR(night.y, c.y, 1e-6f);
        EXPECT_NEAR(night.z, c.z, 1e-6f);
    }
}

TEST(DayCycleTest, StarPositionsAreDeterministic) {
    const auto a = DayCycle::makeStarPositions(1234, 100, 800.0);
    const auto b = DayCycle::makeStarPositions(1234, 100, 800.0);
    ASSERT_EQ(a.size(), b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        EXPECT_EQ(a[i], b[i]) << "star " << i;
    }
    // A different seed produces a different field.
    const auto c = DayCycle::makeStarPositions(4321, 100, 800.0);
    bool anyDifferent = false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i] != c[i]) {
            anyDifferent = true;
            break;
        }
    }
    EXPECT_TRUE(anyDifferent);
}

TEST(DayCycleTest, StarPositionsOnUpperHemisphere) {
    const auto stars = DayCycle::makeStarPositions(7, 500, 800.0);
    ASSERT_EQ(500u, stars.size());
    for (std::size_t i = 0; i < stars.size(); ++i) {
        const mc::Vec3& p = stars[i];
        // On the sphere of the requested radius (float conversion slack).
        EXPECT_NEAR(800.0f, mc::length(p), 0.05f) << "star " << i;
        // Above the horizon margin.
        EXPECT_GE(p.y, DayCycle::kStarHorizonMargin * 800.0f - 0.05f)
            << "star " << i;
    }
}

TEST(DayCycleTest, FixedStepUpdatesAreReproducible) {
    DayCycle a;
    DayCycle b;
    for (int i = 0; i < 1000; ++i) {
        a.update(1.0 / 60.0);
        b.update(1.0 / 60.0);
    }
    EXPECT_DOUBLE_EQ(a.timeOfDay(), b.timeOfDay());
    EXPECT_FLOAT_EQ(a.dayFactor(), b.dayFactor());
}
