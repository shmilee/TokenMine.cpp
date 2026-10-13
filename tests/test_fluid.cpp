// FluidSim tests: the deterministic water simulation (snapshot-based tick:
// falling conveyor, horizontal spread with level decay, terminal level 1,
// merge/absorb on fluid, cross-chunk chains, equilibrium stability).
//
// The simulation is registry-free (it only reads the block/fluid arrays),
// so any non-air BlockID works as "solid" for the floors.
#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "block/block_registry.hpp"
#include "core/json.hpp"
#include "world/fluid.hpp"

using namespace mc;

namespace {

const BlockID kStone{1}; // any non-air id: FluidSim only checks != air

// A stone floor slab at world y covering [x0,x1] x [z0,z1].
void floor(World& world, int x0, int x1, int z0, int z1, int y = 0) {
    for (int x = x0; x <= x1; ++x) {
        for (int z = z0; z <= z1; ++z) {
            world.setBlock(x, y, z, kStone);
        }
    }
}

// The empty registry: no fluid reference blocks, so the water-lava
// reaction pass is inert — the pure per-kind simulation (the historical
// single-fluid behavior).
const BlockRegistry& emptyBlocks() {
    static BlockRegistry reg;
    return reg;
}

// Runs `n` ticks; returns the last tick's changed cells.
std::vector<BlockPos> runTicks(World& world, int n) {
    std::vector<BlockPos> changed;
    for (int i = 0; i < n; ++i) {
        changed = FluidSim::tick(world, emptyBlocks());
    }
    return changed;
}

// The fluid state of a 16x16x16 region (for determinism comparisons).
std::vector<std::pair<BlockPos, std::uint8_t>> snapshot(World& world) {
    std::vector<std::pair<BlockPos, std::uint8_t>> cells;
    for (int y = 0; y < kChunkSize; ++y) {
        for (int z = 0; z < kChunkSize; ++z) {
            for (int x = 0; x < kChunkSize; ++x) {
                const std::uint8_t level = world.getFluid(x, y, z);
                if (level > 0) {
                    cells.push_back({{x, y, z}, level});
                }
            }
        }
    }
    return cells;
}

// Builds the determinism scenario: a floor with a 1-block hole, a full cell
// above the hole, and a second source at the corner.
void buildScenario(World& world) {
    world.getOrCreateChunk({0, 0, 0});
    floor(world, 0, 15, 0, 15, 0);
    world.setBlock(8, 0, 8, BlockID::air()); // the hole (drains to the void)
    world.setFluid(8, 1, 8, FluidKind::Water, kMaxFluidLevel); // a full cell above the hole
    world.setFluid(3, 1, 3, FluidKind::Water, kMaxFluidLevel); // a source at the corner
    world.setFluid(12, 1, 12, FluidKind::Water, 5);            // a flowing cell mid-floor
}

} // namespace

TEST(FluidTest, FullCellRestsOnSolidAndSpreads) {
    World world;
    floor(world, 4, 12, 4, 12, 0);
    ASSERT_TRUE(world.setFluid(8, 1, 8, FluidKind::Water, kMaxFluidLevel));

    const std::vector<BlockPos> changed = runTicks(world, 1);
    // The source stays; the four orthogonal neighbors get level 7 (the
    // diagonal stays dry — no corner spreading).
    EXPECT_EQ(world.getFluid(8, 1, 8), kMaxFluidLevel);
    EXPECT_EQ(world.getFluid(7, 1, 8), 7);
    EXPECT_EQ(world.getFluid(9, 1, 8), 7);
    EXPECT_EQ(world.getFluid(8, 1, 7), 7);
    EXPECT_EQ(world.getFluid(8, 1, 9), 7);
    EXPECT_EQ(world.getFluid(7, 1, 7), 0);
    EXPECT_FALSE(changed.empty());

    runTicks(world, 1);
    // The level-7 ring spreads a 6 ring; the 7 ring keeps 7 (the source
    // re-spreads 7 into it — max merge, no oscillation).
    EXPECT_EQ(world.getFluid(6, 1, 8), 6);
    EXPECT_EQ(world.getFluid(7, 1, 8), 7);
    EXPECT_EQ(world.getFluid(7, 1, 7), 6);
}

TEST(FluidTest, LevelOneIsTerminal) {
    World world;
    floor(world, 4, 12, 4, 12, 0);
    ASSERT_TRUE(world.setFluid(8, 1, 8, FluidKind::Water, 1));
    const std::vector<BlockPos> changed = runTicks(world, 3);
    EXPECT_TRUE(changed.empty());
    EXPECT_EQ(world.getFluid(8, 1, 8), 1);
    EXPECT_EQ(world.getFluid(7, 1, 8), 0);
}

