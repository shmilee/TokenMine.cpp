// ENT-6: voxel pathfinding for the mob chase (stuck-triggered BFS).
//
// The graph: a node is an INTEGER standable position for a mob's feet
// (y = the top plane of the floor). `cellStandable` = the feet cell and
// the bodyCells headroom cells are air, and the cell below is a solid
// FullCube block (an integer floor — stairs/slabs give non-integer feet
// and are out of the graph, the documented deviation). Edges (all cost
// 1, fixed neighbor order = deterministic):
//   - flat: the 4 horizontal neighbors at the same feet y;
//   - step: the 4 neighbors one higher (the physics stepUp crosses a
//     1.0 step — the ENT-3 shared judgment, FullCube floor model);
//   - fall: the 4 downward corridors 1..kPathMaxFall deep, the corridor
//     cells air (a falling mob crosses open space, landing on the
//     floor at the bottom).
// BFS from `from` (which must itself be standable) to the TARGET ZONE
// (any standable node within Chebyshev distance 1 of `target` — the
// final stretch is the straight chase). Expansion is capped at
// `maxNodes`. No randomness, no floats: frame-locked safe (hard rule 8).
//
// The result: an EMPTY path = `from` is already inside the target zone
// (the caller keeps the straight chase); an ERROR = no standable route
// within the cap (the caller takes the lost cooldown).
#pragma once

#include <cstddef>
#include <vector>

#include "core/error.hpp"
#include "world/world.hpp"

namespace mc {

class BlockRegistry;

struct PathNode {
    int x = 0;
    int y = 0;
    int z = 0;
    bool valid = false;
    bool operator==(const PathNode& o) const {
        return x == o.x && y == o.y && z == o.z;
    }
};

// The BFS expansion cap: a path longer than this is not worth planning
// (the chase window re-triggers on the next rhythm).
inline constexpr int kPathMaxNodes = 512;
// The fall-corridor depth cap: deeper drops are open air anyway — the
// straight chase falls as well.
inline constexpr int kPathMaxFall = 8;

// Can a mob with `bodyCells` of body (feet at y, the cell column) stand
// on the integer floor below this node?
bool cellStandable(const World& world, const BlockRegistry& blocks,
                   int x, int y, int z, int bodyCells);

// The BFS above. Deterministic for identical inputs.
Result<std::vector<PathNode>> pathfindBFS(const World& world,
                                          const BlockRegistry& blocks,
                                          const PathNode& from,
                                          const PathNode& target,
                                          int bodyCells, int maxNodes);

} // namespace mc
