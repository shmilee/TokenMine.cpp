// Phase 40 furnace tests: the pure per-substep tick (fuel ignition /
// burning / consumption, smelt progress / completion / pausing, the
// progress-reset rules), the data-time -> substep conversion, the
// FurnaceLayout geometry (pinned rects + hit tests + scale-down) and the
// FurnaceUI pick-and-place state machine (including the input-change
// progress reset). The save format-6 round trip and validation live in
// test_save.cpp (they reuse the save fixture).
#include <gtest/gtest.h>

#include <string>

#include "core/json.hpp"
#include "item/furnace.hpp"
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

// The smelting test set: beef -> cooked (default 10 s), mutton ->
// cooked_mutton (20 s), coal (80 s fuel), planks (7.5 s fuel), stick
// (5 s fuel), stone (neither).
struct Fixture {
    ItemRegistry items;

    Fixture() {
        const struct {
            const char* def;
        } defs[] = {
            {R"({"id":"beef","type":"material","smeltable_to":"cooked"})"},
            {R"({"id":"mutton","type":"material","smeltable_to":"cooked_mutton","smelt_time":20})"},
            {R"({"id":"cooked","type":"material","max_stack":64})"},
            {R"({"id":"cooked_mutton","type":"material","max_stack":64})"},
            {R"({"id":"coal","type":"material","fuel_time":80})"},
            {R"({"id":"planks","type":"material","fuel_time":7.5})"},
            {R"({"id":"stick","type":"material","fuel_time":5})"},
            {R"({"id":"stone","type":"material"})"},
        };
        for (const auto& d : defs) {
            auto r = items.registerItem(parseJson(d.def));
            if (!r.ok()) {
                ADD_FAILURE() << d.def << ": " << r.error().message;
            }
        }
    }

    ItemID id(const char* name) const { return items.idForName(name); }
};

// ItemStack construction for EXPECT/ASSERT arguments (a brace-init
// inside a macro call would split on the comma).
ItemStack stk(ItemID id, int count) { return ItemStack {id, count}; }

void expectRect(const UIRect& r, int x, int y, int w, int h, const char* what) {
    EXPECT_EQ(r.x, x) << what;
    EXPECT_EQ(r.y, y) << what;
    EXPECT_EQ(r.w, w) << what;
    EXPECT_EQ(r.h, h) << what;
}

} // namespace

// -- data times in substeps ---------------------------------------------------

TEST(FurnaceTest, SubstepConversion) {
    Fixture f;
    // round(t * 60): exact for these data values.
    EXPECT_EQ(smeltSubstepsFor(f.items.at(f.id("beef"))), 600); // default 10 s
    EXPECT_EQ(smeltSubstepsFor(f.items.at(f.id("mutton"))), 1200);
    EXPECT_EQ(smeltSubstepsFor(f.items.at(f.id("coal"))), 0); // not smeltable
    EXPECT_EQ(fuelSubstepsFor(f.items.at(f.id("coal"))), 4800);
    EXPECT_EQ(fuelSubstepsFor(f.items.at(f.id("planks"))), 450);
    EXPECT_EQ(fuelSubstepsFor(f.items.at(f.id("stick"))), 300);
    EXPECT_EQ(fuelSubstepsFor(f.items.at(f.id("beef"))), 0); // not fuel
}

// -- fuel ---------------------------------------------------------------------

TEST(FurnaceTest, IgnitionConsumesFuel) {
    Fixture f;
    FurnaceState s;
    s.fuel = stk(f.id("coal"), 1);
    tickFurnace(s, f.items);
    // Consumed at ignition; the full burn time is granted on the
    // ignition tick itself (which also smelts — one substep of the
    // item's burn is effectively "free", deterministic).
    EXPECT_TRUE(s.fuel.empty());
    EXPECT_EQ(s.fuelSubsteps, 4800);
}

