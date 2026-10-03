#include <gtest/gtest.h>

#include <SDL3/SDL.h>

#include <cmath>

#include "core/camera.hpp"
#include "core/input.hpp"
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

TEST(CameraTest, UpdateMovesForwardWithW) {
    Camera cam;
    cam.reset();
    cam.moveSpeed = 5.0f;

    Input input;
    input.frameBegin();
    input.handleKey(SDLK_W, true);

    cam.update(1.0, input); // 1 second of forward motion at 5 u/s
    EXPECT_TRUE(vecNear(cam.position, Vec3{0, 3, 3}, 1e-3f));
}

TEST(CameraTest, UpdateMovesRightWithD) {
    Camera cam;
    cam.reset();
    cam.moveSpeed = 10.0f;

    Input input;
    input.frameBegin();
    input.handleKey(SDLK_D, true);

    cam.update(0.5, input); // 0.5s at 10 u/s = 5 units to the right
    EXPECT_TRUE(vecNear(cam.position, Vec3{5, 3, 8}, 1e-3f));
}

TEST(CameraTest, MouseLook) {
    Camera cam;
    cam.reset();
    cam.mouseSensitivity = 0.01f;

    Input input;
    input.frameBegin();
    // Mouse 100px right, 50px up.
    input.handleMouseMotion(100.0f, -50.0f);

    cam.update(0.016, input);
    EXPECT_NEAR(cam.yaw, -1.0f, 1e-4f);  // right -> yaw decreases
    EXPECT_NEAR(cam.pitch, 0.5f, 1e-4f); // up -> pitch increases
}

TEST(CameraTest, PitchIsClamped) {
    Camera cam;
    cam.reset();
    cam.mouseSensitivity = 1.0f; // huge sensitivity to force the clamp

    Input input;
    input.frameBegin();
    input.handleMouseMotion(0.0f, -1000.0f); // way more than 90 deg up
    cam.update(0.016, input);
    EXPECT_LT(cam.pitch, 1.6f);
    EXPECT_GT(cam.pitch, 1.5f);

    input.frameBegin();
    input.handleMouseMotion(0.0f, 1000.0f); // and far below
    cam.update(0.016, input);
    EXPECT_GT(cam.pitch, -1.6f);
    EXPECT_LT(cam.pitch, -1.5f);
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

TEST(CameraTest, SprintDoublesSpeed) {
    Camera cam;
    cam.reset();
    cam.moveSpeed = 5.0f;
    cam.sprintMultiplier = 2.0f;

    Input input;
    input.frameBegin();
    input.handleKey(SDLK_W, true);
    input.handleKey(SDLK_LSHIFT, true);

    cam.update(1.0, input);
    EXPECT_TRUE(vecNear(cam.position, Vec3{0, 3, -2}, 1e-3f)); // 10 u/s
}
