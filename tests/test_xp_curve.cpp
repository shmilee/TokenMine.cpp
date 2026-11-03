// XP curve tests (V2-11): the pure integer level <-> total-points math
// (core/xp_curve.hpp). Every formula is pinned at the segment boundaries
// and the round trip (levelFromTotalXp ∘ totalXpForLevel = identity) is
// verified over the pinned range, so a formula edit turns the suite red.
#include <gtest/gtest.h>

#include "core/xp_curve.hpp"

using namespace mc;

// -- the per-level cost (xpNeededForLevel) ---------------------------------

TEST(XpCurve, NeededIsTheMinecraftSegmentedFormula) {
    // Segment 1 (L <= 15): 2L + 7 — the flat low-level cost (0->1 = 7).
    EXPECT_EQ(xpNeededForLevel(0), 7);
    EXPECT_EQ(xpNeededForLevel(1), 9);
    EXPECT_EQ(xpNeededForLevel(7), 21);
    EXPECT_EQ(xpNeededForLevel(15), 37);
    // Segment 2 (16 <= L <= 30): 5L - 38 — the mid-level cost.
    EXPECT_EQ(xpNeededForLevel(16), 42);
    EXPECT_EQ(xpNeededForLevel(20), 62);
    EXPECT_EQ(xpNeededForLevel(27), 97);
    EXPECT_EQ(xpNeededForLevel(30), 112);
    // Segment 3 (L >= 31): 9L - 158 — the high-level cost.
    EXPECT_EQ(xpNeededForLevel(31), 121);
    EXPECT_EQ(xpNeededForLevel(32), 130);
    EXPECT_EQ(xpNeededForLevel(40), 202);
}

TEST(XpCurve, NeededIsAlwaysPositive) {
    // The HUD fill divides by needed(level); a zero would be a
    // division-by-zero. The lowest per-level cost in the whole curve is
    // 7 (level 0), so the guard the HUD relies on is structural.
    for (int level = 0; level <= 1000; ++level) {
        EXPECT_GT(xpNeededForLevel(level), 0) << level;
    }
}

// -- the total cost (totalXpForLevel) --------------------------------------

TEST(XpCurve, TotalIsTheMinecraftSegmentedFormula) {
    // The pinned boundary values (the design's 2.5L^2-class integer
    // forms, exact at every segment edge).
    EXPECT_EQ(totalXpForLevel(0), 0);
    EXPECT_EQ(totalXpForLevel(1), 7);
    EXPECT_EQ(totalXpForLevel(7), 91);  // 49 + 42
    EXPECT_EQ(totalXpForLevel(15), 315); // 225 + 90
    EXPECT_EQ(totalXpForLevel(16), 352); // segment 1 top
    EXPECT_EQ(totalXpForLevel(17), 394); // segment 2 start
    EXPECT_EQ(totalXpForLevel(30), 1395);
    EXPECT_EQ(totalXpForLevel(31), 1507);
    EXPECT_EQ(totalXpForLevel(32), 1628); // segment 3 start
    EXPECT_EQ(totalXpForLevel(39), 2727); // (see the sum check below)
}

TEST(XpCurve, TotalIsTheSumOfPerLevelCosts) {
    // totalXpForLevel(L) must equal the sum of needed(0..L-1) — the two
    // functions are the same curve, point form and cumulative form. A
    // mismatch means a segment constant drifted from its neighbor.
    for (int L = 0; L <= 200; ++L) {
        int sum = 0;
        for (int i = 0; i < L; ++i) {
            sum += xpNeededForLevel(i);
        }
        EXPECT_EQ(totalXpForLevel(L), sum) << L;
    }
}

// -- the inverse (levelFromTotalXp) ----------------------------------------

