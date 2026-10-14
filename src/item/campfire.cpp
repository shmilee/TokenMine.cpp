#include "item/campfire.hpp"

namespace mc {

void tickCampfireCook(CampfireCookState& c, const ItemRegistry& items) {
    if (c.slot.empty()) {
        // Nothing on the fire: the progress is lost (the invariant the
        // save cross-checks — an empty slot never carries progress).
        c.progressSubsteps = 0;
        return;
    }
    const Item& in = items.at(c.slot.item);
    if (in.smeltableTo.empty()) {
        // A non-smeltable slot (the smelted product) idles: no progress
        // accrues and any stale count is dropped.
        c.progressSubsteps = 0;
        return;
    }
    c.progressSubsteps++;
    if (c.progressSubsteps < smeltSubstepsFor(in)) {
        return;
    }
    // One unit smelted: the slot becomes the product, progress resets.
    c.slot = ItemStack {items.idForName(in.smeltableTo), 1};
    c.progressSubsteps = 0;
}

} // namespace mc
