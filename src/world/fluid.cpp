#include "world/fluid.hpp"

#include <algorithm>
#include <cassert>
#include <unordered_map>
#include <utility>

#include "world/chunk.hpp"

namespace mc {

namespace {

// The four horizontal neighbors, fixed order (deterministic iteration).
const BlockPos kHorizontal[4] = {
    {1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1},
};

// A proposed fluid-level change at a world position. Multiple proposals for
// the same cell merge to the max level (e.g. two adjacent sources spreading
// into one cell, or a spread racing a falling chain into one cell).
struct Change {
    BlockPos pos;
    std::uint8_t level;
};

void propose(std::vector<Change>& changes, const BlockPos& pos,
             std::uint8_t level) {
    for (Change& c : changes) {
        if (c.pos == pos) {
            c.level = std::max(c.level, level);
            return;
        }
    }
    changes.push_back({pos, level});
}

} // namespace

std::vector<BlockPos> FluidSim::tick(World& world) {
    const std::vector<ChunkPos> positions = world.chunkPositions();
    if (positions.empty()) {
        return {};
    }

    // Per-chunk falling flags, computed column-wise bottom-up. A chunk's
    // bottom row may read the flag of the chunk below it, which is always
    // processed first (ChunkPos sorts by y within equal x: the chunk below
    // has the same x and a smaller y).
    std::unordered_map<ChunkPos, std::array<std::uint8_t, Chunk::kVolume>>
        fallingByChunk;
    fallingByChunk.reserve(positions.size());

    std::vector<Change> changes;

    for (const ChunkPos& cp : positions) {
        const Chunk* c = world.findChunk(cp);
        const BlockID* blocks = c->data();
        const std::uint8_t* fluid = c->fluidData();
        const int ox = cp.x * kChunkSize;
        const int oy = cp.y * kChunkSize;
        const int oz = cp.z * kChunkSize;

        // Neighbor chunks for cross-boundary reads (cached; missing = void).
        const Chunk* belowChunk =
            (oy > 0) ? world.findChunk({cp.x, cp.y - 1, cp.z}) : nullptr;
        const Chunk* aboveChunk = world.findChunk({cp.x, cp.y + 1, cp.z});
        const Chunk* westChunk = world.findChunk({cp.x - 1, cp.y, cp.z});
        const Chunk* eastChunk = world.findChunk({cp.x + 1, cp.y, cp.z});
        const Chunk* southChunk = world.findChunk({cp.x, cp.y, cp.z - 1});
        const Chunk* northChunk = world.findChunk({cp.x, cp.y, cp.z + 1});

        // The cell at local (lx, ly, lz), possibly in a neighbor chunk.
        // Missing chunks read as air/no fluid (the void is dry).
        const auto cellAt = [&](int lx, int ly, int lz)
            -> std::pair<BlockID, std::uint8_t> {
            if (ly < 0) {
                if (belowChunk == nullptr) {
                    return {BlockID::air(), 0};
                }
                const int i = Chunk::index(lx, kChunkSize - 1, lz);
                return {belowChunk->data()[i], belowChunk->fluidData()[i]};
            }
            if (ly >= kChunkSize) {
                if (aboveChunk == nullptr) {
                    return {BlockID::air(), 0};
                }
                const int i = Chunk::index(lx, 0, lz);
                return {aboveChunk->data()[i], aboveChunk->fluidData()[i]};
            }
            if (lx < 0) {
                if (westChunk == nullptr) {
                    return {BlockID::air(), 0};
                }
                const int i = Chunk::index(kChunkSize - 1, ly, lz);
                return {westChunk->data()[i], westChunk->fluidData()[i]};
            }
            if (lx >= kChunkSize) {
                if (eastChunk == nullptr) {
                    return {BlockID::air(), 0};
                }
                const int i = Chunk::index(0, ly, lz);
                return {eastChunk->data()[i], eastChunk->fluidData()[i]};
            }
            if (lz < 0) {
                if (southChunk == nullptr) {
                    return {BlockID::air(), 0};
                }
                const int i = Chunk::index(lx, ly, kChunkSize - 1);
                return {southChunk->data()[i], southChunk->fluidData()[i]};
            }
            if (lz >= kChunkSize) {
                if (northChunk == nullptr) {
                    return {BlockID::air(), 0};
                }
                const int i = Chunk::index(lx, ly, 0);
                return {northChunk->data()[i], northChunk->fluidData()[i]};
            }
            const int i = Chunk::index(lx, ly, lz);
            return {blocks[i], fluid[i]};
        };

        // -- 1. Falling scan (column-wise, bottom-up) -----------------------
        // A fluid cell falls when the cell below is empty or itself falling;
        // a block below supports it.
        std::array<std::uint8_t, Chunk::kVolume>& falling =
            fallingByChunk[cp];
        falling.fill(0);
        for (int lx = 0; lx < kChunkSize; ++lx) {
            for (int lz = 0; lz < kChunkSize; ++lz) {
                for (int ly = 0; ly < kChunkSize; ++ly) {
                    const int i = Chunk::index(lx, ly, lz);
                    if (fluid[i] == 0) {
                        continue;
                    }
                    if (ly > 0) {
                        const int below = i - kChunkSize * kChunkSize;
                        if (blocks[below] != BlockID::air()) {
                            falling[i] = 0; // supported by a block
                        } else if (fluid[below] > 0) {
                            falling[i] = falling[below];
                        } else {
                            falling[i] = 1; // nothing below
                        }
                    } else {
                        const auto [bb, ff] = cellAt(lx, -1, lz);
                        if (bb != BlockID::air()) {
                            falling[i] = 0;
                        } else if (ff > 0) {
                            const auto fit =
                                fallingByChunk.find({cp.x, cp.y - 1, cp.z});
                            const int bi =
                                Chunk::index(lx, kChunkSize - 1, lz);
                            falling[i] = (fit == fallingByChunk.end())
                                             ? 0
                                             : fit->second[bi];
                        } else {
                            falling[i] = 1;
                        }
                    }
                }
            }
        }

        // -- 2. Per-cell rules (all reads are snapshot reads) ---------------
        for (int ly = 0; ly < kChunkSize; ++ly) {
            for (int lz = 0; lz < kChunkSize; ++lz) {
                for (int lx = 0; lx < kChunkSize; ++lx) {
                    const int i = Chunk::index(lx, ly, lz);
                    const std::uint8_t L = fluid[i];
                    if (L == 0) {
                        continue;
                    }
                    const BlockPos wp{ox + lx, oy + ly, oz + lz};

                    if (falling[i]) {
                        // The conveyor: the cell below takes this level (y
                        // < 0 drains out of the world); this cell takes the
                        // level of the cell above (air or a block above
                        // reads no fluid — the chain top empties).
                        if (wp.y > 0) {
                            propose(changes, {wp.x, wp.y - 1, wp.z}, L);
                        }
                        const auto [ab, above] = cellAt(lx, ly + 1, lz);
                        (void)ab;
                        propose(changes, wp, above);
                        continue;
                    }

                    // Resting: the cell below holds a block or a resting
                    // fluid (an empty cell below is the falling case).
                    const auto [bb, below] = cellAt(lx, ly - 1, lz);
                    if (bb != BlockID::air()) {
                        // Supported: spread to the horizontal neighbors.
                        // Level 1 is the terminal film and does not spread.
                        if (L > 1) {
                            const std::uint8_t next = L - 1;
                            for (const BlockPos& d : kHorizontal) {
                                const auto [tb, tf] =
                                    cellAt(lx + d.x, ly, lz + d.z);
                                if (tb != BlockID::air()) {
                                    continue; // a block (any) stops the flow
                                }
                                if (tf >= next) {
                                    continue; // already at/above the level
                                }
                                propose(changes,
                                        {wp.x + d.x, wp.y, wp.z + d.z}, next);
                            }
                        }
                    } else if (below > 0) {
                        // Resting on fluid:
                        //  - a full cell on anything is stable (sources never
                        //    merge);
                        //  - a cell resting on a FULL cell becomes full: a
                        //    column backed by a source below refills to the
                        //    top (this is what settles a broken lake floor —
                        //    without it the refilled column oscillates
                        //    level-7 / absorbed / level-7 forever);
                        //  - a shallower film on a flowing (non-full) pool is
                        //    absorbed (the pool keeps its level);
                        //  - an equal or deeper flow stays and raises the
                        //    pool below to its level.
                        if (L == kMaxFluidLevel) {
                            // Stable.
                        } else if (below == kMaxFluidLevel) {
                            propose(changes, wp, kMaxFluidLevel);
                        } else if (L < below) {
                            propose(changes, wp, 0); // absorbed
                        } else {
                            propose(changes, {wp.x, wp.y - 1, wp.z}, L);
                        }
                    } else {
                        assert(false && "resting cell with empty cell below");
                    }
                }
            }
        }
    }

    // -- 3. Apply the snapshot changes (sorted, for determinism) ------------
    std::sort(changes.begin(), changes.end(),
              [](const Change& a, const Change& b) { return a.pos < b.pos; });
    std::vector<BlockPos> changed;
    for (const Change& c : changes) {
        if (world.getFluid(c.pos.x, c.pos.y, c.pos.z) == c.level) {
            continue; // no net change (e.g. a conveyor re-reading its level)
        }
        if (world.setFluid(c.pos.x, c.pos.y, c.pos.z, c.level)) {
            changed.push_back(c.pos);
        }
    }
    return changed;
}

} // namespace mc
