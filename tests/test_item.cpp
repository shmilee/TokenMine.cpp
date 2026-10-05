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

// -- projectile launching (Phase 21: the bow) --------------------------------

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