TEST(FurnaceTest, NonFuelInSlotNeverBurns) {
    Fixture f;
    FurnaceState s;
    s.fuel = ItemStack {f.id("stone"), 1};
    s.input = ItemStack {f.id("beef"), 1};
    for (int i = 0; i < 100; ++i) {
        tickFurnace(s, f.items);
    }
    EXPECT_EQ(s.fuel, stk(f.id("stone"), 1));
    EXPECT_EQ(s.fuelSubsteps, 0);
    EXPECT_EQ(s.progressSubsteps, 0);
}

TEST(FurnaceTest, FuelSwitchCarriesOver) {
    Fixture f;
    FurnaceState s;
    s.fuel = ItemStack {f.id("planks"), 1};
    s.fuelSubsteps = 5; // mid-burn of the previous fuel
    s.input = ItemStack {f.id("beef"), 1};
    for (int i = 0; i < 5; ++i) {
        tickFurnace(s, f.items);
    }
    // On the tick the fire went out, the next fuel lit immediately
    // (the full burn time is granted on that same tick — the fire never
    // sits out for a whole substep while fuel stock remains).
    EXPECT_EQ(s.fuelSubsteps, 450);
    EXPECT_TRUE(s.fuel.empty());
    tickFurnace(s, f.items);
    EXPECT_EQ(s.fuelSubsteps, 449);
    EXPECT_EQ(s.progressSubsteps, 6); // burned the whole time
}

// -- smelting -------------------------------------------------------------------

TEST(FurnaceTest, NoFuelNoSmelt) {
    Fixture f;
    FurnaceState s;
    s.input = ItemStack {f.id("beef"), 1};
    for (int i = 0; i < 700; ++i) {
        tickFurnace(s, f.items);
    }
    EXPECT_EQ(s.input, stk(f.id("beef"), 1));
    EXPECT_EQ(s.progressSubsteps, 0);
    EXPECT_TRUE(s.output.empty());
}

TEST(FurnaceTest, FullSmeltCompletes) {
    Fixture f;
    FurnaceState s;
    s.input = ItemStack {f.id("beef"), 2};
    s.fuel = ItemStack {f.id("coal"), 1};
    for (int i = 0; i < 599; ++i) {
        tickFurnace(s, f.items);
    }
    EXPECT_EQ(s.progressSubsteps, 599);
    EXPECT_EQ(s.input, stk(f.id("beef"), 2));
    tickFurnace(s, f.items); // substep 600: done
    EXPECT_EQ(s.input, stk(f.id("beef"), 1));
    EXPECT_EQ(s.output, stk(f.id("cooked"), 1));
    EXPECT_EQ(s.progressSubsteps, 0);
    EXPECT_EQ(s.fuelSubsteps, 4800 - 599); // the ignition tick smelted free
}

TEST(FurnaceTest, SecondUnitStartsFromZero) {
    Fixture f;
    FurnaceState s;
    s.input = ItemStack {f.id("beef"), 2};
    s.fuel = ItemStack {f.id("coal"), 1};
    for (int i = 0; i < 1200; ++i) {
        tickFurnace(s, f.items);
    }
    EXPECT_TRUE(s.input.empty());
    EXPECT_EQ(s.output, stk(f.id("cooked"), 2));
    EXPECT_EQ(s.progressSubsteps, 0);
    EXPECT_EQ(s.fuelSubsteps, 4800 - 1199); // the ignition tick smelted free
}

TEST(FurnaceTest, OutputFullPausesButFireBurns) {
    Fixture f;
    FurnaceState s;
    s.input = ItemStack {f.id("beef"), 1};
    s.fuel = ItemStack {f.id("coal"), 1};
    s.output = ItemStack {f.id("cooked"), 64}; // max stack: no room
    for (int i = 0; i < 100; ++i) {
        tickFurnace(s, f.items);
    }
    EXPECT_EQ(s.progressSubsteps, 0);
    EXPECT_EQ(s.fuelSubsteps, 4800 - 99); // the fire keeps burning
    EXPECT_EQ(s.input, stk(f.id("beef"), 1));
    s.output = ItemStack {}; // room opens up
    for (int i = 0; i < 600; ++i) {
        tickFurnace(s, f.items);
    }
    EXPECT_EQ(s.output, stk(f.id("cooked"), 1));
    EXPECT_TRUE(s.input.empty());
}

