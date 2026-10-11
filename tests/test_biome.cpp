#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include <unistd.h>

#include "core/asset_manager.hpp"
#include "world/biome.hpp"

using namespace mc;

namespace {

Json parseJson(const std::string& text) {
    auto r = Json::parse(text);
    if (!r.ok()) {
        ADD_FAILURE() << "parse failed: " << r.error().message;
        return Json::object();
    }
    return *r;
}

} // namespace

TEST(BiomeTest, ParseFullDefinition) {
    const std::string def =
        R"({"id":"plains","weight":4,"surface":"grass","subsurface":"dirt",)"
        R"("subsurface_depth":3,"tree_density":0.05,"height_scale":1.2})";
    BiomeRegistry reg;
    auto r = reg.registerBiome(parseJson(def));
    ASSERT_TRUE(r.ok()) << r.error().message;
    ASSERT_EQ(reg.count(), 1u);
    const Biome& b = reg.all().front();
    EXPECT_EQ(b.name, "plains");
    EXPECT_EQ(b.weight, 4);
    EXPECT_EQ(b.surface, "grass");
    EXPECT_EQ(b.subsurface, "dirt");
    EXPECT_EQ(b.subsurfaceDepth, 3);
    EXPECT_FLOAT_EQ(b.treeDensity, 0.05f);
    EXPECT_FLOAT_EQ(b.heightScale, 1.2f);
    EXPECT_EQ(reg.totalWeight(), 4);
    EXPECT_NE(reg.findByName("plains"), nullptr);
    EXPECT_EQ(reg.findByName("nope"), nullptr);
}