TEST(FluidTest, FlowingColumnFallsIntoHoleAndRests) {
    World world;
    floor(world, 6, 10, 6, 10, 0);
    floor(world, 6, 10, 6, 10, 1);
    world.setBlock(8, 1, 8, BlockID::air()); // the hole in the upper floor

    // A 2-cell flowing column above the hole.
    ASSERT_TRUE(world.setFluid(8, 2, 8, FluidKind::Water, 7));
    ASSERT_TRUE(world.setFluid(8, 3, 8, FluidKind::Water, 7));

    runTicks(world, 1);
    // The whole chain shifted down one cell (the top emptied).
    EXPECT_EQ(world.getFluid(8, 1, 8), 7); // the hole is filled
    EXPECT_EQ(world.getFluid(8, 2, 8), 7);
    EXPECT_EQ(world.getFluid(8, 3, 8), 0);

    // Equilibrium: the bottom rests on the floor (its spreads are all
    // blocked by the upper-floor stone), the top rests on it (equal level:
    // stays, nothing to raise).
    const std::vector<BlockPos> changed = runTicks(world, 3);
    EXPECT_TRUE(changed.empty());
    EXPECT_EQ(world.getFluid(8, 1, 8), 7);
    EXPECT_EQ(world.getFluid(8, 2, 8), 7);
}

TEST(FluidTest, FullCellFallsWhenUnsupported) {
    World world;
    floor(world, 6, 10, 6, 10, 0);
    world.setBlock(8, 0, 8, BlockID::air()); // the hole reaches the void

    ASSERT_TRUE(world.setFluid(8, 1, 8, FluidKind::Water, kMaxFluidLevel));
    runTicks(world, 1);
    // The full cell fell into the hole (unlike Minecraft, a source does not
    // float — this is what drains water into a broken lake floor).
    EXPECT_EQ(world.getFluid(8, 0, 8), kMaxFluidLevel);
    EXPECT_EQ(world.getFluid(8, 1, 8), 0);

    runTicks(world, 1);
    // ...and it drained out of the world through the void.
    EXPECT_EQ(world.getFluid(8, 0, 8), 0);
}

TEST(FluidTest, FullLakeIsStable) {
    World world;
    floor(world, 5, 11, 5, 11, 0);
    // A walled 2-deep basin (x,z 6..10) filled full.
    for (int y = 1; y <= 2; ++y) {
        for (int x = 6; x <= 10; ++x) {
            for (int z = 6; z <= 10; ++z) {
                ASSERT_TRUE(world.setFluid(x, y, z, FluidKind::Water, kMaxFluidLevel));
            }
        }
    }
    for (int y = 1; y <= 2; ++y) {
        for (int x = 5; x <= 11; ++x) {
            for (int z = 5; z <= 11; ++z) {
                if (x >= 6 && x <= 10 && z >= 6 && z <= 10) {
                    continue;
                }
                world.setBlock(x, y, z, kStone); // the walls
            }
        }
    }

    for (int i = 0; i < 5; ++i) {
        const std::vector<BlockPos> changed = FluidSim::tick(world, emptyBlocks());
        EXPECT_TRUE(changed.empty()) << "tick " << i;
    }
    // Every cell is still full.
    for (int y = 1; y <= 2; ++y) {
        for (int x = 6; x <= 10; ++x) {
            for (int z = 6; z <= 10; ++z) {
                EXPECT_EQ(world.getFluid(x, y, z), kMaxFluidLevel);
            }
        }
    }
}

TEST(FluidTest, ShallowFilmAbsorbedIntoDeeperPool) {
    World world;
    floor(world, 4, 12, 4, 12, 0);
    ASSERT_TRUE(world.setFluid(8, 1, 8, FluidKind::Water, 5)); // the pool (flowing, not full)
    ASSERT_TRUE(world.setFluid(8, 2, 8, FluidKind::Water, 3)); // a shallow film above it

    runTicks(world, 1);
    // The shallower film (3 < 5) is absorbed into the flowing pool; the
    // pool keeps its level. (A film resting on a FULL cell behaves
    // differently: it refills to full — see below.)
    EXPECT_EQ(world.getFluid(8, 2, 8), 0);
    EXPECT_EQ(world.getFluid(8, 1, 8), 5);
}

