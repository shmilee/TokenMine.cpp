#include "item/campfire.hpp"

namespace mc {

namespace {

// One slot of the campfire, one fixed 1/60 substep (see the header for
// the rules).
void tickSlot(ItemStack& slot, int& progress, const ItemRegistry& items) {
    if (slot.empty()) {
        // Nothing on the fire: the progress is lost (the invariant the
        // save cross-checks — an empty slot never carries progress).
        progress = 0;
        return;
    }
    const Item& in = items.at(slot.item);
    if (in.smeltableTo.empty()) {
        // A non-smeltable slot (the smelted product) idles: no progress
        // accrues and any stale count is dropped.
        progress = 0;
        return;
    }
    ++progress;
    if (progress < smeltSubstepsFor(in)) {
        return;
    }
    // One unit smelted: the slot becomes the product, progress resets.
    slot = ItemStack {items.idForName(in.smeltableTo), 1};
    progress = 0;
}

} // namespace

void tickCampfireCook(CampfireCookState& c, const ItemRegistry& items) {
    // Phase 75: the four slots cook INDEPENDENTLY (each its own clock —
    // Minecraft's four cooking spots).
    for (std::size_t i = 0; i < kCampfireSlotCount; ++i) {
        tickSlot(c.slots[i], c.progressSubsteps[i], items);
    }
}

} // namespace mc
