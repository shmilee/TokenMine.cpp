// Crafting grid (Phase 17): the 3x3 cell model shared by the crafting
// table GUI (all 9 cells exposed) and the inventory panel's 2x2 grid (the
// top-left 4 cells of the same model; the other 5 are never touched by
// that UI and therefore never hold items).
//
// Pure data + pure matching logic: no SDL, no GL, no ItemRegistry beyond
// what the callers pass in — deterministic and unit-testable.
//
// Matching (matchGridRecipe, the single entry point the UI uses):
//   - SHAPED recipes are tried first (in registry order): the pattern
//     matches when it fits at some offset (dx, dy) of the 3x3 grid, every
//     pattern cell holds the keyed item (any positive count), and every
//     grid cell the pattern does not cover is EMPTY. Extra counts in the
//     covered cells are allowed (they survive the craft).
//   - SHAPELESS recipes are tried second (in registry order): every
//     occupied cell must hold an ingredient item, and the total of each
//     required ingredient over the cells must reach its count.
//   - The first match wins (shaped before shapeless, registry order
//     within a kind — the registry's file-sorted order is deterministic).
//   - UX-5 REPAIR fallback (matchRepair, item/repair.hpp): when NO recipe
//     matches, the grid is a MATERIAL REPAIR if it holds a damaged
//     repairable item (data durability + repair_with, stack damage > 0)
//     plus a cell holding its named material. The UI's match() computes
//     the fallback (GridMatch::repair) — matchGridRecipe itself is the
//     pure recipe matcher and never sees it (the recipe match always
//     wins: a grid that crafts never repairs).
//
// Consumption (consumeGrid) is exact for both kinds:
//   - shaped: one item from each covered cell at the matched offset;
//   - shapeless: each ingredient in recipe order, taken from the
//     lowest-index occupied cell upward (the same rule the inventory
//     craft uses).
#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "item/item.hpp"
#include "item/recipe.hpp"
#include "item/repair.hpp"

namespace mc {

// The 9-cell crafting grid (row-major: cell (row, col) = slot row*3+col).
class CraftingGrid {
public:
    static constexpr int kSize = 9;
    static constexpr int kColumns = 3;

    const ItemStack& at(int slot) const; // asserts in range
    // Mutable view (the UI's pick-and-place + the grid consumption write
    // through it).
    ItemStack& at(int slot);
    bool isEmpty(int slot) const;
    void setSlot(int slot, const ItemStack& stack);
    // Removes up to `n` (clamped to what is there); the cell empties at
    // zero. Returns the number actually removed.
    int take(int slot, int n);
    bool empty() const; // all cells empty
    void clear();

private:
    ItemStack m_slots[kSize] = {};
};

// One successful grid match: the recipe plus the shaped offset (dx, dy;
// (0, 0) for shapeless, which has no offset) — or, when no recipe
// matches, the UX-5 material-repair fallback (the damaged repairable
// tool + its material, see item/repair.hpp). matched() is true for
// either: the UI's output slot renders and the output click acts on
// whichever is set (the recipe always wins — match() fills `repair`
// only when `recipe` is null).
struct GridMatch {
    const Recipe* recipe = nullptr;
    int dx = 0;
    int dy = 0;
    // UX-5: set (and only set) when no recipe matched and the grid is a
    // material repair (see matchRepair). nullopt = no repair.
    std::optional<RepairMatch> repair;

    bool matched() const { return recipe != nullptr || repair.has_value(); }
};

// Does `grid` satisfy the shaped `recipe` at some offset? On success *dx/
// *dy receive the first (row-major) matching offset.
bool matchesShaped(const CraftingGrid& grid, const Recipe& recipe, int* dx,
                   int* dy);

// Does `grid` satisfy the shapeless `recipe` (bag semantics over cells)?
bool matchesShapeless(const CraftingGrid& grid, const Recipe& recipe);

// The first match over the whole registry (shaped first, then shapeless,
// each in registry order); unmatched = {nullptr, 0, 0}.
GridMatch matchGridRecipe(const CraftingGrid& grid,
                          const RecipeRegistry& recipes);

// Consumes `match` from `grid` (see the file header for the exact rules).
void consumeGrid(CraftingGrid& grid, const GridMatch& match);

} // namespace mc
