// Phase 15 UI tests: the inventory screen layout (pure function of the
// window size + panel kind + shapeless recipe count) and the pick-and-
// place + craft click state machine (InventoryUI) — all deterministic,
// no SDL/GL. Phase 17: the crafting grid cells + output slot of both
// panel kinds.
#include <gtest/gtest.h>

#include <string>

#include "core/json.hpp"
#include "item/crafting_grid.hpp"
#include "item/inventory.hpp"
#include "item/item_registry.hpp"
#include "item/recipe.hpp"
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
    CraftingGrid grid;
    RecipeRegistry recipes;
    // Holds references into the four above (declared after all of them).
    // CRAFT-1: the grid is bound in the constructor body (the E panel's
    // pattern — the table panel's UI is re-bound per block instead).
    InventoryUI ui;
    ItemID dirt;
    ItemID stone;
    ItemID pickaxe; // max stack 1
    ItemID planks;
    ItemID stick;
    ItemID table; // the shaped 2x2-planks output

    UiFixture() : ui(inv, items, recipes) {
        ui.bind(grid);
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
        r = items.registerItem(parseJson(
            R"({"id":"planks","type":"material"})"));
        if (!r.ok()) {
            ADD_FAILURE() << r.error().message;
        }
        r = items.registerItem(parseJson(
            R"({"id":"stick","type":"material"})"));
        if (!r.ok()) {
            ADD_FAILURE() << r.error().message;
        }
        r = items.registerItem(parseJson(
            R"({"id":"table","type":"material"})"));
        if (!r.ok()) {
            ADD_FAILURE() << r.error().message;
        }
        dirt = items.idForName("dirt");
        stone = items.idForName("stone");
        pickaxe = items.idForName("pickaxe");
        planks = items.idForName("planks");
        stick = items.idForName("stick");
        table = items.idForName("table");
        // One shapeless (2 planks -> 4 sticks) and one shaped (2x2 planks
        // -> 1 table) recipe: the layout's recipe rows count the
        // shapeless ones only; the grid matches both.
        auto rr = recipes.registerRecipe(
            parseJson(R"({"id":"sticks","output":{"item":"stick","count":4},
                         "ingredients":[{"item":"planks","count":2}]})"),
            items);
        if (!rr.ok()) {
            ADD_FAILURE() << rr.error().message;
        }
        rr = recipes.registerRecipe(
            parseJson(R"({"id":"table","type":"shaped",
                         "output":{"item":"table","count":1},
                         "pattern":["PP","PP"],"key":{"P":"planks"}})"),
            items);
        if (!rr.ok()) {
            ADD_FAILURE() << rr.error().message;
        }
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
    // The 2x2 grid of the Inventory kind + the output slot.
    ASSERT_EQ(layout.gridCells.size(), 4u);
    ASSERT_EQ(layout.gridSlots.size(), 4u);
    // The panel is centered.
    EXPECT_EQ(layout.panel.x, (1280 - layout.panel.w) / 2);
    EXPECT_EQ(layout.panel.y, (720 - layout.panel.h) / 2);
    // Every slot, grid cell, output and recipe row lies inside the panel.
    for (const UIRect& r : layout.slots) {
        EXPECT_TRUE(layout.panel.contains(r.x, r.y));
        EXPECT_TRUE(layout.panel.contains(r.x + r.w - 1, r.y + r.h - 1));
    }
    for (const UIRect& r : layout.gridCells) {
        EXPECT_TRUE(layout.panel.contains(r.x, r.y));
        EXPECT_TRUE(layout.panel.contains(r.x + r.w - 1, r.y + r.h - 1));
    }
    EXPECT_TRUE(layout.panel.contains(layout.output.x, layout.output.y));
    for (const UIRect& r : layout.recipes) {
        EXPECT_TRUE(layout.panel.contains(r.x, r.y));
        EXPECT_TRUE(layout.panel.contains(r.x + r.w - 1, r.y + r.h - 1));
    }
}

