#include "world/fluid.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <map>
#include <set>
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

// A proposed fluid-level change at a world position (the apply phase's
// element). Multiple proposals for the same cell merge to the max level
// (e.g. two adjacent sources spreading into one cell, or a spread racing a
// falling chain into one cell).
struct Change {
    BlockPos pos;
    std::uint8_t level;
};

// PIPE-3: the order-agnostic max merge (the old linear-scan propose's
// semantics): conflicting targets resolve to the max level. The map's pos
// order IS the old post-sort `changes` order (the sort below is kept = the
// minimal diff).
void propose(std::map<BlockPos, std::uint8_t>& proposed, const BlockPos& pos,
             std::uint8_t level) {
    auto& slot = proposed[pos];
    if (level > slot) {
        slot = level;
    }
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

    // (block, water level, lava level, honey level) at local (lx, ly, lz);
    // a level is 0 for a block cell, a void cell, or a cell holding any
    // OTHER kind (FLUID-6: the tuple is one lane per kind).
    std::tuple<BlockID, std::uint8_t, std::uint8_t, std::uint8_t>
    cellAt(int lx, int ly, int lz) const {
        if (ly < 0) {
            if (m_below == nullptr) {
                return {BlockID::air(), 0, 0, 0};
            }
            return read(m_below, Chunk::index(lx, kChunkSize - 1, lz));
        }
        if (ly >= kChunkSize) {
            if (m_above == nullptr) {
                return {BlockID::air(), 0, 0, 0};
            }
            return read(m_above, Chunk::index(lx, 0, lz));
        }
        if (lx < 0) {
            if (m_west == nullptr) {
                return {BlockID::air(), 0, 0, 0};
            }
            return read(m_west, Chunk::index(kChunkSize - 1, ly, lz));
        }
        if (lx >= kChunkSize) {
            if (m_east == nullptr) {
                return {BlockID::air(), 0, 0, 0};
            }
            return read(m_east, Chunk::index(0, ly, lz));
        }
        if (lz < 0) {
            if (m_south == nullptr) {
                return {BlockID::air(), 0, 0, 0};
            }
            return read(m_south, Chunk::index(lx, ly, kChunkSize - 1));
        }
        if (lz >= kChunkSize) {
            if (m_north == nullptr) {
                return {BlockID::air(), 0, 0, 0};
            }
            return read(m_north, Chunk::index(lx, ly, 0));
        }
        return read(m_self, Chunk::index(lx, ly, lz));
    }

private:
    static std::tuple<BlockID, std::uint8_t, std::uint8_t, std::uint8_t>
    read(const Chunk* c, int i) {
        const std::uint8_t code = c->fluidData()[i];
        const std::uint8_t level = fluidLevelOf(code);
        return {c->data()[i],
                fluidKindOf(code) == FluidKind::Water ? level : 0,
                fluidKindOf(code) == FluidKind::Lava ? level : 0,
                fluidKindOf(code) == FluidKind::Honey ? level : 0};
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

// -- Per-kind cell-list collection (PIPE-3) --------------------------------
//
// The chunk's per-kind list (Chunk::m_fluidCells) holds one entry per
// 0->n / kind-change write; entries may be stale (a later drain or kind
// replacement) or duplicated (drain + re-wet before a compaction walk).
// A cell is LIVE for `kind` when its encoded byte is a positive level of
// that kind.

// Local coordinate decompositions of a cell index (i = (ly*16+lz)*16+lx).
inline int localX(std::uint32_t i) {
    return static_cast<int>(i % kChunkSize);
}
inline int localZ(std::uint32_t i) {
    return static_cast<int>((i / kChunkSize) % kChunkSize);
}
inline int localY(std::uint32_t i) {
    return static_cast<int>(i / (kChunkSize * kChunkSize));
}

// The falling-flag iteration order's sort key: column-major, bottom-up
// (col = lx*16+lz outer, ly inner) — the old triple loop's (lx, lz, ly)
// order restricted to fluid cells. The key is bijective in i.
std::uint32_t columnOrderKey(std::uint32_t i) {
    return (static_cast<std::uint32_t>(localX(i)) * kChunkSize +
            static_cast<std::uint32_t>(localZ(i))) *
               kChunkSize +
           static_cast<std::uint32_t>(localY(i));
}

// The simulateType walk's collection: the chunk's live cells of `kind`,
// column-major bottom-up, deduplicated. Non-fluid cells had zero effect in
// the old two 32K phases (phase 1 only wrote falling=0, which only fluid
// cells read; phase 2 skipped L==0), so traversing the live cells changes
// no (cell -> change) mapping. When the list is more than half stale or
// carried duplicates, it is compacted in place (the filtered result is
// stored; the invariant "live cell => at least one entry" holds — every
// stored entry is live).
std::vector<std::uint32_t> collectFluidCells(Chunk* c, FluidKind kind) {
    const std::uint8_t* fluid = c->fluidData();
    const std::vector<std::uint32_t>& list = c->fluidCells(kind);
    std::vector<std::uint32_t> fc;
    fc.reserve(list.size());
    for (std::uint32_t i : list) {
        const std::uint8_t code = fluid[i];
        if (fluidKindOf(code) == kind && fluidLevelOf(code) > 0) {
            fc.push_back(i); // live (the stale entries are filtered out)
        }
    }
    const std::size_t filtered = fc.size();
    std::sort(fc.begin(), fc.end(),
              [](std::uint32_t a, std::uint32_t b) {
                  return columnOrderKey(a) < columnOrderKey(b);
              });
    fc.erase(std::unique(fc.begin(), fc.end()), fc.end());
    if (filtered * 2 < list.size() || filtered != fc.size()) {
        c->compactFluidCells(kind, fc);
    }
    return fc;
}

// The reaction/burn passes' collection: live filter only. The passes are
// order-agnostic (their applies are globally sorted, and a cell collected
// twice burns/converts once — the second setBlock reports no change, the
// tick de-duplicates `changed`), so no sort/dedup is needed; stale
// entries MUST be filtered (a drained cell is no longer lava).
std::vector<std::uint32_t> collectLiveCells(const Chunk* c, FluidKind kind) {
    const std::uint8_t* fluid = c->fluidData();
    const std::vector<std::uint32_t>& list = c->fluidCells(kind);
    std::vector<std::uint32_t> fc;
    fc.reserve(list.size());
    for (std::uint32_t i : list) {
        const std::uint8_t code = fluid[i];
        if (fluidKindOf(code) == kind && fluidLevelOf(code) > 0) {
            fc.push_back(i);
        }
    }
    return fc;
}

// The chunks a per-kind SIMULATION pass must visit this tick: every
// ACTIVE chunk of the kind (a snapshot — the set only GROWS during the
// tick; PIPE-3: the 99.8 activity flags' set twin, the insertion points
// are exhaustive over the flag-setting sites) plus their face neighbors.
// A flow write crosses at most one chunk boundary per tick, so a chunk
// dry at the tick's start can only become wet from a fluid neighbor —
// visiting the neighbors keeps the same-tick propagation the old
// all-chunk scan provided (a newly wetted neighbor is read live, like the
// chunks past it in the sorted order were before), while the ~9k-200k dry
// chunks of a full-ring world are skipped.
std::vector<ChunkPos> simulateTypeChunks(World& world, FluidKind kind) {
    std::set<ChunkPos> visit;
    // Snapshot: the walk consumes entries from the live set below.
    const std::set<ChunkPos> active(world.fluidActiveChunks(kind));
    for (const ChunkPos& cp : active) {
        const Chunk* c = world.findChunk(cp);
        if (c == nullptr) {
            // Cannot happen (eraseChunk purges the entry) — belt.
            world.consumeFluidActive(cp, kind);
            continue;
        }
        // Activity double-check (Phase 99.8 + the PIPE-3 belt): a chunk is
        // walked only while HOT (a cell's fluid changed on the last
        // applied tick: a moving front, a fresh source spreading) or
        // PENDING (a block change under/beside it may have unstuck settled
        // fluid). A settled fluid — static pools AND static flow
        // gradients — changes nothing, goes cold, and costs zero per tick
        // (the Minecraft cell-queue property at chunk granularity). Both
        // flags false = a stale set entry: consume it and skip.
        if (!c->fluidHot(kind) && !c->fluidWalkPending(kind)) {
            world.consumeFluidActive(cp, kind);
            continue;
        }
        visit.insert(cp);
        visit.insert({cp.x - 1, cp.y, cp.z});
        visit.insert({cp.x + 1, cp.y, cp.z});
        visit.insert({cp.x, cp.y - 1, cp.z});
        visit.insert({cp.x, cp.y + 1, cp.z});
        visit.insert({cp.x, cp.y, cp.z - 1});
        visit.insert({cp.x, cp.y, cp.z + 1});
    }
    // Only chunks that EXIST (a neighbor of a fluid chunk is often void;
    // a void neighbor reads air/dry and can be created only by the game's
    // synchronous paths, never here).
    std::vector<ChunkPos> out;
    for (const ChunkPos& cp : visit) {
        if (world.hasChunk(cp)) {
            out.push_back(cp);
        }
    }
    return out;
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
    // Only chunks that HOLD lava can contain a reacting cell (the water
    // neighbor is read across the boundary): the all-chunk scan of the
    // Phase 51 vintage is the fluid-set scan.
    const std::vector<ChunkPos> positions =
        world.fluidChunkPositions(FluidKind::Lava);
    // A lava cell converts only next to a water cell, so a lava chunk
    // with no water in itself or its six face neighbors cannot convert
    // this tick (Phase 99.8: a settled lava sheet far from water — e.g.
    // the Phase 90 lake overflow — is skipped entirely).
    const std::set<ChunkPos>& waterChunks = world.fluidChunks(FluidKind::Water);
    const auto hasWaterNear = [&world, &waterChunks](const ChunkPos& cp) {
        if (waterChunks.count(cp) != 0) {
            return true;
        }
        const auto near = [&](const ChunkPos& q) {
            return waterChunks.count(q) != 0;
        };
        return near({cp.x - 1, cp.y, cp.z}) || near({cp.x + 1, cp.y, cp.z}) ||
               near({cp.x, cp.y - 1, cp.z}) || near({cp.x, cp.y + 1, cp.z}) ||
               near({cp.x, cp.y, cp.z - 1}) || near({cp.x, cp.y, cp.z + 1});
    };
    for (const ChunkPos& cp : positions) {
        if (!hasWaterNear(cp)) {
            continue;
        }
        const Chunk* c = world.findChunk(cp);
        const NeighborCache n = neighborCache(world, cp);
        const CellReader reader(c, n.below, n.above, n.west, n.east,
                                n.south, n.north);
        const int ox = cp.x * kChunkSize;
        const int oy = cp.y * kChunkSize;
        const int oz = cp.z * kChunkSize;
        // PIPE-3: the per-kind cell list replaces the 32K full scan (the
        // per-cell neighbor logic below is unchanged; the pass is
        // order-agnostic — the apply is globally sorted).
        const std::vector<std::uint32_t> fc =
            collectLiveCells(c, FluidKind::Lava);
        for (std::uint32_t i : fc) {
            const int lx = localX(i);
            const int ly = localY(i);
            const int lz = localZ(i);
            // Any 6-neighbor water cell (snapshot reads); a level-8
            // neighbor makes the reaction a source reaction.
            bool anyWater = false;
            bool sourceWater = false;
            for (const BlockPos& d : kSixNeighbors) {
                const auto [tb, twater, tlava, thoney] =
                    reader.cellAt(lx + d.x, ly + d.y, lz + d.z);
                (void)tb;
                (void)tlava;
                (void)thoney;
                if (twater > 0) {
                    anyWater = true;
                    sourceWater = sourceWater || twater == kMaxFluidLevel;
                }
            }
            if (anyWater) {
                const BlockPos wp{ox + lx, oy + ly, oz + lz};
                conversions.push_back({wp, sourceWater ? toSource : toFlowing});
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

// -- Flamable burn pass (Phase 58, the burn target Phase 64) ---------------
//
// Snapshot scan: every BLOCK cell 6-adjacent to a LAVA cell whose data is
// `flamable` burns to `burnTo` (Minecraft: lava burns crops; the shipped
// data flags the crop stages + the natural flammables). The lava is not
// consumed and there is NO drop (a burned crop is gone, like Minecraft's
// char). Inert when no registered block is flamable. A cell adjacent to
// several lava cells is collected once per lava neighbor — the sorted
// apply + the setBlock "changed" return de-duplicate it.
// `burnTo` is the FIRE block in the game (Phase 64: the burn leaves a
// light source that burns out — the init cross-check guarantees the fire
// block whenever a flamable exists) or air in a registry without the
// fire block (unit-test fixtures; the Phase 58 semantics).
// Returns the destroyed cells (they hold `burnTo` now — re-light +
// re-mesh).
std::vector<BlockPos> burnFlamableNeighbors(World& world,
                                            const BlockRegistry& blocks,
                                            BlockID burnTo,
                                            const std::set<ChunkPos>* flamm) {
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
    // Only chunks that HOLD lava contain burning source cells (the
    // flamable neighbor is read across the boundary): the fluid-set scan.
    const std::vector<ChunkPos> positions =
        world.fluidChunkPositions(FluidKind::Lava);
    // Flamable gate (Phase 99.8): a lava chunk can only burn a flamable
    // block in ITSELF or a face neighbor. The set is an over-approximation
    // (chunks that ever held a flamable block — a burn removes flammability
    // without pruning the entry): exact on the safe side (a miss would be
    // an unburned neighbor). Null = the legacy walk of every lava chunk
    // (unit tests).
    for (const ChunkPos& cp : positions) {
        if (flamm != nullptr) {
            bool nearFlamable = flamm->count(cp) != 0;
            const auto near = [flamm](const ChunkPos& q) {
                return flamm->count(q) != 0;
            };
            if (!nearFlamable) {
                nearFlamable =
                    near({cp.x - 1, cp.y, cp.z}) ||
                    near({cp.x + 1, cp.y, cp.z}) ||
                    near({cp.x, cp.y - 1, cp.z}) ||
                    near({cp.x, cp.y + 1, cp.z}) ||
                    near({cp.x, cp.y, cp.z - 1}) ||
                    near({cp.x, cp.y, cp.z + 1});
            }
            if (!nearFlamable) {
                continue;
            }
        }
        const Chunk* c = world.findChunk(cp);
        const NeighborCache n = neighborCache(world, cp);
        const CellReader reader(c, n.below, n.above, n.west, n.east,
                                n.south, n.north);
        const int ox = cp.x * kChunkSize;
        const int oy = cp.y * kChunkSize;
        const int oz = cp.z * kChunkSize;
        // PIPE-3: the per-kind cell list replaces the 32K full scan (the
        // per-cell neighbor logic below is unchanged; the pass is
        // order-agnostic — the apply is globally sorted, a cell collected
        // twice burns once).
        const std::vector<std::uint32_t> fc =
            collectLiveCells(c, FluidKind::Lava);
        for (std::uint32_t i : fc) {
            const int lx = localX(i);
            const int ly = localY(i);
            const int lz = localZ(i);
            // Any 6-neighbor flamable BLOCK cell (snapshot reads).
            for (const BlockPos& d : kSixNeighbors) {
                const auto [tb, twater, tlava, thoney] =
                    reader.cellAt(lx + d.x, ly + d.y, lz + d.z);
                (void)twater;
                (void)tlava;
                (void)thoney;
                if (tb == BlockID::air() ||
                    !blocks.at(tb).properties.flamable) {
                    continue;
                }
                destroyed.push_back(
                    {ox + lx + d.x, oy + ly + d.y, oz + lz + d.z});
            }
        }
    }

    // Apply (sorted for a deterministic order; a cell collected twice
    // burns once — the second setBlock reports no change).
    std::sort(destroyed.begin(), destroyed.end(),
              [](const BlockPos& a, const BlockPos& b) { return a < b; });
    std::vector<BlockPos> changed;
    for (const BlockPos& p : destroyed) {
        if (world.setBlock(p.x, p.y, p.z, burnTo)) {
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
//
// PIPE-3: the two 32K full scans (falling flags + per-cell rules) became
// per-kind CELL-LIST traversals — a visited chunk's live cells of the kind
// (collectFluidCells) in the old (lx, lz, ly) column-major bottom-up order.
// Bit-equivalence: non-fluid cells had zero effect in the old phases
// (phase 1 only wrote falling=0, which only fluid cells read; phase 2
// skipped L==0), so the (cell -> change) mapping is unchanged; the
// in-column dependency follows the same order; the cross-chunk reference
// (a bottom-row cell over a same-kind top cell below) reads topFlags —
// the flag of the column's topmost fluid cell, which IS the old falling
// array's top-row value in exactly the cases the reference fires; the
// proposals merge to the max (order-agnostic) as before.
std::vector<BlockPos> simulateType(World& world, FluidKind kind) {
    // Fluid chunks + their face neighbors (see simulateTypeChunks): a
    // flow write crosses at most one boundary per tick, so this is exactly
    // the set of chunks the tick can touch — the ~9k-200k dry chunks of a
    // full-ring world are skipped.
    const std::vector<ChunkPos> positions = simulateTypeChunks(world, kind);
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
    // The kind's level from a CellReader tuple's per-kind levels.
    const auto kindLevel = [kind](std::uint8_t water, std::uint8_t lava,
                                  std::uint8_t honey) {
        if (kind == FluidKind::Water) {
            return water;
        }
        if (kind == FluidKind::Lava) {
            return lava;
        }
        return honey;
    };
    // The level of ANY other kind present in the cell (0 when none): a
    // cell occupied by a foreign fluid is a block for this kind's scan
    // (support + spread blocker).
    const auto otherLevel = [kind](std::uint8_t water, std::uint8_t lava,
                                   std::uint8_t honey) {
        std::uint8_t best = 0;
        if (kind != FluidKind::Water) {
            best = std::max(best, water);
        }
        if (kind != FluidKind::Lava) {
            best = std::max(best, lava);
        }
        if (kind != FluidKind::Honey) {
            best = std::max(best, honey);
        }
        return best;
    };

    // Cross-chunk falling flags (PIPE-3): one flag per COLUMN (256)
    // replaces the old 32K cell array — only a chunk's bottom row reads
    // the chunk below it, and only when that chunk's TOP cell holds the
    // kind, in which case the read value is the flag of that column's
    // topmost fluid cell. After a chunk's walk, topFlags[cp][col] = the
    // column's topmost fluid cell's flag (a column without fluid cells
    // stays 0 — the reference never fires there: the below top cell is
    // not the kind, so the old code took the block/other/dry branch). A
    // miss = 0 (the old fallingByChunk map-miss semantics; the below
    // chunk is always in positions when it exists — a face neighbor of a
    // visited chunk — and positions iterate in ChunkPos order, the lower
    // chunk before the upper).
    std::unordered_map<ChunkPos, std::array<std::uint8_t, kChunkSize * kChunkSize>>
        topFlags;
    topFlags.reserve(positions.size());

    // The snapshot proposals, merged per cell to the max level (the old
    // propose's order-agnostic semantics); the map's pos order is the old
    // post-sort changes order.
    std::map<BlockPos, std::uint8_t> proposed;

    for (const ChunkPos& cp : positions) {
        const Chunk* c = world.findChunk(cp);
        // Consume the activity flags + the active-set entry (the PIPE-3
        // belt: a chunk visited as a neighbor holds no entry — the erase
        // is a no-op): they forced this walk; changes the walk APPLIES
        // (end-of-tick, via setFluid) re-heat + re-insert the affected
        // chunks for the next tick (snapshot semantics).
        Chunk* self = world.findChunkMut(cp);
        self->clearFluidHot(kind);
        self->clearFluidWalkPending(kind);
        world.consumeFluidActive(cp, kind);
        const BlockID* blocks = c->data();
        const std::uint8_t* fluid = c->fluidData();
        const NeighborCache n = neighborCache(world, cp);
        const CellReader reader(c, n.below, n.above, n.west, n.east,
                                n.south, n.north);
        const int ox = cp.x * kChunkSize;
        const int oy = cp.y * kChunkSize;
        const int oz = cp.z * kChunkSize;

        // -- 0. Collection: the chunk's live cells of the kind, ----------
        // column-major bottom-up, deduplicated (stale entries filtered;
        // the list compacted when more than half stale or duplicated).
        std::vector<std::uint32_t> fc = collectFluidCells(self, kind);

        // -- 1'. Falling flags (over fc only, column-wise, bottom-up) ----
        // A fluid cell falls when the cell below is empty or itself falling;
        // a block below supports it.
        std::array<std::uint8_t, kChunkSize * kChunkSize> colFlag {};
        std::vector<std::uint8_t> flags; // parallel with fc
        flags.reserve(fc.size());
        for (std::uint32_t i : fc) {
            const int lx = localX(i);
            const int lz = localZ(i);
            const int ly = localY(i);
            const int col = lx * kChunkSize + lz;
            std::uint8_t f;
            if (ly > 0) {
                const int below = i - kChunkSize * kChunkSize;
                if (blocks[below] != BlockID::air() ||
                    otherOf(fluid[below]) > 0) {
                    f = 0; // supported (a block or the other kind)
                } else if (levelOf(fluid[below]) > 0) {
                    // The below adjacent cell holds the kind: fc's column
                    // order guarantees it is the column's previous element
                    // (already computed) — colFlag[col] is its flag.
                    f = colFlag[col];
                } else {
                    f = 1; // nothing below
                }
            } else {
                const auto [bb, bw, bl, bh] = reader.cellAt(lx, -1, lz);
                if (bb != BlockID::air() || otherLevel(bw, bl, bh) > 0) {
                    f = 0;
                } else if (kindLevel(bw, bl, bh) > 0) {
                    // The below chunk's top cell holds the kind: read the
                    // column's topmost fluid cell's flag (topFlags).
                    const auto fit = topFlags.find({cp.x, cp.y - 1, cp.z});
                    f = (fit == topFlags.end()) ? 0 : fit->second[col];
                } else {
                    f = 1;
                }
            }
            flags.push_back(f);
            colFlag[col] = f;
        }
        topFlags[cp] = colFlag; // whole copy; no-fluid columns stay 0

        // -- 2'. Per-cell rules (over fc only; all reads are snapshot ----
        // reads). The rule bodies are the old phase 2 verbatim, with the
        // falling flag read from flags[k] (parallel with fc) and the
        // proposals merged into the map.
        for (std::size_t k = 0; k < fc.size(); ++k) {
            const std::uint32_t i = fc[k];
            const int lx = localX(i);
            const int lz = localZ(i);
            const int ly = localY(i);
            const std::uint8_t L = levelOf(fluid[i]); // > 0 (fc is live)
            const BlockPos wp{ox + lx, oy + ly, oz + lz};

            if (flags[k]) {
                // The conveyor: the cell below takes this level (y
                // < 0 drains out of the world); this cell takes the
                // level of the cell above (air or a block above
                // reads no fluid — the chain top empties).
                if (wp.y > 0) {
                    propose(proposed, {wp.x, wp.y - 1, wp.z}, L);
                }
                const auto [ab, aw, al, ah] = reader.cellAt(lx, ly + 1, lz);
                (void)ab;
                propose(proposed, wp, kindLevel(aw, al, ah));
                continue;
            }

            // Resting: the cell below holds a block (or the other
            // kind) or a resting fluid (an empty cell below is the
            // falling case).
            const auto [bb, bw, bl, bh] = reader.cellAt(lx, ly - 1, lz);
            const std::uint8_t below = kindLevel(bw, bl, bh);
            if (bb != BlockID::air() || otherLevel(bw, bl, bh) > 0) {
                // Supported: spread to the horizontal neighbors.
                // Level 1 is the terminal film and does not spread.
                if (L > 1) {
                    const std::uint8_t next = L - 1;
                    for (const BlockPos& d : kHorizontal) {
                        const auto [tb, tw, tl, th] =
                            reader.cellAt(lx + d.x, ly, lz + d.z);
                        if (tb != BlockID::air() || otherLevel(tw, tl, th) > 0) {
                            continue; // a block (or the other kind)
                                       // stops the flow
                        }
                        if (kindLevel(tw, tl, th) >= next) {
                            continue; // already at/above the level
                        }
                        propose(proposed, {wp.x + d.x, wp.y, wp.z + d.z}, next);
                    }
                }
            } else if (below > 0) {
                // Resting on a same-kind fluid of level B. The
                // pre-FLUID-1 rules: a full cell on anything is
                // stable; on a FULL cell, refill to full; a
                // shallower flow is absorbed into the deeper pool;
                // an equal or DEEPER flow stays and RAISED the pool
                // to its own level — that raise was the one
                // unaccounted volume-gain path (a pool gaining
                // volume with no source), and FLUID-1 route A
                // deletes it, replacing the raise with a no-op
                // static deposition:
                //  - B == 8 (source back): the cell becomes full.
                //    A column backed by a source below refills to
                //    the top — this is what settles a broken lake
                //    floor (without it the refilled column
                //    oscillates level-7 / absorbed / level-7
                //    forever);
                //  - 0 < B < 8 and L < B (shallower flow on a
                //    deeper pool): ABSORBED — the cell is removed,
                //    the pool keeps B;
                //  - 0 < B < 8 and L >= B (equal or deeper flow):
                //    STATIC DEPOSITION — the cell stays, the pool
                //    keeps B, and nothing is written. The old rule
                //    raised the pool to L here; that raise is the
                //    only way a pool could gain volume with no
                //    source, so deleting it makes pool levels
                //    monotonically non-increasing per cell while
                //    keeping the deposition stable (a cliff-fed
                //    film cascade settles into a static film stack
                //    instead of oscillating — the FLUID-1 step-0
                //    probe's flip-trigger finding).
                if (L == kMaxFluidLevel) {
                    // Stable.
                } else if (below == kMaxFluidLevel) {
                    propose(proposed, wp, kMaxFluidLevel);
                } else if (L < below) {
                    propose(proposed, wp, 0); // absorbed
                }
                // else: L >= below — static deposition (no-op).
            } else {
                assert(false && "resting cell with empty cell below");
            }
        }
    }

    // -- 3. Apply the snapshot changes (proposed is pos-ordered; the ----
    // sort is kept = the old code's minimal diff).
    std::vector<Change> changes;
    changes.reserve(proposed.size());
    for (const auto& [pos, level] : proposed) {
        changes.push_back({pos, level});
    }
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
                                     const BlockRegistry& blocks,
                                     const std::set<ChunkPos>* flammable,
                                     bool honeyActive) {
    // Reaction first: it separates the kinds (a converted cell becomes a
    // block), so the per-kind scans that follow see 6-disjoint fluids.
    std::vector<BlockPos> changed = reactLavaWithWater(world, blocks);
    // Then the flamable burn (Phase 58, target Phase 64): it destroys
    // neighbor BLOCKS without touching the fluids, so its position
    // relative to the per-kind scans is behavior-neutral — the reaction
    // goes first so the converted cells (blocks now) are visible to it.
    // The burn target is the "fire" block whenever the registry provides
    // it (the game's init cross-check guarantees this whenever a flamable
    // exists); without it the Phase 58 air semantics stay (test
    // fixtures).
    const BlockID burnTarget =
        blocks.idForName("fire").valid() ? blocks.idForName("fire")
                                         : BlockID::air();
    for (const BlockPos& p :
         burnFlamableNeighbors(world, blocks, burnTarget, flammable)) {
        changed.push_back(p);
    }
    for (const FluidKind kind : {FluidKind::Water, FluidKind::Lava}) {
        for (const BlockPos& p : simulateType(world, kind)) {
            changed.push_back(p);
        }
    }
    // Honey last, and only on its gated ticks (FLUID-6: the 1/3-speed
    // high viscosity; water and lava above are unaffected by the gate).
    if (honeyActive) {
        for (const BlockPos& p :
             simulateType(world, FluidKind::Honey)) {
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
