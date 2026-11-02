#include "entity/mob_mesh.hpp"

#include "block/cube_face_table.hpp"

namespace mc {

namespace {

// V2-2: the face-name vocabulary of the per-face tile overrides, keyed by
// the shared table's OUTWARD normal (the yaw-0 = -Z convention: the -Z
// face is the mob's front). kCubeFaces' normals are exactly these six
// float literals (the component comparisons are therefore exact).
const char* faceNameFromNormal(const Vec3& n) {
    if (n.x == 0.0f && n.y == 0.0f && n.z == -1.0f) {
        return "front";
    }
    if (n.x == 0.0f && n.y == 0.0f && n.z == 1.0f) {
        return "back";
    }
    if (n.x == -1.0f && n.y == 0.0f && n.z == 0.0f) {
        return "left";
    }
    if (n.x == 1.0f && n.y == 0.0f && n.z == 0.0f) {
        return "right";
    }
    if (n.x == 0.0f && n.y == 1.0f && n.z == 0.0f) {
        return "top";
    }
    if (n.x == 0.0f && n.y == -1.0f && n.z == 0.0f) {
        return "bottom";
    }
    return nullptr; // unreachable for the shared table's six faces
}

} // namespace

ItemMeshGeometry makeMobModelGeometry(const MobModel& model,
                                      const AtlasLayout& layout) {
    ItemMeshGeometry out;
    out.vertices.reserve(model.boxes.size() * 24);
    out.indices.reserve(model.boxes.size() * 36);
    for (const MobBox& box : model.boxes) {
        for (const CubeFace& face : kCubeFaces) {
            // V2-2: the per-face tile override (an empty box.faces = the
            // pre-V2-2 single-tile behavior: every face uses box.texture,
            // bit-identical UVs — the face-major vertex/index order below
            // is the pre-V2-2 order, corner order and winding untouched).
            const char* fn = faceNameFromNormal(face.normal);
            const auto ov =
                (fn != nullptr) ? box.faces.find(fn) : box.faces.end();
            const std::string& tile =
                (ov != box.faces.end()) ? ov->second : box.texture;
            const Vec2 u0 = AtlasLayout::uvMin(layout, tile);
            const Vec2 u1 = AtlasLayout::uvMax(layout, tile);
            // Corner UVs follow the corner order: (u0,v1),(u1,v1),(u1,v0),(u0,v0)
            // — the chunk-mesher-verified tile mapping (see item_mesh.cpp).
            const Vec2 faceUv[4] = {
                {u0.x, u1.y}, {u1.x, u1.y}, {u1.x, u0.y}, {u0.x, u0.y},
            };
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
