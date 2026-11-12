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

TEST(MenuLayoutTest, StubGeometryAtGateSpace) {
    const MenuStubLayout l = menuStubLayout(kW, kH);
    // The page title (2 CJK glyphs @ 2.0 = 66 px wide).
    EXPECT_EQ(l.titleX, (kW - 66 + 1) / 2);  // 194
    EXPECT_EQ(l.titleY, 50);                 // lround(0.10 * 496)
    // The back button (the 120-px centered, at 0.85h).
    EXPECT_EQ(l.back.x, (kW - kMenuBackWidth + 1) / 2); // 167
    EXPECT_EQ(l.back.y, 422);                           // lround(0.85h)
    EXPECT_EQ(l.back.w, kMenuBackWidth);
    EXPECT_EQ(l.back.h, kMenuItemHeight);
    EXPECT_TRUE(l.back.contains(l.back.x + l.back.w / 2, l.back.y + 16));
    // The tutorial's chapter rows (the display rects, the 0.30h start,
    // the 30-px pitch).
    EXPECT_EQ(l.rows[0].y, 149);  // lround(0.30 * 496)
    EXPECT_EQ(l.rows[4].y, 269);
    EXPECT_EQ(l.rows[0].w, kW);
}

TEST(MenuStepTest, TitleActions) {
    // ESC quits at any hover.
    EXPECT_EQ(menuStepTitle(false, false, true, 0).action, MenuAction::Quit);
    EXPECT_EQ(menuStepTitle(true, false, true, 3).action, MenuAction::Quit);
    // The click / the Enter activate the hovered item.
    EXPECT_EQ(menuStepTitle(true, false, false, 0).action,
              MenuAction::StartSingleplayer);
    EXPECT_EQ(menuStepTitle(false, true, false, 1).page, MenuPage::Multiplayer);
    EXPECT_EQ(menuStepTitle(true, false, false, 2).page, MenuPage::Settings);
    EXPECT_EQ(menuStepTitle(false, true, false, 3).page, MenuPage::Tutorial);
    EXPECT_EQ(menuStepTitle(true, false, false, 4).action, MenuAction::Quit);
    // A click in the air (no hover) is a no-op.
    const MenuStep air = menuStepTitle(true, false, false, -1);
    EXPECT_EQ(air.action, MenuAction::None);
    EXPECT_EQ(air.page, MenuPage::Title);
    // An out-of-range hover is rejected (the index's belt).
    EXPECT_EQ(menuStepTitle(true, false, false, 5).action, MenuAction::None);
    // Nothing pressed = nothing.
    EXPECT_EQ(menuStepTitle(false, false, false, 0).action, MenuAction::None);
    // The transitions' page + the action are mutually exclusive (a
    // navigated step raises no action).
    EXPECT_EQ(menuStepTitle(true, false, false, 1).action, MenuAction::None);
}

TEST(MenuStepTest, StubBack) {
    // ESC = the title's back.
    EXPECT_EQ(menuStepStub(false, false, true, false).action,
              MenuAction::Back);
    // The back button's click / Enter (hovered).
    EXPECT_EQ(menuStepStub(true, false, false, true).action, MenuAction::Back);
    EXPECT_EQ(menuStepStub(false, true, false, true).action, MenuAction::Back);
    // A click off the back button is a no-op (17a's stub's display-only
    // content).
    EXPECT_EQ(menuStepStub(true, false, false, false).action,
              MenuAction::None);
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
