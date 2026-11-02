// UX-4 (2.0.0: the lifted orbit): the third-person camera math
// (core/third_person).
//
// The offset math pins the Minecraft third-person convention (behind
// the orbit point, on the view's own sphere, one trig source with
// Camera::forward); the orbit-center math pins the 2.0.0 lift (the eye
// kThirdPersonOrbitHeight up the view's up axis — the head stays clear
// of the screen-center crosshair); the resolve pins the occlusion
// clamps (wall face, the embedded-ideal edge, fluids transparent,
// beyond reach) with exact expected positions. V2-1: the front view is
// the SAME resolve at the flipped angles (yaw + pi, -pitch) — the
// frontAngles pin, the up-invariance, the empty-world identity (the
// camera in front of the player, mirrored about the orbit center) and
// the mirrored wall clamp.

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

// -- 2.0.0: the lifted orbit center ----------------------------------------------

TEST(ThirdPersonTest, OrbitCenterLiftPins) {
    // pitch 0 / yaw 0: up_cam = +Y EXACTLY (sin 0 = 0, cos 0 = 1 are
    // exact in binary), so the orbit center is bit-exactly the eye
    // kThirdPersonOrbitHeight up.
    const Vec3 orbit0 = thirdPersonOrbitCenter(kEye, 0.0f, 0.0f);
    const Vec3 want0 {22.5f, 40.9f, 0.5f};
    EXPECT_EQ(orbit0, want0);

    // Non-axis: the analytic form of up_cam = cross(right, forward)
    // (see Camera::up) is (sin yaw * sin pitch, cos pitch,
    // sin pitch * cos yaw), so the orbit center is the eye + that,
    // scaled by kThirdPersonOrbitHeight.
    const float cases[][2] = {
        {0.0f, 0.5f},    {kPiOver2, 0.5f}, {0.7f, -0.3f},
        {-2.1f, 0.9f},   {3.0f, -1.2f},    {kPiOver2, 0.0f},
    };
    for (const auto& c : cases) {
        const float yaw = c[0];
        const float pitch = c[1];
        const Vec3 orbit = thirdPersonOrbitCenter(kEye, yaw, pitch);
        expectVecNear("orbit", orbit,
                      kEye.x + kThirdPersonOrbitHeight * std::sin(yaw) *
                          std::sin(pitch),
                      kEye.y + kThirdPersonOrbitHeight * std::cos(pitch),
                      kEye.z + kThirdPersonOrbitHeight * std::sin(pitch) *
                          std::cos(yaw),
                      1e-6f);
        // The lift has the exact magnitude at every orientation
        // (1e-5: recovering it by subtracting the ~40-magnitude eye
        // costs a few ulps at that scale).
        EXPECT_NEAR(length(orbit - kEye), kThirdPersonOrbitHeight, 1e-5f)
            << "yaw " << yaw;
    }
}

// -- occlusion resolve ---------------------------------------------------------

TEST(ThirdPersonTest, ResolveEmptyWorldIsIdeal) {
    World world; // no chunks: every read is air
    // 2.0.0: the yaw-0/pitch-0 orbit center is exactly the eye +
    // (0, 0.4, 0) (up_cam = +Y), so the ideal is exactly the orbit +
    // (0, 0, 4): the unoccluded path returns it BIT-exactly.
    const Vec3 orbit = thirdPersonOrbitCenter(kEye, 0.0f, 0.0f);
    const Vec3 ideal {22.5f, 40.9f, 4.5f};
    const Vec3 cam = resolveThirdPersonCamera(world, orbit, 0.0f, 0.0f);
    EXPECT_EQ(cam, ideal);
    // A non-axis orientation too (same function, so bit-exact here).
    const Vec3 orbit2 = thirdPersonOrbitCenter(kEye, 0.4f, -0.2f);
    const Vec3 cam2 = resolveThirdPersonCamera(world, orbit2, 0.4f, -0.2f);
    const Vec3 ideal2 = orbit2 +
        thirdPersonCameraOffset(0.4f, -0.2f, kThirdPersonDistance);
    EXPECT_EQ(cam2, ideal2);
}

