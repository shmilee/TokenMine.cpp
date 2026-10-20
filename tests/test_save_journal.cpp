// Save journal (PIPE-2) tests: the World cell-mutation observer (the
// choke-point capture, incl. the setBlock displacement double report),
// the SaveJournal bookkeeping (load-time seeding via applyWorldEdits,
// unload eviction, the soft cap fallback), the journalDiff equivalence
// ORACLE against the legacy diffWorld (byte-identical for a mixed-edit
// fixture, parallel == serial above the P37 threshold), and the real-
// asset stair invariant the journal-derived stair array relies on.
//
// Not unit-tested here (Game-level, E2E save-round-trip covered): the
// observer installation in buildScene and the collectSave branch wiring
// (journal path vs legacy fallback vs --save-full-diff).
#include <gtest/gtest.h>

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "block/block_registry.hpp"
#include "core/asset_manager.hpp"
#include "core/save.hpp"
#include "world/biome.hpp"
#include "world/chunk.hpp"
#include "world/world.hpp"
#include "world/worldgen_config.hpp"
#include "world/world_generator.hpp"

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

// air (built-in) + the oracle fixture's blocks.
BlockRegistry makeBlocks() {
    BlockRegistry blocks;
    for (const char* name : {"stone", "dirt", "wood"}) {
        auto r = blocks.registerBlock(parseDef(cubeDef(name)));
        if (!r.ok()) {
            ADD_FAILURE() << "block '" << name << "': " << r.error().message;
        }
    }
    // Phase 94: the stair block (Shape::Stairs gates the per-instance
    // facing; the journal's stair derivation reads it back).
    auto s = blocks.registerBlock(parseDef(
        R"({"id":"stone_stairs","model":{"shape":"stairs","textures":{"top":"t","side":"t","bottom":"t"}}})"));
    if (!s.ok()) {
        ADD_FAILURE() << s.error().message;
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

// One generated chunk: stone below y=4; the (0, 0) chunk adds water L8
// at (0,5,0) + (1,5,0) and a wood cell at (8,5,8) (the burn target).
World::ChunkGenerator oracleGenerator(BlockID stone, BlockID wood) {
    return [stone, wood](Chunk& c, const ChunkPos& cp) {
        for (int y = 0; y < 4; ++y) {
            for (int z = 0; z < Chunk::kSize; ++z) {
                for (int x = 0; x < Chunk::kSize; ++x) {
                    c.set(x, y, z, stone);
                }
            }
        }
        if (cp.x == 0 && cp.z == 0) {
            c.setFluidAt(0, 5, 0, fluidCode(FluidKind::Water, kMaxFluidLevel));
            c.setFluidAt(1, 5, 0, fluidCode(FluidKind::Water, kMaxFluidLevel));
            c.set(8, 5, 8, wood);
        }
    };
}

// The oracle world: three chunks (the (0,0) special + two plain), the
// observer installed BEFORE any edit (the Game's buildScene order).
World makeOracleWorld(SaveJournal& journal, const BlockRegistry& blocks) {
    World world;
    const auto gen = oracleGenerator(blocks.idForName("stone"),
                                     blocks.idForName("wood"));
    world.setChunkGenerator(gen);
    world.setCellMutationObserver(journalObserver(journal));
    world.getOrCreateChunk({0, 0, 0});
    world.getOrCreateChunk({1, 0, 0});
    world.getOrCreateChunk({0, 0, 1});
    return world;
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

// Group 1: the choke-point capture — setBlock (value change vs no-change
// vs the displacement double report), setFluid (change vs no-op), the
// invalid calls stay silent.
TEST(SaveJournalTest, ObserverCapture) {
    const BlockRegistry blocks = makeBlocks();
    const BlockID stone = blocks.idForName("stone");
    std::vector<CellMutation> muts;
    World world;
    world.setCellMutationObserver([&muts](const CellMutation& m) {
        muts.push_back(m);
    });
    // A plain block set: the block field = the id, no fluid field.
    ASSERT_TRUE(world.setBlock(1, 2, 3, stone));
    ASSERT_EQ(muts.size(), 1u);
    EXPECT_EQ(muts[0].pos, (BlockPos {1, 2, 3}));
    EXPECT_EQ(muts[0].block, stone);
    EXPECT_FALSE(muts[0].fluidCode.has_value());
    // setBlock on the SAME id: still a success, but no block field (the
    // value did not change) — the journal entry is idempotent anyway.
    ASSERT_TRUE(world.setBlock(1, 2, 3, stone));
    ASSERT_EQ(muts.size(), 2u);
    EXPECT_FALSE(muts[1].block.valid());
    EXPECT_FALSE(muts[1].fluidCode.has_value());
    // Invalid y: silent failure.
    EXPECT_FALSE(world.setBlock(1, kWorldHeight, 3, stone));
    EXPECT_EQ(muts.size(), 2u);
    // The displacement trap: a fluid cell, then a block into it = ONE
    // mutation with BOTH fields (block + the drained level-0 fluid).
    ASSERT_TRUE(world.setFluid(5, 6, 7, FluidKind::Water, 8));
    ASSERT_EQ(muts.size(), 3u);
    EXPECT_EQ(muts[2].fluidCode.value(),
              fluidCode(FluidKind::Water, 8));
    EXPECT_FALSE(muts[2].block.valid());
    ASSERT_TRUE(world.setBlock(5, 6, 7, stone));
    ASSERT_EQ(muts.size(), 4u);
    EXPECT_EQ(muts[3].block, stone);
    ASSERT_TRUE(muts[3].fluidCode.has_value());
    EXPECT_EQ(muts[3].fluidCode.value(), 0); // the drain double report
    // An AIR setBlock on a fluid cell: no block field (value unchanged),
    // the drain IS reported.
    ASSERT_TRUE(world.setFluid(9, 9, 9, FluidKind::Lava, 4));
    ASSERT_TRUE(world.setBlock(9, 9, 9, BlockID::air()));
    ASSERT_EQ(muts.size(), 6u);
    EXPECT_FALSE(muts[5].block.valid());
    EXPECT_EQ(muts[5].fluidCode.value(), 0);
    // setFluid: a level change reports the AFTER code; an unchanged
    // level is a silent no-op; a level-0 drain reports 0; a missing
    // chunk is a silent failure.
    EXPECT_FALSE(world.setFluid(1000, 5, 1000, FluidKind::Water, 8));
    EXPECT_EQ(muts.size(), 6u);
    world.getOrCreateChunk({1, 1, 1}); // the setFluid below needs a chunk
    ASSERT_TRUE(world.setFluid(20, 20, 20, FluidKind::Water, 8));
    EXPECT_FALSE(world.setFluid(20, 20, 20, FluidKind::Water, 8));
    ASSERT_EQ(muts.size(), 7u);
    ASSERT_TRUE(world.setFluid(20, 20, 20, FluidKind::Water, 3));
    EXPECT_EQ(muts[7].fluidCode.value(), fluidCode(FluidKind::Water, 3));
    ASSERT_TRUE(world.setFluid(20, 20, 20, FluidKind::Water, 0));
    EXPECT_EQ(muts[8].fluidCode.value(), 0);
}

// Group 2: the load-time auto-seeding — the observer installed before
// applyWorldEdits (the Game's buildScene order) records every applied
// edit; the level-0-on-dry skip applies NO setFluid and stays silent.
TEST(SaveJournalTest, LoadReplaySeedsTheJournal) {
    const BlockRegistry blocks = makeBlocks();
    const BlockID dirt = blocks.idForName("dirt");
    SaveJournal journal;
    World world = makeOracleWorld(journal, blocks);
    SaveGame save;
    save.blockEdits.push_back({BlockPos {2, 5, 0}, "dirt"});
    save.fluidEdits.push_back({BlockPos {0, 5, 0}, FluidKind::Water, 0});
    // A level-0 edit on a FRESH DRY cell: applyWorldEdits skips it
    // (no setFluid call -> no mutation -> no journal entry).
    save.fluidEdits.push_back({BlockPos {5, 5, 0}, FluidKind::Water, 0});
    ASSERT_TRUE(applyWorldEdits(save, blocks, world).ok());
    EXPECT_EQ(journal.entries, 2u);
    EXPECT_EQ(journal.blocks.at({0, 0}).count({2, 5, 0}), 1u);
    EXPECT_EQ(journal.fluids.at({0, 0}).count({0, 5, 0}), 1u);
    EXPECT_TRUE(journal.fluids.at({0, 0}).count({5, 5, 0}) == 0);
    // The live world now differs from generation exactly at the applied
    // edits (the drain + the placement; the skipped one is a no-op).
    EXPECT_EQ(world.getFluid(0, 5, 0), 0);
    EXPECT_EQ(world.getBlock(2, 5, 0), dirt);
}

// Group 3: the unload eviction — a column that leaves the world drops
// its entries (both tables) and the total count stays bounded; re-
// recording after eviction works; negative columns use floorDiv.
TEST(SaveJournalTest, UnloadEviction) {
    SaveJournal j;
    ASSERT_FALSE(j.recordBlock({1, 2, 3}));   // column (0, 0)
    ASSERT_FALSE(j.recordBlock({20, 5, 1}));  // column (1, 0)
    ASSERT_FALSE(j.recordFluid({3, 3, 3}));   // column (0, 0)
    EXPECT_EQ(j.entries, 3u);
    j.evictColumn({0, 0});
    EXPECT_EQ(j.entries, 1u);
    EXPECT_EQ(j.blocks.count({0, 0}), 0u);
    EXPECT_EQ(j.fluids.count({0, 0}), 0u);
    EXPECT_EQ(j.blocks.at({1, 0}).count({20, 5, 1}), 1u);
    // Evicting an unrecorded column is a no-op.
    j.evictColumn({9, 9});
    EXPECT_EQ(j.entries, 1u);
    // A column that re-enters (fresh streamed terrain) records anew.
    ASSERT_FALSE(j.recordBlock({5, 5, 5}));
    EXPECT_EQ(j.entries, 2u);
    // Negative coordinates: floorDiv's column (x = -1 -> column -1).
    SaveJournal j2;
    ASSERT_FALSE(j2.recordBlock({-1, 0, -1}));
    EXPECT_EQ(j2.blocks.count({-1, -1}), 1u);
    j2.evictColumn({-1, -1});
    EXPECT_EQ(j2.entries, 0u);
}

// Group 4: the EQUIVALENCE ORACLE (the core) — a fixture world with
// mixed modifications (same-value revert / bucket pour / drain /
// displacement / burn / stair placement+rotation / a far column that is
// edited then UNLOADED) — journalDiff's output is byte-identical to
// diffWorld's, and both match the hand-pinned expectation.
TEST(SaveJournalTest, JournalDiffMatchesDiffWorld) {
    const BlockRegistry blocks = makeBlocks();
    const BlockID stone = blocks.idForName("stone");
    const BlockID dirt = blocks.idForName("dirt");
    const BlockID wood = blocks.idForName("wood");
    const BlockID stairs = blocks.idForName("stone_stairs");
    const auto gen = oracleGenerator(stone, wood);
    SaveJournal journal;
    World world = makeOracleWorld(journal, blocks);
    // 1) Same-value revert: dig the stone, re-place the FRESH value —
    //    the journal entry exists, live == fresh, NO edit.
    ASSERT_TRUE(world.setBlock(0, 0, 0, BlockID::air()));
    ASSERT_TRUE(world.setBlock(0, 0, 0, stone));
    // 2) Dig (a fresh stone cell -> air).
    ASSERT_TRUE(world.setBlock(8, 0, 8, BlockID::air()));
    // 3) Place (a fresh air cell -> dirt).
    ASSERT_TRUE(world.setBlock(4, 5, 4, dirt));
    // 4) Bucket pour (fresh dry cell -> water L8).
    ASSERT_TRUE(world.setFluid(10, 5, 10, FluidKind::Water, 8));
    // 5) Drain (a fresh water cell -> level 0: the fluidKindOf(0)=Water
    //    special case).
    ASSERT_TRUE(world.setFluid(0, 5, 0, FluidKind::Water, 0));
    // 6) Displacement (a fresh water cell -> block: the double report).
    ASSERT_TRUE(world.setBlock(1, 5, 0, dirt));
    // 7) Burn cascade (a fresh wood cell -> air, the P58 burn target).
    ASSERT_TRUE(world.setBlock(8, 5, 8, BlockID::air()));
    // 8) Stair placement + rotation (the facing is per-instance state —
    //    it is NOT in the block/fluid diff; the block edit is).
    ASSERT_TRUE(world.setBlock(2, 5, 2, stairs));
    ASSERT_TRUE(world.setStairFacing(2, 5, 2, 2));
    // 9) A FAR column that is edited and then UNLOADED (the chunks are
    //    gone; the journal is NOT evicted here — journalDiff's
    //    "existing chunks" filter must drop it on its own). Full column
    //    first (the ensureColumn invariant — a column always holds all
    //    its chunks).
    for (int cy = 0; cy * 16 < kWorldHeight; ++cy) {
        world.getOrCreateChunk({5, cy, 5});
    }
    ASSERT_TRUE(world.setBlock(5 * 16 + 8, 0, 5 * 16 + 8, dirt));
    for (int cy = 0; cy * 16 < kWorldHeight; ++cy) {
        ASSERT_TRUE(world.eraseChunk({5, cy, 5}));
    }
    // The hand-pinned expectation (both tables, position-sorted).
    const std::vector<SavedBlockEdit> expectBlocks = {
        {BlockPos {1, 5, 0}, "dirt"},
        {BlockPos {2, 5, 2}, "stone_stairs"},
        {BlockPos {4, 5, 4}, "dirt"},
        {BlockPos {8, 0, 8}, "air"},
        {BlockPos {8, 5, 8}, "air"},
    };
    const std::vector<SavedFluidEdit> expectFluids = {
        {BlockPos {0, 5, 0}, FluidKind::Water, 0},
        {BlockPos {1, 5, 0}, FluidKind::Water, 0},
        {BlockPos {10, 5, 10}, FluidKind::Water, 8},
    };
    std::vector<SavedBlockEdit> fullBlocks;
    std::vector<SavedFluidEdit> fullFluids;
    ASSERT_TRUE(diffWorld(world, gen, blocks, fullBlocks, fullFluids).ok());
    std::vector<SavedBlockEdit> journalBlocks;
    std::vector<SavedFluidEdit> journalFluids;
    ASSERT_TRUE(journalDiff(world, gen, blocks, journal.blocks,
                            journal.fluids, journalBlocks, journalFluids)
                    .ok());
    // Both paths: the exact expectation...
    EXPECT_EQ(fullBlocks.size(), expectBlocks.size());
    EXPECT_TRUE(blockEditsEqual(fullBlocks, expectBlocks));
    EXPECT_EQ(fullFluids.size(), expectFluids.size());
    EXPECT_TRUE(fluidEditsEqual(fullFluids, expectFluids));
    // ...and byte-identical to each other.
    ASSERT_EQ(journalBlocks.size(), expectBlocks.size());
    for (std::size_t i = 0; i < journalBlocks.size(); ++i) {
        EXPECT_EQ(journalBlocks[i].pos, expectBlocks[i].pos);
        EXPECT_EQ(journalBlocks[i].blockName, expectBlocks[i].blockName);
    }
    ASSERT_EQ(journalFluids.size(), expectFluids.size());
    for (std::size_t i = 0; i < journalFluids.size(); ++i) {
        EXPECT_EQ(journalFluids[i].pos, expectFluids[i].pos);
        EXPECT_EQ(journalFluids[i].kind, expectFluids[i].kind);
        EXPECT_EQ(journalFluids[i].level, expectFluids[i].level);
    }
    // After the Game's unload eviction the journal path still matches.
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dz = -1; dz <= 1; ++dz) {
            journal.evictColumn({5 + dx, 5 + dz});
        }
    }
    std::vector<SavedBlockEdit> evBlocks;
    std::vector<SavedFluidEdit> evFluids;
    ASSERT_TRUE(journalDiff(world, gen, blocks, journal.blocks,
                            journal.fluids, evBlocks, evFluids)
                    .ok());
    EXPECT_EQ(evBlocks.size(), expectBlocks.size());
    EXPECT_TRUE(blockEditsEqual(evBlocks, expectBlocks));
    EXPECT_EQ(evFluids.size(), expectFluids.size());
    EXPECT_TRUE(fluidEditsEqual(evFluids, expectFluids));
}

// Group 5: the soft cap — the journal stays valid at exactly
// kSaveJournalMaxEntries, the NEXT record crosses it (returns true once,
// invalidates), and further records are no-ops (the exit save then takes
// the legacy fallback).
TEST(SaveJournalTest, SoftCapFallback) {
    SaveJournal j;
    // 8 columns x 32768 cells (a full column) = exactly the cap.
    for (int col = 0; col < 8; ++col) {
        for (int y = 0; y < kWorldHeight; ++y) {
            for (int z = 0; z < kChunkSize; ++z) {
                for (int x = 0; x < kChunkSize; ++x) {
                    ASSERT_FALSE(j.recordBlock({col * kChunkSize + x, y, z}))
                        << "col " << col << " y " << y << " z " << z;
                }
            }
        }
    }
    EXPECT_EQ(j.entries, kSaveJournalMaxEntries);
    EXPECT_TRUE(j.valid);
    // The crossing record: true exactly once, the journal invalidates.
    ASSERT_TRUE(j.recordBlock({1000 * kChunkSize, 0, 0}));
    EXPECT_FALSE(j.valid);
    EXPECT_EQ(j.entries, kSaveJournalMaxEntries + 1);
    // Further records are silent no-ops (the count is frozen).
    EXPECT_FALSE(j.recordBlock({1000 * kChunkSize, 1, 0}));
    EXPECT_FALSE(j.recordFluid({1000 * kChunkSize, 2, 0}));
    EXPECT_EQ(j.entries, kSaveJournalMaxEntries + 1);
    // Eviction still accounts (the count may shrink after invalidation).
    j.evictColumn({0, 0});
    EXPECT_EQ(j.entries,
              kSaveJournalMaxEntries + 1 -
                  kWorldHeight * kChunkSize * kChunkSize);
}

// Group 6: the real-asset stair invariant — worldgen places NO stair-
// shaped block, so the journal-derived stair array is complete (every
// stair cell was block-edited this session). Same content-pin family as
// the P55/P80 tests: run from the repository root.
TEST(SaveJournalTest, RealWorldgenPlacesNoStairBlock) {
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    BlockRegistry blocks;
    ASSERT_TRUE(blocks.loadDirectory(assets, "data/blocks").ok());
    BiomeRegistry biomes;
    ASSERT_TRUE(biomes.loadDirectory(assets, "data/biomes").ok());
    auto params = WorldgenParams::load(assets);
    ASSERT_TRUE(params.ok()) << params.error().message;
    auto gen = WorldGenerator::create(7, *params, biomes, blocks);
    ASSERT_TRUE(gen.ok()) << gen.error().message;
    WorldGenerator generator = std::move(*gen);
    // A 3x3 column region, all 8 layers (72 chunks) — the stair-shaped
    // cell count must be ZERO (and the region must actually contain
    // terrain, so the scan is not vacuous).
    std::size_t seenCells = 0;
    std::size_t stairCells = 0;
    for (int cx = 0; cx < 3; ++cx) {
        for (int cz = 0; cz < 3; ++cz) {
            for (int cy = 0; cy * kChunkSize < kWorldHeight; ++cy) {
                Chunk chunk;
                generator.generateChunk(chunk, {cx, cy, cz});
                for (int y = 0; y < kChunkSize; ++y) {
                    for (int z = 0; z < kChunkSize; ++z) {
                        for (int x = 0; x < kChunkSize; ++x) {
                            const BlockID id = chunk.get(x, y, z);
                            if (id == BlockID::air()) {
                                continue;
                            }
                            ++seenCells;
                            if (blocks.at(id).model.shape ==
                                BlockModel::Shape::Stairs) {
                                ++stairCells;
                            }
                        }
                    }
                }
            }
        }
    }
    ASSERT_GT(seenCells, 1000u) << "the region is unexpectedly empty";
    EXPECT_EQ(stairCells, 0u);
}

// Group 7: parallel == serial above the P37 threshold — 16x17 = 272
// chunks > kSaveDiffParallelMinChunks (256), one edit per chunk: the
// throwaway-pool path (one chunk per job) must be byte-identical to the
// serial loop.
TEST(SaveJournalTest, ParallelJournalDiffMatchesSerial) {
    const BlockRegistry blocks = makeBlocks();
    const BlockID stone = blocks.idForName("stone");
    const BlockID dirt = blocks.idForName("dirt");
    const auto gen = oracleGenerator(stone, blocks.idForName("wood"));
    // 16 x 17 = 272 chunks, all layer 0 (the edits stay in one chunk
    // each -> K = 272 > 256).
    World world;
    world.setChunkGenerator(gen);
    for (int x = 0; x < 16; ++x) {
        for (int z = 0; z < 17; ++z) {
            world.getOrCreateChunk({x, 0, z});
        }
    }
    SaveJournal journal;
    world.setCellMutationObserver(journalObserver(journal));
    for (int x = 0; x < 16; ++x) {
        for (int z = 0; z < 17; ++z) {
            ASSERT_TRUE(world.setBlock(x * 16 + 8, 0, z * 16 + 8, dirt));
        }
    }
    // A fluid edit in one of the chunks (the (0,0) special water cell).
    ASSERT_TRUE(world.setFluid(0, 5, 0, FluidKind::Water, 0));

    std::vector<SavedBlockEdit> serialBlocks;
    std::vector<SavedFluidEdit> serialFluids;
    ASSERT_TRUE(journalDiff(world, gen, blocks, journal.blocks,
                            journal.fluids, serialBlocks, serialFluids)
                    .ok());
    ASSERT_EQ(serialBlocks.size(), 272u);
    ASSERT_EQ(serialFluids.size(), 1u);

    std::vector<SavedBlockEdit> parallelBlocks;
    std::vector<SavedFluidEdit> parallelFluids;
    ASSERT_TRUE(journalDiff(world, gen, blocks, journal.blocks,
                            journal.fluids, parallelBlocks, parallelFluids,
                            kSaveDiffWorkers)
                    .ok());
    EXPECT_EQ(parallelBlocks.size(), serialBlocks.size());
    EXPECT_TRUE(blockEditsEqual(parallelBlocks, serialBlocks));
    EXPECT_EQ(parallelFluids.size(), serialFluids.size());
    EXPECT_TRUE(fluidEditsEqual(parallelFluids, serialFluids));
    // And the oracle holds at scale too.
    std::vector<SavedBlockEdit> fullBlocks;
    std::vector<SavedFluidEdit> fullFluids;
    ASSERT_TRUE(diffWorld(world, gen, blocks, fullBlocks, fullFluids).ok());
    EXPECT_TRUE(blockEditsEqual(fullBlocks, serialBlocks));
    EXPECT_TRUE(fluidEditsEqual(fullFluids, serialFluids));
}