TEST(FluidTest, FilmAboveFullWaterRefillsToFull) {
    // Regression: after a broken lake floor, the fallen full cell sits in
    // the hole and the neighboring lake spreads a level-7 film above it.
    // A cell resting on a full cell must become full (a column backed by a
    // source below is full); otherwise the film oscillates
    // 7 -> absorbed -> 7 -> ... forever.
    World world;
    floor(world, 6, 10, 6, 10, 0);
    // Walls around the 1x1 hole, so the fallen cell's horizontal spread is
    // blocked and the column rule is isolated.
    for (int d : {7, 9}) {
        ASSERT_TRUE(world.setBlock(d, 1, 8, kStone));
        ASSERT_TRUE(world.setBlock(8, 1, d, kStone));
    }
    ASSERT_TRUE(world.setFluid(8, 1, 8, FluidKind::Water, kMaxFluidLevel)); // the fallen cell
    ASSERT_TRUE(world.setFluid(8, 2, 8, FluidKind::Water, 7));             // the neighbor film

    runTicks(world, 1);
    EXPECT_EQ(world.getFluid(8, 2, 8), kMaxFluidLevel); // refilled to full
    // True equilibrium, no oscillation.
    const std::vector<BlockPos> changed = runTicks(world, 20);
    EXPECT_TRUE(changed.empty());
    EXPECT_EQ(world.getFluid(8, 1, 8), kMaxFluidLevel);
    EXPECT_EQ(world.getFluid(8, 2, 8), kMaxFluidLevel);
}

TEST(FluidTest, BrokenLakeFloorRefillsAndSettles) {
    // A walled 1-deep lake on a two-slab floor (like the seed-29 shore);
    // the player breaks a floor block. The water falls into the hole and
    // the surrounding lake refills the column back to full. The tick
    // stream must end in a change-free equilibrium.
    World world;
    floor(world, 5, 11, 5, 11, 0);
    floor(world, 5, 11, 5, 11, 1); // the floor is two slabs thick
    for (int x = 6; x <= 10; ++x) {
        for (int z = 6; z <= 10; ++z) {
            ASSERT_TRUE(world.setFluid(x, 2, z, FluidKind::Water, kMaxFluidLevel));
        }
    }
    for (int x = 5; x <= 11; ++x) {
        for (int z = 5; z <= 11; ++z) {
            if (x >= 6 && x <= 10 && z >= 6 && z <= 10) {
                continue;
            }
            world.setBlock(x, 2, z, kStone); // the walls
        }
    }
    ASSERT_TRUE(world.setBlock(8, 1, 8, BlockID::air())); // the break

    std::vector<BlockPos> changed;
    for (int i = 0; i < 60; ++i) {
        changed = FluidSim::tick(world, emptyBlocks());
    }
    EXPECT_TRUE(changed.empty()); // equilibrium reached
    // The broken column refilled to full: the lake surface is restored.
    EXPECT_EQ(world.getFluid(8, 1, 8), kMaxFluidLevel); // the fallen water
    EXPECT_EQ(world.getFluid(8, 2, 8), kMaxFluidLevel); // the refilled top
    // The rest of the lake is untouched.
    for (int x = 6; x <= 10; ++x) {
        for (int z = 6; z <= 10; ++z) {
            if (x == 8 && z == 8) {
                continue;
            }
            EXPECT_EQ(world.getFluid(x, 2, z), kMaxFluidLevel)
                << x << "," << z;
        }
    }
}

TEST(FluidTest, FlowingRaisesPoolBelow) {
    World world;
    floor(world, 0, 15, 0, 15, 0);
    ASSERT_TRUE(world.setFluid(8, 1, 8, FluidKind::Water, 3)); // a shallow pool on the floor
    ASSERT_TRUE(world.setFluid(8, 2, 8, FluidKind::Water, 5)); // a deeper flow resting on it

    runTicks(world, 1);
    // The flow stays and raises the pool to its own level.
    EXPECT_EQ(world.getFluid(8, 2, 8), 5);
    EXPECT_EQ(world.getFluid(8, 1, 8), 5);

    // Run to equilibrium: the pool now spreads a 5,4,3,2,1 gradient along
    // the floor and settles (the level-1 ring is terminal).
    std::vector<BlockPos> changed;
    for (int i = 0; i < 40; ++i) {
        changed = FluidSim::tick(world, emptyBlocks());
    }
    EXPECT_TRUE(changed.empty());
    EXPECT_EQ(world.getFluid(8, 1, 8), 5);
    EXPECT_EQ(world.getFluid(7, 1, 8), 4);
    EXPECT_EQ(world.getFluid(6, 1, 8), 3);
    EXPECT_EQ(world.getFluid(5, 1, 8), 2);
    EXPECT_EQ(world.getFluid(4, 1, 8), 1);
}