TEST(ThirdPersonTest, ResolveClampsInFrontOfWall) {
    World world;
    // Eye at kEye, yaw 0 / pitch 0 -> 2.0.0: the orbit center at
    // (22.5, 40.9, 0.5) (the eye 0.4 up, up_cam = +Y) and the ideal
    // camera at (22.5, 40.9, 4.5), looking -Z toward the orbit. Two
    // wall blocks at (22,40,1) and (22,40,2) (the ray's y = 40.9 is
    // still inside their y range [40, 41)): the first occluding face
    // is z = 3, at t = 1.5 from the ideal.
    ASSERT_TRUE(world.setBlock(22, 40, 1, BlockID{1}));
    ASSERT_TRUE(world.setBlock(22, 40, 2, BlockID{1}));
    const Vec3 orbit = thirdPersonOrbitCenter(kEye, 0.0f, 0.0f);
    const Vec3 cam = resolveThirdPersonCamera(world, orbit, 0.0f, 0.0f);
    // Stop kCameraOcclusionMargin in front of the face: 4.5 - (1.5 -
    // 0.1) = 3.1 (0.1 in front of the wall's z = 3 face).
    expectVecNear("clamp", cam, 22.5f, 40.9f, 3.1f);
}

TEST(ThirdPersonTest, ResolveEmbeddedIdealExitsTheFace) {
    // The acceptance scene: the ideal position is INSIDE the wall cell.
    // Eye at (22.5, 39.62, 0.5), yaw 0 / pitch 0 -> 2.0.0: the orbit
    // center at (22.5, 40.02, 0.5) (the eye 0.4 up, up_cam = +Y) and
    // the ideal at (22.5, 40.02, 4.5), embedded in the z = 4 wall cell
    // (the wall moved up with the ideal: (22,40,4), so the embedded
    // semantics are preserved), dir = (0, 0, -1). The DDA first step
    // out of that cell is t_exit = 0.5 (the z = 4 face). The face is
    // the orbit-SIDE boundary of the wall, so the camera exits it and
    // takes the margin OUTSIDE, on the player's side: z = 4.5 -
    // (0.5 + 0.1) = 3.9 — 0.1 in front of the wall's z = 4 face, in
    // the air (NOT z = 4.1: t_exit - margin would leave the camera
    // 0.1 inside the wall, the gate S2b bug).
    World world;
    const Vec3 orbit =
        thirdPersonOrbitCenter(Vec3{22.5f, 39.62f, 0.5f}, 0.0f, 0.0f);
    ASSERT_TRUE(world.setBlock(22, 40, 4, BlockID{1}));
    const Vec3 cam = resolveThirdPersonCamera(world, orbit, 0.0f, 0.0f);
    expectVecNear("embedded", cam, 22.5f, 40.02f, 3.9f);
}

TEST(ThirdPersonTest, ResolveIgnoresFluids) {
    // A water source between the ideal camera and the eye: the block
    // ray passes through fluids (raycastBlock semantics), so the
    // camera stays at the ideal position bit-exactly.
    World world;
    world.getOrCreateChunk(worldToChunk({22, 40, 3}));
    ASSERT_TRUE(world.setFluid(22, 40, 3, FluidKind::Water,
                               kMaxFluidLevel));
    const Vec3 orbit = thirdPersonOrbitCenter(kEye, 0.0f, 0.0f);
    const Vec3 cam = resolveThirdPersonCamera(world, orbit, 0.0f, 0.0f);
    const Vec3 ideal {22.5f, 40.9f, 4.5f};
    EXPECT_EQ(cam, ideal);
}

