// Save system tests (Phase 13): the save-file round trip (serialize ->
// parse, bit-exact), the strict load validation (corrupt/mismatched files
// are rejected with descriptive errors), and the world diff/apply core
// (a save holds exactly the cells that differ from generation, and
// applying them to a freshly generated world reproduces the modified
// world cell for cell).
#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

#include "block/block_registry.hpp"
#include "core/json.hpp"
#include "core/save.hpp"
#include "core/status_effect.hpp"
#include "item/crafting_grid.hpp"
#include "item/item_registry.hpp"
#include "world/world.hpp"

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

// air (built-in) + stone + dirt + furnace.
BlockRegistry makeBlocks() {
    BlockRegistry blocks;
    // Test setup; on failure the id lookups below assert anyway.
    blocks.registerBlock(parseJson(
        R"({"id":"stone","model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    blocks.registerBlock(parseJson(
        R"({"id":"dirt","model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    // Phase 44: the furnace block — the `interact` property (the data-
    // driven "is a furnace" check) makes it a furnace block for the
    // format-7 save cross-check.
    blocks.registerBlock(parseJson(
        R"({"id":"furnace","properties":{"interact":"furnace"},"model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    return blocks;
}

// The shipped effect set (data/effects/*.json): the format-8 save
// cross-checks effect names against this registry.
EffectRegistry makeEffects() {
    EffectRegistry effects;
    const struct {
        const char* def;
    } defs[] = {
        {R"({"id":"poison","tick_type":"damage","tick_amount":1.0,
            "tick_interval":2.5,"min_health":1.0,"negative":true,"icon":0})"},
        {R"({"id":"weakness","melee_penalty":4.0,"negative":true,"icon":1})"},
        {R"({"id":"regeneration","tick_type":"heal","tick_amount":1.0,
            "tick_interval":4.5,"negative":false,"icon":2})"},
        {R"({"id":"absorption","absorb_hp":4,"negative":false,"icon":3})"},
        {R"({"id":"fire","tick_type":"damage","tick_amount":1.0,
            "tick_interval":0.2,"min_health":0.0,"negative":false,
            "extinguished_by_water":true,"ignites_on_lava":true,
            "icon":4})"},
    };
    for (const auto& d : defs) {
        auto r = effects.registerEffect(parseJson(d.def));
        if (!r.ok()) {
            ADD_FAILURE() << d.def << ": " << r.error().message;
        }
    }
    return effects;
}

struct Fixture {
    BlockRegistry blocks = makeBlocks();
    ItemRegistry items;
    EffectRegistry effects = makeEffects();

    Fixture() {
        auto r = items.buildBlockItems(blocks);
        if (!r.ok()) {
            ADD_FAILURE() << r.error().message;
        }
        auto t = items.registerItem(
            parseJson(R"({"id":"pick","type":"tool","max_stack":1})"));
        if (!t.ok()) {
            ADD_FAILURE() << t.error().message;
        }
        // Phase 40: the furnace test items (beef is smeltable -> cooked;
        // coal is fuel).
        const struct {
            const char* def;
        } furnaceDefs[] = {
            {R"({"id":"beef","type":"material","smeltable_to":"cooked"})"},
            {R"({"id":"cooked","type":"material","max_stack":64})"},
            {R"({"id":"coal","type":"material","fuel_time":80})"},
        };
        for (const auto& d : furnaceDefs) {
            auto fr = items.registerItem(parseJson(d.def));
            if (!fr.ok()) {
                ADD_FAILURE() << d.def << ": " << fr.error().message;
            }
        }
    }
};

// A complete, valid save (the round-trip and mutation tests start here).
SaveGame makeSave(const Fixture& f) {
    SaveGame s;
    s.seed = 42;
    s.time = 0.123456789; // non-binary-exact: the round trip must be bit-exact
    s.playerPos = {1.5f, 53.25f, -2.75f};
    s.playerYaw = 0.785f;
    s.playerPitch = -0.4f;
    s.selectedSlot = 3;
    s.slots.resize(Inventory::kTotalSize);
    s.slots[0] = ItemStack {f.items.idForName("stone"), 64};
    s.slots[1] = ItemStack {f.items.idForName("dirt"), 7};
    s.slots[8] = ItemStack {f.items.idForName("pick"), 1};
    s.entities.push_back({"dirt", 2, {0.5f, 52.5f, 0.5f}, 12.5, 0.25});
    s.blockEdits = {{BlockPos {0, 53, 0}, "air"},
                    {BlockPos {0, 54, -1}, "dirt"}};
    s.fluidEdits = {{BlockPos {4, 50, 4}, FluidKind::Water, 5}};
    return s;
}

// Copies a JSON object minus one key (Json has no erase).
// A genuine pre-Phase-51 file carries FOUR-element fluid edits; when a
// test downgrades the format of a serialized (format-9) save, the fluid
// edits are rewritten to the old shape (the parser is strict about the
// element count per format — a five-element edit in a format-8 file is
// corrupt).
Json downgradeFluids(const Json& root) {
    Json arr = Json::array();
    const Json* fls = root.find("fluids");
    if (fls == nullptr || !fls->isArray()) {
        return arr;
    }
    for (std::size_t i = 0; i < fls->size(); ++i) {
        const Json& e = fls->at(i);
        Json a = Json::array();
        a.push_back(e.at(0));
        a.push_back(e.at(1));
        a.push_back(e.at(2));
        a.push_back(e.at(3));
        arr.push_back(std::move(a));
    }
    return arr;
}

Json withoutKey(const Json& root, const std::string& key) {
    Json out = Json::object();
    for (const auto& [k, v] : root.members()) {
        if (k != key) {
            out[k] = v;
        }
    }
    return out;
}

} // namespace

// -- serialize/parse round trip -------------------------------------------------

TEST(SaveRoundTripTest, SerializeParsePreservesEverything) {
    Fixture f;
    const SaveGame in = makeSave(f);
    auto serialized = serializeSave(in, f.items);
    ASSERT_TRUE(serialized.ok()) << serialized.error().message;
    auto parsed = Json::parse(serialized->dump(2));
    ASSERT_TRUE(parsed.ok()) << parsed.error().message;
    auto out = parseSave(*parsed, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;

    EXPECT_EQ(out->seed, in.seed);
    EXPECT_DOUBLE_EQ(out->time, in.time);
    EXPECT_EQ(out->time, in.time); // bit-exact, not just ~equal
    EXPECT_EQ(out->playerPos, in.playerPos);
    EXPECT_FLOAT_EQ(out->playerYaw, in.playerYaw);
    EXPECT_FLOAT_EQ(out->playerPitch, in.playerPitch);
    EXPECT_EQ(out->selectedSlot, in.selectedSlot);
    ASSERT_EQ(out->slots.size(), in.slots.size());
    for (std::size_t i = 0; i < out->slots.size(); ++i) {
        EXPECT_EQ(out->slots[i], in.slots[i]) << "slot " << i;
    }
    ASSERT_EQ(out->entities.size(), in.entities.size());
    for (std::size_t i = 0; i < out->entities.size(); ++i) {
        EXPECT_EQ(out->entities[i].item, in.entities[i].item);
        EXPECT_EQ(out->entities[i].count, in.entities[i].count);
        EXPECT_EQ(out->entities[i].pos, in.entities[i].pos);
        EXPECT_DOUBLE_EQ(out->entities[i].age, in.entities[i].age);
        EXPECT_DOUBLE_EQ(out->entities[i].pickupDelay, in.entities[i].pickupDelay);
    }
    ASSERT_EQ(out->blockEdits.size(), in.blockEdits.size());
    for (std::size_t i = 0; i < out->blockEdits.size(); ++i) {
        EXPECT_EQ(out->blockEdits[i].pos, in.blockEdits[i].pos);
        EXPECT_EQ(out->blockEdits[i].blockName, in.blockEdits[i].blockName);
    }
    ASSERT_EQ(out->fluidEdits.size(), in.fluidEdits.size());
    for (std::size_t i = 0; i < out->fluidEdits.size(); ++i) {
        EXPECT_EQ(out->fluidEdits[i].pos, in.fluidEdits[i].pos);
        EXPECT_EQ(out->fluidEdits[i].level, in.fluidEdits[i].level);
    }
}

TEST(SaveRoundTripTest, SerializeRejectsUnregisteredSlotItem) {
    Fixture f;
    SaveGame s = makeSave(f);
    s.slots[2] = ItemStack {ItemID {999}, 1}; // not in the registry
    EXPECT_FALSE(serializeSave(s, f.items).ok());
}

// -- strict load validation -------------------------------------------------------

TEST(SaveParseTest, RejectsUnknownOrMissingFormat) {
    Fixture f;
    // Format 8 is a future version this build cannot read; a missing
    // format is a corrupt file. (Formats 1-7 are all accepted —
    // see FormatOneLoadsWithoutHealth.)
    Json root = parseJson(R"({"format":8,"seed":42,"time":0.25,"player":{},"inventory":{},"entities":[],"blocks":[],"fluids":[]})");
    EXPECT_FALSE(parseSave(root, f.items, f.blocks, f.effects, 42).ok());
    EXPECT_FALSE(parseSave(withoutKey(root, "format"), f.items, f.blocks, f.effects, 42).ok());
}

// -- crafting grids (format 3; Phase 17) ------------------------------------------

TEST(SaveRoundTripTest, CraftingGridsRoundTrip) {
    Fixture f;
    SaveGame s = makeSave(f);
    s.invGrid.resize(CraftingGrid::kSize);
    s.tableGrid.resize(CraftingGrid::kSize);
    s.invGrid[0] = ItemStack {f.items.idForName("dirt"), 5};
    s.invGrid[4] = ItemStack {f.items.idForName("pick"), 1};
    s.tableGrid[8] = ItemStack {f.items.idForName("stone"), 64};
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok()) << j.error().message;
    // Phase 51: writes are always format 9.
    EXPECT_EQ(j->find("format")->asInt(), 9);
    auto out = parseSave(*j, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    ASSERT_EQ(out->invGrid.size(), static_cast<std::size_t>(CraftingGrid::kSize));
    ASSERT_EQ(out->tableGrid.size(), static_cast<std::size_t>(CraftingGrid::kSize));
    for (int i = 0; i < CraftingGrid::kSize; ++i) {
        EXPECT_EQ(out->invGrid[static_cast<std::size_t>(i)],
                  s.invGrid[static_cast<std::size_t>(i)])
            << "invGrid cell " << i;
        EXPECT_EQ(out->tableGrid[static_cast<std::size_t>(i)],
                  s.tableGrid[static_cast<std::size_t>(i)])
            << "tableGrid cell " << i;
    }
}

TEST(SaveRoundTripTest, EmptyGridsRoundTrip) {
    Fixture f;
    const SaveGame s = makeSave(f); // no grids set
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok()) << j.error().message;
    auto out = parseSave(*j, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    // Format 3 always carries both grids (here: all 9 cells empty).
    EXPECT_EQ(out->invGrid.size(), static_cast<std::size_t>(CraftingGrid::kSize));
    EXPECT_EQ(out->tableGrid.size(), static_cast<std::size_t>(CraftingGrid::kSize));
    for (int i = 0; i < CraftingGrid::kSize; ++i) {
        EXPECT_TRUE(out->invGrid[static_cast<std::size_t>(i)].empty());
        EXPECT_TRUE(out->tableGrid[static_cast<std::size_t>(i)].empty());
    }
}

TEST(SaveParseTest, FormatTwoLoadsWithEmptyGrids) {
    // A format-2 file (Phase 16, pre-grids) still loads: the grids stay
    // empty (the old saves predate them).
    Fixture f;
    const SaveGame s = makeSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    Json root = *j;
    root["format"] = Json(2);
    root["fluids"] = downgradeFluids(root); // pre-Phase-51 shape
    root = withoutKey(root, "inv_grid");
    root = withoutKey(root, "table_grid");
    root = withoutKey(root, "furnaces"); // format 2 predates the furnace
    auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    EXPECT_TRUE(out->invGrid.empty());
    EXPECT_TRUE(out->tableGrid.empty());
}

TEST(SaveParseTest, FormatThreeRequiresBothGrids) {
    Fixture f;
    const SaveGame s = makeSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    Json missingInv = *j;
    missingInv = withoutKey(missingInv, "inv_grid");
    EXPECT_FALSE(parseSave(missingInv, f.items, f.blocks, f.effects, 42).ok());
    Json missingTable = *j;
    missingTable = withoutKey(missingTable, "table_grid");
    EXPECT_FALSE(parseSave(missingTable, f.items, f.blocks, f.effects, 42).ok());
}

TEST(SaveParseTest, RejectsBadCraftingGrid) {
    Fixture f;
    const SaveGame s = makeSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    auto withGrid = [&](Json grid) {
        Json root = *j;
        root["inv_grid"] = std::move(grid);
        return root;
    };
    // Wrong size (8 cells).
    {
        Json shortGrid = Json::array();
        for (int i = 0; i < CraftingGrid::kSize - 1; ++i) {
            Json e = Json::object();
            e["item"] = Json(std::string(""));
            e["count"] = Json(0);
            shortGrid.push_back(std::move(e));
        }
        EXPECT_FALSE(parseSave(withGrid(shortGrid), f.items, f.blocks, f.effects, 42).ok());
    }
    // Unknown item in a cell.
    {
        Json bad = *j;
        Json cells = bad["inv_grid"];
        Json e = Json::object();
        e["item"] = Json(std::string("nope"));
        e["count"] = Json(1);
        // Rebuild with cell 0 replaced (Json arrays have no index-set).
        Json rebuilt = Json::array();
        rebuilt.push_back(std::move(e));
        for (std::size_t i = 1; i < cells.size(); ++i) {
            rebuilt.push_back(cells.at(i));
        }
        bad["inv_grid"] = std::move(rebuilt);
        EXPECT_FALSE(parseSave(bad, f.items, f.blocks, f.effects, 42).ok());
    }
    // Count above the tool's max stack (1).
    {
        Json bad = *j;
        Json cells = bad["inv_grid"];
        Json e = Json::object();
        e["item"] = Json(std::string("pick"));
        e["count"] = Json(2);
        Json rebuilt = Json::array();
        rebuilt.push_back(std::move(e));
        for (std::size_t i = 1; i < cells.size(); ++i) {
            rebuilt.push_back(cells.at(i));
        }
        bad["table_grid"] = std::move(rebuilt);
        EXPECT_FALSE(parseSave(bad, f.items, f.blocks, f.effects, 42).ok());
    }
}

TEST(SaveRoundTripTest, HealthRoundTrips) {
    Fixture f;
    SaveGame s = makeSave(f);
    s.playerHealth = 13.5f;
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok()) << j.error().message;
    auto out = parseSave(*j, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    EXPECT_FLOAT_EQ(out->playerHealth, 13.5f);
}

TEST(SaveParseTest, FormatOneLoadsWithoutHealth) {
    // A format-1 file (Phase 13, no health field, no crafting grids)
    // still loads: the health stays "unspecified" (-1) and the game
    // applies the player's max.
    Fixture f;
    SaveGame s = makeSave(f); // playerHealth -1: the field is omitted
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok()) << j.error().message;
    Json root = *j;
    root["format"] = Json(1);
    root["fluids"] = downgradeFluids(root); // pre-Phase-51 shape
    root = withoutKey(root, "inv_grid"); // format 1 predates the grids
    root = withoutKey(root, "table_grid");
    root = withoutKey(root, "furnaces"); // format 1 predates the furnace
    auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    EXPECT_FLOAT_EQ(out->playerHealth, -1.0f);
    EXPECT_TRUE(out->invGrid.empty());
    EXPECT_TRUE(out->tableGrid.empty());
}

TEST(SaveParseTest, RejectsNegativeHealth) {
    Fixture f;
    SaveGame s = makeSave(f);
    s.playerHealth = 5.0f;
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok()) << j.error().message;
    Json root = *j;
    root["player"]["health"] = Json(-1.0);
    EXPECT_FALSE(parseSave(root, f.items, f.blocks, f.effects, 42).ok());
}

// -- hunger (format 4; Phase 23) --------------------------------------------------

TEST(SaveRoundTripTest, HungerRoundTrips) {
    Fixture f;
    SaveGame s = makeSave(f);
    s.playerHunger = 7.0f;
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok()) << j.error().message;
    auto out = parseSave(*j, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    EXPECT_FLOAT_EQ(out->playerHunger, 7.0f);
}

TEST(SaveParseTest, FormatThreeLoadsWithoutHunger) {
    // A format-3 file (Phase 22 and earlier, no hunger field) still
    // loads: the hunger stays "unspecified" (-1) and the game applies the
    // player's max.
    Fixture f;
    SaveGame s = makeSave(f);
    s.playerHealth = 12.0f;
    s.playerHunger = -1.0f; // omitted by the serializer
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok()) << j.error().message;
    Json root = *j;
    root["format"] = Json(3);
    root["fluids"] = downgradeFluids(root); // pre-Phase-51 shape
    root = withoutKey(root, "furnaces"); // format 3 predates the furnace
    ASSERT_EQ(root["player"].find("hunger"), nullptr);
    auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    EXPECT_FLOAT_EQ(out->playerHunger, -1.0f);
    EXPECT_FLOAT_EQ(out->playerHealth, 12.0f);
}

