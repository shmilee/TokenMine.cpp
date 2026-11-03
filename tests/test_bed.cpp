// V2-13 (v2.14.0): the bed sleep system — the content pins (the 15
// block family + the 15 shaped recipes + the sleep.wav + the dye-family
// completeness), the pure gate family's boundaries (core/bed: the night
// window, the 3-block distance, the 8x5 hostile box, the respawn
// validation), and the save format 28 (player.respawn — the int cell
// triple, always written, the three rejections + the pre-28 sentinel
// migration).
#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "block/block_registry.hpp"
#include "core/asset_manager.hpp"
#include "core/bed.hpp"
#include "core/enchanting.hpp"
#include "core/json.hpp"
#include "core/save.hpp"
#include "core/sound.hpp"
#include "core/status_effect.hpp"
#include "core/tutorial.hpp"
#include "item/inventory.hpp"
#include "item/item.hpp"
#include "item/item_registry.hpp"
#include "item/recipe.hpp"
#include "player/drown.hpp" // kOxygenMax
#include "world/world.hpp"

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

// Remove `key` from a JSON object (the test_enchant_save twin — it is
// file-local there).
Json withoutKeyForTest(const Json& obj, const std::string& key) {
    Json out = Json::object();
    for (const auto& [k, v] : obj.members()) {
        if (k != key) {
            out[k] = v;
        }
    }
    return out;
}

// The 15 dye colors of the bed family (the wool's exact vocabulary —
// white is the base, no suffix; no lime, the wool's documented set).
const std::vector<std::string>& bedColors() {
    static const std::vector<std::string> colors {
        "white", "black", "blue", "brown", "cyan", "gray", "green",
        "light_blue", "magenta", "orange", "pink", "purple", "red",
        "silver", "yellow"};
    return colors;
}

std::string bedName(const std::string& color) {
    return color == "white" ? std::string("bed") : "bed_" + color;
}

std::string woolName(const std::string& color) {
    return color == "white" ? std::string("wool") : "wool_" + color;
}

// A minimal save fixture (the test_enchant_save pattern): the stone
// block + its synthesized block item, the poison effect (the format-12
// absorption invariant's input), the full slot arrays.
struct SaveFixture {
    BlockRegistry blocks;
    EffectRegistry effects;
    ItemRegistry items;
    SaveGame save;

    SaveFixture() {
        blocks.registerBlock(parseDef(
            R"({"id":"stone","model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
        effects.registerEffect(parseDef(
            R"({"id":"poison","tick_type":"damage","tick_amount":1.0,"tick_interval":2.5,"min_health":1.0,"negative":true,"icon":0})"));
        if (!items.buildBlockItems(blocks).ok()) {
            ADD_FAILURE();
        }
        save.seed = 42;
        save.time = 0.7;
        save.playerPos = {1.5f, 53.0f, 2.5f};
        save.playerAbsorption = 0.0f;
        save.playerOxygen = kOxygenMax;
        save.playerXp = 0;
        save.playerXpSeed = 0x1234ULL;
        save.playerRespawn = {3.5f, 54.5f, -7.5f}; // a bed-form position
        save.slots.resize(Inventory::kTotalSize);
        save.invGrid.resize(CraftingGrid::kSize);
    }
};

// The bed family block (any color) + the inert stone, for the respawn
// validation's world fixture (the core/bed consumer: the dye_base
// property — the family check).
BlockRegistry bedRegistry(const std::string& color) {
    BlockRegistry reg;
    const std::string bedDef =
        std::string(R"({"id":")") + bedName(color) +
        R"(","properties":{"dye_base":"bed","dye_color":")" + color +
        R"(","solid":true},"model":{"shape":"slab","textures":{"top":"t","side":"t","bottom":"t"}}})";
    const std::string stoneDef =
        R"({"id":"stone","properties":{"solid":true},"model":{"textures":{"top":"t","side":"t","bottom":"t"}}})";
    for (const std::string& def : {bedDef, stoneDef}) {
        auto r = reg.registerBlock(parseDef(def));
        if (!r.ok()) {
            throw std::runtime_error(r.error().message);
        }
    }
    return reg;
}

} // namespace

// ===========================================================================
// content pins (the shipped data)
// ===========================================================================

