#include <gtest/gtest.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

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
    EXPECT_TRUE(b.properties.fluidName.empty());
    EXPECT_TRUE(b.properties.waterReactionSource.empty());
    EXPECT_TRUE(b.properties.waterReactionFlowing.empty());
    EXPECT_FLOAT_EQ(b.properties.hardness, 1.0f);
    EXPECT_EQ(b.properties.lightEmission, 0);
    EXPECT_EQ(b.properties.drops, ""); // empty = drops nothing (explicit)
    EXPECT_EQ(b.model.shape, BlockModel::Shape::FullCube);
    EXPECT_EQ(b.model.textures.top, "t");
    EXPECT_FALSE(b.properties.beaconBase); // not a beacon base (Phase 83)
    EXPECT_TRUE(b.properties.beaconEffect.empty()); // not a beacon
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

TEST(BlockRegistryTest, FluidBlockLookupByKind) {
    BlockRegistry reg;
    // No fluid blocks yet (air is not one).
    EXPECT_FALSE(reg.fluidBlock("water").has_value());
    EXPECT_FALSE(reg.fluidBlock("lava").has_value());
    ASSERT_TRUE(reg.registerBlock(parseDef(minimalDef("stone"))).ok());
    EXPECT_FALSE(reg.fluidBlock("water").has_value());
    const std::string waterDef =
        R"({"id":"water","properties":{"fluid":"water"},"model":{"textures")"
        R"(:{"top":"t","side":"t","bottom":"t"}}})";
    const std::string lavaDef =
        R"({"id":"lava","properties":{"fluid":"lava",)"
        R"("water_reaction":{"source":"obsidian","flowing":"stone"},)"
        R"("light_emission":15},"model":{"textures":{"top":"t","side":"t",)"
        R"("bottom":"t"}}})";
    auto r = reg.registerBlock(parseDef(waterDef));
    ASSERT_TRUE(r.ok()) << r.error().message;
    ASSERT_TRUE(reg.fluidBlock("water").has_value());
    EXPECT_EQ(*reg.fluidBlock("water"), *r);
    EXPECT_FALSE(reg.fluidBlock("lava").has_value());
    r = reg.registerBlock(parseDef(lavaDef));
    ASSERT_TRUE(r.ok()) << r.error().message;
    ASSERT_TRUE(reg.fluidBlock("lava").has_value());
    EXPECT_EQ(*reg.fluidBlock("lava"), *r);
    const Block& lava = reg.at(*r);
    EXPECT_EQ(lava.properties.fluidName, "lava");
    EXPECT_EQ(lava.properties.waterReactionSource, "obsidian");
    EXPECT_EQ(lava.properties.waterReactionFlowing, "stone");
    EXPECT_EQ(lava.properties.lightEmission, 15);
    // The water kind is untouched by the lava block.
    EXPECT_EQ(*reg.fluidBlock("water"),
              reg.idForName("water"));
}

