// V3-17a: the main menu's pure face — the layout + the state machine
// (renderMenu's twin). The geometry pins below use the GATE's UI
// space (679x744 window @ ui.scale 1.5 = 453x496, the S1's base20
// anchor's geometry): the gate's click arm drives xdotool to the
// 单人 button's center computed by the SAME formula.
#include <gtest/gtest.h>

#include <cstddef>
#include <string>

#include "core/demo.hpp"
#include "core/menu_layout.hpp"

using namespace mc;

namespace {

// The gate's UI space (the S1's window 679x744 @ ui.scale 1.5).
constexpr int kW = 453;
constexpr int kH = 496;

int hoverAt(const MenuButton* items, int n, int x, int y) {
    for (int i = 0; i < n; ++i) {
        if (items[i].contains(x, y)) {
            return i;
        }
    }
    return -1;
}

} // namespace

TEST(MenuLayoutTest, TitleGeometryAtGateSpace) {
    // The version "v3.43.0" is 7 glyphs at scale 1.0 = 7 x 17 - 1.
    const std::string version = "v" + std::string(TM_VERSION);
    const int versionW = static_cast<int>(version.size()) * 17 - 1;
    const MenuTitleLayout l = menuTitleLayout(kW, kH, versionW);

    // The logo "TokenMine.cpp" (13 glyphs @ 2.0 = 440 px wide).
    EXPECT_EQ(l.logoX, (kW - 440 + 1) / 2);  // 7
    EXPECT_EQ(l.logoY, 69);                  // lround(0.14 * 496)
    EXPECT_EQ(l.versionX, (kW - versionW + 1) / 2);
    EXPECT_EQ(l.versionY, l.logoY + 40);     // 109

    // The 5 item buttons (centered, the 44-px pitch from 0.40h).
    for (int i = 0; i < kMenuTitleItemCount; ++i) {
        EXPECT_EQ(l.items[i].x, (kW - kMenuItemWidth + 1) / 2); // 107
        EXPECT_EQ(l.items[i].y, 198 + i * kMenuItemPitch);      // 198..374
        EXPECT_EQ(l.items[i].w, kMenuItemWidth);
        EXPECT_EQ(l.items[i].h, kMenuItemHeight);
    }
    EXPECT_EQ(l.items[0].y, 198);  // lround(0.40 * 496)
    EXPECT_EQ(l.items[4].y, 374);

    // The gate's click: the 单人 button's center (row 0).
    const int cx = l.items[0].x + l.items[0].w / 2;  // 227
    const int cy = l.items[0].y + l.items[0].h / 2;  // 214
    EXPECT_EQ(hoverAt(l.items, kMenuTitleItemCount, cx, cy), 0);
    // A pixel in the pitch's gap hits nothing.
    EXPECT_EQ(hoverAt(l.items, kMenuTitleItemCount, cx, l.items[0].y + 40),
              -1);
    // The row's text center belongs to the row (the hit rect is the
    // 240-px button, not the narrower text).
    EXPECT_EQ(hoverAt(l.items, kMenuTitleItemCount, l.items[2].x + 5,
                      l.items[2].y + 5),
              2);
}

