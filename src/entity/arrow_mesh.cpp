#include "entity/arrow_mesh.hpp"

#include <cmath>

#include "block/cube_face_table.hpp"

namespace mc {

ItemMeshGeometry makeArrowGeometry() {
    ItemMeshGeometry out;
    out.vertices.reserve(24);
    out.indices.reserve(36);
    for (const CubeFace& face : kCubeFaces) {
        const std::uint32_t first =
            static_cast<std::uint32_t>(out.vertices.size());
        for (int i = 0; i < 4; ++i) {
            MeshVertex v;
            // Corner 0/1 offsets become a centered, anisotropic box:
            // (c - 0.5) * size, with the long axis on z (the travel
            // axis: local -Z is the direction of flight).
            v.position = {
                (static_cast<float>(face.corner[i][0]) - 0.5f) *
                    kArrowThickness,
                (static_cast<float>(face.corner[i][1]) - 0.5f) *
                    kArrowThickness,
                (static_cast<float>(face.corner[i][2]) - 0.5f) *
                    kArrowLength};
            v.normal = face.normal;
            v.uv = {0.0f, 0.0f}; // unused: the solid-color path (u_useTexture 0)
            // Full sky light (0..1): the shader applies the day/night
            // scale (like dropped items and mob models).
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
