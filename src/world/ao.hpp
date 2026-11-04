// V2-18 AO (ambient occlusion, v2.19.0): the classic Minecraft vertex AO
// as a pure function family (the chunk mesher's only consumer).
//
// Every drawn face vertex darkens by how the THREE in-front cells around
// it are filled — the classic voxel AO rule (the design's "viewer side =
// C + n base"): for a face of cell C with outward unit-axis normal n,
// the vertex sits on the corner of the VIEWER cell V = C + n; the three
// check cells are the in-front cells sharing the vertex's edges, other
// than V: side1 = V + e1, side2 = V + e2, corner = V + e1 + e2, where
// e1/e2 run along the face's two in-plane axes, AWAY from the cell at
// the vertex (corner on the cell's min side -> -axis, max side -> +axis;
// the three checks are exactly the in-front edge-sharing cells — the
// staircase-riser top edge darkening by the step above and the
// wall-recess corner are the two canonical looks this rule reproduces).
//
// Occlusion = opaque AND HEIGHT BAND (the culling's same `opaque` flag —
// the project's self-consistent rule, not MC's full/half/stair shape
// vocabulary): a query cell occludes the vertex when its within-cell
// solid band contains the vertex's height, endpoints INCLUSIVE (the slab
// seam vertex at 0.5 counts for both the neighbor's [0, 0.5] and the
// [0.5, 1] band — "a vertex is only occluded by same-height neighbors"):
//   full opaque -> [0, 1]
//   slab state 0 -> [0, kSlabHalf], state 1 -> [kSlabHalf, 1],
//         state >= 2 -> [0, 1] (the Entity::solidRange bands)
//   stairs / wall / fence opaque -> [0, 1] (deviation 2: the stair slope
//         is not refined — the slope's high end over-occludes, documented)
//   air / void / non-opaque -> no band (out-of-range reads air = open
//         sky, never occludes)
//
// The level (0..3) drives a 4-tier MC-exact factor:
//   side1 AND side2 -> 3 (the two-side special case = the corner
//         darkness, Minecraft's exact rule — the count would say 2)
//   otherwise      -> the count of the three (0..2)
//   level 0 -> 1.0 (kAoFull) / 1 -> 0.75 (kAoOne) / 2 -> 0.5 (kAoTwo)
//         / 3 -> 0.3 (kAoCorner)
// The mesh bakes factor(light) into the vertex's EXISTING sky/block
// light attributes (zero shader / vertex-format / uniform change — see
// ChunkMesher); the GPU bilinear filter between the vertex factors is
// the smooth lighting. The per-draw light OVERRIDE (beacon beam, item
// drops, XP orbs) replaces the baked light in the vertex stage, so those
// surfaces lose their AO (deviation 3, documented — MC's beam does not
// AO either).
//
// The height argument is the vertex's y within the FACE'S CELL (0..1:
// the full-block corner's y, the slab's scaled y 0/0.5/1, the stair's
// emitted y) — the band test is (cell.y + yLocal) vs the query cell's
// world Y, so a face whose plane does not sit on a cell row (the slab
// mid-face) tests its true height (the slab state-0 TOP face at y+0.5
// queries the row above, whose bands start at y+1 — the mid-plane vertex
// is never covered: half-block faces only AO against same-row neighbors,
// the MC rule).
#pragma once

#include <array>
#include <cstdint>

#include "block/block.hpp"
#include "block/cube_face_table.hpp"
#include "core/error.hpp"
#include "core/math/vec3.hpp"
#include "world/coordinates.hpp"

namespace mc {

class World;
class BlockRegistry;
class Config;

// The 4-tier AO light factors (Minecraft's exact values: the two-side
// special case is 0.3, the single-occludor tiers step by 0.25).
inline constexpr float kAoFull = 1.0f;   // level 0: no occluder
inline constexpr float kAoOne = 0.75f;   // level 1: one of the three
inline constexpr float kAoTwo = 0.5f;    // level 2: two (or the count-2)
inline constexpr float kAoCorner = 0.3f; // level 3: both sides (MC exact)

// One query cell's occlusion band, cell-local and CLOSED (both endpoints
// inclusive). hi < lo = no occluder (air / void / non-opaque).
struct AoBand {
    float lo = 0.0f;
    float hi = 0.0f;

