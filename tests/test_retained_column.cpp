// STRM-2 (retained fluid columns) tests: the unload-time column-diff
// snapshot (retainColumnDiff — the live-vs-fresh difference restricted to
// the column's journal cells, per-cell byte-identical to journalDiff's
// output), the exit-save merge (mergeRetained — the appended + re-sorted
// union of the diffed lists and the retained entries), and the full
// serialize -> parse -> applyWorldEdits round trip of a save whose edits
// carry the retained (unloaded far column) shape.
//
// Not unit-tested here (Game-level, E2E-gated): the unloadFarColumns
// snapshot-before-erase hook, the integrateStreamedColumns re-apply
// (blocks first, then fluids; the column stays unloadable — no
// m_editedColumns insert), and the collectSave merge in both paths
// (journal + the --save-full-diff oracle).
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "block/block_registry.hpp"
#include "core/json.hpp"
#include "core/save.hpp"
#include "core/status_effect.hpp"
#include "item/inventory.hpp"
#include "player/drown.hpp"
#include "world/chunk.hpp"
#include "world/coordinates.hpp"
#include "world/world.hpp"

using namespace mc;

namespace {

Json parseDef(const std::string& text) {
    auto r = Json::parse(text);
    if (!r.ok()) {
        ADD_FAILURE() << "parse failed: " << r.error().message;
        return Json::object();
    }
    return *r;
}

std::string cubeDef(const std::string& id) {
    return std::string("{\"id\":\"") + id +
           "\",\"model\":{\"textures\":{\"top\":\"t\",\"side\":\"t\","
           "\"bottom\":\"t\"}}}";
}

// air (built-in) + the fixture's blocks.
BlockRegistry makeBlocks() {
    BlockRegistry blocks;
    for (const char* name : {"stone", "dirt", "wood"}) {
        auto r = blocks.registerBlock(parseDef(cubeDef(name)));
        if (!r.ok()) {
            ADD_FAILURE() << "block '" << name << "': " << r.error().message;
        }
    }
    return blocks;
}

// The observer the Game installs (buildScene), test-local: forward the
// mutations to the journal. The Game's version adds the cap log.
CellMutationObserver journalObserver(SaveJournal& journal) {
    return [&journal](const CellMutation& mu) {
        if (mu.block.valid()) {
            journal.recordBlock(mu.pos);
        }
        if (mu.fluidCode.has_value()) {
            journal.recordFluid(mu.pos);
        }
    };
}

// Uniform terrain in EVERY chunk (a far column's fresh shape is the near
// column's): stone below y=4, water L8 at local (0,5,0) + (1,5,0), a wood
// cell at local (8,5,8) (the burn target).
World::ChunkGenerator uniformGenerator(BlockID stone, BlockID wood) {
    return [stone, wood](Chunk& c, const ChunkPos&) {
        for (int y = 0; y < 4; ++y) {
            for (int z = 0; z < Chunk::kSize; ++z) {
                for (int x = 0; x < Chunk::kSize; ++x) {
                    c.set(x, y, z, stone);
                }
            }
        }
        c.setFluidAt(0, 5, 0, fluidCode(FluidKind::Water, kMaxFluidLevel));
        c.setFluidAt(1, 5, 0, fluidCode(FluidKind::Water, kMaxFluidLevel));
        c.set(8, 5, 8, wood);
    };
}

// A full column (all eight chunks, the streaming loader's shape — the
// ensureColumn invariant makes "any" and "all" equivalent).
void makeFullColumn(World& world, int cx, int cz) {
    for (int cy = 0; cy * kChunkSize < kWorldHeight; ++cy) {
        world.getOrCreateChunk({cx, cy, cz});
    }
}

ColumnPos columnOf(const BlockPos& p) {
    return {floorDiv(p.x, kChunkSize), floorDiv(p.z, kChunkSize)};
}

// The edit structs carry no operator== — the byte comparisons the tests
// need (pos + name / pos + kind + level).
bool blockEditsEqual(const std::vector<SavedBlockEdit>& a,
                     const std::vector<SavedBlockEdit>& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].pos != b[i].pos || a[i].blockName != b[i].blockName) {
            return false;
        }
    }
    return true;
}

