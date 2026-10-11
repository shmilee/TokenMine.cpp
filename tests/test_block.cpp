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

TEST(BlockRegistryTest, FluidBlockLookup) {
    BlockRegistry reg;
    // No fluid block yet (air is not one).
    EXPECT_FALSE(reg.fluidBlock().has_value());
    ASSERT_TRUE(reg.registerBlock(parseDef(minimalDef("stone"))).ok());
    EXPECT_FALSE(reg.fluidBlock().has_value());
    const std::string waterDef =
        R"({"id":"water","properties":{"fluid":true},"model":{"textures":{"top":)"
        R"("t","side":"t","bottom":"t"}}})";
    auto r = reg.registerBlock(parseDef(waterDef));
    ASSERT_TRUE(r.ok()) << r.error().message;
    ASSERT_TRUE(reg.fluidBlock().has_value());
    EXPECT_EQ(*reg.fluidBlock(), *r);
    // The first fluid block wins (the game defines exactly one).
    const std::string lavaDef =
        R"({"id":"lava","properties":{"fluid":true},"model":{"textures":{"top":)"
        R"("t","side":"t","bottom":"t"}}})";
    ASSERT_TRUE(reg.registerBlock(parseDef(lavaDef)).ok());
    EXPECT_EQ(*reg.fluidBlock(), *r);
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

TEST(BlockRegistryTest, AirIsReservedSentinel) {
    // BlockID{0} is the built-in "no block" sentinel: present in every
    // registry, not data-driven, never assignable to a data block.
    BlockRegistry reg;
    EXPECT_EQ(reg.count(), 1u);
    EXPECT_EQ(reg.idForName("air"), BlockID::air());
    EXPECT_EQ(BlockID::air(), BlockID{0});
    const Block& air = reg.at(BlockID::air());
    EXPECT_FALSE(air.properties.solid);
    EXPECT_FALSE(air.properties.opaque);
    EXPECT_FALSE(air.properties.transparent);
    EXPECT_FALSE(air.properties.destroyable);

    // First data block gets id 1, never 0.
    auto r = reg.registerBlock(parseDef(minimalDef("stone")));
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(*r, BlockID{1});

    // A JSON block cannot claim the "air" id.
    auto dup = reg.registerBlock(parseDef(minimalDef("air")));
    ASSERT_FALSE(dup.ok());
    EXPECT_NE(dup.error().message.find("duplicate"), std::string::npos);

    // Air's empty texture names do not leak into the required set.
    auto textures = reg.requiredTextures();
    ASSERT_EQ(textures.size(), 1u);
    EXPECT_TRUE(textures.count("t"));
}

TEST(BlockRegistryTest, ClearRestoresAirOnly) {
    BlockRegistry reg;
    ASSERT_TRUE(reg.registerBlock(parseDef(minimalDef("stone"))).ok());
    reg.clear();
    EXPECT_EQ(reg.count(), 1u);
    EXPECT_EQ(reg.idForName("air"), BlockID::air());
    EXPECT_FALSE(reg.idForName("stone").valid());
}

TEST(BlockRegistryTest, Lookups) {
    BlockRegistry reg;
    auto a = reg.registerBlock(parseDef(minimalDef("alpha")));
    auto b = reg.registerBlock(parseDef(minimalDef("beta")));
    ASSERT_TRUE(a.ok() && b.ok());

    EXPECT_EQ(reg.count(), 3u); // air + alpha + beta
    EXPECT_EQ(reg.idForName("alpha"), *a);
    EXPECT_FALSE(reg.idForName("nope").valid());
    EXPECT_EQ(reg.findByName("beta"), &reg.at(*b));
    EXPECT_EQ(reg.findByName("nope"), nullptr);
    EXPECT_EQ(reg.find(BlockID{-5}), nullptr);
    EXPECT_EQ(reg.at(*a).name, "alpha");

    auto ids = reg.allIds();
    ASSERT_EQ(ids.size(), 3u);
    EXPECT_EQ(ids[0], BlockID::air());
    EXPECT_EQ(ids[1], *a);
    EXPECT_EQ(ids[2], *b);

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
    EXPECT_EQ(reg.count(), 3u); // air + 2 files
    // Files load in sorted order: alpha gets id 1 (air keeps 0).
    EXPECT_EQ(reg.at(BlockID{0}).name, "air");
    EXPECT_EQ(reg.at(BlockID{1}).name, "alpha");
    EXPECT_EQ(reg.at(BlockID{2}).name, "beta");
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
    EXPECT_EQ(reg.count(), 1u); // only the built-in air sentinel
    std::filesystem::remove_all(root);
}

// -- drops_count (Phase 12) ---------------------------------------------------

TEST(BlockRegistryTest, DropsCountParses) {
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(
        R"({"id":"wood","properties":{"drops":"planks","drops_count":4},)"
        R"("model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(reg.at(*r).properties.drops, "planks");
    EXPECT_EQ(reg.at(*r).properties.dropsCount, 4);
}

TEST(BlockRegistryTest, DropsCountDefaultsToOne) {
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(
        R"({"id":"grass","properties":{"drops":"dirt"},)"
        R"("model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(reg.at(*r).properties.dropsCount, 1);
}

TEST(BlockRegistryTest, RejectsBadDropsCount) {
    const std::string model =
        R"("model":{"textures":{"top":"t","side":"t","bottom":"t"}}})";
    EXPECT_FALSE(BlockRegistry()
                     .registerBlock(parseDef(
                         R"({"id":"a","properties":{"drops":"x","drops_count":0},)"
                         + model))
                     .ok());
    EXPECT_FALSE(BlockRegistry()
                     .registerBlock(parseDef(
                         R"({"id":"a","properties":{"drops":"x","drops_count":65},)"
                         + model))
                     .ok());
    EXPECT_FALSE(BlockRegistry()
                     .registerBlock(parseDef(
                         R"({"id":"a","properties":{"drops":"x","drops_count":"four"},)"
                         + model))
                     .ok());
}

TEST(BlockRegistryTest, RejectsDropsCountWithoutDrops) {
    const std::string model =
        R"("model":{"textures":{"top":"t","side":"t","bottom":"t"}}})";
    EXPECT_FALSE(BlockRegistry()
                     .registerBlock(parseDef(
                         R"({"id":"a","properties":{"drops_count":4},)" + model))
                     .ok());
}

// -- interact (Phase 17) -----------------------------------------------------

TEST(BlockRegistryTest, InteractDefaultsToEmpty) {
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(minimalDef("stone")));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(reg.at(*r).properties.interact, "");
}

TEST(BlockRegistryTest, InteractParsesCraftingTable) {
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(
        R"({"id":"crafting_table","properties":{"interact":"crafting_table"},"model")"
        R"(:{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(reg.at(*r).properties.interact, kInteractCraftingTable);
}

TEST(BlockRegistryTest, InteractParsesFurnace) {
    // Phase 40: the furnace opens the smelting GUI.
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(
        R"({"id":"furnace","properties":{"interact":"furnace"},"model")"
        R"(:{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(reg.at(*r).properties.interact, kInteractFurnace);
}

TEST(BlockRegistryTest, RejectsUnknownInteract) {
    const std::string model =
        R"("model":{"textures":{"top":"t","side":"t","bottom":"t"}}})";
    EXPECT_FALSE(BlockRegistry()
                     .registerBlock(parseDef(
                         R"({"id":"a","properties":{"interact":"fancy"},)" +
                         model))
                     .ok());
    EXPECT_FALSE(BlockRegistry()
                     .registerBlock(parseDef(
                         R"({"id":"a","properties":{"interact":7},)" + model))
                     .ok());
}
