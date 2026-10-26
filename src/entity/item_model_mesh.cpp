#include "entity/item_model_mesh.hpp"

namespace mc {

namespace {
// The corner UVs for one face, in the kCubeFaces corner order — the
// item-cube mapping (chunk-mesher-verified): corner 0 -> (u0, v1),
// corner 1 -> (u1, v1), corner 2 -> (u1, v0), corner 3 -> (u0, v0).
// With the texture's v = 0 at the icon's TOP row (the hand-icon
// convention), the sampled sub-rectangle appears UPRIGHT on every
// outside-facing side face.
constexpr int kFaceUvCorner[4][2] = {
    {0, 1}, // corner 0: (u0, v1)
    {1, 1}, // corner 1: (u1, v1)
    {1, 0}, // corner 2: (u1, v0)
    {0, 0}, // corner 3: (u0, v0)
};
} // namespace

ItemMeshGeometry makeItemModelGeometry(const Item::ItemModel& model,
                                       bool cutout) {
    ItemMeshGeometry out;
    out.vertices.reserve(model.elements.size() * 24);
    out.indices.reserve(model.elements.size() * 36);
    for (const Item::ModelElement& el : model.elements) {
        const float uv[2][2] = {{el.uv0.x, el.uv0.y}, {el.uv1.x, el.uv1.y}};
        for (const CubeFace& face : kCubeFaces) {
            const std::uint32_t first =
                static_cast<std::uint32_t>(out.vertices.size());
            for (int i = 0; i < 4; ++i) {
                MeshVertex v;
                // Corner offset 0/1 -> the element's from/to span, then
                // center the box on the origin (the unit-cube model
                // space, the icon-quad hand convention).
                v.position = {
                    el.from.x +
                        face.corner[i][0] * (el.to.x - el.from.x) - 0.5f,
                    el.from.y +
                        face.corner[i][1] * (el.to.y - el.from.y) - 0.5f,
                    el.from.z +
                        face.corner[i][2] * (el.to.z - el.from.z) - 0.5f};
                v.normal = face.normal;
                v.uv = {uv[kFaceUvCorner[i][0]][0], uv[kFaceUvCorner[i][1]][1]};
                // Full sky light (0..1): the shader applies the day/night
                // scale to it, so a model item is bright by day and dark
                // by night like the world around it (the block-cube-drop
                // convention — the block-cube hand's lighting).
                v.skyLight = 1.0f;
                v.blockLight = 0.0f;
                // PRE-5: the cutout identity mark (location 8) — set on
                // every vertex of a pure-cutout icon's model (the
                // caller's census); 0.0 (the default) otherwise.
                v.cutout = cutout ? 1.0f : 0.0f;
                out.vertices.push_back(v);
            }
            out.indices.insert(out.indices.end(),
                               {first, first + 1, first + 2, first,
                                first + 2, first + 3});
        }
    }
    return out;
}

} // namespace mc
