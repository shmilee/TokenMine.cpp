// Coordinate transform tests. Negative coordinates are the critical case:
// C++ integer division truncates toward zero (-1 / 16 == 0), which would
// silently place block x=-1 into chunk 0. Chunk assignment must floor.
#include <gtest/gtest.h>

#include "world/coordinates.hpp"

using namespace mc;

TEST(CoordinatesTest, FloorDivPositive) {
    EXPECT_EQ(floorDiv(0, 16), 0);
    EXPECT_EQ(floorDiv(15, 16), 0);
    EXPECT_EQ(floorDiv(16, 16), 1);
    EXPECT_EQ(floorDiv(31, 16), 1);
    EXPECT_EQ(floorDiv(32, 16), 2);
    EXPECT_EQ(floorDiv(1000, 16), 62);
}

TEST(CoordinatesTest, FloorDivNegative) {
    EXPECT_EQ(floorDiv(-1, 16), -1);
    EXPECT_EQ(floorDiv(-15, 16), -1);
    EXPECT_EQ(floorDiv(-16, 16), -1);
    EXPECT_EQ(floorDiv(-17, 16), -2);
    EXPECT_EQ(floorDiv(-31, 16), -2);
    EXPECT_EQ(floorDiv(-32, 16), -2);
}

TEST(CoordinatesTest, WorldToChunkFloorsNegativeCoords) {
    // The trap: plain C++ division gives -1/16 == 0, but block -1 belongs
    // to chunk -1.
    EXPECT_EQ(worldToChunk({-1, 0, 0}), (ChunkPos{-1, 0, 0}));
    EXPECT_EQ(worldToChunk({-16, 0, 0}), (ChunkPos{-1, 0, 0}));
    EXPECT_EQ(worldToChunk({-17, 0, 0}), (ChunkPos{-2, 0, 0}));
    EXPECT_EQ(worldToChunk({0, 0, 0}), (ChunkPos{0, 0, 0}));
    EXPECT_EQ(worldToChunk({15, 31, -1}), (ChunkPos{0, 1, -1}));
    EXPECT_EQ(worldToChunk({-15, -16, 16}), (ChunkPos{-1, -1, 1}));
}

TEST(CoordinatesTest, ChunkOriginIsChunkCorner) {
    EXPECT_EQ(chunkOrigin({0, 0, 0}), (BlockPos{0, 0, 0}));
    EXPECT_EQ(chunkOrigin({1, 2, -1}), (BlockPos{16, 32, -16}));
    EXPECT_EQ(chunkOrigin({-2, 0, 3}), (BlockPos{-32, 0, 48}));
}

TEST(CoordinatesTest, WorldLocalRoundTrip) {
    // For a grid of world coordinates (heavily negative), the decomposition
    // chunk origin + local must reproduce the input, and local coordinates
    // must fall in [0, kChunkSize).
    const int xs[] = {-33, -17, -16, -15, -1, 0, 1, 15, 16, 17, 33,
                      12345, -12345};
    for (int x : xs) {
        for (int z : xs) {
            const BlockPos p{x, 5, z};
            const ChunkPos c = worldToChunk(p);
            const BlockPos o = chunkOrigin(c);
            const BlockPos local = worldToChunkLocal(c, p);

            EXPECT_GE(local.x, 0) << "x=" << x;
            EXPECT_LT(local.x, kChunkSize) << "x=" << x;
            EXPECT_GE(local.z, 0) << "z=" << z;
            EXPECT_LT(local.z, kChunkSize) << "z=" << z;

            EXPECT_EQ(o.x + local.x, x);
            EXPECT_EQ(o.z + local.z, z);
            EXPECT_EQ(chunkLocalToWorld(c, local.x, local.y, local.z), p);
        }
    }
}

TEST(CoordinatesTest, LocalToWorldRoundTrip) {
    const ChunkPos c{-2, 3, -1};
    for (int y : {0, 7, 15}) {
        for (int x : {0, 7, 15}) {
            for (int z : {0, 7, 15}) {
                const BlockPos w = chunkLocalToWorld(c, x, y, z);
                EXPECT_EQ(worldToChunk(w), c);
                EXPECT_EQ(worldToChunkLocal(c, w), (BlockPos{x, y, z}));
            }
        }
    }
}

TEST(CoordinatesTest, WorldYRange) {
    EXPECT_FALSE(isWorldYValid(-1));
    EXPECT_TRUE(isWorldYValid(0));
    EXPECT_TRUE(isWorldYValid(kWorldHeight / 2));
    EXPECT_TRUE(isWorldYValid(kWorldHeight - 1));
    EXPECT_FALSE(isWorldYValid(kWorldHeight));
    EXPECT_FALSE(isWorldYValid(kWorldHeight + 100));
}