TEST(InventoryLayoutTest, GridCellsMapToTopLeftOf3x3) {
    const InventoryLayout layout =
        InventoryLayout::compute(1280, 720, 2, PanelKind::Inventory);
    // The 2x2 grid occupies the top-left 4 cells of the 3x3 model:
    // slots 0, 1 (top row) and 3, 4 (second row) — never 2, 5..8.
    const std::vector<int> want = {0, 1, 3, 4};
    ASSERT_EQ(layout.gridSlots.size(), 4u);
    for (std::size_t i = 0; i < 4; ++i) {
        EXPECT_EQ(layout.gridSlots[i], want[i]);
    }
    // The cells sit to the right of the inventory slots and left of the
    // recipe column; the output slot sits right of the grid.
    EXPECT_GT(layout.gridCells[0].x, layout.slots[8].x + layout.slots[8].w);
    EXPECT_LT(layout.gridCells[0].x, layout.recipes[0].x);
    EXPECT_EQ(layout.output.x,
              layout.gridCells[1].x + layout.gridCells[1].w +
                  InventoryLayout::kCraftGap);
}

TEST(InventoryLayoutTest, TableKindIs3x3WithShapedHintList) {
    // CRAFT-4: the table panel carries the shaped hint list in two
    // columns. 5 recipes -> left column rows 0..2, right column 3..4.
    const InventoryLayout layout =
        InventoryLayout::compute(1280, 720, 5, PanelKind::CraftingTable);
    ASSERT_EQ(layout.gridCells.size(), 9u);
    ASSERT_EQ(layout.gridSlots.size(), 9u);
    for (int i = 0; i < 9; ++i) {
        EXPECT_EQ(layout.gridSlots[static_cast<std::size_t>(i)], i);
    }
    ASSERT_EQ(layout.recipes.size(), 5u);
    // Native width with the two-column list:
    // 2*12 + 500 + 12 + (3*52+2*4+12+52) + 12 + (2*280+8) = 1344.
    const double sw = 1278.0 / 1344.0; // height (228) has plenty of room
    EXPECT_DOUBLE_EQ(layout.scale, sw);
    EXPECT_EQ(layout.panel.w, 1278);
    EXPECT_EQ(layout.panel.h, 217); // 228 * sw
    EXPECT_EQ(layout.panel.x, (1280 - layout.panel.w) / 2);
    // The rows split over the two columns; the right-column rows align
    // with the left column's row 0..1.
    for (int i = 0; i < 3; ++i) {
        EXPECT_EQ(layout.recipes[static_cast<std::size_t>(i)].x,
                  layout.craftHeader.x);
    }
    EXPECT_EQ(layout.recipes[3].x,
              layout.craftHeader.x + layout.recipes[3].w +
                  InventoryLayout::kRecipeColumnGap);
    EXPECT_EQ(layout.recipes[4].x, layout.recipes[3].x);
    for (int i = 3; i < 5; ++i) {
        EXPECT_EQ(layout.recipes[static_cast<std::size_t>(i)].y,
                  layout.recipes[static_cast<std::size_t>(i - 3)].y);
    }
    // The rows follow the (scaled) 52 px row height.
    EXPECT_EQ(layout.recipes[1].y - layout.recipes[0].y, 49); // 52 * sw
    // Every row sits inside the panel; the list stays right of the
    // output slot.
    for (const UIRect& r : layout.recipes) {
        EXPECT_TRUE(layout.panel.contains(r.x, r.y));
        EXPECT_TRUE(layout.panel.contains(r.x + r.w - 1, r.y + r.h - 1));
    }
    EXPECT_GT(layout.recipes[0].x, layout.output.x + layout.output.w);
    // The zero-recipe table keeps its former native HEIGHT design (228)
    // — scaled here like everything else.
    const InventoryLayout none =
        InventoryLayout::compute(1280, 720, 0, PanelKind::CraftingTable);
    EXPECT_EQ(none.panel.h, 217);
    EXPECT_TRUE(none.recipes.empty());
}

