// Heart-row HUD layout (Phase 16/23; PRE-11 narrow-window fallback):
// the pure layout owned by the unit tests (the campfire paradigm).
#include <gtest/gtest.h>

#include "core/ui_layout.hpp"

using namespace mc;

namespace {

// The default player row: 10 hearts (20 HP), 10 hunger icons (20
// hunger), the oxygen bar has 10 icons too.
constexpr int kHeartIcons = 10;
constexpr int kRightIcons = 10;

// The strip widths the row is built from (16 px icons, 2 px gaps).
constexpr int kHeartW = kHeartIcons * kUiIconPx + (kHeartIcons - 1) * kUiIconGap; // 178
constexpr int kRightW = kRightIcons * kUiIconPx + (kRightIcons - 1) * kUiIconGap; // 178
constexpr int kAbsW2 = 2 * kUiIconPx + 1 * kUiIconGap; // 34 (pool 4 = 2 icons)

} // namespace

// A wide window (the E2E virtual 908 px) returns the legacy centered
// layout bit-exactly: hearts centered, hunger 6 px past, no shift.
TEST(HeartRowLayoutTest, WideWindowIsLegacyCentered) {
    const HeartRowLayout l = heartRowLayout(908, kHeartIcons, 0, kRightIcons);
    EXPECT_EQ(l.heartsX, (908 - kHeartW) / 2);  // 365
    EXPECT_EQ(l.rightX, l.heartsX + kHeartW + kUiStripGap);  // 549
    EXPECT_EQ(l.rowLeft, l.heartsX);  // no pool: the anchor is the hearts
    // The row fits with room to spare.
    EXPECT_LT(l.rightX + kRightW, 908);
}

// A wide window WITH an absorption pool (4 HP = 2 icons): the row's left
// anchor moves to the absorption strip's left edge (the PRE-10 effect-
// row anchor).
TEST(HeartRowLayoutTest, WideWindowWithPoolAnchorsOnAbsorption) {
    const HeartRowLayout l = heartRowLayout(908, kHeartIcons, 2, kRightIcons);
    EXPECT_EQ(l.heartsX, 365);
    EXPECT_EQ(l.rowLeft, 365 - kUiStripGap - kAbsW2);  // 325
}

// The exact-fit boundary: a window whose width equals the legacy row's
// right edge does NOT shift (the overflow is strict). At w = 545 the
// legacy row ends exactly at the edge.
TEST(HeartRowLayoutTest, ExactFitDoesNotShift) {
    const HeartRowLayout l = heartRowLayout(545, kHeartIcons, 0, kRightIcons);
    EXPECT_EQ(l.heartsX, (545 - kHeartW) / 2);  // 183
    EXPECT_EQ(l.rightX + kRightW, 545);         // flush, no shift
}

// One px narrower: the row overflows by 1 and shifts left by exactly 1
// (the right strip stays flush with the right edge).
TEST(HeartRowLayoutTest, OnePixelOverShiftsByOne) {
    const HeartRowLayout l = heartRowLayout(544, kHeartIcons, 0, kRightIcons);
    EXPECT_EQ(l.rightX + kRightW, 544);
    EXPECT_EQ(l.heartsX, 544 - kRightW - kUiStripGap - kHeartW);  // 182
}

// The narrow-window fallback (the KNOWN_ISSUES Phase-23 case): at 380
// px the legacy row (ending at 463) overflows — the row shifts left
// until the right strip's right edge fits; the hearts stay fully on
// screen (18 >= 0).
TEST(HeartRowLayoutTest, NarrowWindowKeepsRightStripOnScreen) {
    const HeartRowLayout l = heartRowLayout(380, kHeartIcons, 0, kRightIcons);
    EXPECT_EQ(l.rightX + kRightW, 380);  // flush with the right edge
    EXPECT_GE(l.heartsX, 0);             // the hearts are intact
}

// The same narrow window WITH a pool: the absorption strip is the row's
// leftmost part, so it clips first (the documented last resort — the
// right strip still wins the space).
TEST(HeartRowLayoutTest, NarrowWindowWithPoolClipsAbsorptionFirst) {
    const HeartRowLayout l = heartRowLayout(380, kHeartIcons, 2, kRightIcons);
    EXPECT_EQ(l.rightX + kRightW, 380);  // the right strip still fits
    EXPECT_EQ(l.heartsX, 18);            // same shift as the no-pool case
    EXPECT_LT(l.rowLeft, 0);             // the absorption strip clips
}

// Below the row's minimum width (hearts + gap + right strip = 362 px),
// no layout can keep the hearts on screen — they clip too (the
// documented limit; the UI is pixel-ortho).
TEST(HeartRowLayoutTest, SubRowWindowClipsTheHeartsToo) {
    const HeartRowLayout l = heartRowLayout(330, kHeartIcons, 0, kRightIcons);
    EXPECT_EQ(l.rightX + kRightW, 330);
    EXPECT_LT(l.heartsX, 0);
}

// Degenerate zero-width strips (the 0-guard in stripWidth): the layout
// stays well-formed.
TEST(HeartRowLayoutTest, ZeroRightStripIsWellFormed) {
    const HeartRowLayout l = heartRowLayout(908, kHeartIcons, 0, 0);
    EXPECT_EQ(l.heartsX, 365);
    EXPECT_EQ(l.rightX, 365 + kHeartW + kUiStripGap);
    EXPECT_EQ(l.rowLeft, l.heartsX);
}