TEST(SaveParseTest, RejectsNegativeHunger) {
    Fixture f;
    SaveGame s = makeSave(f);
    s.playerHunger = 5.0f;
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok()) << j.error().message;
    Json root = *j;
    root["player"]["hunger"] = Json(-1.0);
    EXPECT_FALSE(parseSave(root, f.items, f.blocks, f.effects, 42).ok());
}

// -- saturation (format 5; Phase 28) --------------------------------------------

TEST(SaveRoundTripTest, SaturationRoundTrips) {
    Fixture f;
    SaveGame s = makeSave(f);
    s.playerSaturation = 6.0f;
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok()) << j.error().message;
    auto out = parseSave(*j, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    EXPECT_FLOAT_EQ(out->playerSaturation, 6.0f);
}

TEST(SaveParseTest, FormatFourLoadsWithoutSaturation) {
    // A format-4 file (Phase 27 and earlier, no saturation field) still
    // loads: the saturation stays "unspecified" (-1) and the game loads
    // it at 0 (no buffer — the pre-Phase-28 behavior).
    Fixture f;
    SaveGame s = makeSave(f);
    s.playerHunger = 14.0f;
    s.playerSaturation = -1.0f; // omitted by the serializer
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok()) << j.error().message;
    Json root = *j;
    root["format"] = Json(4);
    root["fluids"] = downgradeFluids(root); // pre-Phase-51 shape
    root = withoutKey(root, "furnaces"); // format 4 predates the furnace
    ASSERT_EQ(root["player"].find("saturation"), nullptr);
    auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    EXPECT_FLOAT_EQ(out->playerSaturation, -1.0f);
    EXPECT_FLOAT_EQ(out->playerHunger, 14.0f);
}

