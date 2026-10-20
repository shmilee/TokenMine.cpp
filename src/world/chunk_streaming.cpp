#include "world/chunk_streaming.hpp"

#include <algorithm>
#include <set>

namespace mc {

std::int64_t columnDistanceSq(const ColumnPos& a, const ColumnPos& b) {
    const std::int64_t dx = a.x - b.x;
    const std::int64_t dz = a.z - b.z;
    return dx * dx + dz * dz;
}

std::int64_t missingSentinelDistanceSq(int radius) {
    const std::int64_t outside = radius + 1;
    return outside * outside;
}

std::vector<ColumnPos> missingColumns(const ColumnPos& player,
                                      const std::vector<ColumnPos>& loaded,
                                      int radius) {
    const std::set<ColumnPos> have(loaded.begin(), loaded.end());
    const std::int64_t r2 = static_cast<std::int64_t>(radius) * radius;
    struct Candidate {
        std::int64_t distSq;
        ColumnPos pos;
    };
    std::vector<Candidate> missing;
    // Walk the bounding square [−R, R]² (same cost as before STRM-1) and
    // keep the disk by the squared predicate.
    for (int dx = -radius; dx <= radius; ++dx) {
        for (int dz = -radius; dz <= radius; ++dz) {
            const std::int64_t d2 = static_cast<std::int64_t>(dx) * dx +
                                    static_cast<std::int64_t>(dz) * dz;
            if (d2 > r2) {
                continue;
            }
            const ColumnPos c{player.x + dx, player.z + dz};
            if (have.count(c) == 0) {
                missing.push_back({d2, c});
            }
        }
    }
    std::sort(missing.begin(), missing.end(), [](const Candidate& a,
                                                 const Candidate& b) {
        if (a.distSq != b.distSq) {
            return a.distSq < b.distSq;
        }
        if (a.pos.x != b.pos.x) {
            return a.pos.x < b.pos.x;
        }
        return a.pos.z < b.pos.z;
    });
    std::vector<ColumnPos> out;
    out.reserve(missing.size());
    for (const Candidate& c : missing) {
        out.push_back(c.pos);
    }
    return out;
}

std::int64_t nearestMissingDistanceSq(const ColumnPos& player,
                                      const std::vector<ColumnPos>& loaded,
                                      int radius) {
    const std::set<ColumnPos> have(loaded.begin(), loaded.end());
    const std::int64_t r2 = static_cast<std::int64_t>(radius) * radius;
    std::int64_t nearest = missingSentinelDistanceSq(radius);
    for (int dx = -radius; dx <= radius; ++dx) {
        for (int dz = -radius; dz <= radius; ++dz) {
            const std::int64_t d2 = static_cast<std::int64_t>(dx) * dx +
                                    static_cast<std::int64_t>(dz) * dz;
            if (d2 > r2) {
                continue;
            }
            const ColumnPos c{player.x + dx, player.z + dz};
            if (have.count(c) == 0) {
                nearest = std::min(nearest, d2);
            }
        }
    }
    return nearest;
}

std::size_t diskColumnCount(int radius) {
    // Exact integer count of the disk lattice: walk the bounding square,
    // filter by the squared predicate (the same enumeration the
    // streaming pass itself uses).
    const std::int64_t r2 = static_cast<std::int64_t>(radius) * radius;
    std::size_t count = 0;
    for (int dx = -radius; dx <= radius; ++dx) {
        for (int dz = -radius; dz <= radius; ++dz) {
            if (static_cast<std::int64_t>(dx) * dx +
                    static_cast<std::int64_t>(dz) * dz <= r2) {
                ++count;
            }
        }
    }
    return count;
}

bool streamingActive(const ColumnPos& player,
                     const std::vector<ColumnPos>& loaded, int radius,
                     int triggerDistance) {
    // The trigger column can be inside the radius disk (the usual case:
    // the player approaches the edge of the loaded area) or the player's
    // own column can be missing (the player outran the streamer) — the
    // latter is distance 0 and is covered by the scan anyway. The
    // sentinel (whole disk loaded) is strictly outside the disk and must
    // never engage (radius 0: sentinel 1² = trigger 1² — the in-disk
    // guard below is what keeps it out).
    const std::int64_t d2 = nearestMissingDistanceSq(player, loaded, radius);
    const std::int64_t r2 = static_cast<std::int64_t>(radius) * radius;
    const std::int64_t t2 =
        static_cast<std::int64_t>(triggerDistance) * triggerDistance;
    return d2 <= r2 && d2 <= t2;
}

} // namespace mc
