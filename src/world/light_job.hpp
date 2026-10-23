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

// ---------------------------------------------------------------------------
// 99.11: the fluid catch-up (the async twin of the streaming light jobs)
//
// The old fluid-tick path re-lit the bounding box of ALL of the tick's
// changed cells synchronously on the main thread (the Phase 64 pattern).
// In the shipped ocean the coast film cascade scatters changed cells over
// hundreds of columns per tick (713 -> 918 -> 1357 cells/tick and growing),
// so that box's rescan cost the main thread 130 -> 396 -> 661 s per
// 300-frame window (step-0 instrumentation; the tick itself is 20-38 ms —
// 99% of the fluid section's cost was the re-light). 99.11 replaces it
// with a column-granular pending set drained by budgeted worker jobs:
//   - after the tick, every changed cell's column is inserted into the
//     Game's pending set (changedCellColumns — the set merges all cells
//     of one column into one entry; the burn pass's fire cells ride the
//     same `changed` vector, a fire is a light-15 source);
//   - each frame, postFluidLightJobs posts planFluidLightJobs(pending,
//     this window's streaming job columns) as worker jobs (the 99.10
//     infrastructure: columnLightBox + planLightJobs' parallel/sequential
//     partition + dirtyOut net-changed collection);
//   - the main-thread cost drops to set inserts, and the light lands with
//     a bounded lag.
//
// Why the lag is bounded and the catch-up converges EXACTLY to the full
// propagation once drained (the idempotent-convergence oracle, tests):
// light's dependency region is local — sky and block light die within 15
// horizontal steps (enterCost >= 1 per cell), and the full-column box
// expanded by kLightUpdateRadius (16) covers a cell's ENTIRE light
// dependency neighborhood. Relighting one column therefore fixes that
// column's light to its exact value given the current world, no matter
// what the neighbors hold: the rescan re-asserts the vertical values (a
// strict reset, lowering stale-high), the monotone strict-increase BFS
// then restores every value the current world allows (raising stale-low),
// and stored values can never overshoot the true field (the invariant,
// preserved by both stages, seeded by the previous relight's result).
// The coast churn rate (~100-170 changed columns per fluid tick) exceeds
// the budget, so the pending set saturates at the coast column count
// (bounded — unloadFarColumns evicts the dead entries) and every coast
// column is re-lit within one saturation cycle: the per-column lag is
// bounded, and a fully drained set lands on exactly the propagateAll
// light. The film's light values are nearly invariant across its period-2
// churn, so the lagged frames are visually indistinguishable (documented
// deviation: fluid light moves from "tick-synchronous" to "budgeted
// catch-up", the 99.4 lit-pop-in precedent).
//
// Window safety: a fluid job may share a worker window with the streaming
// light jobs only when the pair is lightRegionsIndependent — that
// criterion (write footprint + the block-light boundary-read ring, either
// axis gap >= kLightParallelColumnGap) is exactly the pairwise condition
// for zero shared chunk (write-write AND read-write). planFluidLightJobs
// skips a candidate that fails it against ANY streaming column; the 99.10
// planLightJobs partition keeps the fluid pairs themselves safe.

// The per-frame budget: at most this many fluid columns are posted as
// catch-up jobs per frame (in set order — frame-locked deterministic).
// Sized against the step-0 coast churn (>> budget, so the set saturates
// at the coast column count) and the 99.10 flush floor (one full-column
// box ~8.4 ms/worker).
inline constexpr int kFluidLightColumnsPerFrame = 2;

// The column set of a fluid tick's changed cells (std::set: every cell of
// one column merges into ONE entry, set order is deterministic; floorDiv
// on X/Z, hard rule 1). Empty input -> empty set (a settled world).
std::set<ColumnPos> changedCellColumns(const std::vector<BlockPos>& changed);

// The fluid catch-up job plan: from `pending` (set order), skip the
// columns whose light region is NOT independent of a `streaming` column
// (they cannot share the worker window — see the header note), and take
// the first kFluidLightColumnsPerFrame of the rest; the taken subset is
// partitioned exactly like planLightJobs (an independent pair -> two
// parallel jobs, a dependent pair -> one sequential job, 3+ cannot happen
// because the budget is 2). The union of the returned jobs' columns is
// the taken set — the Game erases it from `pending` at post time (before
// the jobs run) so an in-flight column is never re-taken.
LightJobPlan planFluidLightJobs(const std::set<ColumnPos>& pending,
                                const std::set<ColumnPos>& streaming);

} // namespace mc
