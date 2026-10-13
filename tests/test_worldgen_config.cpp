#include <gtest/gtest.h>

#include <string>

#include "core/asset_manager.hpp"
#include "world/worldgen_config.hpp"

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

TEST(WorldgenConfigTest, EmptyObjectYieldsDefaults) {
    auto r = WorldgenParams::parse(parseJson(R"({})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const WorldgenParams& p = *r;
    EXPECT_EQ(p.baseHeight, 40);
    EXPECT_EQ(p.heightAmplitude, 24);
    EXPECT_EQ(p.seaLevel, 34);
    EXPECT_FLOAT_EQ(p.heightNoiseScale, 0.02f);
    EXPECT_EQ(p.heightOctaves, 4);
    EXPECT_EQ(p.bottomBlock, "stone");
    EXPECT_EQ(p.deepBlock, "stone");
    EXPECT_FLOAT_EQ(p.biomeNoiseScale, 0.02f);
    EXPECT_TRUE(p.cavesEnabled);
    EXPECT_FLOAT_EQ(p.caveThreshold, 0.68f);
    EXPECT_EQ(p.caveMinY, 2);
    EXPECT_EQ(p.surfaceGuard, 4);
    EXPECT_EQ(p.oreLattice, 8);
    EXPECT_TRUE(p.ores.empty());
    EXPECT_EQ(p.treeLattice, 4);
    EXPECT_EQ(p.treeTrunk, "wood");
    EXPECT_EQ(p.treeLeaves, "leaves");
    EXPECT_EQ(p.treeMinHeight, 4);
    EXPECT_EQ(p.treeHeightRange, 3);
}

TEST(WorldgenConfigTest, FullOverride) {
    const std::string def =
        R"({)"
        R"("terrain":{"base_height":50,"height_amplitude":10,"sea_level":30,)"
        R"("height_noise_scale":0.02,"height_octaves":3,)"
        R"("bottom_block":"bedrock","deep_block":"stone"},)"
        R"("biomes":{"noise_scale":0.005},)"
        R"("caves":{"enabled":false,"noise_scale":0.05,"threshold":0.7,)"
        R"("min_y":3,"surface_guard":6},)"
        R"("ores":{"lattice":12,"veins":[)"
        R"({"block":"coal_ore","probability":0.5,"vein_size":9,"max_y":80},)"
        R"({"block":"diamond_ore","probability":0.1,"vein_size":5,"max_y":16}]},)"
        R"("trees":{"lattice":6,"trunk":"wood","leaves":"leaves",)"
        R"("min_height":5,"height_range":2}})";
    auto r = WorldgenParams::parse(parseJson(def));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const WorldgenParams& p = *r;
    EXPECT_EQ(p.baseHeight, 50);
    EXPECT_EQ(p.heightAmplitude, 10);
    EXPECT_EQ(p.seaLevel, 30);
    EXPECT_FLOAT_EQ(p.heightNoiseScale, 0.02f);
    EXPECT_EQ(p.heightOctaves, 3);
    EXPECT_EQ(p.bottomBlock, "bedrock");
    EXPECT_FLOAT_EQ(p.biomeNoiseScale, 0.005f);
    EXPECT_FALSE(p.cavesEnabled);
    EXPECT_EQ(p.caveMinY, 3);
    EXPECT_EQ(p.surfaceGuard, 6);
    EXPECT_EQ(p.oreLattice, 12);
    ASSERT_EQ(p.ores.size(), 2u);
    EXPECT_EQ(p.ores[0].block, "coal_ore");
    EXPECT_FLOAT_EQ(p.ores[0].probability, 0.5f);
    EXPECT_EQ(p.ores[0].veinSize, 9);
    EXPECT_EQ(p.ores[0].maxY, 80);
    EXPECT_EQ(p.ores[1].block, "diamond_ore");
    EXPECT_EQ(p.treeLattice, 6);
    EXPECT_EQ(p.treeMinHeight, 5);
    EXPECT_EQ(p.treeHeightRange, 2);
}

TEST(WorldgenConfigTest, PartialSectionKeepsOtherDefaults) {
    auto r = WorldgenParams::parse(parseJson(R"({"caves":{"enabled":false}})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_FALSE(r->cavesEnabled);
    // Unmentioned fields keep their defaults.
    EXPECT_FLOAT_EQ(r->caveThreshold, 0.68f);
    EXPECT_EQ(r->baseHeight, 40);
    EXPECT_TRUE(r->ores.empty());
}

TEST(WorldgenConfigTest, RejectsNonObject) {
    auto r = WorldgenParams::parse(parseJson(R"([1,2,3])"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("object"), std::string::npos);
}

TEST(WorldgenConfigTest, RejectsWrongTypeInSection) {
    auto r = WorldgenParams::parse(
        parseJson(R"({"terrain":{"base_height":"44"}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("terrain.base_height"), std::string::npos);
}

TEST(WorldgenConfigTest, RejectsSectionWrongType) {
    auto r = WorldgenParams::parse(parseJson(R"({"caves":[true]})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("caves"), std::string::npos);
}

TEST(WorldgenConfigTest, RejectsVeinsNotArray) {
    auto r = WorldgenParams::parse(
        parseJson(R"({"ores":{"veins":{"block":"coal_ore"}}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("ores.veins"), std::string::npos);
}

TEST(WorldgenConfigTest, RejectsVeinMissingBlock) {
    auto r = WorldgenParams::parse(parseJson(
        R"({"ores":{"veins":[{"probability":0.5,"vein_size":8,"max_y":80}]}})"));
    ASSERT_FALSE(r.ok());
    // Absent 'block' keeps the default "", which the non-empty check rejects.
    EXPECT_NE(r.error().message.find("block"), std::string::npos);
}

TEST(WorldgenConfigTest, RejectsVeinProbabilityOutOfRange) {
    auto r = WorldgenParams::parse(parseJson(
        R"({"ores":{"veins":[{"block":"coal_ore","probability":1.5,)"
        R"("vein_size":8,"max_y":80}]}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("probability"), std::string::npos);
}

TEST(WorldgenConfigTest, RejectsVeinSizeOutOfRange) {
    auto r = WorldgenParams::parse(parseJson(
        R"({"ores":{"veins":[{"block":"coal_ore","probability":0.5,)"
        R"("vein_size":0,"max_y":80}]}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("vein_size"), std::string::npos);
}

TEST(WorldgenConfigTest, RejectsMaxYOutOfRange) {
    auto r = WorldgenParams::parse(parseJson(
        R"({"ores":{"veins":[{"block":"coal_ore","probability":0.5,)"
        R"("vein_size":8,"max_y":0}]}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("max_y"), std::string::npos);
}

TEST(WorldgenConfigTest, RejectsInvalidThreshold) {
    auto r = WorldgenParams::parse(
        parseJson(R"({"caves":{"threshold":1.0}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("threshold"), std::string::npos);
}

TEST(WorldgenConfigTest, RejectsEmptyBottomBlock) {
    auto r = WorldgenParams::parse(
        parseJson(R"({"terrain":{"bottom_block":""}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("bottom"), std::string::npos);
}

TEST(WorldgenConfigTest, RejectsNegativeAmplitude) {
    auto r = WorldgenParams::parse(
        parseJson(R"({"terrain":{"height_amplitude":-5}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("height_amplitude"), std::string::npos);
}

TEST(WorldgenConfigTest, RejectsOctavesOutOfRange) {
    auto r = WorldgenParams::parse(
        parseJson(R"({"terrain":{"height_octaves":9}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("height_octaves"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Phase 59 content pin: the ACTUAL worldgen.json must include the gravel
// vein (a stone-replacing gravel patch, the Minecraft gravel source). The
// values are the content contract; a typo'd probability/size parses fine,
// so this is the unit-level guard.
// ---------------------------------------------------------------------------

TEST(WorldgenConfigTest, Phase59GravelVeinContent) {
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    auto r = WorldgenParams::load(assets);
    ASSERT_TRUE(r.ok()) << r.error().message;
    const WorldgenParams& p = *r;
    const OreConfig* gravel = nullptr;
    for (const OreConfig& o : p.ores) {
        if (o.block == "gravel") {
            gravel = &o;
        }
    }
    ASSERT_TRUE(gravel != nullptr) << "no gravel vein in worldgen.json";
    EXPECT_FLOAT_EQ(gravel->probability, 0.25f);
    EXPECT_EQ(gravel->veinSize, 4);
    EXPECT_EQ(gravel->maxY, 80);
}
