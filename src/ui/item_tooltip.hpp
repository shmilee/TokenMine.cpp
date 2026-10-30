// PRE-12 (tooltip): the item hover tooltip lines, as a pure function.
//
// The hovered-slot tooltip (hotbar + every panel) shows the item name —
// and now, per the Phase-15 KNOWN_ISSUES residue, the stack count and the
// accumulated durability readout. The formatting is owned by the unit
// tests (the campfire paradigm); Game::drawUI consumes it.
#pragma once

#include <string>

#include "item/item.hpp"

namespace mc {

struct ItemTooltip {
    // "name" (a single item) or "name xN" (N > 1) — the " xN" form matches
    // the recipe-row output label (the one existing count suffix).
    std::string title;
    // "3.5/59" when the item has a durability (the same d/max readout as
    // the F3 held line — full durability shows "0/max"), "" otherwise.
    std::string durability;
};

ItemTooltip itemTooltip(const Item& item, const ItemStack& stack);

} // namespace mc
