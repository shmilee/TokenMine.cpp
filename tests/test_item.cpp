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
#include "core/asset_manager.hpp"
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

// -- infinite (Phase 77, the bow's Infinity) --------------------------------

TEST(ItemRegistryTest, ParsesInfinite) {
    ItemRegistry items;
    auto r = items.registerItem(
        parseJson(R"({"id":"ibow","type":"material","max_stack":1,
                      "shoot_speed":15.0,"shoot_damage":5.0,
                      "ammo":"arrow","infinite":true})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_TRUE(items.at(*r).infinite);
}

TEST(ItemRegistryTest, InfiniteDefaultsToFalse) {
    ItemRegistry items;
    auto r = items.registerItem(
        parseJson(R"({"id":"bow","type":"material","max_stack":1,
                      "shoot_speed":15.0,"shoot_damage":5.0,
                      "ammo":"arrow"})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_FALSE(items.at(*r).infinite);
}

TEST(ItemRegistryTest, RejectsBadInfinite) {
    ItemRegistry items;
    // Non-bool.
    EXPECT_FALSE(items.registerItem(
                     parseJson(R"({"id":"b","type":"material",
                                  "shoot_speed":10.0,"ammo":"a",
                                  "infinite":"yes"})"))
                     .ok());
    // A launcher WITHOUT ammo already fires unlimited (the pre-Phase-24
    // default): the flag is a no-op typo.
    auto r = items.registerItem(
        parseJson(R"({"id":"b","type":"material","shoot_speed":10.0,
                      "infinite":true})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("infinite"), std::string::npos)
        << r.error().message;
}

TEST(ItemRegistryTest, InfiniteBowContentPinned) {
    // The real asset (Phase 77): the Infinity bow. The values are the
    // contract (the bow's data unchanged + 'infinite' true; max_stack 1;
    // the ammo names the item the player must hold at least one of).
    ItemRegistry items;
    FILE* fp = std::fopen("assets/data/items/infinite_bow.json", "rb");
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
    auto reg = items.registerItem(*parsed);
    ASSERT_TRUE(reg.ok()) << reg.error().message;
    const Item& b = items.at(*reg);
    EXPECT_EQ(b.name, "infinite_bow");
    EXPECT_EQ(b.maxStack, 1);
    EXPECT_FLOAT_EQ(b.shootSpeed, 15.0f);
    EXPECT_FLOAT_EQ(b.shootDamage, 5.0f);
    EXPECT_EQ(b.attackDamage, 1.0f);
    EXPECT_EQ(b.ammo, "arrow");
    EXPECT_TRUE(b.infinite);
    EXPECT_EQ(b.icon, "infinite_bow");
}

// -- Phase 79: the Power / Speed bows (the bow enhancement line) --------

TEST(ItemRegistryTest, PowerBowContentPinned) {
    // The real asset: the Power bow (Minecraft's Power enchantment, the
    // data-driven twin — the same launcher data with a HIGHER
    // shoot_damage; no new field: shoot_speed/shoot_damage already are
    // the data-driven parameters).
    ItemRegistry items;
    FILE* fp = std::fopen("assets/data/items/power_bow.json", "rb");
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
    auto reg = items.registerItem(*parsed);
    ASSERT_TRUE(reg.ok()) << reg.error().message;
    const Item& b = items.at(*reg);
    EXPECT_EQ(b.name, "power_bow");
    EXPECT_EQ(b.maxStack, 1);
    EXPECT_FLOAT_EQ(b.shootSpeed, 15.0f); // the bow's speed unchanged
    EXPECT_FLOAT_EQ(b.shootDamage, 8.0f); // +3 over the bow's 5 (Power III)
    EXPECT_EQ(b.attackDamage, 1.0f);
    EXPECT_EQ(b.ammo, "arrow");
    EXPECT_FALSE(b.infinite);
    EXPECT_EQ(b.icon, "power_bow");
}

TEST(ItemRegistryTest, SpeedBowContentPinned) {
    // The real asset: the Speed bow (the arrow-velocity twin — the same
    // launcher data with a HIGHER shoot_speed).
    ItemRegistry items;
    FILE* fp = std::fopen("assets/data/items/speed_bow.json", "rb");
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
    auto reg = items.registerItem(*parsed);
    ASSERT_TRUE(reg.ok()) << reg.error().message;
    const Item& b = items.at(*reg);
    EXPECT_EQ(b.name, "speed_bow");
    EXPECT_EQ(b.maxStack, 1);
    EXPECT_FLOAT_EQ(b.shootSpeed, 20.0f); // +5 over the bow's 15
    EXPECT_FLOAT_EQ(b.shootDamage, 5.0f); // the bow's damage unchanged
    EXPECT_EQ(b.attackDamage, 1.0f);
    EXPECT_EQ(b.ammo, "arrow");
    EXPECT_FALSE(b.infinite);
    EXPECT_EQ(b.icon, "speed_bow");
}

TEST(ItemRegistryTest, BowEnhancementRecipesPinned) {
    // The shipped recipes: power = bow + 2 diamond (the top material,
    // like the Infinity bow), speed = bow + gold ingot (shapeless, the
    // flint_and_steel pattern).
    const struct {
        const char* path;
        const char* ingredient0;
        int count0;
        const char* ingredient1;
        int count1;
    } cases[] = {
        {"assets/data/recipes/power_bow.json", "bow", 1, "diamond", 2},
        {"assets/data/recipes/speed_bow.json", "bow", 1, "gold_ingot", 1},
    };
    for (const auto& c : cases) {
        SCOPED_TRACE(c.path);
        FILE* fp = std::fopen(c.path, "rb");
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
        EXPECT_EQ(def.find("output")->find("item")->asString(),
                  std::string(c.path).substr(std::string(c.path).find_last_of('/') + 1, std::string(c.path).find_last_of('.') - std::string(c.path).find_last_of('/') - 1));
        EXPECT_EQ(def.find("output")->find("count")->asInt(), 1);
        const Json& ing = *def.find("ingredients");
        ASSERT_EQ(ing.size(), 2u);
        EXPECT_EQ(ing.at(0).find("item")->asString(), c.ingredient0);
        EXPECT_EQ(ing.at(0).find("count")->asInt(), c.count0);
        EXPECT_EQ(ing.at(1).find("item")->asString(), c.ingredient1);
        EXPECT_EQ(ing.at(1).find("count")->asInt(), c.count1);
    }
}

