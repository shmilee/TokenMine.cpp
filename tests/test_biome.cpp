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

// -- V2-16: the biome tree object + the spawn-zone exclusion flag --------

namespace {

// The full six-key tree object (the snowy_taiga's shipped values — the
// spruce_sapling tree section's field face).
const char* kFullTree =
    R"("tree":{"trunk":"spruce_trunk","leaves":"spruce_leaves",)"
    R"("min_height":6,"height_range":3,"leaf_radius":2,"shape":"spruce"})";

// A full valid definition carrying the tree object, with an optional
// suffix (the flag under test).
std::string treeDef(const std::string& suffix = "") {
    return std::string(R"({"id":"x","surface":"s","subsurface":"s",)") +
           kFullTree + (suffix.empty() ? "" : ("," + suffix)) + "}";
}

// The six keys, in parse order (the missing-key loop's vocabulary).
const char* const kTreeKeys[6] = {"trunk", "leaves", "min_height",
                                  "height_range", "leaf_radius", "shape"};

} // namespace

TEST(BiomeTest, ParsesTreeObject) {
    BiomeRegistry reg;
    auto r = reg.registerBiome(parseJson(treeDef()));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const Biome& b = reg.all().front();
    ASSERT_TRUE(b.tree.has_value());
    EXPECT_EQ(b.tree->trunk, "spruce_trunk");
    EXPECT_EQ(b.tree->leaves, "spruce_leaves");
    EXPECT_EQ(b.tree->minHeight, 6);
    EXPECT_EQ(b.tree->heightRange, 3);
    EXPECT_EQ(b.tree->leafRadius, 2);
    EXPECT_EQ(b.tree->shape, "spruce");
    EXPECT_FALSE(b.excludeSpawnArea); // the flag's default
}