TEST(BedContentTest, FifteenBlocksAreTheDyeFamily) {
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    BlockRegistry reg;
    ASSERT_TRUE(reg.loadDirectory(assets, "data/blocks").ok());
    for (const std::string& color : bedColors()) {
        const std::string name = bedName(color);
        const Block* b = reg.findByName(name);
        ASSERT_TRUE(b != nullptr) << name;
        // The family (the property the core reads — never a color id):
        EXPECT_EQ(b->properties.dyeBase, "bed") << name;
        EXPECT_EQ(b->properties.dyeColor, color) << name;
        // The slab shape (the bottom half-cell — the stone_slab
        // precedent; the 1x2 pair is the documented deviation 1):
        EXPECT_EQ(b->model.shape, BlockModel::Shape::Slab) << name;
        // The Minecraft bed values (the named-constant discipline, the
        // data side): hardness 0.2, the cloth material, the self drop.
        EXPECT_FLOAT_EQ(b->properties.hardness, 0.2f) << name;
        EXPECT_EQ(b->properties.soundMaterial, "cloth") << name;
        EXPECT_EQ(b->properties.drops, name) << name;
        EXPECT_EQ(b->properties.dropsCount, 1) << name;
        // The slab's light (a half-block does not fully block it — the
        // stone_slab's light_opacity 0):
        EXPECT_EQ(b->properties.lightOpacity, 0) << name;
        // The texture is the block's own name (the atlas's 15 tiles):
        EXPECT_EQ(b->model.textures.top, name) << name;
        EXPECT_EQ(b->model.textures.side, name) << name;
        EXPECT_EQ(b->model.textures.bottom, name) << name;
        // No interact (dye_base / interact are mutually exclusive — the
        // data load enforces it; the pin is the shipped value):
        EXPECT_TRUE(b->properties.interact.empty()) << name;
    }
}

TEST(BedContentTest, FamilyCoversTheFullDyeVocabulary) {
    // The init cross-check's data-level twin: for every dye ITEM color
    // (the registry's vocabulary), the bed family has its variant block
    // (a missing one would be a dead dye — the Phase 41 completeness
    // rule the game init enforces).
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok());
    BlockRegistry blocks;
    ASSERT_TRUE(blocks.loadDirectory(assets, "data/blocks").ok());
    ItemRegistry items;
    ASSERT_TRUE(items.buildBlockItems(blocks).ok());
    ASSERT_TRUE(items.loadDirectory(assets, "data/items").ok());
    std::size_t colors = 0;
    for (const ItemID id : items.allIds()) {
        if (items.at(id).dyeColor.empty()) {
            continue;
        }
        ++colors;
        const std::string variant = dyeVariantName("bed", items.at(id).dyeColor);
        EXPECT_TRUE(blocks.findByName(variant) != nullptr) << variant;
    }
    EXPECT_EQ(colors, bedColors().size());
    // The base is the white member (the dyeVariantName convention):
    EXPECT_EQ(dyeVariantName("bed", kDyeColorWhite), "bed");
}

TEST(BedContentTest, FifteenShapedRecipes) {
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok());
    BlockRegistry blocks;
    ASSERT_TRUE(blocks.loadDirectory(assets, "data/blocks").ok());
    ItemRegistry items;
    ASSERT_TRUE(items.buildBlockItems(blocks).ok());
    ASSERT_TRUE(items.loadDirectory(assets, "data/items").ok());
    RecipeRegistry recipes;
    ASSERT_TRUE(recipes.loadDirectory(assets, items, "data/recipes").ok());
    for (const std::string& color : bedColors()) {
        const std::string name = bedName(color);
        const Recipe* r = recipes.find(name);
        ASSERT_TRUE(r != nullptr) << name;
        EXPECT_EQ(r->type, RecipeType::Shaped) << name;
        EXPECT_EQ(r->output, items.idForName(name)) << name;
        EXPECT_EQ(r->outputCount, 1) << name;
        // The Minecraft bed pattern (3 same-color wool over 3 planks —
        // the top row empty; planks is the project's single kind):
        EXPECT_EQ(r->pattern, (std::vector<std::string> {"   ", "WWW", "PPP"}))
            << name;
        EXPECT_EQ(r->key.size(), 2u) << name;
        EXPECT_EQ(r->key.at('W'), items.idForName(woolName(color))) << name;
        EXPECT_EQ(r->key.at('P'), items.idForName("planks")) << name;
    }
}