TEST(InventoryLayoutTest, TableHintRowHitTest) {
    const InventoryLayout layout =
        InventoryLayout::compute(1280, 720, 5, PanelKind::CraftingTable);
    // Left-column row 0 and right-column row 3 (its row-0 twin) both hit.
    const UIRect& r0 = layout.recipes[0];
    EXPECT_EQ(layout.recipeRowAt(r0.x + 2, r0.y + 2), 0);
    const UIRect& r3 = layout.recipes[3];
    EXPECT_EQ(layout.recipeRowAt(r3.x + 2, r3.y + 2), 3);
    // The gap between the two columns hits nothing.
    EXPECT_EQ(layout.recipeRowAt(r0.x + r0.w + 4, r0.y + 2), -1);
    // Hit tests stay mutually exclusive with slots/grid/output.
    EXPECT_EQ(layout.slotAt(r3.x + 2, r3.y + 2), -1);
    EXPECT_EQ(layout.gridCellAt(r3.x + 2, r3.y + 2), -1);
    EXPECT_FALSE(layout.inOutput(r3.x + 2, r3.y + 2));
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
    // Grid cell + output hit tests.
    const UIRect& g0 = layout.gridCells[0];
    EXPECT_EQ(layout.gridCellAt(g0.x + 1, g0.y + 1), 0);
    const UIRect& out = layout.output;
    EXPECT_TRUE(layout.inOutput(out.x + 1, out.y + 1));
    // The hit tests are mutually exclusive at any pixel: a point inside
    // the output is no slot and no grid cell.
    EXPECT_EQ(layout.slotAt(out.x + 1, out.y + 1), -1);
    EXPECT_EQ(layout.gridCellAt(out.x + 1, out.y + 1), -1);
    // Recipe row hit test.
    const UIRect& r1 = layout.recipes[1];
    EXPECT_EQ(layout.recipeRowAt(r1.x + 2, r1.y + 2), 1);
    // The gap between the grid and the craft column is empty.
    EXPECT_EQ(layout.slotAt(layout.panel.x + layout.panel.w - 2,
                            layout.panel.y + 2), -1);
}

TEST(InventoryLayoutTest, WideWindowKeepsTheNativeSize) {
    const InventoryLayout layout = InventoryLayout::compute(1280, 720, 2);
    EXPECT_DOUBLE_EQ(layout.scale, 1.0);
    // Native design: 2*12 + 500 + 12 + (2*52+4+12+52) + 12 + 280 = 1000.
    EXPECT_EQ(layout.panel.w, 1000);
    // 2*12 + max(40 + 3*52+2*4, 40 + 2*52) = 228.
    EXPECT_EQ(layout.panel.h, 228);
    EXPECT_EQ(layout.slots[0].w, InventoryLayout::kSlot);
}

TEST(InventoryLayoutTest, NarrowWindowScalesThePanelDown) {
    const InventoryLayout layout = InventoryLayout::compute(640, 360, 2);
    // Width-constrained: s = (640-2)/1000 (the height has plenty of room).
    EXPECT_DOUBLE_EQ(layout.scale, 638.0 / 1000.0);
    // The panel fits with >=1 px of margin on every side.
    EXPECT_GE(layout.panel.x, 0);
    EXPECT_GE(layout.panel.y, 0);
    EXPECT_LE(layout.panel.x + layout.panel.w, 640 - 1);
    EXPECT_LE(layout.panel.y + layout.panel.h, 360 - 1);
    // Slots are scaled (52 -> 33) and every child stays inside the panel.
    EXPECT_EQ(layout.slots[0].w, 33);
    for (const UIRect& r : layout.slots) {
        EXPECT_TRUE(layout.panel.contains(r.x, r.y));
        EXPECT_TRUE(layout.panel.contains(r.x + r.w - 1, r.y + r.h - 1));
    }
    // Adjacent hotbar slots never overlap (the scaled 4px gap is 2 or 3).
    for (int i = 0; i < 8; ++i) {
        const UIRect& a = layout.slots[static_cast<std::size_t>(i)];
        const UIRect& b = layout.slots[static_cast<std::size_t>(i + 1)];
        EXPECT_GE(b.x, a.x + a.w);
        EXPECT_LE(b.x - (a.x + a.w), 6);
    }
    // The hit tests follow the scaled rects: slot 5's center is slot 5.
    const UIRect& s5 = layout.slots[5];
    EXPECT_EQ(layout.slotAt(s5.x + s5.w / 2, s5.y + s5.h / 2), 5);
    // The grid cells and output still sit right of the slots and the
    // recipe rows right of the output (same relative order, scaled).
    EXPECT_GT(layout.gridCells[0].x, layout.slots[8].x + layout.slots[8].w);
    EXPECT_GT(layout.recipes[0].x, layout.output.x + layout.output.w);
}