TEST(FluidTest, DoesNotSpreadIntoBlocks) {
    World world;
    floor(world, 4, 12, 4, 12, 0);
    ASSERT_TRUE(world.setFluid(8, 1, 8, FluidKind::Water, kMaxFluidLevel));
    world.setBlock(9, 1, 8, kStone); // a wall to the east

    runTicks(world, 1);
    // The wall cell stays a block (fluid cannot occupy it); the other three
    // directions spread normally.
    EXPECT_EQ(world.getBlock(9, 1, 8), kStone);
    EXPECT_EQ(world.getFluid(9, 1, 8), 0);
    EXPECT_EQ(world.getFluid(7, 1, 8), 7);
    EXPECT_EQ(world.getFluid(8, 1, 7), 7);
    EXPECT_EQ(world.getFluid(8, 1, 9), 7);
}

TEST(FluidTest, WaterDrainsOutOfWorldAtBottom) {
    World world;
    world.getOrCreateChunk({0, 0, 0});
    ASSERT_TRUE(world.setFluid(8, 0, 8, FluidKind::Water, kMaxFluidLevel)); // at y=0, void below

    runTicks(world, 1);
    EXPECT_EQ(world.getFluid(8, 0, 8), 0);
}

TEST(FluidTest, FallingChainCrossesChunkBoundary) {
    World world;
    world.getOrCreateChunk({0, 0, 0});
    world.getOrCreateChunk({0, 1, 0});
    // A 2-cell falling chain straddling the y=15/16 chunk boundary.
    ASSERT_TRUE(world.setFluid(8, 15, 8, FluidKind::Water, 7));
    ASSERT_TRUE(world.setFluid(8, 16, 8, FluidKind::Water, 7));

    runTicks(world, 1);
    // The chain shifted down one cell across the boundary.
    EXPECT_EQ(world.getFluid(8, 14, 8), 7);
    EXPECT_EQ(world.getFluid(8, 15, 8), 7);
    EXPECT_EQ(world.getFluid(8, 16, 8), 0);
}

TEST(FluidTest, DeterministicAcrossRuns) {
    World a;
    buildScenario(a);
    for (int i = 0; i < 100; ++i) {
        FluidSim::tick(a, emptyBlocks());
    }
    const auto stateA = snapshot(a);

    World b;
    buildScenario(b);
    for (int i = 0; i < 100; ++i) {
        FluidSim::tick(b, emptyBlocks());
    }
    const auto stateB = snapshot(b);

    EXPECT_EQ(stateA, stateB);
    // The scenario actually moved water (the source spread, the hole drained
    // and refilled): the final state is not the initial one.
    EXPECT_GT(stateA.size(), 4u);
}

// -- Phase 51: two kinds (water and lava) + the water-lava reaction --------

namespace {

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

} // namespace

TEST(FluidKindTest, EncodingRoundTrip) {
    // Dry.
    EXPECT_EQ(fluidCode(FluidKind::Water, 0), 0);
    EXPECT_EQ(fluidCode(FluidKind::Lava, 0), 0);
    EXPECT_EQ(fluidLevelOf(0), 0);
    // Water 1..8, lava 1..8 (offset 9).
    for (std::uint8_t l = 1; l <= kMaxFluidLevel; ++l) {
        EXPECT_EQ(fluidLevelOf(fluidCode(FluidKind::Water, l)), l);
        EXPECT_EQ(fluidKindOf(fluidCode(FluidKind::Water, l)),
                  FluidKind::Water);
        EXPECT_EQ(fluidLevelOf(fluidCode(FluidKind::Lava, l)), l);
        EXPECT_EQ(fluidKindOf(fluidCode(FluidKind::Lava, l)), FluidKind::Lava);
    }
    EXPECT_EQ(fluidLevelOf(8), 8);   // water full
    EXPECT_EQ(fluidLevelOf(9), 1);   // lava film
    EXPECT_EQ(fluidLevelOf(16), 8);  // lava full
    EXPECT_STREQ(fluidKindName(FluidKind::Water), "water");
    EXPECT_STREQ(fluidKindName(FluidKind::Lava), "lava");
    EXPECT_EQ(fluidKindForName("water"), FluidKind::Water);
    EXPECT_EQ(fluidKindForName("lava"), FluidKind::Lava);
    EXPECT_FALSE(fluidKindForName("slime").has_value());
}