TEST(BlockRegistryTest, RejectsUnknownFluidName) {
    BlockRegistry reg;
    auto r = reg.registerBlock(
        parseDef(R"({"id":"x","properties":{"fluid":"slime"},)"
                 R"("model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    EXPECT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("unknown fluid name"),
              std::string::npos);
    // A boolean fluid (pre-Phase-51 schema) is a type error.
    r = reg.registerBlock(
        parseDef(R"({"id":"y","properties":{"fluid":true},)"
                 R"("model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    EXPECT_FALSE(r.ok());
}

TEST(BlockRegistryTest, WaterReactionIsStrictlyPaired) {
    // Half a pair (missing "flowing") is rejected.
    auto r = BlockRegistry{}.registerBlock(
        parseDef(R"({"id":"a","properties":{"fluid":"lava","water_reaction":)"
                 R"({"source":"obsidian"}})"
                 R"(,"model":{"textures":{"top":"t","side":"t",)"
                 R"("bottom":"t"}}})"));
    EXPECT_FALSE(r.ok());
    // An empty name in the pair is rejected (ABSENT is the default).
    r = BlockRegistry{}.registerBlock(
        parseDef(R"({"id":"b","properties":{"fluid":"lava","water_reaction":)"
                 R"({"source":"obsidian","flowing":""}})"
                 R"(,"model":{"textures":{"top":"t","side":"t",)"
                 R"("bottom":"t"}}})"));
    EXPECT_FALSE(r.ok());
    // Unknown field inside the object is rejected.
    r = BlockRegistry{}.registerBlock(
        parseDef(R"({"id":"c","properties":{"fluid":"lava","water_reaction":)"
                 R"({"source":"obsidian","flowing":"stone","x":1}})"
                 R"(,"model":{"textures":{"top":"t","side":"t",)"
                 R"("bottom":"t"}}})"));
    EXPECT_FALSE(r.ok());
    // water_reaction requires a fluid block.
    r = BlockRegistry{}.registerBlock(
        parseDef(R"({"id":"d","properties":{"water_reaction":)"
                 R"({"source":"obsidian","flowing":"stone"}})"
                 R"(,"model":{"textures":{"top":"t","side":"t",)"
                 R"("bottom":"t"}}})"));
    EXPECT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("requires the"), std::string::npos);
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
    // Phase 98: full_cube / slab / stairs / cross / wall / fence exist —
    // "pane" is not (a candidate future shape, kept rejected).
    auto r = reg.registerBlock(parseDef(
        R"({"id":"stone","model":{"shape":"pane","textures":{"top":"t","side":"t",)"
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

// -- dye family (Phase 41) ---------------------------------------------------

TEST(BlockRegistryTest, DyeFieldsDefaultToEmpty) {
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(minimalDef("stone")));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_TRUE(reg.at(*r).properties.dyeBase.empty());
    EXPECT_TRUE(reg.at(*r).properties.dyeColor.empty());
}

TEST(BlockRegistryTest, ParsesDyeFamily) {
    // Phase 41: the wool block — a dye family base (white).
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(
        R"({"id":"wool","properties":{"dye_base":"wool","dye_color":"white"},)"
        R"("model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const BlockProperties& p = reg.at(*r).properties;
    EXPECT_EQ(p.dyeBase, "wool");
    EXPECT_EQ(p.dyeColor, "white");
}

TEST(BlockRegistryTest, ParsesDyedVariant) {
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(
        R"({"id":"wool_red","properties":{"dye_base":"wool","dye_color":"red"},)"
        R"("model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const BlockProperties& p = reg.at(*r).properties;
    EXPECT_EQ(p.dyeBase, "wool");
    EXPECT_EQ(p.dyeColor, "red");
}

TEST(BlockRegistryTest, RejectsDyeColorWithoutBase) {
    const std::string model =
        R"("model":{"textures":{"top":"t","side":"t","bottom":"t"}}})";
    EXPECT_FALSE(BlockRegistry()
                     .registerBlock(parseDef(
                         R"({"id":"a","properties":{"dye_color":"red"},)" +
                         model))
                     .ok());
}

TEST(BlockRegistryTest, RejectsEmptyDyeFields) {
    // An explicit empty string is a typo — ABSENT is the "not dyeable"
    // default (the ammo discipline).
    const std::string model =
        R"("model":{"textures":{"top":"t","side":"t","bottom":"t"}}})";
    EXPECT_FALSE(BlockRegistry()
                     .registerBlock(parseDef(
                         R"({"id":"a","properties":{"dye_base":""},)" + model))
                     .ok());
    EXPECT_FALSE(BlockRegistry()
                     .registerBlock(parseDef(
                         R"({"id":"a","properties":{"dye_base":"wool",)"
                         R"("dye_color":""},)" + model))
                     .ok());
    EXPECT_FALSE(BlockRegistry()
                     .registerBlock(parseDef(
                         R"({"id":"a","properties":{"dye_base":7},)" + model))
                     .ok());
}

TEST(BlockRegistryTest, RejectsDyeableWithInteract) {
    // A block either opens a GUI or is dyed — never both.
    const std::string model =
        R"("model":{"textures":{"top":"t","side":"t","bottom":"t"}}})";
    EXPECT_FALSE(BlockRegistry()
                     .registerBlock(parseDef(
                         R"({"id":"a","properties":{"dye_base":"wool",)"
                         R"("interact":"furnace"},)" + model))
                     .ok());
}

TEST(BlockRegistryTest, DyeVariantNameWhiteIsTheBase) {
    // The naming convention: white maps to the base name (no suffix).
    EXPECT_EQ(dyeVariantName("wool", kDyeColorWhite), "wool");
}

TEST(BlockRegistryTest, DyeVariantNameAppendsTheColor) {
    EXPECT_EQ(dyeVariantName("wool", "red"), "wool_red");
    EXPECT_EQ(dyeVariantName("wool", "silver"), "wool_silver");
    EXPECT_EQ(dyeVariantName("carpet", "black"), "carpet_black");
}

// -- cross-plant shape (Phase 45) --------------------------------------------

// A cross plant: non-solid, non-opaque, transparent (the registry enforces
// exactly this combination), one "cross" texture.
const std::string kCrossProps =
    R"({"solid":false,"opaque":false,"transparent":true})";

std::string crossDef(const std::string& id, const std::string& model) {
    return std::string("{\"id\":\"") + id + "\",\"properties\":" + kCrossProps +
           ",\"model\":" + model + "}";
}

TEST(BlockRegistryTest, ParsesCrossShape) {
    BlockRegistry reg;
    auto r = reg.registerBlock(
        parseDef(crossDef(
            "allium",
            R"({"shape":"cross","textures":{"cross":"allium"}})")));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const Block& b = reg.at(*r);
    EXPECT_EQ(b.model.shape, BlockModel::Shape::Cross);
    EXPECT_EQ(b.model.textures.cross, "allium");
    EXPECT_TRUE(b.model.textures.top.empty());
    auto textures = reg.requiredTextures();
    EXPECT_EQ(textures.size(), 1u);
    EXPECT_TRUE(textures.count("allium"));
}

TEST(BlockRegistryTest, CrossShapeIsExplicit) {
    // The default shape stays full_cube; "cross" must be spelled out.
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(minimalDef("stone")));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(reg.at(*r).model.shape, BlockModel::Shape::FullCube);
}

TEST(BlockRegistryTest, RejectsUnsupportedShapeValue) {
    BlockRegistry reg;
    // Phase 98: full_cube / slab / stairs / cross / wall / fence are
    // supported — "pane" is not (a candidate future shape, kept rejected).
    auto r = reg.registerBlock(
        parseDef(crossDef(
            "x", R"({"shape":"pane","textures":{"cross":"x"}})")));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("'model.shape'"), std::string::npos);
}

TEST(BlockRegistryTest, CrossRequiresCrossTexture) {
    BlockRegistry reg;
    auto r = reg.registerBlock(
        parseDef(crossDef("x", R"({"shape":"cross","textures":{}})")));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("model.textures.cross"),
              std::string::npos);
}

TEST(BlockRegistryTest, CrossRejectsCubeFaceKeys) {
    BlockRegistry reg;
    auto r = reg.registerBlock(
        parseDef(crossDef(
            "x", R"({"shape":"cross","textures":{"cross":"x","top":"x"}})")));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("'cross'"), std::string::npos);
}

TEST(BlockRegistryTest, FullCubeRejectsCrossKey) {
    BlockRegistry reg;
    auto r = reg.registerBlock(
        parseDef(R"({"id":"x","model":{"textures":{"top":"x","side":"x",)"
                 R"("bottom":"x","cross":"x"}}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("model.textures.cross"),
              std::string::npos);
}

// A cross shape forces the walk-through blended property combination.
TEST(BlockRegistryTest, CrossRejectsOpaque) {
    BlockRegistry reg;
    auto r = reg.registerBlock(
        parseDef(R"({"id":"x","properties":{"solid":false,"opaque":true,)"
                 R"("transparent":false},"model":{"shape":"cross",)"
                 R"("textures":{"cross":"x"}}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("opaque"), std::string::npos);
}

TEST(BlockRegistryTest, CrossRejectsSolid) {
    BlockRegistry reg;
    auto r = reg.registerBlock(
        parseDef(R"({"id":"x","properties":{"solid":true,"opaque":false,)"
                 R"("transparent":true},"model":{"shape":"cross",)"
                 R"("textures":{"cross":"x"}}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("solid"), std::string::npos);
}

TEST(BlockRegistryTest, CrossRequiresTransparent) {
    BlockRegistry reg;
    auto r = reg.registerBlock(
        parseDef(R"({"id":"x","properties":{"solid":false,"opaque":false,)"
                 R"("transparent":false},"model":{"shape":"cross",)"
                 R"("textures":{"cross":"x"}}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("transparent"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Crop growth fields (Phase 48): grows_to + growth_ticks are a strict pair.
// ---------------------------------------------------------------------------

// A cross-shaped crop def (cross requires solid=false, opaque=false,
// transparent=true) parameterized by id and the growth pair (props may be
// empty — then no trailing comma).
std::string cropDef(const std::string& id, const std::string& props) {
    std::string s = "{\"id\":\"" + id +
                    "\",\"properties\":{\"solid\":false,\"opaque\":false,"
                    "\"transparent\":true";
    if (!props.empty()) {
        s += "," + props;
    }
    s += "},\"model\":{\"shape\":\"cross\",\"textures\":{\"cross\":\"" + id +
         "\"}}}";
    return s;
}

TEST(BlockRegistryTest, ParsesGrowthPair) {
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(
        cropDef("wheat_1", R"("grows_to":"wheat_2","growth_ticks":360)")));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const Block& b = reg.at(*r);
    EXPECT_EQ(b.properties.growsTo, "wheat_2");
    EXPECT_EQ(b.properties.growthTicks, 360);
}

TEST(BlockRegistryTest, MatureBlockHasNoGrowthFields) {
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(cropDef("wheat_7", "")));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const Block& b = reg.at(*r);
    EXPECT_TRUE(b.properties.growsTo.empty());
    EXPECT_EQ(b.properties.growthTicks, 0);
}

TEST(BlockRegistryTest, GrowsToRequiresGrowthTicks) {
    BlockRegistry reg;
    auto r = reg.registerBlock(
        parseDef(cropDef("x", R"("grows_to":"y")")));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("growth_ticks"), std::string::npos);
}

TEST(BlockRegistryTest, GrowthTicksRequiresGrowsTo) {
    BlockRegistry reg;
    auto r = reg.registerBlock(
        parseDef(cropDef("x", R"("growth_ticks":100)")));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("grows_to"), std::string::npos);
}

TEST(BlockRegistryTest, GrowsToRejectsExplicitEmpty) {
    BlockRegistry reg;
    auto r = reg.registerBlock(
        parseDef(cropDef("x", R"("grows_to":"","growth_ticks":100)")));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("non-empty"), std::string::npos);
}

TEST(BlockRegistryTest, GrowsToRejectsSelfReference) {
    BlockRegistry reg;
    auto r = reg.registerBlock(
        parseDef(cropDef("x", R"("grows_to":"x","growth_ticks":100)")));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("itself"), std::string::npos);
}

TEST(BlockRegistryTest, GrowthTicksRejectsOutOfRange) {
    BlockRegistry reg;
    // 0 is out of [1, 1'000'000].
    auto r0 = reg.registerBlock(
        parseDef(cropDef("x", R"("grows_to":"y","growth_ticks":0)")));
    ASSERT_FALSE(r0.ok());
    EXPECT_NE(r0.error().message.find("1000000"), std::string::npos);
    // > 1'000'000 is out of range.
    BlockRegistry reg2;
    auto rBig = reg2.registerBlock(
        parseDef(cropDef("x", R"("grows_to":"y","growth_ticks":1000001)")));
    ASSERT_FALSE(rBig.ok());
    EXPECT_NE(rBig.error().message.find("1000000"), std::string::npos);
}

TEST(BlockRegistryTest, GrowthTicksBoundaryValuesAccepted) {
    BlockRegistry reg;
    auto rLo = reg.registerBlock(
        parseDef(cropDef("a", R"("grows_to":"b","growth_ticks":1)")));
    ASSERT_TRUE(rLo.ok()) << rLo.error().message;
    BlockRegistry reg2;
    auto rHi = reg2.registerBlock(
        parseDef(cropDef("a", R"("grows_to":"b","growth_ticks":1000000)")));
    ASSERT_TRUE(rHi.ok()) << rHi.error().message;
}

// ---------------------------------------------------------------------------
// Chance secondary drop (Phase 49): chance_drop is a strict triple
// (item + count + chance); absent = no chance drop (the default).
// ---------------------------------------------------------------------------

// A default block def parameterized by the full `properties` object text.
std::string propsDef(const std::string& props) {
    return "{\"id\":\"leaves\",\"properties\":" + props +
           ",\"model\":{\"textures\":{\"top\":\"t\",\"side\":\"t\","
           "\"bottom\":\"t\"}}}";
}

TEST(BlockRegistryTest, ParsesChanceDrop) {
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(propsDef(
        R"({"drops":"leaves","chance_drop":{"item":"apple","count":1,
           "chance":0.0078125}})")));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const Block& b = reg.at(*r);
    EXPECT_EQ(b.properties.drops, "leaves");
    EXPECT_EQ(b.properties.chanceDropItem, "apple");
    EXPECT_EQ(b.properties.chanceDropCount, 1);
    EXPECT_DOUBLE_EQ(b.properties.chanceDropChance, 0.0078125);
}

TEST(BlockRegistryTest, ChanceDropAbsentByDefault) {
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(minimalDef("stone")));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const Block& b = reg.at(*r);
    EXPECT_TRUE(b.properties.chanceDropItem.empty());
    EXPECT_EQ(b.properties.chanceDropCount, 0);
    EXPECT_DOUBLE_EQ(b.properties.chanceDropChance, 0.0);
}

TEST(BlockRegistryTest, ChanceDropMustBeObject) {
    BlockRegistry reg;
    auto r = reg.registerBlock(
        parseDef(propsDef(R"({"chance_drop":"apple"})")));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("must be an object"), std::string::npos);
}

TEST(BlockRegistryTest, ChanceDropRejectsUnknownField) {
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(propsDef(
        R"({"chance_drop":{"item":"apple","count":1,"chance":0.5,"foo":1}})")));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("unknown field"), std::string::npos);
}

