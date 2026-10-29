// Heart-row HUD layout (Phase 16/23; PRE-11: the narrow-window fallback).
//
// The heart row is [absorption strip] [hearts] [gap] [hunger|oxygen
// strip] on one line above the hotbar. The pure layout is owned by the
// unit tests (the campfire paradigm); Game::drawUI consumes it.
#pragma once

namespace mc {

// The UI icon metrics (virtual px) — the single source of truth shared
// by the layout math and the drawUI quads.
inline constexpr int kUiIconPx = 16;
inline constexpr int kUiIconGap = 2;
inline constexpr int kUiStripGap = 6;

// The heart-row layout result (virtual px, the row's top edge is
// separate: drawUI keeps hy).
struct HeartRowLayout {
    int heartsX; // the heart strip's left edge
    int rightX;  // the hunger/oxygen strip's left edge
    int rowLeft; // the row's leftmost edge — the absorption strip's left
                 // edge when the pool is up, the hearts' left edge
                 // otherwise (the PRE-10 effect-row / armor-row anchor)
};

// Legacy (Phase 16/23): the hearts are centered, the hunger strip 6 px
// past their right edge — in a narrow window the whole row overflows the
// right edge and the UI's pixel-ortho clip eats the hunger strip (the
// KNOWN_ISSUES Phase-23 entry: no fallback layout). PRE-11: when the row
// overflows the right edge, shift the row LEFT until the right strip's
// right edge fits; the absorption strip (the row's leftmost part) clips
// first (the documented last resort), and below hearts+gap+right width
// the hearts themselves clip (no layout can save a sub-row window).
// A wide window returns the legacy coordinates bit-exactly (the
// no-input baseline + every E2E window are untouched).
HeartRowLayout heartRowLayout(int width, int heartIcons, int absIcons,
                              int rightIcons);

} // namespace mc
