#include <gtest/gtest.h>

#include "world/world.hpp"

using namespace mc;

TEST(WorldTest, EmptyWorldReadsAir) {
    World world;
    EXPECT_EQ(world.getBlock(0, 0, 0), BlockID::air());
    EXPECT_EQ(world.getBlock(-5, 3, 10), BlockID::air());
    EXPECT_EQ(world.getBlock(1000, 4, -1000), BlockID::air());
    EXPECT_EQ(world.chunkCount(), 0u);
}

TEST(WorldTest, SetGetRoundTripCreatesChunk) {
    World world;
    ASSERT_TRUE(world.setBlock(3, 5, 3, BlockID{1}));
    EXPECT_EQ(world.getBlock(3, 5, 3), BlockID{1});
    EXPECT_EQ(world.chunkCount(), 1u);
    EXPECT_TRUE(world.hasChunk({0, 0, 0}));
    EXPECT_EQ(world.findChunk({0, 0, 0}), &world.getOrCreateChunk({0, 0, 0}));
}

TEST(WorldTest, NegativeCoordinatesLandInNegativeChunks) {
    World world;
    ASSERT_TRUE(world.setBlock(-1, 5, -1, BlockID{2}));
    // -1 floors to chunk -1, local coordinate 15 (not chunk 0, local 15
    // via truncation — that is the bug class these tests guard).
    EXPECT_TRUE(world.hasChunk({-1, 0, -1}));
    EXPECT_FALSE(world.hasChunk({0, 0, 0}));
    EXPECT_EQ(world.getBlock(-1, 5, -1), BlockID{2});
    EXPECT_EQ(world.getBlock(0, 5, 0), BlockID::air());
    const Chunk* chunk = world.findChunk({-1, 0, -1});
    ASSERT_NE(chunk, nullptr);
    EXPECT_EQ(chunk->get(15, 5, 15), BlockID{2});
}

TEST(WorldTest, SetBlockOutOfRangeYRefused) {
    World world;
    EXPECT_FALSE(world.setBlock(0, -1, 0, BlockID{1}));
    EXPECT_FALSE(world.setBlock(0, kWorldHeight, 0, BlockID{1}));
    EXPECT_EQ(world.chunkCount(), 0u); // nothing created
    // The topmost valid row works.
    EXPECT_TRUE(world.setBlock(0, kWorldHeight - 1, 0, BlockID{1}));
    EXPECT_EQ(world.getBlock(0, kWorldHeight - 1, 0), BlockID{1});
}

TEST(WorldTest, SetBlockInvalidIdRefused) {
    World world;
    EXPECT_FALSE(world.setBlock(0, 5, 0, BlockID::invalid()));
    EXPECT_EQ(world.chunkCount(), 0u);
}

TEST(WorldTest, GetBlockOutOfRangeYReadsAir) {
    World world;
    ASSERT_TRUE(world.setBlock(0, 0, 0, BlockID{1}));
    EXPECT_EQ(world.getBlock(0, -1, 0), BlockID::air());
    EXPECT_EQ(world.getBlock(0, kWorldHeight, 0), BlockID::air());
}

TEST(WorldTest, GeneratorRunsOnlyOnCreation) {
    World world;
    int calls = 0;
    world.setChunkGenerator([&calls](Chunk& chunk, const ChunkPos& pos) {
        ++calls;
        chunk.set(pos.x, pos.y, pos.z, BlockID{9});
    });
    world.getOrCreateChunk({2, 0, 2});
    EXPECT_EQ(calls, 1);
    world.getOrCreateChunk({2, 0, 2}); // existing: no re-generation
    EXPECT_EQ(calls, 1);
    // Local (2,0,2) of chunk (2,0,2) is world (34, 0, 34).
    EXPECT_EQ(world.getBlock(34, 0, 34), BlockID{9});
    EXPECT_EQ(world.getBlock(33, 0, 34), BlockID::air());
}

