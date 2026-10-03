// ChunkMesher: turns one chunk into GPU-ready vertex/index streams using
// face culling — a face is emitted only when its neighbor block does not
// fully occlude it (neighbor is air or non-opaque). Neighbor lookups go
// through the World, so culling is correct across chunk boundaries; a
// missing neighbor chunk reads as air (faces toward the void are drawn).
//
// Fluids (water): a cell's fluid level (0..kMaxFluidLevel, see Chunk) is
// meshed as a cube scaled to a height of level/kMaxFluidLevel — a full
// level is the ordinary cube. Culling is fluid-aware: a face is hidden by
// an equal-or-higher fluid neighbor (so water-to-water faces are culled),
// a top face by an opaque block above, a bottom face by any fluid below.
// A fluid face is lit by its own cell's light (the water darkens with
// depth, see lighting). The fluid's textures come from the registry's
// fluid block (BlockRegistry::fluidBlock, properties.fluid). A fluid block
// stored in the block array is a data error and is skipped (fluids live in
// the fluid array, not the block array).
//
// Known simplification: two adjacent non-opaque BLOCKS of the same kind
// (leaves next to leaves) still emit the face between them. Correct, just
// wasteful. (Water-to-water faces are culled — fluids are level-aware.)
//
// Output: two sub-meshes per chunk so the renderer can draw all opaque
// geometry first and the transparent (blended) geometry second:
//   - faces belonging to opaque blocks
//   - faces belonging to transparent blocks (glass, leaves, water)
//
// Vertex positions are chunk-local (0..16); the renderer applies
// translate(chunkOrigin).
#pragma once

#include <cstdint>
#include <vector>

#include "block/block_registry.hpp"
#include "renderer/mesh.hpp"
#include "renderer/texture_atlas.hpp"
#include "world/coordinates.hpp"
#include "world/world.hpp"

namespace mc {

struct ChunkMeshData {
    std::vector<MeshVertex> opaqueVertices;
    std::vector<std::uint32_t> opaqueIndices;
    std::vector<MeshVertex> transparentVertices;
    std::vector<std::uint32_t> transparentIndices;

    bool opaqueEmpty() const { return opaqueIndices.empty(); }
    bool transparentEmpty() const { return transparentIndices.empty(); }
};

class ChunkMesher {
public:
    ChunkMesher(const World& world, const BlockRegistry& blocks,
                const AtlasLayout& layout);

    // Meshes the chunk at `pos` (must already exist in the world; a missing
    // chunk yields empty output).
    ChunkMeshData mesh(const ChunkPos& pos) const;

private:
    // Emit the fluid surface of one cell (level > 0) into the transparent
    // sub-mesh: the unit-cube faces scaled to level/kMaxFluidLevel, with
    // fluid-aware culling and the cell's own light baked in.
    void emitFluid(ChunkMeshData& out, const Block& fluidBlock,
                   const BlockPos& origin, int lx, int ly, int lz,
                   std::uint8_t level) const;

    const World& m_world;
    const BlockRegistry& m_blocks;
    const AtlasLayout& m_layout;
};

} // namespace mc