// -- Phase 80: sandstone (the P61 material-chain pattern) -------------

TEST(BlockContentTest, SandstoneBlockContentPinned) {
    // The real asset: sandstone (compressed sand). The values are the
    // contract (hardness 1.5 — between sand 0.5 and stone 2.0; the
    // classic cut; it drops ITSELF, the P61 "stone needs a recipe to
    // come back" pattern: sand -> sandstone one way, no smelting back).
    BlockRegistry blocks;
    FILE* fp = std::fopen("assets/data/blocks/sandstone.json", "rb");
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
    auto reg = blocks.registerBlock(*parsed);
    ASSERT_TRUE(reg.ok()) << reg.error().message;
    const Block& b = blocks.at(*reg);
    EXPECT_EQ(b.name, "sandstone");
    EXPECT_DOUBLE_EQ(b.properties.hardness, 1.5);
    EXPECT_TRUE(b.properties.solid);
    EXPECT_TRUE(b.properties.opaque);
    EXPECT_FALSE(b.properties.transparent);
    EXPECT_EQ(b.properties.lightEmission, 0);
    EXPECT_FALSE(b.properties.flamable);
    EXPECT_EQ(b.properties.drops, "sandstone"); // drops itself
    EXPECT_EQ(b.model.shape, BlockModel::Shape::FullCube);
}

TEST(BlockContentTest, SandstoneRecipeContentPinned) {
    // The shipped recipe: shapeless 3 sand -> 1 sandstone (the P61
    // stone pattern — the material chain's first link).
    FILE* fp = std::fopen("assets/data/recipes/sandstone.json", "rb");
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
    EXPECT_EQ(def.find("output")->find("item")->asString(), "sandstone");
    EXPECT_EQ(def.find("output")->find("count")->asInt(), 1);
    const Json& ing = *def.find("ingredients");
    ASSERT_EQ(ing.size(), 1u);
    EXPECT_EQ(ing.at(0).find("item")->asString(), "sand");
    EXPECT_EQ(ing.at(0).find("count")->asInt(), 3);
}

// -- Phase 81: smooth_sandstone (the P80 chain's polished step) --------

TEST(BlockContentTest, SmoothSandstoneBlockContentPinned) {
    // The real asset: smooth sandstone (the P80 chain extends one more
    // step, 2 sandstone -> 1 smooth_sandstone, Minecraft 1.14+). Same
    // contract as sandstone: hardness 1.5, drops ITSELF (the one-way
    // chain sand -> sandstone -> smooth_sandstone), opaque cube, no
    // light, not flamable.
    BlockRegistry blocks;
    FILE* fp = std::fopen("assets/data/blocks/smooth_sandstone.json", "rb");
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
    auto reg = blocks.registerBlock(*parsed);
    ASSERT_TRUE(reg.ok()) << reg.error().message;
    const Block& b = blocks.at(*reg);
    EXPECT_EQ(b.name, "smooth_sandstone");
    EXPECT_DOUBLE_EQ(b.properties.hardness, 1.5);
    EXPECT_TRUE(b.properties.solid);
    EXPECT_TRUE(b.properties.opaque);
    EXPECT_FALSE(b.properties.transparent);
    EXPECT_EQ(b.properties.lightEmission, 0);
    EXPECT_FALSE(b.properties.flamable);
    EXPECT_EQ(b.properties.drops, "smooth_sandstone"); // drops itself
    EXPECT_EQ(b.model.shape, BlockModel::Shape::FullCube);
}

TEST(BlockContentTest, SmoothSandstoneRecipeContentPinned) {
    // The shipped recipe: shapeless 2 sandstone -> 1 smooth_sandstone
    // (the P80 chain's second link — sandstone is the ingredient, so the
    // chain sand -> sandstone -> smooth_sandstone is one way: no recipe
    // goes BACK, the P61 cobblestone/stone deviation).
    FILE* fp =
        std::fopen("assets/data/recipes/smooth_sandstone.json", "rb");
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
    EXPECT_EQ(def.find("output")->find("item")->asString(),
              "smooth_sandstone");
    EXPECT_EQ(def.find("output")->find("count")->asInt(), 1);
    const Json& ing = *def.find("ingredients");
    ASSERT_EQ(ing.size(), 1u);
    EXPECT_EQ(ing.at(0).find("item")->asString(), "sandstone");
    EXPECT_EQ(ing.at(0).find("count")->asInt(), 2);
}

// -- Phase 82: the Master bow (the three enhancements combined) -------

TEST(ItemRegistryTest, MasterBowContentPinned) {
    // The real asset: the Master bow (power + speed + infinite, the
    // data-driven capstone). It carries ALL THREE of the separate bows'
    // enhancements: shoot_damage 8 (power), shoot_speed 20 (speed), and
    // infinite (it still needs >=1 arrow to fire, and consumes none —
    // the P77 rule, now on the combined bow).
    ItemRegistry items;
    FILE* fp = std::fopen("assets/data/items/master_bow.json", "rb");
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
    auto reg = items.registerItem(*parsed);
    ASSERT_TRUE(reg.ok()) << reg.error().message;
    const Item& b = items.at(*reg);
    EXPECT_EQ(b.name, "master_bow");
    EXPECT_EQ(b.maxStack, 1);
    EXPECT_FLOAT_EQ(b.shootSpeed, 20.0f); // the speed bow's value
    EXPECT_FLOAT_EQ(b.shootDamage, 8.0f); // the power bow's value
    EXPECT_EQ(b.attackDamage, 1.0f);
    EXPECT_EQ(b.ammo, "arrow");
    EXPECT_TRUE(b.infinite); // the infinite bow's flag
    EXPECT_EQ(b.icon, "master_bow");
}