TEST(InventoryLayoutTest, ShortWindowScalesByHeight) {
    const InventoryLayout layout = InventoryLayout::compute(1280, 200, 2);
    // Height-constrained: s = (200-2)/228 (the width has plenty of room).
    EXPECT_DOUBLE_EQ(layout.scale, 198.0 / 228.0);
    EXPECT_LE(layout.panel.y + layout.panel.h, 199);
    EXPECT_LE(layout.panel.x + layout.panel.w, 1279);
    EXPECT_GT(layout.panel.x, 0); // the scaled panel is narrower than 1280
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
    f.ui.leftClick(5, -1, false);
    EXPECT_TRUE(f.ui.carrying());
    EXPECT_EQ(f.ui.carry().item, f.dirt);
    EXPECT_EQ(f.ui.carry().count, 20);
    EXPECT_TRUE(f.inv.isEmpty(5));
}

TEST(InventoryUiTest, LeftClickPlacesCarriedOnEmpty) {
    UiFixture f;
    f.inv.setSlot(5, ItemStack {f.dirt, 20});
    f.ui.leftClick(5, -1, false);
    f.ui.leftClick(12, -1, false);
    EXPECT_FALSE(f.ui.carrying());
    EXPECT_EQ(f.inv.at(12), (ItemStack {f.dirt, 20}));
}

TEST(InventoryUiTest, LeftClickSwapsDifferentItems) {
    UiFixture f;
    f.inv.setSlot(3, ItemStack {f.dirt, 10});
    f.inv.setSlot(9, ItemStack {f.stone, 4});
    f.ui.leftClick(3, -1, false); // pick up the dirt
    f.ui.leftClick(9, -1, false); // the dirt lands in 9, the stone out
    EXPECT_EQ(f.ui.carry(), (ItemStack {f.stone, 4}));
    EXPECT_EQ(f.inv.at(9), (ItemStack {f.dirt, 10}));
    EXPECT_TRUE(f.inv.isEmpty(3));
    f.ui.leftClick(3, -1, false); // the stone goes back to its old neighbor
    EXPECT_FALSE(f.ui.carrying());
    EXPECT_EQ(f.inv.at(3), (ItemStack {f.stone, 4}));
    EXPECT_EQ(f.inv.at(9), (ItemStack {f.dirt, 10}));
}

TEST(InventoryUiTest, LeftClickMergesSameItemIntoRoom) {
    UiFixture f;
    f.inv.setSlot(2, ItemStack {f.dirt, 50});
    f.inv.setSlot(8, ItemStack {f.dirt, 30});
    f.ui.leftClick(2, -1, false); // pick up the 50 dirt
    f.ui.leftClick(8, -1, false); // 34 fit into the stack of 30; 16 stay
    EXPECT_EQ(f.ui.carry(), (ItemStack {f.dirt, 16}));
    EXPECT_EQ(f.inv.at(8), (ItemStack {f.dirt, 64})); // full stack
    f.ui.leftClick(2, -1, false); // the remainder goes back
    EXPECT_FALSE(f.ui.carrying());
    EXPECT_EQ(f.inv.at(2), (ItemStack {f.dirt, 16})); // 50 + 30 - 64
}

TEST(InventoryUiTest, LeftClickKeepsCarriedWhenNoRoom) {
    UiFixture f;
    f.inv.setSlot(2, ItemStack {f.dirt, 64}); // full
    f.inv.setSlot(8, ItemStack {f.dirt, 10});
    f.ui.leftClick(8, -1, false);
    f.ui.leftClick(2, -1, false);
    // No room in the full stack: the carried one stays in the hand.
    EXPECT_TRUE(f.ui.carrying());
    EXPECT_EQ(f.ui.carry(), (ItemStack {f.dirt, 10}));
    EXPECT_EQ(f.inv.at(2), (ItemStack {f.dirt, 64}));
    EXPECT_TRUE(f.inv.isEmpty(8));
}

TEST(InventoryUiTest, ClickOutsideDropsCarriedStack) {
    UiFixture f;
    f.inv.setSlot(5, ItemStack {f.dirt, 20});
    f.ui.leftClick(5, -1, false);
    f.ui.leftClick(-1, -1, false);
    EXPECT_FALSE(f.ui.carrying());
    EXPECT_TRUE(f.inv.isEmpty(5));
}

TEST(InventoryUiTest, RightClickTakesOne) {
    UiFixture f;
    f.inv.setSlot(5, ItemStack {f.dirt, 5});
    f.ui.rightClick(5, -1, false);
    EXPECT_EQ(f.ui.carry(), (ItemStack {f.dirt, 1}));
    EXPECT_EQ(f.inv.at(5), (ItemStack {f.dirt, 4}));
}