TEST(BiomeTest, TreeAndFlagDefaultAbsent) {
    BiomeRegistry reg;
    auto r = reg.registerBiome(
        parseJson(R"({"id":"x","surface":"s","subsurface":"s"})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const Biome& b = reg.all().front();
    EXPECT_FALSE(b.tree.has_value());
    EXPECT_FALSE(b.excludeSpawnArea);
}

TEST(BiomeTest, ParsesExcludeSpawnArea) {
    BiomeRegistry reg;
    auto r = reg.registerBiome(parseJson(treeDef(R"("exclude_spawn_area":true)")));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_TRUE(reg.all().front().excludeSpawnArea);
}

TEST(BiomeTest, RejectsTreeNotAnObject) {
    BiomeRegistry reg;
    auto r = reg.registerBiome(parseJson(
        R"({"id":"x","surface":"s","subsurface":"s","tree":"spruce"})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("'tree'"), std::string::npos);
    EXPECT_NE(r.error().message.find("object"), std::string::npos);
}

TEST(BiomeTest, RejectsTreeUnknownKey) {
    BiomeRegistry reg;
    auto r = reg.registerBiome(parseJson(
        R"({"id":"x","surface":"s","subsurface":"s","tree":)"
        R"({"trunk":"t","leaves":"l","min_height":4,"height_range":3,)"
        R"("leaf_radius":2,"shape":"oak","extra":1}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("extra"), std::string::npos);
}

TEST(BiomeTest, RejectsTreeMissingKeyByName) {
    // Every one of the six keys is REQUIRED (a missing one is a no-op
    // typo — named, the sapling tree.<key> discipline).
    for (const char* missing : kTreeKeys) {
        std::string keys;
        for (const char* key : kTreeKeys) {
            if (std::string(key) == missing) {
                continue;
            }
            const bool isString =
                std::string(key) == "trunk" || std::string(key) == "leaves" ||
                std::string(key) == "shape";
            const std::string value = isString ? "\"v\"" : "1";
            keys += std::string("\"") + key + "\":" + value + ",";
        }
        keys.pop_back();
        BiomeRegistry reg;
        auto r = reg.registerBiome(parseJson(
            std::string(R"({"id":"x","surface":"s","subsurface":"s","tree":{)") +
            keys + "}}"));
        ASSERT_FALSE(r.ok()) << "missing '" << missing << "'";
        EXPECT_NE(r.error().message.find(missing), std::string::npos)
            << "the missing key must be named: " << r.error().message;
    }
}

TEST(BiomeTest, RejectsTreeBadShapeByVocabulary) {
    BiomeRegistry reg;
    auto r = reg.registerBiome(parseJson(
        R"({"id":"x","surface":"s","subsurface":"s","tree":)"
        R"({"trunk":"t","leaves":"l","min_height":4,"height_range":3,)"
        R"("leaf_radius":2,"shape":"pine"}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("shape"), std::string::npos);
    EXPECT_NE(r.error().message.find("pine"), std::string::npos);
    // The vocabulary is named in the error (the sapling precedent).
    EXPECT_NE(r.error().message.find("oak"), std::string::npos);
    EXPECT_NE(r.error().message.find("birch"), std::string::npos);
}

TEST(BiomeTest, RejectsTreeShapeWrongType) {
    BiomeRegistry reg;
    auto r = reg.registerBiome(parseJson(
        R"({"id":"x","surface":"s","subsurface":"s","tree":)"
        R"({"trunk":"t","leaves":"l","min_height":4,"height_range":3,)"
        R"("leaf_radius":2,"shape":3}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("shape"), std::string::npos);
}

TEST(BiomeTest, RejectsTreeNonInteger) {
    BiomeRegistry reg;
    auto r = reg.registerBiome(parseJson(
        R"({"id":"x","surface":"s","subsurface":"s","tree":)"
        R"({"trunk":"t","leaves":"l","min_height":4.5,"height_range":3,)"
        R"("leaf_radius":2,"shape":"oak"}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("min_height"), std::string::npos);
    EXPECT_NE(r.error().message.find("integer"), std::string::npos);
}

TEST(BiomeTest, RejectsTreeRangeViolations) {
    struct { const char* key; int value; } bad[] = {
        {"min_height", 0}, {"min_height", 17},
        {"height_range", -1}, {"height_range", 9},
        {"leaf_radius", 0}, {"leaf_radius", 3},
    };
    for (const auto& b : bad) {
        // The six-key object with the one key under test at its bad value
        // (the in-range literals: min_height 4 / height_range 3 /
        // leaf_radius 2).
        const std::string key = b.key;
        std::string def = std::string(R"({"id":"x","surface":"s",)"
                                      R"("subsurface":"s","tree":)") +
            R"({"trunk":"t","leaves":"l","min_height":)" +
            (key == "min_height" ? std::to_string(b.value) : "4") +
            R"(,"height_range":)" +
            (key == "height_range" ? std::to_string(b.value) : "3") +
            R"(,"leaf_radius":)" +
            (key == "leaf_radius" ? std::to_string(b.value) : "2") +
            R"(,"shape":"oak"}})";
        BiomeRegistry reg;
        auto r = reg.registerBiome(parseJson(def));
        ASSERT_FALSE(r.ok()) << b.key << " = " << b.value;
        EXPECT_NE(r.error().message.find(b.key), std::string::npos)
            << r.error().message;
    }
}

TEST(BiomeTest, RejectsTreeEmptyNames) {
    BiomeRegistry reg;
    auto r = reg.registerBiome(parseJson(
        R"({"id":"x","surface":"s","subsurface":"s","tree":)"
        R"({"trunk":"","leaves":"l","min_height":4,"height_range":3,)"
        R"("leaf_radius":2,"shape":"oak"}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("trunk"), std::string::npos);
    reg.clear();
    auto r2 = reg.registerBiome(parseJson(
        R"({"id":"x","surface":"s","subsurface":"s","tree":)"
        R"({"trunk":"t","leaves":"","min_height":4,"height_range":3,)"
        R"("leaf_radius":2,"shape":"oak"}})"));
    ASSERT_FALSE(r2.ok());
    EXPECT_NE(r2.error().message.find("leaves"), std::string::npos);
}

TEST(BiomeTest, RejectsExcludeSpawnAreaNotBool) {
    BiomeRegistry reg;
    auto r = reg.registerBiome(parseJson(
        R"({"id":"x","surface":"s","subsurface":"s","exclude_spawn_area":"true"})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("exclude_spawn_area"), std::string::npos);
    EXPECT_NE(r.error().message.find("boolean"), std::string::npos);
    reg.clear();
    auto r2 = reg.registerBiome(parseJson(
        R"({"id":"x","surface":"s","subsurface":"s","exclude_spawn_area":1})"));
    ASSERT_FALSE(r2.ok());
    EXPECT_NE(r2.error().message.find("exclude_spawn_area"), std::string::npos);
}

// -- the shipped content (run from the repository root) ----------------------

TEST(BiomeTest, ShippedBiomesLoad) {
    // V2-16: the shipped biome set is now FOUR (desert / forest / plains
    // unchanged + the new snowy_taiga). The three legacy files' field
    // pins (the bit-level-invariant red line's data side) + the taiga's
    // full content (the spruce tree object + the spawn-zone exclusion).
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    BiomeRegistry reg;
    ASSERT_TRUE(reg.loadDirectory(assets, "data/biomes").ok());
    ASSERT_EQ(reg.count(), 4u);
    // The alphabetical load order (the cumulative-weight walk's order).
    EXPECT_EQ(reg.all()[0].name, "desert");
    EXPECT_EQ(reg.all()[1].name, "forest");
    EXPECT_EQ(reg.all()[2].name, "plains");
    EXPECT_EQ(reg.all()[3].name, "snowy_taiga");
    EXPECT_EQ(reg.totalWeight(), 12);

    // The three legacy biomes: UNCHANGED (the pre-V2-16 field pins).
    const Biome& d = reg.all()[0];
    EXPECT_EQ(d.weight, 2);
    EXPECT_EQ(d.surface, "sand");
    EXPECT_EQ(d.subsurface, "sand");
    EXPECT_EQ(d.subsurfaceDepth, 4);
    EXPECT_FLOAT_EQ(d.treeDensity, 0.0f);
    EXPECT_FLOAT_EQ(d.heightScale, 0.9f);
    EXPECT_EQ(d.decorations.size(), 1u);
    EXPECT_FALSE(d.tree.has_value());
    EXPECT_FALSE(d.excludeSpawnArea);
    const Biome& f = reg.all()[1];
    EXPECT_EQ(f.weight, 4);
    EXPECT_EQ(f.surface, "grass");
    EXPECT_EQ(f.subsurface, "dirt");
    EXPECT_EQ(f.subsurfaceDepth, 3);
    EXPECT_FLOAT_EQ(f.treeDensity, 0.12f);
    EXPECT_FLOAT_EQ(f.heightScale, 1.1f);
    EXPECT_EQ(f.decorations.size(), 3u);
    EXPECT_FALSE(f.tree.has_value());
    EXPECT_FALSE(f.excludeSpawnArea);
    const Biome& p = reg.all()[2];
    EXPECT_EQ(p.weight, 4);
    EXPECT_EQ(p.surface, "grass");
    EXPECT_EQ(p.subsurface, "dirt");
    EXPECT_EQ(p.subsurfaceDepth, 3);
    EXPECT_FLOAT_EQ(p.treeDensity, 0.03f);
    EXPECT_FLOAT_EQ(p.heightScale, 1.0f);
    EXPECT_EQ(p.decorations.size(), 3u);
    EXPECT_FALSE(p.tree.has_value());
    EXPECT_FALSE(p.excludeSpawnArea);

    // The new biome (the design's shipped values): weight 2 (the desert
    // rare-band precedent), the snow surface, the forest's density /
    // height scale, the spruce tree object (= the spruce_sapling's tree
    // section), the spawn-zone exclusion, and NO decorations (the
    // recorded deviation: the project has no fern / berry blocks).
    const Biome& t = reg.all()[3];
    EXPECT_EQ(t.weight, 2);
    EXPECT_EQ(t.surface, "snow");
    EXPECT_EQ(t.subsurface, "dirt");
    EXPECT_EQ(t.subsurfaceDepth, 3);
    EXPECT_FLOAT_EQ(t.treeDensity, 0.12f);
    EXPECT_FLOAT_EQ(t.heightScale, 1.1f);
    EXPECT_TRUE(t.decorations.empty());
    EXPECT_TRUE(t.excludeSpawnArea);
    ASSERT_TRUE(t.tree.has_value());
    EXPECT_EQ(t.tree->trunk, "spruce_trunk");
    EXPECT_EQ(t.tree->leaves, "spruce_leaves");
    EXPECT_EQ(t.tree->minHeight, 6);
    EXPECT_EQ(t.tree->heightRange, 3);
    EXPECT_EQ(t.tree->leafRadius, 2);
    EXPECT_EQ(t.tree->shape, "spruce");
}
