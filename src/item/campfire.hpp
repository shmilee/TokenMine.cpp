// Phase 71: the campfire — the cooking state and its pure per-substep
// tick. The game owns one CampfireCookState per campfire FAMILY block
// (lit or extinguished — Phase 75: water puts a lit campfire out into
// the `campfire_extinguished` block, which keeps its items), in a
// std::map<BlockPos, CampfireCookState> (position-ordered so the
// per-frame ticks are deterministic) and saves them in the world save
// (format 13 since Phase 75 — four slots per entry; format 11-12 the
// single-slot shape — the Phase 44 furnace per-block pattern).
//
// Timing: the campfire advances one fixed 1/60 substep per frame (the
// day-cycle class — world state that must stay byte-reproducible in
// headless runs), and its counters are INTEGER substep counts. Data
// times (seconds) convert to substeps with the furnace's smeltSubstepsFor
// (round(t * 60)) — the campfire cooks at the FURNACE pace (Minecraft:
// the same item takes the same time on a campfire), so no per-campfire
// timing data exists.
//
// Cooking rules (one substep, Phase 75: four INDEPENDENT slots —
// Minecraft's campfire has four cooking spots):
//   - a slot holds AT MOST ONE item (count 1): a placed unit, or its
//     smelted product once cooking completes.
//   - while a slot is non-empty and smeltable, one substep of progress
//     is added to THAT slot; at a full smelt time the slot becomes the
//     product and its progress resets to 0 (the other slots are
//     untouched — each slot cooks on its own clock).
//   - the product is never smeltable in the shipped data, so a finished
//     slot idles (progress stays 0). The per-slot invariant "progress > 0
//     implies the slot holds a smeltable item of count 1" is maintained
//     by this tick and by the place/take interaction (the save
//     cross-checks it per slot).
//   - only LIT campfires cook (the game gates the tick on the block id):
//     an extinguished campfire holds its items, no progress accrues.
//   - there is NO fuel: the campfire is a permanent light-15 fire while
//     lit (Minecraft: campfires self-ignite; the furnace's fuel slot is
//     a documented deviation of the furnace, not the campfire).

#pragma once

#include <array>
#include <cstddef>

#include "item/item.hpp"
#include "item/item_registry.hpp"
#include "item/furnace.hpp"

namespace mc {
// The campfire's cooking spots (Minecraft: a campfire has four).
inline constexpr std::size_t kCampfireSlotCount = 4;

struct CampfireCookState {
    // The items on the fire (count 1 when non-empty; the product once
    // smelted). Slot order is deterministic (the save and the first-
    // empty / first-occupied interaction rules index it).
    std::array<ItemStack, kCampfireSlotCount> slots {};
    // 1/60 substeps of smelt progress per slot (0 when the slot is empty
    // / non-smeltable — see the tick rules).
    std::array<int, kCampfireSlotCount> progressSubsteps {};

    bool empty() const {
        for (std::size_t i = 0; i < kCampfireSlotCount; ++i) {
            if (!slots[i].empty() || progressSubsteps[i] != 0) {
                return false;
            }
        }
        return true;
    }
    bool operator==(const CampfireCookState& o) const {
        return slots == o.slots && progressSubsteps == o.progressSubsteps;
    }
    bool operator!=(const CampfireCookState& o) const {
        return !(*this == o);
    }
};

// One fixed 1/60 substep of the campfire (see the file header). Pure
// over (state, items) — no game, no clock — so it is unit-testable and
// byte-deterministic. Runs over ALL four slots (the game calls this only
// for LIT campfires — the extinguished block pauses its clocks).
void tickCampfireCook(CampfireCookState& c, const ItemRegistry& items);

} // namespace mc