TEST(InventoryUiTest, RightClickPlacesOneOnEmpty) {
    UiFixture f;
    f.inv.setSlot(5, ItemStack {f.dirt, 5});
    f.ui.rightClick(5, -1, false);
    f.ui.rightClick(9, -1, false);
    EXPECT_FALSE(f.ui.carrying());
    EXPECT_EQ(f.inv.at(9), (ItemStack {f.dirt, 1}));
    EXPECT_EQ(f.inv.at(5), (ItemStack {f.dirt, 4}));
}

TEST(InventoryUiTest, RightClickAddsOneIntoRoom) {
    UiFixture f;
    f.inv.setSlot(5, ItemStack {f.dirt, 63});
    f.inv.setSlot(9, ItemStack {f.dirt, 1});
    f.ui.rightClick(9, -1, false);
    f.ui.rightClick(5, -1, false);
    EXPECT_FALSE(f.ui.carrying());
    EXPECT_EQ(f.inv.at(5), (ItemStack {f.dirt, 64}));
    EXPECT_TRUE(f.inv.isEmpty(9));
}

TEST(InventoryUiTest, RightClickNoOpOnFullOrDifferentItem) {
    UiFixture f;
    f.inv.setSlot(5, ItemStack {f.dirt, 64}); // full
    f.inv.setSlot(9, ItemStack {f.stone, 1});
    f.inv.setSlot(10, ItemStack {f.dirt, 1});
    f.ui.rightClick(10, -1, false); // carry 1 dirt
    f.ui.rightClick(5, -1, false);  // full dirt: nothing moves
    EXPECT_EQ(f.ui.carry(), (ItemStack {f.dirt, 1}));
    EXPECT_EQ(f.inv.at(5), (ItemStack {f.dirt, 64}));
    f.ui.rightClick(9, -1, false);  // different item, occupied: no-op
    EXPECT_EQ(f.ui.carry(), (ItemStack {f.dirt, 1}));
    EXPECT_EQ(f.inv.at(9), (ItemStack {f.stone, 1}));
    f.ui.rightClick(-1, -1, false); // outside: no-op
    EXPECT_EQ(f.ui.carry(), (ItemStack {f.dirt, 1}));
}

TEST(InventoryUiTest, RightClickRespectsMaxStackOne) {
    UiFixture f;
    // A tool has max stack 1: a second tool cannot share a slot.
    f.inv.setSlot(5, ItemStack {f.pickaxe, 1});
    f.inv.setSlot(9, ItemStack {f.pickaxe, 1});
    f.ui.rightClick(9, -1, false);
    f.ui.rightClick(5, -1, false); // slot full (1/1): the tool stays
    EXPECT_EQ(f.ui.carry(), (ItemStack {f.pickaxe, 1}));
    EXPECT_EQ(f.inv.at(5), (ItemStack {f.pickaxe, 1}));
}

// -- crafting grid + output (Phase 17) -----------------------------------------

TEST(InventoryUiTest, GridCellPickAndPlace) {
    UiFixture f;
    f.inv.setSlot(5, ItemStack {f.planks, 8});
    f.ui.leftClick(5, -1, false);   // carry 8 planks
    f.ui.leftClick(-1, 0, false);   // the whole stack lands in cell 0
    EXPECT_EQ(f.ui.grid().at(0), (ItemStack {f.planks, 8}));
    EXPECT_FALSE(f.ui.carrying());
    EXPECT_TRUE(f.inv.isEmpty(5));
    f.ui.leftClick(-1, 0, false);   // pick the 8 back up
    f.ui.rightClick(-1, 3, false);  // one into cell 3 (grid slot 3 = row1/col0)
    f.ui.rightClick(-1, 1, false);  // one into cell 1
    f.ui.leftClick(-1, 3, false);   // ...and the rest goes back to cell 3
    EXPECT_EQ(f.ui.grid().at(3), (ItemStack {f.planks, 7}));
    EXPECT_EQ(f.ui.grid().at(1), (ItemStack {f.planks, 1}));
    EXPECT_FALSE(f.ui.carrying());
}

