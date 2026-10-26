// 99.8 fluid activity gate — streaming-boundary wake tests: a settled
// (cold) fluid chunk adjacent to a just-appeared chunk must be forced
// through one re-walk (World::wakeFluidChunks, the setBlock UNSTICK pass's
// mirror — the new chunk's appearance changes the fluid's feasibility:
// a spread proposal the VOID just rejected now has a solid target).
// Test 1 is the root-cause discriminator: the lava frontier of a stone
// channel (the E2E scene_base geometry) is rejected by the missing east
// column, goes cold, and — before the fix — stays stuck forever when the
// column re-appears (verified pre-fix FAIL via a path-scoped git stash of
// src/world/world.{hpp,cpp}); after the fix the cascade resumes across
// the boundary within two ticks. Test 2 is the reverse direction (a
// generated sea meeting an adjacent settled dry depression), and test 3
// pins the 99.8 performance side: a fully settled world with NO chunk
// insertions walks nothing (every tick changes nothing, every fluid
// chunk stays cold).
//
// The simulation is registry-free (test_fluid pattern): the reaction pass
// is inert without a lava fluid block, the burn pass inert without a
// flamable block, so any non-air BlockID works as "solid".
#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "block/block.hpp"
#include "world/chunk.hpp"
#include "world/coordinates.hpp"
#include "world/fluid.hpp"
#include "world/world.hpp"

using namespace mc;

namespace {

const BlockID kStone {1}; // any non-air id: FluidSim only checks != air

// The E2E scene (strm2 scene_base.json) geometry, as named constants:
constexpr int kFloorY = 45; // the stone channel bed
constexpr int kFlowY = 46;  // the lava row (one above the bed)
constexpr int kWestCX = 11; // west column: x = 176..191
constexpr int kEastCX = 12; // east column: x = 192..207
constexpr int kPoolX0 = 186; // the L8 pool's west edge
constexpr int kPoolX1 = 188; // the L8 pool's east edge
constexpr int kCapX = 185;  // the west cap wall
constexpr int kFrontierX = 191; // the channel's east end (the frontier)
constexpr int kTrenchX = 191;  // test 2: the depression's column
constexpr int kTrenchFloorY = 44; // test 2: one below the sea level
constexpr int kSeaY = 46;    // test 2: the generated sea level (L8)
constexpr int kSlabWaterY = 46; // test 3: the sealed water slab
constexpr int kSlabLavaY = 43;  // test 3: the sealed lava slab

// One full column (all eight layers, the streaming loader's shape).
void makeFullColumn(World& world, int cx, int cz) {
    for (int cy = 0; cy * kChunkSize < kWorldHeight; ++cy) {
        world.getOrCreateChunk({cx, cy, cz});
    }
}

void eraseFullColumn(World& world, int cx, int cz) {
    for (int cy = 0; cy * kChunkSize < kWorldHeight; ++cy) {
        EXPECT_TRUE(world.eraseChunk({cx, cy, cz}));
    }
}

// One fluid tick; the empty registry keeps reaction + burn inert.
std::vector<BlockPos> tick(World& world) {
    static BlockRegistry blocks; // no fluid block, no flamable: inert
    return FluidSim::tick(world, blocks);
}

// Test 1's generator: the channel bed — a stone floor at y=kFloorY, the
// single z=0 row, in every chunk (the east column's "natural ground" the
// lava flows onto).
World::ChunkGenerator channelBedGenerator() {
    return [](Chunk& c, const ChunkPos& cp) {
        const int oy = cp.y * kChunkSize;
        if (oy > kFloorY || oy + kChunkSize - 1 < kFloorY) {
            return;
        }
        for (int x = 0; x < kChunkSize; ++x) {
            c.set(x, kFloorY - oy, 0, kStone);
        }
    };
}

// The scene channel (test 1): the bed floor (from the generator) + the
// west cap wall + the two side walls at z=±1 + the L8 pool.
void buildChannel(World& world) {
    world.setBlock(kCapX, kFlowY, 0, kStone);
    for (int x = kPoolX0; x <= kFrontierX; ++x) {
        world.setBlock(x, kFlowY, 1, kStone);
        world.setBlock(x, kFlowY, -1, kStone);
    }
    for (int x = kPoolX0; x <= kPoolX1; ++x) {
        world.setFluid(x, kFlowY, 0, FluidKind::Lava, kMaxFluidLevel);
    }
}

// Test 2's generator: the west column's plain floor at y=kSeaY-1 with the
// one-column trench at x=kTrenchX (floor at y=kTrenchFloorY, one below
// the sea level); the east column's floor + the full L8 sea slab.
World::ChunkGenerator depressionSeaGenerator() {
    return [](Chunk& c, const ChunkPos& cp) {
        const int oy = cp.y * kChunkSize;
        if (cp.x == kWestCX) {
            for (int x = 0; x < kChunkSize; ++x) {
                const int wx = cp.x * kChunkSize + x;
                const int floorY =
                    (wx == kTrenchX) ? kTrenchFloorY : kSeaY - 1;
                if (oy <= floorY && oy + kChunkSize - 1 >= floorY) {
                    for (int z = 0; z < kChunkSize; ++z) {
                        c.set(x, floorY - oy, z, kStone);
                    }
                }
            }
        } else if (cp.x == kEastCX) {
            for (int x = 0; x < kChunkSize; ++x) {
                for (int z = 0; z < kChunkSize; ++z) {
                    if (oy <= kSeaY - 1 && oy + kChunkSize - 1 >= kSeaY - 1) {
                        c.set(x, kSeaY - 1 - oy, z, kStone);
                    }
                    if (oy <= kSeaY && oy + kChunkSize - 1 >= kSeaY) {
                        c.setFluidAt(x, kSeaY - oy, z,
                                     fluidCode(FluidKind::Water,
                                               kMaxFluidLevel));
                    }
                }
            }
        }
    };
}

// Test 3's generator: the west column's layer-2 sealed slabs — a full
// L8 water slab and a full L8 lava slab, each between stone slabs (no
// cell can propose anything: every horizontal neighbor is the same kind
// at the same level, every vertical one is stone).
World::ChunkGenerator sealedSlabGenerator() {
    return [](Chunk& c, const ChunkPos& cp) {
        if (cp.x != kWestCX) {
            return;
        }
        const int oy = cp.y * kChunkSize;
        const auto slab = [&](int y, std::uint8_t code) {
            if (oy > y || oy + kChunkSize - 1 < y) {
                return;
            }
            const int ly = y - oy;
            for (int x = 0; x < kChunkSize; ++x) {
                for (int z = 0; z < kChunkSize; ++z) {
                    if (code == 0) {
                        c.set(x, ly, z, kStone);
                    } else {
                        c.setFluidAt(x, ly, z, code);
                    }
                }
            }
        };
        slab(kSlabLavaY - 1, 0); // stone under the lava
        slab(kSlabLavaY, fluidCode(FluidKind::Lava, kMaxFluidLevel));
        slab(kSlabLavaY + 1, 0); // stone over the lava
        slab(kSeaY - 1, 0); // stone under the water
        slab(kSeaY, fluidCode(FluidKind::Water, kMaxFluidLevel));
        slab(kSeaY + 1, 0); // stone over the water
    };
}

} // namespace