TEST(XpCurve, LevelFromTotalIsTheFloorOfTheCurve) {
    // At an exact total the level is that total's level.
    EXPECT_EQ(levelFromTotalXp(0), 0);
    EXPECT_EQ(levelFromTotalXp(7), 1);
    EXPECT_EQ(levelFromTotalXp(352), 16);
    EXPECT_EQ(levelFromTotalXp(394), 17);
    EXPECT_EQ(levelFromTotalXp(1395), 30);
    EXPECT_EQ(levelFromTotalXp(1507), 31);
    EXPECT_EQ(levelFromTotalXp(1628), 32);
    // One point SHORT of the next level's total is still the lower level.
    EXPECT_EQ(levelFromTotalXp(6), 0);
    EXPECT_EQ(levelFromTotalXp(351), 15);
    EXPECT_EQ(levelFromTotalXp(393), 16);
    EXPECT_EQ(levelFromTotalXp(1394), 29);
    EXPECT_EQ(levelFromTotalXp(1506), 30);
    EXPECT_EQ(levelFromTotalXp(1627), 31);
    // A total BETWEEN levels sits in the lower level (partial progress).
    EXPECT_EQ(levelFromTotalXp(8), 1);  // 1 into level 1 (needs 9 for 2)
    EXPECT_EQ(levelFromTotalXp(100), 7); // T(7)=91, T(8)=112 -> 7
    EXPECT_EQ(levelFromTotalXp(1394 + 50), 30);
}

TEST(XpCurve, NegativeTotalClampsToLevelZero) {
    // A corrupt (negative) pool reads as fresh, it does not wrap or
    // underflow (the documented clamp).
    EXPECT_EQ(levelFromTotalXp(-1), 0);
    EXPECT_EQ(levelFromTotalXp(-1000000), 0);
}

TEST(XpCurve, LevelFromTotalRoundTripIsIdentity) {
    // levelFromTotalXp(totalXpForLevel(L)) == L for every pinned L. The
    // binary search is over a strictly increasing T, so the answer is
    // unique — the round trip holds BY CONSTRUCTION, no float.
    for (int L = 0; L <= 39; ++L) {
        EXPECT_EQ(levelFromTotalXp(totalXpForLevel(L)), L) << L;
    }
}

TEST(XpCurve, RoundTripHoldsPastThePinnedRange) {
    // The design pins L 0..39, but the curve is total — the round trip
    // must hold well past the pinned set (the high segment's constant
    // would otherwise drift unnoticed until a level-40+ player hits it).
    for (int L : {40, 50, 64, 100, 150, 200}) {
        EXPECT_EQ(levelFromTotalXp(totalXpForLevel(L)), L) << L;
    }
}

// -- the within-level fill (xpIntoLevel) -----------------------------------

TEST(XpCurve, IntoLevelIsProgressWithinTheLevel) {
    // At an exact level total the fill is 0 (just reached, bar empty).
    EXPECT_EQ(xpIntoLevel(0, 0), 0);
    EXPECT_EQ(xpIntoLevel(352, 16), 0);
    EXPECT_EQ(xpIntoLevel(1395, 30), 0);
    // One point past is 1 into the level.
    EXPECT_EQ(xpIntoLevel(1, 0), 1);
    EXPECT_EQ(xpIntoLevel(353, 16), 1);
    // At the point just BEFORE the next level the fill is needed-1
    // (the bar is full but the level has not advanced).
    EXPECT_EQ(xpIntoLevel(6, 0), 6);  // needed(0)=7 -> max fill 6
    EXPECT_EQ(xpIntoLevel(393, 16), 41); // needed(16)=42 -> max fill 41
    EXPECT_EQ(xpIntoLevel(1506, 30), 111); // needed(30)=112 -> max 111
}

TEST(XpCurve, IntoLevelStaysWithinTheNeededRange) {
    // For every total, with L = levelFromTotalXp(total), the fill
    // xpIntoLevel(total, L) sits in [0, needed(L)) — the invariant the
    // HUD's fill ratio (into/needed) relies on to never overflow a bar.
    for (int total = 0; total <= 20000; ++total) {
        const int L = levelFromTotalXp(total);
        const int into = xpIntoLevel(total, L);
        EXPECT_GE(into, 0) << total;
        EXPECT_LT(into, xpNeededForLevel(L)) << total;
    }
}
