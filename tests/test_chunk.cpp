#include <gtest/gtest.h>

#include "world/chunk.hpp"

using namespace mc;

TEST(ChunkTest, NewChunkIsAirAndDirty) {
    Chunk chunk;
    EXPECT_TRUE(chunk.dirty());
    EXPECT_EQ(chunk.get(0, 0, 0), BlockID::air());
    EXPECT_EQ(chunk.get(15, 15, 15), BlockID::air());
    EXPECT_EQ(chunk.get(8, 8, 8), BlockID::air());
}

TEST(ChunkTest, SetGetRoundTrip) {
    Chunk chunk;
    const BlockID stone = BlockID{1};
    chunk.set(3, 4, 5, stone);
    EXPECT_EQ(chunk.get(3, 4, 5), stone);
    EXPECT_EQ(chunk.get(2, 4, 5), BlockID::air());
    EXPECT_EQ(chunk.get(4, 4, 5), BlockID::air());
    EXPECT_TRUE(chunk.dirty());
    chunk.clearDirty();
    EXPECT_FALSE(chunk.dirty());
    chunk.set(0, 0, 0, BlockID{2});
    EXPECT_TRUE(chunk.dirty()); // any set re-dirties
}

TEST(ChunkTest, IndexLayout) {
    // x varies fastest, then z, then y.
    EXPECT_EQ(Chunk::index(0, 0, 0), 0u);
    EXPECT_EQ(Chunk::index(1, 0, 0), 1u);
    EXPECT_EQ(Chunk::index(15, 0, 0), 15u);
    EXPECT_EQ(Chunk::index(0, 0, 1), 16u);
    EXPECT_EQ(Chunk::index(0, 1, 0), 256u);
    EXPECT_EQ(Chunk::index(15, 15, 15), static_cast<std::size_t>(Chunk::kVolume - 1));
    EXPECT_EQ(Chunk::kVolume, 4096);
    // index() and get()/set() agree.
    Chunk chunk;
    chunk.set(1, 2, 3, BlockID{5});
    EXPECT_EQ(chunk.data()[Chunk::index(1, 2, 3)], BlockID{5});
}

TEST(ChunkTest, FillAndClear) {
    Chunk chunk;
    chunk.fill(BlockID{7});
    EXPECT_TRUE(chunk.dirty());
    for (int y : {0, 8, 15}) {
        for (int x : {0, 8, 15}) {
            EXPECT_EQ(chunk.get(x, y, 8), BlockID{7});
        }
    }
    chunk.clear();
    EXPECT_EQ(chunk.get(7, 7, 7), BlockID::air());
    EXPECT_TRUE(chunk.dirty());
}

TEST(ChunkTest, MarkDirty) {
    Chunk chunk;
    chunk.clearDirty();
    EXPECT_FALSE(chunk.dirty());
    chunk.markDirty();
    EXPECT_TRUE(chunk.dirty());
    chunk.clearDirty();
    EXPECT_FALSE(chunk.dirty());
}
