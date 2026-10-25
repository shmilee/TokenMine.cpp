// Save system tests (Phase 13): the save-file round trip (serialize ->
// parse, bit-exact), the strict load validation (corrupt/mismatched files
// are rejected with descriptive errors), and the world diff/apply core
// (a save holds exactly the cells that differ from generation, and
// applying them to a freshly generated world reproduces the modified
// world cell for cell).
#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "block/block_registry.hpp"
#include "core/json.hpp"
#include "core/save.hpp"
#include "core/status_effect.hpp"
#include "item/crafting_grid.hpp"
#include "item/item_registry.hpp"
#include "player/drown.hpp"
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
    // Phase 64: the fire block — the block edits that reference "fire"
    // need it registered for the format-10 fires cross-check.
    blocks.registerBlock(parseJson(
        R"({"id":"fire","properties":{"solid":false,"opaque":false,"transparent":true},"model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    // Phase 94 / ENT-2: the stair block — the block edits that reference
    // "stone_stairs" need it registered for the format-15 states cross-
    // check (hasState(): the Stairs shape is the per-instance state's gate,
    // count 4, default = stair_facing).
    blocks.registerBlock(parseJson(
        R"({"id":"stone_stairs","model":{"shape":"stairs","textures":{"top":"t","side":"t","bottom":"t"}}})"));
    // GEN-2: the first real stateful block — the oak sapling (8 stages,
    // the tree object) + its tree references. The block edits / states
    // cross-checks need them registered.
    blocks.registerBlock(parseJson(
        R"({"id":"wood","model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    blocks.registerBlock(parseJson(
        R"({"id":"leaves","properties":{"solid":false,"opaque":false,"transparent":true},)"
        R"("model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    blocks.registerBlock(parseJson(
        R"({"id":"oak_sapling","properties":{"solid":false,"opaque":false,"transparent":true,"growth_ticks":120,"tree":{"trunk":"wood","leaves":"leaves","min_height":4,"height_range":3,"leaf_radius":2,"shape":"oak"}},)"
        R"("model":{"shape":"cross","state_count":8,"state_default":0,"textures":{"cross":"t"}}})"));
    // CRAFT-1: the crafting_table block — the `interact` property (the
    // data-driven "is a table" check) makes it a table block for the
    // format-19 save cross-check.
    blocks.registerBlock(parseJson(
        R"({"id":"crafting_table","properties":{"interact":"crafting_table"},"model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
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
        // SURV-1: the armor pieces the format-16 save cross-checks (one
        // per slot; the item's data armor_slot must match the save's
        // key, so a piece filed under the wrong slot is rejected).
        const struct {
            const char* def;
        } armorDefs[] = {
            {R"({"id":"iron_helmet","type":"material","max_stack":1,"armor_slot":"head","armor_points":2})"},
            {R"({"id":"leather_chestplate","type":"material","max_stack":1,"armor_slot":"chest","armor_points":3})"},
            {R"({"id":"diamond_leggings","type":"material","max_stack":1,"armor_slot":"legs","armor_points":3})"},
            {R"({"id":"diamond_boots","type":"material","max_stack":1,"armor_slot":"feet","armor_points":2})"},
        };
        for (const auto& d : armorDefs) {
            auto ar = items.registerItem(parseJson(d.def));
            if (!ar.ok()) {
                ADD_FAILURE() << d.def << ": " << ar.error().message;
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
    s.playerAbsorption = 0.0f; // Phase 72 (no layer in the fixture)
    s.playerOxygen = kOxygenMax; // SURV-2 (no dive in the fixture)
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

// CRAFT-1: the "table_grid" array a pre-format-19 file carries (required
// in format 3-18): all 9 slots empty — the shape a downgrade of a
// current save must restore for the legacy parse to accept it.
Json emptyTableGrid() {
    Json arr = Json::array();
    for (int i = 0; i < CraftingGrid::kSize; ++i) {
        Json e = Json::object();
        e["item"] = Json(std::string(""));
        e["count"] = Json(0);
        arr.push_back(std::move(e));
    }
    return arr;
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
    s.invGrid[0] = ItemStack {f.items.idForName("dirt"), 5};
    s.invGrid[4] = ItemStack {f.items.idForName("pick"), 1};
    // CRAFT-1: the table's grid is per-block — a crafting_table block
    // edit + its "tables" entry (the E panel's grid stays global).
    s.blockEdits.push_back({BlockPos {1, 53, 2}, "crafting_table"});
    std::sort(s.blockEdits.begin(), s.blockEdits.end(),
              [](const SavedBlockEdit& a, const SavedBlockEdit& b) {
                  return a.pos < b.pos;
              });
    CraftingGrid table;
    table.setSlot(8, ItemStack {f.items.idForName("stone"), 64});
    s.tables.push_back(SavedTable {BlockPos {1, 53, 2}, table});
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok()) << j.error().message;
    // CRAFT-1: writes are always format 19 (SURV-3 was format 18).
    EXPECT_EQ(j->find("format")->asInt(), 19);
    auto out = parseSave(*j, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    ASSERT_EQ(out->invGrid.size(), static_cast<std::size_t>(CraftingGrid::kSize));
    ASSERT_EQ(out->tables.size(), 1u);
    EXPECT_EQ(out->tables[0].pos, (BlockPos {1, 53, 2}));
    for (int i = 0; i < CraftingGrid::kSize; ++i) {
        EXPECT_EQ(out->invGrid[static_cast<std::size_t>(i)],
                  s.invGrid[static_cast<std::size_t>(i)])
            << "invGrid cell " << i;
        EXPECT_EQ(out->tables[0].grid.at(i), table.at(i))
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
    // Format 3 always carries the inv grid (here: all 9 cells empty);
    // CRAFT-1: the table grids are the "tables" array — empty (the
    // fixture world holds no crafting_table blocks).
    EXPECT_EQ(out->invGrid.size(), static_cast<std::size_t>(CraftingGrid::kSize));
    for (int i = 0; i < CraftingGrid::kSize; ++i) {
        EXPECT_TRUE(out->invGrid[static_cast<std::size_t>(i)].empty());
    }
    EXPECT_TRUE(out->tables.empty());
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
    root = withoutKey(root, "fires"); // format 2 predates the fire
    root = withoutKey(root, "tables"); // format 2 predates the per-block table grids
    root = withoutKey(root, "campfires"); // format 2 predates the campfire
    root = withoutKey(root, "states"); // format 2 predates per-block state
    root["player"] = withoutKey(root["player"], "absorption"); // format 2 predates the absorption pool
    root["player"] = withoutKey(root["player"], "armor"); // SURV-1: format below 16 predates the player's armor
    root["player"] = withoutKey(root["player"], "oxygen"); // SURV-2: format below 17 predates the player's oxygen
    root["fluids"] = downgradeFluids(root); // pre-Phase-51 shape
    root = withoutKey(root, "inv_grid");
    root = withoutKey(root, "table_grid");
    root = withoutKey(root, "furnaces"); // format 2 predates the furnace
    auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    EXPECT_TRUE(out->invGrid.empty());
    EXPECT_TRUE(out->tables.empty()); // CRAFT-1: no table blocks in the fixture world
}

TEST(SaveParseTest, FormatThreeRequiresBothGrids) {
    Fixture f;
    const SaveGame s = makeSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    // Format 19 (CRAFT-1): the E panel's grid ("inv_grid") and the
    // per-block table arrays ("tables") are both required keys.
    Json missingInv = *j;
    missingInv = withoutKey(missingInv, "inv_grid");
    EXPECT_FALSE(parseSave(missingInv, f.items, f.blocks, f.effects, 42).ok());
    Json missingTables = *j;
    missingTables = withoutKey(missingTables, "tables");
    EXPECT_FALSE(parseSave(missingTables, f.items, f.blocks, f.effects, 42).ok());
    // Legacy (format 3): the table's grid was the global "table_grid"
    // array — a format-3 file still requires it.
    Json legacy = *j;
    legacy["format"] = Json(3);
    legacy = withoutKey(legacy, "fires"); // format 3 predates the fire
    legacy = withoutKey(legacy, "tables"); // format 3 predates the per-block table grids
    legacy = withoutKey(legacy, "campfires"); // format 3 predates the campfire
    legacy = withoutKey(legacy, "states"); // format 3 predates per-block state
    legacy["player"] = withoutKey(legacy["player"], "absorption"); // format 3 predates the absorption pool
    legacy["player"] = withoutKey(legacy["player"], "armor"); // SURV-1: format below 16 predates the player's armor
    legacy["player"] = withoutKey(legacy["player"], "oxygen"); // SURV-2: format below 17 predates the player's oxygen
    legacy["fluids"] = downgradeFluids(legacy); // pre-Phase-51 shape
    legacy = withoutKey(legacy, "furnaces"); // format 3 predates the furnace
    Json missingTable = withoutKey(legacy, "table_grid");
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
        bad["inv_grid"] = std::move(rebuilt);
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
    root = withoutKey(root, "fires"); // format 1 predates the fire
    root = withoutKey(root, "tables"); // format 1 predates the per-block table grids
    root = withoutKey(root, "campfires"); // format 1 predates the campfire
    root = withoutKey(root, "states"); // format 1 predates per-block state
    root["player"] = withoutKey(root["player"], "absorption"); // format 1 predates the absorption pool
    root["player"] = withoutKey(root["player"], "armor"); // SURV-1: format below 16 predates the player's armor
    root["player"] = withoutKey(root["player"], "oxygen"); // SURV-2: format below 17 predates the player's oxygen
    root["fluids"] = downgradeFluids(root); // pre-Phase-51 shape
    root = withoutKey(root, "inv_grid"); // format 1 predates the grids
    root = withoutKey(root, "table_grid");
    root = withoutKey(root, "furnaces"); // format 1 predates the furnace
    auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    EXPECT_FLOAT_EQ(out->playerHealth, -1.0f);
    EXPECT_TRUE(out->invGrid.empty());
    EXPECT_TRUE(out->tables.empty()); // CRAFT-1: no table blocks in the fixture world
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
    root = withoutKey(root, "fires"); // format 3 predates the fire
    root = withoutKey(root, "tables"); // format 3 predates the per-block table grids
    root = withoutKey(root, "campfires"); // format 3 predates the campfire
    root = withoutKey(root, "states"); // format 3 predates per-block state
    root["player"] = withoutKey(root["player"], "absorption"); // format 3 predates the absorption pool
    root["player"] = withoutKey(root["player"], "armor"); // SURV-1: format below 16 predates the player's armor
    root["player"] = withoutKey(root["player"], "oxygen"); // SURV-2: format below 17 predates the player's oxygen
    root["fluids"] = downgradeFluids(root); // pre-Phase-51 shape
    root = withoutKey(root, "furnaces"); // format 3 predates the furnace
    root["table_grid"] = emptyTableGrid(); // CRAFT-1: format 3 still carried the global table grid
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
    root = withoutKey(root, "fires"); // format 4 predates the fire
    root = withoutKey(root, "tables"); // format 4 predates the per-block table grids
    root = withoutKey(root, "campfires"); // format 4 predates the campfire
    root = withoutKey(root, "states"); // format 4 predates per-block state
    root["player"] = withoutKey(root["player"], "absorption"); // format 4 predates the absorption pool
    root["player"] = withoutKey(root["player"], "armor"); // SURV-1: format below 16 predates the player's armor
    root["player"] = withoutKey(root["player"], "oxygen"); // SURV-2: format below 17 predates the player's oxygen
    root["fluids"] = downgradeFluids(root); // pre-Phase-51 shape
    root = withoutKey(root, "furnaces"); // format 4 predates the furnace
    root["table_grid"] = emptyTableGrid(); // CRAFT-1: format 4 still carried the global table grid
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
    // A block edit and a fluid edit on the same cell: a solid block and
    // a positive fluid in one cell is an impossible live state (a block
    // displaces fluid), so the save is corrupt.
    auto blockEdit = [](int x, int y, int z, const std::string& name) {
        Json a = Json::array();
        a.push_back(Json(x));
        a.push_back(Json(y));
        a.push_back(Json(z));
        a.push_back(Json(name));
        return a;
    };
    {
        Json root = *j;
        Json barr = Json::array();
        barr.push_back(blockEdit(1, 10, 1, "stone"));
        root["blocks"] = std::move(barr);
        root["fluids"] =
            withFluids({edit(1, 10, 1, Json(3))})["fluids"];
        auto parsed = parseSave(root, f.items, f.blocks, f.effects, 42);
        EXPECT_FALSE(parsed.ok());
        if (!parsed.ok()) {
            EXPECT_NE(parsed.error().message.find("same cell"), std::string::npos);
        }
    }
    // The legal pairs (the Phase 90 lava-lake burn round trip): the diff
    // records the LIVE state's occupants. An "air" block edit (the
    // generated block was burned and the fluid then flowed in) may share
    // a cell with a positive fluid edit — the live cell is fluid-only.
    {
        Json root = *j;
        Json barr = Json::array();
        barr.push_back(blockEdit(1, 10, 1, "air"));
        root["blocks"] = std::move(barr);
        root["fluids"] =
            withFluids({edit(1, 10, 1, Json(3))})["fluids"];
        auto parsed = parseSave(root, f.items, f.blocks, f.effects, 42);
        ASSERT_TRUE(parsed.ok()) << parsed.error().message;
        ASSERT_EQ(parsed->blockEdits.size(), 1u);
        EXPECT_EQ(parsed->blockEdits[0].blockName, "air");
        ASSERT_EQ(parsed->fluidEdits.size(), 1u);
        EXPECT_EQ(parsed->fluidEdits[0].level, 3);
    }
    // Likewise a non-air block edit with a LEVEL-0 fluid edit: the block
    // (or the water/lava reaction) displaced the generated fluid — the
    // live cell is block-only.
    {
        Json root = *j;
        Json barr = Json::array();
        barr.push_back(blockEdit(1, 10, 1, "stone"));
        root["blocks"] = std::move(barr);
        Json fl = Json::array();
        fl.push_back(edit(1, 10, 1, Json(0), "lava"));
        root["fluids"] = std::move(fl);
        auto parsed = parseSave(root, f.items, f.blocks, f.effects, 42);
        ASSERT_TRUE(parsed.ok()) << parsed.error().message;
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
    root = withoutKey(root, "fires"); // format 8 predates the fire
    root = withoutKey(root, "tables"); // format 8 predates the per-block table grids
    root = withoutKey(root, "campfires"); // format 8 predates the campfire
    root = withoutKey(root, "states"); // format 8 predates per-block state
    root["player"] = withoutKey(root["player"], "absorption"); // format 8 predates the absorption pool
    root["player"] = withoutKey(root["player"], "armor"); // SURV-1: format below 16 predates the player's armor
    root["player"] = withoutKey(root["player"], "oxygen"); // SURV-2: format below 17 predates the player's oxygen
    root["table_grid"] = emptyTableGrid(); // CRAFT-1: format 8 still carried the global table grid
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
    // A full column (the game's load state: the streaming loader works in
    // whole columns; applyWorldEdits tops edited columns up to the same).
    world.ensureColumn(0, 0);
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

TEST(SaveWorldDiffTest, FluidEditInANeverLoadedColumnCreatesTheChunk) {
    // The Phase 90 lava-lake burn round trip: the diff can hold a fluid
    // edit in a column with no block edit (the lava overflowed past the
    // loaded area). applyWorldEdits must ensure the chunk (synchronous
    // generation in the game; the fixture world has no generator, so an
    // empty chunk) and apply the edit instead of failing the load.
    Fixture f;
    const BlockID stone = f.blocks.idForName("stone");
    World world = makeWorld(stone); // only chunk (0, 0, 0)
    SaveGame save;
    // x=64 is chunk (4, 0, 0) — never created, and no block edit creates
    // it either.
    save.fluidEdits = {{BlockPos {64, 5, 0}, FluidKind::Water, 3}};
    auto r = applyWorldEdits(save, f.blocks, world);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(world.getFluidOfKind(64, 5, 0, FluidKind::Water), 3);
}

// A level-0 fluid edit (the generated fluid was drained) on a dry cell
// must apply as a no-op, not a failure.
TEST(SaveWorldDiffTest, FluidDrainEditOnADryCellIsANoOp) {
    Fixture f;
    const BlockID stone = f.blocks.idForName("stone");
    World world = makeWorld(stone);
    SaveGame save;
    save.fluidEdits = {{BlockPos {64, 5, 0}, FluidKind::Lava, 0}};
    auto r = applyWorldEdits(save, f.blocks, world);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(world.getFluidOfKind(64, 5, 0, FluidKind::Lava), 0);
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

// CRAFT-1: a save with two crafting_table blocks (position-sorted, as the
// game's map writes them) and the matching (here empty) grids.
SaveGame makeTableSave(const Fixture& f) {
    SaveGame s = makeSave(f);
    s.blockEdits.push_back({BlockPos {1, 53, 2}, "crafting_table"});
    s.blockEdits.push_back({BlockPos {2, 53, 2}, "crafting_table"});
    std::sort(s.blockEdits.begin(), s.blockEdits.end(),
              [](const SavedBlockEdit& a, const SavedBlockEdit& b) {
                  return a.pos < b.pos;
              });
    s.tables.push_back(SavedTable {BlockPos {1, 53, 2}, CraftingGrid {}});
    s.tables.push_back(SavedTable {BlockPos {2, 53, 2}, CraftingGrid {}});
    return s;
}

// CRAFT-1: a legacy "table_grid" array (9 slot entries) with the given
// (cell, item name, count) fills; all other cells empty (Json arrays
// have no index-set — the fills are applied during the build).
Json legacyTableGrid(
    const std::vector<std::pair<int, std::pair<std::string, int>>>& fills) {
    Json arr = Json::array();
    for (int c = 0; c < CraftingGrid::kSize; ++c) {
        Json e = Json::object();
        bool filled = false;
        for (const auto& [cell, ic] : fills) {
            if (cell == c) {
                e["item"] = Json(ic.first);
                e["count"] = Json(ic.second);
                filled = true;
            }
        }
        if (!filled) {
            e["item"] = Json(std::string(""));
            e["count"] = Json(0);
        }
        arr.push_back(std::move(e));
    }
    return arr;
}

// CRAFT-1: rewrite a serialized format-19 save into the format-18 shape
// (what a pre-CRAFT-1 build wrote): the per-block "tables" array is
// removed, the format downgraded to 18, and the ONE global "table_grid"
// restored. The format-18 parse path (still supported) is tested through
// this.
Json downgradeTablesToFormat18(const Json& root, const Json& tableGrid) {
    Json out = withoutKey(root, "tables");
    out["format"] = Json(18);
    out["table_grid"] = tableGrid;
    return out;
}

// Phase 94 / ENT-2: a save with a stair block + its per-instance state
// (the project's first per-block state; for a stair, the facing).
SaveGame makeStateSave(const Fixture& f) {
    SaveGame s = makeSave(f);
    s.blockEdits.push_back({BlockPos {3, 53, 1}, "stone_stairs"});
    std::sort(s.blockEdits.begin(), s.blockEdits.end(),
              [](const SavedBlockEdit& a, const SavedBlockEdit& b) {
                  return a.pos < b.pos;
              });
    s.states.push_back({BlockPos {3, 53, 1}, 2}); // south-facing (rotated)
    return s;
}

// ENT-2: rewrite a serialized format-15 save into the format-14 shape —
// the "states" array becomes "stairs" (each entry's "state" becomes
// "facing") and the format is downgraded to 14. The format-14 parse path
// (still supported for old files) is tested through this.
Json downgradeToFormat14(const Json& root) {
    Json out = withoutKey(root, "states");
    out = withoutKey(out, "tables"); // CRAFT-1: format 14 predates the per-block table grids
    out["table_grid"] = emptyTableGrid(); // CRAFT-1: format 14 still carried the global table grid
    out["format"] = Json(14);
    out["player"] = withoutKey(out["player"], "armor"); // SURV-1: format 14 predates the player's armor
    out["player"] = withoutKey(out["player"], "oxygen"); // SURV-2: format 14 predates the player's oxygen
    const Json* states = root.find("states");
    if (states == nullptr || !states->isArray()) {
        ADD_FAILURE() << "no 'states' array to downgrade";
        return out;
    }
    Json stairs = Json::array();
    for (std::size_t i = 0; i < states->size(); ++i) {
        const Json* st = states->at(i).find("state");
        if (st == nullptr) {
            ADD_FAILURE() << "state entry #" << i << " has no 'state'";
            return out;
        }
        Json entry = withoutKey(states->at(i), "state");
        entry["facing"] = *st;
        stairs.push_back(std::move(entry));
    }
    out["stairs"] = std::move(stairs);
    return out;
}

TEST(SaveRoundTripTest, StatesRoundTrip) {
    Fixture f;
    SaveGame s = makeStateSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok()) << j.error().message;
    // CRAFT-1: writes are always format 19 (SURV-3 was format 18).
    EXPECT_EQ(j->find("format")->asInt(), 19);
    auto out = parseSave(*j, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    ASSERT_EQ(out->states.size(), 1u);
    const BlockPos sp {3, 53, 1};
    EXPECT_EQ(out->states[0].pos, sp);
    EXPECT_EQ(out->states[0].state, 2); // south (the rotated facing)
}

// GEN-2: the sapling's growth stage rides the format-15 `states` segment
// (the block edit carries the sapling, the state entry the stage — the
// journal derivation reads the live value at exit, the load's cross-check
// pins the entry set to the stateful block cells).
TEST(SaveRoundTripTest, SaplingStageRoundTrip) {
    Fixture f;
    SaveGame s = makeSave(f);
    s.blockEdits.push_back({BlockPos {5, 53, 5}, "oak_sapling"});
    std::sort(s.blockEdits.begin(), s.blockEdits.end(),
              [](const SavedBlockEdit& a, const SavedBlockEdit& b) {
                  return a.pos < b.pos;
              });
    s.states.push_back({BlockPos {5, 53, 5}, 4}); // stage 4 of 8
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok()) << j.error().message;
    auto out = parseSave(*j, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    ASSERT_EQ(out->states.size(), 1u);
    const BlockPos sp {5, 53, 5};
    EXPECT_EQ(out->states[0].pos, sp);
    EXPECT_EQ(out->states[0].state, 4);
    // The block edit survived too (the sapling IS the tracked cell).
    bool found = false;
    for (const auto& e : out->blockEdits) {
        found = found || (e.pos == sp && e.blockName == "oak_sapling");
    }
    EXPECT_TRUE(found);
}

// GEN-2: a GROWN tree is pure block edits (trunk + leaves — no stateful
// block among them) and carries NO states entry: the completion's cells
// are non-stateful, so the save's state segment stays empty (the
// cross-check would reject a stray entry for a non-stateful cell).
TEST(SaveRoundTripTest, GrownTreeSaveCarriesNoStates) {
    Fixture f;
    SaveGame s = makeSave(f);
    const BlockPos root {5, 53, 5};
    // The oak T = 4 plan (a subset is enough — every cell non-stateful).
    for (int i = 0; i < 4; ++i) {
        s.blockEdits.push_back({BlockPos {root.x, root.y + i, root.z},
                                "wood"});
    }
    s.blockEdits.push_back({BlockPos {root.x, root.y + 4, root.z},
                            "leaves"});
    s.blockEdits.push_back({BlockPos {root.x + 1, root.y + 3, root.z},
                            "leaves"});
    std::sort(s.blockEdits.begin(), s.blockEdits.end(),
              [](const SavedBlockEdit& a, const SavedBlockEdit& b) {
                  return a.pos < b.pos;
              });
    // No states entry — the grown tree is stateless.
    ASSERT_TRUE(s.states.empty());
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok()) << j.error().message;
    EXPECT_EQ(j->find("states")->size(), 0u); // the key, empty
    auto out = parseSave(*j, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    EXPECT_TRUE(out->states.empty());
    EXPECT_EQ(out->blockEdits.size(), s.blockEdits.size());
}

TEST(SaveParseTest, Format15RequiresStatesKey) {
    // A format-15 file WITHOUT the "states" key is rejected (the key is
    // required, an empty array is valid).
    Fixture f;
    SaveGame s = makeStateSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    Json root = withoutKey(*j, "states");
    auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
    EXPECT_FALSE(out.ok());
}

TEST(SaveParseTest, RejectsStairsFieldInFormat15) {
    // A format-15 file carrying "stairs" is a version lie (the key is
    // restricted to format 14 — rejected even when a "states" array is
    // present, and the entry shape mismatch is moot: the key itself is
    // the lie).
    Fixture f;
    SaveGame s = makeStateSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    Json root = withoutKey(*j, "states");
    root["stairs"] = Json::array(); // format 15 keeps carrying "stairs"
    auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
    EXPECT_FALSE(out.ok());
}

TEST(SaveParseTest, Format14RequiresStairsKey) {
    // A format-14 file WITHOUT the "stairs" key is rejected (the key is
    // required, an empty array is valid).
    Fixture f;
    SaveGame s = makeStateSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    Json root = withoutKey(downgradeToFormat14(*j), "stairs");
    auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
    EXPECT_FALSE(out.ok());
}

TEST(SaveParseTest, Format14To15MigrationValuePreserved) {
    // A format-14 file LOADS: its "stairs" entries migrate into the
    // states VALUE-PRESERVING (the facing becomes the state) — the
    // rotated facing 2 (south) survives the load.
    Fixture f;
    SaveGame s = makeStateSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    Json root = downgradeToFormat14(*j);
    auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    ASSERT_EQ(out->states.size(), 1u);
    EXPECT_EQ(out->states[0].pos, (BlockPos {3, 53, 1}));
    EXPECT_EQ(out->states[0].state, 2); // the facing value, preserved
}

TEST(SaveParseTest, Format13LoadsEmptyStates) {
    // A format-13 file (no "stairs"/"states" key) still loads: the
    // per-instance state did not exist, the block's state default is used
    // on load. The save's states field is empty.
    Fixture f;
    SaveGame s = makeStateSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    Json root = withoutKey(downgradeToFormat14(*j), "stairs");
    root["format"] = Json(13);
    auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    EXPECT_TRUE(out->states.empty());
}

TEST(SaveParseTest, RejectsStatesFieldInFormat14) {
    // A format-14 file carrying "states" is a version lie (rejected —
    // the key requires format 15 or newer).
    Fixture f;
    SaveGame s = makeStateSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    Json root = *j;
    root["format"] = Json(14); // downgrade but KEEP the "states" array
    root["player"] = withoutKey(root["player"], "armor"); // SURV-1: the states lie is the one under test
    root["player"] = withoutKey(root["player"], "oxygen"); // SURV-2: the states lie is the one under test
    auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
    EXPECT_FALSE(out.ok());
}

TEST(SaveParseTest, RejectsStairsFieldInFormat13) {
    // A format-13 file carrying "stairs" is a version lie (rejected).
    Fixture f;
    SaveGame s = makeStateSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    Json root = *j;
    root = withoutKey(root, "states");
    root["stairs"] = Json::array();
    root["format"] = Json(13); // downgrade but KEEP the "stairs" array
    root["player"] = withoutKey(root["player"], "armor"); // SURV-1: the stairs lie is the one under test
    root["player"] = withoutKey(root["player"], "oxygen"); // SURV-2: the stairs lie is the one under test
    auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
    EXPECT_FALSE(out.ok());
}

TEST(SaveParseTest, RejectsStatesFieldInFormat13) {
    // A format-13 file carrying "states" is a version lie (rejected).
    Fixture f;
    SaveGame s = makeStateSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    Json root = *j;
    root["format"] = Json(13); // downgrade but KEEP the "states" array
    root["player"] = withoutKey(root["player"], "armor"); // SURV-1: the states lie is the one under test
    root["player"] = withoutKey(root["player"], "oxygen"); // SURV-2: the states lie is the one under test
    auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
    EXPECT_FALSE(out.ok());
}

TEST(SaveParseTest, RejectsStateEntryBlockMismatch) {
    // A "states" entry at a position that is NOT a stateful block is
    // rejected (the cross-check, both directions).
    Fixture f;
    SaveGame s = makeStateSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    Json root = *j;
    // Point the state entry at a non-stateful cell (the "dirt" edit at
    // 0,54,-1).
    const Json* states = root.find("states");
    ASSERT_TRUE(states != nullptr && states->isArray() && states->size() == 1);
    Json entry = states->at(0);
    Json pos = Json::array();
    pos.push_back(Json(0));
    pos.push_back(Json(54));
    pos.push_back(Json(-1));
    entry["pos"] = pos;
    Json newStates = Json::array();
    newStates.push_back(entry);
    root["states"] = newStates;
    auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
    EXPECT_FALSE(out.ok());
}

TEST(SaveParseTest, RejectsStateEntryDomain) {
    // A "states" entry whose value is outside the block's state domain
    // [0, stateCount) is rejected and named (a stair holds 0-3; 4 is
    // out of range).
    Fixture f;
    SaveGame s = makeStateSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    Json root = *j;
    const Json* states = root.find("states");
    ASSERT_TRUE(states != nullptr && states->isArray() && states->size() == 1);
    Json entry = states->at(0);
    entry["state"] = Json(4); // a stair's state is 0-3
    Json newStates = Json::array();
    newStates.push_back(entry);
    root["states"] = newStates;
    auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
    ASSERT_FALSE(out.ok());
    EXPECT_NE(out.error().message.find("state"), std::string::npos);
    EXPECT_NE(out.error().message.find("4"), std::string::npos);
}

TEST(SaveParseTest, RejectsStateEntryUnknownField) {
    // A "states" entry with an unknown field is rejected (the strict
    // {pos, state} shape — the furnaces-key entry discipline).
    Fixture f;
    SaveGame s = makeStateSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    Json root = *j;
    const Json* states = root.find("states");
    ASSERT_TRUE(states != nullptr && states->isArray() && states->size() == 1);
    Json entry = states->at(0);
    entry["facing"] = Json(2); // a format-14 field, unknown to 15
    Json newStates = Json::array();
    newStates.push_back(entry);
    root["states"] = newStates;
    auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
    EXPECT_FALSE(out.ok());
}

TEST(SaveRoundTripTest, EmptyStatesArrayValid) {
    // A world without stateful blocks serializes "states" as an empty
    // array and round-trips (the key is required even when empty).
    Fixture f;
    SaveGame s = makeSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok()) << j.error().message;
    const Json* states = j->find("states");
    ASSERT_TRUE(states != nullptr && states->isArray());
    EXPECT_EQ(states->size(), 0u);
    auto out = parseSave(*j, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    EXPECT_TRUE(out->states.empty());
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
    // CRAFT-1: writes are always format 19 (SURV-3 was format 18).
    EXPECT_EQ(j->find("format")->asInt(), 19);
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
    root = withoutKey(root, "fires"); // format 5 predates the fire
    root = withoutKey(root, "tables"); // format 5 predates the per-block table grids
    root = withoutKey(root, "campfires"); // format 5 predates the campfire
    root = withoutKey(root, "states"); // format 5 predates per-block state
    root["player"] = withoutKey(root["player"], "absorption"); // format 5 predates the absorption pool
    root["player"] = withoutKey(root["player"], "armor"); // SURV-1: format below 16 predates the player's armor
    root["player"] = withoutKey(root["player"], "oxygen"); // SURV-2: format below 17 predates the player's oxygen
    root["fluids"] = downgradeFluids(root); // pre-Phase-51 shape
    root = withoutKey(root, "furnaces"); // format 5 predates the furnace
    root["table_grid"] = emptyTableGrid(); // CRAFT-1: format 5 still carried the global table grid
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
    root = withoutKey(root, "fires"); // format 6 predates the fire
    root = withoutKey(root, "tables"); // format 6 predates the per-block table grids
    root = withoutKey(root, "campfires"); // format 6 predates the campfire
    root = withoutKey(root, "states"); // format 6 predates per-block state
    root["player"] = withoutKey(root["player"], "absorption"); // format 6 predates the absorption pool
    root["player"] = withoutKey(root["player"], "armor"); // SURV-1: format below 16 predates the player's armor
    root["player"] = withoutKey(root["player"], "oxygen"); // SURV-2: format below 17 predates the player's oxygen
    root["fluids"] = downgradeFluids(root); // pre-Phase-51 shape
    root["furnace"] = withoutKey(root["furnaces"].at(0), "pos");
    root = withoutKey(root, "furnaces");
    root["table_grid"] = emptyTableGrid(); // CRAFT-1: format 6 still carried the global table grid
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
    root = withoutKey(root, "fires"); // format 6 predates the fire
    root = withoutKey(root, "tables"); // format 6 predates the per-block table grids
    root = withoutKey(root, "campfires"); // format 6 predates the campfire
    root = withoutKey(root, "states"); // format 6 predates per-block state
    root["player"] = withoutKey(root["player"], "absorption"); // format 6 predates the absorption pool
    root["player"] = withoutKey(root["player"], "armor"); // SURV-1: format below 16 predates the player's armor
    root["player"] = withoutKey(root["player"], "oxygen"); // SURV-2: format below 17 predates the player's oxygen
    root["fluids"] = downgradeFluids(root); // pre-Phase-51 shape
    root["furnace"] = withoutKey(root["furnaces"].at(0), "pos");
    root = withoutKey(root, "furnaces");
    root["table_grid"] = emptyTableGrid(); // CRAFT-1: format 6 still carried the global table grid
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
        root2 = withoutKey(root2, "fires"); // format 6 predates the fire
        root2 = withoutKey(root2, "tables"); // format 6 predates the per-block table grids
        root2 = withoutKey(root2, "campfires"); // format 6 predates the campfire
        root2 = withoutKey(root2, "states"); // format 6 predates per-block state
        root2["player"] = withoutKey(root2["player"], "absorption"); // format 6 predates the absorption pool
        root2["player"] = withoutKey(root2["player"], "armor"); // SURV-1: format below 16 predates the player's armor
        root2["player"] = withoutKey(root2["player"], "oxygen"); // SURV-2: format below 17 predates the player's oxygen
        root2["fluids"] = downgradeFluids(root2); // pre-Phase-51 shape
        root2 = withoutKey(root2, "furnaces");
        root2["table_grid"] = emptyTableGrid(); // CRAFT-1: format 6 still carried the global table grid
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
    root = withoutKey(root, "fires"); // the rejection under test is "furnaces"
    root = withoutKey(root, "campfires"); // format older predates the campfire
    root = withoutKey(root, "states"); // format older predates per-block state
    root["player"] = withoutKey(root["player"], "absorption"); // format 5 predates the absorption pool
    root["player"] = withoutKey(root["player"], "armor"); // SURV-1: format below 16 predates the player's armor
    root["player"] = withoutKey(root["player"], "oxygen"); // SURV-2: format below 17 predates the player's oxygen
    root["fluids"] = downgradeFluids(root); // pre-Phase-51 shape
    root = withoutKey(root, "tables"); // CRAFT-1: the rejection under test is "furnaces"
    root["table_grid"] = emptyTableGrid(); // CRAFT-1: format 5 still carried the global table grid
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
    root = withoutKey(root, "fires"); // the rejection under test is "furnaces"
    root = withoutKey(root, "tables"); // the rejection under test is "furnaces" (CRAFT-1)
    root = withoutKey(root, "campfires"); // format older predates the campfire
    root = withoutKey(root, "states"); // format older predates per-block state
    root["player"] = withoutKey(root["player"], "absorption"); // format 6 predates the absorption pool
    root["player"] = withoutKey(root["player"], "armor"); // SURV-1: format below 16 predates the player's armor
    root["player"] = withoutKey(root["player"], "oxygen"); // SURV-2: format below 17 predates the player's oxygen
    root["fluids"] = downgradeFluids(root); // pre-Phase-51 shape
    root["table_grid"] = emptyTableGrid(); // CRAFT-1: format 6 still carried the global table grid
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

// -- burning fires (format 10; Phase 64) ------------------------------------
//
// makeSave has no fire blocks; these tests add them. The format-10
// cross-check is BIDIRECTIONAL, exactly like the furnace states: exactly
// one entry per fire block in the block edits (entries without a block and
// blocks without an entry are both rejected).

// A save with two fire blocks (position-sorted, as the game's map writes
// them) and the matching entries.
SaveGame makeFireSave(const Fixture& f) {
    SaveGame s = makeSave(f);
    s.blockEdits.push_back({BlockPos {1, 53, 1}, "fire"});
    s.blockEdits.push_back({BlockPos {2, 53, 1}, "fire"});
    s.fires.push_back({BlockPos {1, 53, 1}, 2.5});
    s.fires.push_back({BlockPos {2, 53, 1}, 4.0});
    return s;
}

TEST(SaveRoundTripTest, FiresRoundTrip) {
    Fixture f;
    SaveGame s = makeFireSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok()) << j.error().message;
    // CRAFT-1: writes are always format 19 (SURV-3 was format 18).
    EXPECT_EQ(j->find("format")->asInt(), 19);
    auto out = parseSave(*j, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    ASSERT_EQ(out->fires.size(), 2u);
    EXPECT_EQ(out->fires[0].pos, (BlockPos{1, 53, 1}));
    EXPECT_DOUBLE_EQ(out->fires[0].remainingSeconds, 2.5);
    EXPECT_EQ(out->fires[1].pos, (BlockPos{2, 53, 1}));
    EXPECT_DOUBLE_EQ(out->fires[1].remainingSeconds, 4.0);
}

TEST(SaveRoundTripTest, EmptyFiresRoundTrip) {
    // A world without fire blocks still carries the (empty) key — the
    // format-10 discipline (a missing key in a format-10 file is corrupt).
    Fixture f;
    SaveGame s = makeSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok()) << j.error().message;
    ASSERT_NE(j->find("fires"), nullptr);
    EXPECT_EQ(j->find("fires")->size(), 0u);
    auto out = parseSave(*j, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    EXPECT_TRUE(out->fires.empty());
}

TEST(SaveParseTest, FormatNineLoadsWithoutFires) {
    // A format-9 file (fires did not exist) loads with no entries.
    Fixture f;
    SaveGame s = makeSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    Json root = *j;
    root["format"] = Json(9);
    root = withoutKey(root, "fires"); // format 9 predates the fire
    root = withoutKey(root, "tables"); // format 9 predates the per-block table grids
    root = withoutKey(root, "campfires"); // format 9 predates the campfire
    root = withoutKey(root, "states"); // format 9 predates per-block state
    root["player"] = withoutKey(root["player"], "absorption"); // format 9 predates the absorption pool
    root["player"] = withoutKey(root["player"], "armor"); // SURV-1: format below 16 predates the player's armor
    root["player"] = withoutKey(root["player"], "oxygen"); // SURV-2: format below 17 predates the player's oxygen
    root["table_grid"] = emptyTableGrid(); // CRAFT-1: format 9 still carried the global table grid
    auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    EXPECT_TRUE(out->fires.empty());
}

TEST(SaveParseTest, FormatNineFireEditMigratesEmpty) {
    // A hand-edited format-9 file with a fire block edit: the parse keeps
    // the entry list EMPTY (the game adds the full-lifetime entries on
    // load, like the empty furnace states).
    Fixture f;
    SaveGame s = makeSave(f);
    s.blockEdits.push_back({BlockPos {1, 53, 1}, "fire"});
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    Json root = *j;
    root["format"] = Json(9);
    root = withoutKey(root, "fires"); // format 9 predates the fire
    root = withoutKey(root, "tables"); // format 9 predates the per-block table grids
    root = withoutKey(root, "campfires"); // format 9 predates the campfire
    root = withoutKey(root, "states"); // format 9 predates per-block state
    root["player"] = withoutKey(root["player"], "absorption"); // format 9 predates the absorption pool
    root["player"] = withoutKey(root["player"], "armor"); // SURV-1: format below 16 predates the player's armor
    root["player"] = withoutKey(root["player"], "oxygen"); // SURV-2: format below 17 predates the player's oxygen
    root["table_grid"] = emptyTableGrid(); // CRAFT-1: format 9 still carried the global table grid
    auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    EXPECT_TRUE(out->fires.empty());
}

TEST(SaveParseTest, RejectsFiresFieldInFormatNine) {
    Fixture f;
    SaveGame s = makeFireSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    Json root = *j;
    root["format"] = Json(9); // keeps the "fires" field
    root = withoutKey(root, "tables"); // the rejection under test is "fires" (CRAFT-1)
    root["table_grid"] = emptyTableGrid(); // CRAFT-1: format 9 still carried the global table grid
    EXPECT_FALSE(parseSave(root, f.items, f.blocks, f.effects, 42).ok());
}

TEST(SaveParseTest, RejectsFireEntryBlockMismatch) {
    // The bidirectional cross-check: an entry whose position holds no fire
    // block, or a fire block with no entry.
    Fixture f;
    {
        SaveGame s = makeSave(f);
        s.blockEdits.push_back({BlockPos {1, 53, 1}, "fire"});
        s.fires.push_back({BlockPos {5, 53, 5}, 2.0});
        auto j = serializeSave(s, f.items);
        ASSERT_TRUE(j.ok());
        EXPECT_FALSE(parseSave(*j, f.items, f.blocks, f.effects, 42).ok());
    }
    {
        SaveGame s = makeFireSave(f);
        s.fires.pop_back(); // the second block has no entry
        auto j = serializeSave(s, f.items);
        ASSERT_TRUE(j.ok());
        EXPECT_FALSE(parseSave(*j, f.items, f.blocks, f.effects, 42).ok());
    }
}

TEST(SaveParseTest, RejectsDuplicateFireEntries) {
    Fixture f;
    SaveGame s = makeSave(f);
    s.blockEdits.push_back({BlockPos {1, 53, 1}, "fire"});
    s.fires.push_back({BlockPos {1, 53, 1}, 2.0});
    s.fires.push_back({BlockPos {1, 53, 1}, 3.0});
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    EXPECT_FALSE(parseSave(*j, f.items, f.blocks, f.effects, 42).ok());
}

TEST(SaveParseTest, RejectsFireEntryBadRemaining) {
    // remaining must be a number in (0, ...]: the countdown is the save's
    // whole reason to exist (0 or negative would burn out on load).
    Fixture f;
    for (const double bad : {0.0, -1.5}) {
        SaveGame s = makeFireSave(f);
        s.fires[0].remainingSeconds = bad;
        auto j = serializeSave(s, f.items);
        ASSERT_TRUE(j.ok());
        EXPECT_FALSE(parseSave(*j, f.items, f.blocks, f.effects, 42).ok());
    }
}

TEST(SaveParseTest, RejectsFireEntryMissingRemaining) {
    // The "remaining" field is required (the countdown is the entry's
    // whole reason to exist).
    Fixture f;
    SaveGame s = makeFireSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    Json root = *j;
    Json arr = Json::array();
    arr.push_back(withoutKey(root["fires"].at(0), "remaining"));
    arr.push_back(root["fires"].at(1));
    root["fires"] = std::move(arr);
    EXPECT_FALSE(parseSave(root, f.items, f.blocks, f.effects, 42).ok());
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
    root = withoutKey(root, "fires"); // format 7 predates the fire
    root = withoutKey(root, "tables"); // format 7 predates the per-block table grids
    root = withoutKey(root, "campfires"); // format 7 predates the campfire
    root = withoutKey(root, "states"); // format 7 predates per-block state
    root["table_grid"] = emptyTableGrid(); // CRAFT-1: format 7 still carried the global table grid
    root["fluids"] = downgradeFluids(root); // pre-Phase-51 shape
    root["player"] = withoutKey(*j->find("player"), "effects");
    root["player"] = withoutKey(root["player"], "absorption"); // format 7 predates the absorption pool
    root["player"] = withoutKey(root["player"], "armor"); // SURV-1: format below 16 predates the player's armor
    root["player"] = withoutKey(root["player"], "oxygen"); // SURV-2: format below 17 predates the player's oxygen
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

// -- Phase 72: the absorption pool (two components) and the saved health ---

TEST(SaveFormat12Test, AbsorptionPoolRoundTrips) {
    // A save taken while the absorption layer is active stores the BASE
    // health + the SEPARATE pool (16 + 2 of 4: the player already took
    // damage — the pool may sit BELOW the effects' total, the parse
    // accepts it; the game-level rejection of health > base max runs in
    // applySave). The round trip must preserve both pools.
    Fixture f;
    SaveGame s = makeSave(f);
    s.playerHealth = 16.0f;
    s.playerAbsorption = 2.0f;
    s.playerEffects = {{"absorption", 3.25}};
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok()) << j.error().message;
    auto out = parseSave(*j, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    EXPECT_FLOAT_EQ(out->playerHealth, 16.0f);
    EXPECT_FLOAT_EQ(out->playerAbsorption, 2.0f);
    ASSERT_EQ(out->playerEffects.size(), 1u);
    EXPECT_EQ(out->playerEffects[0].name, "absorption");
    EXPECT_DOUBLE_EQ(out->playerEffects[0].remaining, 3.25);
}

TEST(SaveFormat12Test, Format12RequiresTheAbsorptionKey) {
    Fixture f;
    SaveGame s = makeSave(f); // playerAbsorption 0 -> the key IS written
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    Json root = *j;
    root["player"] = withoutKey(root["player"], "absorption");
    EXPECT_FALSE(parseSave(root, f.items, f.blocks, f.effects, 42).ok());
}

TEST(SaveFormat12Test, Format11RejectsTheAbsorptionKey) {
    // The new-key discipline: a format-11 file carrying the pool is a
    // version lie (the shared-health migration reads it instead).
    Fixture f;
    SaveGame s = makeSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    Json root = *j;
    root["format"] = Json(11); // the "absorption" key stays -> rejected
    root = withoutKey(root, "states"); // format 11 predates per-block state
    root["player"] = withoutKey(root["player"], "armor"); // SURV-1: the absorption lie is the one under test
    root["player"] = withoutKey(root["player"], "oxygen"); // SURV-2: the absorption lie is the one under test
    EXPECT_FALSE(parseSave(root, f.items, f.blocks, f.effects, 42).ok());
    // The same file WITHOUT the key loads (the format-11 migration path;
    // the sentinel stays -1 for Game::applySave).
    root = withoutKey(root, "tables"); // CRAFT-1: format 11 predates the per-block table grids
    root["table_grid"] = emptyTableGrid(); // CRAFT-1: format 11 still carried the global table grid
    root["player"] = withoutKey(root["player"], "absorption");
    root["player"] = withoutKey(root["player"], "armor"); // SURV-1: format below 16 predates the player's armor
    root["player"] = withoutKey(root["player"], "oxygen"); // SURV-2: format below 17 predates the player's oxygen
    auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    EXPECT_FLOAT_EQ(out->playerAbsorption, -1.0f);
}

TEST(SaveFormat12Test, AbsorptionAboveTheEffectsTotalRejected) {
    // The pool never exceeds the layer (damage only drains it): 8 > the
    // single 4-HP absorption effect's total.
    Fixture f;
    SaveGame s = makeSave(f);
    s.playerAbsorption = 8.0f;
    s.playerEffects = {{"absorption", 3.25}};
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    EXPECT_FALSE(parseSave(*j, f.items, f.blocks, f.effects, 42).ok());
}

TEST(SaveFormat12Test, AbsorptionAboveTheCapRejected) {
    // The attribute cap (Minecraft): 65 > 64.
    Fixture f;
    SaveGame s = makeSave(f);
    s.playerAbsorption = 65.0f;
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    EXPECT_FALSE(parseSave(*j, f.items, f.blocks, f.effects, 42).ok());
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

// -- ENT-1: the mob limb-swing phase is not a save surface -----------------

TEST(SaveSchemaTest, NoSwingPhaseSurfaceInTheSave) {
    // The swing phase is a render-layer cosmetic state (session-local,
    // like the item bob) and mobs are ephemeral by design (Phase 20): it
    // never enters the save. Two pins: the serialized save is still
    // format kSaveFormat (no new field crept in), and the player
    // object's key whitelist rejects a swing field (a future writer that
    // tried to persist the phase would fail the strict load).
    Fixture f;
    const SaveGame in = makeSave(f);
    auto j = serializeSave(in, f.items);
    ASSERT_TRUE(j.ok()) << j.error().message;
    ASSERT_EQ(j->find("format")->asInt(), kSaveFormat);
    Json root = *j;
    root["player"]["swing_phase"] = Json(3);
    auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
    ASSERT_FALSE(out.ok());
    EXPECT_NE(out.error().message.find("swing_phase"), std::string::npos)
        << out.error().message;
}

// -- SURV-1: the player's armor (format 16) -----------------------------------

TEST(SaveFormat16Test, ArmorRoundTrips) {
    // A save taken while wearing armor: the player.armor object holds the
    // OCCUPIED slots only (the fixed slot order); the round trip must
    // preserve every piece name and keep the empty slots empty.
    Fixture f;
    SaveGame s = makeSave(f);
    s.playerArmor[0] = "iron_helmet";      // head
    s.playerArmor[2] = "diamond_leggings"; // legs
    s.playerArmor[3] = "diamond_boots";    // feet
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok()) << j.error().message;
    const Json* armor = j->find("player")->find("armor");
    ASSERT_TRUE(armor != nullptr && armor->isObject());
    ASSERT_EQ(armor->memberCount(), 3u);
    EXPECT_EQ(armor->find("head")->asString(), "iron_helmet");
    EXPECT_EQ(armor->find("legs")->asString(), "diamond_leggings");
    EXPECT_EQ(armor->find("feet")->asString(), "diamond_boots");
    EXPECT_EQ(armor->find("chest"), nullptr); // the empty slot is unwritten
    auto out = parseSave(*j, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    EXPECT_EQ(out->playerArmor[0], "iron_helmet");
    EXPECT_TRUE(out->playerArmor[1].empty());
    EXPECT_EQ(out->playerArmor[2], "diamond_leggings");
    EXPECT_EQ(out->playerArmor[3], "diamond_boots");
}

TEST(SaveFormat16Test, EmptyArmorObjectValid) {
    // A save without armor writes "armor": {} (the key is required even
    // when empty — the effects-key discipline) and round-trips empty.
    Fixture f;
    const SaveGame s = makeSave(f); // the fixture wears nothing
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok()) << j.error().message;
    const Json* armor = j->find("player")->find("armor");
    ASSERT_TRUE(armor != nullptr && armor->isObject() && armor->memberCount() == 0);
    auto out = parseSave(*j, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    for (int slot = 0; slot < kArmorSlotCount; ++slot) {
        EXPECT_TRUE(out->playerArmor[static_cast<std::size_t>(slot)].empty());
    }
}

TEST(SaveFormat16Test, Format16RequiresTheArmorKey) {
    // A format-16 file WITHOUT the "armor" key is rejected (the key is
    // required; the rest of the player object is intact).
    Fixture f;
    const SaveGame s = makeSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    Json root = *j;
    root["player"] = withoutKey(root["player"], "armor");
    auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
    ASSERT_FALSE(out.ok());
    EXPECT_NE(out.error().message.find("player.armor"), std::string::npos)
        << out.error().message;
}

TEST(SaveFormat16Test, Format15RejectsTheArmorKey) {
    // The version-lie discipline: a format-15 file carrying the armor key
    // is rejected (named); the same file WITHOUT the key loads and the
    // slots migrate empty (the player had no armor before format 16).
    Fixture f;
    SaveGame s = makeSave(f);
    s.playerArmor[0] = "iron_helmet"; // an occupied slot to carry
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    {
        Json root = *j;
        root["format"] = Json(15); // the "armor" key stays -> rejected
        auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
        ASSERT_FALSE(out.ok());
        EXPECT_NE(out.error().message.find("format 16 or newer"),
                  std::string::npos)
            << out.error().message;
    }
    {
        Json root = *j;
        root["format"] = Json(15);
        root = withoutKey(root, "tables"); // CRAFT-1: format 15 predates the per-block table grids
        root["table_grid"] = emptyTableGrid(); // CRAFT-1: format 15 still carried the global table grid
        root["player"] = withoutKey(root["player"], "armor");
        root["player"] = withoutKey(root["player"], "oxygen");
        auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
        ASSERT_TRUE(out.ok()) << out.error().message;
        for (int slot = 0; slot < kArmorSlotCount; ++slot) {
            EXPECT_TRUE(out->playerArmor[static_cast<std::size_t>(slot)].empty());
        }
    }
}

TEST(SaveFormat16Test, ArmorSlotMismatchRejected) {
    // A piece filed under a slot other than its data armor_slot is
    // rejected and named (the serializer writes the key from the array
    // index, so a mismatched array IS the corrupt file — the parse must
    // not trust the writer): iron_helmet's slot is "head", not "legs".
    Fixture f;
    SaveGame s = makeSave(f);
    s.playerArmor[2] = "iron_helmet";
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    auto out = parseSave(*j, f.items, f.blocks, f.effects, 42);
    ASSERT_FALSE(out.ok());
    EXPECT_NE(out.error().message.find("player.armor.legs"),
              std::string::npos)
        << out.error().message;
    EXPECT_NE(out.error().message.find("iron_helmet"), std::string::npos)
        << out.error().message;
}

TEST(SaveFormat16Test, ArmorUnknownItemRejected) {
    Fixture f;
    SaveGame s = makeSave(f);
    s.playerArmor[0] = "nope"; // unregistered
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    auto out = parseSave(*j, f.items, f.blocks, f.effects, 42);
    ASSERT_FALSE(out.ok());
    EXPECT_NE(out.error().message.find("nope"), std::string::npos)
        << out.error().message;
}

TEST(SaveFormat16Test, ArmorUnknownSlotKeyRejected) {
    // A key outside the four-slot vocabulary (a typo or a future slot) is
    // rejected and named.
    Fixture f;
    const SaveGame s = makeSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    Json root = *j;
    root["player"]["armor"]["hood"] = Json("iron_helmet");
    auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
    ASSERT_FALSE(out.ok());
    EXPECT_NE(out.error().message.find("hood"), std::string::npos)
        << out.error().message;
}

// -- CRAFT-1: the per-block crafting-table grids (format 19) -----------------
//
// makeSave has no table blocks; these tests add them. The format-19
// cross-check is BIDIRECTIONAL: exactly one entry per crafting_table
// block in the block edits (entries without a block and blocks without
// an entry are both rejected — the furnaces-key discipline, the Phase
// 44/71 twin).

TEST(SaveRoundTripTest, TablesRoundTrip) {
    Fixture f;
    SaveGame s = makeTableSave(f);
    // The first table holds items (a mid-craft state); the second stays
    // empty.
    s.tables[0].grid.setSlot(0, ItemStack {f.items.idForName("dirt"), 5});
    s.tables[0].grid.setSlot(8, ItemStack {f.items.idForName("stone"), 64});
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok()) << j.error().message;
    // CRAFT-1: writes are always format 19.
    EXPECT_EQ(j->find("format")->asInt(), 19);
    auto out = parseSave(*j, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    ASSERT_EQ(out->tables.size(), 2u);
    const BlockPos p1 {1, 53, 2};
    const BlockPos p2 {2, 53, 2};
    EXPECT_EQ(out->tables[0].pos, p1);
    EXPECT_EQ(out->tables[1].pos, p2);
    for (int c = 0; c < CraftingGrid::kSize; ++c) {
        EXPECT_EQ(out->tables[0].grid.at(c), s.tables[0].grid.at(c))
            << "table #1 cell " << c;
        EXPECT_TRUE(out->tables[1].grid.at(c).empty())
            << "table #2 cell " << c;
    }
}

TEST(SaveRoundTripTest, EmptyTablesRoundTrip) {
    Fixture f;
    const SaveGame s = makeSave(f); // no table blocks
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok()) << j.error().message;
    // A world without table blocks still carries the (empty) key — the
    // format-19 discipline (the furnaces/campfires-key pattern).
    ASSERT_NE(j->find("tables"), nullptr);
    EXPECT_EQ(j->find("tables")->size(), 0u);
    auto out = parseSave(*j, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    EXPECT_TRUE(out->tables.empty());
}

TEST(SaveParseTest, Format19RequiresTablesKey) {
    // A format-19 file WITHOUT the "tables" key is rejected (the key is
    // required; an empty array is valid).
    Fixture f;
    const SaveGame s = makeSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    Json root = withoutKey(*j, "tables");
    EXPECT_FALSE(parseSave(root, f.items, f.blocks, f.effects, 42).ok());
}

TEST(SaveParseTest, Format19RejectsTableGridKey) {
    // A format-19 file carrying the legacy "table_grid" is a version lie
    // (the key is restricted to format 18 and older — replaced by the
    // per-block "tables" array).
    Fixture f;
    const SaveGame s = makeSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    Json root = *j;
    root["table_grid"] = emptyTableGrid();
    auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
    ASSERT_FALSE(out.ok());
    EXPECT_NE(out.error().message.find("table_grid"), std::string::npos)
        << out.error().message;
}

TEST(SaveParseTest, RejectsTablesFieldInFormat18) {
    // The version-lie discipline, the format-18 side: a pre-19 file
    // carrying the "tables" key is rejected (named).
    Fixture f;
    const SaveGame s = makeSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    Json root = *j;
    root["format"] = Json(18); // keeps the "tables" field
    root["table_grid"] = emptyTableGrid();
    auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
    ASSERT_FALSE(out.ok());
    EXPECT_NE(out.error().message.find("format 19 or newer"),
              std::string::npos)
        << out.error().message;
}

TEST(SaveParseTest, RejectsTableEntryBlockMismatch) {
    // The bidirectional cross-check: an entry whose position holds no
    // table block, or a table block with no entry.
    Fixture f;
    {
        SaveGame s = makeSave(f);
        s.tables.push_back(SavedTable {BlockPos {5, 53, 5}, CraftingGrid {}});
        auto j = serializeSave(s, f.items);
        ASSERT_TRUE(j.ok());
        auto out = parseSave(*j, f.items, f.blocks, f.effects, 42);
        EXPECT_FALSE(out.ok());
        EXPECT_NE(out.error().message.find("not a crafting_table block"),
                  std::string::npos)
            << out.error().message;
    }
    {
        SaveGame s = makeTableSave(f);
        s.tables.pop_back(); // the second block has no entry
        auto j = serializeSave(s, f.items);
        ASSERT_TRUE(j.ok());
        auto out = parseSave(*j, f.items, f.blocks, f.effects, 42);
        EXPECT_FALSE(out.ok());
        EXPECT_NE(out.error().message.find("has no 'tables' entry"),
                  std::string::npos)
            << out.error().message;
    }
}

TEST(SaveParseTest, RejectsDuplicateTableEntries) {
    Fixture f;
    SaveGame s = makeTableSave(f);
    s.tables.push_back(SavedTable {s.tables[0].pos, CraftingGrid {}});
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    auto out = parseSave(*j, f.items, f.blocks, f.effects, 42);
    EXPECT_FALSE(out.ok());
}

TEST(SaveParseTest, RejectsBadTableSlots) {
    Fixture f;
    const SaveGame s = makeTableSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    // Rebuild the "tables" array with entry #0's slots replaced.
    const auto withSlots = [&](Json slots) {
        Json root = *j;
        Json entries = Json::array();
        for (std::size_t i = 0; i < root["tables"].size(); ++i) {
            Json entry = withoutKey(root["tables"].at(i), "slots");
            if (i == 0) {
                entry["slots"] = slots;
            }
            entries.push_back(std::move(entry));
        }
        root["tables"] = std::move(entries);
        return root;
    };
    // Wrong size (8 cells).
    {
        Json shortSlots = Json::array();
        for (int i = 0; i < CraftingGrid::kSize - 1; ++i) {
            Json e = Json::object();
            e["item"] = Json(std::string(""));
            e["count"] = Json(0);
            shortSlots.push_back(std::move(e));
        }
        EXPECT_FALSE(parseSave(withSlots(shortSlots), f.items, f.blocks,
                               f.effects, 42)
                         .ok());
    }
    // Unknown item in a cell.
    {
        Json slots = *j->find("tables")->at(0).find("slots");
        Json e = Json::object();
        e["item"] = Json(std::string("nope"));
        e["count"] = Json(1);
        Json rebuilt = Json::array();
        rebuilt.push_back(std::move(e));
        for (std::size_t i = 1; i < slots.size(); ++i) {
            rebuilt.push_back(slots.at(i));
        }
        EXPECT_FALSE(parseSave(withSlots(std::move(rebuilt)), f.items, f.blocks,
                               f.effects, 42)
                         .ok());
    }
    // Count above the tool's max stack (1).
    {
        Json slots = *j->find("tables")->at(0).find("slots");
        Json e = Json::object();
        e["item"] = Json(std::string("pick"));
        e["count"] = Json(2);
        Json rebuilt = Json::array();
        rebuilt.push_back(std::move(e));
        for (std::size_t i = 1; i < slots.size(); ++i) {
            rebuilt.push_back(slots.at(i));
        }
        EXPECT_FALSE(parseSave(withSlots(std::move(rebuilt)), f.items, f.blocks,
                               f.effects, 42)
                         .ok());
    }
    // Empty item with a positive count.
    {
        Json slots = *j->find("tables")->at(0).find("slots");
        Json e = Json::object();
        e["item"] = Json(std::string(""));
        e["count"] = Json(1);
        Json rebuilt = Json::array();
        rebuilt.push_back(std::move(e));
        for (std::size_t i = 1; i < slots.size(); ++i) {
            rebuilt.push_back(slots.at(i));
        }
        EXPECT_FALSE(parseSave(withSlots(std::move(rebuilt)), f.items, f.blocks,
                               f.effects, 42)
                         .ok());
    }
    // A "damage" key on a grid slot: the SURV-3 grid scope (the crafting
    // grids do not carry durability).
    {
        Json slots = *j->find("tables")->at(0).find("slots");
        Json e = Json::object();
        e["item"] = Json(std::string("pick"));
        e["count"] = Json(1);
        e["damage"] = Json(1.0);
        Json rebuilt = Json::array();
        rebuilt.push_back(std::move(e));
        for (std::size_t i = 1; i < slots.size(); ++i) {
            rebuilt.push_back(slots.at(i));
        }
        EXPECT_FALSE(parseSave(withSlots(std::move(rebuilt)), f.items, f.blocks,
                               f.effects, 42)
                         .ok());
    }
    // An unknown field on the entry itself.
    {
        Json root = *j;
        Json entry = root["tables"].at(0);
        entry["extra"] = Json(1);
        Json entries = Json::array();
        entries.push_back(std::move(entry));
        entries.push_back(root["tables"].at(1));
        root["tables"] = std::move(entries);
        auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
        EXPECT_FALSE(out.ok());
        EXPECT_NE(out.error().message.find("extra"), std::string::npos)
            << out.error().message;
    }
}

// -- CRAFT-1: the format-18 migration (sole attribution, Phase 44) ---------

TEST(SaveParseTest, Format18To19MigrationSoleAttribution) {
    // A format-18 file's ONE global table_grid, when non-empty, belonged
    // to the world's (single) table — it is assigned to that block's
    // entry (the Phase 44 sole-attribution precedent).
    Fixture f;
    SaveGame s = makeSave(f);
    s.blockEdits.push_back({BlockPos {1, 53, 2}, "crafting_table"});
    std::sort(s.blockEdits.begin(), s.blockEdits.end(),
              [](const SavedBlockEdit& a, const SavedBlockEdit& b) {
                  return a.pos < b.pos;
              });
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    Json root = downgradeTablesToFormat18(
        *j, legacyTableGrid({{0, {"dirt", 5}}, {8, {"stone", 64}}}));
    auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    ASSERT_EQ(out->tables.size(), 1u);
    EXPECT_EQ(out->tables[0].pos, (BlockPos {1, 53, 2}));
    EXPECT_EQ(out->tables[0].grid.at(0),
              (ItemStack {f.items.idForName("dirt"), 5}));
    EXPECT_EQ(out->tables[0].grid.at(8),
              (ItemStack {f.items.idForName("stone"), 64}));
    EXPECT_TRUE(out->tables[0].grid.at(4).empty());
}

TEST(SaveParseTest, Format18To19MigrationDiscardedWithSeveralTables) {
    // With SEVERAL table blocks the owner of the global grid is
    // unknowable — it is discarded (all grids load empty).
    Fixture f;
    SaveGame s = makeTableSave(f); // two table blocks
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    Json root = downgradeTablesToFormat18(*j,
                                          legacyTableGrid({{0, {"dirt", 5}}}));
    auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    ASSERT_EQ(out->tables.size(), 2u);
    EXPECT_TRUE(out->tables[0].grid.empty());
    EXPECT_TRUE(out->tables[1].grid.empty());
}

TEST(SaveParseTest, Format18To19MigrationDiscardedWithNoTables) {
    // With ZERO table blocks there is nowhere to put it — the grid is
    // discarded (the entry list stays empty).
    Fixture f;
    const SaveGame s = makeSave(f); // no table blocks
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    Json root = downgradeTablesToFormat18(*j,
                                          legacyTableGrid({{8, {"stone", 64}}}));
    auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    EXPECT_TRUE(out->tables.empty());
}

TEST(SaveParseTest, Format18To19MigrationEmptyLegacyGridLoadsEmpty) {
    // An EMPTY legacy grid: nothing to attribute — the entry loads empty
    // (the migration is a no-op on all-empty grids).
    Fixture f;
    SaveGame s = makeSave(f);
    s.blockEdits.push_back({BlockPos {1, 53, 2}, "crafting_table"});
    std::sort(s.blockEdits.begin(), s.blockEdits.end(),
              [](const SavedBlockEdit& a, const SavedBlockEdit& b) {
                  return a.pos < b.pos;
              });
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    Json root = downgradeTablesToFormat18(*j, emptyTableGrid());
    auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    ASSERT_EQ(out->tables.size(), 1u);
    EXPECT_TRUE(out->tables[0].grid.empty());
}
