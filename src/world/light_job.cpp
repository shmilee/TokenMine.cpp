#include "world/light_job.hpp"

#include <cstdlib>

namespace mc {

bool lightRegionsIndependent(const ColumnPos& a, const ColumnPos& b) {
    // One separated axis is enough: the regions' chunk sets (write
    // footprint + boundary-read ring) cannot share a chunk.
    return std::abs(a.x - b.x) >= kLightParallelColumnGap ||
           std::abs(a.z - b.z) >= kLightParallelColumnGap;
}

std::array<BlockPos, 2> columnLightBox(const ColumnPos& col) {
    const BlockPos lo{col.x * kChunkSize, 0, col.z * kChunkSize};
    const BlockPos hi{lo.x + kChunkSize - 1, kWorldHeight - 1,
                      lo.z + kChunkSize - 1};
    return {lo, hi};
}

LightJobPlan planLightJobs(const std::set<ColumnPos>& pending) {
    LightJobPlan plan;
    if (pending.empty()) {
        return plan;
    }
    if (pending.size() == 2) {
        const ColumnPos first = *pending.begin();
        const ColumnPos last = *pending.rbegin();
        if (lightRegionsIndependent(first, last)) {
            // Two parallel jobs, one box each (post order = set order).
            plan.jobs.push_back({first});
            plan.jobs.push_back({last});
            return plan;
        }
    }
    // One sequential job: set order (x, z) = the pre-99.10 box order.
    plan.jobs.push_back(std::vector<ColumnPos>(pending.begin(),
                                               pending.end()));
    return plan;
}

} // namespace mc
