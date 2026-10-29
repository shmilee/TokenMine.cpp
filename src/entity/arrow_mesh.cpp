#include "entity/arrow_mesh.hpp"

#include <cmath>
#include <cstdint>

#include "block/cube_face_table.hpp"

namespace mc {
namespace {

// A centered box (the kCubeFaces shared table scaled anisotropically:
// x/y by xSize/ySize, z by zSize), translated so its center is at
// (0, 0, zCenter). 24 vertices / 36 indices.
ItemMeshGeometry makeBox(float xSize, float ySize, float zSize,
                         float zCenter) {
    ItemMeshGeometry out;
    out.vertices.reserve(24);
    out.indices.reserve(36);
    for (const CubeFace& face : kCubeFaces) {
        const std::uint32_t first =
            static_cast<std::uint32_t>(out.vertices.size());
        for (int i = 0; i < 4; ++i) {
            MeshVertex v;
            v.position = {
                (static_cast<float>(face.corner[i][0]) - 0.5f) * xSize,
                (static_cast<float>(face.corner[i][1]) - 0.5f) * ySize,
                (static_cast<float>(face.corner[i][2]) - 0.5f) * zSize +
                    zCenter};
            v.normal = face.normal;
            v.uv = {0.0f, 0.0f}; // unused: the solid-color path
            // Full sky light: the shader applies the day/night scale.
            v.skyLight = 1.0f;
            v.blockLight = 0.0f;
            out.vertices.push_back(v);
        }
        out.indices.insert(out.indices.end(),
                           {first, first + 1, first + 2, first, first + 2,
                            first + 3});
    }
    return out;
}

// A 4-sided pyramid (the pointed arrow tip): a square base of half-width
// `hw` at z = zBase, the apex at (0, 0, zApex) (zApex < zBase, the front
// / -Z). Four triangular side faces, each wound CCW from OUTSIDE (the
// normal (v1-v0)x(v2-v0) points outward — pinned by the unit test). 12
// vertices / 12 indices (the base is internal, omitted).
ItemMeshGeometry makePyramid(float hw, float zBase, float zApex) {
    ItemMeshGeometry out;
    out.vertices.reserve(12);
    out.indices.reserve(12);
    const Vec3 p[4] = {
        {-hw, -hw, zBase}, // 0: west, -y
        {hw, -hw, zBase},  // 1: east, -y
        {hw, hw, zBase},   // 2: east, +y
        {-hw, hw, zBase},  // 3: west, +y
    };
    const Vec3 apex {0.0f, 0.0f, zApex};
    // {v0, v1, v2 (outward-wound), the face normal}.
    const struct {
        int a, b, c;
        Vec3 normal;
    } faces[4] = {
        {1, 0, -1, {0.0f, -1.0f, 0.0f}},  // south (-Y)
        {2, 1, -1, {1.0f, 0.0f, 0.0f}},   // east (+X)
        {3, 2, -1, {0.0f, 1.0f, 0.0f}},   // north (+Y)
        {0, 3, -1, {-1.0f, 0.0f, 0.0f}},  // west (-X)
    };
    for (const auto& f : faces) {
        const std::uint32_t first =
            static_cast<std::uint32_t>(out.vertices.size());
        const Vec3 v[3] = {p[f.a], p[f.b], apex};
        for (int i = 0; i < 3; ++i) {
            MeshVertex m;
            m.position = v[i];
            m.normal = f.normal;
            m.uv = {0.0f, 0.0f}; // unused: the solid-color path
            m.skyLight = 1.0f;
            m.blockLight = 0.0f;
            out.vertices.push_back(m);
        }
        out.indices.insert(out.indices.end(), {first, first + 1, first + 2});
    }
    return out;
}

} // namespace

ArrowGeometry makeArrowGeometry() {
    ArrowGeometry out;
    // The brown shaft: the middle box, centered on the origin (z in
    // [-kArrowShaftLength/2, +kArrowShaftLength/2]).
    out.shaft = makeBox(kArrowThickness, kArrowThickness, kArrowShaftLength,
                        0.0f);
    // The gray tip: the pointed front (the base at the shaft's front end,
    // the apex at the arrow's front).
    out.tip = makePyramid(kArrowThickness * 0.5f, -kArrowShaftLength * 0.5f,
                          -kArrowLength * 0.5f);
    // The white fletching: the wider back box (centered at the back).
    out.fletching =
        makeBox(kArrowFletchWidth, kArrowFletchWidth, kArrowFletchLength,
                (kArrowShaftLength + kArrowFletchLength) * 0.5f);
    return out;
}

std::pair<float, float> arrowOrientation(Vec3 velocity) {
    const float h =
        std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);
    const float pitch = std::atan2(velocity.y, h);
    if (h < 1e-6f) {
        // Straight up/down: the yaw is unconstrained; pin it to 0 (the
        // IEEE signed-zero case atan2(-0, -0) == -pi would spin the arrow
        // around a vertical axis for no reason).
        return {0.0f, static_cast<float>(pitch)};
    }
    const float yaw = std::atan2(-velocity.x, -velocity.z);
    return {static_cast<float>(yaw), static_cast<float>(pitch)};
}

} // namespace mc
