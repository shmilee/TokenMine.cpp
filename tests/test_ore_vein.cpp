// GEN-3 ore-vein tests: the planOreVein pure function — the 21-cell 3x2x1
// integer ellipsoid (boundary cells pinned), the six axis-permutation
// orientations, the PITS #4 deterministic stream, and the multi-cluster
// bounds (dedup + the margin contract the generator's scan relies on).
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <set>
#include <vector>

#include "world/noise.hpp"
#include "world/ore_vein.hpp"

using namespace mc;

namespace {

// The orientation the first draw yields for (seed, origin) — the test's
// window into the plan's stream (all constants are public by design, the
// CaveMatchesPurePredicate pattern).
int orientationOf(std::uint64_t seed, const BlockPos& origin) {
    const std::uint64_t state = hash3(
        static_cast<std::uint64_t>(static_cast<std::int64_t>(origin.x)),
        static_cast<std::uint64_t>(static_cast<std::int64_t>(origin.y)),
        static_cast<std::uint64_t>(static_cast<std::int64_t>(origin.z)),
        seed ^ kOreVeinSalt);
    return static_cast<int>(splitmix64(state + kOreVeinFeatureStep) % 6);
}

// The per-axis extents (max |offset|) of a K=1 plan — a permutation of
// (3, 2, 1) for a full ellipsoid, whichever orientation it has.
std::vector<int> extents(const std::vector<BlockPos>& plan,
                         const BlockPos& origin) {
    int ex = 0, ey = 0, ez = 0;
    for (const BlockPos& p : plan) {
        ex = std::max(ex, std::abs(p.x - origin.x));
        ey = std::max(ey, std::abs(p.y - origin.y));
        ez = std::max(ez, std::abs(p.z - origin.z));
    }
    std::vector<int> v = {ex, ey, ez};
    std::sort(v.begin(), v.end());
    return v;
}

bool contains(const std::vector<BlockPos>& plan, const BlockPos& p) {
    return std::find(plan.begin(), plan.end(), p) != plan.end();
}

bool hasDuplicates(const std::vector<BlockPos>& plan) {
    std::set<BlockPos> seen;
    for (const BlockPos& p : plan) {
        if (seen.count(p) != 0) {
            return true;
        }
        seen.insert(p);
    }
    return false;
}

} // namespace

TEST(OreVeinTest, K1IsExactly21UniqueCells) {
    const std::uint64_t seed = 0xC0FFEE123456ULL;
    const BlockPos origin {37, 20, -11};
    const std::vector<BlockPos> plan = planOreVein(seed, origin, 1);
    ASSERT_EQ(plan.size(), kOreVeinCellCount);
    EXPECT_EQ(kOreVeinCellCount, 21); // the named constant itself
    EXPECT_FALSE(hasDuplicates(plan));
    // The centre cell (0, 0, 0 of the local frame) is always a cell.
    EXPECT_TRUE(contains(plan, origin));
    // A full ellipsoid's per-axis extents are a permutation of (3, 2, 1).
    EXPECT_EQ(extents(plan, origin), (std::vector<int> {1, 2, 3}));
    // Central symmetry: every offset has its opposite.
    for (const BlockPos& p : plan) {
        const BlockPos opposite {2 * origin.x - p.x, 2 * origin.y - p.y,
                                 2 * origin.z - p.z};
        EXPECT_TRUE(contains(plan, opposite))
            << "missing opposite of (" << p.x << ", " << p.y << ", " << p.z
            << ")";
    }
}

TEST(OreVeinTest, OrientationZeroBoundaryCellsPinned) {
    // Scan for a (seed, origin) whose first draw is orientation 0
    // (axisMap {0,1,2}: u->x, v->y, w->z) — the local frame IS the world
    // frame, so the integer ellipsoid's boundary pins directly:
    // (3,0,0) in, (0,2,0) in, (0,0,1) in, (3,1,0) OUT, (0,2,1) OUT.
    const std::uint64_t seed = 7;
    int foundX = -1, foundZ = -1;
    for (int x = 0; x < 400 && foundX < 0; ++x) {
        for (int z = 0; z < 400 && foundX < 0; ++z) {
            if (orientationOf(seed, BlockPos {x, 21, z}) == 0) {
                foundX = x;
                foundZ = z;
            }
        }
    }
    ASSERT_GE(foundX, 0) << "no orientation-0 origin found (stream broken)";
    const BlockPos origin {foundX, 21, foundZ};
    const std::vector<BlockPos> plan = planOreVein(seed, origin, 1);
    ASSERT_EQ(plan.size(), kOreVeinCellCount);
    EXPECT_TRUE(contains(plan, BlockPos {origin.x + 3, origin.y, origin.z}));
    EXPECT_TRUE(contains(plan, BlockPos {origin.x - 3, origin.y, origin.z}));
    EXPECT_TRUE(contains(plan, BlockPos {origin.x, origin.y + 2, origin.z}));
    EXPECT_TRUE(contains(plan, BlockPos {origin.x, origin.y, origin.z + 1}));
    EXPECT_TRUE(contains(plan, BlockPos {origin.x, origin.y, origin.z - 1}));
    EXPECT_FALSE(contains(plan,
                          BlockPos {origin.x + 3, origin.y + 1, origin.z}));
    EXPECT_FALSE(contains(plan,
                          BlockPos {origin.x, origin.y + 2, origin.z + 1}));
    // The UNsorted extents pin the orientation's axis order: x carries
    // the 3-semi-axis, y the 2, z the 1.
    int ex = 0, ey = 0, ez = 0;
    for (const BlockPos& p : plan) {
        ex = std::max(ex, std::abs(p.x - origin.x));
        ey = std::max(ey, std::abs(p.y - origin.y));
        ez = std::max(ez, std::abs(p.z - origin.z));
    }
    EXPECT_EQ(ex, 3);
    EXPECT_EQ(ey, 2);
    EXPECT_EQ(ez, 1);
}

