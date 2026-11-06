// V3-14c (piglin bartering): the barter init cross-check — the
// campfire / checkWoolDrops pattern: the decision lives here (pure over
// the registries, unit-tested headless), and the game's init calls it
// once after the other mob cross-checks (a thin adapter).
//
// The bartering feature is data-flag gated end to end (the core never
// switches on a species): a mob carrying MobProperties::barterInput is
// only legal when its DATA and the ASSETS are complete:
//   1. the barter input item exists (the held item's name is the data —
//      a missing item would make every trade a silent no-op);
//   2. every barter output item exists (the rolled drop must spawn —
//      the drops' unknown-item discipline);
//   3. every output's rolled count top (countMax) <= the item's max
//      stack (the drops' Phase 38 twin: the roll can never exceed the
//      range's top, so the stack is checked against it).
// The FIRST failing mob (registry order) is named, like every other
// init cross-check.
#pragma once

#include "core/error.hpp"
#include "entity/mob_registry.hpp"
#include "item/item_registry.hpp"

namespace mc {

// The init cross-check (pure over the registries — the campfire /
// checkWoolDrops pattern). The FIRST failing mob (registry order) is
// named, like every other init cross-check.
Result<void> checkBarter(const MobRegistry& mobs, const ItemRegistry& items);

} // namespace mc
