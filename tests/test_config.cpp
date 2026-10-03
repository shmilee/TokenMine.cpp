#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "core/config.hpp"
#include "core/json.hpp"

using mc::Config;
using mc::Json;

namespace {

class ConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_dir = std::filesystem::temp_directory_path() /
                ("mc_config_test_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()));
        std::filesystem::create_directories(m_dir);
        m_path = m_dir / "test.json";
    }
    void TearDown() override { std::filesystem::remove_all(m_dir); }

    void writeConfig(const std::string& content) {
        std::ofstream out(m_path);
        out << content;
    }

    std::filesystem::path m_dir;
    std::filesystem::path m_path;
};

} // namespace

TEST_F(ConfigTest, LoadsValidJson) {
    writeConfig(R"({ "window": { "width": 800 }, "vsync": true })");
    auto r = Config::load(m_path);
    ASSERT_TRUE(r.ok()) << r.error().describe();
    EXPECT_EQ(r->getInt("window.width", 0), 800);
    EXPECT_EQ(r->getBool("vsync", false), true);
}

TEST_F(ConfigTest, MissingKeysFallBackToDefaults) {
    writeConfig(R"({})");
    auto r = Config::load(m_path);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(r->getInt("window.width", 1280), 1280);
    EXPECT_EQ(r->getString("window.title", "MineCraft"), "MineCraft");
    EXPECT_EQ(r->getDouble("audio.volume", 1.0), 1.0);
    EXPECT_EQ(r->getBool("window.fullscreen", true), true);
}

TEST_F(ConfigTest, MissingNestedComponentFallsBack) {
    writeConfig(R"({ "window": { "width": 800 } })");
    auto r = Config::load(m_path);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(r->getInt("window.missing.deeper", -1), -1);
    EXPECT_EQ(r->getInt("top.level", -2), -2);
}

TEST_F(ConfigTest, WrongTypeFallsBackToDefault) {
    writeConfig(R"({ "width": "not a number" })");
    auto r = Config::load(m_path);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(r->getInt("width", 99), 99);
}

TEST_F(ConfigTest, LoadFailsOnMissingFile) {
    const auto r = Config::load(m_dir / "does_not_exist.json");
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().describe().find("does_not_exist.json"), std::string::npos);
}

TEST_F(ConfigTest, LoadFailsOnInvalidJson) {
    writeConfig("{ not json");
    const auto r = Config::load(m_path);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("invalid JSON"), std::string::npos);
}

TEST_F(ConfigTest, LoadFailsOnNonObjectRoot) {
    writeConfig("[1, 2, 3]");
    const auto r = Config::load(m_path);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("JSON object"), std::string::npos);
}

TEST_F(ConfigTest, SetCreatesNestedPaths) {
    Config cfg;
    cfg.set("world.seed", static_cast<long long>(42));
    cfg.set("render.aa.samples", 4);
    EXPECT_EQ(cfg.getInt64("world.seed", 0), 42);
    EXPECT_EQ(cfg.getInt("render.aa.samples", 0), 4);
}

TEST_F(ConfigTest, SaveAndReloadRoundTrip) {
    writeConfig(R"({ "a": { "b": 1 } })");
    auto r = Config::load(m_path);
    ASSERT_TRUE(r.ok());
    r->set("c", 3.5);

    const std::filesystem::path outPath = m_dir / "out.json";
    auto s = r->save(outPath);
    ASSERT_TRUE(s.ok()) << s.error().describe();

    auto r2 = Config::load(outPath);
    ASSERT_TRUE(r2.ok());
    EXPECT_EQ(r2->at("a")->find("b")->asInt(), 1);
    EXPECT_DOUBLE_EQ(r2->getDouble("c", 0.0), 3.5);
}

TEST_F(ConfigTest, DefaultConfigIsValidJson) {
    const auto parsed = Json::parse(Config::defaultConfigJson());
    ASSERT_TRUE(parsed.ok());
    ASSERT_TRUE(parsed->isObject());
    EXPECT_EQ(parsed->find("window")->find("width")->asInt(), 1280);
    EXPECT_EQ(parsed->find("window")->find("title")->asString(), "MineCraft");
    EXPECT_TRUE(parsed->find("render")->contains("vsync"));
    EXPECT_TRUE(parsed->find("world")->contains("seed"));
    EXPECT_TRUE(parsed->find("world")->contains("view_distance"));
    EXPECT_TRUE(parsed->find("audio")->contains("volume"));
}
