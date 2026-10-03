#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include <unistd.h>

#include "block/block_registry.hpp"
#include "core/asset_manager.hpp"

using namespace mc;

namespace {

Json parseDef(const std::string& text) {
    auto r = Json::parse(text);
    if (!r.ok()) {
        ADD_FAILURE() << "parse failed: " << r.error().message;
        return Json::object();
    }
    return *r;
}

// A definition that passes validation, parameterized by id/texture name.
std::string minimalDef(const std::string& id) {
    return "{\"id\":\"" + id +
           "\",\"model\":{\"textures\":{\"top\":\"t\",\"side\":\"t\","
           "\"bottom\":\"t\"}}}";
}

} // namespace

TEST(BlockRegistryTest, MinimalDefUsesDefaults) {
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(minimalDef("stone")));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const Block& b = reg.at(*r);
    EXPECT_EQ(b.name, "stone");
    EXPECT_TRUE(b.properties.solid);
    EXPECT_TRUE(b.properties.opaque);
    EXPECT_FALSE(b.properties.transparent);
    EXPECT_TRUE(b.properties.destroyable);
    EXPECT_FALSE(b.properties.fluid);
    EXPECT_FLOAT_EQ(b.properties.hardness, 1.0f);
    EXPECT_EQ(b.properties.lightEmission, 0);
    EXPECT_EQ(b.properties.drops, ""); // empty = drops nothing (explicit)
    EXPECT_EQ(b.model.shape, BlockModel::Shape::FullCube);
    EXPECT_EQ(b.model.textures.top, "t");
}

TEST(BlockRegistryTest, FullDefinition) {
    BlockRegistry reg;
    const std::string def =
        R"({"id":"grass","properties":{"hardness":0.6,"drops":"dirt"},"model")"
        R"(:{"shape":"full_cube","textures":{"top":"grass_top","side":"grass_side")"
        R"(,"bottom":"dirt"}}})";
    auto r = reg.registerBlock(parseDef(def));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const Block& b = reg.at(*r);
    EXPECT_FLOAT_EQ(b.properties.hardness, 0.6f);
    EXPECT_EQ(b.properties.drops, "dirt");
    EXPECT_EQ(b.model.textures.top, "grass_top");
    EXPECT_EQ(b.model.textures.side, "grass_side");
    EXPECT_EQ(b.model.textures.bottom, "dirt");
    auto textures = reg.requiredTextures();
    EXPECT_EQ(textures.size(), 3u);
    EXPECT_TRUE(textures.count("grass_top"));
    EXPECT_TRUE(textures.count("grass_side"));
    EXPECT_TRUE(textures.count("dirt"));
}

TEST(BlockRegistryTest, RejectsDuplicateId) {
    BlockRegistry reg;
    ASSERT_TRUE(reg.registerBlock(parseDef(minimalDef("stone"))).ok());
    auto r = reg.registerBlock(parseDef(minimalDef("stone")));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("duplicate"), std::string::npos);
}