TEST(BiomeTest, DefaultsApplied) {
    BiomeRegistry reg;
    auto r = reg.registerBiome(
        parseJson(R"({"id":"x","surface":"sand","subsurface":"sand"})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const Biome& b = reg.all().front();
    EXPECT_EQ(b.weight, 1);
    EXPECT_EQ(b.subsurfaceDepth, 3);
    EXPECT_FLOAT_EQ(b.treeDensity, 0.0f);
    EXPECT_FLOAT_EQ(b.heightScale, 1.0f);
}

TEST(BiomeTest, LoadOrderIsLoadOrder) {
    BiomeRegistry reg;
    ASSERT_TRUE(reg
                    .registerBiome(parseJson(
                        R"({"id":"b","surface":"s","subsurface":"s"})"))
                    .ok());
    ASSERT_TRUE(reg
                    .registerBiome(parseJson(
                        R"({"id":"a","surface":"s","subsurface":"s"})"))
                    .ok());
    // Cumulative-weight selection walks this order; it must be stable.
    ASSERT_EQ(reg.all().size(), 2u);
    EXPECT_EQ(reg.all()[0].name, "b");
    EXPECT_EQ(reg.all()[1].name, "a");
}

TEST(BiomeTest, RejectsDuplicateId) {
    BiomeRegistry reg;
    ASSERT_TRUE(reg
                    .registerBiome(parseJson(
                        R"({"id":"a","surface":"s","subsurface":"s"})"))
                    .ok());
    auto r = reg.registerBiome(
        parseJson(R"({"id":"a","surface":"s","subsurface":"s"})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("duplicate"), std::string::npos);
}

TEST(BiomeTest, RejectsMissingId) {
    BiomeRegistry reg;
    auto r = reg.registerBiome(
        parseJson(R"({"surface":"s","subsurface":"s"})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("id"), std::string::npos);
}

TEST(BiomeTest, RejectsWrongType) {
    BiomeRegistry reg;
    auto r = reg.registerBiome(parseJson(
        R"({"id":"a","weight":"heavy","surface":"s","subsurface":"s"})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("weight"), std::string::npos);
}

TEST(BiomeTest, RejectsWeightBelowOne) {
    BiomeRegistry reg;
    auto r = reg.registerBiome(parseJson(
        R"({"id":"a","weight":0,"surface":"s","subsurface":"s"})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("weight"), std::string::npos);
}

TEST(BiomeTest, RejectsEmptyBlockNames) {
    BiomeRegistry reg;
    auto r = reg.registerBiome(
        parseJson(R"({"id":"a","surface":"","subsurface":"s"})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("surface"), std::string::npos);
}

TEST(BiomeTest, RejectsSubsurfaceDepthOutOfRange) {
    BiomeRegistry reg;
    auto r = reg.registerBiome(parseJson(
        R"({"id":"a","surface":"s","subsurface":"s","subsurface_depth":0})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("subsurface_depth"), std::string::npos);
}

TEST(BiomeTest, RejectsTreeDensityOutOfRange) {
    BiomeRegistry reg;
    auto r = reg.registerBiome(parseJson(
        R"({"id":"a","surface":"s","subsurface":"s","tree_density":1.5})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("tree_density"), std::string::npos);
}

TEST(BiomeTest, RejectsHeightScaleNonPositive) {
    BiomeRegistry reg;
    auto r = reg.registerBiome(parseJson(
        R"({"id":"a","surface":"s","subsurface":"s","height_scale":0})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("height_scale"), std::string::npos);
}

TEST(BiomeTest, RejectsNonObject) {
    BiomeRegistry reg;
    auto r = reg.registerBiome(parseJson(R"(["not","an","object"])"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("object"), std::string::npos);
}

TEST(BiomeTest, LoadDirectory) {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        ("mc_biome_test_" + std::to_string(::getpid()));
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "data" / "biomes");
    {
        std::ofstream f(root / "data/biomes/beta.json");
        f << R"({"id":"beta","weight":2,"surface":"s","subsurface":"s"})";
    }
    {
        std::ofstream f(root / "data/biomes/alpha.json");
        f << R"({"id":"alpha","weight":3,"surface":"s","subsurface":"s"})";
    }
    {
        std::ofstream f(root / "data/biomes/ignore.txt");
        f << "not a biome";
    }

    AssetManager assets;
    ASSERT_TRUE(assets.setRoot(root).ok());
    BiomeRegistry reg;
    auto r = reg.loadDirectory(assets, "data/biomes");
    ASSERT_TRUE(r.ok()) << r.error().message;
    // Sorted file order: alpha first.
    ASSERT_EQ(reg.count(), 2u);
    EXPECT_EQ(reg.all()[0].name, "alpha");
    EXPECT_EQ(reg.all()[1].name, "beta");
    EXPECT_EQ(reg.totalWeight(), 5);
    std::filesystem::remove_all(root);
}

TEST(BiomeTest, LoadDirectoryRejectsInvalidJson) {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        ("mc_biome_bad_" + std::to_string(::getpid()));
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "data" / "biomes");
    {
        std::ofstream f(root / "data/biomes/bad.json");
        f << "{ this is not json";
    }

    AssetManager assets;
    ASSERT_TRUE(assets.setRoot(root).ok());
    BiomeRegistry reg;
    auto r = reg.loadDirectory(assets, "data/biomes");
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("bad.json"), std::string::npos);
    std::filesystem::remove_all(root);
}

TEST(BiomeTest, LoadDirectoryMissingDirIsEmpty) {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        ("mc_biome_empty_" + std::to_string(::getpid()));
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    AssetManager assets;
    ASSERT_TRUE(assets.setRoot(root).ok());
    BiomeRegistry reg;
    auto r = reg.loadDirectory(assets, "data/biomes");
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(reg.count(), 0u);
    std::filesystem::remove_all(root);
}

// -- surface decorations (Phase 45) ------------------------------------------

TEST(BiomeTest, ParsesDecorations) {
    const std::string def =
        R"J({"id":"plains","surface":"grass","subsurface":"dirt",)J"
        R"J("decorations":[)J"
        R"J({"block":"allium","probability":0.02},)J"
        R"J({"block":"cactus","probability":0.3}]})J";
    BiomeRegistry reg;
    auto r = reg.registerBiome(parseJson(def));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const Biome& b = reg.all().front();
    ASSERT_EQ(b.decorations.size(), 2u);
    EXPECT_EQ(b.decorations[0].block, "allium");
    EXPECT_FLOAT_EQ(b.decorations[0].probability, 0.02f);
    EXPECT_EQ(b.decorations[1].block, "cactus");
    EXPECT_FLOAT_EQ(b.decorations[1].probability, 0.3f);
}

TEST(BiomeTest, DecorationsDefaultToEmpty) {
    BiomeRegistry reg;
    auto r = reg.registerBiome(
        parseJson(R"({"id":"x","surface":"sand","subsurface":"sand"})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_TRUE(reg.all().front().decorations.empty());
}

TEST(BiomeTest, RejectsDecorationNotAnArray) {
    BiomeRegistry reg;
    auto r = reg.registerBiome(parseJson(
        R"({"id":"x","surface":"s","subsurface":"s",)"
        R"("decorations":{"block":"c"}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("array"), std::string::npos);
}

TEST(BiomeTest, RejectsDecorationUnknownKey) {
    BiomeRegistry reg;
    auto r = reg.registerBiome(parseJson(
        R"({"id":"x","surface":"s","subsurface":"s",)"
        R"("decorations":[{"block":"c","probability":0.1,"extra":1}]})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("extra"), std::string::npos);
}

TEST(BiomeTest, RejectsDecorationMissingBlock) {
    BiomeRegistry reg;
    auto r = reg.registerBiome(parseJson(
        R"({"id":"x","surface":"s","subsurface":"s",)"
        R"("decorations":[{"probability":0.1}]})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("block"), std::string::npos);
}

TEST(BiomeTest, RejectsDecorationBadProbability) {
    BiomeRegistry reg;
    auto r = reg.registerBiome(parseJson(
        R"({"id":"x","surface":"s","subsurface":"s",)"
        R"("decorations":[{"block":"c","probability":1.5}]})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("probability"), std::string::npos);
}
