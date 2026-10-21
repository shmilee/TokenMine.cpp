// GEN-2 tree-shape tests: the pure geometry of a grown tree (planTree's
// three shape tables — the Oak table pinned cell-for-cell against the
// worldgen's placeTrees canopy) and the trunk height roll (deterministic,
// domain-bounded, position-decorrelated — the PITS #4 advancing state).
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "world/coordinates.hpp"
#include "world/tree_shape.hpp"

using namespace mc;

namespace {

// A spec with the OAK shipped geometry (min 4, range 3, radius 2) — the
// worldgen's trees section (min_height 4 / height_range 3 / kTreeLeafRadius
// 2), so the isomorphism test compares like with like.
TreeSpec oakSpec() {
    TreeSpec s;
    s.minHeight = 4;
    s.heightRange = 3;
    s.leafRadius = 2;
    s.shape = TreeShape::Oak;
    return s;
}

// The worldgen's placeTrees cell formula (world_generator.cpp, the
// shipped trunk/canopy loop) re-derived as a SET of offsets relative to
// the sapling root: the trunk at absolute hgt+1..hgt+T maps to offsets
// (0, i, 0) i in [0, T); the canopy at absolute dy = T-1..T+1 (radius 2 /
// 2 / 1) maps to offsets dy-1, the (0, 0) centre skipped while dy <= T
// (offset <= T-1), the radius-2 square's corners rounded.
std::set<std::tuple<int, int, int>> placeTreesCells(int T) {
    std::set<std::tuple<int, int, int>> cells;
    for (int i = 0; i < T; ++i) {
        cells.insert({0, i, 0});
    }
    for (int dy = T - 1; dy <= T + 1; ++dy) {
        const int radius = (dy == T + 1) ? 1 : 2; // kTreeLeafRadius = 2
        for (int dx = -radius; dx <= radius; ++dx) {
            for (int dz = -radius; dz <= radius; ++dz) {
                if (dx == 0 && dz == 0 && dy <= T) {
                    continue; // trunk cell
                }
                if (radius == 2 && std::abs(dx) == radius &&
                    std::abs(dz) == radius) {
                    continue; // round the corners
                }
                cells.insert({dx, dy - 1, dz});
            }
        }
    }
    return cells;
}

std::set<std::tuple<int, int, int>> plannedCells(const std::vector<TreeCell>& c) {
    std::set<std::tuple<int, int, int>> out;
    for (const TreeCell& cell : c) {
        out.insert({cell.offset.x, cell.offset.y, cell.offset.z});
    }
    return out;
}

} // namespace

TEST(TreeShapeTest, ShapeVocabularyAndMapping) {
    // The JSON vocabulary (the parser's acceptance set).
    EXPECT_TRUE(treeShapeValid("oak"));
    EXPECT_TRUE(treeShapeValid("spruce"));
    EXPECT_TRUE(treeShapeValid("birch"));
    EXPECT_FALSE(treeShapeValid("pine"));
    EXPECT_FALSE(treeShapeValid(""));
    EXPECT_FALSE(treeShapeValid("OAK")); // case-sensitive (the data names)
    // The name -> enum mapping (oak is the default fallthrough).
    EXPECT_EQ(treeShapeFromName("oak"), TreeShape::Oak);
    EXPECT_EQ(treeShapeFromName("spruce"), TreeShape::Spruce);
    EXPECT_EQ(treeShapeFromName("birch"), TreeShape::Birch);
}

TEST(TreeShapeTest, PlanTreeOakMatchesPlaceTrees) {
    // The shipped oak heights: min 4 + range 0..3. Every one of them must
    // plan EXACTLY the worldgen's placeTrees cells (the design's
    // cell-for-cell isomorphism — the planted oak grows the generated
    // oak).
    const TreeSpec spec = oakSpec();
    for (int T = 4; T <= 7; ++T) {
        const std::vector<TreeCell> plan = planTree(T, spec);
        EXPECT_EQ(plannedCells(plan), placeTreesCells(T))
            << "height " << T;
        // The exact count: trunk T + the two radius-2 layers (25 - 1
        // centre - 4 corners = 20 each) + the radius-1 cap (9, centre
        // included — it is above the trunk top).
        EXPECT_EQ(plan.size(), static_cast<std::size_t>(T + 20 + 20 + 9))
            << "height " << T;
    }
}

