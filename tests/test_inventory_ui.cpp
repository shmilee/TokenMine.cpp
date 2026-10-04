// Phase 15 UI tests: the inventory screen layout (pure function of the
// window size + recipe count) and the pick-and-place click state machine
// (InventoryUI) — all deterministic, no SDL/GL.
#include <gtest/gtest.h>

#include <string>

#include "core/json.hpp"
#include "item/inventory.hpp"
#include "item/item_registry.hpp"
#include "ui/inventory_ui.hpp"

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

struct UiFixture {
    ItemRegistry items;
    Inventory inv;
    // Holds references into the two above (declared after both).
    InventoryUI ui;
    ItemID dirt;
    ItemID stone;
    ItemID pickaxe; // max stack 1

    UiFixture() : ui(inv, items) {
        auto r = items.registerItem(parseJson(
            R"({"id":"dirt","type":"material"})"));
        if (!r.ok()) {
            ADD_FAILURE() << r.error().message;
        }
        r = items.registerItem(
            parseJson(R"({"id":"stone","type":"material"})"));
        if (!r.ok()) {
            ADD_FAILURE() << r.error().message;
        }
        r = items.registerItem(parseJson(
            R"({"id":"pickaxe","type":"tool","tool":{"break_speed":2.0},"max_stack":1})"));
        if (!r.ok()) {
            ADD_FAILURE() << r.error().message;
        }
        dirt = items.idForName("dirt");
        stone = items.idForName("stone");
        pickaxe = items.idForName("pickaxe");
    }
};

} // namespace

// -- InventoryLayout -----------------------------------------------------------

TEST(InventoryLayoutTest, ComputesSlotsAndPanel) {
    const InventoryLayout layout =
        InventoryLayout::compute(1280, 720, 4);
    // 27 slots: 18 storage + 9 hotbar.
    ASSERT_EQ(layout.slots.size(), 27u);
    ASSERT_EQ(layout.recipes.size(), 4u);
    // The panel is centered.
    EXPECT_EQ(layout.panel.x, (1280 - layout.panel.w) / 2);
    EXPECT_EQ(layout.panel.y, (720 - layout.panel.h) / 2);
    // Every slot and recipe row lies inside the panel.
    for (const UIRect& r : layout.slots) {
        EXPECT_TRUE(layout.panel.contains(r.x, r.y));
        EXPECT_TRUE(layout.panel.contains(r.x + r.w - 1, r.y + r.h - 1));
    }
    for (const UIRect& r : layout.recipes) {
        EXPECT_TRUE(layout.panel.contains(r.x, r.y));
        EXPECT_TRUE(layout.panel.contains(r.x + r.w - 1, r.y + r.h - 1));
    }
}

TEST(InventoryLayoutTest, SlotHitTest) {
    const InventoryLayout layout = InventoryLayout::compute(1280, 720, 4);
    // Slot 0's center maps to slot 0.
    const UIRect& s0 = layout.slots[0];
    EXPECT_EQ(layout.slotAt(s0.x + 1, s0.y + 1), 0);
    // A point between slots maps to nothing (the 4px gap).
    EXPECT_EQ(layout.slotAt(layout.slots[0].x + layout.slots[0].w + 2,
                            layout.slots[0].y + 1), -1);
    // Slot 26 (last storage slot, top-right area).
    const UIRect& s26 = layout.slots[26];
    EXPECT_EQ(layout.slotAt(s26.x + s26.w / 2, s26.y + s26.h / 2), 26);
    // Recipe row hit test.
    const UIRect& r1 = layout.recipes[1];
    EXPECT_EQ(layout.recipeRowAt(r1.x + 2, r1.y + 2), 1);
    // The gap between the grid and the craft column is empty.
    EXPECT_EQ(layout.slotAt(layout.panel.x + layout.panel.w - 2,
                            layout.panel.y + 2), -1);
}

