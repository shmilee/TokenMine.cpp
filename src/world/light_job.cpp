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

std::set<ColumnPos> changedCellColumns(const std::vector<BlockPos>& changed) {
    std::set<ColumnPos> columns;
    for (const BlockPos& p : changed) {
        columns.insert({floorDiv(p.x, kChunkSize),
                        floorDiv(p.z, kChunkSize)});
    }
    return columns;
}

LightJobPlan planFluidLightJobs(const std::set<ColumnPos>& pending,
                                const std::set<ColumnPos>& streaming) {
    if (pending.empty()) {
        return LightJobPlan{};
    }
    std::set<ColumnPos> taken;
    for (const ColumnPos& col : pending) {
        if (taken.size() >=
            static_cast<std::size_t>(kFluidLightColumnsPerFrame)) {
            break; // the budget is full; the rest stays pending
        }
        // Window exclusivity (99.11 header note): a candidate whose light
        // region is not independent of a streaming job's column cannot be
        // posted into the same worker window (the 99.10 parallel safety
        // is per-plan). It stays pending for a later frame.
        bool sharesWindow = false;
        for (const ColumnPos& s : streaming) {
            if (!lightRegionsIndependent(col, s)) {
                sharesWindow = true;
                break;
            }
        }
        if (!sharesWindow) {
            taken.insert(col);
        }
    }
    // The taken subset has at most kFluidLightColumnsPerFrame columns, so
    // planLightJobs' partition applies unchanged (0 -> none, 1 -> one job,
    // 2 -> two parallel jobs iff independent, else one sequential job).
    return planLightJobs(taken);
}

} // namespace mc
