// Phase 40: the furnace — the smelting state and its pure per-substep
// tick. Phase 44: PER-BLOCK states — the game owns one FurnaceState per
// furnace block (std::map<BlockPos, FurnaceState>, position-ordered so
// the per-frame ticks are deterministic) and saves them in the world save
// (format 7, one entry per furnace block). The Phase 40 single-container
// v1 rule (ONE state shared by every block) is gone; format-6 saves
// migrate it (exactly one furnace block: assigned there; otherwise:
// discarded — see save.hpp).
//
// Timing: the furnace advances one fixed 1/60 substep per frame (the
// day-cycle class — world state that must stay byte-reproducible in
// headless runs), and its counters are INTEGER substep countdowns. Data
// times (seconds) convert to substeps with round(t * 60) — exact for the
// shipped data (10.0 / 80.0 / 7.5 / 5.0).
//
// Tick rules (one substep, Minecraft semantics reduced to v1):
//   - fuel: when the fire is out and the fuel slot holds fuel, one fuel
//     item is consumed and its full burn time is added (leftover burn
//     time is kept). A non-fuel item in the slot is never consumed.
//     While the fire burns, one substep of burn time is spent.
//   - smelting: only while burning, the input must be smeltable, and the
//     output must be empty or hold the same product with room. One
//     substep of progress is added; at a full smelt time the input loses
//     one unit and the output gains one of the product.
//   - progress is LOST when the input item changes (including it
//     becoming empty or non-smeltable); a full output keeps the progress
//     (smelting just pauses — the fire still burns, as in Minecraft).

#pragma once

#include <cstddef>

#include "item/item.hpp"
#include "item/item_registry.hpp"

namespace mc {

struct FurnaceState {
    ItemStack input;
    ItemStack fuel;
    ItemStack output;
    // 1/60 substeps of smelt progress on the current input item (0 when
    // the input is empty / non-smeltable — see the tick rules).
    int progressSubsteps = 0;
    // 1/60 substeps of burn time remaining.
    int fuelSubsteps = 0;
    // The item the progress belongs to (live state — NOT saved: the
    // invariant "progressSubsteps > 0 implies progressFor == input.item
    // and the input is smeltable" is maintained by the tick and by the
    // panel UI (a click that changes the input resets the progress), so
    // on load progressFor is derived: input.item when progress > 0,
    // invalid() otherwise. See FurnaceState::deriveProgressFor.
    ItemID progressFor = ItemID::invalid();

    // After a save load: restore progressFor from the (validated) state.
    void deriveProgressFor() {
        progressFor = progressSubsteps > 0 ? input.item : ItemID::invalid();
    }

    bool empty() const {
        return input.empty() && fuel.empty() && output.empty() &&
               progressSubsteps == 0 && fuelSubsteps == 0;
    }
    bool operator==(const FurnaceState& o) const {
        return input == o.input && fuel == o.fuel && output == o.output &&
               progressSubsteps == o.progressSubsteps &&
               fuelSubsteps == o.fuelSubsteps;
    }
    bool operator!=(const FurnaceState& o) const { return !(*this == o); }
};

// Data times in substeps (0 when the item is not smeltable / not fuel).
int smeltSubstepsFor(const Item& item);
int fuelSubstepsFor(const Item& item);

// One fixed 1/60 substep of the furnace (see the file header). Pure over
// (state, items) — no game, no clock — so it is unit-testable and
// byte-deterministic.
void tickFurnace(FurnaceState& f, const ItemRegistry& items);

} // namespace mc
