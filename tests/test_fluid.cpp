// FluidSim tests: the deterministic water simulation (snapshot-based tick:
// falling conveyor, horizontal spread with level decay, terminal level 1,
// the unified pool rules — refill/absorb/static deposition, FLUID-1 route
// A — cross-chunk chains, equilibrium stability, the per-tick accounting
// identity, and the two exact-constant fixtures (sealed box, P90 lake)).
//
// The simulation is registry-free (it only reads the block/fluid arrays),
// so any non-air BlockID works as "solid" for the floors.
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <map>
#include <string>
#include <vector>

#include "block/block_registry.hpp"
#include "core/asset_manager.hpp"
#include "core/json.hpp"
#include "world/biome.hpp"
#include "world/fluid.hpp"
#include "world/world_generator.hpp"
#include "world/worldgen_config.hpp"

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
    // blocked by the upper-floor stone), the top deposits statically on
    // it (equal level: the pool keeps its level, nothing is written —
    // FLUID-1 route A).
    const std::vector<BlockPos> changed = runTicks(world, 3);
    EXPECT_TRUE(changed.empty());
    EXPECT_EQ(world.getFluid(8, 1, 8), 7);
    EXPECT_EQ(world.getFluid(8, 2, 8), 7);
}

// Phase 99.8 regression: a SETTLED level-1 film (cold chunk — the
// activity gate skips it) must still fall when its support is broken:
// World::setBlock forces one fluid walk of the affected chunks
// (walk-pending), so the gate cannot freeze a static sheet in mid-air.
TEST(FluidTest, SettledFilmFallsWhenSupportBroken) {
    World world;
    floor(world, 4, 12, 4, 12, 0);
    // A level-1 water film resting on the floor (terminal — it will not
    // spread; after one tick the chunks are cold).
    ASSERT_TRUE(world.setFluid(8, 1, 8, FluidKind::Water, 1));
    runTicks(world, 2);
    EXPECT_EQ(world.getFluid(8, 1, 8), 1);
    // Break the floor under the film: the cold chunk must be forced
    // through a walk and the film falls into the hole.
    ASSERT_TRUE(world.setBlock(8, 0, 8, BlockID::air()));
    runTicks(world, 1);
    EXPECT_EQ(world.getFluid(8, 1, 8), 0);
    // The film then drains out of the world through the void.
    runTicks(world, 2);
    EXPECT_EQ(world.getFluid(8, 0, 8), 0);
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

TEST(FluidTest, FlowDepositsOnPoolWithoutRaising) {
    // FLUID-1 route A (adaptation of the pre-FLUID-1
    // FlowingRaisesPoolBelow): a deeper flow resting on a shallower flowing
    // pool DEPOSITS — the flow stays, the pool keeps its level (the old
    // rule raised the pool to the flow's level in one tick).
    World world;
    floor(world, 0, 15, 0, 15, 0);
    ASSERT_TRUE(world.setFluid(8, 1, 8, FluidKind::Water, 3)); // a shallow pool on the floor
    ASSERT_TRUE(world.setFluid(8, 2, 8, FluidKind::Water, 5)); // a deeper flow resting on it

    runTicks(world, 1);
    // The flow stays deposited; the pool is NOT raised.
    EXPECT_EQ(world.getFluid(8, 2, 8), 5);
    EXPECT_EQ(world.getFluid(8, 1, 8), 3);

    // Run to equilibrium: the pool's own y=1 fan spreads 2s at the
    // orthogonals and 1s around them (terminal — never back into the
    // deeper pool), and the pool keeps 3 forever. The deposited flow
    // itself never spreads (spreading requires block support, not fluid
    // support), so there is no y=2 ring.
    std::vector<BlockPos> changed;
    for (int i = 0; i < 60; ++i) {
        changed = FluidSim::tick(world, emptyBlocks());
        EXPECT_EQ(world.getFluid(8, 1, 8), 3) << "tick " << i;
    }
    EXPECT_TRUE(changed.empty());
    EXPECT_EQ(world.getFluid(8, 2, 8), 5); // the deposited flow
    EXPECT_EQ(world.getFluid(7, 2, 8), 0); // no y=2 ring (see above)
    EXPECT_EQ(world.getFluid(7, 1, 8), 2);
    EXPECT_EQ(world.getFluid(6, 1, 8), 1); // terminal
    EXPECT_EQ(world.getFluid(8, 1, 7), 2);
    EXPECT_EQ(world.getFluid(8, 1, 6), 1);
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

// -- FLUID-1: the unified pool rules (route A) + the accounting identity ---
//
// The pre-FLUID-1 rule RAISED the pool below to the level of the flow
// resting on it — the only path by which a pool could gain volume with no
// source (an unaccounted gain). Route A deletes that branch:
//   B == 8, L < 8   -> L refills to 8 (source-back, κ);
//   0 < B < 8, L < B -> L is absorbed into the pool (α);
//   0 < B < 8, L >= B -> L DEPOSITS statically: nothing is written.
// Every per-tick cell change is accounted in the four named event classes
// σ/α/κ/δ (the model comment in src/world/fluid.hpp); these pins the rule
// down and the two exact-constant fixtures (sealed box, P90 lake) pin the
// accounting.

namespace {

// The number of flow cells (levels 1..7) in a single-chunk world.
int flowCount(const World& world) {
    int n = 0;
    for (int y = 0; y < kChunkSize; ++y) {
        for (int z = 0; z < kChunkSize; ++z) {
            for (int x = 0; x < kChunkSize; ++x) {
                const std::uint8_t l = world.getFluid(x, y, z);
                if (l >= 1 && l <= 7) {
                    ++n;
                }
            }
        }
    }
    return n;
}

// The sealed box (the E2E S2 layout): a stone shell x[8,17] x y[40,46] x
// z[8,17], interior air x[9,16] y[41,45] z[9,16], spanning the four
// chunks (0..1, 2, 0..1). Water only.
constexpr int kBoxX0 = 8, kBoxX1 = 17;
constexpr int kBoxY0 = 40, kBoxY1 = 46;
constexpr int kBoxZ0 = 8, kBoxZ1 = 17;

bool boxInterior(int x, int y, int z) {
    return x >= kBoxX0 + 1 && x <= kBoxX1 - 1 && y >= kBoxY0 + 1 &&
        y <= kBoxY1 - 1 && z >= kBoxZ0 + 1 && z <= kBoxZ1 - 1;
}

bool boxSolid(int x, int y, int z) {
    return x >= kBoxX0 && x <= kBoxX1 && y >= kBoxY0 && y <= kBoxY1 &&
        z >= kBoxZ0 && z <= kBoxZ1 && !boxInterior(x, y, z);
}

void buildSealedBox(World& world) {
    const BlockID stone{1};
    for (int x = kBoxX0; x <= kBoxX1; ++x) {
        for (int y = kBoxY0; y <= kBoxY1; ++y) {
            for (int z = kBoxZ0; z <= kBoxZ1; ++z) {
                if (!boxInterior(x, y, z)) {
                    world.setBlock(x, y, z, stone);
                }
            }
        }
    }
    world.setFluid(12, 44, 12, FluidKind::Water, kMaxFluidLevel); // the source
    world.setFluid(13, 44, 13, FluidKind::Water, 7);              // diagonal column
    world.setFluid(14, 44, 14, FluidKind::Water, 6);
    world.setFluid(15, 44, 15, FluidKind::Water, 5);
    world.setFluid(16, 44, 16, FluidKind::Water, 4);
    for (int x = 9; x <= 11; ++x) {                               // the corner film
        for (int z = 9; z <= 11; ++z) {
            world.setFluid(x, 41, z, FluidKind::Water, 2);
        }
    }
}

// The box's exact final state: 64 cells, all on the y=41 floor. The final
// state of a sealed region is rule-independent (the source lands, the
// column falls, the film fans out — nothing else can happen), so the step-
// 0 probe verified this table bit-identical under all three rule variants
// (pre-FLUID-1 / unified absorption / route A).
const std::vector<std::array<int, 4>>& sealedBoxFinal() {
    static const std::vector<std::array<int, 4>> cells = {
        {9, 41, 9, 2},  {9, 41, 10, 3}, {9, 41, 11, 4}, {9, 41, 12, 5},
        {9, 41, 13, 4}, {9, 41, 14, 3}, {9, 41, 15, 2}, {9, 41, 16, 1},
        {10, 41, 9, 3}, {10, 41, 10, 4}, {10, 41, 11, 5}, {10, 41, 12, 6},
        {10, 41, 13, 5}, {10, 41, 14, 4}, {10, 41, 15, 3}, {10, 41, 16, 2},
        {11, 41, 9, 4}, {11, 41, 10, 5}, {11, 41, 11, 6}, {11, 41, 12, 7},
        {11, 41, 13, 6}, {11, 41, 14, 5}, {11, 41, 15, 4}, {11, 41, 16, 3},
        {12, 41, 9, 5}, {12, 41, 10, 6}, {12, 41, 11, 7}, {12, 41, 12, 8},
        {12, 41, 13, 7}, {12, 41, 14, 6}, {12, 41, 15, 5}, {12, 41, 16, 4},
        {13, 41, 9, 4}, {13, 41, 10, 5}, {13, 41, 11, 6}, {13, 41, 12, 7},
        {13, 41, 13, 7}, {13, 41, 14, 6}, {13, 41, 15, 5}, {13, 41, 16, 4},
        {14, 41, 9, 3}, {14, 41, 10, 4}, {14, 41, 11, 5}, {14, 41, 12, 6},
        {14, 41, 13, 6}, {14, 41, 14, 6}, {14, 41, 15, 5}, {14, 41, 16, 4},
        {15, 41, 9, 2}, {15, 41, 10, 3}, {15, 41, 11, 4}, {15, 41, 12, 5},
        {15, 41, 13, 5}, {15, 41, 14, 5}, {15, 41, 15, 5}, {15, 41, 16, 4},
        {16, 41, 9, 1}, {16, 41, 10, 2}, {16, 41, 11, 3}, {16, 41, 12, 4},
        {16, 41, 13, 4}, {16, 41, 14, 4}, {16, 41, 15, 4}, {16, 41, 16, 4},
    };
    return cells;
}

// A snapshot of the water levels inside the box (stone reads 0 — the fluid
// XOR block invariant).
struct BoxState {
    std::array<std::uint8_t, 10 * 7 * 10> a{};
    static std::size_t idx(int x, int y, int z) {
        return (x - kBoxX0) + 10 * (y - kBoxY0) + 70 * (z - kBoxZ0);
    }
    std::uint8_t at(int x, int y, int z) const {
        if (x < kBoxX0 || x > kBoxX1 || y < kBoxY0 || y > kBoxY1 ||
            z < kBoxZ0 || z > kBoxZ1) {
            return 0;
        }
        return a[idx(x, y, z)];
    }
    bool solid(int x, int y, int z) const {
        return boxSolid(x, y, z);
    }
    void load(const World& world) {
        a.fill(0);
        for (int x = kBoxX0; x <= kBoxX1; ++x) {
            for (int y = kBoxY0; y <= kBoxY1; ++y) {
                for (int z = kBoxZ0; z <= kBoxZ1; ++z) {
                    a[idx(x, y, z)] =
                        world.getFluidOfKind(x, y, z, FluidKind::Water);
                }
            }
        }
    }
    long flow() const {
        long n = 0;
        for (std::uint8_t l : a) {
            if (l >= 1 && l <= 7) {
                ++n;
            }
        }
        return n;
    }
    long source() const {
        long n = 0;
        for (std::uint8_t l : a) {
            if (l == kMaxFluidLevel) {
                ++n;
            }
        }
        return n;
    }
    long sum() const {
        long n = 0;
        for (std::uint8_t l : a) {
            n += l;
        }
        return n;
    }
};

// Re-derives the sim's falling flag (the model comment's column walk, kept
// in lock-step with fluid.cpp on purpose — drift here fails the identity):
// a water cell falls when the contiguous water run below it ends on dry
// air (or y < 0), not on a block.
bool isFalling(const BoxState& st, int x, int y, int z) {
    for (int yy = y - 1; yy >= 0; --yy) {
        if (st.solid(x, yy, z)) {
            return false;
        }
        if (st.at(x, yy, z) == 0) {
            return true;
        }
    }
    return true;
}

} // namespace

TEST(FluidTest, PoolNeverRaised) {
    // The route-A pin: a deeper flow on a shallower pool NEVER raises the
    // pool (pre-FLUID-1: one tick, pool 3 -> 7). The flow deposits at y=2
    // (and never spreads — fluid support, not block support); the y=1 fan
    // the pool grows is shallower than the pool, so no horizontal spread
    // reaches back into it either.
    World world;
    floor(world, 0, 15, 0, 15, 0);
    ASSERT_TRUE(world.setFluid(8, 1, 8, FluidKind::Water, 3));
    ASSERT_TRUE(world.setFluid(8, 2, 8, FluidKind::Water, 7));
    for (int i = 0; i < 20; ++i) {
        FluidSim::tick(world, emptyBlocks());
        EXPECT_EQ(world.getFluid(8, 1, 8), 3) << "tick " << i;
    }
    EXPECT_EQ(world.getFluid(8, 2, 8), 7);
}

TEST(FluidTest, FilmOnFilmDepositsNoRaise) {
    // A level-2 flow on a level-1 film deposits (the film keeps 1 —
    // pre-FLUID-1 the film would be raised to 2). The flow itself never
    // spreads (fluid support, not block support) and the level-1 film is
    // terminal — the pair is static from tick 1 (the flip-trigger case,
    // where every other rule variant churned).
    World world;
    floor(world, 0, 15, 0, 15, 0);
    ASSERT_TRUE(world.setFluid(8, 1, 8, FluidKind::Water, 1));
    ASSERT_TRUE(world.setFluid(8, 2, 8, FluidKind::Water, 2));
    std::vector<BlockPos> changed;
    for (int i = 0; i < 40; ++i) {
        changed = FluidSim::tick(world, emptyBlocks());
        EXPECT_EQ(world.getFluid(8, 1, 8), 1) << "tick " << i;
        EXPECT_EQ(world.getFluid(8, 2, 8), 2) << "tick " << i;
    }
    EXPECT_TRUE(changed.empty());
}

TEST(FluidTest, EqualFilmDepositsStatic) {
    // The flip-trigger pair in isolation: a level-2 flow on a level-2
    // pool, walled so nothing spreads — ZERO changes on every tick (the
    // deposition is a pure no-op; the pre-FLUID-1 rule wrote a no-op raise
    // proposal here, and the absorbed-everything alternative would have
    // consumed the flow — both would churn this cell every tick).
    World world;
    floor(world, 4, 12, 4, 12, 0);
    ASSERT_TRUE(world.setFluid(8, 1, 8, FluidKind::Water, 2));
    ASSERT_TRUE(world.setFluid(8, 2, 8, FluidKind::Water, 2));
    world.setBlock(7, 2, 8, kStone);
    world.setBlock(9, 2, 8, kStone);
    world.setBlock(8, 2, 7, kStone);
    world.setBlock(8, 2, 9, kStone);
    world.setBlock(7, 1, 8, kStone);
    world.setBlock(9, 1, 8, kStone);
    world.setBlock(8, 1, 7, kStone);
    world.setBlock(8, 1, 9, kStone);
    const std::vector<BlockPos> changed = runTicks(world, 30);
    EXPECT_TRUE(changed.empty());
    EXPECT_EQ(world.getFluid(8, 1, 8), 2);
    EXPECT_EQ(world.getFluid(8, 2, 8), 2);
}

TEST(FluidTest, FilmMergesIntoPoolAccounted) {
    // α: a shallower flow on a deeper pool is absorbed — the flow-cell
    // count drops by exactly one, the pool keeps its level, and the pool's
    // spread ring is the only creation (σ, +4).
    World world;
    floor(world, 0, 15, 0, 15, 0);
    ASSERT_TRUE(world.setFluid(8, 1, 8, FluidKind::Water, 5));
    ASSERT_TRUE(world.setFluid(8, 2, 8, FluidKind::Water, 2));
    const int before = flowCount(world); // 2
    ASSERT_EQ(before, 2);
    FluidSim::tick(world, emptyBlocks());
    EXPECT_EQ(world.getFluid(8, 2, 8), 0);  // absorbed
    EXPECT_EQ(world.getFluid(8, 1, 8), 5);  // pool unchanged
    EXPECT_EQ(flowCount(world), before + 3); // −1 (α) +4 (σ ring)
}

TEST(FluidTest, LavaPoolNeverRaised) {
    // The Q1 lava pin: the rule is per-kind — a deeper LAVA flow on a
    // shallower lava pool deposits; the pool never raises.
    World world;
    floor(world, 0, 15, 0, 15, 0);
    ASSERT_TRUE(world.setFluid(8, 1, 8, FluidKind::Lava, 3));
    ASSERT_TRUE(world.setFluid(8, 2, 8, FluidKind::Lava, 7));
    for (int i = 0; i < 20; ++i) {
        FluidSim::tick(world, emptyBlocks());
        EXPECT_EQ(world.getFluidOfKind(8, 1, 8, FluidKind::Lava), 3)
            << "tick " << i;
    }
    EXPECT_EQ(world.getFluidOfKind(8, 2, 8, FluidKind::Lava), 7);
}

TEST(FluidTest, LavaFilmAbsorbedIntoDeeperLava) {
    // α for lava: a shallower lava film on a deeper lava pool is absorbed;
    // the pool keeps its level.
    World world;
    floor(world, 0, 15, 0, 15, 0);
    ASSERT_TRUE(world.setFluid(8, 1, 8, FluidKind::Lava, 5));
    ASSERT_TRUE(world.setFluid(8, 2, 8, FluidKind::Lava, 2));
    FluidSim::tick(world, emptyBlocks());
    EXPECT_EQ(world.getFluidOfKind(8, 2, 8, FluidKind::Lava), 0);
    EXPECT_EQ(world.getFluidOfKind(8, 1, 8, FluidKind::Lava), 5);
    EXPECT_EQ(world.getFluidOfKind(7, 1, 8, FluidKind::Lava), 4); // the ring
}

TEST(FluidTest, FallChainLevelsPreserved) {
    // The conveyor is a paired net-zero move: a falling chain shifts as a
    // whole (level sum and flow count unchanged), until it reaches the
    // y<0 boundary drain (δ — the only loss term).
    World world;
    floor(world, 6, 10, 6, 10, 0);
    world.setBlock(8, 0, 8, BlockID::air()); // the hole reaches the void
    ASSERT_TRUE(world.setFluid(8, 1, 8, FluidKind::Water, 7));
    ASSERT_TRUE(world.setFluid(8, 2, 8, FluidKind::Water, 7));
    ASSERT_TRUE(world.setFluid(8, 3, 8, FluidKind::Water, 7));

    runTicks(world, 1);
    // The whole chain shifted down one cell (the top emptied): Σlevel 21
    // unchanged, F=3 unchanged.
    EXPECT_EQ(world.getFluid(8, 0, 8), 7);
    EXPECT_EQ(world.getFluid(8, 1, 8), 7);
    EXPECT_EQ(world.getFluid(8, 2, 8), 7);
    EXPECT_EQ(world.getFluid(8, 3, 8), 0);

    runTicks(world, 1);
    // The bottom now sits at y=0 over the void: the next fall drains a
    // level-7 token out of the world (δ): Σlevel 21 -> 14, F 3 -> 2.
    EXPECT_EQ(world.getFluid(8, 0, 8), 7);
    EXPECT_EQ(world.getFluid(8, 1, 8), 7);
    EXPECT_EQ(world.getFluid(8, 2, 8), 0);
}

TEST(FluidTest, WorldBottomDrainCounted) {
    // δ: a flow at y=0 over the void drains out of the world — the level
    // sum drops by the flow's level, the flow count by one.
    World world;
    world.getOrCreateChunk({0, 0, 0});
    ASSERT_TRUE(world.setFluid(8, 0, 8, FluidKind::Water, 7));
    runTicks(world, 1);
    EXPECT_EQ(world.getFluid(8, 0, 8), 0);
    EXPECT_EQ(flowCount(world), 0);
}

TEST(FluidTest, ConflictMaxMergeApplied) {
    // Two spreads into one dry cell merge to the MAX level (the 5 proposes
    // 4, the 7 proposes 6 — the cell takes 6, not the first proposal).
    World world;
    floor(world, 0, 15, 0, 15, 0);
    ASSERT_TRUE(world.setFluid(7, 1, 8, FluidKind::Water, 5));
    ASSERT_TRUE(world.setFluid(9, 1, 8, FluidKind::Water, 7));
    runTicks(world, 1);
    EXPECT_EQ(world.getFluid(8, 1, 8), 6);
}

TEST(FluidTest, FlowOnlyFanSettlesCold) {
    // 99.8 integration: a source-free flow fan settles and its chunk goes
    // fluid-COLD (the activity gate then skips it — a settled pool keeps no
    // chunk permanently hot).
    World world;
    floor(world, 0, 15, 0, 15, 0);
    ASSERT_TRUE(world.setFluid(8, 1, 8, FluidKind::Water, 5));
    std::vector<BlockPos> changed;
    int ticks = 0;
    do {
        changed = FluidSim::tick(world, emptyBlocks());
        ++ticks;
    } while (!changed.empty() && ticks < 60);
    EXPECT_TRUE(changed.empty());
    const Chunk* c = world.findChunk({0, 0, 0});
    ASSERT_NE(c, nullptr);
    EXPECT_FALSE(c->fluidHot(FluidKind::Water));
    // The fan settled to the floor gradient (5 at the center, 1 at the
    // edge — the level-1 ring is terminal).
    EXPECT_EQ(world.getFluid(8, 1, 8), 5);
    EXPECT_EQ(world.getFluid(7, 1, 8), 4);
    EXPECT_EQ(world.getFluid(5, 1, 8), 2);
    EXPECT_EQ(world.getFluid(4, 1, 8), 1);
}

TEST(FluidTest, FlowingLavaPoolBesideSourceWaterBecomesObsidian) {
    // Regression guard: the reaction pass runs BEFORE the pool rules — a
    // flowing (non-full) lava pool 6-adjacent to source water converts to
    // obsidian (never to stone), and the water is not consumed.
    const BlockRegistry blocks = buildReactionRegistry();
    World world;
    floor(world, 4, 12, 4, 12, 0);
    ASSERT_TRUE(world.setFluid(8, 1, 8, FluidKind::Lava, 3));
    ASSERT_TRUE(world.setFluid(9, 1, 8, FluidKind::Water, kMaxFluidLevel));
    FluidSim::tick(world, blocks);
    EXPECT_EQ(world.getBlock(8, 1, 8), blocks.idForName("obsidian"));
    EXPECT_EQ(world.getFluidOfKind(9, 1, 8, FluidKind::Water),
              kMaxFluidLevel);
}

TEST(FluidTest, CrossChunkPoolDeposit) {
    // The pool rule reads the cell below ACROSS the chunk seam: a flow
    // above the seam pool deposits (no raise, no fall) and the whole
    // structure settles with the pool level untouched.
    World world;
    world.getOrCreateChunk({0, 0, 0});
    world.getOrCreateChunk({1, 0, 0});
    floor(world, 0, 31, 0, 15, 0); // spans both chunks
    ASSERT_TRUE(world.setFluid(15, 1, 0, FluidKind::Water, 3)); // west chunk
    ASSERT_TRUE(world.setFluid(16, 1, 0, FluidKind::Water, 3)); // east chunk (seam)
    ASSERT_TRUE(world.setFluid(16, 2, 0, FluidKind::Water, 5)); // above the seam pool
    runTicks(world, 1);
    EXPECT_EQ(world.getFluid(16, 2, 0), 5); // deposited
    EXPECT_EQ(world.getFluid(16, 1, 0), 3); // not raised
    EXPECT_EQ(world.getFluid(15, 1, 0), 3); // the rest of the pool
    std::vector<BlockPos> changed;
    for (int i = 0; i < 40; ++i) {
        changed = FluidSim::tick(world, emptyBlocks());
        EXPECT_EQ(world.getFluid(16, 1, 0), 3) << "tick " << i;
    }
    EXPECT_TRUE(changed.empty());
}

TEST(FluidTest, SealedBoxSingleSourceExactFinal) {
    // The sealed-box exact constant: a single source + a diagonal falling
    // column + a corner film settle to exactly the 64-cell y=41 table (all
    // three rule variants produce it bit-identically — the step-0 probe).
    World world;
    buildSealedBox(world);
    std::vector<BlockPos> changed;
    int settleTick = -1;
    for (int t = 1; t <= 200; ++t) {
        changed = FluidSim::tick(world, emptyBlocks());
        if (changed.empty() && settleTick < 0) {
            settleTick = t;
        }
    }
    EXPECT_LE(settleTick, 20); // settles fast (the probe: tick 11)
    EXPECT_TRUE(changed.empty());
    for (const std::array<int, 4>& c : sealedBoxFinal()) {
        EXPECT_EQ(world.getFluid(c[0], c[1], c[2]), c[3])
            << c[0] << "," << c[1] << "," << c[2];
    }
    // And nothing else in the box is wet (it is sealed).
    BoxState st;
    st.load(world);
    ASSERT_EQ(st.source(), 1);
    ASSERT_EQ(st.flow(), 63);
    ASSERT_EQ(st.sum(), 286);
    for (int x = kBoxX0; x <= kBoxX1; ++x) {
        for (int y = kBoxY0; y <= kBoxY1; ++y) {
            for (int z = kBoxZ0; z <= kBoxZ1; ++z) {
                bool inTable = false;
                for (const std::array<int, 4>& c : sealedBoxFinal()) {
                    if (c[0] == x && c[1] == y && c[2] == z) {
                        inTable = true;
                        break;
                    }
                }
                if (!inTable) {
                    EXPECT_EQ(st.at(x, y, z), 0) << x << "," << y << "," << z;
                }
            }
        }
    }
}

TEST(FluidTest, SealedBoxAccountingIdentity) {
    // The FLUID-1 accounting identity, per tick: every cell change of the
    // sealed box falls into one of the four named event classes (σ spread
    // front / α absorption / κ source-back refill / δ boundary drain) or is
    // a paired conveyor move — and the flow-cell count F and the source
    // count S obey F' = F + σ_c − α − κ − δ, S' = S + κ; a tick with no
    // σ/α/κ/δ event at all conserves the level sum (C3).
    World world;
    buildSealedBox(world);
    BoxState st;
    st.load(world);
    long sigC = 0, sigR = 0, alpha = 0, kappa = 0, delta = 0, conv = 0;
    for (int t = 1; t <= 200; ++t) {
        const long F = st.flow(), S = st.source(), SUM = st.sum();
        const std::vector<BlockPos> changed =
            FluidSim::tick(world, emptyBlocks());
        BoxState st2;
        st2.load(world);
        long sigc = 0, sigr = 0, al = 0, kp = 0, de = 0, cv = 0;
        for (const BlockPos& p : changed) {
            const std::uint8_t o = st.at(p.x, p.y, p.z);
            const std::uint8_t n = st2.at(p.x, p.y, p.z);
            if (o == n) {
                ADD_FAILURE() << "tick " << t << ": no net change at "
                              << p.x << "," << p.y << "," << p.z;
                continue;
            }
            if (isFalling(st, p.x, p.y, p.z)) {
                // Conveyor departure: the cell takes the level of the cell
                // above. The box never feeds a spread into a falling cell
                // (the columns fall through air), so the conveyor value is
                // the only proposal.
                EXPECT_EQ(n, st.at(p.x, p.y + 1, p.z)) << "tick " << t;
                ++cv;
                continue;
            }
            if (o > 0 && n == 0) {
                EXPECT_EQ(st.at(p.x, p.y - 1, p.z), 0) << "tick " << t;
                ++al; // α: absorbed into the deeper pool below
            } else if (o > 0 && n == kMaxFluidLevel) {
                EXPECT_EQ(st.at(p.x, p.y - 1, p.z), kMaxFluidLevel)
                    << "tick " << t;
                ++kp; // κ: source-back refill
            } else if (o == 0 && n > 0) {
                const std::uint8_t a = st.at(p.x, p.y + 1, p.z);
                const bool aboveFalling =
                    a > 0 && isFalling(st, p.x, p.y + 1, p.z);
                if (aboveFalling && n <= a) {
                    ++cv; // conveyor arrival (paired; a suppressed spread
                          // below the falling level changes nothing)
                } else if (n < kMaxFluidLevel) {
                    ++sigc; // σ: spread creation
                } else {
                    ADD_FAILURE() << "tick " << t
                                  << ": a source appeared without a "
                                     "conveyor arrival at "
                                  << p.x << "," << p.y << "," << p.z;
                }
            } else if (o > 0 && n > 0) {
                EXPECT_LT(o, kMaxFluidLevel) << "tick " << t;
                ++sigr; // σ-class: a spread raise (no count term)
            }
        }
        EXPECT_EQ(st2.flow() - F, sigc - al - kp - de) << "tick " << t;
        EXPECT_EQ(st2.source() - S, kp) << "tick " << t;
        if (sigc + sigr + al + kp + de == 0) {
            EXPECT_EQ(st2.sum(), SUM) << "tick " << t << " (C3)";
        }
        sigC += sigc;
        sigR += sigr;
        alpha += al;
        kappa += kp;
        delta += de;
        conv += cv;
        st = std::move(st2);
    }
    // The whole run: only the spread front (σ) and the paired conveyor
    // falls (the diagonal column + the source dropping from y=44 to the
    // floor) — the sealed box never absorbs, refills, or drains.
    EXPECT_EQ(alpha, 0);
    EXPECT_EQ(kappa, 0);
    EXPECT_EQ(delta, 0);
    EXPECT_GT(sigC, 0);
    EXPECT_GT(sigR, 0); // the source's fan raises the film cells it meets
    EXPECT_GT(conv, 0);
    // The source count is constant: the source token only moves (its
    // conveyor arrival/departure pair nets zero).
    EXPECT_EQ(st.source(), 1);
    EXPECT_EQ(st.flow(), 63);
    EXPECT_EQ(st.sum(), 286);
}

TEST(FluidTest, P90OverflowExactFinal) {
    // The P90 surface lake (seed 9, (24,26) R1 depth==fill==2) through the
    // GAME's integration path (insertChunk: fluid registration + the hot
    // first walk), 200 fluid ticks = the game's 600 frames, with the game's
    // fire lifetime emulated (fire.lifetime_seconds 4.0 s = 80 fluid ticks;
    // a fire cell turns to air at ignition + 81 — the first fluid tick
    // after its 240th frame). Fire spread never runs (the game's separate
    // pass; the P90 scene uses spread 0). The route-A constants (identical
    // to the pre-FLUID-1 rule — this scene never triggered the deleted
    // raise branch): the rim chars, the total/late edit counts, the single
    // churn cell, and the final lava census.
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    BlockRegistry blocks;
    ASSERT_TRUE(blocks.loadDirectory(assets, "data/blocks").ok());
    BiomeRegistry biomes;
    ASSERT_TRUE(biomes.loadDirectory(assets, "data/biomes").ok());
    auto params = WorldgenParams::load(assets);
    ASSERT_TRUE(params.ok()) << params.error().message;
    auto gen = WorldGenerator::create(9, *params, biomes, blocks);
    ASSERT_TRUE(gen.ok()) << gen.error().message;
    WorldGenerator generator = std::move(*gen);
    World world;
    // The disk of radius 3 around the lake's chunk (1,1), fully pre-loaded
    // (the lake region is settled before tick 1; the overflow evolves into
    // the hundreds of ticks, so the pre-load is behavior-neutral).
    for (int cx = -1; cx <= 3; ++cx) {
        for (int cz = -1; cz <= 3; ++cz) {
            if ((cx - 1) * (cx - 1) + (cz - 1) * (cz - 1) > 9) {
                continue;
            }
            for (int cy = 0; cy * kChunkSize < kWorldHeight; ++cy) {
                const ChunkPos cp{cx, cy, cz};
                if (world.hasChunk(cp)) {
                    continue;
                }
                Chunk scratch;
                generator.generateChunk(scratch, cp);
                world.insertChunk(cp, std::move(scratch));
            }
        }
    }
    std::map<BlockPos, int> fireIgnition;
    long rimChars = 0;
    long total = 0;
    long late = 0;
    std::map<BlockPos, int> lateChanges;
    for (int t = 1; t <= 200; ++t) {
        // The fire countdown (before this tick's simulation):
        for (auto it = fireIgnition.begin(); it != fireIgnition.end();) {
            if (t >= it->second + 81) {
                world.setBlock(it->first.x, it->first.y, it->first.z,
                               BlockID::air());
                it = fireIgnition.erase(it);
            } else {
                ++it;
            }
        }
        const std::vector<BlockPos> changed = FluidSim::tick(world, blocks);
        total += static_cast<long>(changed.size());
        if (t >= 150) {
            late += static_cast<long>(changed.size());
            for (const BlockPos& p : changed) {
                ++lateChanges[p];
            }
        }
        for (const BlockPos& p : changed) {
            const BlockID b = world.getBlock(p.x, p.y, p.z);
            if (b.valid() && blocks.at(b).name == "fire") {
                if (fireIgnition.count(p) == 0) {
                    ++rimChars; // each rim cell ignites once (spread 0)
                }
                fireIgnition[p] = t;
            }
        }
    }
    int churn = 0;
    for (const auto& [p, n] : lateChanges) {
        if (n >= 5) {
            ++churn;
        }
    }
    EXPECT_EQ(rimChars, 66);
    EXPECT_EQ(total, 473L);
    EXPECT_EQ(late, 171L);
    EXPECT_EQ(churn, 1); // the accepted standing waterfall (the KNOWN_ISSUES
                         // fluid-section churn class)
    // The final lava census: basin vs overflow (the two buried lakes are
    // static and excluded by their region boxes).
    int basin = 0, buried = 0, overflow = 0;
    for (int cx = -1; cx <= 3; ++cx) {
        for (int cz = -1; cz <= 3; ++cz) {
            if ((cx - 1) * (cx - 1) + (cz - 1) * (cz - 1) > 9) {
                continue;
            }
            for (int cy = 0; cy * kChunkSize < kWorldHeight; ++cy) {
                const Chunk* c = world.findChunk({cx, cy, cz});
                if (c == nullptr) {
                    continue;
                }
                for (int i = 0; i < Chunk::kVolume; ++i) {
                    const std::uint8_t code = c->fluidData()[i];
                    if (fluidKindOf(code) != FluidKind::Lava) {
                        continue;
                    }
                    const int lx = static_cast<int>(i % kChunkSize);
                    const int lz =
                        static_cast<int>((i / kChunkSize) % kChunkSize);
                    const int x = cx * kChunkSize + lx;
                    const int z = cz * kChunkSize + lz;
                    if (x >= 23 && x <= 25 && z >= 25 && z <= 27) {
                        ++basin;
                    } else if ((x >= 36 && x <= 44 && z >= 36 && z <= 44) ||
                               (x >= 52 && x <= 58 && z >= 42 && z <= 48)) {
                        ++buried;
                    } else {
                        ++overflow;
                    }
                }
            }
        }
    }
    EXPECT_EQ(basin, 18);
    EXPECT_EQ(buried, 260);
    EXPECT_EQ(overflow, 157);
}
