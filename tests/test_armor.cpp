// SURV-1 armor right-click use (item/armor.cpp — the pure function the
// unit tests own; Game::tryArmor is a thin adapter): equip onto an
// empty slot, swap onto an occupied one, unequip the first occupied
// slot (head -> chest -> legs -> feet) into the selected slot, and the
// fall-through for non-armor / empty hand with nothing worn. Includes
// the regression for the dangling-reference unequip (PITS 247): the
// unequipped piece MUST reach the hand (the pre-fix code cleared the
// armor slot before copying the reference into the inventory — the
// piece vanished).
#include <gtest/gtest.h>

#include <string>
#include <memory>

#include "block/block_registry.hpp"
#include "core/json.hpp"
#include "item/armor.hpp"
#include "item/inventory.hpp"
#include "item/item_registry.hpp"
#include "player/player.hpp"
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

struct Fixture {
    ItemRegistry items;
    BlockRegistry blocks;
    World world;
    BlockID stone;
    Inventory inventory;
    std::unique_ptr<Player> player;

    Fixture() {
        const struct {
            const char* def;
        } defs[] = {
            // The armor pieces (max_stack 1 — one piece per slot).
            {R"({"id":"iron_helmet","type":"material","max_stack":1,)"
             R"("armor_slot":"head","armor_points":2})"},
            {R"({"id":"diamond_helmet","type":"material","max_stack":1,)"
             R"("armor_slot":"head","armor_points":3})"},
            {R"({"id":"iron_chestplate","type":"material","max_stack":1,)"
             R"("armor_slot":"chest","armor_points":6})"},
            {R"({"id":"iron_boots","type":"material","max_stack":1,)"
             R"("armor_slot":"feet","armor_points":1})"},
            // The non-armor control.
            {R"({"id":"stick","type":"material","max_stack":64})"},
        };
        for (const auto& d : defs) {
            auto r = items.registerItem(parseJson(d.def));
            if (!r.ok()) {
                ADD_FAILURE() << d.def << ": " << r.error().message;
            }
        }
        auto r = blocks.registerBlock(parseJson(
            R"({"id":"stone","model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
        if (!r.ok()) {
            ADD_FAILURE() << r.error().message;
        }
        stone = blocks.idForName("stone");
        player = std::make_unique<Player>(world, blocks, Vec3 {8.5f, 2.0f, 8.5f});
    }

    ItemID id(const char* name) const { return items.idForName(name); }
    void hold(ItemID item, int count, std::size_t slot = 0) {
        inventory.setSlot(slot, ItemStack {item, count});
    }
    ArmorUseAction use(std::size_t selectedSlot) {
        return armorRightClickUse(*player, inventory, items, selectedSlot);
    }
};

} // namespace

// -- equip -----------------------------------------------------------------

TEST(ArmorUseTest, HeldPieceEquipsOntoEmptySlot) {
    Fixture f;
    f.hold(f.id("iron_helmet"), 1);
    const ArmorUseAction action =
        f.use(0);
    EXPECT_EQ(action, ArmorUseAction::Equip);
    EXPECT_EQ(f.player->armorSlot(0).item, f.id("iron_helmet"));
    EXPECT_EQ(f.player->armorSlot(0).count, 1);
    EXPECT_EQ(f.player->armorPoints(), 2);
    EXPECT_TRUE(f.inventory.at(0).empty());
}

TEST(ArmorUseTest, PieceGoesToItsOwnSlotOnly) {
    Fixture f;
    // Feet is free, the head is occupied: the held boots must go to the
    // feet and must NOT clobber the equipped helmet.
    f.player->setArmorSlot(0, ItemStack {f.id("iron_helmet"), 1}, 2);
    f.hold(f.id("iron_boots"), 1);
    const ArmorUseAction action = f.use(0);
    EXPECT_EQ(action, ArmorUseAction::Equip);
    EXPECT_EQ(f.player->armorSlot(3).item, f.id("iron_boots"));
    EXPECT_EQ(f.player->armorSlot(0).item, f.id("iron_helmet"));
    EXPECT_TRUE(f.inventory.at(0).empty());
    EXPECT_EQ(f.player->armorPoints(), 3); // 2 + 1
}

// -- swap ------------------------------------------------------------------

TEST(ArmorUseTest, HeldPieceSwapsWithTheEquippedOne) {
    Fixture f;
    f.player->setArmorSlot(0, ItemStack {f.id("diamond_helmet"), 1}, 3);
    f.hold(f.id("iron_helmet"), 1);
    const ArmorUseAction action = f.use(0);
    EXPECT_EQ(action, ArmorUseAction::Swap);
    EXPECT_EQ(f.player->armorSlot(0).item, f.id("iron_helmet"));
    EXPECT_EQ(f.player->armorPoints(), 2);
    // The diamond helmet came to the hand (count 1 — armor stacks are 1).
    EXPECT_EQ(f.inventory.at(0).item, f.id("diamond_helmet"));
    EXPECT_EQ(f.inventory.at(0).count, 1);
}

// -- unequip ---------------------------------------------------------------

TEST(ArmorUseTest, UnequipSendsThePieceToTheHand) {
    // The regression (PITS 247): the pre-fix code held the unequipped
    // piece by reference across the armor-slot clear, so the hand
    // received an EMPTY stack and the piece vanished.
    Fixture f;
    f.player->setArmorSlot(0, ItemStack {f.id("iron_helmet"), 1}, 2);
    const ArmorUseAction action = f.use(0);
    EXPECT_EQ(action, ArmorUseAction::Unequip);
    EXPECT_TRUE(f.player->armorSlot(0).empty());
    EXPECT_EQ(f.player->armorPoints(), 0);
    EXPECT_EQ(f.inventory.at(0).item, f.id("iron_helmet"));
    EXPECT_EQ(f.inventory.at(0).count, 1);
}

TEST(ArmorUseTest, UnequipTakesTheFirstOccupiedSlotInOrder) {
    Fixture f;
    // Chest + feet occupied, head/legs free: the FIRST occupied slot in
    // the fixed order (head -> chest -> legs -> feet) is the chest.
    f.player->setArmorSlot(1, ItemStack {f.id("iron_chestplate"), 1}, 6);
    f.player->setArmorSlot(3, ItemStack {f.id("iron_boots"), 1}, 1);
    const ArmorUseAction action = f.use(0);
    EXPECT_EQ(action, ArmorUseAction::Unequip);
    EXPECT_TRUE(f.player->armorSlot(1).empty());
    EXPECT_EQ(f.inventory.at(0).item, f.id("iron_chestplate"));
    // The feet stay equipped.
    EXPECT_EQ(f.player->armorSlot(3).item, f.id("iron_boots"));
    EXPECT_EQ(f.player->armorPoints(), 1);
}

TEST(ArmorUseTest, UnequipSkipsEmptySlotsDownToFeet) {
    Fixture f;
    f.player->setArmorSlot(3, ItemStack {f.id("iron_boots"), 1}, 1);
    const ArmorUseAction action = f.use(0);
    EXPECT_EQ(action, ArmorUseAction::Unequip);
    EXPECT_TRUE(f.player->armorSlot(3).empty());
    EXPECT_EQ(f.inventory.at(0).item, f.id("iron_boots"));
    EXPECT_EQ(f.player->armorPoints(), 0);
}

TEST(ArmorUseTest, UnequipWritesToTheSelectedSlot) {
    Fixture f;
    f.player->setArmorSlot(0, ItemStack {f.id("iron_helmet"), 1}, 2);
    // The selected slot 4 is empty: the piece lands there, not in 0.
    const ArmorUseAction action = f.use(4);
    EXPECT_EQ(action, ArmorUseAction::Unequip);
    EXPECT_EQ(f.inventory.at(4).item, f.id("iron_helmet"));
    EXPECT_TRUE(f.inventory.at(0).empty());
}

// -- fall-through ----------------------------------------------------------

TEST(ArmorUseTest, NonArmorInHandFallsThrough) {
    Fixture f;
    f.player->setArmorSlot(0, ItemStack {f.id("iron_helmet"), 1}, 2);
    f.hold(f.id("stick"), 1);
    const ArmorUseAction action = f.use(0);
    EXPECT_EQ(action, ArmorUseAction::None);
    // Nothing moved: the stick is held, the helmet is worn.
    EXPECT_EQ(f.inventory.at(0).item, f.id("stick"));
    EXPECT_EQ(f.player->armorSlot(0).item, f.id("iron_helmet"));
}

TEST(ArmorUseTest, EmptyHandWithNothingWornFallsThrough) {
    Fixture f;
    const ArmorUseAction action = f.use(0);
    EXPECT_EQ(action, ArmorUseAction::None);
}
