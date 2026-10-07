// ui.scale (Phase 34): the screen-space UI zoom key. Missing = 1.0 (the
// pixel-exact baseline); present must be a JSON number in [1, 4].

#include <gtest/gtest.h>

#include <string>

#include "core/config.hpp"
#include "ui/ui_scale.hpp"

using mc::Config;
using mc::Json;
using mc::kUiScaleMax;
using mc::kUiScaleMin;
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