TEST(BedContentTest, DyedBedWithInteractIsRejected) {
    // The S6 named case: a bed carrying an interact is a version lie at
    // the data load (dye_base / interact are mutually exclusive — the
    // block registry's check names both sides).
    auto bad = BlockRegistry {}
                   .registerBlock(parseDef(
                       R"({"id":"bed","properties":{"dye_base":"bed","dye_color":"white","interact":"furnace"},"model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    ASSERT_FALSE(bad.ok());
    EXPECT_NE(bad.error().message.find("interact"), std::string::npos)
        << bad.error().message;
    EXPECT_NE(bad.error().message.find("dye_base"), std::string::npos)
        << bad.error().message;
}

// ===========================================================================
// the pure gate family (core/bed)
// ===========================================================================

TEST(BedSleepGateTest, NightWindowBoundaries) {
    // The window is [kDuskEnd, 1) + [0, kDawnStart) — the day cycle's
    // ramp edges (the derived constant span; the MC sky<=11 = [0.586,
    // 0.045] is the documented deviation 3 — the project's window is
    // slightly wider).
    EXPECT_TRUE(sleepNightWindow(DayCycle::kDuskEnd));      // the dusk edge: night
    EXPECT_FALSE(sleepNightWindow(DayCycle::kDuskEnd - 1e-6)); // just before: day
    EXPECT_FALSE(sleepNightWindow(DayCycle::kDawnStart));    // the dawn edge: day (the skip lands here)
    EXPECT_TRUE(sleepNightWindow(DayCycle::kDawnStart - 1e-6)); // just before: night
    EXPECT_TRUE(sleepNightWindow(0.0));      // midnight is night
    EXPECT_TRUE(sleepNightWindow(0.8));      // the night center
    EXPECT_TRUE(sleepNightWindow(0.9999999)); // just before the wrap
    EXPECT_FALSE(sleepNightWindow(0.3));     // noon is day
    EXPECT_FALSE(sleepNightWindow(0.5));     // the dusk ramp: day
}

TEST(BedSleepGateTest, DistanceBoundaries) {
    // The player's feet vs the bed's cell center, 3D (the Minecraft 3 —
    // kSleepMaxDistance): the inclusive edge (exactly 3 still sleeps).
    const Vec3 bed {0.5f, 5.5f, 0.5f};
    EXPECT_TRUE(sleepDistanceOk(Vec3 {3.5f, 5.5f, 0.5f}, bed));    // exactly 3.0 on +X
    EXPECT_FALSE(sleepDistanceOk(Vec3 {3.5001f, 5.5f, 0.5f}, bed)); // just beyond
    EXPECT_TRUE(sleepDistanceOk(Vec3 {0.5f, 8.5f, 0.5f}, bed));     // exactly 3.0 vertical
    EXPECT_TRUE(sleepDistanceOk(Vec3 {0.5f, 5.5f, 0.5f}, bed));     // standing on it
    EXPECT_TRUE(sleepDistanceOk(Vec3 {0.5f + 1.5f, 5.5f, 0.5f - 1.5f}, bed)); // the 1.5-1.5 diagonal (= 2.12)
    EXPECT_FALSE(sleepDistanceOk(Vec3 {0.5f + 2.5f, 5.5f, 0.5f - 2.5f}, bed)); // the 2.5-2.5 diagonal (= 3.54)
}

TEST(BedSleepGateTest, HostileBoxBoundaries) {
    // The 8x5 box around the bed's cell center (the Minecraft head
    // volume — the single-cell slab's whole cell as the head, the
    // deviation 4): horizontal +/-8, vertical +5/-1, the CLOSED
    // boundary (a mob on the face still prevents the sleep).
    const Vec3 bed {0.5f, 5.5f, 0.5f};
    // The eight corners (the box is the closed [c-8, c+8] x [c-1, c+5] x [c-8, c+8]):
    for (const float dx : {-8.0f, 8.0f}) {
        for (const float dy : {-1.0f, 5.0f}) {
            for (const float dz : {-8.0f, 8.0f}) {
                EXPECT_TRUE(inSleepHostileBox(bed, Vec3 {bed.x + dx, bed.y + dy, bed.z + dz}))
                    << dx << " " << dy << " " << dz;
            }
        }
    }
    // Just outside each face:
    EXPECT_FALSE(inSleepHostileBox(bed, Vec3 {bed.x + 8.01f, bed.y, bed.z}));
    EXPECT_FALSE(inSleepHostileBox(bed, Vec3 {bed.x - 8.01f, bed.y, bed.z}));
    EXPECT_FALSE(inSleepHostileBox(bed, Vec3 {bed.x, bed.y + 5.01f, bed.z}));
    EXPECT_FALSE(inSleepHostileBox(bed, Vec3 {bed.x, bed.y - 1.01f, bed.z}));
    EXPECT_FALSE(inSleepHostileBox(bed, Vec3 {bed.x, bed.y, bed.z + 8.01f}));
    // The center (a mob on the bed's cell) blocks:
    EXPECT_TRUE(inSleepHostileBox(bed, bed));
}

TEST(BedSleepGateTest, SpawnPositionCellRoundTrip) {
    const Vec3 pos = bedSpawnPosition(12, 54, -3);
    EXPECT_EQ(pos.x, 12.5f);
    EXPECT_EQ(pos.y, 54.5f);
    EXPECT_EQ(pos.z, -2.5f);
    EXPECT_EQ(bedSpawnCell(pos), (BlockPos {12, 54, -3}));
    // The negative-floor discipline (floorDiv's domain — a negative
    // cell-center floors to its own cell):
    EXPECT_EQ(bedSpawnCell(bedSpawnPosition(-4, 5, -4)),
              (BlockPos {-4, 5, -4}));
}

// ===========================================================================
// the respawn-time bed validation (bedRespawnValid)
// ===========================================================================

TEST(BedRespawnTest, BedWithRoomIsValid) {
    // The player's AABB (the default 0.6 x 1.8) at the bed's cell
    // center: the bed cell itself is the support (the slab's solid
    // region ends at the feet — exempt), the headroom cells air.
    BlockRegistry reg = bedRegistry("white");
    const BlockID bed = reg.idForName("bed");
    World world;
    world.getOrCreateChunk({0, 0, 0});
    world.setBlock(0, 5, 0, bed);
    EXPECT_TRUE(bedRespawnValid(world, reg, bedSpawnPosition(0, 5, 0), 0.6f, 1.8f));
}

TEST(BedRespawnTest, AnyColorOfTheFamilyIsValid) {
    // The family check is color-blind (the kDyeBaseBed property — the
    // recolor does not move the spawn, the design's "换色不影响坐标
    // 记忆").
    BlockRegistry reg = bedRegistry("red");
    const BlockID bedRed = reg.idForName("bed_red");
    World world;
    world.getOrCreateChunk({0, 0, 0});
    world.setBlock(0, 5, 0, bedRed);
    EXPECT_TRUE(bedRespawnValid(world, reg, bedSpawnPosition(0, 5, 0), 0.6f, 1.8f));
}

TEST(BedRespawnTest, NoBedIsInvalid) {
    BlockRegistry reg = bedRegistry("white");
    World world;
    world.getOrCreateChunk({0, 0, 0});
    // Air (the broken bed — the world-spawn fallback):
    EXPECT_FALSE(bedRespawnValid(world, reg, bedSpawnPosition(0, 5, 0), 0.6f, 1.8f));
    // A non-bed solid in the cell (the replaced bed):
    world.setBlock(0, 5, 0, reg.idForName("stone"));
    EXPECT_FALSE(bedRespawnValid(world, reg, bedSpawnPosition(0, 5, 0), 0.6f, 1.8f));
}

TEST(BedRespawnTest, BlockedHeadroomIsInvalid) {
    BlockRegistry reg = bedRegistry("white");
    const BlockID bed = reg.idForName("bed");
    const BlockID stone = reg.idForName("stone");
    World world;
    world.getOrCreateChunk({0, 0, 0});
    world.setBlock(0, 5, 0, bed);
    // A stone one above the bed (the AABB's y range [5.5, 7.3] holds
    // cell 6 fully): the headroom is blocked.
    world.setBlock(0, 6, 0, stone);
    EXPECT_FALSE(bedRespawnValid(world, reg, bedSpawnPosition(0, 5, 0), 0.6f, 1.8f));
    world.setBlock(0, 6, 0, BlockID::air());
    // A stone at 7 (the AABB's top corner reaches it: 7.3 > 7):
    world.setBlock(0, 7, 0, stone);
    EXPECT_FALSE(bedRespawnValid(world, reg, bedSpawnPosition(0, 5, 0), 0.6f, 1.8f));
    world.setBlock(0, 7, 0, BlockID::air());
    // Air again: valid (the restore).
    EXPECT_TRUE(bedRespawnValid(world, reg, bedSpawnPosition(0, 5, 0), 0.6f, 1.8f));
}

// ===========================================================================
// the save format 28 (player.respawn — the int cell triple)
// ===========================================================================

TEST(BedSaveFormat28, BedPositionRoundTripsExactly) {
    // The bed-form position (the cell's center) floors exactly — the
    // round trip is value-identical.
    SaveFixture f;
    auto j = serializeSave(f.save, f.items);
    ASSERT_TRUE(j.ok()) << j.error().message;
    EXPECT_EQ(j->find("format")->asInt(), 28);
    const Json* rp = j->find("player")->find("respawn");
    ASSERT_NE(rp, nullptr);
    ASSERT_TRUE(rp->isArray());
    ASSERT_EQ(rp->size(), 3u);
    EXPECT_EQ(rp->at(0).asInt(), 3);
    EXPECT_EQ(rp->at(1).asInt(), 54);
    // The negative-floor discipline: the cell of -7.5 is -8 (the cell
    // spans [-8, -7) — the floorDiv convention, the coordinates rule).
    EXPECT_EQ(rp->at(2).asInt(), -8);
    auto out = parseSave(*j, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    EXPECT_EQ(out->playerRespawn, f.save.playerRespawn);
}

TEST(BedSaveFormat28, WorldSpawnFormFloorsToTheCell) {
    // The world-spawn-form position (the integer y — the init spawn):
    // the floor triple reconstructs the cell's CENTER (the documented
    // 0.5 y shift — the respawn validation never materializes it: the
    // cell holds no bed, the world spawn is the fallback, which is
    // authoritative).
    SaveFixture f;
    f.save.playerRespawn = {1.5f, 54.0f, 2.5f};
    auto j = serializeSave(f.save, f.items);
    ASSERT_TRUE(j.ok());
    const Json* rp = j->find("player")->find("respawn");
    ASSERT_NE(rp, nullptr);
    EXPECT_EQ(rp->at(0).asInt(), 1);
    EXPECT_EQ(rp->at(1).asInt(), 54);
    EXPECT_EQ(rp->at(2).asInt(), 2);
    auto out = parseSave(*j, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    EXPECT_EQ(out->playerRespawn, bedSpawnPosition(1, 54, 2));
}

TEST(BedSaveFormat28, Format28MissingRespawnRejected) {
    // The required-key discipline (the oxygen / velocity twin): a
    // format-28 file without the key is a version lie.
    SaveFixture f;
    auto j = serializeSave(f.save, f.items);
    ASSERT_TRUE(j.ok());
    Json root = *j;
    root["player"] = withoutKeyForTest(*root.find("player"), "respawn");
    auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
    ASSERT_FALSE(out.ok());
    EXPECT_NE(out.error().message.find("player.respawn"), std::string::npos)
        << out.error().message;
    EXPECT_NE(out.error().message.find("format 28"), std::string::npos)
        << out.error().message;
}

TEST(BedSaveFormat28, Format27CarryingRespawnRejected) {
    // The version-lie discipline (the xp_seed twin): a format-27 file
    // carrying the key is rejected — "respawn" is format-28-and-newer
    // vocabulary (the player whitelist names it).
    SaveFixture f;
    auto j = serializeSave(f.save, f.items);
    ASSERT_TRUE(j.ok());
    Json root = *j;
    root["format"] = Json(27);
    auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
    ASSERT_FALSE(out.ok());
    EXPECT_NE(out.error().message.find("respawn"), std::string::npos)
        << out.error().message;
}

TEST(BedSaveFormat28, Pre28LoadMigratesToTheSentinel) {
    // The 27 -> 28 migration: a format-27 file (no key) loads the -1
    // sentinel (the game's applySave re-pins it to the world spawn —
    // the parse cannot know that one; the xp_seed's deterministic-
    // migration twin with the game-side application).
    SaveFixture f;
    auto j = serializeSave(f.save, f.items);
    ASSERT_TRUE(j.ok());
    Json root = *j;
    root["format"] = Json(27);
    root["player"] = withoutKeyForTest(*root.find("player"), "respawn");
    auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    EXPECT_EQ(out->playerRespawn.y, -1.0f);
}

// Rebuild the respawn array with a replaced y (the const-at twin of the
// test_drown's `root["player"]["oxygen"] = ...` pattern).
Json withRespawnY(const Json& root, double y) {
    const Json* rp = root.find("player")->find("respawn");
    Json arr = Json::array();
    arr.push_back(rp->at(0));
    arr.push_back(Json(y));
    arr.push_back(rp->at(2));
    Json out = root;
    out["player"]["respawn"] = std::move(arr);
    return out;
}

TEST(BedSaveFormat28, NonIntegerComponentRejected) {
    SaveFixture f;
    auto j = serializeSave(f.save, f.items);
    ASSERT_TRUE(j.ok());
    Json root = withRespawnY(*j, 54.5); // the y is not a whole number
    auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
    ASSERT_FALSE(out.ok());
    EXPECT_NE(out.error().message.find("respawn"), std::string::npos)
        << out.error().message;
    EXPECT_NE(out.error().message.find("whole number"), std::string::npos)
        << out.error().message;
}

TEST(BedSaveFormat28, OutOfRangeYRejected) {
    SaveFixture f;
    auto j = serializeSave(f.save, f.items);
    ASSERT_TRUE(j.ok());
    for (double bad : {128.0, -1.0}) {
        Json root = withRespawnY(*j, bad);
        auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
        ASSERT_FALSE(out.ok()) << bad;
        EXPECT_NE(out.error().message.find("respawn"), std::string::npos)
            << out.error().message;
        EXPECT_NE(out.error().message.find("128"), std::string::npos)
            << out.error().message;
    }
}

TEST(BedSaveFormat28, NegativeXZAreLegal) {
    // The world is unbounded in X/Z (the player.pos twin): a negative
    // cell coordinate is a legal bed position.
    SaveFixture f;
    f.save.playerRespawn = {-10.5f, 54.5f, -30.5f};
    auto j = serializeSave(f.save, f.items);
    ASSERT_TRUE(j.ok());
    auto out = parseSave(*j, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    EXPECT_EQ(out->playerRespawn, f.save.playerRespawn);
}

// ===========================================================================
// the sleep sound (the deterministic synthesis's pin)
// ===========================================================================

TEST(BedSoundTest, SleepWavShipsAndParses) {
    // The sleep.wav (the generate_sounds.py's dedicated fixed seed — the
    // smelt-ding precedent): a 16-bit mono 44.1 kHz PCM (the bank's
    // strict format), ~0.4 s (the soft sigh's length).
    const std::filesystem::path path = "assets/sounds/sleep.wav";
    ASSERT_TRUE(std::filesystem::exists(path))
        << "run the tests from the repository root";
    std::ifstream in(path, std::ios::binary);
    ASSERT_TRUE(in);
    std::stringstream ss;
    ss << in.rdbuf();
    const std::string bytes = ss.str();
    std::vector<std::uint8_t> raw(bytes.begin(), bytes.end());
    auto sample = SoundBank::parseWav(raw);
    ASSERT_TRUE(sample.ok()) << sample.error().describe();
    EXPECT_EQ(sample->rate, 44100);
    // 0.4 s at 44100 Hz = 17640 samples (the generator's exact length).
    EXPECT_EQ(sample->pcm.size(), 17640u);
    // The tone is not silence (the synthesis produced content):
    bool any = false;
    for (const std::int16_t s : sample->pcm) {
        if (s != 0) {
            any = true;
            break;
        }
    }
    EXPECT_TRUE(any);
}

// ===========================================================================
// the tutorial vocabulary (the "sleep" bare word)
// ===========================================================================

TEST(BedTutorialTest, SleepIsABareGoal) {
    // The closed grammar accepts the new bare word (the emit side's
    // event; no series JSON uses it yet — a non-matching event is a
    // no-op, the tutorialApply invariant).
    auto r = parseTutorial(parseDef(
        R"({"steps": [{"id": "s", "text": "t", "goal": "sleep", "count": 1}]})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(r->at(0).goal, "sleep");
    // The vocabulary's size (a grammar change updates the count pin).
    EXPECT_EQ(kTutorialBareGoalCount, 4u);
}