TEST(FurnaceTest, OutputHoldingOtherItemBlocksSmelt) {
    Fixture f;
    FurnaceState s;
    s.input = ItemStack {f.id("beef"), 1};
    s.fuel = ItemStack {f.id("coal"), 1};
    s.output = ItemStack {f.id("stone"), 1}; // not the product: no room
    for (int i = 0; i < 100; ++i) {
        tickFurnace(s, f.items);
    }
    EXPECT_EQ(s.progressSubsteps, 0);
    EXPECT_EQ(s.input, stk(f.id("beef"), 1));
    EXPECT_EQ(s.output, stk(f.id("stone"), 1));
}

// -- progress reset rules -------------------------------------------------------

TEST(FurnaceTest, ProgressLostWhenInputBecomesEmpty) {
    Fixture f;
    FurnaceState s;
    s.input = ItemStack {f.id("beef"), 2};
    s.fuel = ItemStack {f.id("coal"), 1};
    for (int i = 0; i < 100; ++i) {
        tickFurnace(s, f.items);
    }
    EXPECT_EQ(s.progressSubsteps, 100);
    s.input = ItemStack {}; // the player takes it out
    tickFurnace(s, f.items);
    EXPECT_EQ(s.progressSubsteps, 0);
}

TEST(FurnaceTest, ProgressLostWhenInputChangesItem) {
    Fixture f;
    FurnaceState s;
    s.input = ItemStack {f.id("beef"), 2};
    s.fuel = ItemStack {f.id("coal"), 1};
    for (int i = 0; i < 100; ++i) {
        tickFurnace(s, f.items);
    }
    s.input = ItemStack {f.id("stone"), 1}; // non-smeltable swap
    tickFurnace(s, f.items);
    EXPECT_EQ(s.progressSubsteps, 0);
}

TEST(FurnaceTest, ProgressKeptWhenSameItemReplaced) {
    Fixture f;
    FurnaceState s;
    s.input = ItemStack {f.id("beef"), 2};
    s.fuel = ItemStack {f.id("coal"), 1};
    for (int i = 0; i < 100; ++i) {
        tickFurnace(s, f.items);
    }
    s.input = ItemStack {f.id("beef"), 1}; // same item, different stack
    tickFurnace(s, f.items);
    EXPECT_EQ(s.progressSubsteps, 101);
}

TEST(FurnaceTest, EmptyFurnaceNoop) {
    Fixture f;
    FurnaceState s;
    for (int i = 0; i < 1000; ++i) {
        tickFurnace(s, f.items);
    }
    EXPECT_TRUE(s.empty());
}

// -- layout -----------------------------------------------------------------------

TEST(FurnaceLayoutTest, Pinned1280x720) {
    const FurnaceLayout L = FurnaceLayout::compute(1280, 720);
    EXPECT_DOUBLE_EQ(L.scale, 1.0);
    expectRect(L.panel, 288, 246, 704, 228, "panel");
    expectRect(L.title, 300, 258, 500, 40, "title");
    EXPECT_EQ(L.slots.size(), 27u);
    // Hotbar row (slots 0..8) bottom, the two storage rows above.
    expectRect(L.slots[0], 300, 410, 52, 52, "slot 0");
    expectRect(L.slots[9], 300, 298, 52, 52, "slot 9");
    expectRect(L.slots[17], 748, 298, 52, 52, "slot 17");
    expectRect(L.slots[18], 300, 354, 52, 52, "slot 18");
    expectRect(L.slots[26], 748, 354, 52, 52, "slot 26");
    // The smelting section (input top-left, output top-right, fuel
    // bottom-left, the progress bar between input and output).
    expectRect(L.input, 812, 326, 52, 52, "input");
    expectRect(L.output, 928, 326, 52, 52, "output");
    expectRect(L.fuel, 812, 382, 52, 52, "fuel");
    expectRect(L.progress, 868, 346, 56, 12, "progress");
    // Hit tests.
    EXPECT_EQ(L.slotAt(310, 415), 0);
    EXPECT_EQ(L.slotAt(310, 303), 9);
    EXPECT_EQ(L.slotAt(810, 330), -1); // the input is not a 27-slot
    EXPECT_EQ(L.furnaceSlotAt(817, 331), 0);
    EXPECT_EQ(L.furnaceSlotAt(817, 387), 1);
    EXPECT_EQ(L.furnaceSlotAt(935, 331), 2);
    EXPECT_EQ(L.furnaceSlotAt(875, 350), -1); // the progress bar: no slot
    EXPECT_EQ(L.slotAt(10, 10), -1);
    EXPECT_EQ(L.furnaceSlotAt(10, 10), -1);
}

