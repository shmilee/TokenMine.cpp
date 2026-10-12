// Item/inventory system tests (Phase 11): the item registry (strict
// validation, implicit block items, explicit tools, directory loading),
// the inventory's merge/overflow/take rules, and the break-time formula.
#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "block/block_registry.hpp"
#include "core/json.hpp"
#include "item/inventory.hpp"
#include "item/item.hpp"
#include "item/item_registry.hpp"

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

// stone + dirt: two distinct data blocks for the implicit block items.
BlockRegistry makeBlocks() {
    BlockRegistry blocks;
    auto r = blocks.registerBlock(parseJson(
        R"({"id":"stone","model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    if (!r.ok()) {
        ADD_FAILURE() << r.error().message;
    }
    r = blocks.registerBlock(parseJson(
        R"({"id":"dirt","model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    if (!r.ok()) {
        ADD_FAILURE() << r.error().message;
    }
    return blocks;
}

struct InvFixture {
    BlockRegistry blocks = makeBlocks();
    ItemRegistry items;
    Inventory inv;
    ItemID dirt;
    ItemID stone;

    InvFixture() {
        auto r = items.buildBlockItems(blocks);
        if (!r.ok()) {
            ADD_FAILURE() << r.error().message;
        }
        dirt = items.idForName("dirt");
        stone = items.idForName("stone");
    }
};

} // namespace

// -- ItemRegistry -------------------------------------------------------------

TEST(ItemRegistryTest, BlockItemsAreBuiltFromBlocks) {
    BlockRegistry blocks = makeBlocks();
    ItemRegistry items;
    auto r = items.buildBlockItems(blocks);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(items.count(), 2u); // no item for the air sentinel
    EXPECT_FALSE(items.idForName("air").valid());
    ItemID stone = items.idForName("stone");
    ASSERT_TRUE(stone.valid());
    const Item& it = items.at(stone);
    EXPECT_EQ(it.name, "stone");
    EXPECT_EQ(it.type, ItemType::Block);
    EXPECT_EQ(it.block, blocks.idForName("stone"));
    EXPECT_FLOAT_EQ(it.breakSpeed, 1.0f);
    EXPECT_EQ(it.maxStack, 64);
}

TEST(ItemRegistryTest, RegisterTool) {
    ItemRegistry items;
    auto r = items.registerItem(parseJson(
        R"({"id":"pick","type":"tool","tool":{"break_speed":2.0},"max_stack":1})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(items.count(), 1u);
    const Item& it = items.at(*r);
    EXPECT_EQ(it.name, "pick");
    EXPECT_EQ(it.type, ItemType::Tool);
    EXPECT_EQ(it.block, BlockID::invalid()); // tools place nothing
    EXPECT_FLOAT_EQ(it.breakSpeed, 2.0f);
    EXPECT_EQ(it.maxStack, 1);
    EXPECT_EQ(items.idForName("pick"), *r);
    EXPECT_NE(items.findByName("pick"), nullptr);
    EXPECT_FALSE(items.idForName("nope").valid());
    EXPECT_EQ(items.find(ItemID{0}), &items.at(*r));
}

TEST(ItemRegistryTest, ToolFieldsDefaultWhenAbsent) {
    ItemRegistry items;
    auto r = items.registerItem(parseJson(R"({"id":"pick","type":"tool"})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_FLOAT_EQ(items.at(*r).breakSpeed, 1.0f);
    EXPECT_EQ(items.at(*r).maxStack, 64);
}

TEST(ItemRegistryTest, RejectsExplicitItemCollidingWithBlock) {
    BlockRegistry blocks = makeBlocks();
    ItemRegistry items;
    ASSERT_TRUE(items.buildBlockItems(blocks).ok());
    auto r = items.registerItem(parseJson(R"({"id":"stone","type":"tool"})"));
    EXPECT_FALSE(r.ok()); // block items are defined by their block
}

TEST(ItemRegistryTest, RejectsDuplicateToolId) {
    ItemRegistry items;
    ASSERT_TRUE(items.registerItem(parseJson(R"({"id":"pick","type":"tool"})"))
                    .ok());
    auto r = items.registerItem(parseJson(R"({"id":"pick","type":"tool"})"));
    EXPECT_FALSE(r.ok());
}

TEST(ItemRegistryTest, RejectsMissingOrWrongType) {
    ItemRegistry items;
    EXPECT_FALSE(items.registerItem(parseJson(R"({"id":"pick"})")).ok());
    EXPECT_FALSE(
        items.registerItem(parseJson(R"({"id":"pick","type":"block"})"))
            .ok()); // block items are implicit
    EXPECT_FALSE(
        items.registerItem(parseJson(R"({"id":"pick","type":42})")).ok());
}

TEST(ItemRegistryTest, RejectsBadBreakSpeed) {
    ItemRegistry items;
    EXPECT_FALSE(
        items.registerItem(parseJson(
            R"({"id":"pick","type":"tool","tool":{"break_speed":0}})"))
            .ok());
    EXPECT_FALSE(
        items.registerItem(parseJson(
            R"({"id":"pick","type":"tool","tool":{"break_speed":-2}})"))
            .ok());
    EXPECT_FALSE(
        items.registerItem(parseJson(
            R"({"id":"pick","type":"tool","tool":{"break_speed":"fast"}})"))
            .ok());
}

TEST(ItemRegistryTest, RejectsBadMaxStack) {
    ItemRegistry items;
    EXPECT_FALSE(
        items.registerItem(
            parseJson(R"({"id":"pick","type":"tool","max_stack":0})"))
            .ok());
    EXPECT_FALSE(
        items.registerItem(
            parseJson(R"({"id":"pick","type":"tool","max_stack":65})"))
            .ok());
    EXPECT_FALSE(
        items.registerItem(
            parseJson(R"({"id":"pick","type":"tool","max_stack":"one"})"))
            .ok());
}

TEST(ItemRegistryTest, RejectsMissingId) {
    ItemRegistry items;
    EXPECT_FALSE(items.registerItem(parseJson(R"({"type":"tool"})")).ok());
    EXPECT_FALSE(
        items.registerItem(parseJson(R"({"id":"","type":"tool"})")).ok());
}

TEST(ItemRegistryTest, LoadDirectory) {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        ("mc_item_test_" + std::to_string(::getpid()));
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "data" / "items");
    {
        std::ofstream f(root / "data/items/alpha_pick.json");
        f << R"({"id":"alpha_pick","type":"tool","tool":{"break_speed":3.0},"max_stack":1})";
    }
    {
        std::ofstream f(root / "data/items/beta_pick.json");
        f << R"({"id":"beta_pick","type":"tool"})";
    }
    {
        std::ofstream f(root / "data/items/ignore.txt");
        f << "not an item definition";
    }

    AssetManager assets;
    ASSERT_TRUE(assets.setRoot(root).ok());
    ItemRegistry items;
    auto r = items.loadDirectory(assets, "data/items");
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(items.count(), 2u); // non-JSON files are skipped
    // Files load in sorted order: alpha_pick gets id 0.
    EXPECT_EQ(items.at(ItemID{0}).name, "alpha_pick");
    EXPECT_EQ(items.at(ItemID{1}).name, "beta_pick");
    EXPECT_FLOAT_EQ(items.at(ItemID{0}).breakSpeed, 3.0f);
    EXPECT_FLOAT_EQ(items.at(ItemID{1}).breakSpeed, 1.0f); // default
    std::filesystem::remove_all(root);
}

TEST(ItemRegistryTest, LoadDirectoryMissingDirIsEmpty) {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        ("mc_item_empty_" + std::to_string(::getpid()));
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    AssetManager assets;
    ASSERT_TRUE(assets.setRoot(root).ok());
    ItemRegistry items;
    auto r = items.loadDirectory(assets, "data/items");
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(items.count(), 0u);
    std::filesystem::remove_all(root);
}

TEST(ItemRegistryTest, LoadDirectoryRejectsInvalidJson) {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        ("mc_item_bad_" + std::to_string(::getpid()));
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "data" / "items");
    {
        std::ofstream f(root / "data/items/bad.json");
        f << "{ this is not json";
    }

    AssetManager assets;
    ASSERT_TRUE(assets.setRoot(root).ok());
    ItemRegistry items;
    auto r = items.loadDirectory(assets, "data/items");
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("bad.json"), std::string::npos);
    std::filesystem::remove_all(root);
}

// -- ItemStack -----------------------------------------------------------------

TEST(ItemStackTest, EmptyAndEquality) {
    ItemStack empty;
    EXPECT_TRUE(empty.empty());
    ItemStack zero {ItemID {1}, 0};
    EXPECT_TRUE(zero.empty());
    ItemStack negative {ItemID {1}, -1};
    EXPECT_TRUE(negative.empty());

    ItemStack a {ItemID {1}, 3};
    EXPECT_FALSE(a.empty());
    EXPECT_EQ(a, (ItemStack {ItemID {1}, 3}));
    EXPECT_NE(a, (ItemStack {ItemID {1}, 4}));
    EXPECT_NE(a, (ItemStack {ItemID {2}, 3}));
}

// -- Inventory -------------------------------------------------------------------

TEST(InventoryTest, AddGoesToFirstEmptySlot) {
    InvFixture f;
    EXPECT_EQ(f.inv.size(), static_cast<std::size_t>(Inventory::kTotalSize));
    int slot = f.inv.add(f.dirt, 1, f.items);
    EXPECT_EQ(slot, 0);
    EXPECT_EQ(f.inv.at(0), (ItemStack {f.dirt, 1}));
}

TEST(InventoryTest, AddMergesIntoPartialStackBeforeUsingEmptySlots) {
    InvFixture f;
    f.inv.setSlot(5, (ItemStack {f.dirt, 3}));
    int slot = f.inv.add(f.dirt, 4, f.items);
    EXPECT_EQ(slot, 5); // merged, not placed into slot 0
    EXPECT_EQ(f.inv.at(5), (ItemStack {f.dirt, 7}));
    EXPECT_TRUE(f.inv.isEmpty(0));
}

TEST(InventoryTest, AddSkipsFullStacksAndUsesFirstEmpty) {
    InvFixture f;
    f.inv.setSlot(0, (ItemStack {f.dirt, 64})); // full dirt
    f.inv.setSlot(1, (ItemStack {f.stone, 64}));
    f.inv.setSlot(2, (ItemStack {f.stone, 64}));
    int slot = f.inv.add(f.dirt, 1, f.items);
    EXPECT_EQ(slot, 3); // slot 0's dirt is full; slot 3 is the first empty
    slot = f.inv.add(f.dirt, 10, f.items);
    EXPECT_EQ(slot, 3); // now merges into the partial stack
    EXPECT_EQ(f.inv.at(3), (ItemStack {f.dirt, 11}));
}

TEST(InventoryTest, AddFailsWhenFull) {
    InvFixture f;
    f.inv.setSlot(0, (ItemStack {f.dirt, 64}));
    for (std::size_t i = 1; i < f.inv.size(); ++i) {
        f.inv.setSlot(i, (ItemStack {f.stone, 64}));
    }
    EXPECT_TRUE(f.inv.isFull(f.dirt, f.items));
    EXPECT_EQ(f.inv.add(f.dirt, 1, f.items), -1);
    EXPECT_EQ(f.inv.at(0), (ItemStack {f.dirt, 64})); // untouched

    // Making room (take one stone out of the last slot) changes nothing for
    // dirt (the stone slot is not dirt), but freeing a dirt slot does:
    f.inv.setSlot(1, (ItemStack {f.dirt, 64}));
    f.inv.take(1, 1);
    EXPECT_FALSE(f.inv.isFull(f.dirt, f.items));
    EXPECT_EQ(f.inv.add(f.dirt, 1, f.items), 1);
    EXPECT_EQ(f.inv.at(1), (ItemStack {f.dirt, 64}));
}

TEST(InventoryTest, TakePartialAndClampsToEmpty) {
    InvFixture f;
    f.inv.setSlot(0, (ItemStack {f.dirt, 5}));
    EXPECT_EQ(f.inv.take(0, 2), 2);
    EXPECT_EQ(f.inv.at(0), (ItemStack {f.dirt, 3}));
    EXPECT_EQ(f.inv.take(0, 10), 3); // clamped to what is there
    EXPECT_TRUE(f.inv.isEmpty(0));
    EXPECT_FALSE(f.inv.at(0).item.valid());
    EXPECT_EQ(f.inv.take(0, 1), 0); // empty slot
}

TEST(InventoryTest, IsFullSeesEmptySlotsAsRoom) {
    InvFixture f;
    EXPECT_FALSE(f.inv.isFull(f.dirt, f.items)); // all empty: room for anything
    // Fill everything with full dirt stacks except the last slot.
    for (std::size_t i = 0; i + 1 < f.inv.size(); ++i) {
        f.inv.setSlot(i, (ItemStack {f.dirt, 64}));
    }
    EXPECT_FALSE(f.inv.isFull(f.dirt, f.items)); // one empty slot left
    f.inv.setSlot(f.inv.size() - 1, (ItemStack {f.dirt, 64}));
    EXPECT_TRUE(f.inv.isFull(f.dirt, f.items)); // now really full
    EXPECT_TRUE(f.inv.isFull(f.stone, f.items)); // no empty slot either
    // A partially freed dirt slot is room for dirt (but not for stone).
    f.inv.take(0, 1);
    EXPECT_FALSE(f.inv.isFull(f.dirt, f.items));
    EXPECT_TRUE(f.inv.isFull(f.stone, f.items));
}

// -- break time --------------------------------------------------------------------

TEST(BreakDurationTest, ZeroHardnessBreaksInstantly) {
    EXPECT_DOUBLE_EQ(breakDurationSeconds(0.0f, 1.0f), 0.0);
    EXPECT_DOUBLE_EQ(breakDurationSeconds(-1.0f, 2.0f), 0.0);
}

TEST(BreakDurationTest, ScalesWithHardnessAndSpeed) {
    // All values are exact in binary, so EXPECT_DOUBLE_EQ is safe.
    EXPECT_DOUBLE_EQ(breakDurationSeconds(1.0f, 1.0f), kBreakBaseSeconds);
    EXPECT_DOUBLE_EQ(breakDurationSeconds(0.5f, 1.0f), 0.75);
    EXPECT_DOUBLE_EQ(breakDurationSeconds(1.5f, 1.0f), 2.25);
    EXPECT_DOUBLE_EQ(breakDurationSeconds(1.5f, 2.0f), 1.125);
    EXPECT_DOUBLE_EQ(breakDurationSeconds(1.5f, 4.0f), 0.5625);
}

// -- material items (Phase 12) ----------------------------------------------

TEST(ItemRegistryTest, RegisterMaterial) {
    ItemRegistry items;
    auto r = items.registerItem(parseJson(R"({"id":"stick","type":"material"})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const Item& it = items.at(*r);
    EXPECT_EQ(it.name, "stick");
    EXPECT_EQ(it.type, ItemType::Material);
    EXPECT_EQ(it.block, BlockID::invalid()); // materials place nothing
    EXPECT_FLOAT_EQ(it.breakSpeed, 1.0f);
    EXPECT_EQ(it.maxStack, 64); // default
}

TEST(ItemRegistryTest, MaterialMaxStackIsDataDriven) {
    ItemRegistry items;
    auto r = items.registerItem(
        parseJson(R"({"id":"gem","type":"material","max_stack":16})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(items.at(*r).maxStack, 16);
}

TEST(ItemRegistryTest, MaterialRejectsToolField) {
    ItemRegistry items;
    auto r = items.registerItem(parseJson(
        R"({"id":"gem","type":"material","tool":{"break_speed":2.0}})"));
    EXPECT_FALSE(r.ok());
}

TEST(ItemRegistryTest, RejectsUnknownItemType) {
    ItemRegistry items;
    EXPECT_FALSE(
        items.registerItem(parseJson(R"({"id":"x","type":"weapon"})")).ok());
}

// -- projectile launching (Phase 21: the bow; charge + ammo Phase 24) --------

TEST(ItemRegistryTest, ParsesShootFields) {
    // The bow: a material that launches Projectiles (data-driven launch).
    ItemRegistry items;
    auto r = items.registerItem(
        parseJson(R"({"id":"bow","type":"material","max_stack":1,
                      "shoot_speed":15.0,"shoot_damage":5.0})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const Item& it = items.at(*r);
    EXPECT_FLOAT_EQ(it.shootSpeed, 15.0f);
    EXPECT_FLOAT_EQ(it.shootDamage, 5.0f);
}

TEST(ItemRegistryTest, ShootFieldsDefaultToZero) {
    // No launch fields: not a launcher (every pre-Phase-21 item).
    ItemRegistry items;
    auto r = items.registerItem(
        parseJson(R"({"id":"gem","type":"material"})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_FLOAT_EQ(items.at(*r).shootSpeed, 0.0f);
    EXPECT_FLOAT_EQ(items.at(*r).shootDamage, 0.0f);
}

TEST(ItemRegistryTest, RejectsBadShootSpeed) {
    ItemRegistry items;
    EXPECT_FALSE(items.registerItem(
                     parseJson(R"({"id":"b","type":"material","shoot_speed":0})"))
                     .ok()); // explicit 0 = a typo (ABSENT is the default)
    EXPECT_FALSE(items.registerItem(
                     parseJson(R"({"id":"b","type":"material","shoot_speed":-1})"))
                     .ok());
    EXPECT_FALSE(items.registerItem(
                     parseJson(R"({"id":"b","type":"material","shoot_speed":101})"))
                     .ok());
    EXPECT_FALSE(items.registerItem(
                     parseJson(
                         R"({"id":"b","type":"material","shoot_speed":"fast"})"))
                     .ok());
}

TEST(ItemRegistryTest, RejectsBadShootDamage) {
    ItemRegistry items;
    EXPECT_FALSE(items.registerItem(
                     parseJson(
                         R"({"id":"b","type":"material","shoot_damage":-1})"))
                     .ok());
    EXPECT_FALSE(items.registerItem(
                     parseJson(
                         R"({"id":"b","type":"material","shoot_damage":101})"))
                     .ok());
    EXPECT_FALSE(items.registerItem(
                     parseJson(
                         R"({"id":"b","type":"material","shoot_damage":"hard"})"))
                     .ok());
}

// -- launcher ammo (Phase 24) ------------------------------------------------

TEST(ItemRegistryTest, ParsesAmmo) {
    // The bow's ammo: a launcher that consumes one `arrow` per shot. The
    // name's existence is cross-checked at game init (the ammo item may
    // be defined in a later file), not here.
    ItemRegistry items;
    auto r = items.registerItem(
        parseJson(R"({"id":"bow","type":"material","max_stack":1,
                      "shoot_speed":15.0,"shoot_damage":5.0,
                      "ammo":"arrow"})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(items.at(*r).ammo, "arrow");
}

TEST(ItemRegistryTest, AmmoDefaultsToEmpty) {
    // No 'ammo' field: the launcher fires without consuming anything
    // (every pre-Phase-24 launcher).
    ItemRegistry items;
    auto r = items.registerItem(
        parseJson(R"({"id":"gem","type":"material","shoot_speed":10.0})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_TRUE(items.at(*r).ammo.empty());
}

TEST(ItemRegistryTest, RejectsBadAmmo) {
    ItemRegistry items;
    // An explicit empty string is a typo (ABSENT is the default), same
    // discipline as restore_hunger / shoot_speed.
    EXPECT_FALSE(items.registerItem(
                     parseJson(R"({"id":"b","type":"material","ammo":""})"))
                     .ok());
    EXPECT_FALSE(items.registerItem(
                     parseJson(R"({"id":"b","type":"material","ammo":5})"))
                     .ok());
}

// -- food (Phase 23) --------------------------------------------------------

TEST(ItemRegistryTest, ParsesFood) {
    // rotten_flesh: a material that feeds the player (data-driven food).
    ItemRegistry items;
    auto r = items.registerItem(
        parseJson(R"({"id":"rotten_flesh","type":"material","max_stack":64,
                      "restore_hunger":4})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(items.at(*r).restoreHunger, 4);
}

TEST(ItemRegistryTest, ParsesMobMeatFoods) {
    // Phase 28: the mob-drop meat foods — beef (cow, +3), mutton (sheep,
    // +2) and chicken (chicken, +2). The saturation gained when eating
    // them is the data-free 2x factor (core/food.hpp), not item data.
    ItemRegistry items;
    const struct {
        const char* id;
        int hunger;
    } cases[] = {{"beef", 3}, {"mutton", 2}, {"chicken", 2}};
    for (const auto& c : cases) {
        std::string def = std::string(R"({"id":")") + c.id +
                          R"(","type":"material","max_stack":64,"restore_hunger":)";
        def += std::to_string(c.hunger) + "}";
        auto r = items.registerItem(parseJson(def));
        ASSERT_TRUE(r.ok()) << c.id << ": " << r.error().message;
        EXPECT_EQ(items.at(*r).restoreHunger, c.hunger);
        EXPECT_EQ(items.at(*r).maxStack, 64);
    }
}

TEST(ItemRegistryTest, ParsesBreadAndWheat) {
    // Phase 48: bread is food (+5, the Minecraft value); wheat is the
    // mature-crop harvest material (not food, crafting input only).
    ItemRegistry items;
    auto bread = items.registerItem(
        parseJson(R"({"id":"bread","type":"material","max_stack":64,
                      "restore_hunger":5})"));
    ASSERT_TRUE(bread.ok()) << bread.error().message;
    EXPECT_EQ(items.at(*bread).restoreHunger, 5);
    EXPECT_EQ(items.at(*bread).maxStack, 64);
    auto wheat = items.registerItem(
        parseJson(R"({"id":"wheat","type":"material","max_stack":64})"));
    ASSERT_TRUE(wheat.ok()) << wheat.error().message;
    EXPECT_EQ(items.at(*wheat).restoreHunger, 0); // not food
    EXPECT_EQ(items.at(*wheat).maxStack, 64);
}

TEST(ItemRegistryTest, FoodDefaultsToNotFood) {
    // No restore_hunger: not food (every pre-Phase-23 item).
    ItemRegistry items;
    auto r = items.registerItem(
        parseJson(R"({"id":"gem","type":"material"})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(items.at(*r).restoreHunger, 0);
}

TEST(ItemRegistryTest, RejectsBadRestoreHunger) {
    ItemRegistry items;
    EXPECT_FALSE(items.registerItem(
                     parseJson(
                         R"({"id":"b","type":"material","restore_hunger":0})"))
                     .ok()); // explicit 0 = a typo (ABSENT is the default)
    EXPECT_FALSE(items.registerItem(
                     parseJson(
                         R"({"id":"b","type":"material","restore_hunger":-1})"))
                     .ok());
    EXPECT_FALSE(items.registerItem(
                     parseJson(
                         R"({"id":"b","type":"material","restore_hunger":21})"))
                     .ok()); // above the player's max hunger
    EXPECT_FALSE(items.registerItem(
                     parseJson(
                         R"({"id":"b","type":"material","restore_hunger":2.5})"))
                     .ok()); // must be an integer
    EXPECT_FALSE(items.registerItem(
                     parseJson(
                         R"({"id":"b","type":"material","restore_hunger":"a lot"})"))
                     .ok());
}

// -- furnace (Phase 40: smelting + fuel) ------------------------------------

TEST(ItemRegistryTest, ParsesFurnaceFields) {
    ItemRegistry items;
    auto r = items.registerItem(parseJson(
        R"({"id":"beef","type":"material","smeltable_to":"cooked_beef",
            "smelt_time":10.5,"fuel_time":7.5})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const Item& it = items.at(*r);
    EXPECT_EQ(it.smeltableTo, "cooked_beef");
    EXPECT_FLOAT_EQ(it.smeltTime, 10.5f);
    EXPECT_FLOAT_EQ(it.fuelTime, 7.5f);
}

TEST(ItemRegistryTest, FurnaceFieldsDefaultToAbsent) {
    // No furnace fields: not smeltable, the smelt time is the Minecraft
    // 10 s default (only meaningful when smeltable), not fuel.
    ItemRegistry items;
    auto r = items.registerItem(parseJson(R"({"id":"gem","type":"material"})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_TRUE(items.at(*r).smeltableTo.empty());
    EXPECT_FLOAT_EQ(items.at(*r).smeltTime, 10.0f);
    EXPECT_FLOAT_EQ(items.at(*r).fuelTime, 0.0f);
}

TEST(ItemRegistryTest, RejectsBadFurnaceFields) {
    ItemRegistry items;
    // An explicit empty smeltable_to is a typo (ABSENT is the default,
    // the ammo discipline).
    EXPECT_FALSE(items.registerItem(
                     parseJson(
                         R"({"id":"b","type":"material","smeltable_to":""})"))
                     .ok());
    EXPECT_FALSE(items.registerItem(
                     parseJson(
                         R"({"id":"b","type":"material","smeltable_to":5})"))
                     .ok());
    // smelt_time must be a number in (0, 100].
    EXPECT_FALSE(items.registerItem(
                     parseJson(R"({"id":"b","type":"material","smelt_time":0})"))
                     .ok()); // explicit 0 = a typo (ABSENT is the default)
    EXPECT_FALSE(items.registerItem(
                     parseJson(R"({"id":"b","type":"material","smelt_time":-3})"))
                     .ok());
    EXPECT_FALSE(items.registerItem(
                     parseJson(R"({"id":"b","type":"material","smelt_time":100.5})"))
                     .ok());
    EXPECT_FALSE(items.registerItem(
                     parseJson(
                         R"({"id":"b","type":"material","smelt_time":"fast"})"))
                     .ok());
    // fuel_time must be a number in (0, 1000] (explicit 0 = a typo).
    EXPECT_FALSE(items.registerItem(
                     parseJson(R"({"id":"b","type":"material","fuel_time":0})"))
                     .ok());
    EXPECT_FALSE(items.registerItem(
                     parseJson(R"({"id":"b","type":"material","fuel_time":-5})"))
                     .ok());
    EXPECT_FALSE(items.registerItem(
                     parseJson(R"({"id":"b","type":"material","fuel_time":1001})"))
                     .ok());
}

// -- dye (Phase 41) ----------------------------------------------------------

TEST(ItemRegistryTest, ParsesDyeColor) {
    ItemRegistry items;
    auto r = items.registerItem(parseJson(
        R"({"id":"red_dye","type":"material","dye_color":"red"})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(items.at(*r).dyeColor, "red");
}

TEST(ItemRegistryTest, DyeColorAbsentByDefault) {
    // No dye field: not a dye (the absent field is the default, so an
    // explicit empty string is a typo — the ammo discipline).
    ItemRegistry items;
    auto r = items.registerItem(parseJson(R"({"id":"gem","type":"material"})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_TRUE(items.at(*r).dyeColor.empty());
}

TEST(ItemRegistryTest, RejectsBadDyeColor) {
    ItemRegistry items;
    EXPECT_FALSE(items.registerItem(
                     parseJson(
                         R"({"id":"b","type":"material","dye_color":""})"))
                     .ok());
    EXPECT_FALSE(items.registerItem(
                     parseJson(
                         R"({"id":"b","type":"material","dye_color":5})"))
                     .ok());
    EXPECT_FALSE(items.registerItem(
                     parseJson(
                         R"({"id":"b","type":"material","dye_color":["red"]})"))
                     .ok());
}

// -- drinkable container (Phase 43: milk_bucket) -----------------------------

TEST(ItemRegistryTest, ParsesDrinkableFields) {
    // The milk bucket: drink_time marks the item drinkable (the consume
    // channel's DATA-DRIVEN duration); returns_item names the empty
    // container a completed drink returns.
    ItemRegistry items;
    auto r = items.registerItem(
        parseJson(R"({"id":"milk_bucket","type":"material","max_stack":1,
                      "drink_time":1.6,"returns_item":"bucket"})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const Item& it = items.at(*r);
    EXPECT_FLOAT_EQ(it.drinkTime, 1.6f);
    EXPECT_EQ(it.returnsItem, "bucket");
}

TEST(ItemRegistryTest, DrinkableDefaultsToAbsent) {
    // No drink fields: not drinkable (every pre-Phase-43 item).
    ItemRegistry items;
    auto r = items.registerItem(parseJson(R"({"id":"gem","type":"material"})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_FLOAT_EQ(items.at(*r).drinkTime, 0.0f);
    EXPECT_TRUE(items.at(*r).returnsItem.empty());
}

TEST(ItemRegistryTest, DrinkTimeWithoutReturnsIsLegal) {
    // A drinkable without a container (a future water-bucket-less drink):
    // returns_item is optional on a drinkable.
    ItemRegistry items;
    auto r = items.registerItem(
        parseJson(R"({"id":"juice","type":"material","drink_time":0.5})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_FLOAT_EQ(items.at(*r).drinkTime, 0.5f);
    EXPECT_TRUE(items.at(*r).returnsItem.empty());
}

TEST(ItemRegistryTest, RejectsBadDrinkTime) {
    ItemRegistry items;
    EXPECT_FALSE(items.registerItem(
                     parseJson(
                         R"({"id":"b","type":"material","drink_time":0})"))
                     .ok()); // explicit 0 = a typo (ABSENT is the default)
    EXPECT_FALSE(items.registerItem(
                     parseJson(
                         R"({"id":"b","type":"material","drink_time":-1})"))
                     .ok());
    EXPECT_FALSE(items.registerItem(
                     parseJson(
                         R"({"id":"b","type":"material","drink_time":11})"))
                     .ok());
    EXPECT_FALSE(items.registerItem(
                     parseJson(
                         R"({"id":"b","type":"material","drink_time":"slow"})"))
                     .ok());
}

TEST(ItemRegistryTest, RejectsBadReturnsItem) {
    ItemRegistry items;
    // Explicit empty / non-string = a typo (the ammo discipline).
    EXPECT_FALSE(items.registerItem(
                     parseJson(
                         R"({"id":"b","type":"material","returns_item":""})"))
                     .ok());
    EXPECT_FALSE(items.registerItem(
                     parseJson(
                         R"({"id":"b","type":"material","returns_item":5})"))
                     .ok());
    // returns_item WITHOUT drink_time = a dead field (the pairing
    // discipline).
    EXPECT_FALSE(items.registerItem(
                     parseJson(
                         R"({"id":"b","type":"material","returns_item":"x"})"))
                     .ok());
}

TEST(ItemRegistryTest, FoodAndDrinkableAreMutuallyExclusive) {
    ItemRegistry items;
    EXPECT_FALSE(items.registerItem(
                     parseJson(
                         R"({"id":"b","type":"material","restore_hunger":3,
                             "drink_time":1.6})"))
                     .ok());
}

// -- Fluid buckets (Phase 47) -----------------------------------------------

TEST(ItemRegistryTest, ParsesFluidBucketPair) {
    // The water bucket pair: the EMPTY side collects, the FULL side pours.
    // The companion NAMES are cross-checked (existence + symmetry) at game
    // init, not here (the ammo pattern).
    ItemRegistry items;
    auto r = items.registerItem(
        parseJson(R"({"id":"bucket","type":"material","max_stack":16,
                      "icon":"bucket",
                      "collects_fluid":"water",
                      "collects_becomes":"water_bucket"})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const Item& b = items.at(*r);
    EXPECT_EQ(b.collectsFluid, "water");
    EXPECT_EQ(b.collectsBecomes, "water_bucket");
    EXPECT_TRUE(b.poursFluid.empty());
    EXPECT_TRUE(b.poursBecomes.empty());
    r = items.registerItem(
        parseJson(R"({"id":"water_bucket","type":"material","max_stack":1,
                      "icon":"water_bucket",
                      "pours_fluid":"water",
                      "pours_becomes":"bucket"})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const Item& w = items.at(*r);
    EXPECT_EQ(w.poursFluid, "water");
    EXPECT_EQ(w.poursBecomes, "bucket");
    EXPECT_TRUE(w.collectsFluid.empty());
}

TEST(ItemRegistryTest, FluidBucketFieldsDefaultToEmpty) {
    // No bucket fields: every pre-Phase-47 item is untouched.
    ItemRegistry items;
    auto r = items.registerItem(
        parseJson(R"({"id":"gem","type":"material"})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const Item& it = items.at(*r);
    EXPECT_TRUE(it.collectsFluid.empty());
    EXPECT_TRUE(it.collectsBecomes.empty());
    EXPECT_TRUE(it.poursFluid.empty());
    EXPECT_TRUE(it.poursBecomes.empty());
}

TEST(ItemRegistryTest, RejectsUnknownFluidName) {
    // v1 vocabulary: the only simulated fluid is "water" (a different
    // name is a typo or a future feature).
    ItemRegistry items;
    EXPECT_FALSE(items.registerItem(
                     parseJson(
                         R"({"id":"b","type":"material",
                             "collects_fluid":"lava",
                             "collects_becomes":"x"})"))
                     .ok());
    EXPECT_FALSE(items.registerItem(
                     parseJson(
                         R"({"id":"b","type":"material",
                             "pours_fluid":"","pours_becomes":"x"})"))
                     .ok());
}

TEST(ItemRegistryTest, RejectsBecomesWithoutFluid) {
    // A `*_becomes` without its `*_fluid` pair is a dead field (the
    // dye_base/dye_color pairing discipline).
    ItemRegistry items;
    EXPECT_FALSE(items.registerItem(
                     parseJson(
                         R"({"id":"b","type":"material",
                             "collects_becomes":"x"})"))
                     .ok());
    EXPECT_FALSE(items.registerItem(
                     parseJson(
                         R"({"id":"b","type":"material",
                             "pours_becomes":5})"))
                     .ok());
}

TEST(ItemRegistryTest, BucketSidesAreMutuallyExclusive) {
    // One item is ONE side of the pair: a collector does not also pour.
    ItemRegistry items;
    EXPECT_FALSE(items.registerItem(
                     parseJson(
                         R"({"id":"b","type":"material",
                             "collects_fluid":"water",
                             "collects_becomes":"a",
                             "pours_fluid":"water",
                             "pours_becomes":"c"})"))
                     .ok());
}

TEST(ItemRegistryTest, BucketConflictsWithFoodAndDrinkable) {
    // A bucket side owns the right-click: it cannot also be food or a
    // drinkable (the eat/drink channels would eat the click first — a
    // dead field, the food/drink discipline).
    ItemRegistry items;
    EXPECT_FALSE(items.registerItem(
                     parseJson(
                         R"({"id":"b","type":"material",
                             "collects_fluid":"water",
                             "collects_becomes":"a",
                             "restore_hunger":3})"))
                     .ok());
    EXPECT_FALSE(items.registerItem(
                     parseJson(
                         R"({"id":"b","type":"material",
                             "pours_fluid":"water",
                             "pours_becomes":"a",
                             "drink_time":1.6})"))
                     .ok());
}