TEST(ItemRegistryTest, MasterBowRecipePinned) {
    // The shipped recipe: shapeless power_bow + speed_bow + infinite_bow
    // -> 1 master_bow (the three separate enhanced bows combine into the
    // capstone — giving them a purpose, the P61 material-chain pattern).
    FILE* fp = std::fopen("assets/data/recipes/master_bow.json", "rb");
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
    EXPECT_EQ(def.find("output")->find("item")->asString(), "master_bow");
    EXPECT_EQ(def.find("output")->find("count")->asInt(), 1);
    const Json& ing = *def.find("ingredients");
    ASSERT_EQ(ing.size(), 3u);
    const std::vector<std::string> want = {
        "power_bow", "speed_bow", "infinite_bow"};
    for (std::size_t i = 0; i < want.size(); i++) {
        SCOPED_TRACE(want[i]);
        EXPECT_EQ(ing.at(i).find("item")->asString(), want[i]);
        EXPECT_EQ(ing.at(i).find("count")->asInt(), 1);
    }
}

TEST(ItemRegistryTest, InfiniteBowRecipePinned) {
    // The shipped recipe: shapeless bow + diamond -> 1 (the enchantment's
    // craftable stand-in; diamond = the project's top material).
    FILE* fp = std::fopen("assets/data/recipes/infinite_bow.json", "rb");
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
    EXPECT_EQ(def.find("output")->find("item")->asString(), "infinite_bow");
    EXPECT_EQ(def.find("output")->find("count")->asInt(), 1);
    const Json& ing = *def.find("ingredients");
    ASSERT_EQ(ing.size(), 2u);
    EXPECT_EQ(ing.at(0).find("item")->asString(), "bow");
    EXPECT_EQ(ing.at(0).find("count")->asInt(), 1);
    EXPECT_EQ(ing.at(1).find("item")->asString(), "diamond");
    EXPECT_EQ(ing.at(1).find("count")->asInt(), 1);
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
    // The bucket pair (Phase 51): the EMPTY side names every fluid it
    // scoops (an array — one bucket scoops water AND lava), the FULL side
    // pours its fluid back into the empty one. The full bucket a scoop
    // yields is DERIVED (fullBucketFor); the pair symmetry is
    // cross-checked at game init, not here (the ammo pattern).
    ItemRegistry items;
    auto r = items.registerItem(
        parseJson(R"({"id":"bucket","type":"material","max_stack":16,
                      "icon":"bucket",
                      "collects_fluid":["water","lava"]})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const Item& b = items.at(*r);
    ASSERT_EQ(b.collectsFluids.size(), 2u);
    EXPECT_EQ(b.collectsFluids[0], "water");
    EXPECT_EQ(b.collectsFluids[1], "lava");
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
    EXPECT_TRUE(w.collectsFluids.empty());
    r = items.registerItem(
        parseJson(R"({"id":"lava_bucket","type":"material","max_stack":1,
                      "icon":"lava_bucket",
                      "pours_fluid":"lava",
                      "pours_becomes":"bucket"})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    // The derived companion: scoop water -> water_bucket, scoop lava ->
    // lava_bucket.
    const Item* fullWater = items.fullBucketFor("water", "bucket");
    const Item* fullLava = items.fullBucketFor("lava", "bucket");
    ASSERT_NE(fullWater, nullptr);
    ASSERT_NE(fullLava, nullptr);
    EXPECT_EQ(fullWater->name, "water_bucket");
    EXPECT_EQ(fullLava->name, "lava_bucket");
    EXPECT_EQ(items.fullBucketFor("water", "other"), nullptr);
}