TEST(MenuStepTest, TitleActions) {
    // ESC quits at any hover.
    EXPECT_EQ(menuStepTitle(false, false, true, 0, -1).action, MenuAction::Quit);
    EXPECT_EQ(menuStepTitle(true, false, true, 3, -1).action, MenuAction::Quit);
    // The click / the Enter activate the hovered item (V3-17c: item 0
    // = the LOGIN page — the name's entry before the start).
    EXPECT_EQ(menuStepTitle(true, false, false, 0, -1).page, MenuPage::Login);
    EXPECT_EQ(menuStepTitle(true, false, false, 0, -1).action, MenuAction::None);
    EXPECT_EQ(menuStepTitle(false, true, false, 1, 1).page, MenuPage::Multiplayer);
    EXPECT_EQ(menuStepTitle(true, false, false, 2, -1).page, MenuPage::Settings);
    EXPECT_EQ(menuStepTitle(false, true, false, 3, 3).page, MenuPage::Tutorial);
    EXPECT_EQ(menuStepTitle(true, false, false, 4, -1).action, MenuAction::Quit);
    // A click in the air (no hover) is a no-op.
    const MenuStep air = menuStepTitle(true, false, false, -1, -1);
    EXPECT_EQ(air.action, MenuAction::None);
    EXPECT_EQ(air.page, MenuPage::Title);
    // An out-of-range hover is rejected (the index's belt).
    EXPECT_EQ(menuStepTitle(true, false, false, 5, -1).action, MenuAction::None);
    // Nothing pressed = nothing.
    EXPECT_EQ(menuStepTitle(false, false, false, 0, -1).action, MenuAction::None);
    // The transitions' page + the action are mutually exclusive (a
    // navigated step raises no action).
    EXPECT_EQ(menuStepTitle(true, false, false, 1, -1).action,
              MenuAction::None);
}

TEST(MenuSettingsTest, LayoutGeometryAtGateSpace) {
    // The gate's UI space (679x744 @ ui.scale 1.5 = 453x496).
    const MenuSettingsLayout l = menuSettingsLayout(kW, kH);
    EXPECT_EQ(l.titleX, (kW - 66 + 1) / 2);  // 194
    EXPECT_EQ(l.titleY, 50);                  // lround(0.10 * 496)
    // The 8 rows (0.18h start, the 36-px pitch).
    EXPECT_EQ(l.rows[0].label.x, 40);
    EXPECT_EQ(l.rows[0].label.y, 89);         // lround(0.18 * 496)
    EXPECT_EQ(l.rows[7].label.y, 89 + 7 * 36); // 341
    EXPECT_EQ(l.rows[0].minus.x, kW - 140);    // 313
    EXPECT_EQ(l.rows[0].plus.x, kW - 104);     // 349
    EXPECT_EQ(l.rows[0].valueX, kW / 2);       // 226
    EXPECT_EQ(l.rows[0].minus.w, 24);
    EXPECT_EQ(l.rows[0].minus.h, 24);
    // The save button (70 px wide = 保存 50 + the padding, at 0.85h).
    EXPECT_EQ(l.save.w, 70);
    EXPECT_EQ(l.save.x, (kW - 70 + 1) / 2);   // 192
    EXPECT_EQ(l.save.y, 422);
    // The hint (8 glyphs @ 1.0 = 135 px, at 0.75h).
    EXPECT_EQ(l.noteX, (kW - 135 + 1) / 2);   // 159
    EXPECT_EQ(l.noteY, 372);
    // The control's hit test (the gate's xdotool clicks the same
    // centers).
    EXPECT_EQ(menuSettingsControl(l, l.rows[0].minus.x + 12,
                                  l.rows[0].minus.y + 12),
              0); // row 0's minus
    EXPECT_EQ(menuSettingsControl(l, l.rows[0].plus.x + 12,
                                  l.rows[0].plus.y + 12),
              1); // row 0's plus
    EXPECT_EQ(menuSettingsControl(l, l.rows[2].minus.x + 12,
                                  l.rows[2].minus.y + 12),
              4); // the toggle row's button
    EXPECT_EQ(menuSettingsControl(l, l.rows[2].plus.x + 12,
                                  l.rows[2].plus.y + 12),
              -1); // the toggle's plus = dead
    EXPECT_EQ(menuSettingsControl(l, l.rows[6].minus.x + 12,
                                  l.rows[6].minus.y + 12),
              12);
    EXPECT_EQ(menuSettingsControl(l, l.save.x + 35, l.save.y + 14),
              kMenuSettingsSave);
    EXPECT_EQ(menuSettingsControl(l, 10, 400), -1);
}