// The root-cause discriminator: the channel's lava frontier (191,46,0)
// L5 proposes into the missing east column, is rejected (the void stays
// dry), the chunk goes cold — and when the column re-appears (the
// streaming re-load), the settled frontier must resume across the
// boundary: (192,46,0) L4, then (193,46,0) L3, within two ticks.
// Pre-fix (no wakeFluidChunks) the first post-reload tick changes
// nothing and (192,46,0) stays dry forever.
TEST(FluidStreamingGateTest, FrontierCrossesTheStreamingBoundary) {
    World world;
    world.setChunkGenerator(channelBedGenerator());
    // Both columns generated, then the east one UNLOADED before any tick
    // (the streaming state: the frontier's target column is not loaded).
    makeFullColumn(world, kWestCX, 0);
    makeFullColumn(world, kEastCX, 0);
    eraseFullColumn(world, kEastCX, 0);
    buildChannel(world);

    // The cascade reaches the frontier in three ticks (one cell per
    // tick: the snapshot model), pinning the scene's live state.
    std::vector<BlockPos> changed = tick(world);
    ASSERT_EQ(changed.size(), 1u);
    EXPECT_EQ(changed[0], (BlockPos {kPoolX1 + 1, kFlowY, 0}));
    EXPECT_EQ(world.getFluid(kPoolX1 + 1, kFlowY, 0), 7);
    changed = tick(world);
    ASSERT_EQ(changed.size(), 1u);
    EXPECT_EQ(changed[0], (BlockPos {kFrontierX - 1, kFlowY, 0}));
    EXPECT_EQ(world.getFluid(kFrontierX - 1, kFlowY, 0), 6);
    changed = tick(world);
    ASSERT_EQ(changed.size(), 1u);
    EXPECT_EQ(changed[0], (BlockPos {kFrontierX, kFlowY, 0}));
    // The frontier cell: L5 at the channel's east end.
    EXPECT_EQ(world.getFluid(kFrontierX, kFlowY, 0), 5);

    // The frontier's spread proposal now targets the MISSING east column:
    // the tick walks (hot from the last write), rejects every proposal
    // (the void stays dry), changes nothing, and goes cold.
    changed = tick(world);
    EXPECT_TRUE(changed.empty()) << "the void-rejected tick must change "
                                   "nothing (the frontier cell only "
                                   "proposes across the boundary)";
    // Two more no-change ticks pin the COLD state (the stuck frontier).
    EXPECT_TRUE(tick(world).empty());
    EXPECT_TRUE(tick(world).empty());

    // The east column re-appears (the streaming re-load, synchronous
    // generation — the getOrCreateChunk path, the applyWorldEdits twin):
    // the settled frontier must wake and resume across the boundary.
    makeFullColumn(world, kEastCX, 0);
    changed = tick(world);
    ASSERT_FALSE(changed.empty())
        << "the re-appeared column must wake the settled frontier "
           "(99.8 streaming-boundary hole)";
    EXPECT_EQ(world.getFluid(kFrontierX + 1, kFlowY, 0), 4);
    changed = tick(world);
    ASSERT_FALSE(changed.empty());
    // The cascade crossed the boundary: L4 at x=192, L3 at x=193
    // (two ticks after the column's re-appearance — within the budget).
    EXPECT_EQ(world.getFluid(kFrontierX + 1, kFlowY, 0), 4);
    EXPECT_EQ(world.getFluid(kFrontierX + 2, kFlowY, 0), 3);
}

