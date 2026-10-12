// Chance secondary drop tests (Phase 49): the deterministic per-break roll
// (rollChanceDrop). The roll is a pure function of (world seed, position,
// chance) — the tests pin the boundaries (0/1), determinism (same cell
// twice), decorrelation (across cells / across seeds), and the empirical
// rate (a uniform roll, not a biased one).
#include <gtest/gtest.h>

#include <cstdint>

#include "block/chance_drop.hpp"

using namespace mc;

TEST(ChanceDropTest, ChanceZeroNeverDrops) {
    for (int x = -4; x < 4; ++x) {
        for (int y = 0; y < 4; ++y) {
            for (int z = -4; z < 4; ++z) {
                EXPECT_FALSE(rollChanceDrop(1234, x, y, z, 0.0));
            }
        }
    }
}

TEST(ChanceDropTest, ChanceOneAlwaysDrops) {
    for (int x = -4; x < 4; ++x) {
        for (int y = 0; y < 4; ++y) {
            for (int z = -4; z < 4; ++z) {
                EXPECT_TRUE(rollChanceDrop(1234, x, y, z, 1.0));
            }
        }
    }
}

TEST(ChanceDropTest, DeterministicPerCell) {
    // Same cell + seed + chance -> the same answer, every call.
    const bool ref = rollChanceDrop(42, 3, 1, -2, 0.5);
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(rollChanceDrop(42, 3, 1, -2, 0.5), ref);
    }
    // And the cells are NOT all identical at 1/2 (that would be a constant
    // hash — the PITS #4 failure mode).
    bool allSame = true;
    for (int x = 0; x < 8; ++x) {
        for (int z = 0; z < 8; ++z) {
            if (rollChanceDrop(42, x, 5, z, 0.5) != ref) {
                allSame = false;
            }
        }
    }
    EXPECT_FALSE(allSame);
}

TEST(ChanceDropTest, DependsOnSeed) {
    // Two worlds with different seeds produce different drop patterns over
    // the same cell region (there exists a cell that disagrees).
    bool differ = false;
    for (int x = 0; x < 16 && !differ; ++x) {
        for (int z = 0; z < 16; ++z) {
            if (rollChanceDrop(1, x, 10, z, 0.5) !=
                rollChanceDrop(2, x, 10, z, 0.5)) {
                differ = true;
                break;
            }
        }
    }
    EXPECT_TRUE(differ);
}

TEST(ChanceDropTest, EmpiricalRateNearChance) {
    // Over a 100x100 cell grid at chance = 0.25, the pass fraction must
    // land in a wide band around 0.25 (a uniform roll; the binomial
    // standard error here is ~0.004, so [0.18, 0.32] is ~16 sigma).
    int n = 0;
    int hits = 0;
    for (int x = 0; x < 100; ++x) {
        for (int z = 0; z < 100; ++z) {
            ++n;
            if (rollChanceDrop(987654, x, 10, z, 0.25)) {
                ++hits;
            }
        }
    }
    const double rate = static_cast<double>(hits) / static_cast<double>(n);
    EXPECT_GT(rate, 0.18);
    EXPECT_LT(rate, 0.32);
}

TEST(ChanceDropTest, NegativeCoordinatesAreWellDefined) {
    // The world is unbounded: negative coordinates are legal and the roll
    // is deterministic for them (the two's-complement hash bijection).
    const bool a = rollChanceDrop(5, -17, 3, -17, 0.5);
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(rollChanceDrop(5, -17, 3, -17, 0.5), a);
    }
}