TEST(MenuSettingsTest, StepActions) {
    // ESC = back.
    EXPECT_EQ(menuSettingsStep(false, false, true, 0, -1, false, false).kind,
              MenuSettingsStep::Kind::Back);
    // The stepper's dec / inc (the control id's row*2 encoding).
    EXPECT_EQ(menuSettingsStep(true, false, false, 0, -1, false, false).kind,
              MenuSettingsStep::Kind::Dec);
    EXPECT_EQ(menuSettingsStep(true, false, false, 0, -1, false, false).row, 0);
    EXPECT_EQ(menuSettingsStep(true, false, false, 1, -1, false, false).kind,
              MenuSettingsStep::Kind::Inc);
    EXPECT_EQ(menuSettingsStep(true, false, false, 13, -1, false, false).kind,
              MenuSettingsStep::Kind::Inc);
    EXPECT_EQ(menuSettingsStep(true, false, false, 13, -1, false, false).row, 6);
    // The toggle rows (2 天气 / 5 全屏): the minus's button = the flip.
    EXPECT_EQ(menuSettingsStep(true, false, false, 4, -1, false, false).kind,
              MenuSettingsStep::Kind::Toggle);
    EXPECT_EQ(menuSettingsStep(true, false, false, 4, -1, false, false).row, 2);
    EXPECT_EQ(menuSettingsStep(true, false, false, 10, -1, false, false).kind,
              MenuSettingsStep::Kind::Toggle);
    EXPECT_EQ(menuSettingsStep(true, false, false, 10, -1, false, false).row, 5);
    // The save (the button OR the Enter's convenience).
    EXPECT_EQ(menuSettingsStep(true, false, false, kMenuSettingsSave, -1, false, false).kind,
              MenuSettingsStep::Kind::Save);
    EXPECT_EQ(menuSettingsStep(false, true, false, -1, -1, false, false).kind,
              MenuSettingsStep::Kind::Save);
    // A click in the air = nothing.
    EXPECT_EQ(menuSettingsStep(true, false, false, -1, -1, false, false).kind,
              MenuSettingsStep::Kind::None);
    EXPECT_EQ(menuSettingsStep(false, false, false, 0, -1, false, false).kind,
              MenuSettingsStep::Kind::None);
}

TEST(MenuStepTest, TitleLabelsAndChaptersAreFontSafe) {
    // The label's vocabulary is pinned (a typo renders '?' — the
    // glyph's presence itself is the font's test's face; here: the
    // exact strings the render consumes).
    EXPECT_STREQ(kMenuTitleItems[0], "单人");
    EXPECT_STREQ(kMenuTitleItems[1], "多人");
    EXPECT_STREQ(kMenuTitleItems[2], "设置");
    EXPECT_STREQ(kMenuTitleItems[3], "教学");
    EXPECT_STREQ(kMenuTitleItems[4], "退出");
    EXPECT_STREQ(kMenuBackLabel, "返回");
    // The tutorial stub's chapters = kDemoCategories' single source
    // (the 5 ids' order pinned — the page renders them in this order).
    EXPECT_EQ(std::size(kDemoCategories), 5u);
    EXPECT_STREQ(kDemoCategories[0].id, "day1");
    EXPECT_STREQ(kDemoCategories[0].title, "第一天基础");
    EXPECT_STREQ(kDemoCategories[1].title, "建造与大地");
    EXPECT_STREQ(kDemoCategories[2].title, "维度远征");
    EXPECT_STREQ(kDemoCategories[3].title, "生存历险");
    EXPECT_STREQ(kDemoCategories[4].title, "多人协作");
}

// ---------------------------------------------------------------------------
// V3-17c: the login page + the tutorial's two-level (the pure face).
// ---------------------------------------------------------------------------