bool fluidEditsEqual(const std::vector<SavedFluidEdit>& a,
                     const std::vector<SavedFluidEdit>& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].pos != b[i].pos || a[i].kind != b[i].kind ||
            a[i].level != b[i].level) {
            return false;
        }
    }
    return true;
}

} // namespace

// The snapshot: exactly the journal cells whose LIVE value differs from
// generation, pos-sorted (both tables). A changed cell the journal never
// saw (the observer was not installed for it) must NOT appear (the
// restriction semantics), and a journaled same-value revert emits
// nothing.
TEST(RetainedColumnDiffTest, SnapshotsOnlyTheJournalCellsSorted) {
    const BlockRegistry blocks = makeBlocks();
    const BlockID stone = blocks.idForName("stone");
    const BlockID dirt = blocks.idForName("dirt");
    const BlockID wood = blocks.idForName("wood");
    const auto gen = uniformGenerator(stone, wood);

    World world;
    world.setChunkGenerator(gen);
    // Column (1,0): x in [16, 32), z in [0, 16).
    makeFullColumn(world, 1, 0);
    // Changed BEFORE the observer exists: not in the journal, and it must
    // not appear in the snapshot.
    ASSERT_TRUE(world.setBlock(28, 0, 0, dirt)); // fresh stone -> dirt
    SaveJournal journal;
    world.setCellMutationObserver(journalObserver(journal));
    // The journaled edits of the column:
    ASSERT_TRUE(world.setBlock(16, 3, 3, dirt));  // fresh stone -> dirt
    ASSERT_TRUE(world.setBlock(24, 5, 8, BlockID::air())); // fresh wood -> air
    ASSERT_TRUE(world.setBlock(20, 2, 0, dirt));  // fresh stone -> dirt ...
    ASSERT_TRUE(world.setBlock(20, 2, 0, stone)); // ... reverted to fresh
    ASSERT_TRUE(world.setFluid(17, 6, 2, FluidKind::Water, 3)); // dry -> L3
    ASSERT_TRUE(world.setFluid(20, 10, 4, FluidKind::Lava, 8)); // dry -> L8

    const RetainedColumnDiff diff = retainColumnDiff(
        world, gen, blocks, {1, 0},
        &journal.blocks.at({1, 0}), &journal.fluids.at({1, 0}));
    ASSERT_EQ(diff.blocks.size(), 2u);
    EXPECT_EQ(diff.blocks[0].pos, (BlockPos {16, 3, 3}));
    EXPECT_EQ(diff.blocks[0].blockName, "dirt");
    EXPECT_EQ(diff.blocks[1].pos, (BlockPos {24, 5, 8}));
    EXPECT_EQ(diff.blocks[1].blockName, "air");
    ASSERT_EQ(diff.fluids.size(), 2u);
    EXPECT_EQ(diff.fluids[0].pos, (BlockPos {17, 6, 2}));
    EXPECT_EQ(diff.fluids[0].kind, FluidKind::Water);
    EXPECT_EQ(diff.fluids[0].level, 3);
    EXPECT_EQ(diff.fluids[1].pos, (BlockPos {20, 10, 4}));
    EXPECT_EQ(diff.fluids[1].kind, FluidKind::Lava);
    EXPECT_EQ(diff.fluids[1].level, 8);
}

// A column whose journal cells all reverted to the fresh value yields an
// empty diff (both vectors) — the caller stores nothing for it.
TEST(RetainedColumnDiffTest, AllRevertsYieldAnEmptyDiff) {
    const BlockRegistry blocks = makeBlocks();
    const BlockID stone = blocks.idForName("stone");
    const BlockID dirt = blocks.idForName("dirt");
    const BlockID wood = blocks.idForName("wood");
    const auto gen = uniformGenerator(stone, wood);

    World world;
    world.setChunkGenerator(gen);
    makeFullColumn(world, 1, 0);
    SaveJournal journal;
    world.setCellMutationObserver(journalObserver(journal));
    ASSERT_TRUE(world.setBlock(16, 3, 3, dirt));
    ASSERT_TRUE(world.setBlock(16, 3, 3, stone)); // back to fresh
    ASSERT_TRUE(world.setFluid(20, 10, 4, FluidKind::Lava, 8));
    ASSERT_TRUE(world.setFluid(20, 10, 4, FluidKind::Lava, 0)); // drained
    ASSERT_EQ(journal.entries, 2u); // the cells ARE journaled ...

    const RetainedColumnDiff diff = retainColumnDiff(
        world, gen, blocks, {1, 0},
        &journal.blocks.at({1, 0}), &journal.fluids.at({1, 0}));
    // ... but the live state equals generation: nothing retained.
    EXPECT_TRUE(diff.blocks.empty());
    EXPECT_TRUE(diff.fluids.empty());
}

