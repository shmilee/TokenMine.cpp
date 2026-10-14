// Phase 71: the campfire — the single-slot cooking state and its pure
// per-substep tick. The game owns one CampfireCookState per campfire
// block (std::map<BlockPos, CampfireCookState>, position-ordered so the
// per-frame ticks are deterministic) and saves them in the world save
// (format 11, one entry per campfire block — the Phase 44 furnace
// per-block pattern).
//
// Timing: the campfire advances one fixed 1/60 substep per frame (the
// day-cycle class — world state that must stay byte-reproducible in
// headless runs), and its counter is an INTEGER substep count. Data
// times (seconds) convert to substeps with the furnace's smeltSubstepsFor
// (round(t * 60)) — the campfire cooks at the FURNACE pace (Minecraft:
// the same item takes the same time on a campfire), so no per-campfire
// timing data exists.
//
// Cooking rules (one substep, Minecraft semantics reduced to v1):
//   - the slot holds AT MOST ONE item (count 1): a placed unit, or its
//     smelted product once cooking completes.
//   - while the slot is non-empty and smeltable, one substep of progress
//     is added; at a full smelt time the slot becomes the product and
//     the progress resets to 0.
//   - the product is never smeltable in the shipped data, so a finished
//     slot idles (progress stays 0). The invariant "progressSubsteps > 0
//     implies the slot holds a smeltable item" is maintained by this tick
//     and by the place/take interaction (the save cross-checks it).
//   - there is NO fuel: the campfire is a permanent light-15 fire
//     (documented v1 deviation from the furnace's fuel slot).

#pragma once

#include <cstddef>

#include "item/item.hpp"
#include "item/item_registry.hpp"
#include "item/furnace.hpp"

namespace mc {

struct CampfireCookState {
    // The item on the fire (count 1 when non-empty; the product once
    // smelted).
    ItemStack slot;
    // 1/60 substeps of smelt progress on the current slot item (0 when
    // the slot is empty / non-smeltable — see the tick rules).
    int progressSubsteps = 0;

    bool empty() const {
        return slot.empty() && progressSubsteps == 0;
    }
    bool operator==(const CampfireCookState& o) const {
        return slot == o.slot && progressSubsteps == o.progressSubsteps;
    }
    bool operator!=(const CampfireCookState& o) const {
        return !(*this == o);
    }
};

// One fixed 1/60 substep of the campfire (see the file header). Pure
// over (state, items) — no game, no clock — so it is unit-testable and
// byte-deterministic.
void tickCampfireCook(CampfireCookState& c, const ItemRegistry& items);

} // namespace mc