TEST(MenuLoginTest, LayoutGeometryAtGateSpace) {
    const MenuLoginLayout l = menuLoginLayout(kW, kH);
    // The title 名字 (66 px @ 2.0).
    EXPECT_EQ(l.titleX, 194);
    EXPECT_EQ(l.titleY, 50);
    // The field (300x40, centered, at 0.32h).
    EXPECT_EQ(l.field.x, 77);
    EXPECT_EQ(l.field.y, 159);
    EXPECT_EQ(l.field.w, 300);
    EXPECT_EQ(l.field.h, 40);
    // The hint （可选） (84 px @ 1.0, under the field).
    EXPECT_EQ(l.hintX, 185);
    EXPECT_EQ(l.hintY, 159 + 40 + 8);
    // The 开始 / 返回 buttons (70 px, at 0.72h / 0.85h).
    EXPECT_EQ(l.start.x, 192);
    EXPECT_EQ(l.start.y, 357);
    EXPECT_EQ(l.start.w, 70);
    EXPECT_EQ(l.back.x, 192);
    EXPECT_EQ(l.back.y, 422);
    // The hit test (the gate's xdotool clicks the same centers).
    EXPECT_TRUE(l.start.contains(l.start.x + 35, l.start.y + 14));
    EXPECT_TRUE(l.back.contains(l.back.x + 35, l.back.y + 14));
    EXPECT_FALSE(l.start.contains(10, 10));
}

TEST(MenuLoginTest, StepActions) {
    // The 开始 button / the Enter = the singleplayer's start.
    EXPECT_EQ(menuLoginStep(true, false, false, true, false).action,
              MenuAction::StartSingleplayer);
    EXPECT_EQ(menuLoginStep(false, true, false, false, false).action,
              MenuAction::StartSingleplayer);
    // ESC / the 返回 button = the title's back.
    EXPECT_EQ(menuLoginStep(false, false, true, false, false).action,
              MenuAction::Back);
    EXPECT_EQ(menuLoginStep(true, false, false, false, true).action,
              MenuAction::Back);
    // A click in the field (the input's face, no button) = nothing
    // (the text rides frameText, not a step).
    EXPECT_EQ(menuLoginStep(true, false, false, false, false).action,
              MenuAction::None);
}

TEST(MenuTutorialTest, LayoutGeometryAtGateSpace) {
    const MenuTutorialLayout l = menuTutorialLayout(kW, kH);
    EXPECT_EQ(l.titleX, 194);
    EXPECT_EQ(l.titleY, 50);
    // The 5 rows (0.22h start, the 32-px pitch).
    EXPECT_EQ(l.rows[0].x, 60);
    EXPECT_EQ(l.rows[0].y, 109);
    EXPECT_EQ(l.rows[0].w, 333);
    EXPECT_EQ(l.rows[4].y, 109 + 4 * 32); // 237
    // The back button (70 px, at 0.85h).
    EXPECT_EQ(l.back.x, 192);
    EXPECT_EQ(l.back.y, 422);
    // The 需联机's message (74 px @ 1.5, at 0.78h).
    EXPECT_EQ(l.msgX, 190);
    EXPECT_EQ(l.msgY, 387);
    EXPECT_TRUE(l.rows[2].contains(60 + 166, l.rows[2].y + 14));
}

TEST(MenuTutorialTest, StepActions) {
    // Tier 1: a row click = OpenChapter.
    EXPECT_EQ(menuTutorialStep(true, false, false, 2, false, -1, false, false, false, 5).kind,
              MenuTutorialStep::Kind::OpenChapter);
    EXPECT_EQ(menuTutorialStep(true, false, false, 2, false, -1, false, false, false, 5).index, 2);
    // Tier 2: a row click = LaunchDemo (the mode's filter is the
    // Game's face — the pure step doesn't know solo/coop).
    EXPECT_EQ(menuTutorialStep(true, false, false, 1, true, -1, false, false, false, 4).kind,
              MenuTutorialStep::Kind::LaunchDemo);
    EXPECT_EQ(menuTutorialStep(true, false, false, 1, true, -1, false, false, false, 4).index, 1);
    // ESC / the back button = Back (the target is the Game's face:
    // tier 2 -> the chapters, tier 1 -> the title).
    EXPECT_EQ(menuTutorialStep(false, true, false, -1, false, -1, false, false, false, 5).kind,
              MenuTutorialStep::Kind::Back);
    EXPECT_EQ(menuTutorialStep(true, false, true, -1, true, -1, false, false, false, 4).kind,
              MenuTutorialStep::Kind::Back);
    // A click in the air = nothing.
    EXPECT_EQ(menuTutorialStep(true, false, false, -1, false, -1, false, false, false, 5).kind,
              MenuTutorialStep::Kind::None);
    // An out-of-range row is rejected (the 5-row belt).
    EXPECT_EQ(menuTutorialStep(true, false, false, 5, false, -1, false, false, false, 5).kind,
              MenuTutorialStep::Kind::None);
    EXPECT_EQ(menuTutorialStep(true, false, false, -2, false, -1, false, false, false, 5).kind,
              MenuTutorialStep::Kind::None);
}

