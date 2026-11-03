// V2-6 pickup magnet: the pure frame-locked pull of a READY drop (or a
// STUCK arrow) toward the player — the "fixed 1.0 block radius, no
// magnet" documented deviation consumed. Pinned: the radial speed curve
// (zero at the edge, monotonically accelerating toward the player), the
// arrival behavior (no direction at zero horizontal distance — no NaN),
// the swirl (periodic in the frame, zero at frame 0) and the horizontal
// (y = 0) discipline.
#include <gtest/gtest.h>

#include "entity/pickup_magnet.hpp"

using namespace mc;

namespace {

// The game's fixed frame step (the kMobStepPerFrame discipline).
constexpr double kH = 1.0 / 60.0;

} // namespace

TEST(PickupMagnetTest, RadialSpeedZeroBeyondRadius) {
    EXPECT_FLOAT_EQ(0.0f, pickupMagnetRadialSpeed(kMagnetRadius));
    EXPECT_FLOAT_EQ(0.0f, pickupMagnetRadialSpeed(kMagnetRadius + 0.01f));
    EXPECT_FLOAT_EQ(0.0f, pickupMagnetRadialSpeed(3.0f));
    EXPECT_FLOAT_EQ(0.0f, pickupMagnetRadialSpeed(100.0f));
}

TEST(PickupMagnetTest, RadialSpeedPins) {
    // speed = kMagnetMaxSpeed * (1 - d/r)^2, pinned at named marks:
    EXPECT_FLOAT_EQ(kMagnetMaxSpeed, pickupMagnetRadialSpeed(0.0f));
    EXPECT_FLOAT_EQ(kMagnetMaxSpeed * 0.64f, pickupMagnetRadialSpeed(0.5f));
    EXPECT_FLOAT_EQ(kMagnetMaxSpeed * 0.36f, pickupMagnetRadialSpeed(1.0f));
    EXPECT_FLOAT_EQ(kMagnetMaxSpeed * 0.25f, pickupMagnetRadialSpeed(1.25f));
    EXPECT_FLOAT_EQ(kMagnetMaxSpeed * 0.04f, pickupMagnetRadialSpeed(2.0f));
    // Total function: negative input clamps to the player (no NaN).
    EXPECT_FLOAT_EQ(kMagnetMaxSpeed, pickupMagnetRadialSpeed(-1.0f));
}

TEST(PickupMagnetTest, RadialSpeedMonotoneApproach) {
    // Closing in: strictly faster the closer the item is (the
    // accelerating approach, the MC feel).
    const float dists[7] = {2.4f, 2.2f, 2.0f, 1.5f, 1.0f, 0.5f, 0.1f};
    for (int i = 1; i < 7; ++i) {
        EXPECT_GT(pickupMagnetRadialSpeed(dists[i]),
                  pickupMagnetRadialSpeed(dists[i - 1]))
            << "closing from " << dists[i - 1] << " to " << dists[i];
    }
}

TEST(PickupMagnetTest, DisplacementIsPureRadialAtFrameZero) {
    // sin(0) = 0: the swirl is absent at frame 0 — the step is EXACTLY
    // the radial direction scaled by speed * h (the frame-locked class:
    // one fixed displacement per game frame).
    const Vec3 item {10.0f, 5.0f, 20.0f};
    const Vec3 player {12.0f, 5.0f, 20.0f}; // 2.0 apart on x
    const Vec3 d = pickupMagnetDisplacement(item, player, 0, kH);
    EXPECT_FLOAT_EQ(0.0f, d.y); // horizontal only
    const float step = static_cast<float>(kH);
    EXPECT_FLOAT_EQ(pickupMagnetRadialSpeed(2.0f) * step, d.x);
    EXPECT_FLOAT_EQ(0.0f, d.z);
}

TEST(PickupMagnetTest, DisplacementPointsTowardPlayer) {
    const Vec3 item {-2.0f, 5.0f, 3.0f};
    const Vec3 player {0.0f, 5.0f, 2.0f}; // sqrt(5) ~ 2.24 apart (in range)
    const Vec3 d = pickupMagnetDisplacement(item, player, 1, kH);
    EXPECT_GT(dot(d, player - item), 0.0f); // a step toward the player
    EXPECT_FLOAT_EQ(0.0f, d.y);
}

TEST(PickupMagnetTest, SwirlAtPeakFrame) {
    // At frame = period/4 the sin is +1: the sideways component is
    // +amplitude * speed * h along the 90°-about-Y direction of the
    // radial (radial +x here -> swirl +z).
    const int frame = kMagnetSwirlPeriodFrames / 4;
    const Vec3 item {0.0f, 5.0f, 0.0f};
    const Vec3 player {1.0f, 5.0f, 0.0f};
    const Vec3 d = pickupMagnetDisplacement(item, player, frame, kH);
    const float step = static_cast<float>(kH);
    const float speed = pickupMagnetRadialSpeed(1.0f);
    EXPECT_FLOAT_EQ(speed * step, d.x);
    EXPECT_FLOAT_EQ(kMagnetSwirlAmplitude * speed * step, d.z);
    EXPECT_FLOAT_EQ(0.0f, d.y);
}

TEST(PickupMagnetTest, DisplacementPeriodicInFrame) {
    // The frame-bounded swirl (the modulo discipline): the displacement
    // repeats EXACTLY every period — frame N and frame N + period are
    // bit-identical (a long session stays float-exact).
    const Vec3 item {0.0f, 5.0f, 0.0f};
    const Vec3 player {1.5f, 5.0f, 0.0f};
    for (int base : {0, 1, 5, 13}) {
        const Vec3 a = pickupMagnetDisplacement(item, player, base, kH);
        const Vec3 b = pickupMagnetDisplacement(item, player,
                                                base + kMagnetSwirlPeriodFrames,
                                                kH);
        EXPECT_EQ(a, b) << "base=" << base;
    }
}

TEST(PickupMagnetTest, DisplacementZeroAtArrival) {
    // Horizontal coincidence (the item at the player's feet, possibly at
    // another height): no direction, no NaN — the zero displacement is
    // EXACT (the pickup pass collects it).
    const Vec3 item {3.0f, 5.0f, 3.0f};
    const Vec3 d1 = pickupMagnetDisplacement(item, item, 7, kH);
    EXPECT_EQ(Vec3::zero(), d1);
    const Vec3 d2 =
        pickupMagnetDisplacement(item, {3.0f, 10.0f, 3.0f}, 7, kH);
    EXPECT_EQ(Vec3::zero(), d2); // vertical offset only: no horizontal pull
}

TEST(PickupMagnetTest, DisplacementZeroBeyondRadius) {
    const Vec3 item {0.0f, 5.0f, 0.0f};
    const Vec3 atEdge {kMagnetRadius, 5.0f, 0.0f};
    EXPECT_EQ(Vec3::zero(), pickupMagnetDisplacement(item, atEdge, 3, kH));
    const Vec3 far {kMagnetRadius + 1.0f, 5.0f, 0.0f};
    EXPECT_EQ(Vec3::zero(), pickupMagnetDisplacement(item, far, 3, kH));
}
