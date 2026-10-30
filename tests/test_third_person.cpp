// UX-4: the third-person back camera (core/third_person).
//
// The offset math pins the Minecraft third-person convention (behind the
// eye, on the view's own sphere, one trig source with Camera::forward);
// the resolve pins the occlusion clamps (wall face, the embedded-ideal
// edge, fluids transparent, beyond reach) with exact expected
// positions.

#include <gtest/gtest.h>

#include <cmath>

#include "core/camera.hpp"
#include "core/third_person.hpp"
#include "world/coordinates.hpp"
#include "world/world.hpp"

using namespace mc;

namespace {

// A fixed eye position (yaw 0 = facing -Z puts the ideal camera at +Z
// behind it, 4 blocks off). sin(0) = 0 and cos(0) = 1 are exact in
// binary, so the yaw-0/pitch-0 offset is EXACTLY (0, 0, 4): the axis
// cases below can assert bit-equality.
constexpr Vec3 kEye {22.5f, 40.5f, 0.5f};

// Float pi/2 (+90 deg). cosf of it is a ~4e-8 residual, far inside the
// 1e-5 tolerance of the axis pins.
constexpr float kPiOver2 = 1.57079637f;

void expectVecNear(const char* what, const Vec3& v, float x, float y, float z,
                   float eps = 1e-5f) {
    EXPECT_NEAR(v.x, x, eps) << what;
    EXPECT_NEAR(v.y, y, eps) << what;
    EXPECT_NEAR(v.z, z, eps) << what;
}

} // namespace

// -- offset math ---------------------------------------------------------------

TEST(ThirdPersonTest, OffsetAxisPins) {
    // yaw 0, pitch 0 -> facing -Z; behind = +Z at the radius.
    expectVecNear("yaw0/p0",
                  thirdPersonCameraOffset(0.0f, 0.0f, kThirdPersonDistance),
                  0.0f, 0.0f, 4.0f);
    // pitch +90 deg (looking straight up) -> the camera is straight
    // BELOW the eye.
    expectVecNear("pitch90",
                  thirdPersonCameraOffset(0.0f, kPiOver2,
                                          kThirdPersonDistance),
                  0.0f, -4.0f, 0.0f);
    // pitch -90 deg (looking straight down) -> the camera is straight
    // ABOVE the eye.
    expectVecNear("pitch-90",
                  thirdPersonCameraOffset(0.0f, -kPiOver2,
                                          kThirdPersonDistance),
                  0.0f, 4.0f, 0.0f);
    // yaw +90 deg (facing -X) -> behind = +X.
    expectVecNear("yaw90",
                  thirdPersonCameraOffset(kPiOver2, 0.0f,
                                          kThirdPersonDistance),
                  4.0f, 0.0f, 0.0f);
    // A custom radius scales the offset.
    expectVecNear("radius2", thirdPersonCameraOffset(0.0f, 0.0f, 2.0f),
                  0.0f, 0.0f, 2.0f);
}

TEST(ThirdPersonTest, OffsetIntermediateAnglesAndLength) {
    // Analytic closed form: offset = -forward * D with forward =
    // (sin yaw * -cos pitch, sin pitch, cos yaw * -cos pitch), so
    // offset = (sin yaw * cos pitch * D, -sin pitch * D,
    // cos yaw * cos pitch * D).
    const float yaw = 0.7f;
    const float pitch = -0.3f;
    const float cp = std::cos(pitch);
    const Vec3 v =
        thirdPersonCameraOffset(yaw, pitch, kThirdPersonDistance);
    expectVecNear("intermediate", v,
                  std::sin(yaw) * cp * kThirdPersonDistance,
                  -std::sin(pitch) * kThirdPersonDistance,
                  std::cos(yaw) * cp * kThirdPersonDistance);
    // The length is always exactly the radius (up to float rounding).
    EXPECT_NEAR(length(v), kThirdPersonDistance, 1e-5f);

    const float yaw2 = -2.1f;
    const float pitch2 = 0.9f;
    const Vec3 v2 =
        thirdPersonCameraOffset(yaw2, pitch2, kThirdPersonDistance);
    const float cp2 = std::cos(pitch2);
    expectVecNear("intermediate2", v2,
                  std::sin(yaw2) * cp2 * kThirdPersonDistance,
                  -std::sin(pitch2) * kThirdPersonDistance,
                  std::cos(yaw2) * cp2 * kThirdPersonDistance);
    EXPECT_NEAR(length(v2), kThirdPersonDistance, 1e-5f);
}

// -- occlusion resolve ---------------------------------------------------------

TEST(ThirdPersonTest, ResolveEmptyWorldIsIdeal) {
    World world; // no chunks: every read is air
    // The yaw-0/pitch-0 ideal is exactly the eye + (0, 0, 4): the
    // unoccluded path returns it BIT-exactly.
    const Vec3 ideal {22.5f, 40.5f, 4.5f};
    const Vec3 cam = resolveThirdPersonCamera(world, kEye, 0.0f, 0.0f);
    EXPECT_EQ(cam, ideal);
    // A non-axis orientation too (same function, so bit-exact here).
    const Vec3 cam2 = resolveThirdPersonCamera(world, kEye, 0.4f, -0.2f);
    const Vec3 ideal2 = kEye +
        thirdPersonCameraOffset(0.4f, -0.2f, kThirdPersonDistance);
    EXPECT_EQ(cam2, ideal2);
}

