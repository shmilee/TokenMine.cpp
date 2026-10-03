#include "entity/item_mesh.hpp"

#include "block/cube_face_table.hpp"
#include "entity/item_entity.hpp"

namespace mc {

ItemMeshGeometry makeItemCubeGeometry(const Block& block,
                                      const AtlasLayout& layout) {
    ItemMeshGeometry out;
    out.vertices.reserve(24);
    out.indices.reserve(36);
    for (const CubeFace& face : kCubeFaces) {
        const std::string& tex = cubeFaceTexture(block, face.textureSlot);
        const Vec2 u0 = AtlasLayout::uvMin(layout, tex);
        const Vec2 u1 = AtlasLayout::uvMax(layout, tex);
        // Corner UVs follow the corner order: (u0,v1),(u1,v1),(u1,v0),(u0,v0)
        // — the chunk-mesher-verified tile mapping.
        const Vec2 faceUv[4] = {
            {u0.x, u1.y}, {u1.x, u1.y}, {u1.x, u0.y}, {u0.x, u0.y},
        };
        const std::uint32_t first =
            static_cast<std::uint32_t>(out.vertices.size());
        for (int i = 0; i < 4; ++i) {
            MeshVertex v;
            // Corner 0/1 offsets become a centered cube: (c - 0.5) * size.
            v.position = {
                (static_cast<float>(face.corner[i][0]) - 0.5f) *
                    ItemEntity::kSize,
                (static_cast<float>(face.corner[i][1]) - 0.5f) *
                    ItemEntity::kSize,
                (static_cast<float>(face.corner[i][2]) - 0.5f) *
                    ItemEntity::kSize};
            v.normal = face.normal;
            v.uv = faceUv[i];
            // Full sky light (0..1): the shader applies the day/night scale
            // to it, so a dropped item is bright by day and dark by night
            // like the world around it.
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

} // namespace mc
