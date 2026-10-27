// PIPE-3: the per-kind fluid cell lists (Chunk::fluidCells) + the World's
// per-kind ACTIVE chunk sets — the equivalence pins. The refactor replaces
// the per-tick 32K full scans (falling flags + per-cell rules, reaction +
// burn) with cell-list traversals, and the "all holding chunks scanned for
// a flag" walk source with the activity flags' set twin (every flag-set
// site inserts, the walk consumes, the flag double-check is the belt).
// The fluid state must stay bit-identical; these tests pin the new
// traversal sources' behavior: list maintenance (wet/drain/re-wet/
// compact/clear), the cross-chunk topFlags falling chain, the active set's
// lifecycle (drain -> one no-op walk -> cold; setBlock drain keeps the
// chunk active; unload purges; a stale entry is consumed without a walk),
// the reaction/burn collection over multiple chunks, the list's boundedness
// under churn, and a hand-computed two-tick reference.
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "block/block.hpp"
#include "block/block_registry.hpp"
#include "core/json.hpp"
#include "world/chunk.hpp"
#include "world/coordinates.hpp"
#include "world/fluid.hpp"
#include "world/world.hpp"

using namespace mc;

namespace {

const BlockID kStone {1}; // any non-air id: FluidSim only checks != air

// A stone floor slab at world y covering [x0,x1] x [z0,z1].
void floor(World& world, int x0, int x1, int z0, int z1, int y = 0) {
    for (int x = x0; x <= x1; ++x) {
        for (int z = z0; z <= z1; ++z) {
            world.setBlock(x, y, z, kStone);
        }
    }
}

// The empty registry: no fluid reference blocks, so the water-lava
// reaction pass is inert — the pure per-kind simulation.
const BlockRegistry& emptyBlocks() {
    static BlockRegistry reg;
    return reg;
}

// One fluid tick; the empty registry keeps reaction + burn inert.
std::vector<BlockPos> tick(World& world) {
    return FluidSim::tick(world, emptyBlocks());
}

Json parseDef(const std::string& text) {
    auto r = Json::parse(text);
    if (!r.ok()) {
        ADD_FAILURE() << "parse failed: " << r.error().message;
        return Json::object();
    }
    return *r;
}

std::string blockDef(const std::string& id, const std::string& extra = "") {
    return "{\"id\":\"" + id + "\",\"properties\":" + (extra.empty()
                                                           ? std::string("{}")
                                                           : std::string("{") +
                                                                 extra + "}") +
            ",\"model\":{\"textures\":{\"top\":\"t\",\"side\":\"t\","
            "\"bottom\":\"t\"}}}";
}

// A registry with the full Phase-51 fluid set: water, lava (with the
// water_reaction + emission), stone and obsidian.
BlockRegistry buildReactionRegistry() {
    BlockRegistry reg;
    (void)reg.registerBlock(parseDef(blockDef("stone")));
    (void)reg.registerBlock(parseDef(blockDef("obsidian")));
    (void)reg.registerBlock(
        parseDef(blockDef("water", R"("fluid":"water")")));
    (void)reg.registerBlock(parseDef(
        blockDef("lava", R"("fluid":"lava","light_emission":15,)"
                           R"("water_reaction":{"source":"obsidian",)"
                           R"("flowing":"stone"})")));
    return reg;
}

// A registry with a non-flamable stone and a flamable wheat (the burn
// pass's only registry dependency; no lava fluid block -> the water-lava
// reaction pass stays inert, the burn pass works off the raw lava bytes).
// No "fire" block: the burn target is air (the Phase 58 semantics).
BlockRegistry buildBurnRegistry() {
    BlockRegistry reg;
    (void)reg.registerBlock(parseDef(blockDef("stone")));
    (void)reg.registerBlock(parseDef(
        blockDef("wheat",
                 R"("solid":false,"opaque":false,"transparent":true,)"
                 R"("flamable":true)")));
    return reg;
}

// The chunk's live cell count of the kind (ground truth for the list's
// boundedness assertions).
int liveCells(const World& world, const ChunkPos& cp, FluidKind kind) {
    const Chunk* c = world.findChunk(cp);
    if (c == nullptr) {
        return 0;
    }
    const std::uint8_t* f = c->fluidData();
    int n = 0;
    for (int i = 0; i < Chunk::kVolume; ++i) {
        if (fluidKindOf(f[i]) == kind && fluidLevelOf(f[i]) > 0) {
            ++n;
        }
    }
    return n;
}

} // namespace