TEST(BlockRegistryTest, ChanceDropRequiresItem) {
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(
        propsDef(R"({"chance_drop":{"count":1,"chance":0.5}})")));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("non-empty"), std::string::npos);
}

TEST(BlockRegistryTest, ChanceDropRequiresCount) {
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(
        propsDef(R"({"chance_drop":{"item":"apple","chance":0.5}})")));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("[1, 64]"), std::string::npos);
}

TEST(BlockRegistryTest, ChanceDropRequiresChance) {
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(
        propsDef(R"({"chance_drop":{"item":"apple","count":1}})")));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("(0, 1]"), std::string::npos);
}

TEST(BlockRegistryTest, ChanceDropCountOutOfRange) {
    auto badLo = parseDef(
        propsDef(R"({"chance_drop":{"item":"apple","count":0,"chance":0.5}})"));
    BlockRegistry regLo;
    auto rLo = regLo.registerBlock(badLo);
    ASSERT_FALSE(rLo.ok());
    EXPECT_NE(rLo.error().message.find("[1, 64]"), std::string::npos);
    auto badHi = parseDef(
        propsDef(R"({"chance_drop":{"item":"apple","count":65,"chance":0.5}})"));
    BlockRegistry regHi;
    auto rHi = regHi.registerBlock(badHi);
    ASSERT_FALSE(rHi.ok());
    EXPECT_NE(rHi.error().message.find("[1, 64]"), std::string::npos);
}

TEST(BlockRegistryTest, ChanceDropChanceOutOfRange) {
    auto zero = parseDef(
        propsDef(R"({"chance_drop":{"item":"apple","count":1,"chance":0}})"));
    BlockRegistry regZero;
    auto rZero = regZero.registerBlock(zero);
    ASSERT_FALSE(rZero.ok());
    EXPECT_NE(rZero.error().message.find("(0, 1]"), std::string::npos);
    auto big = parseDef(
        propsDef(R"({"chance_drop":{"item":"apple","count":1,"chance":1.5}})"));
    BlockRegistry regBig;
    auto rBig = regBig.registerBlock(big);
    ASSERT_FALSE(rBig.ok());
    EXPECT_NE(rBig.error().message.find("(0, 1]"), std::string::npos);
    // The inclusive upper boundary 1.0 is accepted (a guaranteed drop).
    auto one = parseDef(
        propsDef(R"({"chance_drop":{"item":"apple","count":1,"chance":1.0}})"));
    BlockRegistry regOne;
    auto rOne = regOne.registerBlock(one);
    ASSERT_TRUE(rOne.ok()) << rOne.error().message;
    EXPECT_DOUBLE_EQ(regOne.at(*rOne).properties.chanceDropChance, 1.0);
}

// ---------------------------------------------------------------------------
// Chance drop LIST (Phase 52): chance_drops is an array of the same strict
// triples, mutually exclusive with the single chance_drop.
// ---------------------------------------------------------------------------

TEST(BlockRegistryTest, ParsesChanceDrops) {
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(propsDef(
        R"({"drops":"dirt","chance_drops":[
            {"item":"wheat_1","count":1,"chance":0.125},
            {"item":"apple","count":2,"chance":0.5}]})")));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const Block& b = reg.at(*r);
    EXPECT_EQ(b.properties.drops, "dirt");
    ASSERT_EQ(b.properties.chanceDrops.size(), 2u);
    EXPECT_EQ(b.properties.chanceDrops[0].item, "wheat_1");
    EXPECT_EQ(b.properties.chanceDrops[0].count, 1);
    EXPECT_DOUBLE_EQ(b.properties.chanceDrops[0].chance, 0.125);
    EXPECT_EQ(b.properties.chanceDrops[1].item, "apple");
    EXPECT_EQ(b.properties.chanceDrops[1].count, 2);
    EXPECT_DOUBLE_EQ(b.properties.chanceDrops[1].chance, 0.5);
}

TEST(BlockRegistryTest, ChanceDropsAbsentByDefault) {
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(minimalDef("stone")));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_TRUE(reg.at(*r).properties.chanceDrops.empty());
}

TEST(BlockRegistryTest, ChanceDropsMustBeArray) {
    BlockRegistry reg;
    auto r = reg.registerBlock(
        parseDef(propsDef(R"({"chance_drops":{"item":"apple","count":1,
                           "chance":0.5}})")));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("must be an array"), std::string::npos);
}

TEST(BlockRegistryTest, ChanceDropsRejectsEmptyArray) {
    BlockRegistry reg;
    auto r = reg.registerBlock(
        parseDef(propsDef(R"({"chance_drops":[]})")));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("must not be an empty array"),
              std::string::npos);
}

TEST(BlockRegistryTest, ChanceDropsRejectsNonObjectEntry) {
    BlockRegistry reg;
    auto r = reg.registerBlock(
        parseDef(propsDef(R"({"chance_drops":["apple"]})")));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("chance_drops[0]"), std::string::npos);
}

TEST(BlockRegistryTest, ChanceDropsRejectsUnknownField) {
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(
        propsDef(R"({"chance_drops":[{"item":"apple","count":1,
                                      "chance":0.5,"foo":1}]})")));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("unknown field"), std::string::npos);
}

TEST(BlockRegistryTest, ChanceDropsRequiresItem) {
    BlockRegistry reg;
    auto r = reg.registerBlock(
        parseDef(propsDef(R"({"chance_drops":[{"count":1,"chance":0.5}]})")));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("non-empty"), std::string::npos);
}

TEST(BlockRegistryTest, ChanceDropsRequiresCount) {
    BlockRegistry reg;
    auto r = reg.registerBlock(
        parseDef(propsDef(R"({"chance_drops":[{"item":"apple","chance":0.5}]})")));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("[1, 64]"), std::string::npos);
}

TEST(BlockRegistryTest, ChanceDropsRequiresChance) {
    BlockRegistry reg;
    auto r = reg.registerBlock(
        parseDef(propsDef(R"({"chance_drops":[{"item":"apple","count":1}]})")));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("(0, 1]"), std::string::npos);
}

TEST(BlockRegistryTest, ChanceDropsCountOutOfRange) {
    auto badLo = parseDef(propsDef(
        R"({"chance_drops":[{"item":"apple","count":0,"chance":0.5}]})"));
    BlockRegistry regLo;
    auto rLo = regLo.registerBlock(badLo);
    ASSERT_FALSE(rLo.ok());
    EXPECT_NE(rLo.error().message.find("[1, 64]"), std::string::npos);
    auto badHi = parseDef(propsDef(
        R"({"chance_drops":[{"item":"apple","count":65,"chance":0.5}]})"));
    BlockRegistry regHi;
    auto rHi = regHi.registerBlock(badHi);
    ASSERT_FALSE(rHi.ok());
    EXPECT_NE(rHi.error().message.find("[1, 64]"), std::string::npos);
}

TEST(BlockRegistryTest, ChanceDropsChanceOutOfRange) {
    auto zero = parseDef(propsDef(
        R"({"chance_drops":[{"item":"apple","count":1,"chance":0}]})"));
    BlockRegistry regZero;
    auto rZero = regZero.registerBlock(zero);
    ASSERT_FALSE(rZero.ok());
    EXPECT_NE(rZero.error().message.find("(0, 1]"), std::string::npos);
    auto big = parseDef(propsDef(
        R"({"chance_drops":[{"item":"apple","count":1,"chance":1.5}]})"));
    BlockRegistry regBig;
    auto rBig = regBig.registerBlock(big);
    ASSERT_FALSE(rBig.ok());
    EXPECT_NE(rBig.error().message.find("(0, 1]"), std::string::npos);
}