TEST(FurnaceLayoutTest, ScalesDownToFit) {
    const FurnaceLayout L = FurnaceLayout::compute(400, 300);
    EXPECT_DOUBLE_EQ(L.scale, 398.0 / 704.0);
    expectRect(L.panel, 1, 85, 398, 129, "panel");
    // Every slot center hits its own slot (the scaled rects stay
    // consistent with the hit tests).
    for (int i = 0; i < 27; ++i) {
        const UIRect& r = L.slots[static_cast<std::size_t>(i)];
        EXPECT_EQ(L.slotAt(r.x + r.w / 2, r.y + r.h / 2), i) << "slot " << i;
    }
    EXPECT_EQ(L.furnaceSlotAt(L.input.x + L.input.w / 2,
                              L.input.y + L.input.h / 2),
              0);
    EXPECT_EQ(L.furnaceSlotAt(L.fuel.x + L.fuel.w / 2,
                              L.fuel.y + L.fuel.h / 2),
              1);
    EXPECT_EQ(L.furnaceSlotAt(L.output.x + L.output.w / 2,
                              L.output.y + L.output.h / 2),
              2);
}

// -- the pick-and-place state machine ---------------------------------------------

TEST(FurnaceUiTest, LeftClickPickAndPlace) {
    Fixture f;
    Inventory inv;
    FurnaceState fz;
    FurnaceUI ui(inv, f.items);
    ui.bind(fz);
    inv.setSlot(0, ItemStack {f.id("beef"), 3});
    ui.leftClick(0, -1);
    EXPECT_EQ(ui.carry(), stk(f.id("beef"), 3));
    EXPECT_TRUE(inv.at(0).empty());
    ui.leftClick(-1, 0);
    EXPECT_EQ(fz.input, stk(f.id("beef"), 3));
    EXPECT_FALSE(ui.carrying());
}

TEST(FurnaceUiTest, LeftClickSwapsDifferentItems) {
    Fixture f;
    Inventory inv;
    FurnaceState fz;
    FurnaceUI ui(inv, f.items);
    ui.bind(fz);
    fz.input = ItemStack {f.id("coal"), 1};
    inv.setSlot(0, ItemStack {f.id("beef"), 2});
    ui.leftClick(0, -1); // pick the beef
    ui.leftClick(-1, 0); // swap with the coal
    EXPECT_EQ(fz.input, stk(f.id("beef"), 2));
    EXPECT_EQ(ui.carry(), stk(f.id("coal"), 1));
    ui.leftClick(1, -1); // put the coal into slot 1
    EXPECT_EQ(inv.at(1), stk(f.id("coal"), 1));
    EXPECT_FALSE(ui.carrying());
}