TEST(FluidTest, SetFluidReplacesOtherKind) {
    World world;
    world.getOrCreateChunk({0, 0, 0});
    ASSERT_TRUE(world.setFluid(8, 1, 8, FluidKind::Water, 5));
    // Placing lava into the water cell replaces it (one kind per cell).
    ASSERT_TRUE(world.setFluid(8, 1, 8, FluidKind::Lava, 3));
    EXPECT_EQ(world.getFluid(8, 1, 8), 3);
    EXPECT_EQ(world.getFluidKind(8, 1, 8), FluidKind::Lava);
    EXPECT_EQ(world.getFluidOfKind(8, 1, 8, FluidKind::Water), 0);
    EXPECT_EQ(world.getFluidOfKind(8, 1, 8, FluidKind::Lava), 3);
    // Clearing WATER (not present) is a no-op that leaves the lava.
    EXPECT_FALSE(world.setFluid(8, 1, 8, FluidKind::Water, 0));
    EXPECT_EQ(world.getFluidKind(8, 1, 8), FluidKind::Lava);
    // Same kind, same level: no change.
    EXPECT_FALSE(world.setFluid(8, 1, 8, FluidKind::Lava, 3));
    // Clearing the lava empties the cell.
    ASSERT_TRUE(world.setFluid(8, 1, 8, FluidKind::Lava, 0));
    EXPECT_FALSE(world.getFluidKind(8, 1, 8).has_value());
}

TEST(FluidTest, KindsSimulateIndependently) {
    World world;
    floor(world, 0, 15, 0, 15, 0);
    ASSERT_TRUE(world.setFluid(3, 1, 3, FluidKind::Water, kMaxFluidLevel));
    ASSERT_TRUE(world.setFluid(12, 1, 12, FluidKind::Lava, kMaxFluidLevel));

    runTicks(world, 1);
    // Each source spreads its own kind's level-7 ring; the other kind is
    // invisible to the scan.
    EXPECT_EQ(world.getFluidOfKind(3, 1, 3, FluidKind::Water),
              kMaxFluidLevel);
    EXPECT_EQ(world.getFluidOfKind(2, 1, 3, FluidKind::Water), 7);
    EXPECT_EQ(world.getFluidOfKind(4, 1, 3, FluidKind::Water), 7);
    EXPECT_EQ(world.getFluidOfKind(3, 1, 2, FluidKind::Water), 7);
    EXPECT_EQ(world.getFluidOfKind(3, 1, 4, FluidKind::Water), 7);
    EXPECT_EQ(world.getFluidOfKind(12, 1, 12, FluidKind::Lava),
              kMaxFluidLevel);
    EXPECT_EQ(world.getFluidOfKind(11, 1, 12, FluidKind::Lava), 7);
    EXPECT_EQ(world.getFluidOfKind(13, 1, 12, FluidKind::Lava), 7);
    EXPECT_EQ(world.getFluidOfKind(12, 1, 11, FluidKind::Lava), 7);
    EXPECT_EQ(world.getFluidOfKind(12, 1, 13, FluidKind::Lava), 7);
    // No cross-contamination: the other kind's ring cells hold nothing.
    EXPECT_EQ(world.getFluid(2, 1, 12), 0);
    EXPECT_EQ(world.getFluid(12, 1, 3), 0);
}

TEST(FluidTest, LavaBesideSourceWaterBecomesObsidian) {
    const BlockRegistry blocks = buildReactionRegistry();
    World world;
    floor(world, 4, 12, 4, 12, 0);
    ASSERT_TRUE(world.setFluid(8, 1, 8, FluidKind::Lava, kMaxFluidLevel));
    ASSERT_TRUE(world.setFluid(9, 1, 8, FluidKind::Water, kMaxFluidLevel));

    const std::vector<BlockPos> changed =
        FluidSim::tick(world, blocks);
    bool converted = std::any_of(changed.begin(), changed.end(),
                                 [](const BlockPos& p) {
                                     return p == BlockPos {8, 1, 8};
                                 });
    EXPECT_TRUE(converted);
    // The lava cell became obsidian; the water source is untouched.
    EXPECT_EQ(world.getBlock(8, 1, 8), blocks.idForName("obsidian"));
    EXPECT_EQ(world.getFluidOfKind(9, 1, 8, FluidKind::Water),
              kMaxFluidLevel);
    EXPECT_EQ(world.getFluidOfKind(8, 1, 8, FluidKind::Lava), 0);
}

