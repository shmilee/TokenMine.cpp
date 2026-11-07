// V2-12 (v2.13.0): the save format 27 tests — the player.xp_seed (a
// full-64-bit value stored as a hex string, always written, the
// deterministic pre-27 migration) + the per-stack "enchants" object in the
// THREE stack-carrying places (inventory slots / armor / item entities,
// omit-when-empty). The three-way key discipline (a format-26 file carrying
// xp_seed OR enchants is a version lie; a format-27 file missing xp_seed is
// rejected) + the game-load cross-validation (validateEnchants: unknown id,
// level > max, class mismatch, incompatible pair).
#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "block/block_registry.hpp"
#include "core/enchanting.hpp"
#include "core/json.hpp"
#include "core/save.hpp"
#include "core/status_effect.hpp"
#include "player/drown.hpp" // kOxygenMax
#include "item/inventory.hpp"
#include "item/item.hpp"
#include "item/item_registry.hpp"

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

// Remove `key` from a JSON object (the test_save.cpp withoutKey twin,
// re-declared here — it is file-local there).
Json withoutKeyForTest(const Json& obj, const std::string& key) {
    Json out = Json::object();
    for (const auto& [k, v] : obj.members()) {
        if (k != key) {
            out[k] = v;
        }
    }
    return out;
}
// V3-1 Phase B: the downgrade's format-30 key strip (the test_save.cpp
// stripFormat31Keys twin — the genuine pre-30 file never carried the
// format-30 keys: player.dimension / nether / nether_entities).
// V3-12c2 (format 33): the serialized document's per-player seam
// (the root "player"'s relocated twin — players[0] + the nested
// "inventory" / "inv_grid"); the old-format simulation's shape
// conversion (the test_save's legacyShape twin).
const Json* primaryEntry(const Json& doc) {
    const Json* players = doc.find("players");
    return (players != nullptr && players->isArray() && players->size() > 0)
               ? &players->at(0)
               : nullptr;
}
Json legacyShape(Json doc) {
    const Json* players = doc.find("players");
    if (players == nullptr || !players->isArray() || players->size() == 0) {
        return doc;
    }
    Json entry = players->at(0);
    doc = withoutKeyForTest(doc, "players");
    doc = withoutKeyForTest(doc, "end_xp_entities");
    if (const Json* inv = entry.find("inventory")) {
        doc["inventory"] = *inv;
        entry = withoutKeyForTest(entry, "inventory");
    }
    if (const Json* g = entry.find("inv_grid")) {
        doc["inv_grid"] = *g;
        entry = withoutKeyForTest(entry, "inv_grid");
    }
    doc["player"] = std::move(entry);
    // V3-12c2: the respawn's int cell's RE-ENCODE (the format-33
    // document's exact float's legacy (the 32-and-below) shape — the
    // old writer's floor's twin; the 32-and-below parse's cell-
    // center's reconstruction's premise).
    if (const Json* pl = doc.find("player");
        pl != nullptr && pl->isObject()) {
        const Json* rp = pl->find("respawn");
        if (rp != nullptr && rp->isArray() && rp->size() == 3) {
            Json cell = Json::array();
            for (std::size_t i = 0; i < 3; ++i) {
                cell.push_back(Json(std::floor(rp->at(i).asDouble())));
            }
            Json plOut = withoutKeyForTest(*pl, "respawn");
            plOut["respawn"] = std::move(cell);
            doc = withoutKeyForTest(doc, "player");
            doc["player"] = std::move(plOut);
        }
    }
    return doc;
}