// List maintenance at the Chunk level: wet -> entry, same-kind level
// change -> no entry, drain -> stale entry stays (lazy), re-wet -> second
// entry, compact -> the single live entry, kind change -> the new kind's
// list gains an entry while the old one goes stale, clearFluid -> empty.
TEST(FluidCellListTest, CellListTracksWetDrainRewet) {
    Chunk c;
    EXPECT_TRUE(c.fluidCells(FluidKind::Water).empty());
    EXPECT_TRUE(c.fluidCells(FluidKind::Lava).empty());
    const std::uint32_t i = static_cast<std::uint32_t>(c.index(1, 2, 3));
    // 0 -> n: an entry.
    c.setFluidAt(1, 2, 3, fluidCode(FluidKind::Water, 5));
    ASSERT_EQ(c.fluidCells(FluidKind::Water).size(), 1u);
    EXPECT_EQ(c.fluidCells(FluidKind::Water)[0], i);
    // A same-kind level change: no entry (the cell already belongs).
    c.setFluidAt(1, 2, 3, fluidCode(FluidKind::Water, 3));
    EXPECT_EQ(c.fluidCells(FluidKind::Water).size(), 1u);
    // A drain: no removal (the stale entry stays until the walk).
    c.setFluidAt(1, 2, 3, 0);
    EXPECT_EQ(c.fluidCells(FluidKind::Water).size(), 1u);
    // A re-wet: a second entry (one stale, one live).
    c.setFluidAt(1, 2, 3, fluidCode(FluidKind::Water, 2));
    EXPECT_EQ(c.fluidCells(FluidKind::Water).size(), 2u);
    // A compact: the storage is replaced by the live list.
    c.compactFluidCells(FluidKind::Water, {i});
    ASSERT_EQ(c.fluidCells(FluidKind::Water).size(), 1u);
    EXPECT_EQ(c.fluidCells(FluidKind::Water)[0], i);
    // A kind change: the new kind's list gains an entry; the old kind's
    // entry goes stale (still present).
    c.setFluidAt(1, 2, 3, fluidCode(FluidKind::Lava, 4));
    ASSERT_EQ(c.fluidCells(FluidKind::Lava).size(), 1u);
    EXPECT_EQ(c.fluidCells(FluidKind::Lava)[0], i);
    EXPECT_EQ(c.fluidCells(FluidKind::Water).size(), 1u);
    // clearFluid empties the lists with the fluid array.
    c.setFluidAt(4, 5, 6, fluidCode(FluidKind::Lava, 1));
    EXPECT_EQ(c.fluidCells(FluidKind::Lava).size(), 2u);
    c.clearFluid();
    EXPECT_TRUE(c.fluidCells(FluidKind::Water).empty());
    EXPECT_TRUE(c.fluidCells(FluidKind::Lava).empty());
    EXPECT_EQ(c.fluidAt(1, 2, 3), 0);
    EXPECT_EQ(c.fluidAt(4, 5, 6), 0);
}

// The cross-chunk falling chain over the topFlags path: a 2-cell chain
// straddling the y=15/16 boundary, the lower chunk's top cell unsupported.
// The UPPER cell's falling flag is read from the below chunk's topFlags
// (the old fallingByChunk's top row) — if that read were missing/zero the
// upper cell would be "resting on same-kind fluid" (static deposition,
// no change) and the final state would differ.
TEST(FluidCellListTest, TickColumnOrderAcrossChunkYBoundary) {
    World world;
    world.getOrCreateChunk({0, 0, 0});
    world.getOrCreateChunk({0, 1, 0});
    ASSERT_TRUE(world.setFluid(8, 15, 8, FluidKind::Water, 7));
    ASSERT_TRUE(world.setFluid(8, 16, 8, FluidKind::Water, 7));

    const std::vector<BlockPos> changed = tick(world);
    // The whole chain shifted down one cell across the boundary (both
    // cells conveyor: the lower over the void column, the upper over the
    // lower top cell's flag).
    EXPECT_EQ(world.getFluid(8, 14, 8), 7);
    EXPECT_EQ(world.getFluid(8, 15, 8), 7);
    EXPECT_EQ(world.getFluid(8, 16, 8), 0);
    ASSERT_EQ(changed.size(), 2u);

    // The chain keeps falling (the column is still unsupported).
    tick(world);
    EXPECT_EQ(world.getFluid(8, 13, 8), 7);
    EXPECT_EQ(world.getFluid(8, 14, 8), 7);
    EXPECT_EQ(world.getFluid(8, 15, 8), 0);
}