TEST(OreVeinTest, AllSixOrientationsAppearWithPermutationExtents) {
    // A scan of origins must yield all six axis permutations; each plan's
    // extents are (1,2,3) sorted and match the permutation's world order.
    const std::uint64_t seed = 99;
    std::set<int> seen;
    for (int x = 0; x < 600 && seen.size() < 6; ++x) {
        for (int z = 0; z < 600 && seen.size() < 6; ++z) {
            const BlockPos origin {x, 17, z};
            const int o = orientationOf(seed, origin);
            if (seen.count(o) != 0) {
                continue;
            }
            seen.insert(o);
            const std::vector<BlockPos> plan = planOreVein(seed, origin, 1);
            ASSERT_EQ(plan.size(), kOreVeinCellCount) << "orientation " << o;
            // The extents' SORTED form is always (1, 2, 3).
            EXPECT_EQ(extents(plan, origin), (std::vector<int> {1, 2, 3}))
                << "orientation " << o;
        }
    }
    ASSERT_EQ(seen.size(), 6u) << "all six orientations must occur";
}

TEST(OreVeinTest, DeterministicAndSeedSensitive) {
    const BlockPos origin {5, 12, 9};
    const std::vector<BlockPos> a = planOreVein(1, origin, 3);
    const std::vector<BlockPos> b = planOreVein(1, origin, 3);
    EXPECT_EQ(a, b);
    // A different seed plans a different vein (scan for the first
    // divergence — a constant stream would never differ, PITS #4).
    bool differs = false;
    for (std::uint64_t s = 2; s < 100 && !differs; ++s) {
        if (planOreVein(s, origin, 3) != a) {
            differs = true;
        }
    }
    EXPECT_TRUE(differs);
    // A different origin plans a different vein (position decorrelation).
    differs = false;
    for (int x = 1; x < 100 && !differs; ++x) {
        if (planOreVein(1, BlockPos {5 + x, 12, 9}, 3) != a) {
            differs = true;
        }
    }
    EXPECT_TRUE(differs);
}

TEST(OreVeinTest, ClusterBoundsAndDedup) {
    // K = 2: at most 2 ellipsoids (42 cells), deduped, and every cell
    // within the margin bound 3 + 2*(K-1) = 5 of the start.
    const std::uint64_t seed = 42;
    for (int x = 0; x < 30; ++x) {
        const BlockPos origin {x * 7, 15, x * 13};
        const std::vector<BlockPos> plan = planOreVein(seed, origin, 2);
        EXPECT_GE(plan.size(), 21u);
        EXPECT_LE(plan.size(), 42u);
        EXPECT_FALSE(hasDuplicates(plan));
        for (const BlockPos& p : plan) {
            EXPECT_LE(std::abs(p.x - origin.x), 5);
            EXPECT_LE(std::abs(p.y - origin.y), 5);
            EXPECT_LE(std::abs(p.z - origin.z), 5);
        }
    }
    // K = 8 (the ceiling): at most 168 cells, margin 3 + 2*7 = 17.
    const BlockPos o {3, 9, -3};
    const std::vector<BlockPos> big = planOreVein(seed, o, 8);
    EXPECT_LE(big.size(), 168u);
    EXPECT_FALSE(hasDuplicates(big));
    for (const BlockPos& p : big) {
        EXPECT_LE(std::abs(p.x - o.x), 17);
        EXPECT_LE(std::abs(p.y - o.y), 17);
        EXPECT_LE(std::abs(p.z - o.z), 17);
    }
    // Out-of-domain clamps (a logic guard, not a data path): K < 1 and
    // K > 8 behave like 1 and 8.
    EXPECT_EQ(planOreVein(seed, o, 0), planOreVein(seed, o, 1));
    EXPECT_EQ(planOreVein(seed, o, 99), planOreVein(seed, o, 8));
}