// Consistency with journalDiff: the per-column snapshot is entry-identical
// to journalDiff's output partitioned by column (the byte-identical-
// semantics discipline — the snapshot reuses journalDiffChunk), and a
// column with no journal entries at all (nullptr sets) snapshots empty.
TEST(RetainedColumnDiffTest, MatchesJournalDiffPerColumn) {
    const BlockRegistry blocks = makeBlocks();
    const BlockID stone = blocks.idForName("stone");
    const BlockID dirt = blocks.idForName("dirt");
    const BlockID wood = blocks.idForName("wood");
    const auto gen = uniformGenerator(stone, wood);

    World world;
    world.setChunkGenerator(gen);
    SaveJournal journal;
    world.setCellMutationObserver(journalObserver(journal));
    makeFullColumn(world, 0, 0);
    makeFullColumn(world, 1, 0);
    // An unedited column: the nullptr-sets path.
    makeFullColumn(world, 2, 0);
    // Edits in BOTH columns:
    ASSERT_TRUE(world.setBlock(0, 0, 0, BlockID::air())); // col (0,0): dig
    ASSERT_TRUE(world.setFluid(2, 5, 0, FluidKind::Water, 3)); // col (0,0)
    ASSERT_TRUE(world.setBlock(16, 3, 3, dirt)); // col (1,0): stone -> dirt
    ASSERT_TRUE(world.setBlock(24, 5, 8, BlockID::air())); // col (1,0): burn
    ASSERT_TRUE(world.setFluid(20, 10, 4, FluidKind::Lava, 8)); // col (1,0)

    std::vector<SavedBlockEdit> jdBlocks;
    std::vector<SavedFluidEdit> jdFluids;
    ASSERT_TRUE(journalDiff(world, gen, blocks, journal.blocks,
                            journal.fluids, jdBlocks, jdFluids)
                    .ok());
    // The hand-pinned expectation (both columns, position-sorted).
    const std::vector<SavedBlockEdit> expectBlocks = {
        {BlockPos {0, 0, 0}, "air"},
        {BlockPos {16, 3, 3}, "dirt"},
        {BlockPos {24, 5, 8}, "air"},
    };
    const std::vector<SavedFluidEdit> expectFluids = {
        {BlockPos {2, 5, 0}, FluidKind::Water, 3},
        {BlockPos {20, 10, 4}, FluidKind::Lava, 8},
    };
    EXPECT_TRUE(blockEditsEqual(jdBlocks, expectBlocks));
    EXPECT_TRUE(fluidEditsEqual(jdFluids, expectFluids));

    for (const ColumnPos col : {ColumnPos {0, 0}, ColumnPos {1, 0},
                                ColumnPos {2, 0}}) {
        // journalDiff's output restricted to the column:
        std::vector<SavedBlockEdit> partBlocks;
        std::vector<SavedFluidEdit> partFluids;
        for (const SavedBlockEdit& e : jdBlocks) {
            if (columnOf(e.pos) == col) {
                partBlocks.push_back(e);
            }
        }
        for (const SavedFluidEdit& e : jdFluids) {
            if (columnOf(e.pos) == col) {
                partFluids.push_back(e);
            }
        }
        const auto jb = journal.blocks.find(col);
        const auto jf = journal.fluids.find(col);
        const RetainedColumnDiff diff = retainColumnDiff(
            world, gen, blocks, col,
            jb == journal.blocks.end() ? nullptr : &jb->second,
            jf == journal.fluids.end() ? nullptr : &jf->second);
        EXPECT_TRUE(blockEditsEqual(diff.blocks, partBlocks))
            << "column (" << col.x << ", " << col.z << ")";
        EXPECT_TRUE(fluidEditsEqual(diff.fluids, partFluids))
            << "column (" << col.x << ", " << col.z << ")";
    }
}

