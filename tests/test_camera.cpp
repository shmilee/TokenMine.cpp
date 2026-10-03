#include <gtest/gtest.h>

#include <cmath>

#include "core/camera.hpp"
#include "core/math/vec3.hpp"

using namespace mc;

namespace {
constexpr float kPi = 3.14159265358979f;

bool near(float a, float b, float eps = 1e-4f) {
    return std::fabs(a - b) < eps;
}
bool vecNear(const Vec3& a, const Vec3& b, float eps = 1e-4f) {
    return near(a.x, b.x, eps) && near(a.y, b.y, eps) && near(a.z, b.z, eps);
}
} // namespace

// Note: the Phase 1 free-fly behaviors (WASD movement, mouse look, pitch
// clamp, sprint) were moved to the Player in Phase 5 and are covered by
// tests/test_player.cpp.

TEST(CameraTest, DefaultOrientation) {
    Camera cam;
    cam.reset();
    EXPECT_TRUE(vecNear(cam.forward(), Vec3{0, 0, -1}));
    EXPECT_TRUE(vecNear(cam.right(), Vec3{1, 0, 0}));
    EXPECT_TRUE(vecNear(cam.position, Vec3{0, 3, 8}));
}

TEST(CameraTest, YawTurnsLeftPositive) {
    Camera cam;
    cam.reset();
    cam.yaw = kPi / 2.0f; // turned left 90 deg
    EXPECT_TRUE(vecNear(cam.forward(), Vec3{-1, 0, 0}));
    EXPECT_TRUE(vecNear(cam.right(), Vec3{0, 0, -1}));
}

TEST(CameraTest, PitchLooksUpPositive) {
    Camera cam;
    cam.reset();
    cam.pitch = kPi / 3.0f; // 60 deg up
    const Vec3 f = cam.forward();
    EXPECT_NEAR(f.y, std::sin(kPi / 3.0f), 1e-4f);
    EXPECT_NEAR(f.z, -std::cos(kPi / 3.0f), 1e-4f);
    EXPECT_NEAR(f.x, 0.0f, 1e-4f);
    EXPECT_NEAR(length(f), 1.0f, 1e-4f); // stays normalized
}

TEST(CameraTest, ViewMatrixMapsForwardToMinusZ) {
    Camera cam;
    cam.reset();
    cam.yaw = 0.4f;
    cam.pitch = -0.2f;

    const Mat4 view = cam.viewMatrix();
    // A point 1 unit in front of the camera must land at (0, 0, -1).
    const Vec3 ahead = cam.position + cam.forward();
    EXPECT_TRUE(vecNear(view.transformPoint(ahead), Vec3{0, 0, -1}, 1e-3f));
    // The camera position itself maps to the origin.
    EXPECT_TRUE(vecNear(view.transformPoint(cam.position), Vec3::zero(), 1e-3f));
}
