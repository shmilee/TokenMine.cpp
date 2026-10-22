// Ore vein shape (GEN-3): the PURE geometry of a generated ore vein — a
// cluster of Minecraft 3x2x1 rotated ellipsoids (the retired 26-neighbor
// random walk). One ellipsoid = 21 cells: the integer point set of the
// axis-aligned ellipsoid with semi-axes (3, 2, 1),
//
//   4u^2 + 9v^2 + 36w^2 <= 36        (u on the 3-axis, v on 2, w on 1)
//
// (the w = 0 plane holds 19 cells + the two end caps (0, 0, +/-1) —
// EXACTLY 21, integer arithmetic only, no floats; pinned: (3,0,0) in,
// (0,2,0) in, (0,0,1) in, (3,1,0) out, (0,2,1) out). The 21 cells are
// centrally symmetric, so an orientation is one of the SIX axis
// permutations of (u, v, w) onto (x, y, z) (no independent mirror).
//
// A vein with `clusters` (K, the `vein_size` field, [1, kMaxOreVeinClusters])
// is K ellipsoids: the first centred on the vein start, each next centred
// within kOreVeinClusterSpread (2) of the previous one — a deterministic
// PITS #4 stream (state += golden step, then splitmix64, the
// computeProceduralLakes pattern) seeded by hash3(origin, salted seed).
// The output is the WORLD-COORDINATE cell list in cluster order, each
// cluster in the fixed table order, duplicates dropped (first occurrence
// kept). The max distance of any cell from the vein start is
// kOreVeinMaxAxis + kOreVeinClusterSpread * (K - 1) — the caller's
// lattice scan margin.
//
// placeOres feeds the lattice roll's existing per-ore salt stream and the
// in-cell uniform start; the per-cell filters (y bounds, inside-chunk,
// deep-only replacement) stay in the generator's unchanged placeOre.
#pragma once

#include <cstdint>
#include <vector>

#include "world/coordinates.hpp"

namespace mc {

// The ellipsoid's cell count (the 4u^2 + 9v^2 + 36w^2 <= 36 integer set).
inline constexpr int kOreVeinCellCount = 21;
// The longest semi-axis (the 3 of 3x2x1).
inline constexpr int kOreVeinMaxAxis = 3;
// The max centre-to-centre step between clusters (the (v % 5 - 2) range).
inline constexpr int kOreVeinClusterSpread = 2;
// The cluster-count ceiling (the `vein_size` domain [1, 8]; 8 ellipsoids
// = 168 cells, the saturation cap).
inline constexpr int kMaxOreVeinClusters = 8;
// Dedicated salt for the vein-plan stream: the plan draws never track the
// retired random-walk stream or any other feature roll (the worldgen
// salt family, after kSaltCave2).
inline constexpr std::uint64_t kOreVeinSalt = 0x1A2B3C4D5E6F7A09ULL;
// The PITS #4 golden step: the stream state must ADVANCE by this before
// every splitmix64 draw (the finalizer alone is not a progression).
inline constexpr std::uint64_t kOreVeinFeatureStep = 0x9E3779B97F4A7C15ULL;

// True when the local-axis-frame cell (u, v, w) is inside the 3x2x1
// ellipsoid (integer predicate: 4u^2 + 9v^2 + 36w^2 <= 36).
constexpr bool oreVeinCellInEllipse(int u, int v, int w) {
    return 4 * u * u + 9 * v * v + 36 * w * w <= 36;
}

// The vein plan: the WORLD-COORDINATE cells of a `clusters`-ellipsoid vein
// started at `origin` (pure function of seed + origin + clusters — the
// same start under the same seed always plans the same cells, whatever
// chunk requests it: the cross-chunk seam contract). `clusters` is clamped
// to [1, kMaxOreVeinClusters] (validated upstream — the clamp is a logic
// guard, not a data path).
std::vector<BlockPos> planOreVein(std::uint64_t seed, const BlockPos& origin,
                                  int clusters);

} // namespace mc