TEST(BlockRegistryTest, ChanceDropsValidatesEveryEntry) {
    // A bad SECOND entry must be caught too (not just the first).
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(
        propsDef(R"({"chance_drops":[{"item":"apple","count":1,"chance":0.5},
                                      {"item":"","count":1,"chance":0.5}]})")));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("chance_drops[1]"), std::string::npos);
}

TEST(BlockRegistryTest, ChanceDropsMutuallyExclusiveWithChanceDrop) {
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(
        propsDef(R"({"chance_drop":{"item":"apple","count":1,"chance":0.5},
                     "chance_drops":[{"item":"wheat_1","count":1,
                                      "chance":0.125}]})")));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("mutually exclusive"),
              std::string::npos);
}

// ---------------------------------------------------------------------------
// Planting placement restriction (Phase 53): plant_on is an OPTIONAL array
// of block names (the soil the block may be placed on). Absent = no
// restriction; an empty array is rejected (the ammo discipline).
// ---------------------------------------------------------------------------

TEST(BlockRegistryTest, ParsesPlantOn) {
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(propsDef(
        R"({"grows_on":"farmland","plant_on":["farmland","dirt"]})")));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const Block& b = reg.at(*r);
    ASSERT_EQ(b.properties.plantOn.size(), 2u);
    EXPECT_EQ(b.properties.plantOn[0], "farmland");
    EXPECT_EQ(b.properties.plantOn[1], "dirt");
}

TEST(BlockRegistryTest, PlantOnAbsentByDefault) {
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(minimalDef("stone")));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_TRUE(reg.at(*r).properties.plantOn.empty());
}

TEST(BlockRegistryTest, PlantOnMustBeArray) {
    BlockRegistry reg;
    auto r = reg.registerBlock(
        parseDef(propsDef(R"({"plant_on":"farmland"})")));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("must be an array"), std::string::npos);
}

TEST(BlockRegistryTest, PlantOnRejectsEmptyArray) {
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(propsDef(R"({"plant_on":[]})")));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("must not be an empty array"),
              std::string::npos);
}

TEST(BlockRegistryTest, PlantOnRejectsNonStringEntry) {
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(propsDef(R"({"plant_on":[1]})")));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("plant_on[0]"), std::string::npos);
}

TEST(BlockRegistryTest, PlantOnRejectsEmptyStringEntry) {
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(propsDef(R"({"plant_on":[""]})")));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("plant_on[0]"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Crop environment fields (Phase 50): grows_on / growth_light / dry_slowdown
// are optional growth-condition fields (absent = no requirement).
// ---------------------------------------------------------------------------

TEST(BlockRegistryTest, CropEnvironmentDefaultsAbsent) {
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(minimalDef("stone")));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const Block& b = reg.at(*r);
    EXPECT_TRUE(b.properties.growsOn.empty());
    EXPECT_EQ(b.properties.growthLight, 0);
    EXPECT_EQ(b.properties.drySlowdown, 1);
}

TEST(BlockRegistryTest, ParsesCropEnvironment) {
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(cropDef(
        "wheat_1",
        R"("grows_to":"wheat_2","growth_ticks":360,"grows_on":"farmland","growth_light":9,"dry_slowdown":2)")));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const Block& b = reg.at(*r);
    EXPECT_EQ(b.properties.growsOn, "farmland");
    EXPECT_EQ(b.properties.growthLight, 9);
    EXPECT_EQ(b.properties.drySlowdown, 2);
}

TEST(BlockRegistryTest, GrowsOnRejectsExplicitEmpty) {
    BlockRegistry reg;
    auto r = reg.registerBlock(
        parseDef(cropDef("x", R"("grows_on":"")")));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("non-empty"), std::string::npos);
}

TEST(BlockRegistryTest, GrowthLightRange) {
    // 0 and 15 are the inclusive boundaries (0 = no requirement).
    BlockRegistry reg;
    auto rLo = reg.registerBlock(
        parseDef(cropDef("a", R"("growth_light":0)")));
    ASSERT_TRUE(rLo.ok()) << rLo.error().message;
    BlockRegistry reg2;
    auto rHi = reg2.registerBlock(
        parseDef(cropDef("a", R"("growth_light":15)")));
    ASSERT_TRUE(rHi.ok()) << rHi.error().message;
    BlockRegistry reg3;
    auto rNeg = reg3.registerBlock(
        parseDef(cropDef("a", R"("growth_light":-1)")));
    ASSERT_FALSE(rNeg.ok());
    EXPECT_NE(rNeg.error().message.find("[0, 15]"), std::string::npos);
    BlockRegistry reg4;
    auto rBig = reg4.registerBlock(
        parseDef(cropDef("a", R"("growth_light":16)")));
    ASSERT_FALSE(rBig.ok());
    EXPECT_NE(rBig.error().message.find("[0, 15]"), std::string::npos);
    BlockRegistry reg5;
    auto rFloat = reg5.registerBlock(
        parseDef(cropDef("a", R"("growth_light":2.5)")));
    ASSERT_FALSE(rFloat.ok());
    EXPECT_NE(rFloat.error().message.find("integer"), std::string::npos);
}