TEST(MenuTutorialTest, DemoLengthText) {
    // 1/60 fps -> mm:ss (the --list-demos' verbatim read: the index's
    // lengths' pinned values).
    EXPECT_EQ(menuDemoLengthText(180), "00:03");
    EXPECT_EQ(menuDemoLengthText(11387), "03:09");  // guide_mining
    EXPECT_EQ(menuDemoLengthText(15214), "04:13");  // guide_dragon
    EXPECT_EQ(menuDemoLengthText(10865), "03:01");  // guide_nether
    EXPECT_EQ(menuDemoLengthText(0), "00:00");
    EXPECT_EQ(menuDemoLengthText(59), "00:00");
    EXPECT_EQ(menuDemoLengthText(60), "00:01");
}
// ---------------------------------------------------------------------------
// V3-17d: the multiplayer page (name + host/join) + the join sub-page
// (host:port) — the pure face (renderMenu's twin).
// ---------------------------------------------------------------------------

TEST(MenuMultiplayerTest, LayoutGeometryAtGateSpace) {
    const MenuMultiplayerLayout l = menuMultiplayerLayout(kW, kH);
    EXPECT_EQ(l.titleX, 194);  // 多人 (66 px @ 2.0)
    EXPECT_EQ(l.titleY, 50);   // lround(0.10 * 496)
    // The name's field (the login's twin: 300x40 centered, at 0.30h).
    EXPECT_EQ(l.nameField.x, 77);
    EXPECT_EQ(l.nameField.y, 149);
    EXPECT_EQ(l.nameField.w, 300);
    EXPECT_EQ(l.nameField.h, 40);
    EXPECT_EQ(l.nameHintX, 185);  // （可选） 84 px @ 1.0
    EXPECT_EQ(l.nameHintY, 149 + 40 + 8);
    // The 创建游戏/加入游戏's buttons (150 px, at 0.55h / 0.68h).
    EXPECT_EQ(l.host.x, 152);
    EXPECT_EQ(l.host.y, 273);
    EXPECT_EQ(l.host.w, 150);
    EXPECT_EQ(l.host.h, 28);
    EXPECT_EQ(l.join.x, 152);
    EXPECT_EQ(l.join.y, 337);
    // The 返回 (70 px, at 0.85h).
    EXPECT_EQ(l.back.x, 192);
    EXPECT_EQ(l.back.y, 422);
    // The hit test (the gate's xdotool clicks the same centers).
    EXPECT_TRUE(l.host.contains(l.host.x + 75, l.host.y + 14));
    EXPECT_TRUE(l.join.contains(l.join.x + 75, l.join.y + 14));
    EXPECT_TRUE(l.back.contains(l.back.x + 35, l.back.y + 14));
    EXPECT_FALSE(l.host.contains(10, 10));
    EXPECT_FALSE(l.join.contains(l.host.x + 75, l.host.y + 50));
}

