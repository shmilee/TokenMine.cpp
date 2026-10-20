// Mesh queue selection (PIPE-1): the pure decision logic for which dirty
// chunk columns get a worker mesh job this frame. No World, no threads —
// the Game wires the result into the pool (postMeshJobs), the same
// chunk_streaming pattern.
//
// The mesher's compute moved off the main thread (the 99.x/step-0
// measurement: up to ~11 ms/frame of main-thread remesh during the
// fill, plus a multi-minute tail). Each posted job meshes ONE column
// (8 chunks) on a worker against its own throwaway snapshot World
// (the column's 3x3 column neighborhood — the mesher's full read set);
// the main thread only uploads the merged column mesh to GL.
//
// The selection rules keep every snapshot's light FINAL (the only
// worker that writes the live World after the snapshot is this frame's
// lighting job):
//   - a column in `unlit` (its own light job not done) never posts —
//     it meshes once, lit (the 99.4 "no light-0 first mesh" rule);
//   - a column within Chebyshev 1 of any `lightPending` column never
//     posts — the lighting job's footprint (kLightUpdateRadius = 16
//     blocks = one column) rewrites that neighborhood AFTER the
//     snapshot; the job re-marks the chunks dirty, so the column re-
//     enters the queue on a later frame (self-healing);
//   - a column with an in-flight mesh job never posts twice.
// Nearest to the player first (columnDistanceSq, ties by (x, z)) —
// deterministic; the frame-locked FIFO integration preserves it.
#pragma once

#include <set>
#include <vector>

#include "world/chunk_streaming.hpp"

namespace mc {

// Up to `cap` columns to post for worker meshing this frame, drawn from
// the dirty COLUMNS (the dirty chunk set's column projection), nearest
// to `player` first (squared Euclidean, ties by (x, z)). See the header
// for the exclusion rules. cap <= 0 yields an empty result.
std::vector<ColumnPos> nextMeshColumns(const std::set<ColumnPos>& dirty,
                                       const ColumnPos& player,
                                       const std::set<ColumnPos>& unlit,
                                       const std::set<ColumnPos>& lightPending,
                                       const std::set<ColumnPos>& inFlight,
                                       int cap);

} // namespace mc