TEST(FluidTest, LavaBesideFlowingWaterBecomesStone) {
    const BlockRegistry blocks = buildReactionRegistry();
    World world;
    floor(world, 4, 12, 4, 12, 0);
    ASSERT_TRUE(world.setFluid(8, 1, 8, FluidKind::Lava, kMaxFluidLevel));
    // A source two cells away: tick 1 spreads level 7 into (9,1,8) (no
    // reaction yet — the snapshot had no water neighbor), tick 2 reacts
    // with the flowing film.
    ASSERT_TRUE(world.setFluid(10, 1, 8, FluidKind::Water, kMaxFluidLevel));

    FluidSim::tick(world, blocks);
    EXPECT_EQ(world.getFluidOfKind(9, 1, 8, FluidKind::Water), 7);
    EXPECT_NE(world.getBlock(8, 1, 8), blocks.idForName("stone"));
    FluidSim::tick(world, blocks);
    EXPECT_EQ(world.getBlock(8, 1, 8), blocks.idForName("stone"));
    EXPECT_EQ(world.getFluidOfKind(8, 1, 8, FluidKind::Lava), 0);
}

TEST(FluidTest, SourceWaterNeighborWinsOverFlowing) {
    const BlockRegistry blocks = buildReactionRegistry();
    World world;
    floor(world, 4, 12, 4, 12, 0);
    ASSERT_TRUE(world.setFluid(8, 1, 8, FluidKind::Lava, 4)); // flowing lava
    ASSERT_TRUE(world.setFluid(7, 1, 8, FluidKind::Water, kMaxFluidLevel));
    ASSERT_TRUE(world.setFluid(9, 1, 8, FluidKind::Water, 3)); // flowing

    FluidSim::tick(world, blocks);
    // A source neighbor exists -> obsidian (even though a flowing
    // neighbor is closer in the fixed order).
    EXPECT_EQ(world.getBlock(8, 1, 8), blocks.idForName("obsidian"));
}

TEST(FluidTest, InertLavaDoesNotReact) {
    // A lava fluid block WITHOUT water_reaction is inert: adjacent water
    // never converts it. The kinds stay apart (the other kind blocks the
    // spread and supports).
    BlockRegistry blocks;
    (void)blocks.registerBlock(parseDef(blockDef("stone")));
    (void)blocks.registerBlock(
        parseDef(blockDef("water", R"("fluid":"water")")));
    (void)blocks.registerBlock(
        parseDef(blockDef("lava", R"("fluid":"lava")")));
    World world;
    floor(world, 4, 12, 4, 12, 0);
    ASSERT_TRUE(world.setFluid(8, 1, 8, FluidKind::Lava, kMaxFluidLevel));
    ASSERT_TRUE(world.setFluid(9, 1, 8, FluidKind::Water, kMaxFluidLevel));

    for (int i = 0; i < 20; ++i) {
        FluidSim::tick(world, blocks);
    }
    EXPECT_EQ(world.getFluidOfKind(8, 1, 8, FluidKind::Lava),
              kMaxFluidLevel);
    EXPECT_EQ(world.getFluidOfKind(9, 1, 8, FluidKind::Water),
              kMaxFluidLevel);
    EXPECT_EQ(world.getBlock(8, 1, 8), BlockID::air());
    // The water spread stops at the lava cell (an occupied cell).
    EXPECT_EQ(world.getFluidOfKind(8, 1, 8, FluidKind::Water), 0);
}

TEST(FluidTest, SettledWorldHasNoAdjacentKinds) {
    // After the reaction pass separates the kinds, a settled world
    // (changed == {}) has no lava cell 6-adjacent to a water cell.
    const BlockRegistry blocks = buildReactionRegistry();
    World world;
    floor(world, 0, 15, 0, 15, 0);
    ASSERT_TRUE(world.setFluid(8, 1, 8, FluidKind::Lava, kMaxFluidLevel));
    ASSERT_TRUE(world.setFluid(8, 1, 10, FluidKind::Water, kMaxFluidLevel));
    std::vector<BlockPos> changed;
    for (int i = 0; i < 120; ++i) {
        changed = FluidSim::tick(world, blocks);
        if (changed.empty()) {
            break;
        }
    }
    EXPECT_TRUE(changed.empty()); // settled
    // No lava cell is 6-adjacent (the reaction's neighborhood) to a water
    // cell; diagonal corner contact is a stable settled state.
    const BlockPos kSix[6] = {
        {1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}, {0, 1, 0}, {0, -1, 0},
    };
    for (int x = 0; x < kChunkSize; ++x) {
        for (int z = 0; z < kChunkSize; ++z) {
            if (world.getFluidOfKind(x, 1, z, FluidKind::Lava) == 0) {
                continue;
            }
            for (const BlockPos& d : kSix) {
                const int nx = x + d.x, ny = 1 + d.y, nz = z + d.z;
                if (nx < 0 || nx >= kChunkSize || nz < 0 || nz >= kChunkSize ||
                    ny < 0 || ny >= kWorldHeight) {
                    continue;
                }
                EXPECT_EQ(world.getFluidOfKind(nx, ny, nz, FluidKind::Water),
                          0)
                    << "adjacent water at " << nx << "," << ny << "," << nz;
            }
        }
    }
}

