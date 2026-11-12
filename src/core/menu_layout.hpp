// V3-17: the main menu's pure layout + state machine (the 4th loop's
// presentation layer — renderMenu's twin, unit-tested; the campfire
// paradigm: every rect is a pure function of the UI space, so the
// gate clicks the exact centers without a live window).
//
// 17a's face: the title (logo + version + the 5 items 单人/多人/设置/
// 教学/退出) + the 3 stub pages (多人/设置 = "the later face", 教学 =
// the 5 chapter titles' display — the two-level demo list + the real
// settings form + the multiplayer connect are the 17b/17c/17d phases).
// 单人 starts the fresh world directly (the --new's shape — the save
// list is 17c's singleplayer page's face).
#pragma once

#include <cstddef>

namespace mc {

// The menu's pages (the loop's state).
enum class MenuPage {
    Title,
    Multiplayer,
    Settings,
    Tutorial,
};

// The actions a menu step raises (the loop consumes them).
enum class MenuAction {
    None,
    Back,             // a stub page -> the title (ESC / the 返回 click)
    StartSingleplayer, // the title's 单人 -> the fresh world's start
    Quit,              // the title's 退出 / the ESC -> the process exit
};

// The title's 5 items' order (the labels are font-safe — the V3-17
// 21-glyph addition carries 人/设/教/学; a typo here would render
// silently as '?' — the FontSafeLabels pin catches it).
inline constexpr int kMenuTitleItemCount = 5;
inline constexpr const char* kMenuTitleItems[kMenuTitleItemCount] = {
    "单人", "多人", "设置", "教学", "退出",
};
// The stub's back button's label (返回 — the V3-17 addition).
inline constexpr const char* kMenuBackLabel = "返回";
// The stub's placeholder (the pages whose face lands in 17b/17d).
inline constexpr const char* kMenuLaterFace = "the later face";

// One clickable rect (UI pixels, y-down, the uiSize's convention).
struct MenuButton {
    int x = 0, y = 0, w = 0, h = 0;
    bool contains(int px, int py) const {
        return px >= x && px < x + w && py >= y && py < y + h;
    }
};

// The menu's type sizes (the ui.font_scale rides on top at draw time —
// the layout is the fs-1.0 form, renderUi's twin convention).
inline constexpr double kMenuLogoScale = 2.0;
inline constexpr double kMenuTitleScale = 2.0;
inline constexpr double kMenuItemScale = 2.0;
inline constexpr int kMenuItemWidth = 240;
inline constexpr int kMenuItemHeight = 32;
inline constexpr int kMenuItemPitch = 44;
inline constexpr int kMenuBackWidth = 120;

// lround(num/den) in exact integer math (the half-up the C++ lround's
// twin for non-negatives — the test's Python mirror must agree).
inline int menuRoundDiv(int num, int den) {
    return (num + den / 2) / den;
}

// Pure: the title page's geometry for a w x h UI space.
struct MenuTitleLayout {
    int logoX = 0, logoY = 0;     // the logo's top-left (13 glyphs)
    int versionX = 0, versionY = 0;
    MenuButton items[kMenuTitleItemCount];
};

inline MenuTitleLayout menuTitleLayout(int w, int h, int versionWidthPx) {
    MenuTitleLayout l;
    // The logo "TokenMine.cpp" = 13 codepoints: 13 x (16+1) x scale -
    // scale (textWidth's formula at the integer scale 2.0).
    const int logoW =
        13 * static_cast<int>(17.0 * kMenuLogoScale) -
        static_cast<int>(kMenuLogoScale);
    l.logoX = (w - logoW + 1) / 2;
    l.logoY = menuRoundDiv(14 * h, 100); // lround(0.14 h)
    l.versionX = (w - versionWidthPx + 1) / 2;
    l.versionY = l.logoY + 40; // the logo's 32-px height + 8 gap
    const int firstRowY = menuRoundDiv(40 * h, 100); // lround(0.40 h)
    const int itemX = (w - kMenuItemWidth + 1) / 2;
    for (int i = 0; i < kMenuTitleItemCount; ++i) {
        l.items[i] = MenuButton {itemX, firstRowY + i * kMenuItemPitch,
                                 kMenuItemWidth, kMenuItemHeight};
    }
    return l;
}

// Pure: a stub page's geometry (the page title + the back button + the
// tutorial's chapter rows — the display rects, the two-level's 17c
// clicks them later).
struct MenuStubLayout {
    int titleX = 0, titleY = 0;
    MenuButton back;
    MenuButton rows[5];
};

inline MenuStubLayout menuStubLayout(int w, int h) {
    MenuStubLayout l;
    // The page title (2 CJK glyphs at scale 2.0 = 2 x 34 - 2 = 66 px).
    const int titleW = 2 * static_cast<int>(17.0 * kMenuTitleScale) -
                       static_cast<int>(kMenuTitleScale);
    l.titleX = (w - titleW + 1) / 2;
    l.titleY = menuRoundDiv(10 * h, 100); // lround(0.10 h)
    l.back = MenuButton {(w - kMenuBackWidth + 1) / 2,
                         menuRoundDiv(85 * h, 100), // lround(0.85 h)
                         kMenuBackWidth, kMenuItemHeight};
    const int rowY0 = menuRoundDiv(30 * h, 100); // lround(0.30 h)
    for (int i = 0; i < 5; ++i) {
        l.rows[i] = MenuButton {0, rowY0 + i * 30, w, 24};
    }
    return l;
}

// A menu step's outcome (the page's transition + the raised action).
struct MenuStep {
    MenuPage page = MenuPage::Title;
    MenuAction action = MenuAction::None;
};

// Pure: the title page's step. hovered = the item index under the
// pointer (-1 = none); click or Enter activates the hovered item; ESC
// quits. An out-of-range / no hover is a no-op (a click in the air).
inline MenuStep menuStepTitle(bool click, bool enter, bool esc,
                              int hovered) {
    MenuStep s;
    if (esc) {
        s.action = MenuAction::Quit;
        return s;
    }
    if ((click || enter) && hovered >= 0 &&
        hovered < kMenuTitleItemCount) {
        switch (hovered) {
            case 0:
                s.action = MenuAction::StartSingleplayer;
                return s;
            case 1:
                s.page = MenuPage::Multiplayer;
                return s;
            case 2:
                s.page = MenuPage::Settings;
                return s;
            case 3:
                s.page = MenuPage::Tutorial;
                return s;
            case 4:
                s.action = MenuAction::Quit;
                return s;
        }
    }
    return s;
}

// Pure: a stub page's step (ESC or the back button's click/Enter =
// the title). The stub's content is display-only in 17a.
inline MenuStep menuStepStub(bool click, bool enter, bool esc,
                             bool backHovered) {
    MenuStep s;
    if (esc || (backHovered && (click || enter))) {
        s.action = MenuAction::Back;
    }
    return s;
}

} // namespace mc
