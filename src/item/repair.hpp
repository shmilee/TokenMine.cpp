// UX-5: the material repair system's pure helpers — the campfire /
// durability pattern: the decision + every repair computation lives
// here, unit-tested headless; the InventoryUI's output click and the
// game's init are thin adapters that call in.
//
// The model (Minecraft's anvil, in crafting-grid form — the documented
// simplification: this project's repair runs on ANY crafting grid,
// including the E panel's 2x2, and carries no XP cost): a grid holding
// a DAMAGED repairable item (data 'durability' > 0 + a non-empty
// 'repair_with' material name + accumulated stack damage > 0) and one
// or more units of the named material matches a REPAIR instead of a
// recipe (the recipe match always wins first — the GridMatch fallback,
// see crafting_grid.hpp). One material unit restores kRepairFraction
// of the item's max durability (Minecraft: 1 material = +25%); the
// accumulated damage never goes below 0 (the final repair of a tool at
// a quarter-max or less of damage lands exactly on 0 = full durability).
//
// The repair is IN PLACE on the grid: the tool's cell becomes the
// repaired stack (same item, same count — the repair never changes the
// number of units — reduced damage) and the material's cell loses one
// unit. The UI's output click then takes the repaired tool into the
// carry (the anvil's take-the-result), leaving the cell empty.
//
// No core code switches on item identity: the repairable set and every
// material are pure data ('durability' + 'repair_with'), read through
// the ItemRegistry.
#pragma once

#include <optional>

#include "core/error.hpp"

namespace mc {

class CraftingGrid;
class ItemRegistry;

// UX-5: the fraction of an item's max durability ONE repair material
// restores (Minecraft: 1 material = +25%). Named constant, no config
// (the repair rule is core behavior, the materials are content data).
inline constexpr float kRepairFraction = 0.25f;

// One successful repair match over a crafting grid: the cell holding
// the damaged repairable item, the cell holding its material (one unit
// is spent from it), and the accumulated damage the item keeps after
// the repair (damage - durability x kRepairFraction, clamped at 0).
struct RepairMatch {
    int itemSlot;
    int materialSlot;
    float outputDamage;
};

// The grid's repair match: the FIRST (cell-index order) cell holding a
// damaged repairable item, paired with the FIRST (cell-index order)
// OTHER cell holding that item's named material (any positive count).
// nullopt when no cell holds a damaged repairable item or no other cell
// holds its material. Deterministic over the cell index order (the
// matchGridRecipe registry-order twin).
std::optional<RepairMatch> matchRepair(const CraftingGrid& grid,
                                       const ItemRegistry& items);

// Applies `m` to `grid` in place: the item's cell becomes the repaired
// stack (same item, its CURRENT count — the repair never changes the
// unit count; every max-stack-1 tool is count 1, and the stackable
// repairables (the flint and steel's 64) keep their surplus units —
// reduced damage) and the material's cell loses one unit.
void applyRepair(CraftingGrid& grid, const RepairMatch& m,
                 const ItemRegistry& items);

// The init cross-check (pure over the registry — the CRAFT-3
// checkSoundMaterials pattern; the game's init calls it once after the
// other item cross-checks): every item's repair material must EXIST
// (the ammo / grows_to discipline — a typo fails init loudly, the
// first bad item in registry order is named) and only an item WITH a
// durability may be repairable (the pairing discipline — a repairable
// without durability is a no-op typo). An empty registry section (no
// item carries 'repair_with') passes.
Result<void> checkRepairMaterials(const ItemRegistry& items);

} // namespace mc