TEST(BlockRegistryTest, RejectsMissingId) {
    BlockRegistry reg;
    auto r = reg.registerBlock(
        parseDef(R"({"model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("id"), std::string::npos);
}

TEST(BlockRegistryTest, RejectsWrongType) {
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(
        R"({"id":"stone","properties":{"hardness":"fast"},"model":{"textures")"
        R"(:{"top":"t","side":"t","bottom":"t"}}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("hardness"), std::string::npos);
}

TEST(BlockRegistryTest, RejectsNegativeHardness) {
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(
        R"({"id":"stone","properties":{"hardness":-1},"model":{"textures")"
        R"(:{"top":"t","side":"t","bottom":"t"}}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("hardness"), std::string::npos);
}

TEST(BlockRegistryTest, RejectsLightEmissionOutOfRange) {
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(
        R"({"id":"stone","properties":{"light_emission":16},"model":{"textures")"
        R"(:{"top":"t","side":"t","bottom":"t"}}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("light_emission"), std::string::npos);
}

TEST(BlockRegistryTest, RejectsOpaqueAndTransparent) {
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(
        R"({"id":"stone","properties":{"opaque":true,"transparent":true},"model")"
        R"(:{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("opaque"), std::string::npos);
}

TEST(BlockRegistryTest, RejectsMissingTextureFace) {
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(
        R"({"id":"stone","model":{"textures":{"top":"t","bottom":"t"}}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("side"), std::string::npos);
}

TEST(BlockRegistryTest, RejectsMissingModel) {
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(R"({"id":"stone"})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("model"), std::string::npos);
}

TEST(BlockRegistryTest, RejectsUnknownShape) {
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(
        R"({"id":"stone","model":{"shape":"stairs","textures":{"top":"t","side":"t",)"
        R"("bottom":"t"}}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("shape"), std::string::npos);
}

TEST(BlockRegistryTest, ToleratesUnknownFields) {
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(
        R"({"id":"stone","experimental":42,"model":{"textures":{"top":"t","side":"t",)"
        R"("bottom":"t"}}})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
}

TEST(BlockRegistryTest, Lookups) {
    BlockRegistry reg;
    auto a = reg.registerBlock(parseDef(minimalDef("alpha")));
    auto b = reg.registerBlock(parseDef(minimalDef("beta")));
    ASSERT_TRUE(a.ok() && b.ok());

    EXPECT_EQ(reg.count(), 2u);
    EXPECT_EQ(reg.idForName("alpha"), *a);
    EXPECT_FALSE(reg.idForName("nope").valid());
    EXPECT_EQ(reg.findByName("beta"), &reg.at(*b));
    EXPECT_EQ(reg.findByName("nope"), nullptr);
    EXPECT_EQ(reg.find(BlockID{-5}), nullptr);
    EXPECT_EQ(reg.at(*a).name, "alpha");

    auto ids = reg.allIds();
    ASSERT_EQ(ids.size(), 2u);
    EXPECT_EQ(ids[0], *a);
    EXPECT_EQ(ids[1], *b);

    // References stay valid across further registrations (deque storage).
    const Block* stable = &reg.at(*a);
    auto c = reg.registerBlock(parseDef(minimalDef("gamma")));
    ASSERT_TRUE(c.ok());
    EXPECT_EQ(stable->name, "alpha");
}

TEST(BlockRegistryTest, LoadDirectory) {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        ("mc_block_test_" + std::to_string(::getpid()));
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "data" / "blocks");
    {
        std::ofstream f(root / "data/blocks/alpha.json");
        f << minimalDef("alpha");
    }
    {
        std::ofstream f(root / "data/blocks/beta.json");
        f << minimalDef("beta");
    }
    {
        std::ofstream f(root / "data/blocks/ignore.txt");
        f << "not a block definition";
    }

    AssetManager assets;
    ASSERT_TRUE(assets.setRoot(root).ok());
    BlockRegistry reg;
    auto r = reg.loadDirectory(assets, "data/blocks");
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(reg.count(), 2u);
    // Files load in sorted order: alpha gets id 0.
    EXPECT_EQ(reg.at(BlockID{0}).name, "alpha");
    EXPECT_EQ(reg.at(BlockID{1}).name, "beta");
    std::filesystem::remove_all(root);
}

TEST(BlockRegistryTest, LoadDirectoryRejectsInvalidJson) {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        ("mc_block_bad_" + std::to_string(::getpid()));
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "data/blocks");
    {
        std::ofstream f(root / "data/blocks/bad.json");
        f << "{ this is not json";
    }

    AssetManager assets;
    ASSERT_TRUE(assets.setRoot(root).ok());
    BlockRegistry reg;
    auto r = reg.loadDirectory(assets, "data/blocks");
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("bad.json"), std::string::npos);
    std::filesystem::remove_all(root);
}

TEST(BlockRegistryTest, LoadDirectoryMissingDirIsEmpty) {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        ("mc_block_empty_" + std::to_string(::getpid()));
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    AssetManager assets;
    ASSERT_TRUE(assets.setRoot(root).ok());
    BlockRegistry reg;
    auto r = reg.loadDirectory(assets, "data/blocks");
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(reg.count(), 0u);
    std::filesystem::remove_all(root);
}
