// V2-6 first-person arm: the visibility gate (the cameraMode matrix —
// the arm draws ONLY in first person + pointer locked + no panel, the
// held-hand's existing gate) and the camera-frame pose (static, but
// following the held item's offset).
#include <gtest/gtest.h>

#include "core/first_person_arm.hpp"

using namespace mc;

// -- the gate (the cameraMode matrix) -------------------------------------

TEST(FirstPersonArmTest, GateMatrix) {
    using M = Game::CameraMode;
    // mode x pointer-locked x panel-open: ONLY (First, locked, no panel)
    // is visible. ThirdBack / ThirdFront hide the arm (the world-space
    // body is visible instead — the V2-1 gate, unchanged).
    EXPECT_TRUE(firstPersonArmVisible(M::First, true, false));
    EXPECT_FALSE(firstPersonArmVisible(M::First, false, false));
    EXPECT_FALSE(firstPersonArmVisible(M::First, true, true));
    EXPECT_FALSE(firstPersonArmVisible(M::First, false, true));
    EXPECT_FALSE(firstPersonArmVisible(M::ThirdBack, true, false));
    EXPECT_FALSE(firstPersonArmVisible(M::ThirdBack, true, true));
    EXPECT_FALSE(firstPersonArmVisible(M::ThirdFront, true, false));
    EXPECT_FALSE(firstPersonArmVisible(M::ThirdFront, true, true));
    // A no-input run is never locked: the (locked, *, *) rows above with
    // locked = false all stay hidden — the byte-identical baseline's
    // structural-immunity root.
    EXPECT_FALSE(firstPersonArmVisible(M::First, false, false));
    EXPECT_FALSE(firstPersonArmVisible(M::ThirdBack, false, false));
    EXPECT_FALSE(firstPersonArmVisible(M::ThirdFront, false, false));
}

// -- the pose ---------------------------------------------------------------

TEST(FirstPersonArmTest, ArmCenterIsHandOffsetPlusDelta) {
    // Identity camera: the model's translation column IS the arm's
    // camera-frame center = the hand offset + the fixed delta (right of,
    // below and behind the hand).
    const Mat4 cam = Mat4::identity();
    const Vec3 hand {0.42f, -0.38f, -0.85f}; // kHandOffset's value
    const Mat4 m = Mat4::multiply(cam, firstPersonArmLocal(hand));
    EXPECT_FLOAT_EQ(hand.x + kArmCenterDeltaX, m.at(0, 3));
    EXPECT_FLOAT_EQ(hand.y + kArmCenterDeltaY, m.at(1, 3));
    EXPECT_FLOAT_EQ(hand.z + kArmCenterDeltaZ, m.at(2, 3));
}

TEST(FirstPersonArmTest, PoseIsStaticAndDeterministic) {
    // No frame / state input: the pose is a function of the hand offset
    // ALONE (the static-pose deviation — no walking swing): same inputs,
    // bit-identical matrices.
    const Vec3 a {0.42f, -0.38f, -0.85f};
    const Vec3 b {0.42f, -0.38f, -0.85f};
    EXPECT_EQ(firstPersonArmLocal(a), firstPersonArmLocal(b));
}

TEST(FirstPersonArmTest, PoseFollowsTheHandOffset) {
    // The bow-draw / eat-lift channels move the hand offset; the arm
    // center moves by exactly the same delta (the "the hand carries the
    // arm" semantics).
    const Vec3 rest {0.42f, -0.38f, -0.85f};
    const Vec3 drawn = rest + Vec3 {0.0f, 0.0f, 0.3f}; // a bow pull
    const Mat4 mr = firstPersonArmLocal(rest);
    const Mat4 md = firstPersonArmLocal(drawn);
    EXPECT_NEAR(mr.at(0, 3), md.at(0, 3), 1e-6f);
    EXPECT_NEAR(mr.at(1, 3), md.at(1, 3), 1e-6f);
    EXPECT_NEAR(mr.at(2, 3) + 0.3f, md.at(2, 3), 1e-6f);
}

TEST(FirstPersonArmTest, ArmCarriesWithTheCamera) {
    // A yawed camera (yaw 0 = facing -Z, the camera convention): the
    // arm's world position = the camera rotation applied to the
    // camera-frame center (the held-hand's transform convention — the
    // arm sits in the camera frame, so it follows every camera move).
    const float yaw = 0.7f;
    const Mat4 cam = Mat4::rotateY(yaw);
    const Vec3 hand {0.42f, -0.38f, -0.85f};
    const Mat4 m = Mat4::multiply(cam, firstPersonArmLocal(hand));
    const Vec3 center = hand + Vec3 {kArmCenterDeltaX, kArmCenterDeltaY,
                                     kArmCenterDeltaZ};
    const Vec3 expected = cam.transformDirection(center);
    EXPECT_NEAR(expected.x, m.at(0, 3), 1e-5f);
    EXPECT_NEAR(expected.y, m.at(1, 3), 1e-5f);
    EXPECT_NEAR(expected.z, m.at(2, 3), 1e-5f);
}
