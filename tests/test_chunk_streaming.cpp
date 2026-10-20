// Chunk streaming tests (Phase 14, STRM-1): the pure decision logic —
// missing column enumeration over the DISK (radius, deterministic
// nearest-first order by squared Euclidean distance) and the idle-player
// trigger (a stationary player in the center of the loaded area must
// never activate streaming).
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <vector>

#include "world/chunk_streaming.hpp"

using namespace mc;

namespace {

// The columns of the square [min, max] x [min, max]. The demo area is a
// square (it predates the disk ring, STRM-1).
std::vector<ColumnPos> square(int min, int max) {
    std::vector<ColumnPos> out;
    for (int x = min; x <= max; ++x) {
        for (int z = min; z <= max; ++z) {
            out.push_back({x, z});
        }
    }
    return out;
}

// The columns of the disk around `center`: squared Euclidean distance
// <= radius² (the stream ring, STRM-1).
std::vector<ColumnPos> disk(const ColumnPos& center, int radius) {
    const std::int64_t r2 = static_cast<std::int64_t>(radius) * radius;
    std::vector<ColumnPos> out;
    for (int dx = -radius; dx <= radius; ++dx) {
        for (int dz = -radius; dz <= radius; ++dz) {
            if (static_cast<std::int64_t>(dx) * dx +
                    static_cast<std::int64_t>(dz) * dz <= r2) {
                out.push_back({center.x + dx, center.z + dz});
            }
        }
    }
    return out;
}

bool contains(const std::vector<ColumnPos>& v, const ColumnPos& c) {
    return std::find(v.begin(), v.end(), c) != v.end();
}

} // namespace

TEST(ChunkStreamingTest, ColumnDistanceSqIsSquaredEuclidean) {
    EXPECT_EQ(columnDistanceSq({0, 0}, {0, 0}), 0);
    EXPECT_EQ(columnDistanceSq({0, 0}, {3, -4}), 25); // 3² + 4²
    EXPECT_EQ(columnDistanceSq({-2, 5}, {4, 5}), 36); // 6² + 0²
    // The diagonal: squared Euclidean 2 where the old Chebyshev said 1 —
    // the trigger at distance 1 no longer sees diagonals (STRM-1).
    EXPECT_EQ(columnDistanceSq({0, 0}, {1, 1}), 2);
}

TEST(ChunkStreamingTest, MissingColumnsSortedByDistanceSqThenXThenZ) {
    // Player in the center of the 4x4 loaded square (the demo area),
    // radius 2: the disk pokes two columns past the square's edge —
    // (2, 0) and (0, 2), tied on distanceSq 4 and sorted by x.
    const ColumnPos player{0, 0};
    const std::vector<ColumnPos> missing =
        missingColumns(player, square(-2, 1), 2);
    const std::vector<ColumnPos> expect = {{0, 2}, {2, 0}};
    EXPECT_EQ(missing, expect);
}

TEST(ChunkStreamingTest, MissingColumnsRadiusBoundaryInclusive) {
    const ColumnPos player{0, 0};
    // Radius 1 with nothing loaded: the 5-column cross (0,0)/(±1,0)/
    // (0,±1) — the diagonals at distanceSq 2 are OUTSIDE the disk (the
    // old 3x3 square held 9 columns).
    std::vector<ColumnPos> missing = missingColumns(player, {}, 1);
    const std::vector<ColumnPos> cross = {
        {0, 0}, {-1, 0}, {0, -1}, {0, 1}, {1, 0}};
    EXPECT_EQ(missing, cross);
    // Radius 0 = only the player's own column.
    missing = missingColumns(player, {}, 0);
    ASSERT_EQ(missing.size(), 1u);
    EXPECT_EQ(missing.front(), (ColumnPos {0, 0}));
    // A loaded column inside the radius drops out.
    EXPECT_TRUE(missingColumns(player, {{0, 0}}, 0).empty());
    // The far edge of the disk is included (distanceSq <= radius²).
    missing = missingColumns(player, {{0, 0}}, 1);
    ASSERT_EQ(missing.size(), 4u);
    EXPECT_FALSE(contains(missing, {1, 1})); // the diagonal stays out
}