TEST(InventoryLayoutTest, SlotOrderIsStorageThenHotbar) {
    const InventoryLayout layout = InventoryLayout::compute(1280, 720, 4);
    // The bottom row (largest y) is the hotbar: slots 0..8 left to right.
    int hotbarRowY = -1;
    for (const UIRect& r : layout.slots) {
        if (r.y > hotbarRowY) {
            hotbarRowY = r.y;
        }
    }
    for (int i = 0; i < 9; ++i) {
        EXPECT_EQ(layout.slots[static_cast<std::size_t>(i)].y, hotbarRowY);
    }
    // The two storage rows sit above it.
    EXPECT_LT(layout.slots[9].y, hotbarRowY);
    EXPECT_LT(layout.slots[26].y, hotbarRowY);
    // Storage row 1 (slots 18..26) sits directly above the hotbar row;
    // storage row 0 (slots 9..17) is the topmost.
    EXPECT_EQ(layout.slots[18].y,
              layout.slots[0].y - (InventoryLayout::kSlot +
                                   InventoryLayout::kGap));
    EXPECT_EQ(layout.slots[9].y,
              layout.slots[0].y - 2 * (InventoryLayout::kSlot +
                                       InventoryLayout::kGap));
}

// -- InventoryUI click state machine --------------------------------------------

TEST(InventoryUiTest, LeftClickPicksUpWholeStack) {
    UiFixture f;
    f.inv.setSlot(5, ItemStack {f.dirt, 20});
    f.ui.leftClick(5);
    EXPECT_TRUE(f.ui.carrying());
    EXPECT_EQ(f.ui.carry().item, f.dirt);
    EXPECT_EQ(f.ui.carry().count, 20);
    EXPECT_TRUE(f.inv.isEmpty(5));
}

TEST(InventoryUiTest, LeftClickPlacesCarriedOnEmpty) {
    UiFixture f;
    f.inv.setSlot(5, ItemStack {f.dirt, 20});
    f.ui.leftClick(5);
    f.ui.leftClick(12);
    EXPECT_FALSE(f.ui.carrying());
    EXPECT_EQ(f.inv.at(12), (ItemStack {f.dirt, 20}));
}

TEST(InventoryUiTest, LeftClickSwapsDifferentItems) {
    UiFixture f;
    f.inv.setSlot(3, ItemStack {f.dirt, 10});
    f.inv.setSlot(9, ItemStack {f.stone, 4});
    f.ui.leftClick(3); // pick up the dirt
    f.ui.leftClick(9); // the dirt lands in 9, the stone comes out to the hand
    EXPECT_EQ(f.ui.carry(), (ItemStack {f.stone, 4}));
    EXPECT_EQ(f.inv.at(9), (ItemStack {f.dirt, 10}));
    EXPECT_TRUE(f.inv.isEmpty(3));
    f.ui.leftClick(3); // the stone goes back to its old neighbor
    EXPECT_FALSE(f.ui.carrying());
    EXPECT_EQ(f.inv.at(3), (ItemStack {f.stone, 4}));
    EXPECT_EQ(f.inv.at(9), (ItemStack {f.dirt, 10}));
}

TEST(InventoryUiTest, LeftClickMergesSameItemIntoRoom) {
    UiFixture f;
    f.inv.setSlot(2, ItemStack {f.dirt, 50});
    f.inv.setSlot(8, ItemStack {f.dirt, 30});
    f.ui.leftClick(2); // pick up the 50 dirt
    f.ui.leftClick(8); // 34 fit into the stack of 30; 16 stay carried
    EXPECT_EQ(f.ui.carry(), (ItemStack {f.dirt, 16}));
    EXPECT_EQ(f.inv.at(8), (ItemStack {f.dirt, 64})); // full stack
    f.ui.leftClick(2); // the remainder goes back
    EXPECT_FALSE(f.ui.carrying());
    EXPECT_EQ(f.inv.at(2), (ItemStack {f.dirt, 16})); // 50 + 30 - 64
}