TEST(ItemRegistryTest, FluidBucketFieldsDefaultToEmpty) {
    // No bucket fields: every pre-Phase-47 item is untouched.
    ItemRegistry items;
    auto r = items.registerItem(
        parseJson(R"({"id":"gem","type":"material"})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const Item& it = items.at(*r);
    EXPECT_TRUE(it.collectsFluids.empty());
    EXPECT_TRUE(it.poursFluid.empty());
    EXPECT_TRUE(it.poursBecomes.empty());
}

TEST(ItemRegistryTest, RejectsUnknownFluidName) {
    // Two-kind vocabulary: "water" and "lava" (any other name is a typo
    // or an unimplemented fluid).
    ItemRegistry items;
    EXPECT_FALSE(items.registerItem(
                     parseJson(
                         R"({"id":"b","type":"material",
                             "collects_fluid":["slime"]})"))
                     .ok());
    EXPECT_FALSE(items.registerItem(
                     parseJson(
                         R"({"id":"b","type":"material",
                             "pours_fluid":"","pours_becomes":"x"})"))
                     .ok());
    // The collect side is an ARRAY: a bare name is a type error.
    EXPECT_FALSE(items.registerItem(
                     parseJson(
                         R"({"id":"b","type":"material",
                             "collects_fluid":"water"})"))
                     .ok());
    // Empty array / duplicate entry are data errors.
    EXPECT_FALSE(items.registerItem(
                     parseJson(
                         R"({"id":"b","type":"material",
                             "collects_fluid":[]})"))
                     .ok());
    EXPECT_FALSE(items.registerItem(
                     parseJson(
                         R"({"id":"b","type":"material",
                             "collects_fluid":["water","water"]})"))
                     .ok());
}

TEST(ItemRegistryTest, RejectsBecomesWithoutFluid) {
    // A `pours_becomes` without its `pours_fluid` pair is a dead field
    // (the dye_base/dye_color pairing discipline).
    ItemRegistry items;
    EXPECT_FALSE(items.registerItem(
                     parseJson(
                         R"({"id":"b","type":"material",
                             "pours_becomes":"x"})"))
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
                             "collects_fluid":["water"],
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
                             "collects_fluid":["water"],
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

// -- Status-effect grant on eat (Phase 49: golden_apple) ---------------------

TEST(ItemRegistryTest, ParsesGrantEffect) {
    // The golden apple: food (+4, the Minecraft value) that grants
    // regeneration for 10 s when a full eat completes. The effect NAME
    // existence is cross-checked against the EffectRegistry at game init
    // (the ammo pattern), not here.
    ItemRegistry items;
    auto r = items.registerItem(
        parseJson(R"({"id":"golden_apple","type":"material","max_stack":64,
                      "icon":"golden_apple","restore_hunger":4,
                      "grants_effect":{"name":"regeneration","duration":10.0}})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const Item& it = items.at(*r);
    EXPECT_EQ(it.grantsEffect, "regeneration");
    EXPECT_DOUBLE_EQ(it.grantsEffectDuration, 10.0);
}

TEST(ItemRegistryTest, GrantEffectAbsentByDefault) {
    // Every pre-Phase-49 item grants nothing on eat.
    ItemRegistry items;
    auto r = items.registerItem(
        parseJson(R"({"id":"bread","type":"material","restore_hunger":5})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const Item& it = items.at(*r);
    EXPECT_TRUE(it.grantsEffect.empty());
    EXPECT_DOUBLE_EQ(it.grantsEffectDuration, 0.0);
}

TEST(ItemRegistryTest, GrantEffectMustBeObject) {
    ItemRegistry items;
    EXPECT_FALSE(items.registerItem(
                     parseJson(R"({"id":"b","type":"material",
                                   "restore_hunger":3,"grants_effect":"x"})"))
                     .ok());
}

TEST(ItemRegistryTest, GrantEffectRejectsUnknownField) {
    ItemRegistry items;
    EXPECT_FALSE(items.registerItem(
                     parseJson(R"({"id":"b","type":"material",
                                   "restore_hunger":3,
                                   "grants_effect":{"name":"x","duration":5,
                                                    "potency":2}})"))
                     .ok());
}

TEST(ItemRegistryTest, GrantEffectRequiresName) {
    ItemRegistry items;
    EXPECT_FALSE(items.registerItem(
                     parseJson(R"({"id":"b","type":"material",
                                   "restore_hunger":3,
                                   "grants_effect":{"duration":5}})"))
                     .ok());
    // An explicit empty name is a typo (the on_hit_effect discipline).
    EXPECT_FALSE(items.registerItem(
                     parseJson(R"({"id":"b","type":"material",
                                   "restore_hunger":3,
                                   "grants_effect":{"name":"","duration":5}})"))
                     .ok());
}

TEST(ItemRegistryTest, GrantEffectRequiresPositiveDuration) {
    ItemRegistry items;
    EXPECT_FALSE(items.registerItem(
                     parseJson(R"({"id":"b","type":"material",
                                   "restore_hunger":3,
                                   "grants_effect":{"name":"x"}})"))
                     .ok());
    EXPECT_FALSE(items.registerItem(
                     parseJson(R"({"id":"b","type":"material",
                                   "restore_hunger":3,
                                   "grants_effect":{"name":"x","duration":0}})"))
                     .ok());
    EXPECT_FALSE(items.registerItem(
                     parseJson(R"({"id":"b","type":"material",
                                   "restore_hunger":3,
                                   "grants_effect":{"name":"x","duration":-3}})"))
                     .ok());
}

TEST(ItemRegistryTest, GrantEffectRequiresFood) {
    // Only FOOD grants an effect on eat: a drinkable CLEARS effects on
    // drink, and a non-food has no eat channel at all (the grant would be
    // a dead field).
    ItemRegistry items;
    EXPECT_FALSE(items.registerItem(
                     parseJson(R"({"id":"b","type":"material",
                                   "grants_effect":{"name":"x","duration":5}})"))
                     .ok());
    // A drinkable with the grant: rejected (the food/drink exclusivity
    // fires first; either way the item cannot be registered).
    ItemRegistry items2;
    EXPECT_FALSE(items2.registerItem(
                     parseJson(R"({"id":"c","type":"material",
                                   "drink_time":1.6,
                                   "grants_effect":{"name":"x","duration":5}})"))
                     .ok());
}

// -- Status-effect grant LIST on eat (Phase 56: grants_effects) --------------

TEST(ItemRegistryTest, ParsesGrantEffectsList) {
    // The Phase 56 golden apple: regeneration + absorption, applied IN
    // ORDER on a full eat.
    ItemRegistry items;
    auto r = items.registerItem(
        parseJson(R"({"id":"golden_apple","type":"material","max_stack":64,
                      "icon":"golden_apple","restore_hunger":4,
                      "grants_effects":[
                        {"name":"regeneration","duration":10.0},
                        {"name":"absorption","duration":5.0}]})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const Item& it = items.at(*r);
    EXPECT_TRUE(it.grantsEffect.empty());
    ASSERT_EQ(it.grantsEffects.size(), 2u);
    EXPECT_EQ(it.grantsEffects[0].name, "regeneration");
    EXPECT_DOUBLE_EQ(it.grantsEffects[0].duration, 10.0);
    EXPECT_EQ(it.grantsEffects[1].name, "absorption");
    EXPECT_DOUBLE_EQ(it.grantsEffects[1].duration, 5.0);
}

TEST(ItemRegistryTest, GrantEffectsMutuallyExclusiveWithSingle) {
    ItemRegistry items;
    auto r = items.registerItem(
        parseJson(R"({"id":"b","type":"material","restore_hunger":3,
                      "grants_effect":{"name":"x","duration":5},
                      "grants_effects":[{"name":"y","duration":5}]})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("mutually exclusive"),
              std::string::npos);
}

TEST(ItemRegistryTest, GrantEffectsRejections) {
    const std::vector<std::string> bad = {
        // not an array / empty array
        R"({"id":"b","type":"material","restore_hunger":3,"grants_effects":[]})",
        R"({"id":"b","type":"material","restore_hunger":3,
            "grants_effects":{"name":"x","duration":5}})",
        // a non-object entry
        R"({"id":"b","type":"material","restore_hunger":3,
            "grants_effects":["x"]})",
        // an entry with an unknown field
        R"({"id":"b","type":"material","restore_hunger":3,
            "grants_effects":[{"name":"x","duration":5,"potency":2}]})",
        // a missing / empty name
        R"({"id":"b","type":"material","restore_hunger":3,
            "grants_effects":[{"duration":5}]})",
        R"({"id":"b","type":"material","restore_hunger":3,
            "grants_effects":[{"name":"","duration":5}]})",
        // a missing / non-positive duration
        R"({"id":"b","type":"material","restore_hunger":3,
            "grants_effects":[{"name":"x"}]})",
        R"({"id":"b","type":"material","restore_hunger":3,
            "grants_effects":[{"name":"x","duration":0}]})",
    };
    for (const std::string& def : bad) {
        ItemRegistry items;
        EXPECT_FALSE(items.registerItem(parseJson(def)).ok()) << def;
    }
    // Only FOOD grants effects on eat (same as the single field).
    ItemRegistry noFood;
    EXPECT_FALSE(noFood.registerItem(
                     parseJson(R"({"id":"b","type":"material",
                                   "grants_effects":[{"name":"x","duration":5}]})"))
                     .ok());
}