TEST(WorldTest, GeneratorDeterministicAcrossClear) {
    // clear() keeps the generator; re-creating a chunk regenerates it
    // identically (the property Phase 4's seeded terrain will rely on).
    World world;
    world.setChunkGenerator([](Chunk& chunk, const ChunkPos& pos) {
        chunk.set(pos.x * 5 + 1, 0, pos.z * 5 + 1, BlockID{4});
    });
    world.getOrCreateChunk({0, 0, 0});
    const BlockID first = world.getBlock(1, 0, 1);
    world.clear();
    world.getOrCreateChunk({0, 0, 0});
    EXPECT_EQ(world.getBlock(1, 0, 1), first);
}

TEST(WorldTest, BoundarySetMarksNeighborDirty) {
    World world;
    world.getOrCreateChunk({0, 0, 0});
    world.getOrCreateChunk({1, 0, 0});
    world.getOrCreateChunk({0, 0, 0}).clearDirty();
    world.getOrCreateChunk({1, 0, 0}).clearDirty();

    // Local x=15 of chunk 0 shares a face with local x=0 of chunk 1:
    // editing it must invalidate BOTH meshes.
    ASSERT_TRUE(world.setBlock(15, 4, 4, BlockID{1}));
    EXPECT_TRUE(world.findChunk({0, 0, 0})->dirty());
    EXPECT_TRUE(world.findChunk({1, 0, 0})->dirty());

    // Interior block: only its own chunk.
    world.getOrCreateChunk({0, 0, 0}).clearDirty();
    world.getOrCreateChunk({1, 0, 0}).clearDirty();
    ASSERT_TRUE(world.setBlock(8, 4, 4, BlockID{1}));
    EXPECT_TRUE(world.findChunk({0, 0, 0})->dirty());
    EXPECT_FALSE(world.findChunk({1, 0, 0})->dirty());
}

TEST(WorldTest, BoundarySetAtNegativeWorldEdge) {
    World world;
    world.getOrCreateChunk({-2, 0, 0});
    world.getOrCreateChunk({-1, 0, 0});
    world.getOrCreateChunk({-2, 0, 0}).clearDirty();
    world.getOrCreateChunk({-1, 0, 0}).clearDirty();
    // World x=-17 is local x=15 in chunk -2; the neighbor is chunk -1.
    ASSERT_TRUE(world.setBlock(-17, 3, 3, BlockID{1}));
    EXPECT_TRUE(world.findChunk({-2, 0, 0})->dirty());
    EXPECT_TRUE(world.findChunk({-1, 0, 0})->dirty());
    // And the block really sits on the boundary.
    EXPECT_EQ(world.findChunk({-2, 0, 0})->get(15, 3, 3), BlockID{1});
}

TEST(WorldTest, VerticalBoundaryMarksNeighborDirty) {
    World world;
    world.getOrCreateChunk({0, 0, 0});
    world.getOrCreateChunk({0, 1, 0});
    world.getOrCreateChunk({0, 0, 0}).clearDirty();
    world.getOrCreateChunk({0, 1, 0}).clearDirty();
    ASSERT_TRUE(world.setBlock(4, 15, 4, BlockID{1})); // top face of lower chunk
    EXPECT_TRUE(world.findChunk({0, 0, 0})->dirty());
    EXPECT_TRUE(world.findChunk({0, 1, 0})->dirty());
}

TEST(WorldTest, BoundarySetWithoutNeighborOnlyMarksSelf) {
    World world;
    world.getOrCreateChunk({0, 0, 0});
    world.getOrCreateChunk({0, 0, 0}).clearDirty();
    // No chunk at {1,0,0}: nothing to invalidate on that side.
    ASSERT_TRUE(world.setBlock(15, 4, 4, BlockID{1}));
    EXPECT_TRUE(world.findChunk({0, 0, 0})->dirty());
    EXPECT_EQ(world.chunkCount(), 1u);
}

TEST(WorldTest, ChunkPositionsSorted) {
    World world;
    world.getOrCreateChunk({3, 0, -2});
    world.getOrCreateChunk({0, 5, 0});
    world.getOrCreateChunk({-3, 0, 1});
    world.getOrCreateChunk({0, 0, 1});
    const std::vector<ChunkPos> positions = world.chunkPositions();
    ASSERT_EQ(positions.size(), 4u);
    for (std::size_t i = 1; i < positions.size(); ++i) {
        EXPECT_LT(positions[i - 1], positions[i]);
    }
}