    static constexpr AoBand none() { return {1.0f, 0.0f}; }
    bool empty() const { return hi < lo; }
};

// The band from the query cell's block + its resolved per-cell state
// (the slab read rule: kBlockStateUnset already resolved to the block's
// state default by the caller — the emitSlab rule; non-slab blocks
// ignore `state`). Air / non-opaque = empty (no occlusion); slab = the
// kSlabHalf bands by state (0 -> [0, 0.5], 1 -> [0.5, 1], >= 2 ->
// [0, 1]); everything else opaque (full cube, stairs — deviation 2,
// wall, fence: their within-cell solid regions span the whole cell) =
// [0, 1].
AoBand aoOccluderBand(const Block& block, int state);

// true when a vertex at row-local height y (0..1 in the FACE'S cell) is
// inside a query cell's band: the query's world row is `rowOffset` from
// the face's cell row, so the vertex's height in the query's row is
// y - rowOffset, tested against [lo, hi] with BOTH endpoints inclusive
// (the closed interval is exact float — every value here is a multiple
// of 0.5, no epsilon is needed or wanted).
bool aoBandCovers(float y, int rowOffset, const AoBand& band);

// The 4-level corner count (Minecraft's exact rule): the two-side
// special case wins (both sides solid = the corner darkness, level 3),
// otherwise the plain count of the three booleans (0..2).
std::uint8_t aoCornerLevel(bool side1, bool side2, bool corner);

// The light factor by level (the 4-tier MC-exact table above).
float aoFactor(std::uint8_t level);

// The AO level (0..3) of ONE face vertex: the three in-front cells
// around it (viewer side = based at cell + normal), each tested by
// aoOccluderBand against the vertex's height. `cell` = the meshed cell,
// `normal` = the face's OUTWARD normal (a unit axis — the kCubeFaces
// convention; the stair slope passes +Y, see the caller), `cornerX/Y/Z`
// = the vertex's cell-local 0/1 corner (the face table's corner,
// PRE-scale — the in-plane components select the e directions, the
// normal-axis component is inert), `yLocal` = the vertex's height in
// the face's cell (0..1, the EMITTED position's cell-local y: the slab's
// 0/0.5, the stair slope's 0/1).
std::uint8_t aoVertexLevel(const World& world, const BlockRegistry& blocks,
                           const BlockPos& cell, const Vec3& normal,
                           int cornerX, int cornerY, int cornerZ,
                           float yLocal);

// The 4 corner levels of one greedy slice face of one cell (the
// MaskCell's ao[4], the face table's corner order c0..c3): the 8 in-
// front cells around the viewer cell are fetched ONCE per cell (the 4
// corners' 3 checks draw from those 8), then each corner's band test
// runs against the vertex's table-corner y. `face.corner[k][1]` is
// that yLocal (a full-cube face corner's y is 0 or 1).
void aoCellCornerLevels(const World& world, const BlockRegistry& blocks,
                        const BlockPos& cell, const CubeFace& face,
                        std::array<std::uint8_t, 4>& out);

// render.ao (strict read — the weatherEnabled / readNightSkyLevel
// discipline, the SAVE-1 config pattern): absent = true (the code
// default — the committed config.json carries no such key); present and
// non-boolean = named and rejected at init. OFF = the mesher's AO factor
// is constant 1.0 = the pre-V2-19 mesh bit-identical (the gate's
// "new-off" control: the AO path is gated at mesher construction — the
// shader / vertex format / per-draw overrides are untouched either way).
Result<bool> aoEnabled(const Config& config);

} // namespace mc
