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
// Output: THREE sub-meshes per chunk so the renderer can draw the
// geometry in the classic three-pass order (PRE-5):
//   - faces belonging to opaque blocks
//   - faces belonging to CUTOUT blocks (transparent + the `cutout`
//     property: leaves, cross plants) — drawn after the opaque pass with
//     alpha discard active AND depth write on (the holes write no depth)
//   - faces belonging to the remaining transparent (translucent-BLEND)
//     blocks (glass, the glass pane, the fluids) — drawn last, blended,
//     depth write off
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
    // PRE-5: the cutout bucket (transparent + `cutout` blocks: leaves,
    // cross plants). Drawn in the cutout pass (discard + depth write).
    std::vector<MeshVertex> cutoutVertices;
    std::vector<std::uint32_t> cutoutIndices;
    // The translucent-blend bucket (the remaining transparent blocks:
    // glass, the glass pane, the fluids). Drawn last, blended, no depth
    // write (render.alpha_test on) / depth write (off = the pre-PRE-5
    // pipeline, the cutout bucket included — see the game's render()).
    std::vector<MeshVertex> transparentVertices;
    std::vector<std::uint32_t> transparentIndices;

    bool opaqueEmpty() const { return opaqueIndices.empty(); }
    bool cutoutEmpty() const { return cutoutIndices.empty(); }
    bool transparentEmpty() const { return transparentIndices.empty(); }
};

class ChunkMesher {
public:
    // One mask cell: a visible face on a greedy slice, carrying everything
    // that must be identical for two cells to merge into one quad — the
    // atlas tile (texture), the sub-mesh (the opaque / cutout / blend
    // pass), and the baked face light (light-conservative merging).
    struct MaskCell {
        bool valid = false;
        int tileCol = 0;
        int tileRow = 0;
        bool transparent = false;
        // PRE-5: the block's `cutout` property (transparent sub-pass
        // routing: cutout pass vs translucent-blend pass). In sameAs so a
        // cutout face and a blend face sharing tile + light never merge
        // (they route to different sub-meshes).
        bool cutout = false;
        std::uint8_t sky = 0;
        std::uint8_t blockLight = 0;

        bool sameAs(const MaskCell& o) const {
            return tileCol == o.tileCol && tileRow == o.tileRow &&
                   transparent == o.transparent && cutout == o.cutout &&
                   sky == o.sky && blockLight == o.blockLight;
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
    // vertical quads (the two cell diagonals) — full texture width on
    // each quad, the cell's own light baked in, no neighbor culling (the
    // quads sit inside the cell; the depth test against opaque neighbors
    // handles visibility, as in Minecraft). PRE-5: routed by the block's
    // `cutout` property — the cutout sub-mesh (depth write + discard) or
    // the translucent-blend sub-mesh. Drawn unculled, so each quad is
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
    // Phase 95 wall: a 1.5-block-tall pillar (the cell + a half cap above).
    // The bottom + top cap faces are culled by an opaque neighbor; the side
    // faces are always emitted (the wall extends above the cell — the
    // conservative approach accepts the Z-fighting, like the slab).
    void emitWall(ChunkMeshData& out, const Block& block,
                  const BlockPos& origin, int lx, int ly, int lz) const;
    // Phase 98 fence: two crossed 1/8-thick posts (y 0..1.5) + a connection
    // bar (open ends) toward each horizontal fence / full opaque block
    // neighbor. The post top / bottom faces are culled by an opaque
    // neighbor above / below (a fence stacked on a fence).
    void emitFence(ChunkMeshData& out, const Block& block,
                   const BlockPos& origin, int lx, int ly, int lz) const;
    // Phase 99 glass pane: a 1/16-thick 1x1 sheet (z-centered, double-sided,
    // transparent sub-mesh) + rim slivers (culled against a pane neighbor —
    // the transparent version of the Phase 96 connection culling; the top /
    // bottom rims also against an opaque neighbor).
    void emitGlassPane(ChunkMeshData& out, const Block& block,
                       const BlockPos& origin, int lx, int ly, int lz) const;

    const World& m_world;
    const BlockRegistry& m_blocks;
    const AtlasLayout& m_layout;
};

} // namespace mc