TEST(TreeShapeTest, PlanTreeSpruceAndBirchExactCells) {
    // Spruce (min 6): the bottom-wide cone at T = 6 — the exact set.
    TreeSpec spruce;
    spruce.minHeight = 6;
    spruce.heightRange = 3;
    spruce.leafRadius = 2;
    spruce.shape = TreeShape::Spruce;
    const std::set<std::tuple<int, int, int>> wantSpruce = {
        // trunk 0..5
        {0, 0, 0}, {0, 1, 0}, {0, 2, 0}, {0, 3, 0}, {0, 4, 0}, {0, 5, 0},
        // layer 4 (radius 2, corners rounded: 12 cells)
        {-2, 4, -1}, {-2, 4, 0}, {-2, 4, 1}, {-1, 4, -2}, {-1, 4, -1},
        {-1, 4, 0}, {-1, 4, 1}, {-1, 4, 2}, {0, 4, -2}, {0, 4, -1},
        {0, 4, 1}, {0, 4, 2}, {1, 4, -2}, {1, 4, -1}, {1, 4, 0},
        {1, 4, 1}, {1, 4, 2}, {2, 4, -1}, {2, 4, 0}, {2, 4, 1},
        // layers 5, 6, 7 (radius 1: 5 cells each, centre included at 6/7)
        {-1, 5, -1}, {-1, 5, 0}, {-1, 5, 1}, {0, 5, -1}, {0, 5, 1},
        {1, 5, -1}, {1, 5, 0}, {1, 5, 1},
        {-1, 6, -1}, {-1, 6, 0}, {-1, 6, 1}, {0, 6, -1}, {0, 6, 0},
        {0, 6, 1}, {1, 6, -1}, {1, 6, 0}, {1, 6, 1},
        {-1, 7, -1}, {-1, 7, 0}, {-1, 7, 1}, {0, 7, -1}, {0, 7, 0},
        {0, 7, 1}, {1, 7, -1}, {1, 7, 0}, {1, 7, 1},
    };
    const std::vector<TreeCell> sprucePlan = planTree(6, spruce);
    EXPECT_EQ(plannedCells(sprucePlan), wantSpruce);

    // Birch (min 5): the slim small crown at T = 5 — the exact set.
    TreeSpec birch;
    birch.minHeight = 5;
    birch.heightRange = 2;
    birch.leafRadius = 1;
    birch.shape = TreeShape::Birch;
    const std::set<std::tuple<int, int, int>> wantBirch = {
        {0, 0, 0}, {0, 1, 0}, {0, 2, 0}, {0, 3, 0}, {0, 4, 0},
        {-1, 4, -1}, {-1, 4, 0}, {-1, 4, 1}, {0, 4, -1}, {0, 4, 1},
        {1, 4, -1}, {1, 4, 0}, {1, 4, 1},
        {-1, 5, -1}, {-1, 5, 0}, {-1, 5, 1}, {0, 5, -1}, {0, 5, 0},
        {0, 5, 1}, {1, 5, -1}, {1, 5, 0}, {1, 5, 1},
        {-1, 6, -1}, {-1, 6, 0}, {-1, 6, 1}, {0, 6, -1}, {0, 6, 0},
        {0, 6, 1}, {1, 6, -1}, {1, 6, 0}, {1, 6, 1},
    };
    const std::vector<TreeCell> birchPlan = planTree(5, birch);
    EXPECT_EQ(plannedCells(birchPlan), wantBirch);

    // Trunk-first ordering (the placement contract: trunk before canopy)
    // and the root exactly once (the trunk base — no duplicate cells).
    for (const std::vector<TreeCell>& plan : {sprucePlan, birchPlan}) {
        int lastTrunk = -1;
        for (std::size_t i = 0; i < plan.size(); ++i) {
            if (plan[i].trunkCell) {
                lastTrunk = static_cast<int>(i);
            }
        }
        EXPECT_GT(lastTrunk, 0);
        for (std::size_t i = 0; i <= static_cast<std::size_t>(lastTrunk);
             ++i) {
            EXPECT_TRUE(plan[i].trunkCell);
        }
        std::size_t roots = 0;
        for (const TreeCell& c : plan) {
            roots += static_cast<std::size_t>(
                c.offset.x == 0 && c.offset.y == 0 && c.offset.z == 0);
        }
        EXPECT_EQ(roots, 1u);
    }
}

TEST(TreeShapeTest, TreeHeightRollDeterministicDomainAndDecorrelation) {
    const TreeSpec spec = oakSpec(); // 4..7
    // Determinism: the same (seed, pos) rolls the same height, always.
    const BlockPos p {101, 60, -57};
    const int h0 = treeHeightRoll(42, p, spec);
    for (int i = 0; i < 20; ++i) {
        EXPECT_EQ(treeHeightRoll(42, p, spec), h0);
    }
    // The domain: every height in [minHeight, minHeight + heightRange].
    for (int x = 0; x < 40; ++x) {
        for (int z = 0; z < 40; ++z) {
            const int h = treeHeightRoll(7, BlockPos {x, 54, z}, spec);
            EXPECT_GE(h, 4);
            EXPECT_LE(h, 7);
        }
    }
    // Decorrelation: different roots (and different seeds) yield different
    // heights over a scan (a constant stream would fail, PITS #4).
    bool sawDifferentPos = false, sawDifferentSeed = false;
    for (int x = 0; x < 200 && !sawDifferentPos; ++x) {
        if (treeHeightRoll(7, BlockPos {x, 54, x * 3}, spec) != h0) {
            sawDifferentPos = true;
        }
    }
    for (std::uint64_t s = 0; s < 200 && !sawDifferentSeed; ++s) {
        if (treeHeightRoll(s, p, spec) != h0) {
            sawDifferentSeed = true;
        }
    }
    EXPECT_TRUE(sawDifferentPos);
    EXPECT_TRUE(sawDifferentSeed);
}
