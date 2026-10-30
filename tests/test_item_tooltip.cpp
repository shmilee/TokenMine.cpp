// PRE-12 (tooltip): the item hover tooltip lines (ui/item_tooltip, the
// campfire paradigm — the unit tests own the formatting, the Game's
// hover sites are thin adapters). Consumes the Phase-15 KNOWN_ISSUES
// residue "the item tooltip shows only the name" (now: name + " xN" +
// the d/max durability readout).
#include <gtest/gtest.h>

#include "item/item.hpp"
#include "ui/item_tooltip.hpp"

using namespace mc;

namespace {

Item makeItem(const std::string& name, float durability = 0.0f) {
    Item it;
    it.name = name;
    it.durability = durability;
    return it;
}

ItemStack makeStack(const Item& item, int count, float damage = 0.0f) {
    ItemStack s;
    s.item = ItemID{7}; // any valid id — the tooltip is name/data driven
    s.count = count;
    s.damage = damage;
    (void)item;
    return s;
}

} // namespace

// A single item without durability: just the name, no second line.
TEST(ItemTooltipTest, SingleItemNoDurability) {
    const Item it = makeItem("dirt");
    const ItemTooltip t = itemTooltip(it, makeStack(it, 1));
    EXPECT_EQ(t.title, "dirt");
    EXPECT_TRUE(t.durability.empty());
}

// A stack of more than one item: the " xN" suffix (the same form as the
// recipe-row output label).
TEST(ItemTooltipTest, StackCountSuffix) {
    const Item it = makeItem("dirt");
    const ItemTooltip t = itemTooltip(it, makeStack(it, 64));
    EXPECT_EQ(t.title, "dirt x64");
    EXPECT_TRUE(t.durability.empty());
}

// A tool with durability: the d/max readout line (the F3 held-line
// convention — full durability shows 0/max, text unlike the HUD strip).
TEST(ItemTooltipTest, DurabilityReadoutFull) {
    const Item it = makeItem("wooden_pickaxe", 59.0f);
    const ItemTooltip t = itemTooltip(it, makeStack(it, 1, 0.0f));
    EXPECT_EQ(t.title, "wooden_pickaxe");
    EXPECT_EQ(t.durability, "0.0/59");
}

// A worn tool: the accumulated damage (one decimal place).
TEST(ItemTooltipTest, DurabilityReadoutWorn) {
    const Item it = makeItem("iron_pickaxe", 250.0f);
    const ItemTooltip t = itemTooltip(it, makeStack(it, 1, 238.5f));
    EXPECT_EQ(t.title, "iron_pickaxe");
    EXPECT_EQ(t.durability, "238.5/250");
}

// Count + durability combine (a stack of tools is impossible with
// max_stack 1 in practice, but the formatter is data-driven and must
// not assume one).
TEST(ItemTooltipTest, CountAndDurabilityCombine) {
    const Item it = makeItem("stick", 10.0f);
    const ItemTooltip t = itemTooltip(it, makeStack(it, 16, 2.5f));
    EXPECT_EQ(t.title, "stick x16");
    EXPECT_EQ(t.durability, "2.5/10");
}