TEST(ThirdPersonTest, ResolveClampsInFrontOfWall) {
    World world;
    // Eye at kEye, yaw 0 / pitch 0 -> the ideal camera at
    // (22.5, 40.5, 4.5), looking -Z toward the eye. Two wall blocks at
    // (22,40,1) and (22,40,2): the first occluding face is z = 3, at
    // t = 1.5 from the ideal.
    ASSERT_TRUE(world.setBlock(22, 40, 1, BlockID{1}));
    ASSERT_TRUE(world.setBlock(22, 40, 2, BlockID{1}));
    const Vec3 cam = resolveThirdPersonCamera(world, kEye, 0.0f, 0.0f);
    // Stop kCameraOcclusionMargin in front of the face: 4.5 - (1.5 -
    // 0.1) = 3.1 (0.1 in front of the wall's z = 3 face).
    expectVecNear("clamp", cam, 22.5f, 40.5f, 3.1f);
}

TEST(ThirdPersonTest, ResolveEmbeddedIdealExitsTheFace) {
    // The acceptance scene: the ideal position is INSIDE the wall cell.
    // Eye at (22.5, 39.62, 0.5), yaw 0 / pitch 0 -> ideal at
    // (22.5, 39.62, 4.5), embedded in the z = 4 wall cell, dir =
    // (0, 0, -1). The DDA first step out of that cell is t_exit = 0.5
    // (the z = 4 face). The face is the eye-SIDE boundary of the wall,
    // so the camera exits it and takes the margin OUTSIDE, on the
    // player's side: z = 4.5 - (0.5 + 0.1) = 3.9 — 0.1 in front of the
    // wall's z = 4 face, in the air (NOT z = 4.1: t_exit - margin
    // would leave the camera 0.1 inside the wall, the gate S2b bug).
    World world;
    const Vec3 orbit {22.5f, 39.62f, 0.5f};
    ASSERT_TRUE(world.setBlock(22, 39, 4, BlockID{1}));
    const Vec3 cam = resolveThirdPersonCamera(world, orbit, 0.0f, 0.0f);
    expectVecNear("embedded", cam, 22.5f, 39.62f, 3.9f);
}

TEST(ThirdPersonTest, ResolveIgnoresFluids) {
    // A water source between the ideal camera and the eye: the block
    // ray passes through fluids (raycastBlock semantics), so the
    // camera stays at the ideal position bit-exactly.
    World world;
    world.getOrCreateChunk(worldToChunk({22, 40, 3}));
    ASSERT_TRUE(world.setFluid(22, 40, 3, FluidKind::Water,
                               kMaxFluidLevel));
    const Vec3 cam = resolveThirdPersonCamera(world, kEye, 0.0f, 0.0f);
    const Vec3 ideal {22.5f, 40.5f, 4.5f};
    EXPECT_EQ(cam, ideal);
}

TEST(ThirdPersonTest, ResolveIgnoresWallBeyondReach) {
    // A wall past the orbit center (more than kThirdPersonDistance from
    // the ideal, at the z = 0 plane = t 4.5 > 4.0): it occludes nothing
    // on the camera->eye segment.
    World world;
    ASSERT_TRUE(world.setBlock(22, 40, -1, BlockID{1}));
    const Vec3 cam = resolveThirdPersonCamera(world, kEye, 0.0f, 0.0f);
    const Vec3 ideal {22.5f, 40.5f, 4.5f};
    EXPECT_EQ(cam, ideal);
}

// -- consistency with the view's own trig source --------------------------------

TEST(ThirdPersonTest, OffsetPointsOppositeTheViewForward) {
    // The orbit offset is the unit forward negated (the view matrix and
    // the camera sphere share one trig source): a drift guard for the
    // case Camera::forward or thirdPersonCameraOffset ever changes.
    const float cases[][2] = {
        {0.0f, 0.0f},   {0.4f, -0.2f}, {1.2f, 0.5f},
        {-2.1f, 0.9f},  {3.0f, -1.2f}, {kPiOver2, 0.0f},
    };
    for (const auto& c : cases) {
        Camera cam {};
        cam.yaw = c[0];
        cam.pitch = c[1];
        const Vec3 off =
            thirdPersonCameraOffset(c[0], c[1], kThirdPersonDistance);
        const Vec3 dir = normalized(off);
        const Vec3 want = -cam.forward();
        EXPECT_NEAR(dir.x, want.x, 1e-4f) << "yaw " << c[0];
        EXPECT_NEAR(dir.y, want.y, 1e-4f) << "yaw " << c[0];
        EXPECT_NEAR(dir.z, want.z, 1e-4f) << "yaw " << c[0];
    }
}
