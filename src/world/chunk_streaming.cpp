#include "world/chunk_streaming.hpp"

#include <algorithm>
#include <cmath>
#include <set>

namespace mc {

int columnDistance(const ColumnPos& a, const ColumnPos& b) {
    return std::max(std::abs(a.x - b.x), std::abs(a.z - b.z));
}

std::vector<ColumnPos> missingColumns(const ColumnPos& player,
                                      const std::vector<ColumnPos>& loaded,
                                      int radius) {
    const std::set<ColumnPos> have(loaded.begin(), loaded.end());
    struct Candidate {
        int dist;
        ColumnPos pos;
    };
    std::vector<Candidate> missing;
    for (int dx = -radius; dx <= radius; ++dx) {
        for (int dz = -radius; dz <= radius; ++dz) {
            const ColumnPos c{player.x + dx, player.z + dz};
            if (have.count(c) == 0) {
                missing.push_back({columnDistance(player, c), c});
            }
        }
    }
    std::sort(missing.begin(), missing.end(), [](const Candidate& a,
                                                 const Candidate& b) {
        if (a.dist != b.dist) {
            return a.dist < b.dist;
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

int nearestMissingDistance(const ColumnPos& player,
                           const std::vector<ColumnPos>& loaded, int radius) {
    const std::set<ColumnPos> have(loaded.begin(), loaded.end());
    int nearest = radius + 1;
    for (int dx = -radius; dx <= radius; ++dx) {
        for (int dz = -radius; dz <= radius; ++dz) {
            const ColumnPos c{player.x + dx, player.z + dz};
            if (have.count(c) == 0) {
                const int d = columnDistance(player, c);
                nearest = std::min(nearest, d);
            }
        }
    }
    return nearest;
}

bool streamingActive(const ColumnPos& player,
                     const std::vector<ColumnPos>& loaded, int radius,
                     int triggerDistance) {
    // The trigger column can be inside the radius ring (the usual case:
    // the player approaches the edge of the loaded area) or the player's
    // own column can be missing (the player outran the streamer) — the
    // latter is distance 0 and is covered by the scan anyway. d > radius
    // is the "whole ring loaded" sentinel, which must never engage.
    const int d = nearestMissingDistance(player, loaded, radius);
    return d <= radius && d <= triggerDistance;
}

} // namespace mc