// The active set's drain lifecycle: a fully drained chunk (its last cell
// removed) is still in the active set (the drain's re-heat mirror), so the
// next tick walks it ONCE (a no-op: no live cells) and consumes it — the
// tick after, nothing is visited and both flags are false.
TEST(FluidCellListTest, DrainedChunkWalksOnceThenCold) {
    World world;
    world.getOrCreateChunk({0, 0, 0});
    floor(world, 0, 15, 0, 15, 0);
    // A level-1 film on the floor: terminal (no spread), one tick to cold.
    ASSERT_TRUE(world.setFluid(8, 1, 8, FluidKind::Water, 1));
    ASSERT_TRUE(tick(world).empty());
    const ChunkPos cp {0, 0, 0};
    const Chunk* c0 = world.findChunk(cp);
    ASSERT_NE(c0, nullptr);
    EXPECT_FALSE(c0->fluidHot(FluidKind::Water));
    EXPECT_EQ(world.fluidActiveChunks(FluidKind::Water).count(cp), 0u);

    // Drain the only cell: the chunk leaves the holder set but the active
    // set keeps it (the insert mirrors the drain's re-heat).
    ASSERT_TRUE(world.setFluid(8, 1, 8, FluidKind::Water, 0));
    EXPECT_EQ(world.fluidChunks(FluidKind::Water).count(cp), 0u);
    EXPECT_EQ(world.fluidActiveChunks(FluidKind::Water).count(cp), 1u);

    // The next tick walks it (a no-op) and consumes the entry.
    ASSERT_TRUE(tick(world).empty());
    EXPECT_EQ(world.fluidActiveChunks(FluidKind::Water).count(cp), 0u);
    const Chunk* c1 = world.findChunk(cp);
    ASSERT_NE(c1, nullptr);
    EXPECT_FALSE(c1->fluidHot(FluidKind::Water));
    EXPECT_FALSE(c1->fluidWalkPending(FluidKind::Water));

    // The tick after: zero visited (the active set is empty, the flags
    // false) — the chunk is cold.
    EXPECT_TRUE(tick(world).empty());
    EXPECT_TRUE(world.fluidActiveChunks(FluidKind::Water).empty());
}

// A setBlock into a fluid cell (the displacement drain) keeps the chunk
// active: the next tick walks it (a no-op — no live cells) and consumes.
TEST(FluidCellListTest, SetBlockDrainKeepsChunkActive) {
    World world;
    world.getOrCreateChunk({0, 0, 0});
    ASSERT_TRUE(world.setFluid(8, 1, 8, FluidKind::Water, kMaxFluidLevel));
    const ChunkPos cp {0, 0, 0};
    ASSERT_TRUE(world.setBlock(8, 1, 8, kStone));
    EXPECT_EQ(world.getFluid(8, 1, 8), 0);
    EXPECT_EQ(world.getBlock(8, 1, 8), kStone);
    EXPECT_EQ(world.fluidActiveChunks(FluidKind::Water).count(cp), 1u);
    ASSERT_TRUE(tick(world).empty());
    EXPECT_EQ(world.fluidActiveChunks(FluidKind::Water).count(cp), 0u);
}

// A chunk unload purges both maintenance sets: no stale active entry
// survives, the next tick visits nothing.
TEST(FluidCellListTest, UnloadPurgesActive) {
    World world;
    world.getOrCreateChunk({0, 0, 0});
    ASSERT_TRUE(world.setFluid(8, 8, 8, FluidKind::Water, kMaxFluidLevel));
    const ChunkPos cp {0, 0, 0};
    EXPECT_EQ(world.fluidActiveChunks(FluidKind::Water).count(cp), 1u);
    ASSERT_TRUE(world.eraseChunk(cp));
    EXPECT_TRUE(world.fluidActiveChunks(FluidKind::Water).empty());
    EXPECT_TRUE(world.fluidChunks(FluidKind::Water).empty());
    EXPECT_TRUE(tick(world).empty());
}

