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
#include <cstdio>
#include <optional>
#include <string>

namespace mc {

// The menu's pages (the loop's state).
enum class MenuPage {
    Title,
    Login,       // 17c: the name's entry (before the singleplayer's start)
    Multiplayer, // 17d: the host/join's page (the name's field + the two actions)
    Settings,
    Tutorial,    // 17c: the two-level's host (the chapter's list / the demos')
    Join,        // 17d: the join's sub-page (the host:port's field)
};

// The actions a menu step raises (the loop consumes them).
enum class MenuAction {
    None,
    Back,             // a stub page -> the title (ESC / the 返回 click)
    StartSingleplayer, // the login page's 开始 -> the fresh world's start
    LaunchDemo,        // the tutorial's demo row (solo) -> the replay's start
    HostGame,          // 17d: the multiplayer's 创建游戏 -> the host's start
    JoinGame,          // 17d: the join's 开始 -> the client's connect
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

// A menu step's outcome (the page's transition + the raised action).
struct MenuStep {
    MenuPage page = MenuPage::Title;
    MenuAction action = MenuAction::None;
};

// Pure: the title page's step. hovered = the item index under the
// pointer (-1 = none); click or Enter activates the hovered item; ESC
// quits. Item 0 (单人) = the LOGIN page (17c: the name's entry before
// the start); an out-of-range / no hover is a no-op.
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
                s.page = MenuPage::Login;
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

// ---------------------------------------------------------------------------
// V3-17c: the login page (the name's entry) + the tutorial's two-level.
// ---------------------------------------------------------------------------

// The login page's geometry (the title 名字 + the field + the hint +
// the 开始/返回 buttons). The field = the SDL text input's face (the
// creative search / the chat's precedent — opened only while this page
// is active).
struct MenuLoginLayout {
    int titleX = 0, titleY = 0;
    MenuButton field;
    int hintX = 0, hintY = 0; // the （可选）
    MenuButton start;
    MenuButton back;
};

inline MenuLoginLayout menuLoginLayout(int w, int h) {
    MenuLoginLayout l;
    const int titleW = 2 * static_cast<int>(17.0 * kMenuTitleScale) -
                       static_cast<int>(kMenuTitleScale);
    l.titleX = (w - titleW + 1) / 2;
    l.titleY = menuRoundDiv(10 * h, 100);
    constexpr int kFieldW = 300;
    constexpr int kFieldH = 40;
    l.field = MenuButton {(w - kFieldW + 1) / 2, menuRoundDiv(32 * h, 100),
                          kFieldW, kFieldH};
    const int hintW = 5 * 17 - 1; // （可选） 5 glyphs @ 1.0
    l.hintX = (w - hintW + 1) / 2;
    l.hintY = l.field.y + l.field.h + 8;
    l.start = MenuButton {(w - 70 + 1) / 2, menuRoundDiv(72 * h, 100), 70, 28};
    l.back = MenuButton {(w - 70 + 1) / 2, menuRoundDiv(85 * h, 100), 70, 28};
    return l;
}

// Pure: the login page's step. The whole page is the input's face (the
// text rides Input::frameText/frameBackspace); the 开始 button / Enter
// = the start, ESC / the 返回 button = the title.
inline MenuStep menuLoginStep(bool click, bool enter, bool esc,
                              bool startHovered, bool backHovered) {
    MenuStep s;
    if (esc) {
        s.action = MenuAction::Back;
        return s;
    }
    if (enter || (click && startHovered)) {
        s.action = MenuAction::StartSingleplayer;
        return s;
    }
    if (click && backHovered) {
        s.action = MenuAction::Back;
        return s;
    }
    return s;
}

// The tutorial's two-level's geometry (tier 1 = the 5 chapters, tier 2
// = the chapter's demos — the same rows' rect, the title's scale is the
// caller's face).
struct MenuTutorialLayout {
    int titleX = 0, titleY = 0;
    MenuButton rows[5];
    MenuButton back;
    int msgX = 0, msgY = 0; // the 需联机's message (tier 2's reject face)
};

inline MenuTutorialLayout menuTutorialLayout(int w, int h) {
    MenuTutorialLayout l;
    const int titleW = 2 * static_cast<int>(17.0 * kMenuTitleScale) -
                       static_cast<int>(kMenuTitleScale);
    l.titleX = (w - titleW + 1) / 2;
    l.titleY = menuRoundDiv(10 * h, 100);
    const int rowY0 = menuRoundDiv(22 * h, 100);
    constexpr int kRowPitch = 32;
    constexpr int kRowX = 60;
    constexpr int kRowW = 333;
    for (int i = 0; i < 5; ++i) {
        l.rows[i] = MenuButton {kRowX, rowY0 + i * kRowPitch, kRowW, 28};
    }
    l.back = MenuButton {(w - 70 + 1) / 2, menuRoundDiv(85 * h, 100), 70, 28};
    const int msgW = 3 * 25 - 1; // 需联机 3 glyphs @ 1.5
    l.msgX = (w - msgW + 1) / 2;
    l.msgY = menuRoundDiv(78 * h, 100);
    return l;
}

// Pure: the tutorial's tier step. hoveredRow = the row under the pointer
// (-1 = none); the tier's kind selects the back's target (tier 1 = the
// title, tier 2 = the chapters); a row click = OpenChapter (tier 1) /
// LaunchDemo (tier 2 — the mode's filter is the Game's face: the coop's
// row raises the message, never the launch).
struct MenuTutorialStep {
    enum class Kind { None, Back, OpenChapter, LaunchDemo };
    Kind kind = Kind::None;
    int index = -1;
};

inline MenuTutorialStep menuTutorialStep(bool click, bool esc, bool backHovered,
                                         int hoveredRow, bool tier2) {
    MenuTutorialStep s;
    if (esc || (click && backHovered)) {
        s.kind = MenuTutorialStep::Kind::Back;
        return s;
    }
    if (click && hoveredRow >= 0 && hoveredRow < 5) {
        s.index = hoveredRow;
        s.kind = tier2 ? MenuTutorialStep::Kind::LaunchDemo
                       : MenuTutorialStep::Kind::OpenChapter;
        return s;
    }
    return s;
}

// Pure: the demo length's mm:ss text (1/60 fps — the --list-demos'
// VERBATIM read: mm = total/3600, ss = (total%3600)/60).
inline std::string menuDemoLengthText(int frames) {
    const int mm = frames / 3600;
    const int ss = (frames % 3600) / 60;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d:%02d", mm, ss);
    return std::string(buf);
}

// V3-21: the --menu-open's page vocabulary (the closed set — the
// named reject's belt; the Join's sub-page is the Multiplayer's
// interior, never a boot's page). The menu's deep-link's face: the
// demo's return's re-exec lands on the tutorial's page.
inline std::optional<MenuPage> menuOpenPage(const std::string& word) {
    if (word == "title") {
        return MenuPage::Title;
    }
    if (word == "login") {
        return MenuPage::Login;
    }
    if (word == "multiplayer") {
        return MenuPage::Multiplayer;
    }
    if (word == "settings") {
        return MenuPage::Settings;
    }
    if (word == "tutorial") {
        return MenuPage::Tutorial;
    }
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// V3-17d: the multiplayer page (the name's field + the host/join's actions)
// + the join's sub-page (the host:port's field).
// ---------------------------------------------------------------------------

// The multiplayer page's geometry: the name's field (the login's twin —
// m_menuName's carrier, the 12-Latin's cap) + the 创建游戏/加入游戏/
// 返回's buttons. The host's action starts the fresh world on the
// kNetDefaultPort; the join's action opens the Join sub-page.
struct MenuMultiplayerLayout {
    int titleX = 0, titleY = 0;
    MenuButton nameField; // the name's input (300x40, the login's twin)
    int nameHintX = 0, nameHintY = 0; // the （可选）
    MenuButton host;      // 创建游戏
    MenuButton join;      // 加入游戏
    MenuButton back;      // 返回
};

inline MenuMultiplayerLayout menuMultiplayerLayout(int w, int h) {
    MenuMultiplayerLayout l;
    const int titleW = 2 * static_cast<int>(17.0 * kMenuTitleScale) -
                       static_cast<int>(kMenuTitleScale);
    l.titleX = (w - titleW + 1) / 2;
    l.titleY = menuRoundDiv(10 * h, 100);
    constexpr int kFieldW = 300;
    constexpr int kFieldH = 40;
    l.nameField = MenuButton {(w - kFieldW + 1) / 2, menuRoundDiv(30 * h, 100),
                              kFieldW, kFieldH};
    const int hintW = 5 * 17 - 1;
    l.nameHintX = (w - hintW + 1) / 2;
    l.nameHintY = l.nameField.y + l.nameField.h + 8;
    // The host/join's buttons (4 CJK glyphs @ 1.5 = 96 px + the padding).
    constexpr int kWideBtn = 150;
    l.host = MenuButton {(w - kWideBtn + 1) / 2, menuRoundDiv(55 * h, 100),
                         kWideBtn, 28};
    l.join = MenuButton {(w - kWideBtn + 1) / 2, menuRoundDiv(68 * h, 100),
                         kWideBtn, 28};
    l.back = MenuButton {(w - 70 + 1) / 2, menuRoundDiv(85 * h, 100), 70, 28};
    return l;
}

// Pure: the multiplayer page's step. The name's field is the text-input's
// face (the loop's, not a step); the buttons raise their actions.
inline MenuStep menuMultiplayerStep(bool click, bool esc,
                                    bool hostHovered, bool joinHovered,
                                    bool backHovered) {
    MenuStep s;
    if (esc) {
        s.action = MenuAction::Back;
        return s;
    }
    if (click) {
        if (hostHovered) {
            s.action = MenuAction::HostGame;
            return s;
        }
        if (joinHovered) {
            s.action = MenuAction::JoinGame;
            return s;
        }
        if (backHovered) {
            s.action = MenuAction::Back;
            return s;
        }
    }
    return s;
}

// The join's sub-page's geometry: the host:port's field + the 开始/返回.
struct MenuJoinLayout {
    int titleX = 0, titleY = 0;
    MenuButton serverField; // the host:port's input (300x40)
    int hintX = 0, hintY = 0; // the "host:port"'s hint
    MenuButton start;  // 开始
    MenuButton back;   // 返回
};

inline MenuJoinLayout menuJoinLayout(int w, int h) {
    MenuJoinLayout l;
    const int titleW = 2 * static_cast<int>(17.0 * kMenuTitleScale) -
                       static_cast<int>(kMenuTitleScale);
    l.titleX = (w - titleW + 1) / 2;
    l.titleY = menuRoundDiv(10 * h, 100);
    constexpr int kFieldW = 300;
    constexpr int kFieldH = 40;
    l.serverField = MenuButton {(w - kFieldW + 1) / 2,
                                menuRoundDiv(32 * h, 100), kFieldW, kFieldH};
    const int hintW = 9 * 17 - 1; // "host:port" 9 ASCII @ 1.0
    l.hintX = (w - hintW + 1) / 2;
    l.hintY = l.serverField.y + l.serverField.h + 8;
    l.start = MenuButton {(w - 70 + 1) / 2, menuRoundDiv(60 * h, 100), 70, 28};
    l.back = MenuButton {(w - 70 + 1) / 2, menuRoundDiv(75 * h, 100), 70, 28};
    return l;
}

// Pure: the join's sub-page's step. The server's field is the text-input's
// face; the 开始 raises JoinGame (the caller parses host:port — a bad
// value is the named reject's, the back's no-op), 返回/ESC = the back.
inline MenuStep menuJoinStep(bool click, bool esc, bool startHovered,
                             bool backHovered) {
    MenuStep s;
    if (esc) {
        s.action = MenuAction::Back;
        return s;
    }
    if (click) {
        if (startHovered) {
            s.action = MenuAction::JoinGame;
            return s;
        }
        if (backHovered) {
            s.action = MenuAction::Back;
            return s;
        }
    }
    return s;
}

// The host:port's parse (the Join sub-page's 开始's face): "host" + ':' +
// "port". host = the dotted-quad / hostname (dots + alnum + hyphen, the
// LAN's loopback's 127.0.0.1's belt); port = 1..65535. Returns -1 on any
// violation (the caller's named reject). Pure.
inline int menuParseHostPort(const std::string& host, const std::string& port) {
    if (host.empty() || host.size() > 255) {
        return -1;
    }
    for (unsigned char c : host) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') || c == '.' || c == '-';
        if (!ok) {
            return -1;
        }
    }
    if (port.empty() || port.size() > 5) {
        return -1;
    }
    int value = 0;
    for (char c : port) {
        if (c < '0' || c > '9') {
            return -1;
        }
        value = value * 10 + (c - '0');
    }
    if (value < 1 || value > 65535) {
        return -1;
    }
    return value;
}

} // namespace mc