TEST(ItemRegistryTest, GrantEffectsEntryIndexedInErrors) {
    // A bad SECOND entry is named grants_effects[1] (the chance_drops[i]
    // pattern).
    ItemRegistry items;
    auto r = items.registerItem(
        parseJson(R"({"id":"b","type":"material","restore_hunger":3,
                      "grants_effects":[{"name":"ok","duration":5},
                                        {"duration":5}]})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("grants_effects[1]"),
              std::string::npos)
        << r.error().message;
}

// ---------------------------------------------------------------------------
// Phase 59 content pin: flint is the gravel chance-drop item — a plain
// material (no use in v1, documented deviation; see KNOWN_ISSUES).
// ---------------------------------------------------------------------------

TEST(ItemRegistryTest, Phase59FlintItemContent) {
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    BlockRegistry blocks;
    ASSERT_TRUE(blocks.loadDirectory(assets, "data/blocks").ok());
    ItemRegistry items;
    ASSERT_TRUE(items.buildBlockItems(blocks).ok());
    ASSERT_TRUE(items.loadDirectory(assets, "data/items").ok());
    const Item* it = items.findByName("flint");
    ASSERT_TRUE(it != nullptr);
    EXPECT_EQ(it->type, ItemType::Material);
    EXPECT_EQ(it->maxStack, 64);
    // The block item side: gravel is placeable and drops itself.
    EXPECT_NE(items.findByName("gravel"), nullptr);
    EXPECT_EQ(items.findByName("gravel")->type, ItemType::Block);
}

// ---------------------------------------------------------------------------
// Ignition (Phase 60): the 'ignites_flamable' field — a strict bool that
// marks an item as an IGNITER (right-click burns `flamable` blocks to air,
// no drops, the item is not consumed). Channel-ownership exclusivity with
// every other right-click channel (food / drinkable / launcher / dye /
// fluid bucket).
// ---------------------------------------------------------------------------