TEST(FluidTest, ReactionIsDeterministicAcrossRuns) {
    const BlockRegistry blocks = buildReactionRegistry();
    auto build = [](World& world) {
        floor(world, 0, 15, 0, 15, 0);
        (void)world.setFluid(8, 1, 8, FluidKind::Lava, kMaxFluidLevel);
        (void)world.setFluid(9, 1, 8, FluidKind::Lava, kMaxFluidLevel);
        (void)world.setFluid(10, 1, 8, FluidKind::Lava, kMaxFluidLevel);
        (void)world.setFluid(8, 2, 8, FluidKind::Lava, kMaxFluidLevel);
        (void)world.setFluid(8, 1, 11, FluidKind::Water, kMaxFluidLevel);
        (void)world.setFluid(10, 1, 11, FluidKind::Water, 5);
    };
    World a, b;
    build(a);
    build(b);
    for (int i = 0; i < 100; ++i) {
        FluidSim::tick(a, blocks);
        FluidSim::tick(b, blocks);
    }
    EXPECT_EQ(snapshot(a), snapshot(b));
    // The lava's z=9 spread row converted to stone the moment the water
    // front reached it — a wall between the two kinds. The lava ROW itself
    // (and the lava above it) is never 6-adjacent to water, so it
    // survives; the water diamond fills the z >= 10 side of the wall.
    EXPECT_EQ(a.getBlock(8, 1, 9), blocks.idForName("stone"));
    EXPECT_EQ(a.getBlock(9, 1, 9), blocks.idForName("stone"));
    EXPECT_EQ(a.getBlock(10, 1, 9), blocks.idForName("stone"));
    EXPECT_EQ(a.getFluidOfKind(8, 1, 8, FluidKind::Lava), kMaxFluidLevel);
    EXPECT_EQ(a.getFluidOfKind(9, 1, 8, FluidKind::Lava), kMaxFluidLevel);
    EXPECT_EQ(a.getFluidOfKind(10, 1, 8, FluidKind::Lava), kMaxFluidLevel);
    EXPECT_EQ(a.getFluidOfKind(8, 2, 8, FluidKind::Lava), kMaxFluidLevel);
    EXPECT_EQ(a.getFluidOfKind(8, 1, 10, FluidKind::Water), 7);
    EXPECT_EQ(a.getFluidOfKind(8, 1, 11, FluidKind::Water),
              kMaxFluidLevel);
}

// -- flamable burn pass (Phase 58, the fire system v2) ----------------------

// A registry with a non-flamable stone and a flamable wheat (the burn
// pass's only registry dependency; no lava fluid block -> the water-lava
// reaction pass stays inert, the burn pass works off the raw lava bytes).
BlockRegistry buildBurnRegistry() {
    BlockRegistry reg;
    (void)reg.registerBlock(parseDef(blockDef("stone")));
    (void)reg.registerBlock(parseDef(blockDef(
        "wheat",
        R"("solid":false,"opaque":false,"transparent":true,"flamable":true)")));
    return reg;
}

TEST(FluidBurnTest, LavaBurnsFlamableNeighbor) {
    World world;
    world.getOrCreateChunk({0, 0, 0});
    floor(world, 0, 15, 0, 15, 0);
    BlockRegistry blocks = buildBurnRegistry();
    const BlockID wheat = blocks.idForName("wheat");
    world.setBlock(8, 1, 8, wheat);                  // the crop
    world.setFluid(9, 1, 8, FluidKind::Lava, kMaxFluidLevel); // 6-adjacent
    std::vector<BlockPos> changed = FluidSim::tick(world, blocks);
    EXPECT_EQ(world.getBlock(8, 1, 8), BlockID::air()); // burned to air
    EXPECT_NE(std::find(changed.begin(), changed.end(), BlockPos{8, 1, 8}),
              changed.end());
    // The lava is not consumed by the burn.
    EXPECT_EQ(world.getFluidOfKind(9, 1, 8, FluidKind::Lava), kMaxFluidLevel);
}

