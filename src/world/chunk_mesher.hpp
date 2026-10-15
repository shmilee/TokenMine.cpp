// ChunkMesher: turns one chunk into GPU-ready vertex/index streams using
// GREEDY MESHING (Phase 18). A face exists when its neighbor block does
// not fully occlude it (neighbor is air or non-opaque); per face direction
// the 16x16 slice of existing faces is flood-merged into maximal runs of
// same (texture, sub-mesh, face light) cells, and each run emits ONE quad
// instead of one quad per face. Vertex/quad counts then scale with the
// number of runs, not with the number of exposed faces (a 16x16 flat
// ground is one quad, not 256).
//
// Two slices exist per plane because two different blocks can own a face
// on the same plane (both non-opaque, e.g. leaves next to leaves): the
// +normal face of the cell below the plane and the -normal face of the
// cell above it. Each slice is merged independently (opposite winding),
// so the between-two-adjacent-non-opaque-blocks faces are still both
// emitted — correct, just wasteful (see the Phase 2 known simplification,
// now preserved by design). Neighbor lookups go through the World, so
// culling is correct across chunk boundaries; a missing neighbor chunk
// reads as air (faces toward the void are drawn).
//
// Merging is light-conservative: a face bakes the light of the cell it
// borders (per face, like before), and two faces merge only when that
// baked light is identical — so every merged quad is exactly the union of
// same-texture, same-light faces (no lighting approximation is introduced
// by merging).
//
// Tiled UVs: a merged quad repeats one atlas tile W x H times, which a
// single [0,1] UV pair cannot express under CLAMP_TO_EDGE. Actually
// merged quads (W * H > 1) are emitted with `tiled = 1`, `uv` = the
// atlas tile index, and `uvLocal` = the position in tile units;
// basic.frag wraps the fractional part back into the tile (see the
// shader). Single-face quads (W = H = 1) keep the legacy atlas-space UV
// and `tiled = 0`, so they sample bit-identically to the Phase 17
// per-face mesh. All other producers (item cube, particles, cracks, UI)
// also keep `tiled = 0` and their atlas-space UVs.
//
// Fluids (water and lava): a cell's fluid level (0..kMaxFluidLevel, see
// Chunk) is meshed as a cube scaled to a height of level/kMaxFluidLevel —
// a full level is the ordinary cube. Culling is fluid-aware per KIND: a
// face is hidden by an equal-or-higher neighbor of the SAME kind (so
// water-to-water and lava-to-lava faces are culled; the other kind never
// hides a face), a top face by an opaque block above, a bottom face by any
// same-kind fluid below. A fluid face is lit by its own cell's light (the
// fluid darkens with depth, see lighting; a glowing kind — lava — carries
// its block light). The fluid's textures come from the registry's
// reference block of the cell's kind (BlockRegistry::fluidBlock,
// properties.fluid names the kind). A fluid block stored in the block
// array is a data error and is skipped (fluids live in the fluid array,
// not the block array). Fluid surfaces stay per-face (level-scaled
// geometry, not merged).
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
#include "block/cube_face_table.hpp"
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
    // One mask cell: a visible face on a greedy slice, carrying everything
    // that must be identical for two cells to merge into one quad — the
    // atlas tile (texture), the sub-mesh (opaque vs transparent pass), and
    // the baked face light (light-conservative merging).
    struct MaskCell {
        bool valid = false;
        int tileCol = 0;
        int tileRow = 0;
        bool transparent = false;
        std::uint8_t sky = 0;
        std::uint8_t blockLight = 0;

        bool sameAs(const MaskCell& o) const {
            return tileCol == o.tileCol && tileRow == o.tileRow &&
                   transparent == o.transparent && sky == o.sky &&
                   blockLight == o.blockLight;
        }
    };

    ChunkMesher(const World& world, const BlockRegistry& blocks,
                const AtlasLayout& layout);

    // Meshes the chunk at `pos` (must already exist in the world; a missing
    // chunk yields empty output).
    ChunkMeshData mesh(const ChunkPos& pos) const;

private:
    // Greedy-mesh all faces of one direction: build the 16x16 mask of
    // visible faces per slice along the normal axis, flood-merge runs of
    // same (texture, sub-mesh, light) cells, emit one quad per run.
    void meshFaceDirection(ChunkMeshData& out, const Chunk& chunk,
                           const BlockPos& origin, const CubeFace& face,
                           int faceIndex) const;

    // Emit one merged quad: the run of `w` x `h` cells anchored at mask
    // position (u0, v0) on slice plane `p`, using the seed cell's
    // texture/light. Preserves the kCubeFaces winding and UV convention.
    void emitMergedQuad(ChunkMeshData& out, const CubeFace& face,
                        int faceIndex, int p, int u0, int v0, int w, int h,
                        const MaskCell& seed) const;

    // Emit the fluid surface of one cell (level > 0) into the transparent
    // sub-mesh: the unit-cube faces scaled to level/kMaxFluidLevel, with
    // same-KIND fluid-aware culling (a neighbor of the other kind never
    // hides a face) and the cell's own light baked in.
    void emitFluid(ChunkMeshData& out, const Block& fluidBlock,
                   const BlockPos& origin, int lx, int ly, int lz,
                   std::uint8_t level, FluidKind kind) const;

    // Emit the cross-plant geometry of one cell (Phase 45): two crossing
    // vertical quads (the two cell diagonals) into the transparent
    // sub-mesh — full texture width on each quad, the cell's own light
    // baked in, no neighbor culling (the quads sit inside the cell; the
    // depth test against opaque neighbors handles visibility, as in
    // Minecraft). Drawn in the unculled blending pass, so each quad is
    // visible from both sides.
    void emitCross(ChunkMeshData& out, const Block& block,
                   const BlockPos& origin, int lx, int ly, int lz) const;
    // Phase 85 slab: the unit-cube faces scaled to the bottom half (y 0..0.5)
    // — the top face is always emitted (the cell's top half is air); the
    // bottom/side faces are culled by an opaque neighbor (like emitFluid,
    // but into the OPAQUE sub-mesh and the top is never occluded).
    void emitSlab(ChunkMeshData& out, const Block& block,
                  const BlockPos& origin, int lx, int ly, int lz) const;
    // Phase 87 stair: a north-facing wedge (-Z end full height, +Z end at the
    // floor). A slanted top (always drawn) + the -Z back wall + the -X/+X side
    // triangles + the bottom; the +Z front is at floor level (no face). Side /
    // back / bottom faces are culled by an opaque neighbor (like the slab).
    void emitStairs(ChunkMeshData& out, const Block& block,
                    const BlockPos& origin, int lx, int ly, int lz) const;

    const World& m_world;
    const BlockRegistry& m_blocks;
    const AtlasLayout& m_layout;
};

} // namespace mc