TEST(ThirdPersonTest, ResolveIgnoresWallBeyondReach) {
    // A wall past the orbit center (more than kThirdPersonDistance from
    // the ideal, at the z = 0 plane = t 4.5 > 4.0): it occludes nothing
    // on the camera->orbit segment.
    World world;
    ASSERT_TRUE(world.setBlock(22, 40, -1, BlockID{1}));
    const Vec3 orbit = thirdPersonOrbitCenter(kEye, 0.0f, 0.0f);
    const Vec3 cam = resolveThirdPersonCamera(world, orbit, 0.0f, 0.0f);
    const Vec3 ideal {22.5f, 40.9f, 4.5f};
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

TEST(ThirdPersonTest, UpCamIsOrthonormalToTheViewAxes) {
    // 2.0.0: up_cam = cross(right, forward) (Camera::up) is the view's
    // ACTUAL up — unit length, orthogonal to the view forward and the
    // view right, at every player-clamped orientation. This is what
    // makes the orbit lift "straight up in view space" for every
    // yaw/pitch (looking up or down never re-puts the head on the
    // crosshair).
    const float cases[][2] = {
        {0.0f, 0.0f},   {0.4f, -0.2f}, {1.2f, 0.5f},
        {-2.1f, 0.9f},  {3.0f, -1.2f}, {kPiOver2, 0.0f},
    };
    for (const auto& c : cases) {
        Camera cam {};
        cam.yaw = c[0];
        cam.pitch = c[1];
        const Vec3 up = cam.up();
        EXPECT_NEAR(length(up), 1.0f, 1e-6f) << "yaw " << c[0];
        EXPECT_NEAR(dot(up, cam.forward()), 0.0f, 1e-6f) << "yaw " << c[0];
        EXPECT_NEAR(dot(up, cam.right()), 0.0f, 1e-6f) << "yaw " << c[0];
        // Cross-check against the view matrix's OWN up row: lookAt
        // derives u = cross(s, f') with f' = normalized(forward) and
        // s = normalized(cross(f', unitY)); for the player's clamped
        // pitch, cross(f', unitY) = cos(pitch) * right() (a POSITIVE
        // scalar), so s = right() and u == up() (to within the
        // normalization's few ulps).
        const Mat4 view = cam.viewMatrix();
        EXPECT_NEAR(view.at(1, 0), up.x, 1e-6f) << "yaw " << c[0];
        EXPECT_NEAR(view.at(1, 1), up.y, 1e-6f) << "yaw " << c[0];
        EXPECT_NEAR(view.at(1, 2), up.z, 1e-6f) << "yaw " << c[0];
    }
}

// -- UX-8g: the aim rays leave from the eye in both modes ------------------------

TEST(ThirdPersonTest, EyeAimRayCoincidesWithTheViewAxis) {
    // UX-8g: the interaction raycasts (targetBlock / targetMob / the
    // bucket rays / the arrow origin) leave from the EYE along
    // m_camera.forward() in BOTH camera modes. In third person the
    // camera sits kThirdPersonDistance behind the (2.0.0: lifted)
    // orbit center; this test pins the geometry that keeps the body
    // out of the aim: the unoccluded resolved camera lies EXACTLY on
    // the screen-center line through the orbit center at t =
    // -kThirdPersonDistance (orbit + forward * t). The screen-center
    // line is the view's own forward — PARALLEL to the eye's aim ray,
    // offset by kThirdPersonOrbitHeight along up_cam: a fixed
    // third-person parallax (the Minecraft third-person look,
    // documented deviation — no code compensation). The full kReach
    // budget of the eye ray is still spent IN FRONT of the eye.
    // (targetBlock's origin itself is a Game method, not
    // unit-testable in isolation; this collinearity + the bit-exact
    // First assignment in game_frame are the two halves of the
    // first-person identity argument.)
    World world; // no chunks: the unoccluded resolve = the ideal orbit
    const float cases[][2] = {
        {0.0f, 0.0f},   {0.4f, -0.2f}, {1.2f, 0.5f},
        {-2.1f, 0.9f},  {3.0f, -1.2f}, {kPiOver2, 0.0f},
    };
    for (const auto& c : cases) {
        Camera view {};
        view.yaw = c[0];
        view.pitch = c[1];
        const Vec3 orbit = thirdPersonOrbitCenter(kEye, c[0], c[1]);
        const Vec3 cam = resolveThirdPersonCamera(world, orbit, c[0], c[1]);
        // The camera is on the screen-center line (through the lifted
        // orbit center) at t = -kThirdPersonDistance.
        const Vec3 onRay =
            orbit + view.forward() * (-kThirdPersonDistance);
        EXPECT_NEAR(cam.x, onRay.x, 1e-5f) << "yaw " << c[0];
        EXPECT_NEAR(cam.y, onRay.y, 1e-5f) << "yaw " << c[0];
        EXPECT_NEAR(cam.z, onRay.z, 1e-5f) << "yaw " << c[0];
    }
}

TEST(ThirdPersonTest, UnoccludedCameraPinsTheLiftedOrbitInvariant) {
    // 2.0.0 invariants for the unoccluded resolve, at every
    // orientation:
    //   (1) the resolved camera is BIT-exactly on the lifted orbit
    //       line (orbit + offset, the resolve's own arithmetic);
    //   (2) the lift vector identity: (eye - camera) =
    //       forward * kThirdPersonDistance - up_cam *
    //       kThirdPersonOrbitHeight — its component along up_cam is
    //       exactly -kThirdPersonOrbitHeight, so the head stays a
    //       fixed distance below the screen center at every
    //       yaw/pitch;
    //   (3) the screen-center ray (the resolved camera's own forward)
    //       is the eye aim ray's PARALLEL — the direction is
    //       bit-exactly equal (same trig expression), so the
    //       parallax is a fixed offset, never a direction change.
    World world; // no chunks: the unoccluded resolve = the ideal orbit
    const float cases[][2] = {
        {0.0f, 0.0f},   {0.4f, -0.2f}, {1.2f, 0.5f},
        {-2.1f, 0.9f},  {3.0f, -1.2f}, {kPiOver2, 0.0f},
    };
    for (const auto& c : cases) {
        Camera view {};
        view.yaw = c[0];
        view.pitch = c[1];
        const Vec3 orbit = thirdPersonOrbitCenter(kEye, c[0], c[1]);
        const Vec3 cam = resolveThirdPersonCamera(world, orbit, c[0], c[1]);
        // (1) bit-exact on the lifted orbit line.
        const Vec3 want = orbit +
            thirdPersonCameraOffset(c[0], c[1], kThirdPersonDistance);
        EXPECT_EQ(cam, want) << "yaw " << c[0];
        // (2) the lift vector identity, per view axis (1e-5: the
        //     segment is recovered by subtracting ~40-magnitude
        //     coordinates, costing a few ulps at that scale).
        const Vec3 seg = kEye - cam;
        EXPECT_NEAR(dot(seg, view.up()), -kThirdPersonOrbitHeight, 1e-5f)
            << "yaw " << c[0];
        EXPECT_NEAR(dot(seg, view.forward()), kThirdPersonDistance, 1e-5f)
            << "yaw " << c[0];
        EXPECT_NEAR(dot(seg, view.right()), 0.0f, 1e-5f) << "yaw " << c[0];
        // (3) the screen-center ray direction == the eye ray
        //     direction, bit-exactly.
        Camera resolved {};
        resolved.position = cam;
        resolved.yaw = c[0];
        resolved.pitch = c[1];
        EXPECT_EQ(resolved.forward(), view.forward()) << "yaw " << c[0];
    }
}

// -- V2-1: the front view (the flipped-angles resolve) -------------------------

TEST(ThirdPersonTest, FrontAnglesFlipPins) {
    // The flip, pinned by the SAME expression (bit-exact): yaw' = yaw +
    // kPi, pitch' = -pitch. Multiple orientations, including the axis
    // cases.
    const float cases[][2] = {
        {0.0f, 0.0f},   {0.4f, -0.2f}, {1.2f, 0.5f},
        {-2.1f, 0.9f},  {3.0f, -1.2f}, {kPiOver2, 0.0f},
    };
    for (const auto& c : cases) {
        const ThirdPersonAngles a = thirdPersonFrontAngles(c[0], c[1]);
        EXPECT_FLOAT_EQ(a.yaw, c[0] + kPi) << "yaw " << c[0];
        EXPECT_FLOAT_EQ(a.pitch, -c[1]) << "yaw " << c[0];
    }
}

TEST(ThirdPersonTest, FrontViewUpIsInvariant) {
    // up_cam(yaw', pitch') = up_cam(yaw, pitch): the flip negates BOTH
    // right and forward (cross(-r, -f) = cross(r, f)), so the view's up
    // axis is unchanged (to the sin(yaw + kPi) ulp — 1e-6, NOT
    // bit-exact). This is what lets the orbit center keep using the
    // PLAYER's angles: the same kThirdPersonOrbitHeight lift, the head
    // 29-96 px below the screen center as in the back view.
    const float cases[][2] = {
        {0.0f, 0.0f},   {0.4f, -0.2f}, {1.2f, 0.5f},
        {-2.1f, 0.9f},  {3.0f, -1.2f}, {kPiOver2, 0.0f},
    };
    for (const auto& c : cases) {
        Camera a {};
        a.yaw = c[0];
        a.pitch = c[1];
        Camera b {};
        const ThirdPersonAngles fa = thirdPersonFrontAngles(c[0], c[1]);
        b.yaw = fa.yaw;
        b.pitch = fa.pitch;
        const Vec3 ua = a.up();
        const Vec3 ub = b.up();
        EXPECT_NEAR(ua.x, ub.x, 1e-6f) << "yaw " << c[0];
        EXPECT_NEAR(ua.y, ub.y, 1e-6f) << "yaw " << c[0];
        EXPECT_NEAR(ua.z, ub.z, 1e-6f) << "yaw " << c[0];
    }
}

TEST(ThirdPersonTest, FrontViewEmptyWorldResolveIdentity) {
    // Empty world (no occlusion): the front-view resolve is the ideal
    // position, and the ideal is orbit + forward(yaw, pitch) * D — the
    // camera IN FRONT of the player (the player's front is visible).
    // (1) Bit-exact against the resolve's OWN unoccluded expression
    //     (orbit + thirdPersonCameraOffset(fa) — same operation order,
    //     self-consistent);
    // (2) the semantic identity against the player's forward at 1e-6
    //     (the sin(yaw + kPi) ulp — NOT bit-exact across the flip);
    // (3) the mirror relation to the back view at the SAME orbit
    //     center: front_cam - orbit = -(back_cam - orbit) (1e-5: kEye-
    //     scale coordinate rounding).
    World world; // no chunks: every read is air
    const float cases[][2] = {
        {0.0f, 0.0f},   {0.4f, -0.2f}, {1.2f, 0.5f},
        {-2.1f, 0.9f},  {3.0f, -1.2f}, {kPiOver2, 0.0f},
    };
    for (const auto& c : cases) {
        Camera view {};
        view.yaw = c[0];
        view.pitch = c[1];
        const ThirdPersonAngles fa = thirdPersonFrontAngles(c[0], c[1]);
        // The orbit center keeps the PLAYER's angles (the invariance
        // above) — the same point the back view orbits.
        const Vec3 orbit = thirdPersonOrbitCenter(kEye, c[0], c[1]);
        const Vec3 camFront =
            resolveThirdPersonCamera(world, orbit, fa.yaw, fa.pitch);
        const Vec3 camBack =
            resolveThirdPersonCamera(world, orbit, c[0], c[1]);
        // (1) bit-exact: the resolve's own unoccluded arithmetic.
        const Vec3 want =
            orbit + thirdPersonCameraOffset(fa.yaw, fa.pitch,
                                            kThirdPersonDistance);
        EXPECT_EQ(camFront, want) << "yaw " << c[0];
        // (2) the semantic identity: in front of the eye along the
        //     player's forward (1e-5: the sin(yaw + kPi) ulp plus the
        //     ~21-magnitude coordinate rounding — the file's convention
        //     for values recovered at kEye scale; NOT bit-exact across
        //     the flip).
        expectVecNear("front ideal", camFront,
                      orbit.x + view.forward().x * kThirdPersonDistance,
                      orbit.y + view.forward().y * kThirdPersonDistance,
                      orbit.z + view.forward().z * kThirdPersonDistance,
                      1e-5f);
        // (3) the mirror relation about the orbit center (1e-5: the
        //     difference of ~21-magnitude coordinates costs a ulp at
        //     that scale — 1.9e-6 observed at yaw 0.4).
        EXPECT_NEAR(camFront.x - orbit.x, -(camBack.x - orbit.x), 1e-5f)
            << "yaw " << c[0];
        EXPECT_NEAR(camFront.y - orbit.y, -(camBack.y - orbit.y), 1e-5f)
            << "yaw " << c[0];
        EXPECT_NEAR(camFront.z - orbit.z, -(camBack.z - orbit.z), 1e-5f)
            << "yaw " << c[0];
    }
}

TEST(ThirdPersonTest, FrontViewClampsInFrontOfWall) {
    // The back-view wall clamp (ResolveClampsInFrontOfWall) mirrored to
    // the +f side: eye at kEye, yaw 0 (facing -Z) / pitch 0 -> the orbit
    // center at (22.5, 40.9, 0.5) and the FRONT ideal camera at
    // (22.5, 40.9, -3.5), looking +Z toward the orbit. Two wall blocks
    // IN FRONT of the player at (22,40,-1) and (22,40,-2) (the ray's
    // y = 40.9 is still inside their y range [40, 41)): the first
    // occluding face is z = -2, at t = 1.5 from the ideal.
    World world;
    ASSERT_TRUE(world.setBlock(22, 40, -1, BlockID{1}));
    ASSERT_TRUE(world.setBlock(22, 40, -2, BlockID{1}));
    const Vec3 orbit = thirdPersonOrbitCenter(kEye, 0.0f, 0.0f);
    const ThirdPersonAngles fa = thirdPersonFrontAngles(0.0f, 0.0f);
    const Vec3 cam =
        resolveThirdPersonCamera(world, orbit, fa.yaw, fa.pitch);
    // Stop kCameraOcclusionMargin in front of the face, on the
    // PLAYER's side (z > -2): -3.5 + (1.5 - 0.1) = -2.1 (the exact
    // mirror of the back view's 3.1 about the orbit's z = 0.5).
    expectVecNear("front clamp", cam, 22.5f, 40.9f, -2.1f);
}
