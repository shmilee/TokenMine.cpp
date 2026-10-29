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

// -- geometry (PRE-9: the three-part MC-style arrow) --------------------------

TEST(ArrowGeometryTest, PartVertexCounts) {
    // Shaft + fletching = boxes (24 vertices / 36 indices each, the
    // kCubeFaces table); tip = a 4-sided pyramid (12 vertices / 12
    // indices, four triangles).
    const ArrowGeometry g = makeArrowGeometry();
    EXPECT_EQ(g.shaft.vertices.size(), 24u);
    EXPECT_EQ(g.shaft.indices.size(), 36u);
    EXPECT_EQ(g.tip.vertices.size(), 12u);
    EXPECT_EQ(g.tip.indices.size(), 12u);
    EXPECT_EQ(g.fletching.vertices.size(), 24u);
    EXPECT_EQ(g.fletching.indices.size(), 36u);
    for (const std::uint32_t i : g.shaft.indices) {
        EXPECT_LT(i, 24u);
    }
    for (const std::uint32_t i : g.tip.indices) {
        EXPECT_LT(i, 12u);
    }
    for (const std::uint32_t i : g.fletching.indices) {
        EXPECT_LT(i, 24u);
    }
}

TEST(ArrowGeometryTest, PartsTileTheTotalLength) {
    // The three parts tile the whole arrow (tip front + shaft middle +
    // fletch back) to exactly kArrowLength (the pre-PRE-9 box length —
    // the overall silhouette size is unchanged).
    EXPECT_NEAR(kArrowTipLength + kArrowShaftLength + kArrowFletchLength,
                kArrowLength, 1e-6f);
}

TEST(ArrowGeometryTest, WholeArrowSpansLengthCenteredOnOrigin) {
    // Every part lies in z in [-kArrowLength/2, +kArrowLength/2], and both
    // ends are reached: the tip apex at the front (-L/2), the fletching at
    // the back (+L/2).
    const ArrowGeometry g = makeArrowGeometry();
    float zmin = 1e9f;
    float zmax = -1e9f;
    for (const auto& m : {g.shaft, g.tip, g.fletching}) {
        for (const MeshVertex& v : m.vertices) {
            zmin = std::min(zmin, v.position.z);
            zmax = std::max(zmax, v.position.z);
        }
    }
    EXPECT_NEAR(zmin, -kArrowLength * 0.5f, kEps);
    EXPECT_NEAR(zmax, kArrowLength * 0.5f, kEps);
}

TEST(ArrowGeometryTest, TipIsPointedPyramidAtFront) {
    // The tip: a single apex at the arrow's front (z = -L/2, duplicated
    // once per face = 4 vertices) and a square base at the shaft's front
    // end (z = -ShaftLength/2, 4 corners x 2 faces each = 8 vertices).
    const ArrowGeometry g = makeArrowGeometry();
    int apex = 0;
    int base = 0;
    for (const MeshVertex& v : g.tip.vertices) {
        if (std::abs(v.position.z + kArrowLength * 0.5f) < kEps) {
            ++apex;
        }
        if (std::abs(v.position.z + kArrowShaftLength * 0.5f) < kEps) {
            ++base;
        }
    }
    EXPECT_EQ(apex, 4);
    EXPECT_EQ(base, 8);
    // The apex is on-axis (x = y = 0): a point, not a flat face.
    for (const MeshVertex& v : g.tip.vertices) {
        if (std::abs(v.position.z + kArrowLength * 0.5f) < kEps) {
            EXPECT_NEAR(v.position.x, 0.0f, kEps);
            EXPECT_NEAR(v.position.y, 0.0f, kEps);
        }
    }
}

