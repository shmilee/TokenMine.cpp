// CRAFT-3 tests: the sound-material name selection (blockSoundName) + the
// init cross-check (checkSoundMaterials) — pure over the registry +
// assets (no SDL, no Game — the campfire / durability paradigm).
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <unistd.h>

#include "block/block_sound.hpp"

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

// A definition that passes validation, with an optional sound_material.
std::string defWithMaterial(const std::string& id,
                            const std::string& material = "") {
    std::string props;
    if (!material.empty()) {
        props = "\"sound_material\":\"" + material + "\",";
    }
    return "{\"id\":\"" + id + "\",\"properties\":{" + props +
           "\"hardness\":1.0},\"model\":{\"textures\":{\"top\":\"t\","
           "\"side\":\"t\",\"bottom\":\"t\"}}}";
}

// A scratch asset root with dummy sound files (existence is all the
// cross-check reads — the content is irrelevant).
std::filesystem::path makeRoot(const std::vector<std::string>& wavs) {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        ("mc_block_sound_" + std::to_string(::getpid()));
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "sounds");
    for (const std::string& w : wavs) {
        std::ofstream f(root / "sounds" / w);
        f << "fake";
    }
    return root;
}

} // namespace

// -- blockSoundName (the event-name selection, the bit-level pin) --------

TEST(BlockSoundNameTest, GenericEventsWithoutMaterial) {
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(defWithMaterial("rock")));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const Block& b = reg.at(*r);
    // The no-material block keeps the EXACT generic names (the structural
    // immunity red line: pre-CRAFT-3 samples, bit-level).
    EXPECT_EQ(blockSoundName(b, "dig"), "dig");
    EXPECT_EQ(blockSoundName(b, "place"), "place");
    EXPECT_EQ(blockSoundName(b, "break"), "break");
}

TEST(BlockSoundNameTest, MaterialPrefixedEvents) {
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(defWithMaterial("rock", "stone")));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const Block& b = reg.at(*r);
    EXPECT_EQ(blockSoundName(b, "dig"), "stone_dig");
    EXPECT_EQ(blockSoundName(b, "place"), "stone_place");
    EXPECT_EQ(blockSoundName(b, "break"), "stone_break");
}

TEST(BlockSoundNameTest, EventVocabularyIsExactlyThree) {
    EXPECT_EQ(kSoundMaterialEvents[0], "dig");
    EXPECT_EQ(kSoundMaterialEvents[1], "place");
    EXPECT_EQ(kSoundMaterialEvents[2], "break");
}

// -- checkSoundMaterials (the init cross-check, the grows_to discipline) --

TEST(CheckSoundMaterialsTest, PassesWhenAllThreeExist) {
    const auto root = makeRoot({"stone_dig.wav", "stone_place.wav",
                                "stone_break.wav"});
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot(root).ok());
    BlockRegistry reg;
    ASSERT_TRUE(reg.registerBlock(parseDef(defWithMaterial("rock", "stone"))).ok());
    auto r = checkSoundMaterials(reg, assets);
    EXPECT_TRUE(r.ok()) << r.error().message;
    std::filesystem::remove_all(root);
}

TEST(CheckSoundMaterialsTest, NamesTheMissingPiece) {
    // Only two of the three pieces: the failure names the missing one.
    const auto root = makeRoot({"stone_dig.wav", "stone_place.wav"});
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot(root).ok());
    BlockRegistry reg;
    ASSERT_TRUE(reg.registerBlock(parseDef(defWithMaterial("rock", "stone"))).ok());
    auto r = checkSoundMaterials(reg, assets);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("rock"), std::string::npos)
        << r.error().message;
    EXPECT_NE(r.error().message.find("stone_break.wav"), std::string::npos)
        << r.error().message;
    std::filesystem::remove_all(root);
}

TEST(CheckSoundMaterialsTest, NamesTheFirstMissingBlock) {
    // Registry order (id order) decides which failure surfaces: the block
    // registered FIRST with a broken material is the one named.
    const auto root = makeRoot({"stone_dig.wav", "stone_place.wav",
                                "stone_break.wav",
                                "wood_dig.wav"});
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot(root).ok());
    BlockRegistry reg;
    ASSERT_TRUE(reg.registerBlock(parseDef(defWithMaterial("alpha", "stone"))).ok());
    ASSERT_TRUE(reg.registerBlock(parseDef(defWithMaterial("beta", "wood"))).ok());
    // alpha (stone) is complete; beta (wood) misses place + break -> beta
    // is named, with its FIRST missing piece (wood_place.wav).
    auto r = checkSoundMaterials(reg, assets);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("beta"), std::string::npos)
        << r.error().message;
    EXPECT_NE(r.error().message.find("wood_place.wav"), std::string::npos)
        << r.error().message;
    std::filesystem::remove_all(root);
}

TEST(CheckSoundMaterialsTest, FieldlessBlocksNeedNoFiles) {
    // An empty sound directory + field-less blocks = a clean pass (the
    // generic fallback is the default, the structural-immunity side).
    const auto root = makeRoot({});
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot(root).ok());
    BlockRegistry reg;
    ASSERT_TRUE(reg.registerBlock(parseDef(defWithMaterial("rock"))).ok());
    auto r = checkSoundMaterials(reg, assets);
    EXPECT_TRUE(r.ok()) << r.error().message;
    std::filesystem::remove_all(root);
}

TEST(CheckSoundMaterialsTest, CustomSoundDirectory) {
    const auto root = std::filesystem::temp_directory_path() /
                      ("mc_block_sound_custom_" + std::to_string(::getpid()));
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "altsounds");
    for (const std::string& w : {std::string("stone_dig.wav"),
                                 std::string("stone_place.wav"),
                                 std::string("stone_break.wav")}) {
        std::ofstream f(root / "altsounds" / w);
        f << "fake";
    }
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot(root).ok());
    BlockRegistry reg;
    ASSERT_TRUE(reg.registerBlock(parseDef(defWithMaterial("rock", "stone"))).ok());
    EXPECT_FALSE(checkSoundMaterials(reg, assets).ok()); // "sounds" is empty
    auto r = checkSoundMaterials(reg, assets, "altsounds");
    EXPECT_TRUE(r.ok()) << r.error().message;
    std::filesystem::remove_all(root);
}
