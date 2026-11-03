// V2-12 (v2.13.0): the enchantment CONTENT tests — the item enchant fields
// (strict parse + the class<->ability pairing), the ItemStack operator== /
// Inventory::add enchant-aware merge, the two new blocks (enchanting_table
// + bookshelf), and the two new recipes. The pure-function family (offer
// roll, bookshelfCount, XP charge, unbreakingWear, the effect accessors) is
// test_enchanting.cpp; the save format 27 is test_enchant_save.cpp.
#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "block/block_registry.hpp"
#include "core/asset_manager.hpp"
#include "core/enchanting.hpp"
#include "core/json.hpp"
#include "item/inventory.hpp"
#include "item/item.hpp"
#include "item/item_registry.hpp"
#include "item/recipe.hpp"

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

// A minimal tool item with the V2-12 enchant fields.
std::string toolDef(const std::string& id, const std::string& extra) {
    return std::string("{\"id\":\"") + id + "\",\"type\":\"tool\"," + extra +
           "\"tool\":{\"break_speed\":2.0}}";
}
// A minimal material item.
std::string matDef(const std::string& id, const std::string& extra) {
    return std::string("{\"id\":\"") + id + "\",\"type\":\"material\"," +
           extra + "}";
}

} // namespace

// ===========================================================================
// The item's enchant fields (strict parse + the class<->ability pairing)
// ===========================================================================

TEST(EnchantItemFields, ClassAndAbilityPairAccepted) {
    ItemRegistry reg;
    auto r = reg.registerItem(parseDef(
        toolDef("axe", R"("enchant_class":"axe","enchantability":14,)")));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const Item& it = reg.at(*r);
    EXPECT_EQ(it.enchantClass, EnchantClass::Axe);
    EXPECT_EQ(it.enchantability, 14);
    EXPECT_FALSE(it.enchantMaterial);
}

TEST(EnchantItemFields, ClassWithoutAbilityRejected) {
    ItemRegistry reg;
    auto r = reg.registerItem(
        parseDef(toolDef("axe", R"("enchant_class":"axe",)")));
    EXPECT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("strict pair"), std::string::npos);
}

TEST(EnchantItemFields, AbilityWithoutClassRejected) {
    ItemRegistry reg;
    auto r = reg.registerItem(
        parseDef(toolDef("axe", R"("enchantability":14,)")));
    EXPECT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("strict pair"), std::string::npos);
}

TEST(EnchantItemFields, UnknownClassRejected) {
    ItemRegistry reg;
    auto r = reg.registerItem(parseDef(
        toolDef("axe", R"("enchant_class":"wrench","enchantability":5,)")));
    EXPECT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("unknown class"), std::string::npos);
}

TEST(EnchantItemFields, NegativeAbilityRejected) {
    ItemRegistry reg;
    auto r = reg.registerItem(parseDef(
        toolDef("axe", R"("enchant_class":"axe","enchantability":-1,)")));
    EXPECT_FALSE(r.ok());
}

TEST(EnchantItemFields, EnchantMaterialOnlyOnMaterial) {
    // A material carrying enchant_material is accepted.
    ItemRegistry reg;
    auto ok = reg.registerItem(parseDef(matDef("lapis", R"("enchant_material":true)")));
    ASSERT_TRUE(ok.ok()) << ok.error().message;
    EXPECT_TRUE(reg.at(*ok).enchantMaterial);
    // A tool carrying enchant_material is rejected (the gate is material-only).
    auto bad = reg.registerItem(parseDef(toolDef("axe", R"("enchant_material":true,)")));
    EXPECT_FALSE(bad.ok());
    EXPECT_NE(bad.error().message.find("only valid for type \"material\""),
              std::string::npos);
}

TEST(EnchantItemFields, EnchantMaterialStrictBool) {
    ItemRegistry reg;
    auto bad = reg.registerItem(
        parseDef(matDef("lapis", R"("enchant_material":"yes")")));
    EXPECT_FALSE(bad.ok());
    EXPECT_NE(bad.error().message.find("boolean"), std::string::npos);
}

TEST(EnchantItemFields, NoFieldsIsNotEnchantable) {
    // The pre-V2-12 shape (no enchant fields) loads as not-enchantable.
    ItemRegistry reg;
    auto r = reg.registerItem(parseDef(toolDef("axe", "")));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(reg.at(*r).enchantClass, EnchantClass::None);
    EXPECT_EQ(reg.at(*r).enchantability, 0);
}

// ===========================================================================
// ItemStack operator== + Inventory::add (the enchant-aware merge)
// ===========================================================================

TEST(EnchantItemStack, EqualityIncludesEnchants) {
    // ItemStack equality compares item + count + damage + enchants.
    ItemStack plain;
    plain.item = ItemID{0};
    plain.count = 1;
    ItemStack enchanted;
    enchanted.item = ItemID{0};
    enchanted.count = 1;
    enchanted.enchants["sharpness"] = 3;
    // Same item + count + damage, different enchants -> NOT equal.
    EXPECT_NE(plain, enchanted);
    // A copy is equal.
    ItemStack copy = enchanted;
    EXPECT_EQ(copy, enchanted);
}