// The reaction and burn passes' collection over the cell lists: two
// scenes, both spanning the x=15/16 chunk seam — a lava source converted
// by a water source across the boundary (the reaction reads the water
// neighbor across the seam), and a wheat burned by a lava source across
// the seam (the flamable neighbor is read across the boundary). The exact
// final states pin the per-cell logic on the new traversal source.
TEST(FluidCellListTest, ReactionBurnScanCellListEquivalence) {
    // Reaction.
    {
        BlockRegistry blocks = buildReactionRegistry();
        World world;
        floor(world, 0, 31, 0, 15, 0); // spans both chunks
        ASSERT_TRUE(world.setFluid(15, 1, 8, FluidKind::Lava,
                                   kMaxFluidLevel));
        ASSERT_TRUE(world.setFluid(16, 1, 8, FluidKind::Water,
                                   kMaxFluidLevel));
        FluidSim::tick(world, blocks);
        // The lava cell (a level-8 water neighbor) -> obsidian; the water
        // is not consumed and spreads its level-7 ring (the seam cell is
        // now a block — the spread stops there).
        EXPECT_EQ(world.getBlock(15, 1, 8), blocks.idForName("obsidian"));
        EXPECT_EQ(world.getFluid(15, 1, 8), 0);
        EXPECT_EQ(world.getFluidOfKind(16, 1, 8, FluidKind::Water),
                  kMaxFluidLevel);
        EXPECT_EQ(world.getFluidOfKind(17, 1, 8, FluidKind::Water), 7);
        EXPECT_EQ(world.getFluidOfKind(16, 1, 7, FluidKind::Water), 7);
        EXPECT_EQ(world.getFluidOfKind(16, 1, 9, FluidKind::Water), 7);
        EXPECT_EQ(world.getFluid(14, 1, 8), 0); // the lava did not spread
                                                // (converted before its
                                                // simulation pass)
    }
    // Burn.
    {
        BlockRegistry blocks = buildBurnRegistry();
        const BlockID wheat = blocks.idForName("wheat");
        World world;
        floor(world, 0, 31, 0, 15, 0); // spans both chunks
        ASSERT_TRUE(world.setBlock(16, 1, 8, wheat));
        ASSERT_TRUE(world.setFluid(15, 1, 8, FluidKind::Lava,
                                   kMaxFluidLevel));
        const std::vector<BlockPos> changed = FluidSim::tick(world, blocks);
        // Burned to air (no fire block in the registry: the Phase 58
        // semantics); the lava rests and spreads its level-7 ring (into
        // the burned cell too — the burn applies before the simulation).
        EXPECT_EQ(world.getBlock(16, 1, 8), BlockID::air());
        EXPECT_NE(std::find(changed.begin(), changed.end(),
                            BlockPos {16, 1, 8}),
                  changed.end());
        EXPECT_EQ(world.getFluidOfKind(15, 1, 8, FluidKind::Lava),
                  kMaxFluidLevel);
        EXPECT_EQ(world.getFluidOfKind(14, 1, 8, FluidKind::Lava), 7);
        EXPECT_EQ(world.getFluidOfKind(15, 1, 7, FluidKind::Lava), 7);
        EXPECT_EQ(world.getFluidOfKind(15, 1, 9, FluidKind::Lava), 7);
        EXPECT_EQ(world.getFluidOfKind(16, 1, 8, FluidKind::Lava), 7);
    }
}

// Churn boundedness: a sealed cell oscillating water <-> lava every tick
// (the list's worst case: every write is a membership change that pushes
// an entry, every drain leaves a stale one). The walks' compaction keeps
// each kind's list within 2x its live cell count.
TEST(FluidCellListTest, ChurnListStaysBounded) {
    World world;
    world.getOrCreateChunk({0, 0, 0});
    floor(world, 7, 9, 7, 9, 0);
    // Seal the cell (8,1,8): its six neighbors are blocks (no spread, no
    // fall — the oscillation is pure kind-swapping).
    world.setBlock(8, 2, 8, kStone);
    for (int d : {7, 9}) {
        world.setBlock(d, 1, 8, kStone);
        world.setBlock(8, 1, d, kStone);
    }
    const ChunkPos cp {0, 0, 0};
    ASSERT_TRUE(world.setFluid(8, 1, 8, FluidKind::Water, kMaxFluidLevel));
    for (int t = 0; t < 200; ++t) {
        const FluidKind kind = (t % 2 == 0) ? FluidKind::Lava
                                            : FluidKind::Water;
        ASSERT_TRUE(world.setFluid(8, 1, 8, kind, kMaxFluidLevel));
        tick(world);
        // The cell holds exactly the kind just written, at full level.
        EXPECT_EQ(world.getFluidOfKind(8, 1, 8, kind), kMaxFluidLevel)
            << "tick " << t;
        const Chunk* c = world.findChunk(cp);
        ASSERT_NE(c, nullptr);
        const int liveW = liveCells(world, cp, FluidKind::Water);
        const int liveL = liveCells(world, cp, FluidKind::Lava);
        EXPECT_LE(c->fluidCells(FluidKind::Water).size(),
                  2u * static_cast<unsigned>(liveW))
            << "tick " << t;
        EXPECT_LE(c->fluidCells(FluidKind::Lava).size(),
                  2u * static_cast<unsigned>(liveL))
            << "tick " << t;
    }
}

