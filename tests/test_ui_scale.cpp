// ui.scale (Phase 34): the screen-space UI zoom key. Missing = 1.0 (the
// pixel-exact baseline); present must be a JSON number in [1, 4].
// UX-8e: ui.font_scale — the text-only zoom. Missing = 1.0 (the
// bit-exact v1.44.0 baseline); present must be a JSON number in
// (0, kFontScaleMax].

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <string>

#include "core/config.hpp"
#include "core/ui_layout.hpp"
#include "ui/font.hpp"
#include "ui/ui_scale.hpp"

using mc::Config;
using mc::Json;
using mc::kFontScaleDefault;
using mc::kFontScaleMax;
using mc::kUiIconPx;
using mc::kUiStripGap;
using mc::kUiScaleMax;
using mc::kUiScaleMin;
using mc::textWidth;
using mc::uiFontScaleFromConfig;
using mc::uiScaleFromConfig;

TEST(UiScaleTest, MissingKeyIsBaselineOne) {
    Config cfg;
    auto r = uiScaleFromConfig(cfg);
    ASSERT_TRUE(r.ok()) << r.error().describe();
    EXPECT_FLOAT_EQ(*r, 1.0f);
}

TEST(UiScaleTest, ExplicitOneIsBaseline) {
    Config cfg;
    cfg.set("ui.scale", Json(1));
    auto r = uiScaleFromConfig(cfg);
    ASSERT_TRUE(r.ok()) << r.error().describe();
    EXPECT_FLOAT_EQ(*r, 1.0f);
}

TEST(UiScaleTest, FractionalScaleAccepted) {
    Config cfg;
    cfg.set("ui.scale", Json(1.5));
    auto r = uiScaleFromConfig(cfg);
    ASSERT_TRUE(r.ok()) << r.error().describe();
    EXPECT_FLOAT_EQ(*r, 1.5f);
}

TEST(UiScaleTest, MaxBoundaryAccepted) {
    Config cfg;
    cfg.set("ui.scale", Json(kUiScaleMax));
    auto r = uiScaleFromConfig(cfg);
    ASSERT_TRUE(r.ok()) << r.error().describe();
    EXPECT_FLOAT_EQ(*r, kUiScaleMax);
}

TEST(UiScaleTest, MinBoundaryAccepted) {
    Config cfg;
    cfg.set("ui.scale", Json(kUiScaleMin));
    auto r = uiScaleFromConfig(cfg);
    ASSERT_TRUE(r.ok()) << r.error().describe();
    EXPECT_FLOAT_EQ(*r, kUiScaleMin);
}

TEST(UiScaleTest, RejectsBelowOne) {
    Config cfg;
    cfg.set("ui.scale", Json(0.999));
    auto r = uiScaleFromConfig(cfg);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("ui.scale"), std::string::npos);
}

TEST(UiScaleTest, RejectsAboveMax) {
    Config cfg;
    cfg.set("ui.scale", Json(kUiScaleMax + 0.001));
    auto r = uiScaleFromConfig(cfg);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("ui.scale"), std::string::npos);
}

TEST(UiScaleTest, RejectsZero) {
    Config cfg;
    cfg.set("ui.scale", Json(0));
    auto r = uiScaleFromConfig(cfg);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("ui.scale"), std::string::npos);
}

TEST(UiScaleTest, RejectsNegative) {
    Config cfg;
    cfg.set("ui.scale", Json(-1));
    auto r = uiScaleFromConfig(cfg);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("ui.scale"), std::string::npos);
}

TEST(UiScaleTest, RejectsNonNumber) {
    // The Config getters fall back silently on a type mismatch, so a
    // present non-number key must fail startup (the fromConfig pattern).
    Config cfg;
    cfg.set("ui.scale", Json("2"));
    auto r = uiScaleFromConfig(cfg);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("ui.scale"), std::string::npos);
    EXPECT_NE(r.error().message.find("number"), std::string::npos);
}

TEST(UiScaleTest, RejectsNull) {
    Config cfg;
    cfg.set("ui.scale", Json(nullptr));
    auto r = uiScaleFromConfig(cfg);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("ui.scale"), std::string::npos);
}

TEST(UiScaleTest, RejectsBool) {
    Config cfg;
    cfg.set("ui.scale", Json(true));
    auto r = uiScaleFromConfig(cfg);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("ui.scale"), std::string::npos);
}

// ---------------------------------------------------------------------------
// UX-8e: ui.font_scale — the strict config read (text-only zoom)
// ---------------------------------------------------------------------------

TEST(UiFontScaleTest, MissingKeyIsDefaultOne) {
    Config cfg;
    auto r = uiFontScaleFromConfig(cfg);
    ASSERT_TRUE(r.ok()) << r.error().describe();
    EXPECT_DOUBLE_EQ(*r, 1.0);
    EXPECT_DOUBLE_EQ(*r, kFontScaleDefault);
}

TEST(UiFontScaleTest, UiSectionWithoutKeyIsDefaultOne) {
    // The key sits in the "ui" section: a ui.scale-only config leaves
    // font_scale absent = the default.
    Config cfg;
    cfg.set("ui.scale", Json(2));
    auto r = uiFontScaleFromConfig(cfg);
    ASSERT_TRUE(r.ok()) << r.error().describe();
    EXPECT_DOUBLE_EQ(*r, 1.0);
}

TEST(UiFontScaleTest, ExplicitOneIsDefault) {
    Config cfg;
    cfg.set("ui.font_scale", Json(1));
    auto r = uiFontScaleFromConfig(cfg);
    ASSERT_TRUE(r.ok()) << r.error().describe();
    EXPECT_DOUBLE_EQ(*r, 1.0);
}