TEST(BlockRegistryTest, DrySlowdownRange) {
    BlockRegistry reg;
    auto rOne = reg.registerBlock(
        parseDef(cropDef("a", R"("dry_slowdown":1)")));
    ASSERT_TRUE(rOne.ok()) << rOne.error().message;
    BlockRegistry reg2;
    auto rHi = reg2.registerBlock(
        parseDef(cropDef("a", R"("dry_slowdown":64)")));
    ASSERT_TRUE(rHi.ok()) << rHi.error().message;
    BlockRegistry reg3;
    auto rZero = reg3.registerBlock(
        parseDef(cropDef("a", R"("dry_slowdown":0)")));
    ASSERT_FALSE(rZero.ok());
    EXPECT_NE(rZero.error().message.find("[1, 64]"), std::string::npos);
    BlockRegistry reg4;
    auto rBig = reg4.registerBlock(
        parseDef(cropDef("a", R"("dry_slowdown":65)")));
    ASSERT_FALSE(rBig.ok());
    EXPECT_NE(rBig.error().message.find("[1, 64]"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Drop content (Phase 55): multi-drop content expansion. These pin the
// ACTUAL asset files (not a synthetic copy): the expansion values are the
// content contract (Minecraft semantics), and a typo'd chance (0.2 instead
// of 0.02) parses fine, so the registry-level check is the unit-level guard.
// The tests must run from the repository root (the game's resource paths
// are relative, same contract).
// ---------------------------------------------------------------------------

TEST(BlockRegistryTest, Phase55LeavesDropContent) {
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    BlockRegistry reg;
    ASSERT_TRUE(reg.loadDirectory(assets, "data/blocks").ok());
    const Block* b = reg.findByName("leaves");
    ASSERT_TRUE(b != nullptr);
    // The single chance_drop migrated to the list (Phase 55); the field is
    // gone, the list holds the stick entry (Minecraft 1.14+: 2%) in front
    // of the Phase 49 apple entry (1/128, the project value, kept).
    EXPECT_EQ(b->properties.drops, "leaves");
    EXPECT_TRUE(b->properties.chanceDropItem.empty());
    ASSERT_EQ(b->properties.chanceDrops.size(), 2u);
    EXPECT_EQ(b->properties.chanceDrops[0].item, "stick");
    EXPECT_EQ(b->properties.chanceDrops[0].count, 1);
    EXPECT_DOUBLE_EQ(b->properties.chanceDrops[0].chance, 0.02);
    EXPECT_EQ(b->properties.chanceDrops[1].item, "apple");
    EXPECT_EQ(b->properties.chanceDrops[1].count, 1);
    EXPECT_DOUBLE_EQ(b->properties.chanceDrops[1].chance, 0.0078125);
}

TEST(BlockRegistryTest, Phase55LapisOreDropContent) {
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    BlockRegistry reg;
    ASSERT_TRUE(reg.loadDirectory(assets, "data/blocks").ok());
    const Block* b = reg.findByName("lapis_ore");
    ASSERT_TRUE(b != nullptr);
    // 2..4 lapis, uniform: base 2 + two independent 50% rolls = the
    // Minecraft lapis-ore quantity distribution.
    EXPECT_EQ(b->properties.drops, "lapis");
    EXPECT_EQ(b->properties.dropsCount, 2);
    ASSERT_EQ(b->properties.chanceDrops.size(), 2u);
    for (const ChanceDrop& e : b->properties.chanceDrops) {
        EXPECT_EQ(e.item, "lapis");
        EXPECT_EQ(e.count, 1);
        EXPECT_DOUBLE_EQ(e.chance, 0.5);
    }
}

TEST(BlockRegistryTest, Phase55GlowstoneDropContent) {
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    BlockRegistry reg;
    ASSERT_TRUE(reg.loadDirectory(assets, "data/blocks").ok());
    const Block* b = reg.findByName("glowstone");
    ASSERT_TRUE(b != nullptr);
    // 2..4 glowstone: the Minecraft quantity, item = the block item (this
    // project has no glowstone dust).
    EXPECT_EQ(b->properties.drops, "glowstone");
    EXPECT_EQ(b->properties.dropsCount, 2);
    ASSERT_EQ(b->properties.chanceDrops.size(), 2u);
    for (const ChanceDrop& e : b->properties.chanceDrops) {
        EXPECT_EQ(e.item, "glowstone");
        EXPECT_EQ(e.count, 1);
        EXPECT_DOUBLE_EQ(e.chance, 0.5);
    }
}

// ---------------------------------------------------------------------------
// Flamable burn (Phase 58, the fire system v2): the block flag + the wheat
// crop content pin.
// ---------------------------------------------------------------------------

TEST(BlockRegistryTest, FlamableDefaultsFalse) {
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(minimalDef("stone")));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_FALSE(reg.at(reg.idForName("stone")).properties.flamable);
}

TEST(BlockRegistryTest, FlamableParses) {
    BlockRegistry reg;
    auto r = reg.registerBlock(
        parseDef(R"({"id":"wheat_3","properties":{"flamable":true,)"
                 R"("solid":false,"opaque":false,"transparent":true},)"
                 R"("model":{"shape":"cross","textures":{"cross":"w"}}})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_TRUE(reg.at(reg.idForName("wheat_3")).properties.flamable);
}

TEST(BlockRegistryTest, FlamableNonBoolRejected) {
    BlockRegistry reg;
    auto r = reg.registerBlock(
        parseDef(R"({"id":"x","properties":{"flamable":"yes"},)"
                 R"("model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("must be a boolean"), std::string::npos);
}

TEST(BlockRegistryTest, IgnitesToIsAccepted) {
    // Phase 76: the igniter's relight target (a block name).
    BlockRegistry reg;
    auto r = reg.registerBlock(
        parseDef(R"({"id":"unlit","properties":{"ignites_to":"lit"},)"
                 R"("model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(reg.at(*r).properties.ignitesTo, "lit");
    // Absent = the default empty (no relight target).
    auto r2 = reg.registerBlock(
        parseDef(R"({"id":"plain","model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    ASSERT_TRUE(r2.ok()) << r2.error().message;
    EXPECT_TRUE(reg.at(*r2).properties.ignitesTo.empty());
}

TEST(BlockRegistryTest, IgnitesToNonStringRejected) {
    BlockRegistry reg;
    auto r = reg.registerBlock(
        parseDef(R"({"id":"x","properties":{"ignites_to":7},)"
                 R"("model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("string"), std::string::npos)
        << r.error().message;
}

TEST(BlockRegistryTest, IgnitesToEmptyStringRejected) {
    // An EXPLICIT empty ignites_to is a no-op typo (the ammo discipline).
    BlockRegistry reg;
    auto r = reg.registerBlock(
        parseDef(R"({"id":"x","properties":{"ignites_to":""},)"
                 R"("model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("non-empty"), std::string::npos)
        << r.error().message;
}

TEST(BlockRegistryTest, Phase58WheatCropFlamableContent) {
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    BlockRegistry reg;
    ASSERT_TRUE(reg.loadDirectory(assets, "data/blocks").ok());
    // The whole crop line (Minecraft: every wheat stage burns in lava) is
    // flagged, and the flag is NOT sprinkled on the non-flammable blocks.
    // Phase 63 EXPANDED the set with the natural surface flammables
    // (wood/leaves/grass — see Phase63FlamableContent for the exact set);
    // the negative list here keeps the never-flammable blocks.
    for (int i = 1; i <= 7; ++i) {
        const Block* b = reg.findByName("wheat_" + std::to_string(i));
        ASSERT_TRUE(b != nullptr) << "wheat_" << i;
        EXPECT_TRUE(b->properties.flamable) << "wheat_" << i;
    }
    for (const char* name : {"stone", "wool", "sand"}) {
        const Block* b = reg.findByName(name);
        ASSERT_TRUE(b != nullptr) << name;
        EXPECT_FALSE(b->properties.flamable) << name;
    }
}

// ---------------------------------------------------------------------------
// Gravel content (Phase 59): the multi-drop content re-expansion. Pins the
// ACTUAL asset file: gravel drops itself (the block item) plus a 10% chance
// of one flint (the Minecraft value) — a typo'd chance (0.1 vs 0.01) parses
// fine, so the registry-level check is the unit-level guard.
// ---------------------------------------------------------------------------

TEST(BlockRegistryTest, Phase59GravelDropContent) {
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    BlockRegistry reg;
    ASSERT_TRUE(reg.loadDirectory(assets, "data/blocks").ok());
    const Block* b = reg.findByName("gravel");
    ASSERT_TRUE(b != nullptr);
    EXPECT_FLOAT_EQ(b->properties.hardness, 0.6f);  // Minecraft value
    EXPECT_EQ(b->properties.drops, "gravel");       // the block item
    EXPECT_EQ(b->properties.dropsCount, 1);
    // The single-entry chance drop (mutually exclusive with the list).
    EXPECT_EQ(b->properties.chanceDropItem, "flint");
    EXPECT_EQ(b->properties.chanceDropCount, 1);
    EXPECT_DOUBLE_EQ(b->properties.chanceDropChance, 0.1);
    EXPECT_TRUE(b->properties.chanceDrops.empty());
    EXPECT_FALSE(b->properties.flamable);  // gravel does not burn (v1)
}

// Cobble content (Phase 61): the stone→cobblestone material chain. Pins
// the ACTUAL asset files: cobblestone drops itself, and STONE (natural
// stone) now drops cobblestone — the Minecraft material cycle (the stone
// self-drop change is the documented content-contract change of the
// phase). A typo'd drops name parses fine, so the registry-level
// assertion is the unit-level guard.
// ---------------------------------------------------------------------------

TEST(BlockRegistryTest, Phase61CobblestoneContent) {
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    BlockRegistry reg;
    ASSERT_TRUE(reg.loadDirectory(assets, "data/blocks").ok());
    const Block* c = reg.findByName("cobblestone");
    ASSERT_TRUE(c != nullptr);
    EXPECT_FLOAT_EQ(c->properties.hardness, 2.0f);  // Minecraft value
    EXPECT_EQ(c->properties.drops, "cobblestone");  // the block item
    EXPECT_EQ(c->properties.dropsCount, 1);
    EXPECT_FALSE(c->properties.flamable);
    // The contract change: natural stone drops COBBLESTONE (Minecraft:
    // mining stone yields cobble; stone comes back via the 4-cobble
    // recipe, Phase 61).
    const Block* s = reg.findByName("stone");
    ASSERT_TRUE(s != nullptr);
    EXPECT_EQ(s->properties.drops, "cobblestone");
}

// Flamable expansion (Phase 63): the three NATURAL surface flammables
// (wood / leaves / grass) join the wheat stages (Phase 58) as
// `flamable: true` — Minecraft's natural flammables, pure data (both
// consumers, the igniter [Phase 60] and the lava burn pass [Phase 58],
// read the flag). Pins the set EXACTLY: wood/leaves/grass + wheat_1..7,
// nothing else (a stray flamable would silently expand the burn surface).
// ---------------------------------------------------------------------------

TEST(BlockRegistryTest, Phase63FlamableContent) {
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    BlockRegistry reg;
    ASSERT_TRUE(reg.loadDirectory(assets, "data/blocks").ok());
    for (const char* name : {"wood", "leaves", "grass"}) {
        const Block* b = reg.findByName(name);
        ASSERT_TRUE(b != nullptr) << name;
        EXPECT_TRUE(b->properties.flamable) << name;
    }
    // The complete flamable set = 3 + 7 = 10 blocks, nothing else.
    std::vector<std::string> flamables;
    for (std::size_t i = 0; i < reg.count(); ++i) {
        const Block& b = reg.at(BlockID{static_cast<int>(i)});
        if (b.properties.flamable) {
            flamables.push_back(b.name);
        }
    }
    std::sort(flamables.begin(), flamables.end());
    EXPECT_EQ(flamables.size(), 10u);
    const std::vector<std::string> want = {"grass", "leaves",
                                           "wheat_1", "wheat_2", "wheat_3",
                                           "wheat_4", "wheat_5", "wheat_6",
                                           "wheat_7", "wood"};
    EXPECT_EQ(flamables, want);
}

// Fire block (Phase 64, the fire visuals): the burn TARGET of the igniter
// (Phase 60) and the lava burn pass (Phase 58) — a light source that burns
// out after fire.lifetime_seconds and is extinguished by water. Pins the
// ACTUAL asset file: a non-solid, non-opaque transparent cross (the plant
// combination the registry enforces), full light 15 (the Minecraft fire
// value), instant break (hardness 0), NO drops (a burned char drops
// nothing, like the Phase 58 burn) and NOT flamable (fire does not burn —
// it is the end state of the burn).
// ---------------------------------------------------------------------------

TEST(BlockRegistryTest, Phase64FireBlockContent) {
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    BlockRegistry reg;
    ASSERT_TRUE(reg.loadDirectory(assets, "data/blocks").ok());
    const Block* b = reg.findByName("fire");
    ASSERT_TRUE(b != nullptr);
    EXPECT_FALSE(b->properties.solid);
    EXPECT_FALSE(b->properties.opaque);
    EXPECT_TRUE(b->properties.transparent);
    EXPECT_EQ(b->properties.lightEmission, 15);  // the Minecraft fire value
    EXPECT_FLOAT_EQ(b->properties.hardness, 0.0f);  // instant break
    EXPECT_TRUE(b->properties.drops.empty());  // the burn leaves no char
    EXPECT_FALSE(b->properties.flamable);  // fire is the burn's end state
    EXPECT_EQ(b->model.shape, BlockModel::Shape::Cross);
    EXPECT_EQ(b->model.textures.cross, "fire");
}

// Torch (Phase 65, the torch content): a placeable light source — the
// first crafting use of coal (the recipe, pinned in test_recipe). Pins
// the ACTUAL asset file: the plant combination (non-solid, non-opaque,
// transparent), full-minus-one light 14 (the Minecraft torch value),
// instant break (hardness 0), drops ITSELF (the torch is reusable —
// unlike fire, which leaves nothing) and NOT flamable (a torch is a
// tool, not fuel for the burn pass).
// ---------------------------------------------------------------------------

TEST(BlockRegistryTest, Phase65TorchBlockContent) {
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    BlockRegistry reg;
    ASSERT_TRUE(reg.loadDirectory(assets, "data/blocks").ok());
    const Block* b = reg.findByName("torch");
    ASSERT_TRUE(b != nullptr);
    EXPECT_FALSE(b->properties.solid);
    EXPECT_FALSE(b->properties.opaque);
    EXPECT_TRUE(b->properties.transparent);
    EXPECT_EQ(b->properties.lightEmission, 14);  // the Minecraft torch value
    EXPECT_FLOAT_EQ(b->properties.hardness, 0.0f);  // instant break
    EXPECT_EQ(b->properties.drops, "torch");  // the torch drops itself
    EXPECT_FALSE(b->properties.flamable);  // a torch is not fuel
    EXPECT_EQ(b->model.shape, BlockModel::Shape::Cross);
    EXPECT_EQ(b->model.textures.cross, "torch");
}

// ---------------------------------------------------------------------------
// Phase 83 beacon: the two data-driven properties + the shipped content.
// ---------------------------------------------------------------------------

// A block def with a single extra property injected into "properties".
std::string propDef(const std::string& id, const std::string& extra) {
    return "{\"id\":\"" + id + "\",\"properties\":{" + extra +
           "},\"model\":{\"textures\":{\"top\":\"t\",\"side\":\"t\","
           "\"bottom\":\"t\"}}}";
}

TEST(BlockRegistryTest, BeaconBaseIsStrictlyBoolean) {
    // The beacon_base flag is a strict boolean (the flamable discipline).
    EXPECT_TRUE(
        BlockRegistry{}.registerBlock(parseDef(propDef("iron",
                                                       "\"beacon_base\":true")))
            .ok());
    // A non-boolean (the "yes" typo) is rejected and named.
    auto bad = BlockRegistry{}.registerBlock(parseDef(
        propDef("iron", "\"beacon_base\":\"yes\"")));
    ASSERT_FALSE(bad.ok());
    EXPECT_NE(bad.error().message.find("beacon_base"), std::string::npos);
}

TEST(BlockRegistryTest, BeaconBaseFlagIsStored) {
    BlockRegistry reg;
    auto r = reg.registerBlock(
        parseDef(propDef("iron", "\"beacon_base\":true")));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_TRUE(reg.at(*r).properties.beaconBase);
}

TEST(BlockRegistryTest, BeaconEffectIsAString) {
    BlockRegistry reg;
    auto r = reg.registerBlock(
        parseDef(propDef("beacon", "\"beacon_effect\":\"absorption\"")));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(reg.at(*r).properties.beaconEffect, "absorption");
}

TEST(BlockRegistryTest, BeaconEffectExplicitEmptyRejected) {
    // An EXPLICIT empty beacon_effect is a no-op typo (the ammo discipline,
    // the ignites_to pattern); ABSENT is the "not a beacon" default.
    auto bad = BlockRegistry{}.registerBlock(
        parseDef(propDef("beacon", "\"beacon_effect\":\"\"")));
    ASSERT_FALSE(bad.ok());
    EXPECT_NE(bad.error().message.find("beacon_effect"), std::string::npos);
}

TEST(BlockRegistryTest, Phase83BeaconBlockContent) {
    // The shipped beacon: hardness 8 (Minecraft), drops itself, and applies
    // its effect list (Phase 83 "absorption"; Phase 91 ["absorption",
    // "speed"] — the multi-effect "enchanting table", MC primary +
    // secondary).
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    BlockRegistry reg;
    ASSERT_TRUE(reg.loadDirectory(assets, "data/blocks").ok());
    const Block* b = reg.findByName("beacon");
    ASSERT_TRUE(b != nullptr);
    EXPECT_TRUE(b->properties.solid);
    EXPECT_TRUE(b->properties.opaque);
    EXPECT_EQ(b->properties.lightEmission, 0);
    EXPECT_FLOAT_EQ(b->properties.hardness, 8.0f);
    EXPECT_EQ(b->properties.drops, "beacon"); // drops itself
    EXPECT_EQ(b->properties.beaconEffects,
              (std::vector<std::string>{"absorption", "speed"}));
    EXPECT_FALSE(b->properties.beaconBase);
    EXPECT_EQ(b->model.shape, BlockModel::Shape::FullCube);
}

TEST(BlockRegistryTest, Phase91BeaconEffectsListParsed) {
    // The beacon_effects LIST is parsed (a non-empty array of names); the
    // single beacon_effect is the backward-compat 1-element list; an empty
    // array is rejected (the ammo discipline); each entry must be a
    // non-empty string (named).
    BlockRegistry reg;
    // A non-empty list parses into beaconEffects.
    auto r = reg.registerBlock(parseDef(
        R"({"id":"b1","properties":{"beacon_effects":["a","b"]},)"
        R"("model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(reg.at(*r).properties.beaconEffects,
              (std::vector<std::string>{"a", "b"}));
    // Backward compat: the single beacon_effect becomes a 1-element list.
    auto r0 = reg.registerBlock(parseDef(
        R"({"id":"b0","properties":{"beacon_effect":"a"},)"
        R"("model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    ASSERT_TRUE(r0.ok()) << r0.error().message;
    EXPECT_EQ(reg.at(*r0).properties.beaconEffects,
              (std::vector<std::string>{"a"}));
    // An empty list is a no-op typo (rejected).
    auto rEmpty = reg.registerBlock(parseDef(
        R"({"id":"be","properties":{"beacon_effects":[]},)"
        R"("model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    ASSERT_FALSE(rEmpty.ok());
    EXPECT_NE(rEmpty.error().message.find("beacon_effects"),
              std::string::npos);
    // A non-string / empty entry is rejected (named).
    auto rBad = reg.registerBlock(parseDef(
        R"({"id":"bb","properties":{"beacon_effects":["a",""]},)"
        R"("model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    ASSERT_FALSE(rBad.ok());
    EXPECT_NE(rBad.error().message.find("beacon_effects[1]"),
              std::string::npos);
}

TEST(BlockRegistryTest, Phase91BeaconEffectMutuallyExclusive) {
    // beacon_effect (single) and beacon_effects (list) are mutually
    // exclusive (both present = rejected, the grants_effect /
    // grants_effects discipline).
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(
        R"({"id":"bm","properties":{"beacon_effect":"a",)"
        R"("beacon_effects":["a"]}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("both"), std::string::npos);
}

TEST(BlockRegistryTest, Phase83IronBlockContent) {
    // The shipped iron block: the canonical beacon base (beacon_base true),
    // hardness 5 (Minecraft), drops itself.
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    BlockRegistry reg;
    ASSERT_TRUE(reg.loadDirectory(assets, "data/blocks").ok());
    const Block* b = reg.findByName("iron_block");
    ASSERT_TRUE(b != nullptr);
    EXPECT_TRUE(b->properties.solid);
    EXPECT_TRUE(b->properties.opaque);
    EXPECT_EQ(b->properties.lightEmission, 0);
    EXPECT_FLOAT_EQ(b->properties.hardness, 5.0f);
    EXPECT_EQ(b->properties.drops, "iron_block"); // drops itself
    EXPECT_TRUE(b->properties.beaconBase);
    EXPECT_EQ(b->model.shape, BlockModel::Shape::FullCube);
}

TEST(BlockRegistryTest, Phase85StoneSlabContent) {
    // The shipped stone slab (Phase 85): the bottom half-block shape, reusing
    // the stone texture (no new atlas tile), hardness 2.0, drops itself,
    // solid + opaque (it occludes light like a full block, a v1 simplification).
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    BlockRegistry reg;
    ASSERT_TRUE(reg.loadDirectory(assets, "data/blocks").ok());
    const Block* b = reg.findByName("stone_slab");
    ASSERT_TRUE(b != nullptr);
    EXPECT_EQ(b->model.shape, BlockModel::Shape::Slab);
    EXPECT_TRUE(b->properties.solid);
    EXPECT_TRUE(b->properties.opaque);
    EXPECT_EQ(b->properties.lightEmission, 0);
    EXPECT_FLOAT_EQ(b->properties.hardness, 2.0f);
    EXPECT_EQ(b->properties.drops, "stone_slab"); // drops itself
    EXPECT_EQ(b->model.textures.top, "stone"); // reuses the stone texture
    EXPECT_EQ(b->model.textures.side, "stone");
    EXPECT_EQ(b->model.textures.bottom, "stone");
}

TEST(BlockRegistryTest, Phase95WallShapeParsed) {
    // The "wall" shape (Phase 95, the third new block model type) is parsed;
    // it uses the cube face textures (like the slab).
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(
        R"({"id":"w","model":{"shape":"wall","textures":{"top":"t","side":"t","bottom":"t"}}})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(reg.at(*r).model.shape, BlockModel::Shape::Wall);
}

TEST(BlockRegistryTest, Phase98FenceShapeParsed) {
    // The "fence" shape (Phase 98, the fourth new block model type) uses a
    // single "side" texture (the posts + bars all draw it).
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(
        R"({"id":"f","model":{"shape":"fence","textures":{"side":"t"}}})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(reg.at(*r).model.shape, BlockModel::Shape::Fence);
    EXPECT_EQ(reg.at(*r).model.textures.side, "t");
}

TEST(BlockRegistryTest, Phase98FenceRequiresSideTexture) {
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(
        R"({"id":"f","model":{"shape":"fence","textures":{"top":"t"}}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("model.textures.side"), std::string::npos);
}

TEST(BlockRegistryTest, Phase98FenceRejectsCubeTextureKeys) {
    // The cube face keys are dead data for a fence (no cube faces) —
    // rejected (the cross discipline).
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(
        R"({"id":"f","model":{"shape":"fence","textures":{"side":"t","top":"t"}}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("model.textures.top"), std::string::npos);
}

TEST(BlockRegistryTest, Phase98FenceContent) {
    // The shipped wooden fence (Phase 98): the fence shape, a single
    // "fence" texture, hardness 2.0 (Minecraft), drops itself, opaque (it
    // blocks light like a full block).
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    BlockRegistry reg;
    ASSERT_TRUE(reg.loadDirectory(assets, "data/blocks").ok());
    const Block* b = reg.findByName("fence");
    ASSERT_TRUE(b != nullptr);
    EXPECT_EQ(b->model.shape, BlockModel::Shape::Fence);
    EXPECT_TRUE(b->properties.solid);
    EXPECT_TRUE(b->properties.opaque);
    EXPECT_EQ(b->properties.lightEmission, 0);
    EXPECT_FLOAT_EQ(b->properties.hardness, 2.0f);
    EXPECT_EQ(b->properties.drops, "fence"); // drops itself
    EXPECT_EQ(b->model.textures.side, "fence"); // single side texture
}

TEST(BlockRegistryTest, Phase99GlassPaneShapeParsed) {
    // The "glass_pane" shape (Phase 99, the fifth new block model type)
    // uses a single "side" texture (the pane sheet + rim slivers).
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(
        R"({"id":"gp","properties":{"opaque":false,"transparent":true},"model":{"shape":"glass_pane","textures":{"side":"t"}}})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(reg.at(*r).model.shape, BlockModel::Shape::GlassPane);
    EXPECT_EQ(reg.at(*r).model.textures.side, "t");
}

TEST(BlockRegistryTest, Phase99GlassPaneRequiresSideTexture) {
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(
        R"({"id":"gp","properties":{"opaque":false,"transparent":true},"model":{"shape":"glass_pane","textures":{"top":"t"}}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("model.textures.side"),
              std::string::npos);
}

TEST(BlockRegistryTest, Phase99GlassPaneForcesNonOpaque) {
    // A pane must not be opaque (light passes through the sheet — the
    // property combination is forced, the cross discipline). opaque:true +
    // transparent:false passes the generic pair check and hits the forced
    // rule.
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(
        R"({"id":"gp","properties":{"opaque":true,"transparent":false},"model":{"shape":"glass_pane","textures":{"side":"t"}}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("must not be 'opaque'"),
              std::string::npos);
}

TEST(BlockRegistryTest, Phase99GlassPaneForcesTransparent) {
    // A pane must be transparent (it renders in the blending pass).
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(
        R"({"id":"gp","properties":{"opaque":false,"transparent":false},"model":{"shape":"glass_pane","textures":{"side":"t"}}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("must be 'transparent'"),
              std::string::npos);
}

TEST(BlockRegistryTest, Phase99GlassPaneContent) {
    // The shipped glass pane (Phase 99): the pane shape, a single
    // "glass_pane" texture, hardness 0.3 (Minecraft's glass), drops itself,
    // non-opaque + transparent (light passes through), solid (it blocks
    // movement — jumpable at 1.0).
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    BlockRegistry reg;
    ASSERT_TRUE(reg.loadDirectory(assets, "data/blocks").ok());
    const Block* b = reg.findByName("glass_pane");
    ASSERT_TRUE(b != nullptr);
    EXPECT_EQ(b->model.shape, BlockModel::Shape::GlassPane);
    EXPECT_TRUE(b->properties.solid);
    EXPECT_FALSE(b->properties.opaque);
    EXPECT_TRUE(b->properties.transparent);
    EXPECT_EQ(b->properties.lightEmission, 0);
    EXPECT_FLOAT_EQ(b->properties.hardness, 0.3f);
    EXPECT_EQ(b->properties.drops, "glass_pane"); // drops itself
    EXPECT_EQ(b->model.textures.side, "glass_pane"); // single side texture
}

TEST(BlockRegistryTest, Phase97StairFacingFromYaw) {
    // Phase 97: placement auto-orientation — the stair's low side faces
    // where the player looks (facing = the HIGH side). yaw 0 = facing -Z.
    const float pi = 3.14159265358979323846f;
    // The four cardinal placements (Minecraft's rule).
    EXPECT_EQ(stairFacingFromYaw(0.0f), 2); // facing -Z -> high +Z (south)
    EXPECT_EQ(stairFacingFromYaw(pi / 2), 1); // facing -X -> high +X (east)
    EXPECT_EQ(stairFacingFromYaw(pi), 0); // facing +Z -> high -Z (north)
    EXPECT_EQ(stairFacingFromYaw(3 * pi / 2), 3); // facing +X -> high -X
    // Quadrant robustness: a yaw within a 45-degree window of a cardinal
    // snaps to that cardinal.
    EXPECT_EQ(stairFacingFromYaw(0.3f), 2);
    EXPECT_EQ(stairFacingFromYaw(-0.3f), 2);
    EXPECT_EQ(stairFacingFromYaw(pi / 2 - 0.3f), 1);
    EXPECT_EQ(stairFacingFromYaw(pi / 2 + 0.3f), 1);
    EXPECT_EQ(stairFacingFromYaw(pi - 0.3f), 0);
    EXPECT_EQ(stairFacingFromYaw(pi + 0.3f), 0);
    EXPECT_EQ(stairFacingFromYaw(3 * pi / 2 - 0.3f), 3);
    EXPECT_EQ(stairFacingFromYaw(3 * pi / 2 + 0.3f), 3);
    // The exact 45-degree tie resolves to the X axis (deterministic).
    EXPECT_EQ(stairFacingFromYaw(pi / 4), 1); // fx=fz<0 -> east
    EXPECT_EQ(stairFacingFromYaw(5 * pi / 4), 3); // fx<0, fz>0 -> west
}

TEST(BlockRegistryTest, Phase95StoneWallContent) {
    // The shipped stone wall (Phase 95): the 1.5-block-tall pillar shape,
    // reusing the stone texture (no new atlas tile), hardness 1.5 (Minecraft),
    // drops itself, solid + opaque.
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    BlockRegistry reg;
    ASSERT_TRUE(reg.loadDirectory(assets, "data/blocks").ok());
    const Block* b = reg.findByName("stone_wall");
    ASSERT_TRUE(b != nullptr);
    EXPECT_EQ(b->model.shape, BlockModel::Shape::Wall);
    EXPECT_TRUE(b->properties.solid);
    EXPECT_TRUE(b->properties.opaque);
    EXPECT_EQ(b->properties.lightEmission, 0);
    EXPECT_FLOAT_EQ(b->properties.hardness, 1.5f);
    EXPECT_EQ(b->properties.drops, "stone_wall"); // drops itself
    EXPECT_EQ(b->model.textures.top, "stone"); // reuses the stone texture
    EXPECT_EQ(b->model.textures.side, "stone");
    EXPECT_EQ(b->model.textures.bottom, "stone");
}

TEST(BlockRegistryTest, Phase87StoneStairsContent) {
    // The shipped stone stairs (Phase 87): the north-facing wedge shape,
    // reusing the stone texture (no new atlas tile), hardness 1.5, drops
    // itself, solid + opaque.
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    BlockRegistry reg;
    ASSERT_TRUE(reg.loadDirectory(assets, "data/blocks").ok());
    const Block* b = reg.findByName("stone_stairs");
    ASSERT_TRUE(b != nullptr);
    EXPECT_EQ(b->model.shape, BlockModel::Shape::Stairs);
    EXPECT_TRUE(b->properties.solid);
    EXPECT_TRUE(b->properties.opaque);
    EXPECT_EQ(b->properties.lightEmission, 0);
    EXPECT_FLOAT_EQ(b->properties.hardness, 1.5f);
    EXPECT_EQ(b->properties.drops, "stone_stairs"); // drops itself
    EXPECT_EQ(b->model.textures.top, "stone"); // reuses the stone texture
    EXPECT_EQ(b->model.textures.side, "stone");
    EXPECT_EQ(b->model.textures.bottom, "stone");
}

TEST(BlockRegistryTest, Phase87StairsShapeRequiresCubeTextures) {
    // A stairs uses the cube face textures (like a slab); a "cross" texture
    // is rejected (a stair is a solid wedge, not a cross plant).
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(
        R"({"id":"st","model":{"shape":"stairs","textures":{"top":"t","side":"t","bottom":"t"}}})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(reg.at(*r).model.shape, BlockModel::Shape::Stairs);
    auto bad = reg.registerBlock(parseDef(
        R"({"id":"st2","model":{"shape":"stairs","textures":{"cross":"t"}}})"));
    ASSERT_FALSE(bad.ok());
}

TEST(BlockRegistryTest, Phase89StairFacingParsed) {
    // The stair_facing field (0..3) is parsed; out-of-range is rejected and
    // named. A default (absent) facing is 0 (north).
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(
        R"({"id":"st","model":{"shape":"stairs","stair_facing":2,)"
        R"("textures":{"top":"t","side":"t","bottom":"t"}}})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(reg.at(*r).model.stairFacing, 2); // south
    auto r0 = reg.registerBlock(parseDef(
        R"({"id":"st0","model":{"shape":"stairs",)"
        R"("textures":{"top":"t","side":"t","bottom":"t"}}})"));
    ASSERT_TRUE(r0.ok());
    EXPECT_EQ(reg.at(*r0).model.stairFacing, 0); // default north
    auto bad = reg.registerBlock(parseDef(
        R"({"id":"stb","model":{"shape":"stairs","stair_facing":4,)"
        R"("textures":{"top":"t","side":"t","bottom":"t"}}})"));
    ASSERT_FALSE(bad.ok());
    EXPECT_NE(bad.error().message.find("stair_facing"), std::string::npos);
}

TEST(BlockRegistryTest, Phase89StairVariantsContent) {
    // The 4 shipped stair variants (north/east/south/west) reuse the stone
    // texture and differ only in stair_facing (0/1/2/3).
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    BlockRegistry reg;
    ASSERT_TRUE(reg.loadDirectory(assets, "data/blocks").ok());
    const struct {
        const char* name;
        int facing;
    } want[] = {
        {"stone_stairs", 0}, {"stone_stairs_east", 1},
        {"stone_stairs_south", 2}, {"stone_stairs_west", 3},
    };
    for (const auto& w : want) {
        const Block* b = reg.findByName(w.name);
        ASSERT_TRUE(b != nullptr) << w.name;
        EXPECT_EQ(b->model.shape, BlockModel::Shape::Stairs);
        EXPECT_EQ(b->model.stairFacing, w.facing);
        EXPECT_EQ(b->model.textures.top, "stone");
        EXPECT_EQ(b->properties.drops, w.name);
    }
}

TEST(BlockRegistryTest, Phase92StairRotatesToParsed) {
    // stair_rotates_to (the next facing variant) is parsed; an EXPLICIT
    // empty string is a no-op typo (rejected, the ammo discipline).
    const std::string model =
        R"("model":{"shape":"stairs","textures":{"top":"t","side":"t","bottom":"t"}})";
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(
        std::string(R"({"id":"st","properties":{"stair_rotates_to":"ste"},)") +
        model + "}"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(reg.at(*r).properties.stairRotatesTo, "ste");
    auto rEmpty = reg.registerBlock(parseDef(
        std::string(R"({"id":"ste","properties":{"stair_rotates_to":""},)") +
        model + "}"));
    ASSERT_FALSE(rEmpty.ok());
    EXPECT_NE(rEmpty.error().message.find("stair_rotates_to"),
              std::string::npos);
}

TEST(BlockRegistryTest, Phase92StairRotationCycle) {
    // The 4 shipped stair variants form a closed rotation cycle (north ->
    // east -> south -> west -> north): each variant's stair_rotates_to names
    // the NEXT facing, and the cycle closes back to stone_stairs.
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    BlockRegistry reg;
    ASSERT_TRUE(reg.loadDirectory(assets, "data/blocks").ok());
    const struct {
        const char* name;
        const char* next;
        int nextFacing;
    } cycle[] = {
        {"stone_stairs", "stone_stairs_east", 1},
        {"stone_stairs_east", "stone_stairs_south", 2},
        {"stone_stairs_south", "stone_stairs_west", 3},
        {"stone_stairs_west", "stone_stairs", 0}, // closes the cycle
    };
    for (const auto& c : cycle) {
        const Block* b = reg.findByName(c.name);
        ASSERT_TRUE(b != nullptr) << c.name;
        EXPECT_EQ(b->properties.stairRotatesTo, c.next);
        // The named next variant exists and has the next facing.
        const Block* n = reg.findByName(c.next);
        ASSERT_TRUE(n != nullptr) << c.next;
        EXPECT_EQ(n->model.stairFacing, c.nextFacing);
    }
}

TEST(BlockRegistryTest, Phase85SlabShapeRequiresCubeTextures) {
    // A slab uses the cube face textures (top/side/bottom), like a full cube
    // (the "cross" texture is rejected — a slab is not a cross plant).
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(
        R"({"id":"s","model":{"shape":"slab","textures":{"top":"t","side":"t","bottom":"t"}}})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(reg.at(*r).model.shape, BlockModel::Shape::Slab);
    // A "cross" texture key on a slab is a typo (slabs use cube textures).
    auto bad = reg.registerBlock(parseDef(
        R"({"id":"s2","model":{"shape":"slab","textures":{"cross":"t"}}})"));
    ASSERT_FALSE(bad.ok());
}

TEST(BlockRegistryTest, Phase83IronBlockRecipeContent) {
    // The shipped iron block recipe: shaped 3x3 iron_ingot -> 1 (the
    // Minecraft pattern — the beacon base is crafted from the ingot).
    FILE* fp = std::fopen("assets/data/recipes/iron_block.json", "rb");
    ASSERT_NE(fp, nullptr) << "run from the repo root";
    std::string text;
    char buf[4096];
    std::size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), fp)) > 0) {
        text.append(buf, n);
    }
    std::fclose(fp);
    auto parsed = Json::parse(text);
    ASSERT_TRUE(parsed.ok()) << parsed.error().message;
    const Json& def = *parsed;
    EXPECT_EQ(def.find("output")->find("item")->asString(), "iron_block");
    EXPECT_EQ(def.find("output")->find("count")->asInt(), 1);
    const Json& pattern = *def.find("pattern");
    ASSERT_EQ(pattern.size(), 3u);
    for (std::size_t i = 0; i < pattern.size(); ++i) {
        EXPECT_EQ(pattern.at(i).asString(), "III");
    }
    EXPECT_EQ(def.find("key")->find("I")->asString(), "iron_ingot");
}