TEST(FurnaceUiTest, LeftClickMergesSameItem) {
    Fixture f;
    Inventory inv;
    FurnaceState fz;
    FurnaceUI ui(inv, f.items);
    ui.bind(fz);
    fz.input = ItemStack {f.id("cooked"), 10};
    inv.setSlot(0, ItemStack {f.id("cooked"), 60});
    ui.leftClick(0, -1); // pick the 60
    // Merge into the input (room 54): move 54, keep 60 - 54 = 6.
    ui.leftClick(-1, 0);
    EXPECT_EQ(fz.input, stk(f.id("cooked"), 64));
    EXPECT_EQ(ui.carry(), stk(f.id("cooked"), 6));
}

TEST(FurnaceUiTest, RightClickMovesOne) {
    Fixture f;
    Inventory inv;
    FurnaceState fz;
    FurnaceUI ui(inv, f.items);
    ui.bind(fz);
    inv.setSlot(0, ItemStack {f.id("beef"), 3});
    ui.rightClick(0, -1);
    EXPECT_EQ(ui.carry(), stk(f.id("beef"), 1));
    EXPECT_EQ(inv.at(0), stk(f.id("beef"), 2));
    ui.rightClick(-1, 1); // one into the fuel slot
    EXPECT_EQ(fz.fuel, stk(f.id("beef"), 1));
    EXPECT_FALSE(ui.carrying());
}

TEST(FurnaceUiTest, RightClickIntoDifferentItemNoop) {
    Fixture f;
    Inventory inv;
    FurnaceState fz;
    FurnaceUI ui(inv, f.items);
    ui.bind(fz);
    fz.input = ItemStack {f.id("coal"), 1};
    inv.setSlot(0, ItemStack {f.id("beef"), 1});
    ui.rightClick(0, -1); // pick the beef
    ui.rightClick(-1, 0); // the input holds a different item: no-op
    EXPECT_EQ(fz.input, stk(f.id("coal"), 1));
    EXPECT_EQ(ui.carry(), stk(f.id("beef"), 1));
}

TEST(FurnaceUiTest, InputChangeResetsProgress) {
    Fixture f;
    Inventory inv;
    FurnaceState fz;
    FurnaceUI ui(inv, f.items);
    ui.bind(fz);
    fz.input = ItemStack {f.id("beef"), 3};
    fz.progressSubsteps = 100;
    // Same item, partial removal: the progress is kept.
    ui.rightClick(-1, 0); // carry 1, input 2
    EXPECT_EQ(fz.input, stk(f.id("beef"), 2));
    EXPECT_EQ(fz.progressSubsteps, 100);
    ui.rightClick(0, -1); // the carried one back to the inventory
    EXPECT_FALSE(ui.carrying());
    // Emptying the input: the progress is lost.
    ui.leftClick(-1, 0); // pick up the remaining 2
    EXPECT_TRUE(fz.input.empty());
    EXPECT_EQ(fz.progressSubsteps, 0);
    EXPECT_EQ(ui.carry(), stk(f.id("beef"), 2));
}

TEST(FurnaceUiTest, InputSwapResetsProgress) {
    Fixture f;
    Inventory inv;
    FurnaceState fz;
    FurnaceUI ui(inv, f.items);
    ui.bind(fz);
    fz.input = ItemStack {f.id("beef"), 1};
    fz.progressSubsteps = 50;
    inv.setSlot(0, ItemStack {f.id("coal"), 1});
    ui.leftClick(0, -1);
    ui.leftClick(-1, 0);
    EXPECT_EQ(fz.input, stk(f.id("coal"), 1));
    EXPECT_EQ(ui.carry(), stk(f.id("beef"), 1));
    EXPECT_EQ(fz.progressSubsteps, 0);
}

TEST(FurnaceUiTest, ClickOutsideDropsCarry) {
    Fixture f;
    Inventory inv;
    FurnaceState fz;
    FurnaceUI ui(inv, f.items);
    ui.bind(fz);
    inv.setSlot(0, ItemStack {f.id("beef"), 3});
    ui.leftClick(0, -1);
    ui.leftClick(-1, -1);
    EXPECT_FALSE(ui.carrying());
    EXPECT_TRUE(inv.at(0).empty());
}
