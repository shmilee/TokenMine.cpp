// Chunk streaming tests (Phase 14): the pure decision logic — missing
// column enumeration (radius, deterministic nearest-first order) and the
// idle-player trigger (a stationary player in the center of the loaded
// area must never activate streaming).
#include <gtest/gtest.h>

#include <vector>

#include "world/chunk_streaming.hpp"

using namespace mc;

namespace {

// The columns of the square [min, max] x [min, max].
std::vector<ColumnPos> square(int min, int max) {
    std::vector<ColumnPos> out;
    for (int x = min; x <= max; ++x) {
        for (int z = min; z <= max; ++z) {
            out.push_back({x, z});
        }
    }
    return out;
}

} // namespace

TEST(ChunkStreamingTest, ColumnDistanceIsChebyshev) {
    EXPECT_EQ(columnDistance({0, 0}, {0, 0}), 0);
    EXPECT_EQ(columnDistance({0, 0}, {3, -4}), 4);
    EXPECT_EQ(columnDistance({-2, 5}, {4, 5}), 6);
}

TEST(ChunkStreamingTest, MissingColumnsSortedByDistanceThenXThenZ) {
    // Player in the center of the 4x4 loaded square (the demo area),
    // radius 2: exactly the ring at Chebyshev distance 2 is missing, all
    // tied on distance and sorted by (x, z).
    const ColumnPos player{0, 0};
    const std::vector<ColumnPos> missing =
        missingColumns(player, square(-2, 1), 2);
    const std::vector<ColumnPos> expect = {
        {-2, 2}, {-1, 2}, {0, 2}, {1, 2},
        {2, -2}, {2, -1}, {2, 0}, {2, 1}, {2, 2}};
    EXPECT_EQ(missing, expect);
}

TEST(ChunkStreamingTest, MissingColumnsRadiusBoundaryInclusive) {
    const ColumnPos player{0, 0};
    // Radius 1 with nothing loaded: the full 3x3 ring, player first.
    std::vector<ColumnPos> missing = missingColumns(player, {}, 1);
    ASSERT_EQ(missing.size(), 9u);
    EXPECT_EQ(missing.front(), (ColumnPos {0, 0}));
    // Radius 0 = only the player's own column.
    missing = missingColumns(player, {}, 0);
    ASSERT_EQ(missing.size(), 1u);
    EXPECT_EQ(missing.front(), (ColumnPos {0, 0}));
    // A loaded column inside the radius drops out.
    EXPECT_TRUE(missingColumns(player, {{0, 0}}, 0).empty());
    // The far edge of the ring is included (Chebyshev, not Euclidean).
    missing = missingColumns(player, {{0, 0}}, 1);
    EXPECT_EQ(missing.size(), 8u); // the 3x3 ring minus the loaded center
}

TEST(ChunkStreamingTest, MissingColumnsNearestRingFirst) {
    const ColumnPos player{3, -2};
    // Only the player's own column is loaded; radius 2 -> rings 1 and 2.
    const std::vector<ColumnPos> missing =
        missingColumns(player, {{3, -2}}, 2);
    ASSERT_EQ(missing.size(), 24u);
    for (int i = 0; i < 8; ++i) {
        EXPECT_EQ(columnDistance(player, missing[i]), 1);
    }
    for (int i = 8; i < 24; ++i) {
        EXPECT_EQ(columnDistance(player, missing[i]), 2);
    }
}

TEST(ChunkStreamingTest, NearestMissingDistance) {
    const std::vector<ColumnPos> loaded = square(-2, 1); // the demo area
    // Center of the demo area: the nearest missing column is 2 away
    // (e.g. (2, 0)); the edge-adjacent column (1, 0) sees distance 1; a
    // column standing in missing territory sees 0.
    EXPECT_EQ(nearestMissingDistance({0, 0}, loaded, 8), 2);
    EXPECT_EQ(nearestMissingDistance({1, 0}, loaded, 8), 1);
    EXPECT_EQ(nearestMissingDistance({2, 0}, loaded, 8), 0);
    // The radius bounds the search: with radius 0 the (0,0) player's
    // ring is just its own (loaded) column -> no missing in range, and
    // the answer is radius + 1, distinct from the radius-8 distance.
    EXPECT_EQ(nearestMissingDistance({0, 0}, loaded, 0), 1);
    // A fully loaded ring reports the radius + 1 sentinel.
    const std::vector<ColumnPos> all = square(-8, 8);
    EXPECT_EQ(nearestMissingDistance({0, 0}, all, 8), 9);
    // ...and a fully loaded ring must never engage the streamer, even
    // when the trigger distance reaches the sentinel.
    EXPECT_FALSE(streamingActive({0, 0}, all, 8, 8));
}

TEST(ChunkStreamingTest, StreamingInactiveForIdlePlayerInCenter) {
    // The demo area (4x4 columns) with the player at its center: the
    // nearest missing column is at distance 2, beyond the trigger
    // distance — no streaming, ever, no matter how large the radius.
    const std::vector<ColumnPos> loaded = square(-2, 1);
    for (int radius : {1, 8, 32}) {
        EXPECT_FALSE(streamingActive({0, 0}, loaded, radius, 1))
            << "radius " << radius;
    }
}

TEST(ChunkStreamingTest, StreamingActiveAtTheLoadedEdge) {
    const std::vector<ColumnPos> loaded = square(-2, 1);
    // One column toward the edge: (2, 0) is adjacent and missing.
    EXPECT_TRUE(streamingActive({1, 0}, loaded, 8, 1));
    // Standing in a missing column (outran the streamer): active.
    EXPECT_TRUE(streamingActive({2, 0}, loaded, 8, 1));
    // Deep in an unexplored region: the whole ring is missing.
    EXPECT_TRUE(streamingActive({9, 9}, loaded, 8, 1));
}

TEST(ChunkStreamingTest, StreamingActiveRespectsRadius) {
    const std::vector<ColumnPos> loaded = square(-2, 1);
    // Player at (1, 0): the adjacent missing column (2, 0) is at distance
    // 1 — outside a radius-0 ring (only the loaded player column), inside
    // a radius-1 ring.
    EXPECT_FALSE(streamingActive({1, 0}, loaded, 0, 1));
    EXPECT_TRUE(streamingActive({1, 0}, loaded, 1, 1));
    // A loaded ring around the player (radius 0) is never active.
    EXPECT_FALSE(streamingActive({1, 0}, loaded, 0, 0));
}
