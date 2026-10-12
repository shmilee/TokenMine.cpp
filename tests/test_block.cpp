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
    EXPECT_TRUE(b.properties.fluidName.empty());
    EXPECT_TRUE(b.properties.waterReactionSource.empty());
    EXPECT_TRUE(b.properties.waterReactionFlowing.empty());
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
    auto r = reg.registerBlock(
        parseDef(crossDef(
            "x", R"({"shape":"slab","textures":{"cross":"x"}})")));
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
