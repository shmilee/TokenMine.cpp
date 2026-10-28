#include "entity/pathfind.hpp"

#include <algorithm>
#include <cstdint>
#include <unordered_set>
#include <vector>

#include "block/block.hpp"
#include "block/block_registry.hpp"

namespace mc {

namespace {

struct NodeKey {
    int x = 0;
    int y = 0;
    int z = 0;
    bool operator==(const NodeKey&) const = default;
};

struct NodeKeyHash {
    std::size_t operator()(const NodeKey& k) const {
        // FNV-1a over the coordinates: path-scale, collision-irrelevant
        // (the set only checks equality after the hash).
        std::uint64_t h = 1469598103934665603ULL;
        const int parts[3] = {k.x, k.y, k.z};
        for (const int p : parts) {
            h ^= static_cast<std::uint32_t>(p);
            h *= 1099511628211ULL;
        }
        return static_cast<std::size_t>(h);
    }
};

// The fixed neighbor order (-X, -Z, +Z, +X) — the determinism contract:
// identical inputs expand identical queues.
constexpr int kDirX[4] = {-1, 0, 0, 1};
constexpr int kDirZ[4] = {0, -1, 1, 0};

bool chebyshevOne(int ax, int ay, int az, const PathNode& b) {
    return ax >= b.x - 1 && ax <= b.x + 1 && ay >= b.y - 1 && ay <= b.y + 1 &&
           az >= b.z - 1 && az <= b.z + 1;
}

} // namespace

bool cellStandable(const World& world, const BlockRegistry& blocks,
                   int x, int y, int z, int bodyCells) {
    // The feet cell + the headroom cells are air.
    for (int k = 0; k < bodyCells; ++k) {
        if (world.getBlock(x, y + k, z) != BlockID::air()) {
            return false;
        }
    }
    // The floor: a solid FullCube below the feet (the integer top
    // plane — stairs/slabs give non-integer feet and stay out of the
    // graph, the documented deviation).
    const BlockID floorId = world.getBlock(x, y - 1, z);
    if (floorId == BlockID::air()) {
        return false;
    }
    const Block& floor = blocks.at(floorId);
    return floor.properties.solid &&
           floor.model.shape == BlockModel::Shape::FullCube;
}

Result<std::vector<PathNode>> pathfindBFS(const World& world,
                                          const BlockRegistry& blocks,
                                          const PathNode& from,
                                          const PathNode& target,
                                          int bodyCells, int maxNodes) {
    // Already in the target zone: no path needed (the straight chase
    // covers the final stretch).
    if (chebyshevOne(from.x, from.y, from.z, target)) {
        return std::vector<PathNode>{};
    }
    if (!cellStandable(world, blocks, from.x, from.y, from.z, bodyCells)) {
        return MC_FAIL("pathfinding: the start cell is not standable");
    }

    std::vector<PathNode> seen;
    seen.reserve(static_cast<std::size_t>(maxNodes));
    seen.push_back(from);
    // The parent index of each seen node (-1 = the start).
    std::vector<int> parent;
    parent.reserve(static_cast<std::size_t>(maxNodes));
    parent.push_back(-1);
    std::unordered_set<NodeKey, NodeKeyHash> visited;
    visited.reserve(static_cast<std::size_t>(maxNodes));
    visited.emplace(from.x, from.y, from.z);
    // The BFS queue: seen indices in expansion order.
    std::vector<int> queue{0};

    for (std::size_t head = 0; head < queue.size(); ++head) {
        if (seen.size() >= static_cast<std::size_t>(maxNodes)) {
            return MC_FAIL("pathfinding: the route exceeds the node cap");
        }
        const PathNode& n = seen[queue[head]];
        for (int i = 0; i < 4; ++i) {
            const int nx = n.x + kDirX[i];
            const int nz = n.z + kDirZ[i];

            // Try the flat neighbor, then the 1-step-up neighbor, then
            // the fall corridors (deepest last). Each candidate is
            // enqueued at most once (the visited set).
            PathNode candidates[2 + kPathMaxFall];
            int count = 0;
            candidates[count++] = {nx, n.y, nz, true};
            candidates[count++] = {nx, n.y + 1, nz, true};
            for (int k = 1; k <= kPathMaxFall; ++k) {
                if (world.getBlock(nx, n.y - k, nz) != BlockID::air()) {
                    break; // solid in the fall corridor: no deeper
                }
                candidates[count++] = {nx, n.y - k, nz, true};
            }
            for (int c = 0; c < count; ++c) {
                const PathNode& m = candidates[c];
                if (!cellStandable(world, blocks, m.x, m.y, m.z, bodyCells)) {
                    continue;
                }
                const NodeKey key{m.x, m.y, m.z};
                if (!visited.insert(key).second) {
                    continue;
                }
                const int idx = static_cast<int>(seen.size());
                seen.push_back(m);
                parent.push_back(queue[head]);
                if (chebyshevOne(m.x, m.y, m.z, target)) {
                    // Reconstruct: the chain from m back to the start,
                    // excluding the start, reversed.
                    std::vector<PathNode> path;
                    for (int cur = idx; cur != 0; cur = parent[cur]) {
                        path.push_back(seen[cur]);
                    }
                    std::reverse(path.begin(), path.end());
                    return path;
                }
                queue.push_back(idx);
            }
        }
    }
    return MC_FAIL("pathfinding: no standable route to the target");
}

} // namespace mc