TEST(ChunkStreamingTest, MissingColumnsNearestDiskFirst) {
    const ColumnPos player{3, -2};
    // Only the player's own column is loaded; radius 2 -> the disk's
    // other 12 columns: the four axis-neighbors (distanceSq 1) first,
    // then the four diagonals (distanceSq 2), then the four distanceSq-
    // 4 axis-far columns.
    const std::vector<ColumnPos> missing =
        missingColumns(player, {{3, -2}}, 2);
    const std::vector<ColumnPos> expect = {
        {2, -2}, {3, -3}, {3, -1}, {4, -2},   // distanceSq 1
        {2, -3}, {2, -1}, {4, -3}, {4, -1},   // distanceSq 2
        {1, -2}, {3, -4}, {3, 0}, {5, -2}};   // distanceSq 4
    EXPECT_EQ(missing, expect);
}

TEST(ChunkStreamingTest, DiskColumnCountExact) {
    EXPECT_EQ(diskColumnCount(0), 1u);
    EXPECT_EQ(diskColumnCount(1), 5u);
    // The far-plane cap (kStreamCapRadius in core/game.cpp) and the
    // shipped user config (world.view_distance 80): exact lattice
    // counts, pinned (a regression here moves the log "N/M" totals and
    // the streaming budget math).
    EXPECT_EQ(diskColumnCount(64), 12853u);
    EXPECT_EQ(diskColumnCount(80), 20081u);
}

TEST(ChunkStreamingTest, DiskRadius80Boundary) {
    // The squared predicate at the configured radius 80 (r² = 6400):
    // the four boundary lattice points from the STRM-1 spec.
    const std::vector<ColumnPos> disk80 = missingColumns({0, 0}, {}, 80);
    ASSERT_EQ(disk80.size(), 20081u);
    EXPECT_TRUE(contains(disk80, {80, 0}));   // 6400 <= 6400
    EXPECT_FALSE(contains(disk80, {1, 80}));  // 6401 > 6400
    EXPECT_TRUE(contains(disk80, {50, 62}));  // 6344 <= 6400
    EXPECT_FALSE(contains(disk80, {79, 14})); // 6437 > 6400
}

TEST(ChunkStreamingTest, MissingSentinelDistanceSq) {
    // The "whole disk loaded" sentinel is (radius + 1)², strictly above
    // the largest in-disk squared distance (radius²).
    EXPECT_EQ(missingSentinelDistanceSq(0), 1);
    EXPECT_EQ(missingSentinelDistanceSq(1), 4);
    EXPECT_EQ(missingSentinelDistanceSq(8), 81);
    EXPECT_EQ(missingSentinelDistanceSq(64), 4225); // 65²
    EXPECT_EQ(missingSentinelDistanceSq(80), 6561); // 81²
}

TEST(ChunkStreamingTest, NearestMissingDistanceSq) {
    const std::vector<ColumnPos> loaded = square(-2, 1); // the demo area
    // Center of the demo area: the nearest missing columns are (2, 0)
    // and (0, 2) at distanceSq 4; the edge-adjacent column (1, 0) sees
    // distanceSq 1; a column standing in missing territory sees 0.
    EXPECT_EQ(nearestMissingDistanceSq({0, 0}, loaded, 8), 4);
    EXPECT_EQ(nearestMissingDistanceSq({1, 0}, loaded, 8), 1);
    EXPECT_EQ(nearestMissingDistanceSq({2, 0}, loaded, 8), 0);
    // The radius bounds the search: with radius 0 the (0,0) player's
    // disk is just its own (loaded) column -> no missing in range, and
    // the answer is the (radius + 1)² sentinel.
    EXPECT_EQ(nearestMissingDistanceSq({0, 0}, loaded, 0),
              missingSentinelDistanceSq(0));
    // A fully loaded DISK (not the old square) reports the sentinel.
    const std::vector<ColumnPos> all = disk({0, 0}, 8);
    EXPECT_EQ(nearestMissingDistanceSq({0, 0}, all, 8),
              missingSentinelDistanceSq(8));
    // ...and a fully loaded disk must never engage the streamer, even
    // when the trigger distance reaches the sentinel.
    EXPECT_FALSE(streamingActive({0, 0}, all, 8, 8));
}

