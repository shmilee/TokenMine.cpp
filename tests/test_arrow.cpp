// Arrow geometry + orientation (Phase 21): the bow's projectile render.
#include <gtest/gtest.h>

#include <cmath>

#include "core/math/mat4.hpp"
#include "core/math/vec3.hpp"
#include "entity/arrow_mesh.hpp"

using namespace mc;

namespace {

constexpr float kEps = 1e-4f;

// The render model (rotateY(yaw) * rotateX(pitch)) must rotate local -Z
// onto the normalized velocity (the arrow's travel direction).
void expectAligns(const Vec3& v) {
    const auto [yaw, pitch] = arrowOrientation(v);
    const Mat4 m = Mat4::multiply(Mat4::rotateY(yaw), Mat4::rotateX(pitch));
    const Vec3 dir = m.transformDirection({0.0f, 0.0f, -1.0f});
    const Vec3 n = normalized(v);
    EXPECT_NEAR(dir.x, n.x, kEps)
        << "v = (" << v.x << ", " << v.y << ", " << v.z << ")";
    EXPECT_NEAR(dir.y, n.y, kEps)
        << "v = (" << v.x << ", " << v.y << ", " << v.z << ")";
    EXPECT_NEAR(dir.z, n.z, kEps)
        << "v = (" << v.x << ", " << v.y << ", " << v.z << ")";
}

} // namespace

// -- geometry ------------------------------------------------------------------

TEST(ArrowGeometryTest, SixFacesTwentyFourVertices) {
    const ItemMeshGeometry g = makeArrowGeometry();
    EXPECT_EQ(g.vertices.size(), 24u);
    EXPECT_EQ(g.indices.size(), 36u);
    for (const std::uint32_t i : g.indices) {
        EXPECT_LT(i, 24u);
    }
}

TEST(ArrowGeometryTest, BoxAlongZCenteredAtOrigin) {
    // The box is centered on the origin, the long axis on z (the travel
    // axis: local -Z is the direction of flight), thin on x/y.
    const ItemMeshGeometry g = makeArrowGeometry();
    for (const MeshVertex& v : g.vertices) {
        EXPECT_GE(v.position.z, -kArrowLength * 0.5f - kEps);
        EXPECT_LE(v.position.z, kArrowLength * 0.5f + kEps);
        EXPECT_GE(v.position.x, -kArrowThickness * 0.5f - kEps);
        EXPECT_LE(v.position.x, kArrowThickness * 0.5f + kEps);
        EXPECT_GE(v.position.y, -kArrowThickness * 0.5f - kEps);
        EXPECT_LE(v.position.y, kArrowThickness * 0.5f + kEps);
    }
    // Every extent is reached (the box is not smaller than its constants).
    float zmax = -1e9f;
    float xmax = -1e9f;
    for (const MeshVertex& v : g.vertices) {
        zmax = std::max(zmax, std::abs(v.position.z));
        xmax = std::max(xmax, std::abs(v.position.x));
    }
    EXPECT_NEAR(zmax, kArrowLength * 0.5f, kEps);
    EXPECT_NEAR(xmax, kArrowThickness * 0.5f, kEps);
}

TEST(ArrowGeometryTest, WindingIsCounterClockwiseFromOutside) {
    // The box is axis-aligned: each face's winding cross product must be
    // exactly one of the unit axes, pointing away from the box center.
    const ItemMeshGeometry g = makeArrowGeometry();
    for (int face = 0; face < 6; ++face) {
        const Vec3& a = g.vertices[g.indices[face * 6 + 0]].position;
        const Vec3& b = g.vertices[g.indices[face * 6 + 1]].position;
        const Vec3& c = g.vertices[g.indices[face * 6 + 2]].position;
        const Vec3 n = normalized(cross(b - a, c - a));
        const float ax = std::abs(n.x);
        const float ay = std::abs(n.y);
        const float az = std::abs(n.z);
        const int axis = (ax >= ay && ax >= az) ? 0 : (ay >= az) ? 1 : 2;
        const float major = (axis == 0) ? ax : (axis == 1) ? ay : az;
        EXPECT_NEAR(major, 1.0f, 1e-4f) << "face " << face;
        // Outward: the normal points away from the origin (the box center).
        const float s = (axis == 0) ? n.x : (axis == 1) ? n.y : n.z;
        const float p = (axis == 0) ? a.x : (axis == 1) ? a.y : a.z;
        EXPECT_GT(s * p, 0.0f) << "face " << face;
    }
}

TEST(ArrowGeometryTest, FullSkyLightNoBlockLight) {
    // Full sky light baked (like dropped items / mob models): the shader
    // applies the day/night scale.
    const ItemMeshGeometry g = makeArrowGeometry();
    for (const MeshVertex& v : g.vertices) {
        EXPECT_FLOAT_EQ(v.skyLight, 1.0f);
        EXPECT_FLOAT_EQ(v.blockLight, 0.0f);
    }
}

// -- orientation -----------------------------------------------------------------

TEST(ArrowOrientationTest, AlignsMinusZWithVelocity) {
    expectAligns({0.0f, 0.0f, -1.0f});   // straight ahead (-Z)
    expectAligns({1.0f, 0.0f, 0.0f});    // +X
    expectAligns({0.0f, 0.0f, 1.0f});    // backwards (+Z: yaw = pi)
    expectAligns({-0.7f, 0.0f, 0.7f});   // horizontal diagonal
    expectAligns({0.5f, 0.6f, -0.62f});  // diagonal up
    expectAligns({0.3f, -0.9f, -0.3f});  // steep down
}

TEST(ArrowOrientationTest, UnnormalizedDirection) {
    // The velocity is used as-is (no normalization needed).
    expectAligns({3.0f, 0.0f, -4.0f});
}

TEST(ArrowOrientationTest, StraightUpPinsYawToZero) {
    // Near-zero horizontal part: the yaw is unconstrained and pinned to 0
    // (the IEEE signed-zero case atan2(-0, -0) == -pi would spin the arrow
    // around a vertical axis for no reason); the pitch carries the whole
    // direction.
    const auto [yaw, pitch] = arrowOrientation({0.0f, 1.0f, 0.0f});
    EXPECT_NEAR(yaw, 0.0f, 1e-6f);
    EXPECT_NEAR(pitch, std::numbers::pi * 0.5f, 1e-6f);
}