Json stripFormat31Keys(Json root) {
    root = legacyShape(std::move(root)); // V3-12c2: the 33 shape's revert
    root = withoutKeyForTest(root, "nether");
    root = withoutKeyForTest(root, "nether_entities");
    // V3-10a: the format-32 keys join the strip (the test_save twin —
    // a genuine pre-32 file never carried end / end_entities).
    root = withoutKeyForTest(root, "end");
    root = withoutKeyForTest(root, "end_entities");
    if (const Json* player = root.find("player");
        player != nullptr && player->isObject() &&
        player->find("dimension") != nullptr) {
        Json p = withoutKeyForTest(*player, "dimension");
        root = withoutKeyForTest(root, "player");
        root["player"] = std::move(p);
    }
    // V3-11a: the format-31 creative key strip (the dimension twin).
    if (const Json* player = root.find("player");
        player != nullptr && player->isObject() &&
        player->find("creative") != nullptr) {
        Json p = withoutKeyForTest(*player, "creative");
        root = withoutKeyForTest(root, "player");
        root["player"] = std::move(p);
    }
    return root;
}


BlockRegistry makeBlocks() {
    BlockRegistry blocks;
    blocks.registerBlock(parseDef(
        R"({"id":"stone","model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    return blocks;
}

EffectRegistry makeEffects() {
    EffectRegistry e;
    e.registerEffect(parseDef(
        R"({"id":"poison","tick_type":"damage","tick_amount":1.0,"tick_interval":2.5,"min_health":1.0,"negative":true,"icon":0})"));
    return e;
}

// A small enchant registry: the shipped effects + one incompatible PAIR
// (for the cross-validation reject) — "sharpness" and "smite" are
// incompatible (a synthetic pair, the shipped seven have none).
EnchantmentRegistry makeEnchants() {
    EnchantmentRegistry reg;
    const char* defs[] = {
        R"({"id":"sharpness","name":"锋利","effect":"sharpness","classes":["axe"],"weight":10,"ecost":[[1,11]]})",
        R"({"id":"protection","name":"保护","effect":"protection","classes":["armor"],"weight":10,"ecost":[[5,12]]})",
        R"({"id":"unbreaking","name":"耐久","effect":"unbreaking","classes":["axe","armor"],"weight":5,"ecost":[[5,12],[13,20]]})",
        R"({"id":"smite","name":"击打","effect":"sharpness","classes":["axe"],"weight":1,"ecost":[[1,11]],"incompatible":["sharpness"]})",
        R"({"id":"sharpness2","name":"锋利","effect":"sharpness","classes":["axe"],"weight":1,"ecost":[[1,11]],"incompatible":["smite"]})",
    };
    for (const char* d : defs) {
        auto r = reg.registerEnchantment(parseDef(d));
        if (!r.ok()) {
            throw std::runtime_error(r.error().message);
        }
    }
    return reg;
}

struct Fixture {
    BlockRegistry blocks = makeBlocks();
    EffectRegistry effects = makeEffects();
    ItemRegistry items;
    EnchantmentRegistry enchants = makeEnchants();
    ItemID axe;
    ItemID chest;

    Fixture() {
        auto b = items.buildBlockItems(blocks);
        if (!b.ok()) ADD_FAILURE() << b.error().message;
        auto a = items.registerItem(parseDef(
            R"({"id":"axe","type":"tool","max_stack":1,"tool":{"break_speed":2.0},"enchant_class":"axe","enchantability":14})"));
        if (!a.ok()) ADD_FAILURE() << a.error().message;
        axe = *a;
        auto c = items.registerItem(parseDef(
            R"({"id":"chest","type":"material","max_stack":1,"armor_slot":"chest","armor_points":5,"durability":100,"enchant_class":"armor","enchantability":9})"));
        if (!c.ok()) ADD_FAILURE() << c.error().message;
        chest = *c;
    }
};

// A minimal valid SaveGame (a format-27 shape).
SaveGame makeSave(std::uint64_t xpSeed) {
    SaveGame s;
    s.seed = 42;
    s.time = 0.25;
    s.playerPos = {1.5f, 53.0f, 2.5f};
    s.playerAbsorption = 0.0f;
    s.playerOxygen = kOxygenMax;
    s.playerXp = 19;
    s.playerRespawn = {3.5f, 54.5f, -7.5f}; // V2-13 (format 28 always writes it)
    s.playerXpSeed = xpSeed;
    s.slots.resize(Inventory::kTotalSize);
    s.invGrid.resize(CraftingGrid::kSize);
    return s;
}

} // namespace

// ===========================================================================
// xp_seed (the full-64-bit hex-string round trip + the migration)
// ===========================================================================

TEST(SaveFormat27, XpSeedRoundTripsLosslessly) {
    Fixture f;
    // A value FAR above 2^53 (the double mantissa) — a numeric field would
    // lose it; the hex string must not.
    const std::uint64_t seed = 0x5EED123456789ABCULL;
    SaveGame s = makeSave(seed);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok()) << j.error().message;
    // Stored as a 16-char lowercase hex string.
    const Json* xs = primaryEntry(*j)->find("xp_seed");
    ASSERT_NE(xs, nullptr);
    ASSERT_TRUE(xs->isString());
    EXPECT_EQ(xs->asString(), "5eed123456789abc");
    // The round trip is exact.
    auto out = parseSave(*j, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    EXPECT_EQ(out->playerXpSeed, seed);
}

TEST(SaveFormat27, Pre27LoadMigratesXpSeed) {
    Fixture f;
    SaveGame s = makeSave(0x5EED123456789ABCULL);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    Json root = *j;
    // Downgrade to format 26 + strip the format-27 keys (the pre-27 shape:
    // no xp_seed, no enchants).
    root = stripFormat31Keys(std::move(root));
    root["format"] = Json(26);
    if (const Json* player = root.find("player")) {
        Json p = *player;
        if (player->find("xp_seed") != nullptr) {
            p = withoutKeyForTest(p, "xp_seed");
        }
        if (player->find("respawn") != nullptr) {
            p = withoutKeyForTest(p, "respawn"); // V2-13: format 28
        }
        root["player"] = std::move(p);
    }
    auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    // The deterministic migration: worldSeed ^ kXpSeedInit.
    EXPECT_EQ(out->playerXpSeed,
              static_cast<std::uint64_t>(42) ^ kXpSeedInit);
    // V2-13: the pre-28 respawn migration is the sentinel (the game's
    // applySave re-pins it to the world spawn — the parse cannot know it).
    EXPECT_EQ(out->playerRespawn.y, -1.0f);
}

TEST(SaveFormat27, Format27MissingXpSeedRejected) {
    Fixture f;
    SaveGame s = makeSave(0x1234ULL);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    // V3-12c2: the entry's xp_seed (the format stays 33 — the
    // required-key message names "player.xp_seed", the hardcoded
    // form).
    Json root = *j;
    {
        const Json* players = root.find("players");
        Json entry = players->at(0);
        entry = withoutKeyForTest(entry, "xp_seed");
        Json arr = Json::array();
        arr.push_back(std::move(entry));
        root = withoutKeyForTest(root, "players");
        root["players"] = std::move(arr);
    }
    auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
    ASSERT_FALSE(out.ok());
    EXPECT_NE(out.error().message.find("xp_seed"), std::string::npos);
}

TEST(SaveFormat27, Format26CarryingXpSeedRejected) {
    Fixture f;
    SaveGame s = makeSave(0x1234ULL);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    Json root = *j;
    // Downgrade to format 26 but KEEP xp_seed -> a version lie (the key is
    // format-27 vocabulary; the player whitelist rejects it). The V2-13
    // respawn key (format 28) is stripped so the xp_seed lie is the one
    // named (the whitelist reports the alphabetically first unknown).
    root = stripFormat31Keys(std::move(root));
    root["format"] = Json(26);
    if (const Json* player = root.find("player")) {
        Json p = *player;
        if (player->find("respawn") != nullptr) {
            p = withoutKeyForTest(p, "respawn");
        }
        root["player"] = std::move(p);
    }
    auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
    ASSERT_FALSE(out.ok());
    EXPECT_NE(out.error().message.find("xp_seed"), std::string::npos);
}

// ===========================================================================
// the per-stack "enchants" object (three places, omit-when-empty)
// ===========================================================================

TEST(SaveFormat27, SlotEnchantsRoundTrip) {
    Fixture f;
    SaveGame s = makeSave(0xABCDEFULL);
    // An enchanted axe in slot 0.
    ItemStack stack {f.axe, 1};
    stack.enchants["sharpness"] = 5;
    stack.enchants["unbreaking"] = 2;
    s.slots[0] = stack;
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok()) << j.error().message;
    // The "enchants" object is written (the id-sorted map).
    const Json& slot0 = primaryEntry(*j)->find("inventory")->find("slots")->at(0);
    ASSERT_NE(slot0.find("enchants"), nullptr);
    EXPECT_EQ(slot0.find("enchants")->find("sharpness")->asInt(), 5);
    EXPECT_EQ(slot0.find("enchants")->find("unbreaking")->asInt(), 2);
    auto out = parseSave(*j, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    EXPECT_EQ(out->slots[0].enchants.at("sharpness"), 5);
    EXPECT_EQ(out->slots[0].enchants.at("unbreaking"), 2);
    // An EMPTY slot carries NO "enchants" key (the omit-when-empty).
    const Json& slot1 = primaryEntry(*j)->find("inventory")->find("slots")->at(1);
    EXPECT_EQ(slot1.find("enchants"), nullptr);
}

TEST(SaveFormat27, UnenchantedSlotHasNoEnchantsKey) {
    Fixture f;
    SaveGame s = makeSave(0x1ULL);
    s.slots[0] = ItemStack {f.axe, 1}; // no enchants
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    const Json& slot0 = primaryEntry(*j)->find("inventory")->find("slots")->at(0);
    EXPECT_EQ(slot0.find("enchants"), nullptr); // byte-identical pre-27 shape
}

TEST(SaveFormat27, ArmorEnchantsRoundTrip) {
    Fixture f;
    SaveGame s = makeSave(0x2ULL);
    s.playerArmor[1] = "chest"; // the chest slot
    s.playerArmorDamage[1] = 10.0f; // the object shape (damaged)
    s.playerArmorEnchants[1]["protection"] = 3;
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok()) << j.error().message;
    const Json* armor = primaryEntry(*j)->find("armor");
    ASSERT_NE(armor, nullptr);
    ASSERT_NE(armor->find("chest"), nullptr);
    const Json& entry = *armor->find("chest");
    ASSERT_TRUE(entry.isObject());
    EXPECT_EQ(entry.find("enchants")->find("protection")->asInt(), 3);
    auto out = parseSave(*j, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    EXPECT_EQ(out->playerArmorEnchants[1].at("protection"), 3);
}

TEST(SaveFormat27, FullDurabilityEnchantedArmorUsesObjectShape) {
    Fixture f;
    SaveGame s = makeSave(0x3ULL);
    s.playerArmor[1] = "chest";
    s.playerArmorDamage[1] = 0.0f; // FULL durability
    s.playerArmorEnchants[1]["protection"] = 4; // but enchanted
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok()) << j.error().message;
    // The object shape (damage absent, enchants present) — NOT the string.
    const Json& entry = *primaryEntry(*j)->find("armor")->find("chest");
    ASSERT_TRUE(entry.isObject());
    EXPECT_EQ(entry.find("damage"), nullptr); // omit-when-0 (full)
    EXPECT_EQ(entry.find("enchants")->find("protection")->asInt(), 4);
    auto out = parseSave(*j, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    EXPECT_EQ(out->playerArmorEnchants[1].at("protection"), 4);
    EXPECT_FLOAT_EQ(out->playerArmorDamage[1], 0.0f);
}

TEST(SaveFormat27, EntityEnchantsRoundTrip) {
    Fixture f;
    SaveGame s = makeSave(0x4ULL);
    SavedItemEntity se;
    se.item = "axe";
    se.count = 1;
    se.pos = {3.5f, 53.5f, 3.5f};
    se.age = 1.0;
    se.pickupDelay = 0.0;
    se.enchants["sharpness"] = 3;
    s.entities.push_back(se);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok()) << j.error().message;
    const Json& e0 = j->find("entities")->at(0);
    EXPECT_EQ(e0.find("enchants")->find("sharpness")->asInt(), 3);
    auto out = parseSave(*j, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    EXPECT_EQ(out->entities[0].enchants.at("sharpness"), 3);
}

TEST(SaveFormat27, Format26CarryingEnchantsRejected) {
    Fixture f;
    SaveGame s = makeSave(0x5ULL);
    ItemStack stack {f.axe, 1};
    stack.enchants["sharpness"] = 1;
    s.slots[0] = stack;
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    Json root = *j;
    // Downgrade to format 26 but KEEP the slot's enchants -> a version lie.
    root = stripFormat31Keys(std::move(root));
    root["format"] = Json(26);
    if (const Json* player = root.find("player")) {
        Json p = *player;
        if (player->find("xp_seed") != nullptr) {
            p = withoutKeyForTest(p, "xp_seed");
        }
        if (player->find("respawn") != nullptr) {
            p = withoutKeyForTest(p, "respawn"); // V2-13: format 28
        }
        root["player"] = std::move(p);
    }
    auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
    ASSERT_FALSE(out.ok());
    EXPECT_NE(out.error().message.find("enchants"), std::string::npos);
}

// ===========================================================================
// the game-load cross-validation (validateEnchants)
// ===========================================================================

TEST(SaveFormat27, ValidateRejectsUnknownEnchantId) {
    Fixture f;
    SaveGame s = makeSave(0x6ULL);
    ItemStack stack {f.axe, 1};
    stack.enchants["bogus"] = 1;
    s.slots[0] = stack;
    auto r = validateEnchants(s, f.items, f.enchants);
    EXPECT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("unknown enchantment"), std::string::npos);
}