TEST(EnchantItemStack, AddDoesNotMergeDifferentEnchants) {
    ItemRegistry reg;
    // A material with max_stack 64 (so two stacks can share a slot).
    auto r = reg.registerItem(parseDef(matDef("lapis", R"("max_stack":64)")));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const ItemID lapis = *r;
    Inventory inv;
    // Place a plain lapis stack in slot 0.
    inv.setSlot(0, (ItemStack {lapis, 10}));
    // Add an ENCHANTED lapis (10 more): it must NOT merge into the plain
    // stack (different enchants) — it takes slot 1.
    const ItemStack ench {lapis, 10};
    ItemStack enchWith = ench;
    enchWith.enchants["sharpness"] = 1;
    // add() with the enchanted map -> a new slot.
    const int slot = inv.add(lapis, 10, reg, 0.0f, enchWith.enchants);
    EXPECT_EQ(slot, 1);
    EXPECT_EQ(inv.at(0).count, 10); // the plain stack is untouched
    EXPECT_TRUE(inv.at(0).enchants.empty());
    EXPECT_EQ(inv.at(1).count, 10);
    EXPECT_EQ(inv.at(1).enchants.at("sharpness"), 1);
    // Adding the SAME enchanted stack merges (same item + damage + enchants).
    const int slot2 = inv.add(lapis, 5, reg, 0.0f, enchWith.enchants);
    EXPECT_EQ(slot2, 1);
    EXPECT_EQ(inv.at(1).count, 15);
}

// ===========================================================================
// The two new blocks (content pins, loaded from the real data dir)
// ===========================================================================

TEST(EnchantBlocks, EnchantingTableContentPinned) {
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok());
    BlockRegistry reg;
    ASSERT_TRUE(reg.loadDirectory(assets, "data/blocks").ok());
    const Block* et = reg.findByName("enchanting_table");
    ASSERT_NE(et, nullptr);
    EXPECT_EQ(et->properties.interact, std::string(kInteractEnchant));
    EXPECT_EQ(et->properties.lightEmission, 7); // MC's enchanting-table light
    EXPECT_EQ(et->properties.hardness, 5.0f);
    EXPECT_EQ(et->properties.drops, "enchanting_table"); // drops self
    EXPECT_FALSE(et->properties.enchantBookshelf);
}

TEST(EnchantBlocks, BookshelfContentPinned) {
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok());
    BlockRegistry reg;
    ASSERT_TRUE(reg.loadDirectory(assets, "data/blocks").ok());
    const Block* bs = reg.findByName("bookshelf");
    ASSERT_NE(bs, nullptr);
    EXPECT_TRUE(bs->properties.enchantBookshelf); // the count flag
    EXPECT_EQ(bs->properties.hardness, 1.5f);
    EXPECT_EQ(bs->properties.drops, "bookshelf"); // drops self
    EXPECT_TRUE(bs->properties.interact.empty()); // no panel
}

TEST(EnchantBlocks, EnchantInteractIsWhitelisted) {
    // A block naming interact "enchant" is accepted (the whitelist).
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(
        R"({"id":"t","properties":{"interact":"enchant"},"model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    EXPECT_TRUE(r.ok()) << r.error().message;
    // An unknown interact is still rejected.
    auto bad = reg.registerBlock(parseDef(
        R"({"id":"t2","properties":{"interact":"bogus"},"model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    EXPECT_FALSE(bad.ok());
}

// ===========================================================================
// The two new recipes (content pins, loaded from the real data dir)
// ===========================================================================

TEST(EnchantRecipes, EnchantingTableRecipePinned) {
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok());
    BlockRegistry blocks;
    ASSERT_TRUE(blocks.loadDirectory(assets, "data/blocks").ok());
    ItemRegistry items;
    ASSERT_TRUE(items.buildBlockItems(blocks).ok());
    ASSERT_TRUE(items.loadDirectory(assets, "data/items").ok());
    RecipeRegistry recipes;
    ASSERT_TRUE(recipes.loadDirectory(assets, items).ok());
    const Recipe* r = recipes.find("enchanting_table");
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->type, RecipeType::Shapeless);
    EXPECT_EQ(r->outputCount, 1);
    // The shapeless ingredients: 4 obsidian + 2 diamond + 3 lapis (the bag).
    int obsidian = 0, diamond = 0, lapis = 0;
    for (const auto& [item, count] : r->ingredients) {
        const std::string name = items.at(item).name;
        if (name == "obsidian") obsidian = count;
        if (name == "diamond") diamond = count;
        if (name == "lapis") lapis = count;
    }
    EXPECT_EQ(obsidian, 4);
    EXPECT_EQ(diamond, 2);
    EXPECT_EQ(lapis, 3);
}

TEST(EnchantRecipes, BookshelfRecipePinned) {
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok());
    BlockRegistry blocks;
    ASSERT_TRUE(blocks.loadDirectory(assets, "data/blocks").ok());
    ItemRegistry items;
    ASSERT_TRUE(items.buildBlockItems(blocks).ok());
    ASSERT_TRUE(items.loadDirectory(assets, "data/items").ok());
    RecipeRegistry recipes;
    ASSERT_TRUE(recipes.loadDirectory(assets, items).ok());
    const Recipe* r = recipes.find("bookshelf");
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->type, RecipeType::Shaped);
    ASSERT_EQ(r->pattern.size(), 3u);
    EXPECT_EQ(r->pattern[0], "PPP");
    EXPECT_EQ(r->pattern[1], "LLL");
    EXPECT_EQ(r->pattern[2], "PPP");
    // P = planks, L = lapis (the MC "book" replaced by lapis, the deviation).
    const ItemID p = r->key.at('P');
    const ItemID l = r->key.at('L');
    EXPECT_EQ(items.at(p).name, "planks");
    EXPECT_EQ(items.at(l).name, "lapis");
}
