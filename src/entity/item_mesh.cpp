#include "entity/item_mesh.hpp"

#include "block/cube_face_table.hpp"
#include "entity/item_entity.hpp"

namespace mc {

ItemMeshGeometry makeItemCubeGeometry(const Block& block,
                                      const AtlasLayout& layout) {
    ItemMeshGeometry out;
    if (block.model.shape == BlockModel::Shape::Cross) {
        // Cross plants (Phase 45): the icon is the same two crossing quads
        // as the world cross pass (block_mesh), centered on the item cube —
        // no cube faces exist for a cross block. Full sky light, like the
        // cube path.
        const Vec2 u0 = AtlasLayout::uvMin(layout, block.model.textures.cross);
        const Vec2 u1 = AtlasLayout::uvMax(layout, block.model.textures.cross);
        const float h = ItemEntity::kSize * 0.5f;
        const Vec2 faceUv[4] = {
            {u0.x, u1.y}, {u1.x, u1.y}, {u1.x, u0.y}, {u0.x, u0.y},
        };
        const float corners[2][4][3] = {
            {{-h, -h, -h}, {h, -h, h}, {h, h, h}, {-h, h, -h}},
            {{h, -h, -h}, {-h, -h, h}, {-h, h, h}, {h, h, -h}},
        };
        out.vertices.reserve(8);
        out.indices.reserve(12);
        for (const auto& quad : corners) {
            const std::uint32_t first =
                static_cast<std::uint32_t>(out.vertices.size());
            for (int i = 0; i < 4; ++i) {
                MeshVertex v;
                v.position = {quad[i][0], quad[i][1], quad[i][2]};
                v.normal = {0.0f, 1.0f, 0.0f}; // full-brightness face shade
                v.uv = faceUv[i];
                v.skyLight = 1.0f;
                v.blockLight = 0.0f;
                out.vertices.push_back(v);
            }
            out.indices.insert(out.indices.end(),
                               {first, first + 1, first + 2, first,
                                first + 2, first + 3});
        }
        return out;
    }
    out.vertices.reserve(24);
    out.indices.reserve(36);
    // Fence / glass pane (Phase 98/99): the world geometry is a thin
    // structure carried by the single 'side' texture (top/bottom are
    // absent by design — the init cross-check enforces exactly that).
    // The drop icon cube uses the side texture on every face, the Phase 65
    // cross-particle rule for shapes without cube faces (cubeFaceTexture
    // would return the empty top/bottom name and the atlas lookup would
    // throw).
    const bool thinSideOnly =
        block.model.shape == BlockModel::Shape::Fence ||
        block.model.shape == BlockModel::Shape::GlassPane;
    for (const CubeFace& face : kCubeFaces) {
        const std::string& tex =
            thinSideOnly ? block.model.textures.side
                         : cubeFaceTexture(block, face.textureSlot);
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