// The exit-save merge: the journal output and the retained entries'
// positions INTERLEAVE — the merged lists are globally position-sorted
// with no duplicate cell (the one-of invariant's observable form).
TEST(RetainedColumnDiffTest, MergeSortsTheInterleavedUnion) {
    SaveGame save;
    // The journal-diff shape (position-sorted — the tail sort's output),
    // covering columns (0,0) and (2,0):
    save.blockEdits = {
        {BlockPos {0, 5, 0}, "dirt"},
        {BlockPos {32, 5, 0}, "air"},
    };
    save.fluidEdits = {{BlockPos {8, 5, 0}, FluidKind::Water, 3}};
    // Two UNLOADED columns interleaving the journal's positions:
    std::map<ColumnPos, RetainedColumnDiff> retained;
    retained[{0, 1}] =
        RetainedColumnDiff {
            std::vector<SavedBlockEdit> {{BlockPos {4, 2, 20}, "wood"}},
            std::vector<SavedFluidEdit> {}};
    retained[{1, 0}] =
        RetainedColumnDiff {
            std::vector<SavedBlockEdit> {{BlockPos {16, 3, 3}, "dirt"}},
            std::vector<SavedFluidEdit> {{BlockPos {20, 10, 4},
                                          FluidKind::Lava, 8}}};
    mergeRetained(save, retained);

    const std::vector<SavedBlockEdit> expectBlocks = {
        {BlockPos {0, 5, 0}, "dirt"},
        {BlockPos {4, 2, 20}, "wood"},
        {BlockPos {16, 3, 3}, "dirt"},
        {BlockPos {32, 5, 0}, "air"},
    };
    EXPECT_TRUE(blockEditsEqual(save.blockEdits, expectBlocks));
    const std::vector<SavedFluidEdit> expectFluids = {
        {BlockPos {8, 5, 0}, FluidKind::Water, 3},
        {BlockPos {20, 10, 4}, FluidKind::Lava, 8},
    };
    EXPECT_TRUE(fluidEditsEqual(save.fluidEdits, expectFluids));
}

// The no-op: an empty retained map leaves the diffed lists bit-identical
// (the pre-STRM-2 save shape).
TEST(RetainedColumnDiffTest, MergeOfAnEmptyMapIsANoOp) {
    SaveGame save;
    save.blockEdits = {{BlockPos {0, 5, 0}, "dirt"}};
    save.fluidEdits = {{BlockPos {8, 5, 0}, FluidKind::Water, 3}};
    const std::map<ColumnPos, RetainedColumnDiff> empty;
    mergeRetained(save, empty);
    ASSERT_EQ(save.blockEdits.size(), 1u);
    EXPECT_EQ(save.blockEdits[0].pos, (BlockPos {0, 5, 0}));
    EXPECT_EQ(save.blockEdits[0].blockName, "dirt");
    ASSERT_EQ(save.fluidEdits.size(), 1u);
    EXPECT_EQ(save.fluidEdits[0].pos, (BlockPos {8, 5, 0}));
    EXPECT_EQ(save.fluidEdits[0].kind, FluidKind::Water);
    EXPECT_EQ(save.fluidEdits[0].level, 3);
}

