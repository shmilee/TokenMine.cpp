#include "entity/barter_check.hpp"

namespace mc {

Result<void> checkBarter(const MobRegistry& mobs, const ItemRegistry& items) {
    // The bartering mob's data must be complete against the item
    // registry (the checkWoolDrops / drops pattern): the input exists,
    // every output exists, and each output's rolled top fits its stack.
    // The first failing mob (registry order) is named.
    for (const MobID id : mobs.allIds()) {
        const Mob& mob = mobs.at(id);
        const MobProperties& p = mob.properties;
        if (p.barterInput.empty()) {
            continue; // not a bartering mob (the pre-V3-14c data)
        }
        const ItemID input = items.idForName(p.barterInput);
        if (!input.valid()) {
            return MC_FAIL("mob '" + mob.name + "' barter_input is an "
                           "unknown item '" + p.barterInput + "'");
        }
        for (const BarterOutput& out : p.barterOutputs) {
            const ItemID oid = items.idForName(out.item);
            if (!oid.valid()) {
                return MC_FAIL("mob '" + mob.name + "' barter output is an "
                               "unknown item '" + out.item + "'");
            }
            // The ROLLED count can never exceed countMax (the drops'
            // Phase 38 twin), so the max stack is checked against the
            // range's top.
            if (out.countMax > items.at(oid).maxStack) {
                return MC_FAIL("mob '" + mob.name + "' barters up to " +
                               std::to_string(out.countMax) + " of '" +
                               out.item + "' but its max stack is " +
                               std::to_string(items.at(oid).maxStack));
            }
        }
    }
    return {};
}

} // namespace mc