TEST(SaveParseTest, RejectsNegativeSaturation) {
    Fixture f;
    SaveGame s = makeSave(f);
    s.playerSaturation = 5.0f;
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok()) << j.error().message;
    Json root = *j;
    root["player"]["saturation"] = Json(-0.5);
    EXPECT_FALSE(parseSave(root, f.items, f.blocks, f.effects, 42).ok());
}

TEST(SaveParseTest, RejectsSeedMismatch) {
    Fixture f;
    const SaveGame s = makeSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    EXPECT_FALSE(parseSave(*j, f.items, f.blocks, f.effects, 43).ok());
}

TEST(SaveParseTest, RejectsUnknownTopLevelField) {
    Fixture f;
    const SaveGame s = makeSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    Json root = *j;
    root["extra"] = Json(1);
    EXPECT_FALSE(parseSave(root, f.items, f.blocks, f.effects, 42).ok());
}

TEST(SaveParseTest, RejectsMissingRequiredFields) {
    Fixture f;
    const SaveGame s = makeSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    for (const char* key :
         {"seed", "time", "player", "inventory", "entities", "blocks", "fluids"}) {
        EXPECT_FALSE(
            parseSave(withoutKey(*j, key), f.items, f.blocks, f.effects, 42).ok())
            << "missing '" << key << "' must be rejected";
    }
}

TEST(SaveParseTest, RejectsBadTime) {
    Fixture f;
    const SaveGame s = makeSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    Json bad = *j;
    bad["time"] = Json(1.0);
    EXPECT_FALSE(parseSave(bad, f.items, f.blocks, f.effects, 42).ok());
    bad = *j;
    bad["time"] = Json(-0.1);
    EXPECT_FALSE(parseSave(bad, f.items, f.blocks, f.effects, 42).ok());
    bad = *j;
    bad["time"] = Json(std::string("noon"));
    EXPECT_FALSE(parseSave(bad, f.items, f.blocks, f.effects, 42).ok());
}

TEST(SaveParseTest, RejectsBadPlayer) {
    Fixture f;
    const SaveGame s = makeSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    Json bad = *j;
    Json shortPos = Json::array();
    shortPos.push_back(Json(1.0));
    shortPos.push_back(Json(2.0));
    bad["player"]["pos"] = shortPos;
    EXPECT_FALSE(parseSave(bad, f.items, f.blocks, f.effects, 42).ok());
    bad = *j;
    bad["player"] = withoutKey(bad["player"], "yaw");
    EXPECT_FALSE(parseSave(bad, f.items, f.blocks, f.effects, 42).ok());
}

TEST(SaveParseTest, RejectsBadInventory) {
    Fixture f;
    const SaveGame s = makeSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());

    auto withSlots = [&](Json slots) {
        Json root = *j;
        root["inventory"]["slots"] = std::move(slots);
        return root;
    };
    // Exactly 27 slots are required.
    Json slots26 = Json::array();
    for (int i = 0; i < 26; ++i) {
        Json e = Json::object();
        e["item"] = Json(std::string(""));
        e["count"] = Json(0);
        slots26.push_back(e);
    }
    EXPECT_FALSE(parseSave(withSlots(slots26), f.items, f.blocks, f.effects, 42).ok());

    // selected must be a hotbar index.
    Json bad = *j;
    bad["inventory"]["selected"] = Json(9);
    EXPECT_FALSE(parseSave(bad, f.items, f.blocks, f.effects, 42).ok());

    // The specific slot rejections need a FULL 27-slot array with only
    // slot 0 bad (a short array is rejected for its size, not its slot).
    auto slotsWith = [&](const std::string& item, int count) {
        Json slots = Json::array();
        for (int i = 0; i < Inventory::kTotalSize; ++i) {
            Json e = Json::object();
            if (i == 0) {
                e["item"] = Json(item);
                e["count"] = Json(count);
            } else {
                e["item"] = Json(std::string(""));
                e["count"] = Json(0);
            }
            slots.push_back(std::move(e));
        }
        return slots;
    };
    // Unknown item, zero count with an item, count above the item's max
    // stack (the tool's is 1), empty item with a count.
    EXPECT_FALSE(
        parseSave(withSlots(slotsWith("nope", 1)), f.items, f.blocks, f.effects, 42).ok());
    EXPECT_FALSE(
        parseSave(withSlots(slotsWith("stone", 0)), f.items, f.blocks, f.effects, 42).ok());
    EXPECT_FALSE(
        parseSave(withSlots(slotsWith("pick", 2)), f.items, f.blocks, f.effects, 42).ok());
    EXPECT_FALSE(
        parseSave(withSlots(slotsWith("", 3)), f.items, f.blocks, f.effects, 42).ok());
    // A slot that is not an object.
    {
        Json slots = slotsWith("stone", 1);
        Json badSlot = Json(42);
        // Json arrays have no index-set: rebuild with slot 0 replaced.
        Json rebuilt = Json::array();
        rebuilt.push_back(std::move(badSlot));
        for (std::size_t i = 1; i < slots.size(); ++i) {
            rebuilt.push_back(slots.at(i));
        }
        EXPECT_FALSE(parseSave(withSlots(rebuilt), f.items, f.blocks, f.effects, 42).ok());
    }
}

