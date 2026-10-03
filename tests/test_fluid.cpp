// FluidSim tests: the deterministic water simulation (snapshot-based tick:
// falling conveyor, horizontal spread with level decay, terminal level 1,
// merge/absorb on fluid, cross-chunk chains, equilibrium stability).
//
// The simulation is registry-free (it only reads the block/fluid arrays),
// so any non-air BlockID works as "solid" for the floors.
#include <gtest/gtest.h>

#include <vector>

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

// Runs `n` ticks; returns the last tick's changed cells.
std::vector<BlockPos> runTicks(World& world, int n) {
    std::vector<BlockPos> changed;
    for (int i = 0; i < n; ++i) {
        changed = FluidSim::tick(world);
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
    world.setFluid(8, 1, 8, kMaxFluidLevel); // a full cell above the hole
    world.setFluid(3, 1, 3, kMaxFluidLevel); // a source at the corner
    world.setFluid(12, 1, 12, 5);            // a flowing cell mid-floor
}

} // namespace

TEST(FluidTest, FullCellRestsOnSolidAndSpreads) {
    World world;
    floor(world, 4, 12, 4, 12, 0);
    ASSERT_TRUE(world.setFluid(8, 1, 8, kMaxFluidLevel));

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
    ASSERT_TRUE(world.setFluid(8, 1, 8, 1));
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
    ASSERT_TRUE(world.setFluid(8, 2, 8, 7));
    ASSERT_TRUE(world.setFluid(8, 3, 8, 7));

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

    ASSERT_TRUE(world.setFluid(8, 1, 8, kMaxFluidLevel));
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
                ASSERT_TRUE(world.setFluid(x, y, z, kMaxFluidLevel));
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
        const std::vector<BlockPos> changed = FluidSim::tick(world);
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
    ASSERT_TRUE(world.setFluid(8, 1, 8, 5)); // the pool (flowing, not full)
    ASSERT_TRUE(world.setFluid(8, 2, 8, 3)); // a shallow film above it

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
    ASSERT_TRUE(world.setFluid(8, 1, 8, kMaxFluidLevel)); // the fallen cell
    ASSERT_TRUE(world.setFluid(8, 2, 8, 7));             // the neighbor film

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
            ASSERT_TRUE(world.setFluid(x, 2, z, kMaxFluidLevel));
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
        changed = FluidSim::tick(world);
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
    ASSERT_TRUE(world.setFluid(8, 1, 8, 3)); // a shallow pool on the floor
    ASSERT_TRUE(world.setFluid(8, 2, 8, 5)); // a deeper flow resting on it

    runTicks(world, 1);
    // The flow stays and raises the pool to its own level.
    EXPECT_EQ(world.getFluid(8, 2, 8), 5);
    EXPECT_EQ(world.getFluid(8, 1, 8), 5);

    // Run to equilibrium: the pool now spreads a 5,4,3,2,1 gradient along
    // the floor and settles (the level-1 ring is terminal).
    std::vector<BlockPos> changed;
    for (int i = 0; i < 40; ++i) {
        changed = FluidSim::tick(world);
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
    ASSERT_TRUE(world.setFluid(8, 1, 8, kMaxFluidLevel));
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
    ASSERT_TRUE(world.setFluid(8, 0, 8, kMaxFluidLevel)); // at y=0, void below

    runTicks(world, 1);
    EXPECT_EQ(world.getFluid(8, 0, 8), 0);
}

TEST(FluidTest, FallingChainCrossesChunkBoundary) {
    World world;
    world.getOrCreateChunk({0, 0, 0});
    world.getOrCreateChunk({0, 1, 0});
    // A 2-cell falling chain straddling the y=15/16 chunk boundary.
    ASSERT_TRUE(world.setFluid(8, 15, 8, 7));
    ASSERT_TRUE(world.setFluid(8, 16, 8, 7));

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
        FluidSim::tick(a);
    }
    const auto stateA = snapshot(a);

    World b;
    buildScenario(b);
    for (int i = 0; i < 100; ++i) {
        FluidSim::tick(b);
    }
    const auto stateB = snapshot(b);

    EXPECT_EQ(stateA, stateB);
    // The scenario actually moved water (the source spread, the hole drained
    // and refilled): the final state is not the initial one.
    EXPECT_GT(stateA.size(), 4u);
}