// The reverse direction: a settled DRY depression (the west column, one
// column below the sea level) meets a freshly generated sea (the east
// column's L8 slab, hot from its generation scan): the sea flows into
// the depression within two ticks — the depression cell (one below the
// sea surface) fills to L7.
TEST(FluidStreamingGateTest, GeneratedSeaFillsTheDryDepression) {
    World world;
    world.setChunkGenerator(depressionSeaGenerator());
    makeFullColumn(world, kWestCX, 0); // the dry depression (settled: no
                                       // fluid anywhere — nothing to walk)
    EXPECT_TRUE(tick(world).empty());

    // The east column (the sea) appears:
    makeFullColumn(world, kEastCX, 0);
    // Tick 1: the sea's west edge spreads into the depression's mouth
    // (one above the trench floor — air); the sea slab itself is stable
    // (every cell L8, neighbors L8).
    const std::vector<BlockPos> first = tick(world);
    ASSERT_FALSE(first.empty());
    EXPECT_EQ(world.getFluid(kTrenchX, kSeaY, 0), 7);
    // Tick 2: the mouth cell falls into the trench (the floor is one
    // lower) and rests at the depression's bottom.
    const std::vector<BlockPos> second = tick(world);
    ASSERT_FALSE(second.empty());
    EXPECT_EQ(world.getFluid(kTrenchX, kSeaY - 1, 0), 7);
    EXPECT_EQ(world.getFluid(kTrenchX, kSeaY, 0), 0);
}

// The 99.8 performance side stays intact: a fully settled world (the
// sealed slabs — no cell can propose anything) with NO chunk insertion
// walks nothing on every tick (empty changed sets, every fluid chunk
// cold for both kinds). The wake is an insertion-only event: without
// insertions it cannot add a single walk.
TEST(FluidStreamingGateTest, SettledWorldStaysColdWithoutInsertions) {
    World world;
    world.setChunkGenerator(sealedSlabGenerator());
    makeFullColumn(world, kWestCX, 0);
    ASSERT_EQ(world.fluidChunkPositions(FluidKind::Water).size(), 1u);
    ASSERT_EQ(world.fluidChunkPositions(FluidKind::Lava).size(), 1u);

    // Tick 1 consumes the generation's hot flags and changes nothing
    // (the sealed slabs are already at rest); ticks 2-6 must do the
    // same — a settled world is a no-op for the simulation.
    for (int i = 1; i <= 6; ++i) {
        const std::vector<BlockPos> changed = tick(world);
        EXPECT_TRUE(changed.empty()) << "settled tick " << i << " changed";
    }

    // Every fluid chunk is COLD for both kinds (the activity gate's
    // observable: the next tick would walk nothing).
    for (const FluidKind kind : {FluidKind::Water, FluidKind::Lava}) {
        for (const ChunkPos& cp : world.fluidChunkPositions(kind)) {
            const Chunk* c = world.findChunk(cp);
            ASSERT_NE(c, nullptr);
            EXPECT_FALSE(c->fluidHot(kind))
                << "kind " << static_cast<int>(kind) << " still hot";
            EXPECT_FALSE(c->fluidWalkPending(kind))
                << "kind " << static_cast<int>(kind)
                << " walk pending without an insertion";
        }
    }
}
