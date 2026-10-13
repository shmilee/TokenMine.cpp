#include "world/fluid.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "world/chunk.hpp"

namespace mc {

namespace {

// The four horizontal neighbors, fixed order (deterministic iteration).
const BlockPos kHorizontal[4] = {
    {1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1},
};

// The six 3D neighbors (reaction pass), fixed order (deterministic).
const BlockPos kSixNeighbors[6] = {
    {1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}, {0, 1, 0}, {0, -1, 0},
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

// Reads cells of one chunk and its six face neighbors; a missing chunk
// reads as air/no fluid (the void is dry). Out-of-range local coordinates
// index the neighbor chunk's border row.
class CellReader {
public:
    CellReader(const Chunk* self, const Chunk* below, const Chunk* above,
               const Chunk* west, const Chunk* east, const Chunk* south,
               const Chunk* north)
        : m_self(self), m_below(below), m_above(above), m_west(west),
          m_east(east), m_south(south), m_north(north) {}

    // (block, water level, lava level) at local (lx, ly, lz); a level is 0
    // for a block cell, a void cell, or a cell holding the other kind.
    std::tuple<BlockID, std::uint8_t, std::uint8_t>
    cellAt(int lx, int ly, int lz) const {
        if (ly < 0) {
            if (m_below == nullptr) {
                return {BlockID::air(), 0, 0};
            }
            return read(m_below, Chunk::index(lx, kChunkSize - 1, lz));
        }
        if (ly >= kChunkSize) {
            if (m_above == nullptr) {
                return {BlockID::air(), 0, 0};
            }
            return read(m_above, Chunk::index(lx, 0, lz));
        }
        if (lx < 0) {
            if (m_west == nullptr) {
                return {BlockID::air(), 0, 0};
            }
            return read(m_west, Chunk::index(kChunkSize - 1, ly, lz));
        }
        if (lx >= kChunkSize) {
            if (m_east == nullptr) {
                return {BlockID::air(), 0, 0};
            }
            return read(m_east, Chunk::index(0, ly, lz));
        }
        if (lz < 0) {
            if (m_south == nullptr) {
                return {BlockID::air(), 0, 0};
            }
            return read(m_south, Chunk::index(lx, ly, kChunkSize - 1));
        }
        if (lz >= kChunkSize) {
            if (m_north == nullptr) {
                return {BlockID::air(), 0, 0};
            }
            return read(m_north, Chunk::index(lx, ly, 0));
        }
        return read(m_self, Chunk::index(lx, ly, lz));
    }

private:
    static std::tuple<BlockID, std::uint8_t, std::uint8_t>
    read(const Chunk* c, int i) {
        const std::uint8_t code = c->fluidData()[i];
        return {c->data()[i],
                fluidKindOf(code) == FluidKind::Water
                    ? fluidLevelOf(code)
                    : 0,
                fluidKindOf(code) == FluidKind::Lava
                    ? fluidLevelOf(code)
                    : 0};
    }

    const Chunk* m_self;
    const Chunk* m_below;
    const Chunk* m_above;
    const Chunk* m_west;
    const Chunk* m_east;
    const Chunk* m_south;
    const Chunk* m_north;
};

// One chunk's six face neighbors (missing = nullptr).
struct NeighborCache {
    const Chunk* below = nullptr;
    const Chunk* above = nullptr;
    const Chunk* west = nullptr;
    const Chunk* east = nullptr;
    const Chunk* south = nullptr;
    const Chunk* north = nullptr;
};

NeighborCache neighborCache(const World& world, const ChunkPos& cp) {
    return NeighborCache{
        (cp.y > 0) ? world.findChunk({cp.x, cp.y - 1, cp.z}) : nullptr,
        world.findChunk({cp.x, cp.y + 1, cp.z}),
        world.findChunk({cp.x - 1, cp.y, cp.z}),
        world.findChunk({cp.x + 1, cp.y, cp.z}),
        world.findChunk({cp.x, cp.y, cp.z - 1}),
        world.findChunk({cp.x, cp.y, cp.z + 1}),
    };
}

// -- Water-lava reaction pass (Phase 51) -------------------------------------
//
// Snapshot scan: every LAVA cell 6-adjacent to a WATER cell converts to the
// block named by the lava fluid block's water_reaction ('source' when any
// water neighbor is a level-8 source, else 'flowing'). The water is not
// consumed. Inert when the lava reference block is missing, has no
// water_reaction, or names a block the registry does not know (the game
// init cross-checks the names — a test registry without them stays inert).
// Returns the converted cells (they are blocks now — re-light + re-mesh).
std::vector<BlockPos> reactLavaWithWater(World& world,
                                         const BlockRegistry& blocks) {
    BlockID toSource;
    BlockID toFlowing;
    if (const auto lavaId =
            blocks.fluidBlock(fluidKindName(FluidKind::Lava))) {
        const Block& lava = blocks.at(*lavaId);
        if (!lava.properties.waterReactionSource.empty() &&
            !lava.properties.waterReactionFlowing.empty()) {
            toSource =
                blocks.idForName(lava.properties.waterReactionSource);
            toFlowing =
                blocks.idForName(lava.properties.waterReactionFlowing);
        }
    }
    if (!toSource.valid() || !toFlowing.valid()) {
        return {}; // inert lava
    }

    struct Conversion {
        BlockPos pos;
        BlockID to;
    };
    std::vector<Conversion> conversions;
    const std::vector<ChunkPos> positions = world.chunkPositions();
    for (const ChunkPos& cp : positions) {
        const Chunk* c = world.findChunk(cp);
        const std::uint8_t* fluid = c->fluidData();
        const NeighborCache n = neighborCache(world, cp);
        const CellReader reader(c, n.below, n.above, n.west, n.east,
                                n.south, n.north);
        const int ox = cp.x * kChunkSize;
        const int oy = cp.y * kChunkSize;
        const int oz = cp.z * kChunkSize;
        for (int ly = 0; ly < kChunkSize; ++ly) {
            for (int lz = 0; lz < kChunkSize; ++lz) {
                for (int lx = 0; lx < kChunkSize; ++lx) {
                    const std::uint8_t code =
                        fluid[Chunk::index(lx, ly, lz)];
                    if (fluidKindOf(code) != FluidKind::Lava ||
                        fluidLevelOf(code) == 0) {
                        continue;
                    }
                    // Any 6-neighbor water cell (snapshot reads); a level-8
                    // neighbor makes the reaction a source reaction.
                    bool anyWater = false;
                    bool sourceWater = false;
                    for (const BlockPos& d : kSixNeighbors) {
                        const auto [tb, twater, tlava] =
                            reader.cellAt(lx + d.x, ly + d.y, lz + d.z);
                        (void)tb;
                        (void)tlava;
                        if (twater > 0) {
                            anyWater = true;
                            sourceWater =
                                sourceWater || twater == kMaxFluidLevel;
                        }
                    }
                    if (anyWater) {
                        const BlockPos wp{ox + lx, oy + ly, oz + lz};
                        conversions.push_back(
                            {wp, sourceWater ? toSource : toFlowing});
                    }
                }
            }
        }
    }

    // Apply (each conversion touches a distinct cell; sorted for a
    // deterministic order).
    std::sort(conversions.begin(), conversions.end(),
              [](const Conversion& a, const Conversion& b) {
                  return a.pos < b.pos;
              });
    std::vector<BlockPos> changed;
    for (const Conversion& cv : conversions) {
        if (world.setBlock(cv.pos.x, cv.pos.y, cv.pos.z, cv.to)) {
            changed.push_back(cv.pos);
        }
    }
    return changed;
}

// -- Flamable burn pass (Phase 58) ------------------------------------------
//
// Snapshot scan: every BLOCK cell 6-adjacent to a LAVA cell whose data is
// `flamable` burns to air (Minecraft: lava burns crops; the shipped data
// flags the wheat crop stages only). The lava is not consumed and there is
// NO drop (a burned crop is gone, like Minecraft's char). Inert when no
// registered block is flamable. A cell adjacent to several lava cells is
// collected once per lava neighbor — the sorted apply + the setBlock
// "changed" return de-duplicate it.
// Returns the destroyed cells (they are air now — re-light + re-mesh).
std::vector<BlockPos> burnFlamableNeighbors(World& world,
                                            const BlockRegistry& blocks) {
    // Fast exit: no flamable block in the registry -> nothing to burn.
    bool anyFlamable = false;
    for (const BlockID id : blocks.allIds()) {
        if (blocks.at(id).properties.flamable) {
            anyFlamable = true;
            break;
        }
    }
    if (!anyFlamable) {
        return {};
    }

    std::vector<BlockPos> destroyed;
    const std::vector<ChunkPos> positions = world.chunkPositions();
    for (const ChunkPos& cp : positions) {
        const Chunk* c = world.findChunk(cp);
        const std::uint8_t* fluid = c->fluidData();
        const NeighborCache n = neighborCache(world, cp);
        const CellReader reader(c, n.below, n.above, n.west, n.east,
                                n.south, n.north);
        const int ox = cp.x * kChunkSize;
        const int oy = cp.y * kChunkSize;
        const int oz = cp.z * kChunkSize;
        for (int ly = 0; ly < kChunkSize; ++ly) {
            for (int lz = 0; lz < kChunkSize; ++lz) {
                for (int lx = 0; lx < kChunkSize; ++lx) {
                    const std::uint8_t code =
                        fluid[Chunk::index(lx, ly, lz)];
                    if (fluidKindOf(code) != FluidKind::Lava ||
                        fluidLevelOf(code) == 0) {
                        continue;
                    }
                    // Any 6-neighbor flamable BLOCK cell (snapshot reads).
                    for (const BlockPos& d : kSixNeighbors) {
                        const auto [tb, twater, tlava] =
                            reader.cellAt(lx + d.x, ly + d.y, lz + d.z);
                        (void)twater;
                        (void)tlava;
                        if (tb == BlockID::air() ||
                            !blocks.at(tb).properties.flamable) {
                            continue;
                        }
                        destroyed.push_back(
                            {ox + lx + d.x, oy + ly + d.y, oz + lz + d.z});
                    }
                }
            }
        }
    }

    // Apply (sorted for a deterministic order; a cell collected twice
    // burns once — the second setBlock(air) reports no change).
    std::sort(destroyed.begin(), destroyed.end(),
              [](const BlockPos& a, const BlockPos& b) { return a < b; });
    std::vector<BlockPos> changed;
    for (const BlockPos& p : destroyed) {
        if (world.setBlock(p.x, p.y, p.z, BlockID::air())) {
            changed.push_back(p);
        }
    }
    return changed;
}

// -- Per-kind simulation -------------------------------------------------------
//
// One KIND of fluid over all loaded chunks (the other kind is invisible to
// the scan — after the reaction pass the two kinds are 6-disjoint, so this
// is exact). The same snapshot-based model as the historical single-fluid
// tick; returns the cells whose level changed.
std::vector<BlockPos> simulateType(World& world, FluidKind kind) {
    const std::vector<ChunkPos> positions = world.chunkPositions();
    if (positions.empty()) {
        return {};
    }

    // The kind's level in an encoded byte (0 for dry or the other kind).
    const auto levelOf = [kind](std::uint8_t code) {
        return fluidKindOf(code) == kind ? fluidLevelOf(code) : 0;
    };
    // The OTHER kind's level in an encoded byte (0 for dry or this kind).
    // A cell occupied by the other kind is treated as a BLOCK by this
    // kind's scan (support + spread blocker): with a water_reaction the
    // kinds are 6-disjoint after the reaction pass so this never fires,
    // but an inert lava (no reaction) must not let the kinds replace
    // each other.
    const auto otherOf = [kind](std::uint8_t code) {
        return fluidKindOf(code) == kind ? 0 : fluidLevelOf(code);
    };
    // The kind's level from a CellReader tuple's (water, lava) levels.
    const auto kindLevel = [kind](std::uint8_t water, std::uint8_t lava) {
        return kind == FluidKind::Water ? water : lava;
    };
    // The other kind's level from a CellReader tuple.
    const auto otherLevel = [kind](std::uint8_t water, std::uint8_t lava) {
        return kind == FluidKind::Water ? lava : water;
    };

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
        const NeighborCache n = neighborCache(world, cp);
        const CellReader reader(c, n.below, n.above, n.west, n.east,
                                n.south, n.north);
        const int ox = cp.x * kChunkSize;
        const int oy = cp.y * kChunkSize;
        const int oz = cp.z * kChunkSize;

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
                    const std::uint8_t L = levelOf(fluid[i]);
                    if (L == 0) {
                        continue;
                    }
                    if (ly > 0) {
                        const int below = i - kChunkSize * kChunkSize;
                        if (blocks[below] != BlockID::air() ||
                            otherOf(fluid[below]) > 0) {
                            falling[i] = 0; // supported (a block or the
                                            // other kind)
                        } else if (levelOf(fluid[below]) > 0) {
                            falling[i] = falling[below];
                        } else {
                            falling[i] = 1; // nothing below
                        }
                    } else {
                        const auto [bb, bw, bl] = reader.cellAt(lx, -1, lz);
                        if (bb != BlockID::air() || otherLevel(bw, bl) > 0) {
                            falling[i] = 0;
                        } else if (kindLevel(bw, bl) > 0) {
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
                    const std::uint8_t L = levelOf(fluid[i]);
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
                        const auto [ab, aw, al] =
                            reader.cellAt(lx, ly + 1, lz);
                        (void)ab;
                        propose(changes, wp, kindLevel(aw, al));
                        continue;
                    }

                    // Resting: the cell below holds a block (or the other
                    // kind) or a resting fluid (an empty cell below is the
                    // falling case).
                    const auto [bb, bw, bl] = reader.cellAt(lx, ly - 1, lz);
                    const std::uint8_t below = kindLevel(bw, bl);
                    if (bb != BlockID::air() || otherLevel(bw, bl) > 0) {
                        // Supported: spread to the horizontal neighbors.
                        // Level 1 is the terminal film and does not spread.
                        if (L > 1) {
                            const std::uint8_t next = L - 1;
                            for (const BlockPos& d : kHorizontal) {
                                const auto [tb, tw, tl] =
                                    reader.cellAt(lx + d.x, ly, lz + d.z);
                                if (tb != BlockID::air() ||
                                    otherLevel(tw, tl) > 0) {
                                    continue; // a block (or the other kind)
                                               // stops the flow
                                }
                                if (kindLevel(tw, tl) >= next) {
                                    continue; // already at/above the level
                                }
                                propose(changes,
                                        {wp.x + d.x, wp.y, wp.z + d.z}, next);
                            }
                        }
                    } else if (below > 0) {
                        // Resting on fluid:
                        //  - a full cell on anything is stable (sources
                        //    never merge);
                        //  - a cell resting on a FULL cell becomes full: a
                        //    column backed by a source below refills to the
                        //    top (this is what settles a broken lake floor —
                        //    without it the refilled column oscillates
                        //    level-7 / absorbed / level-7 forever);
                        //  - a shallower film on a flowing (non-full) pool
                        //    is absorbed (the pool keeps its level);
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
        if (world.getFluidOfKind(c.pos.x, c.pos.y, c.pos.z, kind) ==
            c.level) {
            continue; // no net change (e.g. a conveyor re-reading its level)
        }
        if (world.setFluid(c.pos.x, c.pos.y, c.pos.z, kind, c.level)) {
            changed.push_back(c.pos);
        }
    }
    return changed;
}

} // namespace

std::vector<BlockPos> FluidSim::tick(World& world,
                                     const BlockRegistry& blocks) {
    // Reaction first: it separates the kinds (a converted cell becomes a
    // block), so the per-kind scans that follow see 6-disjoint fluids.
    std::vector<BlockPos> changed = reactLavaWithWater(world, blocks);
    // Then the flamable burn (Phase 58): it destroys neighbor BLOCKS
    // without touching the fluids, so its position relative to the
    // per-kind scans is behavior-neutral — the reaction goes first so the
    // converted cells (blocks now) are visible to it.
    for (const BlockPos& p : burnFlamableNeighbors(world, blocks)) {
        changed.push_back(p);
    }
    for (const FluidKind kind : {FluidKind::Water, FluidKind::Lava}) {
        for (const BlockPos& p : simulateType(world, kind)) {
            changed.push_back(p);
        }
    }
    std::sort(changed.begin(), changed.end());
    changed.erase(
        std::unique(changed.begin(), changed.end(),
                    [](const BlockPos& a, const BlockPos& b) {
                        return a == b;
                    }),
        changed.end());
    return changed;
}

} // namespace mc