TEST(ItemRegistryTest, IgnitesFlamableDefaultsFalse) {
    ItemRegistry items;
    auto r = items.registerItem(
        parseJson(R"({"id":"x","type":"material"})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_FALSE(items.at(*r).ignitesFlamable);
}

TEST(ItemRegistryTest, IgnitesFlamableAcceptsBool) {
    ItemRegistry items;
    auto r = items.registerItem(
        parseJson(R"({"id":"x","type":"material","ignites_flamable":true})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_TRUE(items.at(*r).ignitesFlamable);
}

TEST(ItemRegistryTest, IgnitesFlamableRejectsNonBool) {
    ItemRegistry items;
    auto r = items.registerItem(
        parseJson(R"({"id":"x","type":"material","ignites_flamable":"yes"})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("ignites_flamable"), std::string::npos);
    EXPECT_NE(r.error().message.find("boolean"), std::string::npos);
}

TEST(ItemRegistryTest, IgnitesFlamableRejectsFood) {
    ItemRegistry items;
    auto r = items.registerItem(parseJson(
        R"({"id":"x","type":"material","restore_hunger":4,"ignites_flamable":true})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("one item owns one right-click channel"),
              std::string::npos);
}

TEST(ItemRegistryTest, IgnitesFlamableRejectsLauncher) {
    ItemRegistry items;
    auto r = items.registerItem(parseJson(
        R"({"id":"x","type":"material","shoot_speed":30,"ignites_flamable":true})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("launcher"), std::string::npos);
}

// Phase 60 content pin: flint_and_steel is the igniter — a plain material
// with the 'ignites_flamable' flag (the flint uses: arrows + flint and
// steel, replacing the Phase 24 stick-only arrow recipe, MC 1.14+).
// ---------------------------------------------------------------------------

TEST(ItemRegistryTest, Phase60FlintAndSteelItemContent) {
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    BlockRegistry blocks;
    ASSERT_TRUE(blocks.loadDirectory(assets, "data/blocks").ok());
    ItemRegistry items;
    ASSERT_TRUE(items.buildBlockItems(blocks).ok());
    ASSERT_TRUE(items.loadDirectory(assets, "data/items").ok());
    const Item* it = items.findByName("flint_and_steel");
    ASSERT_NE(it, nullptr);
    EXPECT_EQ(it->type, ItemType::Material);
    EXPECT_EQ(it->maxStack, 64);
    EXPECT_TRUE(it->ignitesFlamable);
    EXPECT_EQ(it->icon, "flint_and_steel");
}

// Phase 62 content pin: stone_pickaxe is the MIDDLE tier of the pickaxe
// tool line (break speed 2.0 -> 3.0 -> 4.0, attack 2 -> 2.5 -> 3 on the
// project's compressed scale — exactly between the committed wooden and
// iron values). Crafted from 3 cobblestone + 2 sticks (Minecraft), giving
// Phase 61's cobblestone its first tool use.
// ---------------------------------------------------------------------------

TEST(ItemRegistryTest, Phase62StonePickaxeItemContent) {
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    BlockRegistry blocks;
    ASSERT_TRUE(blocks.loadDirectory(assets, "data/blocks").ok());
    ItemRegistry items;
    ASSERT_TRUE(items.buildBlockItems(blocks).ok());
    ASSERT_TRUE(items.loadDirectory(assets, "data/items").ok());
    const Item* it = items.findByName("stone_pickaxe");
    ASSERT_NE(it, nullptr);
    EXPECT_EQ(it->type, ItemType::Tool);
    EXPECT_EQ(it->maxStack, 1);
    EXPECT_FLOAT_EQ(it->breakSpeed, 3.0f);   // wooden 2.0 < stone < iron 4.0
    EXPECT_FLOAT_EQ(it->attackDamage, 2.5f); // wooden 2 < stone < iron 3
    EXPECT_EQ(it->icon, "stone_pickaxe");
}

// ---------------------------------------------------------------------------
// Phase 67: per-block tool efficiency ('fast_against' +
// 'fast_against_multiplier', the first per-block tool category: axes chop
// wood 2x faster — the KNOWN_ISSUES "no axe tools" entry consumed).
// ---------------------------------------------------------------------------

TEST(ItemRegistryTest, FastAgainstAcceptsToolPair) {
    ItemRegistry items;
    auto r = items.registerItem(parseJson(
        R"({"id":"x","type":"tool","fast_against":["wood","leaves"],"fast_against_multiplier":2.0})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const Item* it = items.findByName("x");
    ASSERT_NE(it, nullptr);
    EXPECT_EQ(it->fastAgainst,
              (std::vector<std::string>{"wood", "leaves"}));
    EXPECT_FLOAT_EQ(it->fastAgainstMultiplier, 2.0f);
}

TEST(ItemRegistryTest, FastAgainstDefaultsAbsent) {
    ItemRegistry items;
    auto r = items.registerItem(parseJson(R"({"id":"x","type":"tool"})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const Item& it = items.at(*r);
    EXPECT_TRUE(it.fastAgainst.empty());
    EXPECT_FLOAT_EQ(it.fastAgainstMultiplier, 0.0f);
}

TEST(ItemRegistryTest, FastAgainstRejectsMissingPartner) {
    ItemRegistry items;
    auto r = items.registerItem(
        parseJson(R"({"id":"x","type":"tool","fast_against":["wood"]})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("strict pair"), std::string::npos);
    auto r2 = items.registerItem(
        parseJson(R"({"id":"y","type":"tool","fast_against_multiplier":2.0})"));
    ASSERT_FALSE(r2.ok());
    EXPECT_NE(r2.error().message.find("strict pair"), std::string::npos);
}

TEST(ItemRegistryTest, FastAgainstRejectsEmptyList) {
    ItemRegistry items;
    auto r = items.registerItem(parseJson(
        R"({"id":"x","type":"tool","fast_against":[],"fast_against_multiplier":2.0})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("non-empty array"), std::string::npos);
}

TEST(ItemRegistryTest, FastAgainstRejectsBadEntryAndMultiplier) {
    ItemRegistry items;
    auto r = items.registerItem(parseJson(
        R"({"id":"x","type":"tool","fast_against":["wood",42],"fast_against_multiplier":2.0})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("fast_against[1]"), std::string::npos);
    auto r2 = items.registerItem(parseJson(
        R"({"id":"y","type":"tool","fast_against":["wood"],"fast_against_multiplier":0})"));
    ASSERT_FALSE(r2.ok());
    EXPECT_NE(r2.error().message.find("fast_against_multiplier"),
              std::string::npos);
}

TEST(ItemRegistryTest, FastAgainstRejectsMaterial) {
    ItemRegistry items;
    auto r = items.registerItem(parseJson(
        R"({"id":"x","type":"material","fast_against":["wood"],"fast_against_multiplier":2.0})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("only valid for type"),
              std::string::npos);
}

namespace {
// A data axe for the pure-function tests (the shipped content pins are
// separate — Phase67AxeItemContent).
Item axeItem() {
    Item it;
    it.type = ItemType::Tool;
    it.breakSpeed = 1.0f;
    it.fastAgainst = {"wood", "leaves"};
    it.fastAgainstMultiplier = 2.0f;
    return it;
}

Block blockNamed(const char* name) {
    Block b;
    b.name = name;
    return b;
}
} // namespace

TEST(ItemRegistryTest, EffectiveBreakSpeedFastAgainstNamedBlocksOnly) {
    const Item axe = axeItem();
    EXPECT_FLOAT_EQ(effectiveBreakSpeed(axe, blockNamed("wood")), 2.0f);
    EXPECT_FLOAT_EQ(effectiveBreakSpeed(axe, blockNamed("leaves")), 2.0f);
    // Off-list: the multiplier does not apply (the axe = hand speed).
    EXPECT_FLOAT_EQ(effectiveBreakSpeed(axe, blockNamed("stone")), 1.0f);
}

TEST(ItemRegistryTest, EffectiveBreakSpeedFlatToolStaysFlat) {
    Item pick;
    pick.type = ItemType::Tool;
    pick.breakSpeed = 4.0f;
    EXPECT_FLOAT_EQ(effectiveBreakSpeed(pick, blockNamed("wood")), 4.0f);
    EXPECT_FLOAT_EQ(effectiveBreakSpeed(pick, blockNamed("stone")), 4.0f);
}

TEST(ItemRegistryTest, EffectiveBreakSpeedNonToolsAreOne) {
    Item hand; // the default (block-item type): never a tool
    Item mat;
    mat.type = ItemType::Material;
    EXPECT_FLOAT_EQ(effectiveBreakSpeed(hand, blockNamed("wood")), 1.0f);
    EXPECT_FLOAT_EQ(effectiveBreakSpeed(mat, blockNamed("wood")), 1.0f);
}

// Pins the ACTUAL axe content (Minecraft values: 2x on wood/leaves, 1x
// elsewhere = the bare-hand pace, attack one point above the matching
// pickaxe — the project's compressed tier scale).
TEST(ItemRegistryTest, Phase67AxeItemContent) {
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    BlockRegistry blocks;
    ASSERT_TRUE(blocks.loadDirectory(assets, "data/blocks").ok());
    ItemRegistry items;
    ASSERT_TRUE(items.buildBlockItems(blocks).ok());
    ASSERT_TRUE(items.loadDirectory(assets, "data/items").ok());
    const std::vector<std::pair<std::string, float>> want = {
        {"wooden_axe", 3.0f}, {"stone_axe", 3.5f}, {"iron_axe", 4.0f}};
    for (const auto& [name, atk] : want) {
        const Item* it = items.findByName(name);
        ASSERT_NE(it, nullptr) << name;
        EXPECT_EQ(it->type, ItemType::Tool) << name;
        EXPECT_EQ(it->maxStack, 1) << name;
        EXPECT_FLOAT_EQ(it->breakSpeed, 1.0f) << name; // MC: the axe = 1x vs stone
        EXPECT_EQ(it->fastAgainst,
                  (std::vector<std::string>{"wood", "leaves"})) << name;
        EXPECT_FLOAT_EQ(it->fastAgainstMultiplier, 2.0f) << name; // MC: 2x vs wood
        EXPECT_FLOAT_EQ(it->attackDamage, atk) << name;
        EXPECT_EQ(it->icon, name) << name;
    }
}

// Phase 68: the shovel content — the second fast_against category (the
// dirt family: dirt/grass/sand/gravel x2 = the Minecraft shovel's
// effective set, the project's subset — no clay/sandstone/snow here).
// Same shape as the axes: break_speed 1.0 (the bare-hand pace — the
// Minecraft shovel is 1x vs stone), attack 2.5/3/3.5 = the Minecraft
// values exactly (the Minecraft delta of +0.5 over the matching
// pickaxe tier, kept by the project's compressed pickaxe scale).
TEST(ItemRegistryTest, Phase68ShovelItemContent) {
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    BlockRegistry blocks;
    ASSERT_TRUE(blocks.loadDirectory(assets, "data/blocks").ok());
    ItemRegistry items;
    ASSERT_TRUE(items.buildBlockItems(blocks).ok());
    ASSERT_TRUE(items.loadDirectory(assets, "data/items").ok());
    const std::vector<std::pair<std::string, float>> want = {
        {"wooden_shovel", 2.5f}, {"stone_shovel", 3.0f},
        {"iron_shovel", 3.5f}};
    for (const auto& [name, atk] : want) {
        const Item* it = items.findByName(name);
        ASSERT_NE(it, nullptr) << name;
        EXPECT_EQ(it->type, ItemType::Tool) << name;
        EXPECT_EQ(it->maxStack, 1) << name;
        EXPECT_FLOAT_EQ(it->breakSpeed, 1.0f) << name; // MC: the shovel = 1x vs stone
        EXPECT_EQ(
            it->fastAgainst,
            (std::vector<std::string>{
                "dirt", "grass", "sand", "gravel"})) << name;
        EXPECT_FLOAT_EQ(it->fastAgainstMultiplier, 2.0f) << name; // MC: 2x vs dirt
        EXPECT_FLOAT_EQ(it->attackDamage, atk) << name;
        EXPECT_EQ(it->icon, name) << name;
    }
}

// Phase 69: the pickaxes gain the stone/ore category — the project's
// FULL set (MC: the pickaxe is 2x on stone, the ores and glowstone).
// obsidian is deliberately EXCLUDED (Minecraft: an iron pickaxe cannot
// break obsidian at all; this project has no diamond pickaxe, so the
// exclusion keeps obsidian at the plain pickaxe pace — 3.75 s for
// iron — the closest analog to "needs a better pickaxe").
TEST(ItemRegistryTest, Phase69PickaxeFastAgainstContent) {
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    BlockRegistry blocks;
    ASSERT_TRUE(blocks.loadDirectory(assets, "data/blocks").ok());
    ItemRegistry items;
    ASSERT_TRUE(items.buildBlockItems(blocks).ok());
    ASSERT_TRUE(items.loadDirectory(assets, "data/items").ok());
    const std::vector<std::pair<std::string, float>> want = {
        {"wooden_pickaxe", 2.0f}, {"stone_pickaxe", 3.0f},
        {"iron_pickaxe", 4.0f}};
    // Phase 80: sandstone joins the family (MC: the pickaxe works on
    // sandstone — unlike obsidian, no tool-tier gap exists for it).
    // Phase 81: smooth_sandstone joins it too (the polished step, same
    // stone family).
    const std::vector<std::string> list = {
        "stone", "cobblestone", "coal_ore", "iron_ore", "gold_ore",
        "diamond_ore", "lapis_ore", "glowstone", "sandstone",
        "smooth_sandstone"};
    for (const auto& [name, speed] : want) {
        const Item* it = items.findByName(name);
        ASSERT_NE(it, nullptr) << name;
        EXPECT_FLOAT_EQ(it->breakSpeed, speed) << name; // the flat tiers are unchanged
        EXPECT_EQ(it->fastAgainst, list) << name;
        EXPECT_FLOAT_EQ(it->fastAgainstMultiplier, 2.0f) << name; // MC: 2x on stone/ores
    }
}

// Phase 69: the effectiveBreakSpeed pure function on the REAL pickaxe
// content — 2x on the stone/ore family, the flat tier on everything
// else (wood stays at the pickaxe's own speed; obsidian = the
// documented exclusion).
TEST(ItemRegistryTest, Phase69PickaxeEffectiveBreakSpeed) {
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    BlockRegistry blocks;
    ASSERT_TRUE(blocks.loadDirectory(assets, "data/blocks").ok());
    ItemRegistry items;
    ASSERT_TRUE(items.buildBlockItems(blocks).ok());
    ASSERT_TRUE(items.loadDirectory(assets, "data/items").ok());
    const Item* iron = items.findByName("iron_pickaxe");
    const Item* wooden = items.findByName("wooden_pickaxe");
    ASSERT_NE(iron, nullptr);
    ASSERT_NE(wooden, nullptr);
    const auto blk = [&](const char* n) {
        return blocks.at(blocks.idForName(n));
    };
    EXPECT_FLOAT_EQ(effectiveBreakSpeed(*iron, blk("stone")), 8.0f);
    EXPECT_FLOAT_EQ(effectiveBreakSpeed(*iron, blk("cobblestone")), 8.0f);
    EXPECT_FLOAT_EQ(effectiveBreakSpeed(*iron, blk("diamond_ore")), 8.0f);
    EXPECT_FLOAT_EQ(effectiveBreakSpeed(*iron, blk("glowstone")), 8.0f);
    EXPECT_FLOAT_EQ(effectiveBreakSpeed(*iron, blk("sandstone")), 8.0f);
    EXPECT_FLOAT_EQ(effectiveBreakSpeed(*iron, blk("smooth_sandstone")),
                    8.0f);
    // Off-list: the flat tier (no multiplier leak).
    EXPECT_FLOAT_EQ(effectiveBreakSpeed(*iron, blk("wood")), 4.0f);
    EXPECT_FLOAT_EQ(effectiveBreakSpeed(*iron, blk("obsidian")), 4.0f);
    EXPECT_FLOAT_EQ(effectiveBreakSpeed(*wooden, blk("stone")), 4.0f);
}

// ---------------------------------------------------------------------------
// GEN-2: the starter-storage fill (hotbar.json's `inventory` section — the
// strict {slot, item, count} parse, the hidden-sapling-starter pattern).
// ---------------------------------------------------------------------------

TEST(ItemRegistryTest, StarterSlotsAbsentSectionYieldsNoFill) {
    InvFixture f;
    const Json def = parseJson(
        R"({"items":["stone","dirt"]})");
    auto r = parseStarterSlots(def, f.items);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_TRUE(r->empty());
}

TEST(ItemRegistryTest, StarterSlotsStrictParse) {
    InvFixture f;
    // A valid fill: two storage entries, resolved + count-checked.
    {
        const Json def = parseJson(
            R"({"items":["stone"],"inventory":[)"
            R"({"slot":9,"item":"dirt","count":1},)"
            R"({"slot":26,"item":"stone","count":64}]})");
        auto r = parseStarterSlots(def, f.items);
        ASSERT_TRUE(r.ok()) << r.error().message;
        ASSERT_EQ(r->size(), 2u);
        EXPECT_EQ((*r)[0].slot, 9);
        EXPECT_EQ((*r)[0].item, f.dirt);
        EXPECT_EQ((*r)[0].count, 1);
        EXPECT_EQ((*r)[1].slot, 26);
        EXPECT_EQ((*r)[1].item, f.stone);
        EXPECT_EQ((*r)[1].count, 64);
    }
    // The rejections (each named): a hotbar slot, an out-of-range slot,
    // an unknown item, a zero count, an over-maxStack count, a duplicate
    // slot, an unknown field, a missing key, a non-integer slot.
    const struct {
        const char* inventory;
        const char* needle;
    } bad[] = {
        {R"([{"slot":8,"item":"dirt","count":1}])", "slot"},
        {R"([{"slot":27,"item":"dirt","count":1}])", "slot"},
        {R"([{"slot":9,"item":"diamondz","count":1}])", "unknown item"},
        {R"([{"slot":9,"item":"dirt","count":0}])", "count"},
        {R"([{"slot":9,"item":"dirt","count":65}])", "count"},
        {R"([{"slot":9,"item":"dirt","count":1},{"slot":9,"item":"stone","count":1}])",
         "duplicates"},
        {R"([{"slot":9,"item":"dirt","count":1,"extra":2}])", "unknown field"},
        {R"([{"slot":9,"item":"dirt"}])", "strict triple"},
        {R"([{"slot":9.5,"item":"dirt","count":1}])", "slot"},
        {R"([{"slot":9,"item":"dirt","count":"1"}])", "count"},
    };
    for (const auto& b : bad) {
        const Json def = parseJson(std::string(
            R"({"items":["stone"],"inventory":)") +
            std::string(b.inventory) + "}");
        auto r = parseStarterSlots(def, f.items);
        ASSERT_FALSE(r.ok()) << b.inventory;
        EXPECT_NE(r.error().message.find(b.needle), std::string::npos)
            << r.error().message;
    }
    // A non-array section.
    {
        const Json def = parseJson(
            R"({"items":["stone"],"inventory":{"slot":9}})");
        auto r = parseStarterSlots(def, f.items);
        ASSERT_FALSE(r.ok());
        EXPECT_NE(r.error().message.find("'inventory' must be an array"),
                  std::string::npos);
    }
}

TEST(ItemRegistryTest, Gen2HotbarStarterPinned) {
    // The SHIPPED hotbar.json: the three sapling starters, three each, in
    // the hidden storage slots 24/25/26 (NOT the hotbar — the structural
    // immunity of the no-input run).
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    BlockRegistry blocks;
    ASSERT_TRUE(blocks.loadDirectory(assets, "data/blocks").ok());
    ItemRegistry items;
    ASSERT_TRUE(items.buildBlockItems(blocks).ok());
    ASSERT_TRUE(items.loadDirectory(assets, "data/items").ok());
    auto text = assets.loadText("data/hotbar.json");
    ASSERT_TRUE(text.ok()) << text.error().message;
    auto parsed = Json::parse(*text);
    ASSERT_TRUE(parsed.ok()) << parsed.error().message;
    auto r = parseStarterSlots(*parsed, items);
    ASSERT_TRUE(r.ok()) << r.error().message;
    ASSERT_EQ(r->size(), 3u);
    const struct {
        int slot;
        const char* item;
    } want[] = {
        {24, "oak_sapling"}, {25, "spruce_sapling"}, {26, "birch_sapling"},
    };
    for (std::size_t i = 0; i < r->size(); ++i) {
        EXPECT_EQ((*r)[i].slot, want[i].slot);
        EXPECT_EQ((*r)[i].item, items.idForName(want[i].item));
        EXPECT_EQ((*r)[i].count, 3);
    }
    // The hotbar fill itself is unchanged (the 9 shipped items, slots
    // 0..8 — no storage slot in the `items` list).
    const Json* list = parsed->find("items");
    ASSERT_TRUE(list != nullptr && list->isArray());
    ASSERT_EQ(list->size(), 9u);
}