// The round trip: a save whose edits carry the retained (far column)
// shape — produced by retainColumnDiff + mergeRetained from a live world,
// covering the three legal cell-pair forms (a plain placement, a burn
// cascade = "air" + positive fluid in one cell, a displacement = non-air
// block + level-0 fluid in one cell) — serializes, parses (the strict
// format-19 validation), and applies to a FRESH world; the far column's
// cells match the source live state, and the column was topped up to
// complete (the existing applyWorldEdits mechanism).
TEST(RetainedColumnDiffTest, RetainedShapeSaveRoundTrips) {
    const BlockRegistry blocks = makeBlocks();
    const BlockID stone = blocks.idForName("stone");
    const BlockID dirt = blocks.idForName("dirt");
    const BlockID wood = blocks.idForName("wood");
    const auto gen = uniformGenerator(stone, wood);

    // The source: the far column (2,0) (x in [32, 48)) modified, then
    // "unloaded" (its diff retained).
    World source;
    source.setChunkGenerator(gen);
    SaveJournal journal;
    source.setCellMutationObserver(journalObserver(journal));
    makeFullColumn(source, 2, 0);
    // Plain placement (fresh stone -> dirt):
    ASSERT_TRUE(source.setBlock(32, 3, 3, dirt));
    // Burn cascade: the fresh wood cell -> air, then lava flows in (the
    // "air" block edit + positive fluid edit pair):
    ASSERT_TRUE(source.setBlock(40, 5, 8, BlockID::air()));
    ASSERT_TRUE(source.setFluid(40, 5, 8, FluidKind::Lava, 8));
    // Fresh-dry cell -> lava source:
    ASSERT_TRUE(source.setFluid(36, 10, 4, FluidKind::Lava, 8));
    // Displacement: a fresh water cell (L8) -> stone (the double report:
    // block edit + level-0 fluid edit):
    ASSERT_TRUE(source.setBlock(32, 5, 0, stone));

    const RetainedColumnDiff diff = retainColumnDiff(
        source, gen, blocks, {2, 0},
        &journal.blocks.at({2, 0}), &journal.fluids.at({2, 0}));
    const std::vector<SavedBlockEdit> expectBlocks = {
        {BlockPos {32, 3, 3}, "dirt"},
        {BlockPos {32, 5, 0}, "stone"},
        {BlockPos {40, 5, 8}, "air"},
    };
    const std::vector<SavedFluidEdit> expectFluids = {
        {BlockPos {32, 5, 0}, FluidKind::Water, 0},
        {BlockPos {36, 10, 4}, FluidKind::Lava, 8},
        {BlockPos {40, 5, 8}, FluidKind::Lava, 8},
    };
    EXPECT_TRUE(blockEditsEqual(diff.blocks, expectBlocks));
    EXPECT_TRUE(fluidEditsEqual(diff.fluids, expectFluids));

    // The game's collectSave shape: the journal output (empty here — the
    // column was unloaded, its journal entries evicted) + mergeRetained.
    SaveGame save;
    save.seed = 42;
    save.slots.resize(Inventory::kTotalSize); // all empty
    save.playerAbsorption = 0.0f; // format 12 (always written)
    save.playerOxygen = kOxygenMax; // format 17 (always written)
    std::map<ColumnPos, RetainedColumnDiff> retained;
    retained[{2, 0}] = diff;
    mergeRetained(save, retained);
    EXPECT_TRUE(blockEditsEqual(save.blockEdits, expectBlocks));
    EXPECT_TRUE(fluidEditsEqual(save.fluidEdits, expectFluids));

    ItemRegistry items; // the slots are empty: no lookups needed
    EffectRegistry effects; // no effects in the save
    auto serialized = serializeSave(save, items);
    ASSERT_TRUE(serialized.ok()) << serialized.error().message;
    auto parsed = parseSave(*serialized, items, blocks, effects, 42);
    ASSERT_TRUE(parsed.ok()) << parsed.error().message;
    EXPECT_TRUE(blockEditsEqual(parsed->blockEdits, save.blockEdits));
    EXPECT_TRUE(fluidEditsEqual(parsed->fluidEdits, save.fluidEdits));

    // The FRESH world (no chunks at all): the replay must create + top up
    // the far column and reproduce the source's live cells.
    World fresh;
    fresh.setChunkGenerator(gen);
    auto applied = applyWorldEdits(*parsed, blocks, fresh);
    ASSERT_TRUE(applied.ok()) << applied.error().message;
    EXPECT_TRUE(fresh.columnComplete(2, 0));
    for (int y = 0; y < kWorldHeight; ++y) {
        for (int z = 0; z < kChunkSize; ++z) {
            for (int x = 0; x < kChunkSize; ++x) {
                const int wx = 2 * kChunkSize + x;
                EXPECT_EQ(fresh.getBlock(wx, y, z), source.getBlock(wx, y, z))
                    << "block at " << wx << ", " << y << ", " << z;
                EXPECT_EQ(fresh.getFluid(wx, y, z), source.getFluid(wx, y, z))
                    << "fluid at " << wx << ", " << y << ", " << z;
            }
        }
    }
}
