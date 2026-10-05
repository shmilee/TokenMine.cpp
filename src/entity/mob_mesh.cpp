#include "entity/mob_mesh.hpp"

#include "block/cube_face_table.hpp"

namespace mc {

ItemMeshGeometry makeMobModelGeometry(const MobModel& model,
                                      const AtlasLayout& layout) {
    ItemMeshGeometry out;
    out.vertices.reserve(model.boxes.size() * 24);
    out.indices.reserve(model.boxes.size() * 36);
    for (const MobBox& box : model.boxes) {
        const Vec2 u0 = AtlasLayout::uvMin(layout, box.texture);
        const Vec2 u1 = AtlasLayout::uvMax(layout, box.texture);
        // Corner UVs follow the corner order: (u0,v1),(u1,v1),(u1,v0),(u0,v0)
        // — the chunk-mesher-verified tile mapping (see item_mesh.cpp).
        const Vec2 faceUv[4] = {
            {u0.x, u1.y}, {u1.x, u1.y}, {u1.x, u0.y}, {u0.x, u0.y},
        };
        for (const CubeFace& face : kCubeFaces) {
            const std::uint32_t first =
                static_cast<std::uint32_t>(out.vertices.size());
            for (int i = 0; i < 4; ++i) {
                MeshVertex v;
                // Box corner = center + (corner - 0.5) * size (the 0/1
                // corner offsets of the shared table, scaled to the box).
                v.position = {
                    box.center.x +
                        (static_cast<float>(face.corner[i][0]) - 0.5f) *
                            box.size.x,
                    box.center.y +
                        (static_cast<float>(face.corner[i][1]) - 0.5f) *
                            box.size.y,
                    box.center.z +
                        (static_cast<float>(face.corner[i][2]) - 0.5f) *
                            box.size.z};
                v.normal = face.normal;
                v.uv = faceUv[i];
                // Full sky light (0..1), baked (see the file head).
                v.skyLight = 1.0f;
                v.blockLight = 0.0f;
                out.vertices.push_back(v);
            }
            for (int i = 0; i < 6; ++i) {
                static constexpr std::uint32_t kQuad[6] = {0, 1, 2, 0, 2, 3};
                out.indices.push_back(first + kQuad[i]);
            }
        }
    }
    return out;
}

} // namespace mc
