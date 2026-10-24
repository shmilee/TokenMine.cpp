// SURV-1: the armor right-click use as a pure function over the
// player's equipped slots, the inventory and the item registry (the
// campfire pattern — the decision + every state write live here so the
// unit tests own them; Game::tryArmor is the thin adapter that plays
// the sound). A held piece equips onto its data slot (an occupied slot
// SWAPS the piece with the held one — Minecraft); an empty hand
// unequips the FIRST occupied slot in the fixed order head -> chest ->
// legs -> feet.
#pragma once

#include <cstddef>

#include "item/item.hpp"
#include "player/player.hpp"

namespace mc {

class ItemRegistry;
class Inventory;

enum class ArmorUseAction {
    None,    // the click is not consumed (fall through to the next channel)
    Equip,   // the held piece went onto an empty slot
    Swap,    // the held piece replaced the equipped one (it came to hand)
    Unequip, // the first equipped slot came to the selected slot
};

// The right-click armor use on the given selected slot. Returns None
// when the channel does not consume the click.
ArmorUseAction armorRightClickUse(Player& player, Inventory& inventory,
                                  const ItemRegistry& items,
                                  std::size_t selectedSlot);

} // namespace mc
