#include "world/mesh_job.hpp"

#include <algorithm>
#include <cstdlib>

namespace mc {

namespace {

// The lighting job's footprint: a region box expanded by
// kLightUpdateRadius (16 blocks) — exactly ONE column (Chebyshev) in
// each horizontal direction. A mesh candidate closer than this to a
// light-pending column would snapshot stale light (the job runs after
// the snapshot in the same worker window).
constexpr int kMeshLightExclusionColumns = 1;

bool withinLightFootprint(const ColumnPos& col,
                          const std::set<ColumnPos>& lightPending) {
    for (const ColumnPos& l : lightPending) {
        if (std::abs(col.x - l.x) <= kMeshLightExclusionColumns &&
            std::abs(col.z - l.z) <= kMeshLightExclusionColumns) {
            return true;
        }
    }
    return false;
}

} // namespace

std::vector<ColumnPos> nextMeshColumns(const std::set<ColumnPos>& dirty,
                                       const ColumnPos& player,
                                       const std::set<ColumnPos>& unlit,
                                       const std::set<ColumnPos>& lightPending,
                                       const std::set<ColumnPos>& inFlight,
                                       int cap) {
    if (cap <= 0) {
        return {};
    }
    // The dirty set is sorted (x, y, z): the projection below keeps a
    // deterministic (x, z) order before the distance sort.
    std::vector<ColumnPos> eligible;
    for (const ColumnPos& col : dirty) {
        if (unlit.count(col) != 0) {
            continue; // its own light job has not landed yet
        }
        if (inFlight.count(col) != 0) {
            continue; // a mesh job for it is already posted
        }
        if (withinLightFootprint(col, lightPending)) {
            continue; // this frame's lighting job rewrites its
                      // neighborhood after the snapshot
        }
        eligible.push_back(col);
    }
    std::sort(eligible.begin(), eligible.end(), [&player](const ColumnPos& a,
                                                          const ColumnPos& b) {
        const std::int64_t da = columnDistanceSq(player, a);
        const std::int64_t db = columnDistanceSq(player, b);
        if (da != db) {
            return da < db;
        }
        return a < b; // ColumnPos lexicographic (x, z)
    });
    if (eligible.size() > static_cast<std::size_t>(cap)) {
        eligible.resize(static_cast<std::size_t>(cap));
    }
    return eligible;
}

} // namespace mc
