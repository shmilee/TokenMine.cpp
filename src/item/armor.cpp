#include "item/armor.hpp"

#include "item/inventory.hpp"
#include "item/item_registry.hpp"

namespace mc {

ArmorUseAction armorRightClickUse(Player& player, Inventory& inventory,
                                  const ItemRegistry& items,
                                  std::size_t selectedSlot) {
    const ItemStack held = inventory.at(selectedSlot);
    if (!held.empty()) {
        const Item& item = items.at(held.item);
        if (item.armorSlot.empty()) {
            return ArmorUseAction::None; // not armor: the next channel
        }
        // The slot >= 0 is guaranteed (the parse's vocabulary check).
        const int slot = armorSlotIndex(item.armorSlot);
        const ItemStack& worn = player.armorSlot(slot);
        if (worn.empty()) {
            // Equip: the piece goes on (its count is 1 by the armor
            // items' max_stack — the inventory enforces the data cap),
            // the held slot empties. SURV-3: the piece's accumulated
            // damage + its data durability move with it (the stack copy
            // carries the damage, the setter takes the max).
            player.setArmorSlot(slot, held, item.armorPoints,
                                item.durability);
            inventory.setSlot(selectedSlot, ItemStack {});
            return ArmorUseAction::Equip;
        }
        // Swap: the held piece goes on, the equipped piece comes to the
        // hand. The equipped stack is COPIED before the slot write — a
        // reference would dangle across the clear (PITS 247).
        const ItemStack wornCopy = worn;
        player.setArmorSlot(slot, held, item.armorPoints, item.durability);
        inventory.setSlot(selectedSlot, wornCopy);
        return ArmorUseAction::Swap;
    }
    // Empty hand: unequip the FIRST occupied slot (the fixed order
    // head -> chest -> legs -> feet). The piece comes to the selected
    // slot — so the NEXT right-click re-equips it (the hand holds a
    // piece again). Stripping a whole set means moving to a free
    // hotbar slot between clicks, not clicking repeatedly.
    for (int slot = 0; slot < kArmorSlotCount; ++slot) {
        const ItemStack& worn = player.armorSlot(slot);
        if (worn.empty()) {
            continue;
        }
        const ItemStack wornCopy = worn; // copy before the clear
        player.setArmorSlot(slot, ItemStack {}, 0);
        inventory.setSlot(selectedSlot, wornCopy);
        return ArmorUseAction::Unequip;
    }
    return ArmorUseAction::None; // nothing to unequip: fall through
}

} // namespace mc
