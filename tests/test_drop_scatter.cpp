// Drop spawn scatter tests (UX-8d): dropSpawnOffset. The offset is a pure
// function of (world seed, block position, drop index) — the tests pin the
// horizontal bound (the item stays inside the broken cell), determinism
// (same call twice), decorrelation (across drops of one break, across
// cells, across seeds), and the empirical spread (a uniform hash, not a
// biased or constant one).
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "block/drop_scatter.hpp"
#include "entity/item_entity.hpp"

using namespace mc;

TEST(DropScatterTest, ComponentsWithinBound) {
    // Both components land in [-kDropScatterRange, +kDropScatterRange]
    // for a mix of seeds, cells (incl. negative coordinates) and the
    // dropIndex convention values (0/1/2/3 = primary, chance_drop,
    // chance_drops entries 0/1).
    for (std::uint64_t seed = 0; seed < 4; ++seed) {
        for (int x = -4; x < 4; ++x) {
            for (int y = 0; y < 3; ++y) {
                for (int z = -4; z < 4; ++z) {
                    for (int di = 0; di < 4; ++di) {
                        const Vec2 off =
                            dropSpawnOffset(seed, x, y, z, di);
                        EXPECT_LE(std::fabs(off.x), kDropScatterRange);
                        EXPECT_LE(std::fabs(off.y), kDropScatterRange);
                    }
                }
            }
        }
    }
}

TEST(DropScatterTest, DeterministicPerCellPerDrop) {
    // Same cell + seed + dropIndex -> the same offset, every call.
    const Vec2 ref = dropSpawnOffset(42, 3, 5, -2, 1);
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(dropSpawnOffset(42, 3, 5, -2, 1), ref);
    }
    // And the cells are NOT all identical (a constant hash — the PITS #4
    // failure mode — would make every cell scatter to the same spot).
    bool allSame = true;
    for (int x = 0; x < 8; ++x) {
        for (int z = 0; z < 8; ++z) {
            if (dropSpawnOffset(42, x, 5, z, 0) != ref) {
                allSame = false;
            }
        }
    }
    EXPECT_FALSE(allSame);
}

TEST(DropScatterTest, StaysInsideTheBrokenCell) {
    // The bound argument, pinned: item half-size (kSize 0.25 / 2 = 0.125)
    // + max offset (0.2) = 0.325 < 0.5 (half a cell) — a scattered item
    // can never leave the broken cell (no neighbor-solid intrusion, no
    // new collision/float edge cases).
    EXPECT_LT(kDropScatterRange + ItemEntity::kSize * 0.5f, 0.5f);
}

TEST(DropScatterTest, DropsOfOneBreakAreDecorrelated) {
    // The drops of a single break (dropIndex 0 = primary, 1 = chance_drop,
    // 2/3 = chance_drops entries) must not track each other: over a cell
    // region there must exist a cell where each pair disagrees.
    for (int a = 0; a < 3; ++a) {
        for (int b = a + 1; b < 4; ++b) {
            bool differ = false;
            for (int x = 0; x < 32 && !differ; ++x) {
                for (int z = 0; z < 32; ++z) {
                    if (dropSpawnOffset(7, x, 9, z, a) !=
                        dropSpawnOffset(7, x, 9, z, b)) {
                        differ = true;
                        break;
                    }
                }
            }
            EXPECT_TRUE(differ);
        }
    }
}

TEST(DropScatterTest, DependsOnSeed) {
    // Two worlds with different seeds scatter the same cell region
    // differently (there exists a cell that disagrees).
    bool differ = false;
    for (int x = 0; x < 16 && !differ; ++x) {
        for (int z = 0; z < 16; ++z) {
            if (dropSpawnOffset(1, x, 10, z, 0) !=
                dropSpawnOffset(2, x, 10, z, 0)) {
                differ = true;
                break;
            }
        }
    }
    EXPECT_TRUE(differ);
}

TEST(DropScatterTest, NegativeCoordinatesAreWellDefined) {
    // The world is unbounded: negative coordinates are legal and the offset
    // is deterministic for them (the two's-complement hash bijection).
    const Vec2 a = dropSpawnOffset(5, -17, 3, -17, 2);
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(dropSpawnOffset(5, -17, 3, -17, 2), a);
    }
}

TEST(DropScatterTest, SpreadsAcrossTheBand) {
    // Over a 32x32 cell grid each component's range (max - min) must cover
    // at least half the full band (0.2 of the 0.4 full width): a uniform
    // hash fills the band (range ~0.4), a biased or degenerate one would
    // not.
    float minX = 0.0f;
    float maxX = 0.0f;
    float minZ = 0.0f;
    float maxZ = 0.0f;
    bool first = true;
    for (int x = 0; x < 32; ++x) {
        for (int z = 0; z < 32; ++z) {
            const Vec2 off = dropSpawnOffset(987654, x, 10, z, 0);
            if (first) {
                minX = maxX = off.x;
                minZ = maxZ = off.y;
                first = false;
            }
            minX = std::min(minX, off.x);
            maxX = std::max(maxX, off.x);
            minZ = std::min(minZ, off.y);
            maxZ = std::max(maxZ, off.y);
        }
    }
    EXPECT_GT(maxX - minX, kDropScatterRange); // half the full 0.4 band
    EXPECT_GT(maxZ - minZ, kDropScatterRange);
}