TEST(InventoryUiTest, LeftClickKeepsCarriedWhenNoRoom) {
    UiFixture f;
    f.inv.setSlot(2, ItemStack {f.dirt, 64}); // full
    f.inv.setSlot(8, ItemStack {f.dirt, 10});
    f.ui.leftClick(8);
    f.ui.leftClick(2);
    // No room in the full stack: the carried one stays in the hand.
    EXPECT_TRUE(f.ui.carrying());
    EXPECT_EQ(f.ui.carry(), (ItemStack {f.dirt, 10}));
    EXPECT_EQ(f.inv.at(2), (ItemStack {f.dirt, 64}));
    EXPECT_TRUE(f.inv.isEmpty(8));
}

TEST(InventoryUiTest, ClickOutsideDropsCarriedStack) {
    UiFixture f;
    f.inv.setSlot(5, ItemStack {f.dirt, 20});
    f.ui.leftClick(5);
    f.ui.leftClick(-1);
    EXPECT_FALSE(f.ui.carrying());
    EXPECT_TRUE(f.inv.isEmpty(5));
}

TEST(InventoryUiTest, RightClickTakesOne) {
    UiFixture f;
    f.inv.setSlot(5, ItemStack {f.dirt, 5});
    f.ui.rightClick(5);
    EXPECT_EQ(f.ui.carry(), (ItemStack {f.dirt, 1}));
    EXPECT_EQ(f.inv.at(5), (ItemStack {f.dirt, 4}));
}

TEST(InventoryUiTest, RightClickPlacesOneOnEmpty) {
    UiFixture f;
    f.inv.setSlot(5, ItemStack {f.dirt, 5});
    f.ui.rightClick(5);
    f.ui.rightClick(9);
    EXPECT_FALSE(f.ui.carrying());
    EXPECT_EQ(f.inv.at(9), (ItemStack {f.dirt, 1}));
    EXPECT_EQ(f.inv.at(5), (ItemStack {f.dirt, 4}));
}

TEST(InventoryUiTest, RightClickAddsOneIntoRoom) {
    UiFixture f;
    f.inv.setSlot(5, ItemStack {f.dirt, 63});
    f.inv.setSlot(9, ItemStack {f.dirt, 1});
    f.ui.rightClick(9);
    f.ui.rightClick(5);
    EXPECT_FALSE(f.ui.carrying());
    EXPECT_EQ(f.inv.at(5), (ItemStack {f.dirt, 64}));
    EXPECT_TRUE(f.inv.isEmpty(9));
}

TEST(InventoryUiTest, RightClickNoOpOnFullOrDifferentItem) {
    UiFixture f;
    f.inv.setSlot(5, ItemStack {f.dirt, 64}); // full
    f.inv.setSlot(9, ItemStack {f.stone, 1});
    f.inv.setSlot(10, ItemStack {f.dirt, 1});
    f.ui.rightClick(10); // carry 1 dirt
    f.ui.rightClick(5);  // full dirt: nothing moves
    EXPECT_EQ(f.ui.carry(), (ItemStack {f.dirt, 1}));
    EXPECT_EQ(f.inv.at(5), (ItemStack {f.dirt, 64}));
    f.ui.rightClick(9);  // different item, occupied: no-op
    EXPECT_EQ(f.ui.carry(), (ItemStack {f.dirt, 1}));
    EXPECT_EQ(f.inv.at(9), (ItemStack {f.stone, 1}));
    f.ui.rightClick(-1); // outside: no-op
    EXPECT_EQ(f.ui.carry(), (ItemStack {f.dirt, 1}));
}

TEST(InventoryUiTest, RightClickRespectsMaxStackOne) {
    UiFixture f;
    // A tool has max stack 1: a second tool cannot share a slot.
    f.inv.setSlot(5, ItemStack {f.pickaxe, 1});
    f.inv.setSlot(9, ItemStack {f.pickaxe, 1});
    f.ui.rightClick(9);
    f.ui.rightClick(5); // slot full (1/1): the tool stays carried
    EXPECT_EQ(f.ui.carry(), (ItemStack {f.pickaxe, 1}));
    EXPECT_EQ(f.inv.at(5), (ItemStack {f.pickaxe, 1}));
}
