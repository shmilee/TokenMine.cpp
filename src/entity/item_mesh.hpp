// Dropped-item geometry (Phase 10): the CPU side of the item mesh.
//
// makeItemCubeGeometry builds the vertex/index data for a single floating
// block item: the block's six faces (no culling — a dropped item shows
// every face), scaled to ItemEntity::kSize and centered on the origin, with
// full sky light baked in (the shader's day/night scale then shades it like
// the world: bright by day, dark by night). The game uploads the result to
// a Mesh once per block id and draws it with a translate + rotateY model
// matrix; the bob/spin come from the frame counter (Game), so headless runs
// stay byte-reproducible.
#pragma once

#include "block/block.hpp"
#include "renderer/mesh.hpp"
#include "renderer/texture_atlas.hpp"

namespace mc {

struct ItemMeshGeometry {
    std::vector<MeshVertex> vertices;
    std::vector<std::uint32_t> indices;
};

ItemMeshGeometry makeItemCubeGeometry(const Block& block,
                                      const AtlasLayout& layout);

} // namespace mc
