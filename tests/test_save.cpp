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

// air (built-in) + stone + dirt.
BlockRegistry makeBlocks() {
    BlockRegistry blocks;
    // Test setup; on failure the id lookups below assert anyway.
    blocks.registerBlock(parseJson(
        R"({"id":"stone","model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    blocks.registerBlock(parseJson(
        R"({"id":"dirt","model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    return blocks;
}

struct Fixture {
    BlockRegistry blocks = makeBlocks();
    ItemRegistry items;

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
    s.fluidEdits = {{BlockPos {4, 50, 4}, 5}};
    return s;
}

// Copies a JSON object minus one key (Json has no erase).
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
    auto out = parseSave(*parsed, f.items, f.blocks, 42);
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
    // Format 4 is a future version this build cannot read; a missing
    // format is a corrupt file. (Formats 1, 2 and 3 are all accepted —
    // see FormatOneLoadsWithoutHealth.)
    Json root = parseJson(R"({"format":4,"seed":42,"time":0.25,"player":{},"inventory":{},"entities":[],"blocks":[],"fluids":[]})");
    EXPECT_FALSE(parseSave(root, f.items, f.blocks, 42).ok());
    EXPECT_FALSE(parseSave(withoutKey(root, "format"), f.items, f.blocks, 42).ok());
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
    // Format 3 writes are always format 3.
    EXPECT_EQ(j->find("format")->asInt(), 3);
    auto out = parseSave(*j, f.items, f.blocks, 42);
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
    auto out = parseSave(*j, f.items, f.blocks, 42);
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
    root = withoutKey(root, "inv_grid");
    root = withoutKey(root, "table_grid");
    auto out = parseSave(root, f.items, f.blocks, 42);
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
    EXPECT_FALSE(parseSave(missingInv, f.items, f.blocks, 42).ok());
    Json missingTable = *j;
    missingTable = withoutKey(missingTable, "table_grid");
    EXPECT_FALSE(parseSave(missingTable, f.items, f.blocks, 42).ok());
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
        EXPECT_FALSE(parseSave(withGrid(shortGrid), f.items, f.blocks, 42).ok());
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
        EXPECT_FALSE(parseSave(bad, f.items, f.blocks, 42).ok());
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
        EXPECT_FALSE(parseSave(bad, f.items, f.blocks, 42).ok());
    }
}

TEST(SaveRoundTripTest, HealthRoundTrips) {
    Fixture f;
    SaveGame s = makeSave(f);
    s.playerHealth = 13.5f;
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok()) << j.error().message;
    auto out = parseSave(*j, f.items, f.blocks, 42);
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
    root = withoutKey(root, "inv_grid"); // format 1 predates the grids
    root = withoutKey(root, "table_grid");
    auto out = parseSave(root, f.items, f.blocks, 42);
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
    EXPECT_FALSE(parseSave(root, f.items, f.blocks, 42).ok());
}

TEST(SaveParseTest, RejectsSeedMismatch) {
    Fixture f;
    const SaveGame s = makeSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    EXPECT_FALSE(parseSave(*j, f.items, f.blocks, 43).ok());
}

TEST(SaveParseTest, RejectsUnknownTopLevelField) {
    Fixture f;
    const SaveGame s = makeSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    Json root = *j;
    root["extra"] = Json(1);
    EXPECT_FALSE(parseSave(root, f.items, f.blocks, 42).ok());
}

TEST(SaveParseTest, RejectsMissingRequiredFields) {
    Fixture f;
    const SaveGame s = makeSave(f);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    for (const char* key :
         {"seed", "time", "player", "inventory", "entities", "blocks", "fluids"}) {
        EXPECT_FALSE(
            parseSave(withoutKey(*j, key), f.items, f.blocks, 42).ok())
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
    EXPECT_FALSE(parseSave(bad, f.items, f.blocks, 42).ok());
    bad = *j;
    bad["time"] = Json(-0.1);
    EXPECT_FALSE(parseSave(bad, f.items, f.blocks, 42).ok());
    bad = *j;
    bad["time"] = Json(std::string("noon"));
    EXPECT_FALSE(parseSave(bad, f.items, f.blocks, 42).ok());
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
    EXPECT_FALSE(parseSave(bad, f.items, f.blocks, 42).ok());
    bad = *j;
    bad["player"] = withoutKey(bad["player"], "yaw");
    EXPECT_FALSE(parseSave(bad, f.items, f.blocks, 42).ok());
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
    EXPECT_FALSE(parseSave(withSlots(slots26), f.items, f.blocks, 42).ok());

    // selected must be a hotbar index.
    Json bad = *j;
    bad["inventory"]["selected"] = Json(9);
    EXPECT_FALSE(parseSave(bad, f.items, f.blocks, 42).ok());

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
        parseSave(withSlots(slotsWith("nope", 1)), f.items, f.blocks, 42).ok());
    EXPECT_FALSE(
        parseSave(withSlots(slotsWith("stone", 0)), f.items, f.blocks, 42).ok());
    EXPECT_FALSE(
        parseSave(withSlots(slotsWith("pick", 2)), f.items, f.blocks, 42).ok());
    EXPECT_FALSE(
        parseSave(withSlots(slotsWith("", 3)), f.items, f.blocks, 42).ok());
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
        EXPECT_FALSE(parseSave(withSlots(rebuilt), f.items, f.blocks, 42).ok());
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
                           f.items, f.blocks, 42)
                     .ok());
    EXPECT_FALSE(
        parseSave(withEntity(entity("item", "nope", 1, 1.0, 0.1)), f.items,
                  f.blocks, 42)
            .ok());
    EXPECT_FALSE(
        parseSave(withEntity(entity("item", "dirt", 0, 1.0, 0.1)), f.items,
                  f.blocks, 42)
            .ok());
    EXPECT_FALSE(
        parseSave(withEntity(entity("item", "dirt", 65, 1.0, 0.1)), f.items,
                  f.blocks, 42)
            .ok());
    EXPECT_FALSE(
        parseSave(withEntity(entity("item", "pick", 2, 1.0, 0.1)), f.items,
                  f.blocks, 42)
            .ok()); // above the tool's max stack
    EXPECT_FALSE(
        parseSave(withEntity(entity("item", "dirt", 1, 120.0, 0.1)), f.items,
                  f.blocks, 42)
            .ok()); // at the despawn age
    EXPECT_FALSE(
        parseSave(withEntity(entity("item", "dirt", 1, 1.0, 0.6)), f.items,
                  f.blocks, 42)
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
    EXPECT_FALSE(parseSave(withBlock(edit(0, 128, 0, "stone")), f.items, f.blocks, 42).ok());
    EXPECT_FALSE(parseSave(withBlock(edit(0, -1, 0, "stone")), f.items, f.blocks, 42).ok());
    EXPECT_FALSE(parseSave(withBlock(edit(0, 10, 0, "nope")), f.items, f.blocks, 42).ok());
    {
        Json shortEdit = Json::array();
        shortEdit.push_back(Json(0));
        shortEdit.push_back(Json(10));
        shortEdit.push_back(Json(0));
        EXPECT_FALSE(parseSave(withBlock(shortEdit), f.items, f.blocks, 42).ok());
    }
    {
        // Duplicate positions.
        Json root = *j;
        Json arr = Json::array();
        arr.push_back(edit(1, 10, 1, "stone"));
        arr.push_back(edit(1, 10, 1, "dirt"));
        root["blocks"] = std::move(arr);
        EXPECT_FALSE(parseSave(root, f.items, f.blocks, 42).ok());
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
    auto edit = [](int x, int y, int z, Json level) {
        Json a = Json::array();
        a.push_back(Json(x));
        a.push_back(Json(y));
        a.push_back(Json(z));
        a.push_back(std::move(level));
        return a;
    };
    EXPECT_FALSE(
        parseSave(withFluids({edit(0, 10, 0, Json(9))}), f.items, f.blocks, 42).ok());
    EXPECT_FALSE(parseSave(withFluids({edit(0, 10, 0, Json(2.5))}), f.items,
                          f.blocks, 42)
                     .ok());
    EXPECT_FALSE(
        parseSave(withFluids({edit(1, 10, 1, Json(3)), edit(1, 10, 1, Json(4))}),
                  f.items, f.blocks, 42)
            .ok()); // duplicate cell
    // A block edit and a fluid edit on the same cell: a block displaces
    // fluid, so the save would be corrupt.
    {
        Json root = *j;
        Json barr = Json::array();
        barr.push_back(edit(1, 10, 1, "stone"));
        root["blocks"] = std::move(barr);
        root["fluids"] =
            withFluids({edit(1, 10, 1, Json(3))})["fluids"];
        EXPECT_FALSE(parseSave(root, f.items, f.blocks, 42).ok());
    }
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
        c.setFluidAt(0, 5, 0, kMaxFluidLevel);
        c.setFluidAt(1, 5, 0, kMaxFluidLevel);
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
    ASSERT_TRUE(world.setFluid(0, 5, 0, 3));
    ASSERT_TRUE(world.setFluid(1, 5, 0, 0));

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
    EXPECT_EQ(fluidEdits[0].level, 3);
    EXPECT_EQ(fluidEdits[1].pos, (BlockPos {1, 5, 0}));
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

TEST(SaveWorldDiffTest, ApplyRestoresTheModifiedWorld) {
    Fixture f;
    const BlockID stone = f.blocks.idForName("stone");
    const BlockID dirt = f.blocks.idForName("dirt");
    World world = makeWorld(stone);
    ASSERT_TRUE(world.setBlock(0, 0, 0, BlockID::air()));
    ASSERT_TRUE(world.setBlock(2, 5, 0, dirt));
    ASSERT_TRUE(world.setFluid(0, 5, 0, 3));
    ASSERT_TRUE(world.setFluid(1, 5, 0, 0));

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
    save.fluidEdits = {{BlockPos {0, 2, 0}, 5}};
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
    save.fluidEdits = {{BlockPos {64, 5, 0}, 3}};
    auto r = applyWorldEdits(save, f.blocks, world);
    EXPECT_FALSE(r.ok());
}