TEST(SaveParseTest, RejectsBadEntity) {
    Fixture f;
    const SaveGame s = makeSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());

    auto withEntity = [&](Json e) {
        Json root = *j;
        Json arr = Json::array();
        arr.push_back(std::move(e));
        root["entities"] = std::move(arr);
        return root;
    };
    auto entity = [](const std::string& type, const std::string& item,
                     int count, double age, double delay) {
        Json e = Json::object();
        e["type"] = Json(type);
        e["item"] = Json(item);
        e["count"] = Json(count);
        Json pos = Json::array();
        pos.push_back(Json(0.5));
        pos.push_back(Json(52.5));
        pos.push_back(Json(0.5));
        e["pos"] = std::move(pos);
        e["age"] = Json(age);
        e["pickup_delay"] = Json(delay);
        return e;
    };
    EXPECT_FALSE(parseSave(withEntity(entity("projectile", "dirt", 1, 1.0, 0.1)),
                           f.items, f.blocks, f.effects, 42)
                     .ok());
    EXPECT_FALSE(
        parseSave(withEntity(entity("item", "nope", 1, 1.0, 0.1)), f.items,
                  f.blocks, f.effects, 42)
            .ok());
    EXPECT_FALSE(
        parseSave(withEntity(entity("item", "dirt", 0, 1.0, 0.1)), f.items,
                  f.blocks, f.effects, 42)
            .ok());
    EXPECT_FALSE(
        parseSave(withEntity(entity("item", "dirt", 65, 1.0, 0.1)), f.items,
                  f.blocks, f.effects, 42)
            .ok());
    EXPECT_FALSE(
        parseSave(withEntity(entity("item", "pick", 2, 1.0, 0.1)), f.items,
                  f.blocks, f.effects, 42)
            .ok()); // above the tool's max stack
    EXPECT_FALSE(
        parseSave(withEntity(entity("item", "dirt", 1, 120.0, 0.1)), f.items,
                  f.blocks, f.effects, 42)
            .ok()); // at the despawn age
    EXPECT_FALSE(
        parseSave(withEntity(entity("item", "dirt", 1, 1.0, 0.6)), f.items,
                  f.blocks, f.effects, 42)
            .ok()); // beyond the pickup delay
}

TEST(SaveParseTest, RejectsBadBlockEdit) {
    Fixture f;
    const SaveGame s = makeSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());

    auto withBlock = [&](Json edit) {
        Json root = *j;
        Json arr = Json::array();
        arr.push_back(std::move(edit));
        root["blocks"] = std::move(arr);
        return root;
    };
    auto edit = [](int x, int y, int z, const std::string& name) {
        Json a = Json::array();
        a.push_back(Json(x));
        a.push_back(Json(y));
        a.push_back(Json(z));
        a.push_back(Json(name));
        return a;
    };
    EXPECT_FALSE(parseSave(withBlock(edit(0, 128, 0, "stone")), f.items, f.blocks, f.effects, 42).ok());
    EXPECT_FALSE(parseSave(withBlock(edit(0, -1, 0, "stone")), f.items, f.blocks, f.effects, 42).ok());
    EXPECT_FALSE(parseSave(withBlock(edit(0, 10, 0, "nope")), f.items, f.blocks, f.effects, 42).ok());
    {
        Json shortEdit = Json::array();
        shortEdit.push_back(Json(0));
        shortEdit.push_back(Json(10));
        shortEdit.push_back(Json(0));
        EXPECT_FALSE(parseSave(withBlock(shortEdit), f.items, f.blocks, f.effects, 42).ok());
    }
    {
        // Duplicate positions.
        Json root = *j;
        Json arr = Json::array();
        arr.push_back(edit(1, 10, 1, "stone"));
        arr.push_back(edit(1, 10, 1, "dirt"));
        root["blocks"] = std::move(arr);
        EXPECT_FALSE(parseSave(root, f.items, f.blocks, f.effects, 42).ok());
    }
}

TEST(SaveParseTest, RejectsBadFluidEdit) {
    Fixture f;
    const SaveGame s = makeSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());

    auto withFluids = [&](std::vector<Json> edits) {
        Json root = *j;
        Json arr = Json::array();
        for (auto& e : edits) {
            arr.push_back(std::move(e));
        }
        root["fluids"] = std::move(arr);
        return root;
    };
    auto edit = [](int x, int y, int z, Json level,
                   const std::string& kind = "water") {
        Json a = Json::array();
        a.push_back(Json(x));
        a.push_back(Json(y));
        a.push_back(Json(z));
        a.push_back(std::move(level));
        a.push_back(Json(kind));
        return a;
    };
    EXPECT_FALSE(
        parseSave(withFluids({edit(0, 10, 0, Json(9))}), f.items, f.blocks, f.effects, 42).ok());
    EXPECT_FALSE(parseSave(withFluids({edit(0, 10, 0, Json(2.5))}), f.items,
                          f.blocks, f.effects, 42)
                     .ok());
    EXPECT_FALSE(
        parseSave(withFluids({edit(1, 10, 1, Json(3)), edit(1, 10, 1, Json(4))}),
                  f.items, f.blocks, f.effects, 42)
            .ok()); // duplicate cell
    // Format 9: a four-element edit (the pre-Phase-51 shape) is a size
    // error, an unknown fluid name is a vocabulary error, and a
    // non-string kind is a type error.
    {
        Json shortEdit = Json::array();
        shortEdit.push_back(Json(1));
        shortEdit.push_back(Json(10));
        shortEdit.push_back(Json(1));
        shortEdit.push_back(Json(3));
        EXPECT_FALSE(parseSave(withFluids({shortEdit}), f.items, f.blocks,
                               f.effects, 42)
                         .ok());
    }
    EXPECT_FALSE(
        parseSave(withFluids({edit(1, 10, 1, Json(3), "slime")}), f.items,
                  f.blocks, f.effects, 42)
            .ok());
    {
        Json badKind = Json::array();
        badKind.push_back(Json(1));
        badKind.push_back(Json(10));
        badKind.push_back(Json(1));
        badKind.push_back(Json(3));
        badKind.push_back(Json(7)); // a number, not a kind name
        EXPECT_FALSE(parseSave(withFluids({badKind}), f.items, f.blocks,
                               f.effects, 42)
                         .ok());
    }
    // A block edit and a fluid edit on the same cell: a block displaces
    // fluid, so the save would be corrupt.
    {
        Json root = *j;
        Json barr = Json::array();
        barr.push_back(edit(1, 10, 1, "stone"));
        root["blocks"] = std::move(barr);
        root["fluids"] =
            withFluids({edit(1, 10, 1, Json(3))})["fluids"];
        EXPECT_FALSE(parseSave(root, f.items, f.blocks, f.effects, 42).ok());
    }
}