// Phase 64: when the registry PROVIDES the "fire" block, the burn target
// is FIRE (not air) — the light source that burns out. The game's init
// cross-check guarantees the fire block whenever a flamable exists, so
// the in-game registry always burns to fire; the air fallback above stays
// for registries without it (the Phase 58 semantics).
TEST(FluidBurnTest, BurnsToFireWhenRegistryHasFire) {
    World world;
    world.getOrCreateChunk({0, 0, 0});
    floor(world, 0, 15, 0, 15, 0);
    BlockRegistry blocks = buildBurnRegistry();
    (void)blocks.registerBlock(parseDef(blockDef(
        "fire",
        R"("solid":false,"opaque":false,"transparent":true,"light_emission":15)")));
    const BlockID fire = blocks.idForName("fire");
    ASSERT_TRUE(fire.valid());
    world.setBlock(8, 1, 8, blocks.idForName("wheat"));
    world.setFluid(9, 1, 8, FluidKind::Lava, kMaxFluidLevel);
    std::vector<BlockPos> changed = FluidSim::tick(world, blocks);
    EXPECT_EQ(world.getBlock(8, 1, 8), fire); // burned to FIRE (Phase 64)
    EXPECT_NE(std::find(changed.begin(), changed.end(), BlockPos{8, 1, 8}),
              changed.end());
    // The fire is not flamable: a second tick does not re-burn it (the
    // cell stays fire — the burn path is inert on its own product).
    (void)FluidSim::tick(world, blocks);
    EXPECT_EQ(world.getBlock(8, 1, 8), fire);
}

TEST(FluidBurnTest, OnlyDirectNeighborsBurn) {
    World world;
    world.getOrCreateChunk({0, 0, 0});
    floor(world, 0, 15, 0, 15, 0);
    BlockRegistry blocks = buildBurnRegistry();
    const BlockID wheat = blocks.idForName("wheat");
    world.setBlock(8, 1, 8, wheat);  // diagonal to the lava
    world.setBlock(10, 1, 8, wheat); // two cells away (air in between)
    world.setFluid(9, 1, 9, FluidKind::Lava, kMaxFluidLevel);
    (void)FluidSim::tick(world, blocks);
    EXPECT_EQ(world.getBlock(8, 1, 8), wheat);  // diagonal: safe
    EXPECT_EQ(world.getBlock(10, 1, 8), wheat); // two away: safe (v1: no
                                                // fire spread over time)
}

TEST(FluidBurnTest, NonFlamableNeighborSurvives) {
    World world;
    world.getOrCreateChunk({0, 0, 0});
    floor(world, 0, 15, 0, 15, 0);
    BlockRegistry blocks = buildBurnRegistry();
    const BlockID stone = blocks.idForName("stone");
    world.setBlock(9, 1, 8, stone);
    world.setFluid(8, 1, 8, FluidKind::Lava, kMaxFluidLevel);
    (void)FluidSim::tick(world, blocks);
    EXPECT_EQ(world.getBlock(9, 1, 8), stone);   // the neighbor block
    EXPECT_EQ(world.getBlock(8, 0, 8), stone);   // the floor below
}

TEST(FluidBurnTest, InertWithoutFlamableBlock) {
    // A registry with NO flamable block: the pass fast-exits, even though
    // lava sits next to blocks (the raw-id floor of the registry-free
    // tests is fine — the fast exit happens before any registry read).
    World world;
    world.getOrCreateChunk({0, 0, 0});
    floor(world, 0, 15, 0, 15, 0);
    world.setBlock(9, 1, 8, kStone);
    world.setFluid(8, 1, 8, FluidKind::Lava, kMaxFluidLevel);
    (void)FluidSim::tick(world, emptyBlocks());
    EXPECT_EQ(world.getBlock(9, 1, 8), kStone);
}

TEST(FluidBurnTest, TwoLavaNeighborsBurnTheCellOnce) {
    World world;
    world.getOrCreateChunk({0, 0, 0});
    floor(world, 0, 15, 0, 15, 0);
    BlockRegistry blocks = buildBurnRegistry();
    world.setBlock(8, 1, 8, blocks.idForName("wheat"));
    world.setFluid(7, 1, 8, FluidKind::Lava, kMaxFluidLevel);
    world.setFluid(9, 1, 8, FluidKind::Lava, kMaxFluidLevel);
    std::vector<BlockPos> changed = FluidSim::tick(world, blocks);
    EXPECT_EQ(world.getBlock(8, 1, 8), BlockID::air());
    int hits = 0;
    for (const BlockPos& p : changed) {
        if (p == BlockPos{8, 1, 8}) {
            ++hits;
        }
    }
    EXPECT_EQ(hits, 1); // collected twice, burned once
}
