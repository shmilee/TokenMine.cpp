// Lighting job planning (99.10): the pure decision logic for when the
// frame's pending lighting columns get their OWN parallel pool jobs
// versus one sequential job. No World, no threads — the Game wires the
// plan into the pool (postLightJobs), the same chunk_streaming / mesh_job
// pattern.
//
// 99.10 (the walking-streaming lighting floor): while the ring fills,
// two new columns land every frame, and the single lighting job re-lit
// both boxes sequentially on ONE worker (LGT-1 step-0: median 16.74 ms /
// frame, per box ~8.37 ms) — the main-thread flush floor. The two light
// regions are bit-level independent (parallel-safe) when neither region
// can WRITE or READ a light cell the other region writes:
//   - WRITE reach: a full-column box expanded by kLightUpdateRadius (16
//     blocks) covers exactly three columns (kLightRegionReach = 1 column
//     out on each side);
//   - READ reach: the block-light BFS additionally seeds lit cells one
//     CHUNK ring outside the box (the boundary source, lighting.cpp),
//     so a region reads one more column of light than it writes.
// Two regions are independent iff the column gap on at least one axis is
// >= kLightParallelColumnGap (4): one separated axis leaves no shared
// chunk to write or read. (The LGT-1 "footprint intersection" criterion
// |Δcol| ≤ 2 covers the WRITE reach only; a gap of 3 is write-disjoint
// but the shell ring still crosses, so 3 stays sequential.) The
// confluence half (sequential order does not change the final light for
// independent regions) makes the parallel completion order irrelevant:
// the result is a pure function of the pre-insertion world.
#pragma once

#include <array>
#include <set>
#include <vector>

#include "world/chunk_streaming.hpp"
#include "world/lighting.hpp"
#include "world/coordinates.hpp"

namespace mc {

// Write reach of one column's light region, in column units: the
// kChunkSize-block column box expanded by kLightUpdateRadius on each side
// reaches floor((kChunkSize - 1 + kLightUpdateRadius) / kChunkSize) = 1
// column out (15 + 16 = 31 < 32 = two columns).
inline constexpr int kLightRegionReach =
    (kChunkSize - 1 + kLightUpdateRadius) / kChunkSize;

// Minimum column gap (on EITHER axis) for two light regions to be
// bit-level independent (parallel-safe): the gap must exceed the write
// reach in BOTH directions plus the one-chunk boundary-read ring, i.e.
// 2 * (kLightRegionReach + 1) = 4.
inline constexpr int kLightParallelColumnGap = 2 * (kLightRegionReach + 1);

// True when the two columns' light regions (write footprint + the
// block-light boundary-read ring) share no chunk at all: posting them as
// two parallel pool jobs leaves the final light bit-identical to running
// them sequentially in EITHER order.
bool lightRegionsIndependent(const ColumnPos& a, const ColumnPos& b);

// The full-column light box a pending column's job re-lights: the
// column's kChunkSize² X/Z footprint × the full world height (the job
// runs Lighting::updateRegion over it with newTerrain = true).
std::array<BlockPos, 2> columnLightBox(const ColumnPos& col);

// The frame's lighting job plan. Each job lists its columns in the
// sequential box order (sorted (x, z) — for one job that is exactly the
// pre-99.10 single-job order).
struct LightJobPlan {
    std::vector<std::vector<ColumnPos>> jobs;
};

// Partition the pending columns into AT MOST TWO pool jobs:
//   - 0 columns: no jobs;
//   - 1 column: one job;
//   - exactly 2 columns, lightRegionsIndependent: two jobs, one box
//     each, posted in set order — the workers run them in parallel;
//   - a dependent pair, or 3+ columns: one job, all boxes sequential in
//     set order (the pre-99.10 behavior, bit-identical; 3+ pending
//     happens only on multi-column first-touch placement frames).
// Hard rule 7 (revised, 99.10): at most 2 lighting jobs per frame, and
// only for bit-level-independent regions; dependent columns always
// fall back to the single sequential job.
LightJobPlan planLightJobs(const std::set<ColumnPos>& pending);

} // namespace mc