TEST(SaveParseTest, FluidEditKindRoundTripsAndMigrates) {
    // Phase 51: the fluid kind on fluid edits (format 9). A lava edit
    // survives the round trip (five elements, kind named); a format-8
    // four-element edit migrates to water (the only simulated fluid
    // before Phase 51).
    Fixture f;
    const SaveGame s = makeSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());

    Json root = *j;
    Json lavaEdit = Json::array();
    lavaEdit.push_back(Json(2));
    lavaEdit.push_back(Json(10));
    lavaEdit.push_back(Json(2));
    lavaEdit.push_back(Json(5));
    lavaEdit.push_back(Json("lava"));
    Json arr = Json::array();
    arr.push_back(std::move(lavaEdit));
    root["fluids"] = std::move(arr);
    auto parsed = parseSave(root, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(parsed.ok()) << parsed.error().message;
    ASSERT_EQ(parsed->fluidEdits.size(), 1u);
    EXPECT_EQ(parsed->fluidEdits[0].kind, FluidKind::Lava);
    EXPECT_EQ(parsed->fluidEdits[0].level, 5);
    auto reser = serializeSave(*parsed, f.items);
    ASSERT_TRUE(reser.ok());
    const Json& out = reser->find("fluids")->at(0);
    ASSERT_EQ(out.size(), 5u);
    EXPECT_EQ(out.at(4).asString(), "lava");

    root["format"] = Json(8);
    Json waterEdit = Json::array();
    waterEdit.push_back(Json(3));
    waterEdit.push_back(Json(10));
    waterEdit.push_back(Json(3));
    waterEdit.push_back(Json(4));
    Json arr2 = Json::array();
    arr2.push_back(std::move(waterEdit));
    root["fluids"] = std::move(arr2);
    auto migrated = parseSave(root, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(migrated.ok()) << migrated.error().message;
    ASSERT_EQ(migrated->fluidEdits.size(), 1u);
    EXPECT_EQ(migrated->fluidEdits[0].kind, FluidKind::Water);
    EXPECT_EQ(migrated->fluidEdits[0].level, 4);
}

// -- world diff / apply -------------------------------------------------------------

namespace {

// One generated chunk: stone below y=4, fluid 8 at (0,5,0) and (1,5,0).
World::ChunkGenerator referenceGenerator(BlockID stone) {
    return [stone](Chunk& c, const ChunkPos&) {
        for (int y = 0; y < 4; ++y) {
            for (int z = 0; z < Chunk::kSize; ++z) {
                for (int x = 0; x < Chunk::kSize; ++x) {
                    c.set(x, y, z, stone);
                }
            }
        }
        c.setFluidAt(0, 5, 0, fluidCode(FluidKind::Water, kMaxFluidLevel));
        c.setFluidAt(1, 5, 0, fluidCode(FluidKind::Water, kMaxFluidLevel));
    };
}

World makeWorld(BlockID stone) {
    World world;
    world.setChunkGenerator(referenceGenerator(stone));
    world.getOrCreateChunk({0, 0, 0});
    return world;
}

} // namespace

TEST(SaveWorldDiffTest, DiffWorldDetectsExactlyTheChanges) {
    Fixture f;
    const BlockID stone = f.blocks.idForName("stone");
    const BlockID dirt = f.blocks.idForName("dirt");
    World world = makeWorld(stone);
    // The modifications: dig one stone, place dirt in air, thin one
    // water cell, drain the other.
    ASSERT_TRUE(world.setBlock(0, 0, 0, BlockID::air()));
    ASSERT_TRUE(world.setBlock(2, 5, 0, dirt));
    ASSERT_TRUE(world.setFluid(0, 5, 0, FluidKind::Water, 3));
    ASSERT_TRUE(world.setFluid(1, 5, 0, FluidKind::Water, 0));

    std::vector<SavedBlockEdit> blockEdits;
    std::vector<SavedFluidEdit> fluidEdits;
    auto r = diffWorld(world, referenceGenerator(stone), f.blocks, blockEdits,
                       fluidEdits);
    ASSERT_TRUE(r.ok()) << r.error().message;
    ASSERT_EQ(blockEdits.size(), 2u);
    EXPECT_EQ(blockEdits[0].pos, (BlockPos {0, 0, 0}));
    EXPECT_EQ(blockEdits[0].blockName, "air");
    EXPECT_EQ(blockEdits[1].pos, (BlockPos {2, 5, 0}));
    EXPECT_EQ(blockEdits[1].blockName, "dirt");
    ASSERT_EQ(fluidEdits.size(), 2u);
    EXPECT_EQ(fluidEdits[0].pos, (BlockPos {0, 5, 0}));
    EXPECT_EQ(fluidEdits[0].kind, FluidKind::Water);
    EXPECT_EQ(fluidEdits[0].level, 3);
    EXPECT_EQ(fluidEdits[1].pos, (BlockPos {1, 5, 0}));
    EXPECT_EQ(fluidEdits[1].kind, FluidKind::Water);
    EXPECT_EQ(fluidEdits[1].level, 0);
}

TEST(SaveWorldDiffTest, DiffWorldOfUntouchedWorldIsEmpty) {
    Fixture f;
    const BlockID stone = f.blocks.idForName("stone");
    World world = makeWorld(stone);
    std::vector<SavedBlockEdit> blockEdits;
    std::vector<SavedFluidEdit> fluidEdits;
    auto r = diffWorld(world, referenceGenerator(stone), f.blocks, blockEdits,
                       fluidEdits);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_TRUE(blockEdits.empty());
    EXPECT_TRUE(fluidEdits.empty());
}

// Phase 37: above the parallel threshold the diff runs one chunk per job
// on a throwaway pool — the position-sorted output must be identical to
// the serial path.
TEST(SaveWorldDiffTest, ParallelDiffMatchesSerialDiff) {
    Fixture f;
    const BlockID stone = f.blocks.idForName("stone");
    const BlockID dirt = f.blocks.idForName("dirt");
    // 16 x 17 = 272 chunks > kSaveDiffParallelMinChunks (256).
    World world;
    world.setChunkGenerator(referenceGenerator(stone));
    for (int x = 0; x < 16; ++x) {
        for (int z = 0; z < 17; ++z) {
            world.getOrCreateChunk({x, 0, z});
        }
    }
    // Edits spread over three different chunks, plus a fluid thinning.
    ASSERT_TRUE(world.setBlock(0, 0, 0, BlockID::air()));
    ASSERT_TRUE(world.setBlock(8 * 16, 3, 8 * 16, dirt));
    ASSERT_TRUE(world.setBlock(15 * 16 + 15, 5, 16 * 16 + 15, stone));
    ASSERT_TRUE(world.setFluid(1, 5, 0, FluidKind::Water, 3));

    std::vector<SavedBlockEdit> serialBlocks;
    std::vector<SavedFluidEdit> serialFluids;
    auto r = diffWorld(world, referenceGenerator(stone), f.blocks,
                       serialBlocks, serialFluids);
    ASSERT_TRUE(r.ok()) << r.error().message;
    ASSERT_EQ(serialBlocks.size(), 3u);
    ASSERT_EQ(serialFluids.size(), 1u); // only the thinned (1,5,0) differs

    std::vector<SavedBlockEdit> parallelBlocks;
    std::vector<SavedFluidEdit> parallelFluids;
    auto p = diffWorld(world, referenceGenerator(stone), f.blocks,
                       parallelBlocks, parallelFluids, kSaveDiffWorkers);
    ASSERT_TRUE(p.ok()) << p.error().message;
    ASSERT_EQ(parallelBlocks.size(), serialBlocks.size());
    for (std::size_t i = 0; i < serialBlocks.size(); ++i) {
        EXPECT_EQ(parallelBlocks[i].pos, serialBlocks[i].pos);
        EXPECT_EQ(parallelBlocks[i].blockName, serialBlocks[i].blockName);
    }
    ASSERT_EQ(parallelFluids.size(), serialFluids.size());
    for (std::size_t i = 0; i < serialFluids.size(); ++i) {
        EXPECT_EQ(parallelFluids[i].pos, serialFluids[i].pos);
        EXPECT_EQ(parallelFluids[i].kind, serialFluids[i].kind);
        EXPECT_EQ(parallelFluids[i].level, serialFluids[i].level);
    }
}

// Phase 37: workers > 1 on a small world must fall back to the serial
// path and still produce the correct diff.
TEST(SaveWorldDiffTest, SmallWorldIgnoresTheWorkerCount) {
    Fixture f;
    const BlockID stone = f.blocks.idForName("stone");
    const BlockID dirt = f.blocks.idForName("dirt");
    World world = makeWorld(stone);
    ASSERT_TRUE(world.setBlock(2, 5, 0, dirt));

    std::vector<SavedBlockEdit> single;
    std::vector<SavedFluidEdit> singleFluids;
    auto r = diffWorld(world, referenceGenerator(stone), f.blocks, single,
                       singleFluids);
    ASSERT_TRUE(r.ok()) << r.error().message;
    ASSERT_EQ(single.size(), 1u);

    std::vector<SavedBlockEdit> multi;
    std::vector<SavedFluidEdit> multiFluids;
    auto p = diffWorld(world, referenceGenerator(stone), f.blocks, multi,
                       multiFluids, kSaveDiffWorkers);
    ASSERT_TRUE(p.ok()) << p.error().message;
    ASSERT_EQ(multi.size(), 1u);
    EXPECT_EQ(multi[0].pos, single[0].pos);
    EXPECT_EQ(multi[0].blockName, single[0].blockName);
    EXPECT_TRUE(multiFluids.empty());
}

TEST(SaveWorldDiffTest, ApplyRestoresTheModifiedWorld) {
    Fixture f;
    const BlockID stone = f.blocks.idForName("stone");
    const BlockID dirt = f.blocks.idForName("dirt");
    World world = makeWorld(stone);
    ASSERT_TRUE(world.setBlock(0, 0, 0, BlockID::air()));
    ASSERT_TRUE(world.setBlock(2, 5, 0, dirt));
    ASSERT_TRUE(world.setFluid(0, 5, 0, FluidKind::Water, 3));
    ASSERT_TRUE(world.setFluid(1, 5, 0, FluidKind::Water, 0));

    SaveGame save;
    save.seed = 42;
    auto diff = diffWorld(world, referenceGenerator(stone), f.blocks,
                          save.blockEdits, save.fluidEdits);
    ASSERT_TRUE(diff.ok()) << diff.error().message;

    // A FRESHLY generated world + the edits == the modified world, for
    // every cell.
    World fresh = makeWorld(stone);
    auto applied = applyWorldEdits(save, f.blocks, fresh);
    ASSERT_TRUE(applied.ok()) << applied.error().message;
    for (int y = 0; y < kWorldHeight; ++y) {
        for (int z = 0; z < 16; ++z) {
            for (int x = 0; x < 16; ++x) {
                EXPECT_EQ(fresh.getBlock(x, y, z), world.getBlock(x, y, z))
                    << "block at " << x << ", " << y << ", " << z;
                EXPECT_EQ(fresh.getFluid(x, y, z), world.getFluid(x, y, z))
                    << "fluid at " << x << ", " << y << ", " << z;
            }
        }
    }
}

TEST(SaveWorldDiffTest, ApplyFailsWhenFluidCellIsOccupied) {
    Fixture f;
    const BlockID stone = f.blocks.idForName("stone");
    World world = makeWorld(stone);
    SaveGame save;
    // (0, 2, 0) is generated stone — a fluid edit there cannot apply.
    save.fluidEdits = {{BlockPos {0, 2, 0}, FluidKind::Water, 5}};
    auto r = applyWorldEdits(save, f.blocks, world);
    EXPECT_FALSE(r.ok());
}

TEST(SaveWorldDiffTest, ApplyFailsWhenTheChunkDoesNotExist) {
    Fixture f;
    const BlockID stone = f.blocks.idForName("stone");
    World world = makeWorld(stone); // only chunk (0, 0, 0)
    SaveGame save;
    // x=64 is chunk (4, 0, 0) — never created, and no block edit creates
    // it either.
    save.fluidEdits = {{BlockPos {64, 5, 0}, FluidKind::Water, 3}};
    auto r = applyWorldEdits(save, f.blocks, world);
    EXPECT_FALSE(r.ok());
}

// -- per-block furnace states (format 7; Phase 44) ----------------------------
//
// makeSave has no furnace blocks; these tests add them. The format-7
// cross-check is BIDIRECTIONAL: exactly one entry per furnace block in
// the block edits (entries without a block and blocks without an entry
// are both rejected).

// A save with two furnace blocks (position-sorted, as the game's map
// writes them) and the matching (here empty) entries.
SaveGame makeFurnaceSave(const Fixture& f) {
    SaveGame s = makeSave(f);
    s.blockEdits.push_back({BlockPos {1, 53, 1}, "furnace"});
    s.blockEdits.push_back({BlockPos {2, 53, 1}, "furnace"});
    s.furnaces.push_back({BlockPos {1, 53, 1}, FurnaceState {}});
    s.furnaces.push_back({BlockPos {2, 53, 1}, FurnaceState {}});
    return s;
}

TEST(SaveRoundTripTest, FurnacesRoundTrip) {
    Fixture f;
    SaveGame s = makeFurnaceSave(f);
    // The first furnace holds a mid-smelt state; the second stays empty
    // (per-block independence — the empty entry is saved too).
    s.furnaces[0].state.input = ItemStack {f.items.idForName("beef"), 3};
    s.furnaces[0].state.fuel = ItemStack {f.items.idForName("coal"), 1};
    s.furnaces[0].state.output = ItemStack {f.items.idForName("cooked"), 2};
    s.furnaces[0].state.progressSubsteps = 300;
    s.furnaces[0].state.fuelSubsteps = 1234;
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok()) << j.error().message;
    // Phase 51: writes are always format 9.
    EXPECT_EQ(j->find("format")->asInt(), 9);
    auto out = parseSave(*j, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    ASSERT_EQ(out->furnaces.size(), 2u);
    const BlockPos p1 {1, 53, 1};
    const BlockPos p2 {2, 53, 1};
    EXPECT_EQ(out->furnaces[0].pos, p1);
    EXPECT_EQ(out->furnaces[1].pos, p2);
    EXPECT_EQ(out->furnaces[0].state.input, s.furnaces[0].state.input);
    EXPECT_EQ(out->furnaces[0].state.fuel, s.furnaces[0].state.fuel);
    EXPECT_EQ(out->furnaces[0].state.output, s.furnaces[0].state.output);
    EXPECT_EQ(out->furnaces[0].state.progressSubsteps, 300);
    EXPECT_EQ(out->furnaces[0].state.fuelSubsteps, 1234);
    EXPECT_TRUE(out->furnaces[1].state.empty());
}

TEST(SaveRoundTripTest, EmptyFurnacesRoundTrip) {
    Fixture f;
    const SaveGame s = makeSave(f); // no furnace blocks
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    // Always written (here: the empty array).
    ASSERT_TRUE(j->find("furnaces") != nullptr);
    EXPECT_EQ(j->find("furnaces")->size(), 0u);
    auto out = parseSave(*j, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    EXPECT_TRUE(out->furnaces.empty());
}

TEST(SaveParseTest, FormatFiveLoadsWithEmptyFurnaces) {
    // A format-5 file (Phase 39, pre-furnace) still loads: the world's
    // furnace blocks load with empty states.
    Fixture f;
    SaveGame s = makeFurnaceSave(f);
    s.furnaces[0].state.input = ItemStack {f.items.idForName("beef"), 3};
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    Json root = *j;
    root["format"] = Json(5);
    root["fluids"] = downgradeFluids(root); // pre-Phase-51 shape
    root = withoutKey(root, "furnaces"); // format 5 predates the furnace
    auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    ASSERT_EQ(out->furnaces.size(), 2u); // the blocks' entries, empty states
    EXPECT_TRUE(out->furnaces[0].state.empty());
    EXPECT_TRUE(out->furnaces[1].state.empty());
}

TEST(SaveParseTest, FormatSixMigratesStateToSoleFurnace) {
    // The Phase 44 migration: a format-6 file's ONE global state
    // belonged to the world's (single) furnace — with exactly one
    // furnace block it inherits the state.
    Fixture f;
    SaveGame s = makeSave(f);
    s.blockEdits.push_back({BlockPos {1, 53, 1}, "furnace"});
    s.furnaces.push_back({BlockPos {1, 53, 1}, FurnaceState {}});
    s.furnaces[0].state.input = ItemStack {f.items.idForName("beef"), 3};
    s.furnaces[0].state.fuel = ItemStack {f.items.idForName("coal"), 1};
    s.furnaces[0].state.progressSubsteps = 300;
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    Json root = *j;
    root["format"] = Json(6);
    root["fluids"] = downgradeFluids(root); // pre-Phase-51 shape
    root["furnace"] = withoutKey(root["furnaces"].at(0), "pos");
    root = withoutKey(root, "furnaces");
    auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    ASSERT_EQ(out->furnaces.size(), 1u);
    const BlockPos sole {1, 53, 1};
    EXPECT_EQ(out->furnaces[0].pos, sole);
    const ItemStack beef = ItemStack {f.items.idForName("beef"), 3};
    const ItemStack coal = ItemStack {f.items.idForName("coal"), 1};
    EXPECT_EQ(out->furnaces[0].state.input, beef);
    EXPECT_EQ(out->furnaces[0].state.fuel, coal);
    EXPECT_EQ(out->furnaces[0].state.progressSubsteps, 300);
}

TEST(SaveParseTest, FormatSixMigrationDiscardsWithoutSoleFurnace) {
    // The Phase 44 migration, part 2: with SEVERAL furnace blocks the
    // owner of the global state is unknowable — it is discarded (all
    // states load empty).
    Fixture f;
    SaveGame s = makeFurnaceSave(f); // two furnace blocks
    s.furnaces[0].state.input = ItemStack {f.items.idForName("beef"), 3};
    s.furnaces[0].state.progressSubsteps = 300;
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    Json root = *j;
    root["format"] = Json(6);
    root["fluids"] = downgradeFluids(root); // pre-Phase-51 shape
    root["furnace"] = withoutKey(root["furnaces"].at(0), "pos");
    root = withoutKey(root, "furnaces");
    auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    ASSERT_EQ(out->furnaces.size(), 2u);
    EXPECT_TRUE(out->furnaces[0].state.empty());
    EXPECT_TRUE(out->furnaces[1].state.empty());
    // ...and with ZERO furnace blocks there is nowhere to put it.
    {
        SaveGame s2 = makeSave(f); // no furnace blocks
        auto j2 = serializeSave(s2, f.items);
        ASSERT_TRUE(j2.ok());
        Json root2 = *j2;
        root2["format"] = Json(6);
        root2["fluids"] = downgradeFluids(root2); // pre-Phase-51 shape
        root2 = withoutKey(root2, "furnaces");
        Json fz = Json::object();
        Json in = Json::object();
        in["item"] = Json(std::string("beef"));
        in["count"] = Json(3);
        Json empty = Json::object();
        empty["item"] = Json(std::string(""));
        empty["count"] = Json(0);
        fz["input"] = in;
        fz["fuel"] = empty;
        fz["output"] = empty;
        fz["progress_substeps"] = Json(300);
        fz["fuel_substeps"] = Json(0);
        root2["furnace"] = fz;
        auto out2 = parseSave(root2, f.items, f.blocks, f.effects, 42);
        ASSERT_TRUE(out2.ok()) << out2.error().message;
        EXPECT_TRUE(out2->furnaces.empty());
    }
}

TEST(SaveParseTest, RejectsFurnaceFieldInOldFormat) {
    Fixture f;
    SaveGame s = makeSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    Json root = *j;
    root["format"] = Json(5); // keeps the "furnaces" field
    root["fluids"] = downgradeFluids(root); // pre-Phase-51 shape
    EXPECT_FALSE(parseSave(root, f.items, f.blocks, f.effects, 42).ok());
}

TEST(SaveParseTest, RejectsFurnaceFieldInFormatSeven) {
    // A format-7 file must not carry the format-6 "furnace" object (the
    // per-block "furnaces" array replaced it).
    Fixture f;
    SaveGame s = makeFurnaceSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    Json root = *j;
    root["furnace"] = withoutKey(root["furnaces"].at(0), "pos");
    EXPECT_FALSE(parseSave(root, f.items, f.blocks, f.effects, 42).ok());
}

TEST(SaveParseTest, RejectsFurnacesFieldInFormatSix) {
    // A format-6 file must not carry the format-7 "furnaces" array.
    Fixture f;
    SaveGame s = makeFurnaceSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    Json root = *j;
    root["format"] = Json(6); // keeps the "furnaces" field
    root["fluids"] = downgradeFluids(root); // pre-Phase-51 shape
    EXPECT_FALSE(parseSave(root, f.items, f.blocks, f.effects, 42).ok());
}

TEST(SaveParseTest, RejectsFurnaceEntryBlockMismatch) {
    // The bidirectional cross-check: an entry whose position holds no
    // furnace block, or a furnace block with no entry.
    Fixture f;
    {
        SaveGame s = makeSave(f);
        s.blockEdits.push_back({BlockPos {1, 53, 1}, "furnace"});
        s.furnaces.push_back({BlockPos {5, 53, 5}, FurnaceState {}});
        auto j = serializeSave(s, f.items);
        ASSERT_TRUE(j.ok());
        EXPECT_FALSE(parseSave(*j, f.items, f.blocks, f.effects, 42).ok());
    }
    {
        SaveGame s = makeFurnaceSave(f);
        s.furnaces.pop_back(); // the second block has no entry
        auto j = serializeSave(s, f.items);
        ASSERT_TRUE(j.ok());
        EXPECT_FALSE(parseSave(*j, f.items, f.blocks, f.effects, 42).ok());
    }
}

TEST(SaveParseTest, RejectsDuplicateFurnaceEntries) {
    Fixture f;
    SaveGame s = makeSave(f);
    s.blockEdits.push_back({BlockPos {1, 53, 1}, "furnace"});
    s.furnaces.push_back({BlockPos {1, 53, 1}, FurnaceState {}});
    s.furnaces.push_back({BlockPos {1, 53, 1}, FurnaceState {}});
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    EXPECT_FALSE(parseSave(*j, f.items, f.blocks, f.effects, 42).ok());
}

TEST(SaveParseTest, RejectsBadFurnacePos) {
    Fixture f;
    const char* emptyEntry =
        R"([{"pos":[1,53,1],"input":{"item":"","count":0},"fuel":{"item":"","count":0},"output":{"item":"","count":0},"progress_substeps":0,"fuel_substeps":0}])";
    SaveGame s = makeSave(f);
    s.blockEdits.push_back({BlockPos {1, 53, 1}, "furnace"});
    s.furnaces.push_back({BlockPos {1, 53, 1}, FurnaceState {}});
    auto base = serializeSave(s, f.items);
    ASSERT_TRUE(base.ok());
    // y outside the world height.
    {
        Json root = *base;
        Json bad = parseJson(
            std::string(emptyEntry).replace(11, 2, "200"));
        root["furnaces"] = bad;
        EXPECT_FALSE(parseSave(root, f.items, f.blocks, f.effects, 42).ok());
    }
    // A non-integer coordinate (the x at index 9).
    {
        Json root = *base;
        Json bad = parseJson(
            std::string(emptyEntry).replace(9, 1, "1.5"));
        root["furnaces"] = bad;
        EXPECT_FALSE(parseSave(root, f.items, f.blocks, f.effects, 42).ok());
    }
    // The wrong arity.
    {
        Json root = *base;
        Json bad = parseJson(
            R"([{"pos":[1,53],"input":{"item":"","count":0},"fuel":{"item":"","count":0},"output":{"item":"","count":0},"progress_substeps":0,"fuel_substeps":0}])");
        root["furnaces"] = bad;
        EXPECT_FALSE(parseSave(root, f.items, f.blocks, f.effects, 42).ok());
    }
}

TEST(SaveParseTest, RejectsProgressWithoutSmeltableInput) {
    // The progress invariant (furnace.hpp): progress > 0 implies the
    // input is a smeltable item — a save violating it would resurrect
    // progress the tick/UI rules say was lost.
    Fixture f;
    SaveGame s = makeSave(f);
    s.blockEdits.push_back({BlockPos {1, 53, 1}, "furnace"});
    s.furnaces.push_back({BlockPos {1, 53, 1}, FurnaceState {}});
    s.furnaces[0].state.progressSubsteps = 10;
    s.furnaces[0].state.input =
        ItemStack {f.items.idForName("stone"), 1}; // not smeltable
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    EXPECT_FALSE(parseSave(*j, f.items, f.blocks, f.effects, 42).ok());
    s.furnaces[0].state.input = ItemStack {}; // and with an empty input
    j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    EXPECT_FALSE(parseSave(*j, f.items, f.blocks, f.effects, 42).ok());
}

TEST(SaveParseTest, RejectsBadFurnaceFields) {
    Fixture f;
    SaveGame s = makeSave(f);
    s.blockEdits.push_back({BlockPos {1, 53, 1}, "furnace"});
    s.furnaces.push_back({BlockPos {1, 53, 1}, FurnaceState {}});
    s.furnaces[0].state.input = ItemStack {f.items.idForName("beef"), 3};
    s.furnaces[0].state.progressSubsteps = 10;
    auto base = serializeSave(s, f.items);
    ASSERT_TRUE(base.ok());
    // A format-7 file without the "furnaces" array.
    {
        Json root = withoutKey(*base, "furnaces");
        EXPECT_FALSE(parseSave(root, f.items, f.blocks, f.effects, 42).ok());
    }
    // An unknown field inside a furnace entry.
    {
        Json root = *base;
        Json entry = root["furnaces"].at(0);
        entry["extra"] = Json(1);
        Json arr = Json::array();
        arr.push_back(std::move(entry));
        root["furnaces"] = std::move(arr);
        EXPECT_FALSE(parseSave(root, f.items, f.blocks, f.effects, 42).ok());
    }
    // Out-of-range / non-integer counters.
    const struct {
        const char* field;
        double value;
    } counters[] = {
        {"progress_substeps", -1},
        {"progress_substeps", 100.0 * 60.0 + 1},
        {"fuel_substeps", 1000.0 * 60.0 + 1},
        {"fuel_substeps", 0.5},
    };
    for (const auto& c : counters) {
        Json root = *base;
        Json entry = root["furnaces"].at(0);
        entry[c.field] = Json(c.value);
        Json arr = Json::array();
        arr.push_back(std::move(entry));
        root["furnaces"] = std::move(arr);
        EXPECT_FALSE(parseSave(root, f.items, f.blocks, f.effects, 42).ok()) << c.field;
    }
    // A missing slot.
    {
        Json root = *base;
        Json arr = Json::array();
        arr.push_back(withoutKey(root["furnaces"].at(0), "fuel"));
        root["furnaces"] = std::move(arr);
        EXPECT_FALSE(parseSave(root, f.items, f.blocks, f.effects, 42).ok());
    }
}

// -- format 8: the player's active status effects (Phase 46) ---------------

TEST(SaveFormat8Test, EffectsRoundTripNameSorted) {
    Fixture f;
    SaveGame s = makeSave(f);
    // Unsorted input: the serializer writes name-sorted, and the parse
    // normalizes to the same order (bit-exact round trip).
    s.playerEffects = {{"regeneration", 12.5},
                       {"poison", 3.25},
                       {"weakness", 30.0}};
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok()) << j.error().message;
    const Json& arr = *j->find("player")->find("effects");
    ASSERT_EQ(arr.size(), 3u);
    EXPECT_EQ(arr.at(0).find("name")->asString(), "poison");
    EXPECT_EQ(arr.at(1).find("name")->asString(), "regeneration");
    EXPECT_EQ(arr.at(2).find("name")->asString(), "weakness");
    auto out = parseSave(*j, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    ASSERT_EQ(out->playerEffects.size(), 3u);
    EXPECT_EQ(out->playerEffects[0].name, "poison");
    EXPECT_DOUBLE_EQ(out->playerEffects[0].remaining, 3.25);
    EXPECT_EQ(out->playerEffects[1].name, "regeneration");
    EXPECT_DOUBLE_EQ(out->playerEffects[1].remaining, 12.5);
    EXPECT_EQ(out->playerEffects[2].name, "weakness");
    EXPECT_DOUBLE_EQ(out->playerEffects[2].remaining, 30.0);
}

TEST(SaveFormat8Test, EmptyEffectsArrayIsValid) {
    Fixture f;
    SaveGame s = makeSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    ASSERT_TRUE(j->find("player")->find("effects") != nullptr);
    EXPECT_EQ(j->find("player")->find("effects")->size(), 0u);
    auto out = parseSave(*j, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    EXPECT_TRUE(out->playerEffects.empty());
}

TEST(SaveFormat8Test, FormatSevenFileWithoutEffectsLoads) {
    // Format 1-7 files predate the effects: the key is ABSENT (not
    // required) and the effects load empty.
    Fixture f;
    SaveGame s = makeSave(f);
    s.playerEffects = {{"poison", 5.0}};
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    Json root = *j;
    root["format"] = Json(7);
    root["fluids"] = downgradeFluids(root); // pre-Phase-51 shape
    root["player"] = withoutKey(*j->find("player"), "effects");
    auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    EXPECT_TRUE(out->playerEffects.empty());
}

TEST(SaveFormat8Test, FormatEightRequiresTheEffectsKey) {
    Fixture f;
    SaveGame s = makeSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    Json root = *j;
    root["player"] = withoutKey(*j->find("player"), "effects");
    EXPECT_FALSE(parseSave(root, f.items, f.blocks, f.effects, 42).ok());
}

TEST(SaveFormat8Test, RejectsBadEffectEntries) {
    Fixture f;
    SaveGame s = makeSave(f);
    s.playerEffects = {{"poison", 5.0}};
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());

    auto withEffect = [&](Json entry) {
        Json root = *j;
        Json arr = Json::array();
        arr.push_back(std::move(entry));
        root["player"]["effects"] = std::move(arr);
        return root;
    };
    const auto entry = [&]() {
        Json e = Json::object();
        e["name"] = Json(std::string("poison"));
        e["remaining"] = Json(5.0);
        return e;
    };
    // An unregistered effect name.
    {
        Json e = entry();
        e["name"] = Json(std::string("nope"));
        EXPECT_FALSE(
            parseSave(withEffect(std::move(e)), f.items, f.blocks,
                      f.effects, 42)
                .ok());
    }
    // remaining <= 0.
    for (double rem : {0.0, -1.0}) {
        Json e = entry();
        e["remaining"] = Json(rem);
        EXPECT_FALSE(
            parseSave(withEffect(std::move(e)), f.items, f.blocks,
                      f.effects, 42)
                .ok()) << rem;
    }
    // A duplicate name.
    {
        Json root = *j;
        Json arr = Json::array();
        arr.push_back(entry());
        arr.push_back(entry());
        root["player"]["effects"] = std::move(arr);
        EXPECT_FALSE(parseSave(root, f.items, f.blocks, f.effects, 42).ok());
    }
    // An unknown field inside the entry.
    {
        Json e = entry();
        e["extra"] = Json(1);
        EXPECT_FALSE(
            parseSave(withEffect(std::move(e)), f.items, f.blocks,
                      f.effects, 42)
                .ok());
    }
    // A non-object entry.
    {
        EXPECT_FALSE(
            parseSave(withEffect(Json(1)), f.items, f.blocks, f.effects, 42)
                .ok());
    }
}

// -- Phase 56: the absorption layer and the saved health ----------------------

TEST(SaveFormat8Test, HealthAboveBaseWithAbsorptionLayerRoundTrips) {
    // A save taken while the absorption layer is active stores a health
    // ABOVE the base max (24 with the 4-HP layer). The parse accepts it
    // (save.cpp has no upper bound; the GAME clamps against the effective
    // max after applying the layer — see Game::applySave 4e); the
    // round trip must preserve the value.
    Fixture f;
    SaveGame s = makeSave(f);
    s.playerHealth = 24.0f;
    s.playerEffects = {{"absorption", 3.25}};
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok()) << j.error().message;
    auto out = parseSave(*j, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    EXPECT_FLOAT_EQ(out->playerHealth, 24.0f);
    ASSERT_EQ(out->playerEffects.size(), 1u);
    EXPECT_EQ(out->playerEffects[0].name, "absorption");
    EXPECT_DOUBLE_EQ(out->playerEffects[0].remaining, 3.25);
}

// -- Phase 57: the fire effect in a save ------------------------------------

TEST(SaveFormat8Test, FireEffectRoundTrips) {
    // A save taken while burning stores the fire like any effect; the
    // reload continues the burn (the remaining counts down from the saved
    // value — the lingering burn survives a save/load).
    Fixture f;
    SaveGame s = makeSave(f);
    s.playerHealth = 13.0f;
    s.playerEffects = {{"fire", 2.5}};
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok()) << j.error().message;
    auto out = parseSave(*j, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    EXPECT_FLOAT_EQ(out->playerHealth, 13.0f);
    ASSERT_EQ(out->playerEffects.size(), 1u);
    EXPECT_EQ(out->playerEffects[0].name, "fire");
    EXPECT_DOUBLE_EQ(out->playerEffects[0].remaining, 2.5);
}