// The hand-computed reference: a level-8 source at the center of a full
// floor. Tick 1: the four orthogonals take level 7. Tick 2: the eight
// Manhattan-2 cells take level 6 (the ring's spread — the center's
// level-7 re-proposal into the ring is a no-op max merge), and nothing
// beyond.
TEST(FluidCellListTest, TwoTicksBitIdenticalToReference) {
    World world;
    floor(world, 0, 15, 0, 15, 0);
    ASSERT_TRUE(world.setFluid(8, 1, 8, FluidKind::Water, kMaxFluidLevel));
    tick(world);
    EXPECT_EQ(world.getFluid(8, 1, 8), kMaxFluidLevel);
    EXPECT_EQ(world.getFluid(7, 1, 8), 7);
    EXPECT_EQ(world.getFluid(9, 1, 8), 7);
    EXPECT_EQ(world.getFluid(8, 1, 7), 7);
    EXPECT_EQ(world.getFluid(8, 1, 9), 7);
    EXPECT_EQ(world.getFluid(7, 1, 7), 0); // no diagonal spreading
    tick(world);
    EXPECT_EQ(world.getFluid(8, 1, 8), kMaxFluidLevel);
    EXPECT_EQ(world.getFluid(7, 1, 8), 7); // the ring keeps 7 (max merge)
    EXPECT_EQ(world.getFluid(9, 1, 8), 7);
    EXPECT_EQ(world.getFluid(8, 1, 7), 7);
    EXPECT_EQ(world.getFluid(8, 1, 9), 7);
    EXPECT_EQ(world.getFluid(6, 1, 8), 6);
    EXPECT_EQ(world.getFluid(10, 1, 8), 6);
    EXPECT_EQ(world.getFluid(8, 1, 6), 6);
    EXPECT_EQ(world.getFluid(8, 1, 10), 6);
    EXPECT_EQ(world.getFluid(7, 1, 7), 6);
    EXPECT_EQ(world.getFluid(7, 1, 9), 6);
    EXPECT_EQ(world.getFluid(9, 1, 7), 6);
    EXPECT_EQ(world.getFluid(9, 1, 9), 6);
    EXPECT_EQ(world.getFluid(5, 1, 8), 0); // nothing beyond Manhattan 2
}

// The belt: an active-set entry whose chunk flags are BOTH false (a stale
// entry) is consumed (erased) without the chunk being visited. The
// production insertion points pair every insert with a flag-set (the six
// sites are exhaustive over the flag-setting sites), so this state is
// constructed directly: the setBlock UNSTICK inserts the entry, then the
// flags are cleared out-of-band. The next tick must drop the entry and
// change nothing.
TEST(FluidCellListTest, StaleInsertConsumed) {
    World world;
    world.getOrCreateChunk({0, 0, 0});
    const ChunkPos cp {0, 0, 0};
    // A setBlock on a dry chunk inserts the target unconditionally (both
    // kinds) with the pending flags.
    ASSERT_TRUE(world.setBlock(8, 0, 8, kStone));
    EXPECT_EQ(world.fluidActiveChunks(FluidKind::Water).count(cp), 1u);
    Chunk* c = world.findChunkMut(cp);
    ASSERT_NE(c, nullptr);
    // Simulate the out-of-band consumption: the flags are cleared, the
    // entry remains.
    c->clearFluidHot(FluidKind::Water);
    c->clearFluidWalkPending(FluidKind::Water);
    // The next tick consumes the stale water entry (no walk: the double-
    // check fails) and the lava entry (its pending flag is real — the
    // walk is a no-op on the dry chunk). Nothing changes.
    ASSERT_TRUE(tick(world).empty());
    EXPECT_EQ(world.fluidActiveChunks(FluidKind::Water).count(cp), 0u);
    EXPECT_TRUE(world.fluidActiveChunks(FluidKind::Lava).empty());
}