TEST(SaveFormat27, ValidateRejectsLevelAboveMax) {
    Fixture f;
    SaveGame s = makeSave(0x7ULL);
    ItemStack stack {f.axe, 1};
    stack.enchants["sharpness"] = 99; // the fixture's sharpness max is 1
    s.slots[0] = stack;
    auto r = validateEnchants(s, f.items, f.enchants);
    EXPECT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("above its max"), std::string::npos);
}

TEST(SaveFormat27, ValidateRejectsClassMismatch) {
    Fixture f;
    SaveGame s = makeSave(0x8ULL);
    // protection (an armor enchant) on the AXE (class axe) -> mismatch.
    ItemStack stack {f.axe, 1};
    stack.enchants["protection"] = 1;
    s.slots[0] = stack;
    auto r = validateEnchants(s, f.items, f.enchants);
    EXPECT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("does not apply"), std::string::npos);
}

TEST(SaveFormat27, ValidateRejectsEnchantOnClasslessItem) {
    Fixture f;
    SaveGame s = makeSave(0x9ULL);
    // The "stone" block item has no enchant class -> any enchant is corrupt.
    ItemStack stack {f.items.idForName("stone"), 1};
    stack.enchants["sharpness"] = 1;
    s.slots[0] = stack;
    auto r = validateEnchants(s, f.items, f.enchants);
    EXPECT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("no enchant class"), std::string::npos);
}

TEST(SaveFormat27, ValidateRejectsIncompatiblePair) {
    Fixture f;
    SaveGame s = makeSave(0xAULL);
    ItemStack stack {f.axe, 1};
    stack.enchants["sharpness"] = 1;
    stack.enchants["smite"] = 1; // incompatible with sharpness (the fixture)
    s.slots[0] = stack;
    auto r = validateEnchants(s, f.items, f.enchants);
    EXPECT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("incompatible"), std::string::npos);
}

TEST(SaveFormat27, ValidateAcceptsASaneEnchant) {
    Fixture f;
    SaveGame s = makeSave(0xBULL);
    ItemStack stack {f.axe, 1};
    stack.enchants["sharpness"] = 1; // within the fixture's max, class axe
    s.slots[0] = stack;
    auto r = validateEnchants(s, f.items, f.enchants);
    EXPECT_TRUE(r.ok()) << r.error().message;
}
