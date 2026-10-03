// ChunkMesher: turns one chunk into GPU-ready vertex/index streams using
// face culling — a face is emitted only when its neighbor block does not
// fully occlude it (neighbor is air or non-opaque). Neighbor lookups go
// through the World, so culling is correct across chunk boundaries; a
// missing neighbor chunk reads as air (faces toward the void are drawn).
//
// Known simplification (refined in the lighting/fluid phases): two adjacent
// non-opaque blocks of the same kind (water next to water, leaves next to
// leaves) still emit the face between them. Correct, just wasteful.
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
    const World& m_world;
    const BlockRegistry& m_blocks;
    const AtlasLayout& m_layout;
};

} // namespace mc
