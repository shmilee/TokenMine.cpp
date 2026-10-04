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
//
// Consumption (consumeGrid) is exact for both kinds:
//   - shaped: one item from each covered cell at the matched offset;
//   - shapeless: each ingredient in recipe order, taken from the
//     lowest-index occupied cell upward (the same rule the inventory
//     craft uses).
#pragma once

#include <cstddef>
#include <vector>

#include "item/item.hpp"
#include "item/recipe.hpp"

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
// (0, 0) for shapeless, which has no offset).
struct GridMatch {
    const Recipe* recipe = nullptr;
    int dx = 0;
    int dy = 0;

    bool matched() const { return recipe != nullptr; }
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