TEST(InventoryUiTest, OutputShowsShapedMatch) {
    UiFixture f;
    // One planks in each of the 2x2's four cells: the shaped table
    // recipe matches (and beats the shapeless sticks recipe).
    f.grid.setSlot(0, (ItemStack {f.planks, 1}));
    f.grid.setSlot(1, (ItemStack {f.planks, 1}));
    f.grid.setSlot(3, (ItemStack {f.planks, 1}));
    f.grid.setSlot(4, (ItemStack {f.planks, 1}));
    const GridMatch m = f.ui.match();
    ASSERT_TRUE(m.matched());
    EXPECT_EQ(m.recipe->id, "table");
    EXPECT_EQ(m.dx, 0);
    EXPECT_EQ(m.dy, 0);
}

TEST(InventoryUiTest, OutputCraftConsumesGrid) {
    UiFixture f;
    f.grid.setSlot(0, (ItemStack {f.planks, 5}));
    f.grid.setSlot(1, (ItemStack {f.planks, 1}));
    f.grid.setSlot(3, (ItemStack {f.planks, 1}));
    f.grid.setSlot(4, (ItemStack {f.planks, 1}));
    const bool crafted = f.ui.leftClick(-1, -1, true);
    ASSERT_TRUE(crafted);
    // The shaped craft consumes exactly one per covered cell: the extra
    // 4 planks in cell 0 survive.
    EXPECT_EQ(f.ui.grid().at(0), (ItemStack {f.planks, 4}));
    EXPECT_TRUE(f.ui.grid().isEmpty(1));
    EXPECT_TRUE(f.ui.grid().isEmpty(3));
    EXPECT_TRUE(f.ui.grid().isEmpty(4));
    EXPECT_EQ(f.ui.carry(), (ItemStack {f.table, 1}));
    // The 4 surviving planks in cell 0 still satisfy the SHAPELESS sticks
    // recipe (the shaped table no longer does: 3 of its cells are empty).
    const GridMatch after = f.ui.match();
    ASSERT_TRUE(after.matched());
    EXPECT_EQ(after.recipe->id, "sticks");
}

TEST(InventoryUiTest, OutputRightClickTakesOne) {
    UiFixture f;
    // Shapeless: 2 planks anywhere -> 4 sticks.
    f.grid.setSlot(0, (ItemStack {f.planks, 2}));
    const bool crafted = f.ui.rightClick(-1, -1, true);
    ASSERT_TRUE(crafted);
    EXPECT_EQ(f.ui.carry(), (ItemStack {f.stick, 1}));
    EXPECT_TRUE(f.ui.grid().empty());
    // Nothing left to craft: further output clicks are no-ops.
    EXPECT_FALSE(f.ui.leftClick(-1, -1, true));
    EXPECT_FALSE(f.ui.rightClick(-1, -1, true));
    EXPECT_EQ(f.ui.carry(), (ItemStack {f.stick, 1}));
}

TEST(InventoryUiTest, OutputRefusesForeignCarry) {
    UiFixture f;
    f.grid.setSlot(0, (ItemStack {f.planks, 2}));
    f.inv.setSlot(5, ItemStack {f.dirt, 1});
    f.ui.leftClick(5, -1, false); // carry 1 dirt
    // The output cannot hold a foreign item: no craft, grid untouched.
    EXPECT_FALSE(f.ui.leftClick(-1, -1, true));
    EXPECT_EQ(f.ui.grid().at(0), (ItemStack {f.planks, 2}));
    EXPECT_EQ(f.ui.carry(), (ItemStack {f.dirt, 1}));
    // Same item with room merges into the carry instead: swap the dirt
    // for the stick stack, then craft on top of it.
    f.inv.setSlot(6, ItemStack {f.stick, 3});
    f.ui.leftClick(6, -1, false); // swap: dirt in 6, 3 sticks carried
    EXPECT_EQ(f.inv.at(6), (ItemStack {f.dirt, 1}));
    EXPECT_EQ(f.ui.carry(), (ItemStack {f.stick, 3}));
    EXPECT_TRUE(f.ui.leftClick(-1, -1, true)); // 3 + 4 fit in 64
    EXPECT_EQ(f.ui.carry(), (ItemStack {f.stick, 7}));
    EXPECT_TRUE(f.ui.grid().empty());
}

