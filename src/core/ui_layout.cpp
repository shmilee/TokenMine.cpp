#include "core/ui_layout.hpp"

namespace mc {

namespace {

// A strip of `icons` icons: icon px + gaps between them.
int stripWidth(int icons) {
    return icons > 0 ? icons * kUiIconPx + (icons - 1) * kUiIconGap : 0;
}

} // namespace

HeartRowLayout heartRowLayout(int width, int heartIcons, int absIcons,
                              int rightIcons) {
    const int heartW = stripWidth(heartIcons);
    const int absW = stripWidth(absIcons);
    const int rightW = stripWidth(rightIcons);
    // The legacy centered layout (bit-identical when the row fits).
    int hx0 = (width - heartW) / 2;
    int gx0 = hx0 + heartW + kUiStripGap;
    // The narrow-window fallback: the right strip (hunger/oxygen — the
    // starvation/air readout, the more urgent one) must stay on screen;
    // shift the row left until it fits, letting the absorption strip
    // (the row's leftmost part) clip first.
    const int rowRight = gx0 + rightW;
    if (rowRight > width) {
        const int shift = rowRight - width;
        hx0 -= shift;
        gx0 -= shift;
    }
    // The row's leftmost edge: the absorption strip's left edge when the
    // pool is up, the hearts' left edge otherwise (the PRE-10
    // effect-row / armor-row anchor — the no-pool case must NOT eat the
    // strip gap).
    const int rowLeft =
        absIcons > 0 ? hx0 - kUiStripGap - absW : hx0;
    return {hx0, gx0, rowLeft};
}

} // namespace mc
