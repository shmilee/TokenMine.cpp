// XP curve (V2-11): the Minecraft experience math — the level <-> total-
// points mapping of the player's experience pool.
//
// The player carries ONE integer (Player::m_xpTotal, saved as
// player.xp, format 26): the TOTAL experience points ever gained. The
// LEVEL is never stored — it is derived from the pool through
// levelFromTotalXp (the single source of truth; a stored level would be a
// second, driftable copy).
//
// The formulas are the modern Minecraft ones (Minecraft wiki "Experience:
// Useful numbers", verified by the orchestrator 11-03 — 0->1 = 7, 0->30 =
// 1395, 27->30 = 306, the segment boundaries are continuous; cross-checked
// three ways). They are PINNED by the unit tests (tests/test_xp_curve.cpp)
// at every segment boundary, so a formula edit turns the suite red.
//
// EVERY operation is INTEGER (no floating-point sqrt / division tricks —
// the inverse is an integer binary search over the monotonically growing
// total function, so the round trip levelFromTotalXp(totalXpForLevel(L))
// == L holds BY CONSTRUCTION for every L, and the HUD fill ratio
// xpIntoLevel / xpNeededForLevel is exact integer math).
#pragma once

namespace mc {

// The experience points needed to advance FROM level `level` to level + 1
// (the per-level bar cost):
//   level <= 15:      2L + 7      (0->1 = 7, 15->16 = 37)
//   16 <= level <= 30: 5L - 38    (16->17 = 42, 30->31 = 112)
//   level >= 31:      9L - 158    (31->32 = 121)
// Always >= 7, so the HUD fill division never divides by zero.
inline int xpNeededForLevel(int level) {
    if (level <= 15) {
        return 2 * level + 7;
    }
    if (level <= 30) {
        return 5 * level - 38;
    }
    return 9 * level - 158;
}

// The TOTAL experience points needed to reach level `level` (0 at level
// 0; the per-level costs summed — the segment values are the INTEGER form
// of the wiki's 2.5L^2 - 4.5L - ... series, exact for every integer L):
//   level <= 16:      L^2 + 6L
//   17 <= level <= 31: (5L^2 - 81L + 720) / 2
//   level >= 32:      (9L^2 - 325L + 4440) / 2
// Pinned: T(0) = 0, T(1) = 7, T(16) = 352, T(17) = 394, T(30) = 1395,
// T(31) = 1507, T(32) = 1628 (the segment boundaries are continuous).
// long long internally: the int API stays (the player's pool is an int),
// but the quadratic never overflows while the binary search below
// doubles its high bound past the int-total range.
inline int totalXpForLevel(int level) {
    const long long L = level;
    long long total;
    if (level <= 16) {
        total = L * L + 6 * L;
    } else if (level <= 31) {
        total = (5 * L * L - 81 * L + 720) / 2;
    } else {
        total = (9 * L * L - 325 * L + 4440) / 2;
    }
    return static_cast<int>(total);
}

// The level a total of `total` experience points buys: the LARGEST level
// L with totalXpForLevel(L) <= total. totalXpForLevel is strictly
// increasing (every per-level cost is >= 7), so the answer is unique and
// the round trip through totalXpForLevel is the identity (by
// construction — no floating-point inverse). total < 0 clamps to 0 (a
// corrupt pool reads as fresh, it does not wrap).
inline int levelFromTotalXp(int total) {
    if (total <= 0) {
        return 0;
    }
    int hi = 1;
    while (totalXpForLevel(hi) <= total) {
        hi *= 2; // T(hi) ~ 4.5 hi^2: ~14 doublings cover the int range
    }
    int lo = 0; // T(0) = 0 <= total
    while (hi - lo > 1) {
        const int mid = lo + (hi - lo) / 2;
        if (totalXpForLevel(mid) <= total) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    return lo;
}

// The progress WITHIN the player's current level (the HUD bar's fill
// numerator): total - T(level), in [0, xpNeededForLevel(level)).
inline int xpIntoLevel(int total, int level) {
    return total - totalXpForLevel(level);
}

} // namespace mc