TEST(MenuMultiplayerTest, StepActions) {
    // ESC = the title's back.
    EXPECT_EQ(menuMultiplayerStep(false, true, false, false, false, -1, false).action,
              MenuAction::Back);
    // The 创建游戏's click = the host's start.
    EXPECT_EQ(menuMultiplayerStep(true, false, true, false, false, -1, false).action,
              MenuAction::HostGame);
    // The 加入游戏's click = the join's sub-page.
    EXPECT_EQ(menuMultiplayerStep(true, false, false, true, false, -1, false).action,
              MenuAction::JoinGame);
    // The 返回's click = the back.
    EXPECT_EQ(menuMultiplayerStep(true, false, false, false, true, -1, false).action,
              MenuAction::Back);
    // A click in the air / nothing pressed = nothing.
    EXPECT_EQ(menuMultiplayerStep(true, false, false, false, false, -1, false).action,
              MenuAction::None);
    EXPECT_EQ(menuMultiplayerStep(false, false, false, false, false, -1, false).action,
              MenuAction::None);
}

TEST(MenuJoinTest, LayoutGeometryAtGateSpace) {
    const MenuJoinLayout l = menuJoinLayout(kW, kH);
    EXPECT_EQ(l.titleX, 194);
    EXPECT_EQ(l.titleY, 50);
    // The host:port's field (300x40 centered, at 0.32h).
    EXPECT_EQ(l.serverField.x, 77);
    EXPECT_EQ(l.serverField.y, 159);
    EXPECT_EQ(l.serverField.w, 300);
    EXPECT_EQ(l.serverField.h, 40);
    // The "host:port"'s hint (9 ASCII @ 1.0 = 152 px).
    EXPECT_EQ(l.hintX, 151);
    EXPECT_EQ(l.hintY, 159 + 40 + 8);
    // The 开始/返回's buttons (70 px, at 0.60h / 0.75h).
    EXPECT_EQ(l.start.x, 192);
    EXPECT_EQ(l.start.y, 298);
    EXPECT_EQ(l.back.x, 192);
    EXPECT_EQ(l.back.y, 372);
    EXPECT_TRUE(l.start.contains(l.start.x + 35, l.start.y + 14));
    EXPECT_TRUE(l.back.contains(l.back.x + 35, l.back.y + 14));
    EXPECT_FALSE(l.start.contains(10, 10));
}

TEST(MenuJoinTest, StepActions) {
    // ESC = the multiplayer's back.
    EXPECT_EQ(menuJoinStep(false, true, false, false, -1, false).action,
              MenuAction::Back);
    // The 开始's click = the join's connect (the parse is the loop's).
    EXPECT_EQ(menuJoinStep(true, false, true, false, -1, false).action,
              MenuAction::JoinGame);
    // The 返回's click = the back.
    EXPECT_EQ(menuJoinStep(true, false, false, true, -1, false).action,
              MenuAction::Back);
    // A click in the air / nothing pressed = nothing.
    EXPECT_EQ(menuJoinStep(true, false, false, false, -1, false).action,
              MenuAction::None);
    EXPECT_EQ(menuJoinStep(false, false, false, false, -1, false).action,
              MenuAction::None);
}

TEST(MenuJoinTest, ParseHostPort) {
    // The valid's faces (the LAN's loopback's + the hostname's belts).
    EXPECT_EQ(menuParseHostPort("127.0.0.1", "5555"), 5555);
    EXPECT_EQ(menuParseHostPort("localhost", "6000"), 6000);
    EXPECT_EQ(menuParseHostPort("a-b.c", "100"), 100);
    EXPECT_EQ(menuParseHostPort("h", "1"), 1);
    EXPECT_EQ(menuParseHostPort("h", "65535"), 65535);
    EXPECT_EQ(menuParseHostPort("HOSTNAME", "8080"), 8080); // case's belt
    // The reject's faces (the named -1's).
    EXPECT_EQ(menuParseHostPort("", "5555"), -1);          // no host
    EXPECT_EQ(menuParseHostPort("h", ""), -1);             // no port
    EXPECT_EQ(menuParseHostPort("h", "0"), -1);            // port 0
    EXPECT_EQ(menuParseHostPort("h", "65536"), -1);        // port > 65535
    EXPECT_EQ(menuParseHostPort("h", "99999"), -1);        // 5-digit's over
    EXPECT_EQ(menuParseHostPort("h ", "5555"), -1);        // the space's
    EXPECT_EQ(menuParseHostPort("主 机", "5555"), -1);     // the non-Latin's
    EXPECT_EQ(menuParseHostPort("h:x", "5555"), -1);       // the colon's
    EXPECT_EQ(menuParseHostPort("h", "1a"), -1);           // the digit's belt
    EXPECT_EQ(menuParseHostPort(std::string(256, 'a'), "5555"), -1); // > 255
}