TEST(InventoryUiTest, GridAndInventoryInteractThroughCarry) {
    UiFixture f;
    // The grid and the 27-slot inventory share the carried stack: move a
    // stack from a grid cell to an inventory slot and back.
    f.grid.setSlot(0, (ItemStack {f.stone, 10}));
    f.ui.leftClick(-1, 0, false); // carry 10 stone
    f.ui.leftClick(11, -1, false); // into inventory slot 11
    EXPECT_EQ(f.inv.at(11), (ItemStack {f.stone, 10}));
    EXPECT_TRUE(f.ui.grid().empty());
    f.ui.leftClick(11, -1, false); // back out...
    f.ui.leftClick(-1, 4, false);  // ...into grid slot 4
    EXPECT_EQ(f.ui.grid().at(4), (ItemStack {f.stone, 10}));
    EXPECT_TRUE(f.inv.isEmpty(11));
}

TEST(InventoryUiTest, UnmatchedGridCraftsNothing) {
    UiFixture f;
    // A single planks cell matches neither recipe (sticks need 2, the
    // table needs the full 2x2).
    f.grid.setSlot(0, (ItemStack {f.planks, 1}));
    EXPECT_FALSE(f.ui.match().matched());
    EXPECT_FALSE(f.ui.leftClick(-1, -1, true));
    EXPECT_EQ(f.ui.grid().at(0), (ItemStack {f.planks, 1}));
    EXPECT_FALSE(f.ui.carrying());
}

// -- CRAFT-1: the bindable grid (per-block table separation) ---------------
// The table panel's InventoryUI is re-bound to the clicked block's
// per-block grid on every open (the game does the binding — the UI just
// operates on the bound one). These tests pin the isolation: a re-bind
// leaves the previous grid intact and the new grid empty.

TEST(InventoryUiBindTest, RebindSeparatesTheGrids) {
    UiFixture f;
    CraftingGrid other; // the second table block's grid
    // Fill the fixture grid through the bound UI (grid A).
    f.inv.setSlot(0, ItemStack {f.dirt, 2});
    f.ui.leftClick(0, -1, false);  // carry the stack
    f.ui.leftClick(-1, 2, false);  // into A cell 2
    ASSERT_EQ(f.grid.at(2), (ItemStack {f.dirt, 2}));
    // Re-bind to grid B (the other table block).
    f.ui.bind(other);
    ASSERT_EQ(&f.ui.grid(), static_cast<const CraftingGrid*>(&other)); // the UI now exposes B
    EXPECT_TRUE(other.at(2).empty());   // B started empty
    EXPECT_EQ(f.grid.at(2), (ItemStack {f.dirt, 2})); // A kept its stack
    // An operation on B leaves A untouched.
    f.inv.setSlot(1, ItemStack {f.stone, 1});
    f.ui.leftClick(1, -1, false);  // carry the stone
    f.ui.leftClick(-1, 0, false);  // into B cell 0
    EXPECT_EQ(other.at(0), (ItemStack {f.stone, 1}));
    EXPECT_TRUE(f.grid.at(0).empty());
    EXPECT_EQ(f.grid.at(2), (ItemStack {f.dirt, 2}));
}

TEST(InventoryUiBindTest, CraftConsumesOnlyTheBoundGrid) {
    UiFixture f;
    CraftingGrid other;
    // A: the full 2x2 planks (the shaped table recipe matches).
    f.grid.setSlot(0, (ItemStack {f.planks, 1}));
    f.grid.setSlot(1, (ItemStack {f.planks, 1}));
    f.grid.setSlot(3, (ItemStack {f.planks, 1}));
    f.grid.setSlot(4, (ItemStack {f.planks, 1}));
    // Re-bind to the EMPTY grid B and craft: the output is empty (B
    // matches nothing) and A is untouched.
    f.ui.bind(other);
    EXPECT_FALSE(f.ui.match().matched());
    EXPECT_FALSE(f.ui.leftClick(-1, -1, true));
    EXPECT_TRUE(other.empty());
    EXPECT_EQ(f.grid.at(0), (ItemStack {f.planks, 1}));
    EXPECT_EQ(f.grid.at(4), (ItemStack {f.planks, 1}));
    // Bind A back: the same craft now consumes A's cells.
    f.ui.bind(f.grid);
    ASSERT_TRUE(f.ui.match().matched());
    ASSERT_TRUE(f.ui.leftClick(-1, -1, true));
    EXPECT_TRUE(f.grid.empty());
    EXPECT_EQ(f.ui.carry(), (ItemStack {f.table, 1}));
}