TEST(ChunkStreamingTest, StreamingInactiveForIdlePlayerInCenter) {
    // The demo area (4x4 columns) with the player at its center: the
    // nearest missing column is (2, 0)/(0, 2) at distanceSq 4, beyond
    // the trigger (1²) — no streaming, ever, no matter how large the
    // radius, including the capped user config values 64 and 80.
    const std::vector<ColumnPos> loaded = square(-2, 1);
    for (int radius : {1, 8, 64, 80}) {
        EXPECT_FALSE(streamingActive({0, 0}, loaded, radius, 1))
            << "radius " << radius;
    }
}

TEST(ChunkStreamingTest, StreamingActiveAtTheLoadedEdge) {
    const std::vector<ColumnPos> loaded = square(-2, 1);
    // One column toward the edge: (2, 0) is axis-adjacent and missing
    // (distanceSq 1).
    EXPECT_TRUE(streamingActive({1, 0}, loaded, 8, 1));
    // Standing in a missing column (outran the streamer): active.
    EXPECT_TRUE(streamingActive({2, 0}, loaded, 8, 1));
    // Deep in an unexplored region: the whole disk is missing.
    EXPECT_TRUE(streamingActive({9, 9}, loaded, 8, 1));
}

TEST(ChunkStreamingTest, StreamingTriggerIsAxisOnly) {
    // The radius-2 disk with everything loaded except the diagonal
    // (1, 1) at distanceSq 2: NOT active — the squared trigger only
    // sees the player's own column + the four axis-neighbors (the old
    // Chebyshev trigger engaged at distance 1 here).
    std::vector<ColumnPos> loaded = disk({0, 0}, 2);
    loaded.erase(std::remove(loaded.begin(), loaded.end(),
                             ColumnPos {1, 1}),
                 loaded.end());
    EXPECT_FALSE(streamingActive({0, 0}, loaded, 2, 1));
    // The moment an AXIS-neighbor is missing (distanceSq 1): active.
    loaded = disk({0, 0}, 2);
    loaded.erase(std::remove(loaded.begin(), loaded.end(),
                             ColumnPos {1, 0}),
                 loaded.end());
    EXPECT_TRUE(streamingActive({0, 0}, loaded, 2, 1));
}

TEST(ChunkStreamingTest, StreamingActiveRespectsRadius) {
    const std::vector<ColumnPos> loaded = square(-2, 1);
    // Player at (1, 0): the axis-adjacent missing column (2, 0) is at
    // distanceSq 1 — outside a radius-0 disk (only the loaded player
    // column), inside a radius-1 disk.
    EXPECT_FALSE(streamingActive({1, 0}, loaded, 0, 1));
    EXPECT_TRUE(streamingActive({1, 0}, loaded, 1, 1));
    // A loaded disk around the player (radius 0) is never active.
    EXPECT_FALSE(streamingActive({1, 0}, loaded, 0, 0));
}

TEST(ChunkStreamingTest, MissingColumnsEuclideanOrderDiffersFromChebyshev) {
    // Four missing columns, all at Chebyshev distance 2 — the old
    // Chebyshev order within that tie was (x, z): (0,2), (1,2), (2,0),
    // (2,1). The squared order groups the axis columns (distanceSq 4)
    // before the near-diagonals (distanceSq 5): (0,2), (2,0), (1,2),
    // (2,1). Player at the origin, disk radius 3, everything else
    // loaded.
    const ColumnPos player{0, 0};
    const std::vector<ColumnPos> all = disk(player, 3);
    const std::vector<ColumnPos> missing4 = {{0, 2}, {2, 0}, {1, 2},
                                             {2, 1}};
    std::vector<ColumnPos> loaded;
    for (const ColumnPos& c : all) {
        if (!contains(missing4, c)) {
            loaded.push_back(c);
        }
    }
    const std::vector<ColumnPos> expect = missing4; // (sq, x, z) order
    EXPECT_EQ(missingColumns(player, loaded, 3), expect);
}
