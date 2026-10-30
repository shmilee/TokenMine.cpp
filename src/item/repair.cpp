#include "item/repair.hpp"

#include <algorithm>

#include "item/crafting_grid.hpp"
#include "item/item_registry.hpp"

namespace mc {

std::optional<RepairMatch> matchRepair(const CraftingGrid& grid,
                                       const ItemRegistry& items) {
    // The first (cell-index order) damaged repairable item: the cell
    // holds a stack with accumulated damage > 0 of an item whose data
    // marks it repairable (a durability + a non-empty repair material).
    int itemSlot = -1;
    for (int i = 0; i < CraftingGrid::kSize; ++i) {
        const ItemStack& s = grid.at(i);
        if (s.empty() || s.damage <= 0.0f) {
            continue;
        }
        const Item& it = items.at(s.item);
        if (it.durability > 0.0f && !it.repairWith.empty()) {
            itemSlot = i;
            break;
        }
    }
    if (itemSlot < 0) {
        return std::nullopt; // no damaged repairable item in the grid
    }
    // The first (cell-index order) OTHER cell holding the named
    // material (any positive count — one unit is spent). The init
    // cross-check guarantees the name exists (idForName invalid = no
    // cell can match it, the defensive no-op).
    const Item& tool = items.at(grid.at(itemSlot).item);
    const ItemID material = items.idForName(tool.repairWith);
    for (int i = 0; i < CraftingGrid::kSize; ++i) {
        if (i == itemSlot) {
            continue;
        }
        const ItemStack& s = grid.at(i);
        if (!s.empty() && s.item == material) {
            // The anvil output rule (the float operation order IS the
            // formula): one material restores kRepairFraction of the
            // max durability, the accumulated damage never below 0.
            const float out =
                std::max(0.0f, grid.at(itemSlot).damage -
                                   tool.durability * kRepairFraction);
            return RepairMatch {itemSlot, i, out};
        }
    }
    return std::nullopt; // damaged repairable item, but no material
}

void applyRepair(CraftingGrid& grid, const RepairMatch& m,
                 const ItemRegistry& items) {
    (void)items; // the match carries the whole computation (see header)
    const ItemStack& tool = grid.at(m.itemSlot);
    // The tool is repaired IN PLACE (it is the output): same item, its
    // current count (the repair never changes the unit count), the
    // match's reduced damage. The result stays below the max — the
    // match requires damage > 0 (a live stack: damage < durability)
    // and subtracts a positive quarter-max or clamps at 0.
    grid.setSlot(m.itemSlot, ItemStack {tool.item, tool.count,
                                        m.outputDamage});
    grid.take(m.materialSlot, 1); // one material unit spent
}

Result<void> checkRepairMaterials(const ItemRegistry& items) {
    // Registry order (the file-sorted load order) — the first bad item
    // is named (the CRAFT-3 naming discipline).
    for (const ItemID id : items.allIds()) {
        const Item& item = items.at(id);
        if (item.repairWith.empty()) {
            continue; // not repairable (every pre-UX-5 item)
        }
        // The pairing discipline: repair is only for durable items — a
        // 'repair_with' without a 'durability' is a no-op typo.
        if (item.durability <= 0.0f) {
            return MC_FAIL("item '" + item.name + "' has 'repair_with' "
                           "but no 'durability' (repair is only for "
                           "durable items)");
        }
        // The material must exist (the ammo / grows_to discipline — a
        // typo fails init loudly instead of a dead field).
        if (items.findByName(item.repairWith) == nullptr) {
            return MC_FAIL("item '" + item.name + "' references unknown "
                           "repair material '" + item.repairWith + "'");
        }
    }
    return {};
}

} // namespace mc