// ---------------------------------------------------------------------------
// V3-21: the --menu-open's page vocabulary (the demo's return's
// deep-link's belt).
// ---------------------------------------------------------------------------

TEST(MenuOpenTest, PageVocabulary) {
    // The 5 words (the Join's sub-page is the Multiplayer's interior —
    // never a boot's page).
    EXPECT_EQ(menuOpenPage("title"), MenuPage::Title);
    EXPECT_EQ(menuOpenPage("login"), MenuPage::Login);
    EXPECT_EQ(menuOpenPage("multiplayer"), MenuPage::Multiplayer);
    EXPECT_EQ(menuOpenPage("settings"), MenuPage::Settings);
    EXPECT_EQ(menuOpenPage("tutorial"), MenuPage::Tutorial);
    // The reject's faces (the named error's belt).
    EXPECT_FALSE(menuOpenPage("").has_value());
    EXPECT_FALSE(menuOpenPage("Title").has_value()); // case's strict
    EXPECT_FALSE(menuOpenPage("join").has_value());  // the sub-page's
    EXPECT_FALSE(menuOpenPage("bogus").has_value());
    EXPECT_FALSE(menuOpenPage("demo").has_value());
}

// ---------------------------------------------------------------------------
// V3-22: the menu's keyboard's navigation (the arrows' + WASD's
// selection's move, the Enter's activation — the pure step's twin's
// face: the Input's edge's semantics are the loop's, the test feeds
// the raw booleans).
// ---------------------------------------------------------------------------

TEST(MenuKeyboardTest, MoveSel) {
    // The first press's entry (down = 0, up = the last).
    EXPECT_EQ(menuMoveSel(-1, false, true, 5), 0);
    EXPECT_EQ(menuMoveSel(-1, true, false, 5), 4);
    EXPECT_EQ(menuMoveSel(-1, false, false, 5), -1); // no move's no-op
    // The wrap's both directions.
    EXPECT_EQ(menuMoveSel(0, true, false, 5), 4);
    EXPECT_EQ(menuMoveSel(4, false, true, 5), 0);
    EXPECT_EQ(menuMoveSel(2, false, true, 5), 3);
    EXPECT_EQ(menuMoveSel(2, true, false, 5), 1);
    // The count's belt (0 = the no-row's no-op, the over's clamp).
    EXPECT_EQ(menuMoveSel(-1, false, true, 0), -1);
    EXPECT_EQ(menuMoveSel(7, false, true, 5), 0);
    EXPECT_EQ(menuMoveSel(7, true, false, 5), 3);
}

TEST(MenuKeyboardTest, TitleEnterActivatesSelection) {
    // The keyboard's Enter (no hover — the mouse's (10,10) twin): the
    // selection's activate.
    EXPECT_EQ(menuStepTitle(false, true, false, -1, 2).page,
              MenuPage::Settings);
    EXPECT_EQ(menuStepTitle(false, true, false, -1, 3).page,
              MenuPage::Tutorial);
    EXPECT_EQ(menuStepTitle(false, true, false, -1, 4).action,
              MenuAction::Quit);
    // No selection = no Enter's no-op (the mouse's face unchanged).
    EXPECT_EQ(menuStepTitle(false, true, false, -1, -1).action,
              MenuAction::None);
    // The click's hover's face (the sel's independent — the old's
    // bit-line).
    EXPECT_EQ(menuStepTitle(true, false, false, 2, -1).page,
              MenuPage::Settings);
}

