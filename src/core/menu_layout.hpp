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

// ---------------------------------------------------------------------------
// V3-17b: the settings page's form (the 8 rows + the save button).
// ---------------------------------------------------------------------------

// The form's rows (the pure vocabulary — the config's keys + the
// value's codec live on the Game's side, the row index selects them).
// The integer domains: view distance 4..64 step 4 (the kStreamCapRadius
// 64's belt), the day length 60..86400 step 60 (seconds), the volume
// 0..10 (x10), the ui scale 2..8 (x2 = 1.0..4.0, the runtime's live
// apply), the window's width 640..2560 / height 480..1440 step 64
// (the restart's face — Window::init only).
enum class MenuSettingsRow {
    ViewDistance = 0,
    DayLength = 1,
    Weather = 2, // toggle (the minus's button = the 开/关)
    Volume = 3,
    UiScale = 4,
    Fullscreen = 5, // toggle
    WinWidth = 6,
    WinHeight = 7,
};
inline constexpr int kMenuSettingsRowCount = 8;
inline constexpr const char* kMenuSettingsLabels[kMenuSettingsRowCount] = {
    "视图距离", "白天长度", "天气", "声音",
    "界面缩放", "全屏", "窗口宽", "窗口高",
};
inline constexpr int kMenuSettingsSave = 32; // the 保存 button's control id
// The control id's encoding: row*2 = the minus / toggle button,
// row*2+1 = the plus (the toggle row's plus = dead).
inline constexpr bool isToggleRow(int row) {
    return row == 2 || row == 5;
}

struct MenuSettingsLayout {
    int titleX = 0, titleY = 0;
    struct Row {
        MenuButton label; // the 4-CJK label's rect (x 40, the left's face)
        MenuButton minus;
        MenuButton plus;
        int valueX = 0;   // the value text's CENTER x
    };
    Row rows[kMenuSettingsRowCount];
    MenuButton save;
    int noteX = 0, noteY = 0; // the 「部分项重启后生效」 hint
};

inline MenuSettingsLayout menuSettingsLayout(int w, int h) {
    MenuSettingsLayout l;
    // The title 设置 (2 CJK glyphs @ 2.0 = 66 px, the stub's twin).
    const int titleW = 2 * static_cast<int>(17.0 * kMenuTitleScale) -
                       static_cast<int>(kMenuTitleScale);
    l.titleX = (w - titleW + 1) / 2;
    l.titleY = menuRoundDiv(10 * h, 100);
    const int rowY0 = menuRoundDiv(18 * h, 100);
    constexpr int kRowPitch = 36;
    constexpr int kRowH = 24;
    for (int i = 0; i < kMenuSettingsRowCount; ++i) {
        const int ry = rowY0 + i * kRowPitch;
        l.rows[i].label = MenuButton {40, ry, 100, kRowH};
        l.rows[i].valueX = w / 2; // the label's right..button's left's center
        l.rows[i].minus = MenuButton {w - 140, ry, 24, kRowH};
        l.rows[i].plus = MenuButton {w - 104, ry, 24, kRowH};
    }
    // The 保存 button (70 px = the 49-px text + the padding, at 0.85h).
    constexpr int kMenuSaveWidth = 70;
    l.save = MenuButton {(w - kMenuSaveWidth + 1) / 2,
                         menuRoundDiv(85 * h, 100), kMenuSaveWidth, 28};
    // The hint (8 glyphs @ 1.0 = 135 px).
    const int noteW = 8 * 17 - 1;
    l.noteX = (w - noteW + 1) / 2;
    l.noteY = menuRoundDiv(75 * h, 100);
    return l;
}

// Pure: the control under (mx, my) — the id (the row*2 encoding) or -1.
inline int menuSettingsControl(const MenuSettingsLayout& l, int mx, int my) {
    for (int i = 0; i < kMenuSettingsRowCount; ++i) {
        if (l.rows[i].minus.contains(mx, my)) {
            return i * 2;
        }
        if (l.rows[i].plus.contains(mx, my) && !isToggleRow(i)) {
            return i * 2 + 1;
        }
    }
    if (l.save.contains(mx, my)) {
        return kMenuSettingsSave;
    }
    return -1;
}

struct MenuSettingsStep {
    enum class Kind { None, Back, Dec, Inc, Toggle, Save };
    Kind kind = Kind::None;
    int row = -1;
};

// Pure: the settings page's step. ESC = back, the Enter = the save's
// convenience, the click = the hovered control's action.
inline MenuSettingsStep menuSettingsStep(bool click, bool enter, bool esc,
                                         int controlId) {
    MenuSettingsStep s;
    if (esc) {
        s.kind = MenuSettingsStep::Kind::Back;
        return s;
    }
    if (enter) {
        s.kind = MenuSettingsStep::Kind::Save;
        return s;
    }
    if (!click) {
        return s;
    }
    if (controlId == kMenuSettingsSave) {
        s.kind = MenuSettingsStep::Kind::Save;
        return s;
    }
    if (controlId >= 0 && controlId < kMenuSettingsRowCount * 2) {
        s.row = controlId / 2;
        s.kind = (controlId % 2 == 0 && isToggleRow(s.row))
                     ? MenuSettingsStep::Kind::Toggle
                     : (controlId % 2 == 0 ? MenuSettingsStep::Kind::Dec
                                           : MenuSettingsStep::Kind::Inc);
        return s;
    }
    return s;
}

} // namespace mc