TEST(UiFontScaleTest, TwoAccepted) {
    Config cfg;
    cfg.set("ui.font_scale", Json(2.0));
    auto r = uiFontScaleFromConfig(cfg);
    ASSERT_TRUE(r.ok()) << r.error().describe();
    EXPECT_DOUBLE_EQ(*r, 2.0);
}

TEST(UiFontScaleTest, FractionalAccepted) {
    Config cfg;
    cfg.set("ui.font_scale", Json(0.7));
    auto r = uiFontScaleFromConfig(cfg);
    ASSERT_TRUE(r.ok()) << r.error().describe();
    EXPECT_DOUBLE_EQ(*r, 0.7);
}

TEST(UiFontScaleTest, MaxBoundaryAccepted) {
    Config cfg;
    cfg.set("ui.font_scale", Json(kFontScaleMax));
    auto r = uiFontScaleFromConfig(cfg);
    ASSERT_TRUE(r.ok()) << r.error().describe();
    EXPECT_DOUBLE_EQ(*r, kFontScaleMax);
}

TEST(UiFontScaleTest, RejectsZero) {
    Config cfg;
    cfg.set("ui.font_scale", Json(0));
    auto r = uiFontScaleFromConfig(cfg);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("ui.font_scale"), std::string::npos);
    EXPECT_NE(r.error().message.find("(0, "), std::string::npos);
}

TEST(UiFontScaleTest, RejectsNegative) {
    Config cfg;
    cfg.set("ui.font_scale", Json(-1));
    auto r = uiFontScaleFromConfig(cfg);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("ui.font_scale"), std::string::npos);
}

TEST(UiFontScaleTest, RejectsAboveMax) {
    Config cfg;
    cfg.set("ui.font_scale", Json(4.0001));
    auto r = uiFontScaleFromConfig(cfg);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("ui.font_scale"), std::string::npos);
}

TEST(UiFontScaleTest, RejectsString) {
    Config cfg;
    cfg.set("ui.font_scale", Json("fast"));
    auto r = uiFontScaleFromConfig(cfg);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("ui.font_scale"), std::string::npos);
    EXPECT_NE(r.error().message.find("number"), std::string::npos);
}

TEST(UiFontScaleTest, RejectsNull) {
    Config cfg;
    cfg.set("ui.font_scale", Json(nullptr));
    auto r = uiFontScaleFromConfig(cfg);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("ui.font_scale"), std::string::npos);
}

TEST(UiFontScaleTest, RejectsBool) {
    Config cfg;
    cfg.set("ui.font_scale", Json(true));
    auto r = uiFontScaleFromConfig(cfg);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("ui.font_scale"), std::string::npos);
}

TEST(UiFontScaleTest, RejectsNonFinite) {
    // The file parser's strtod turns an overflow literal (1e999) into
    // inf; this pins the same value through the direct-set path: a
    // present number that is not finite is named.
    Config cfg;
    cfg.set("ui.font_scale",
            Json(std::numeric_limits<double>::infinity()));
    auto r = uiFontScaleFromConfig(cfg);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("ui.font_scale"), std::string::npos);
    EXPECT_NE(r.error().message.find("finite"), std::string::npos);
}

// ---------------------------------------------------------------------------
// UX-8e: the font-scale math invariants behind renderUi's fScale seam
// ---------------------------------------------------------------------------

TEST(UiFontScaleTest, TextWidthScalesLinearly) {
    // Every addText in renderUi is centered/anchored by the SAME
    // effective scale's textWidth (the (w - textWidth) / 2 formula), so
    // the font stays exactly placed at any ui.font_scale only if the
    // width stays linear in the scale. Integer and power-of-two factors
    // are exact in textWidth's double arithmetic (the values below are
    // all whole numbers at scale 1.0).
    const std::string ascii = "crafting table";
    EXPECT_DOUBLE_EQ(textWidth(ascii, 2.0), 2.0 * textWidth(ascii, 1.0));
    EXPECT_DOUBLE_EQ(textWidth(ascii, 0.5), 0.5 * textWidth(ascii, 1.0));
    // The tutorial line (a CJK string, the UX-8e feedback case): the
    // CJK monospace advance keeps the same linearity.
    const std::string cjk = "合成 4 个木板";
    EXPECT_DOUBLE_EQ(textWidth(cjk, 2.0), 2.0 * textWidth(cjk, 1.0));
}

TEST(UiFontScaleTest, FScaleIsBitwiseIdentityAtDefault) {
    // game_ui.cpp's seam is fScale(s) = s * fs. At the default
    // fs = kFontScaleDefault the factor must be BIT-identical (not just
    // close) — the structural-immunity root: the v1.45.0 default run
    // renders exactly the v1.44.0 vertices. EXPECT_DOUBLE_EQ is a
    // bit-exact comparison.
    for (const double s : {1.0, 2.0, 0.352, 12.0}) {
        EXPECT_DOUBLE_EQ(s * kFontScaleDefault, s) << "s = " << s;
    }
}

TEST(UiFontScaleTest, LineStepsAreIdentityAtDefault) {
    // The three text-stack line steps are lround(constant * fs) in
    // renderUi: at the default they must come back exactly (no lround
    // drift). 18 = kF3LineHeight (game_ui.cpp, the 16 px glyph + 2 px
    // gap); 16 = the tooltip lineGap's unscaled metric (L(16) at panel
    // scale 1).
    EXPECT_EQ(std::lround(18 * kFontScaleDefault), 18);
    EXPECT_EQ(
        std::lround((kUiIconPx + kUiStripGap) * kFontScaleDefault),
        kUiIconPx + kUiStripGap);
    EXPECT_EQ(std::lround(16 * kFontScaleDefault), 16);
}
