// V2-19: the double-tap Space fly toggle (core/fly) — the pure input-
// state decision (the CRAFT-7 kDoubleClickSeconds real-dt pattern the
// Game adapts; the unit tests own the window semantics).
#include <gtest/gtest.h>

#include "core/fly.hpp"

using namespace mc;

TEST(FlyToggleTest, WindowIsTheCraft7Value) {
    // 0.3 s = the CRAFT-7 kDoubleClickSeconds value (the "fast double
    // tap" band: wide enough for xdotool's ~50 ms click jitter, narrow
    // enough that a deliberate single tap never toggles).
    EXPECT_DOUBLE_EQ(kFlyDoubleTapSeconds, 0.3);
}

TEST(FlyToggleTest, FirstPressIsNeverADoubleTap) {
    // No press yet (lastPressTime < 0): the first tap only arms the
    // window — it never toggles (the jump stays the pre-V2-19 one).
    const FlyToggleDecision d = flyToggleDecision(100.0, -1.0, false);
    EXPECT_FALSE(d.doubleTap);
    EXPECT_FALSE(d.nowFlying);
    // Same when already flying (a single tap does not exit either).
    const FlyToggleDecision d2 = flyToggleDecision(100.0, -1.0, true);
    EXPECT_FALSE(d2.doubleTap);
    EXPECT_TRUE(d2.nowFlying);
}

TEST(FlyToggleTest, SingleTapOutsideWindowKeepsState) {
    // More than kFlyDoubleTapSeconds since the last press: outside the
    // window (the boundary is inclusive — the next test pins the edge,
    // this one the clear miss).
    const FlyToggleDecision d =
        flyToggleDecision(10.0 + kFlyDoubleTapSeconds + 1e-6, 10.0, false);
    EXPECT_FALSE(d.doubleTap);
    EXPECT_FALSE(d.nowFlying);
}

TEST(FlyToggleTest, DoubleTapOnTheInclusiveBoundaryToggles) {
    // EXACTLY kFlyDoubleTapSeconds apart: a double-tap (the CRAFT-7
    // predicate's `<=`) — and it exits (the creative's bidirectional
    // toggle: the same gesture that entered leaves). The 0.0 base keeps
    // the difference EXACTLY kFlyDoubleTapSeconds in binary (10.0 + 0.3
    // rounded up a ulp above the window).
    const FlyToggleDecision d =
        flyToggleDecision(kFlyDoubleTapSeconds, 0.0, true);
    EXPECT_TRUE(d.doubleTap);
    EXPECT_FALSE(d.nowFlying);
}

TEST(FlyToggleTest, DoubleTapWithinWindowTogglesOn) {
    // The second press inside the window enters the fly (the second
    // tap's jump is swallowed by the caller — Player's suppressJumpGrant
    // — not by this decision).
    const FlyToggleDecision d = flyToggleDecision(10.0 + 0.15, 10.0, false);
    EXPECT_TRUE(d.doubleTap);
    EXPECT_TRUE(d.nowFlying);
}

TEST(FlyToggleTest, ToggleIsBidirectional) {
    // Enter, then exit, with the same gesture (the design's "开关"):
    // each double-tap flips exactly once, a single tap in between keeps
    // the state (it only re-arms the window).
    const FlyToggleDecision enter = flyToggleDecision(1.1, 1.0, false);
    ASSERT_TRUE(enter.nowFlying);
    const FlyToggleDecision single = flyToggleDecision(1.1 + 0.5, 1.1, true);
    ASSERT_FALSE(single.doubleTap);
    ASSERT_TRUE(single.nowFlying); // still flying after a lone tap
    const FlyToggleDecision exit = flyToggleDecision(1.1 + 0.5 + 0.2,
                                                     1.1 + 0.5, true);
    ASSERT_TRUE(exit.doubleTap);
    EXPECT_FALSE(exit.nowFlying);
}