TEST(ArrowGeometryTest, FletchingWiderThanShaft) {
    // The fletching (the feathers) is wider than the shaft.
    const ArrowGeometry g = makeArrowGeometry();
    float shaftX = 0.0f;
    float fletchX = 0.0f;
    for (const MeshVertex& v : g.shaft.vertices) {
        shaftX = std::max(shaftX, std::abs(v.position.x));
    }
    for (const MeshVertex& v : g.fletching.vertices) {
        fletchX = std::max(fletchX, std::abs(v.position.x));
    }
    EXPECT_NEAR(shaftX, kArrowThickness * 0.5f, kEps);
    EXPECT_NEAR(fletchX, kArrowFletchWidth * 0.5f, kEps);
    EXPECT_GT(fletchX, shaftX);
}

TEST(ArrowGeometryTest, WindingIsOutwardOnEveryPart) {
    // Shaft + fletching are axis-aligned boxes (kCubeFaces): each face's
    // winding normal is a unit axis pointing AWAY FROM THE BOX'S OWN
    // CENTER (the fletching is z-offset from the origin, so "away from the
    // origin" would be wrong — the check is against the box center). The
    // tip is a pyramid: each of its 4 faces' winding normal points AWAY
    // from the arrow's axis (dot with the face centroid's (x,y) > 0).
    const ArrowGeometry g = makeArrowGeometry();
    for (const auto& m : {g.shaft, g.fletching}) {
        float xmin = 1e9f, xmax = -1e9f, ymin = 1e9f, ymax = -1e9f;
        float zmin = 1e9f, zmax = -1e9f;
        for (const MeshVertex& v : m.vertices) {
            xmin = std::min(xmin, v.position.x);
            xmax = std::max(xmax, v.position.x);
            ymin = std::min(ymin, v.position.y);
            ymax = std::max(ymax, v.position.y);
            zmin = std::min(zmin, v.position.z);
            zmax = std::max(zmax, v.position.z);
        }
        const Vec3 cen {(xmin + xmax) * 0.5f, (ymin + ymax) * 0.5f,
                        (zmin + zmax) * 0.5f};
        for (std::size_t tri = 0; tri < m.indices.size(); tri += 3) {
            const Vec3& a = m.vertices[m.indices[tri + 0]].position;
            const Vec3& b = m.vertices[m.indices[tri + 1]].position;
            const Vec3& c = m.vertices[m.indices[tri + 2]].position;
            const Vec3 n = normalized(cross(b - a, c - a));
            const float ax = std::abs(n.x);
            const float ay = std::abs(n.y);
            const float az = std::abs(n.z);
            const int axis = (ax >= ay && ax >= az) ? 0 : (ay >= az) ? 1 : 2;
            const float major = (axis == 0) ? ax : (axis == 1) ? ay : az;
            EXPECT_NEAR(major, 1.0f, 1e-4f);
            // Outward: the normal points away from the box center.
            const Vec3 rel = a - cen;
            const float s = (axis == 0) ? n.x : (axis == 1) ? n.y : n.z;
            const float p = (axis == 0) ? rel.x : (axis == 1) ? rel.y : rel.z;
            EXPECT_GT(s * p, 0.0f) << "box face " << tri;
        }
    }
    for (int face = 0; face < 4; ++face) {
        const Vec3& a = g.tip.vertices[g.tip.indices[face * 3 + 0]].position;
        const Vec3& b = g.tip.vertices[g.tip.indices[face * 3 + 1]].position;
        const Vec3& c = g.tip.vertices[g.tip.indices[face * 3 + 2]].position;
        const Vec3 n = cross(b - a, c - a);
        const Vec3 cen = (a + b + c) * (1.0f / 3.0f);
        EXPECT_GT(n.x * cen.x + n.y * cen.y, 0.0f) << "tip face " << face;
    }
}

TEST(ArrowGeometryTest, FullSkyLightNoBlockLight) {
    // Full sky light baked on every part (like dropped items / mob
    // models): the shader applies the day/night scale.
    const ArrowGeometry g = makeArrowGeometry();
    for (const auto& m : {g.shaft, g.tip, g.fletching}) {
        for (const MeshVertex& v : m.vertices) {
            EXPECT_FLOAT_EQ(v.skyLight, 1.0f);
            EXPECT_FLOAT_EQ(v.blockLight, 0.0f);
        }
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