TEST(MenuKeyboardTest, SettingsLeftRightAdjustsSelectedRow) {
    // The step row's (0 = 视图距离): left = Dec, right = Inc.
    EXPECT_EQ(menuSettingsStep(false, false, false, -1, 0, true, false).kind,
              MenuSettingsStep::Kind::Dec);
    EXPECT_EQ(menuSettingsStep(false, false, false, -1, 0, true, false).row, 0);
    EXPECT_EQ(menuSettingsStep(false, false, false, -1, 0, false, true).kind,
              MenuSettingsStep::Kind::Inc);
    // The toggle row's (2 = 天气): the either direction = the Toggle.
    EXPECT_EQ(menuSettingsStep(false, false, false, -1, 2, true, false).kind,
              MenuSettingsStep::Kind::Toggle);
    EXPECT_EQ(menuSettingsStep(false, false, false, -1, 2, false, true).kind,
              MenuSettingsStep::Kind::Toggle);
    // No selection = no adjust's no-op (the Enter's Save's face
    // unchanged).
    EXPECT_EQ(menuSettingsStep(false, false, false, -1, -1, true, false).kind,
              MenuSettingsStep::Kind::None);
}

TEST(MenuKeyboardTest, TutorialKeyboard) {
    // tier 1: the Enter / Right's chapter's open (the selection's
    // index).
    EXPECT_EQ(menuTutorialStep(false, false, false, -1, false, 1, true, false,
                               false, 5)
                  .kind,
              MenuTutorialStep::Kind::OpenChapter);
    EXPECT_EQ(menuTutorialStep(false, false, false, -1, false, 1, false, false,
                               true, 5)
                  .index, 1);
    // tier 2: the Left's back (the sel's independent), the Enter /
    // Right's launch (the selection's index).
    EXPECT_EQ(menuTutorialStep(false, false, false, -1, true, 0, false, true,
                               false, 4)
                  .kind,
              MenuTutorialStep::Kind::Back);
    EXPECT_EQ(menuTutorialStep(false, false, false, -1, true, 2, true, false,
                               false, 4)
                  .kind,
              MenuTutorialStep::Kind::LaunchDemo);
    EXPECT_EQ(menuTutorialStep(false, false, false, -1, true, 2, false, false,
                               true, 4)
                  .index, 2);
    // The click's row's face (the old's bit-line, the rowCount's
    // belt: the out-of-range's no-op).
    EXPECT_EQ(menuTutorialStep(true, false, false, 5, false, -1, false,
                               false, false, 5)
                  .kind,
              MenuTutorialStep::Kind::None);
}

TEST(MenuKeyboardTest, MultiplayerAndJoinEnter) {
    // The multiplayer's buttons' selection's Enter (0 = 创建, 1 = 加入,
    // 2 = 返回).
    EXPECT_EQ(menuMultiplayerStep(false, false, false, false, false, 0, true)
                  .action,
              MenuAction::HostGame);
    EXPECT_EQ(menuMultiplayerStep(false, false, false, false, false, 1, true)
                  .action,
              MenuAction::JoinGame);
    EXPECT_EQ(menuMultiplayerStep(false, false, false, false, false, 2, true)
                  .action,
              MenuAction::Back);
    EXPECT_EQ(menuMultiplayerStep(false, false, false, false, false, -1, true)
                  .action,
              MenuAction::None);
    // The join's 开始's parse's caller's belt (the action's raise only).
    EXPECT_EQ(menuJoinStep(false, false, false, false, 0, true).action,
              MenuAction::JoinGame);
    EXPECT_EQ(menuJoinStep(false, false, false, false, 1, true).action,
              MenuAction::Back);
}
