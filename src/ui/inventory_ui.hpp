// Phase 15: the inventory screen — pure layout + click state machine.
// Phase 17: the crafting panels — the inventory screen gains a 2x2
// crafting grid + output slot (its top-left cells in the shared 3x3
// CraftingGrid model), and a second panel kind (CraftingTable, opened by
// right-clicking a crafting table in the world) exposes the full 3x3 grid.
//
// No SDL and no GL: the layout is a function of the window size, the
// panel kind and the shapeless recipe count, and the clicks mutate the
// Inventory + CraftingGrid through their public slot operations.
// Everything is deterministic, so the whole screen is unit-testable
// without a game.
//
// Screen layout (window 1280x720, panel centered; Inventory kind):
//   +------------------------------------------------------------+
//   |  Inventory        [g][g]   [out]     Crafting              |
//   |  [9 ][10]...[17]  [g][g]           [icon] planks x4        |
//   |  [18][19]...[26]                [icon] stick x4            |
//   |  [0 ][1 ]...[8 ]  (hotbar)                           |
//   +------------------------------------------------------------+
// Storage = slots 9..26 in two rows (top to bottom), the hotbar = slots
// 0..8 in the bottom row — the same 27 Inventory slots the game already
// owns (no separate storage). The CraftingTable kind is the same panel
// without the recipe column and with a 3x3 grid instead of 2x2.
//
// Click semantics (Minecraft's minimal subset):
//   left click  a slot or grid cell: pick up a whole stack / place or swap
//               the carried one (merging when the same item has room); a
//               click outside every slot drops the carried stack.
//               the output slot: craft — the grid is consumed and the
//               whole output goes to the cursor (or merges into a carried
//               stack of the same item when it fits).
//   right click a slot or grid cell: move ONE item between the carried
//               stack and the cell (empty target, or the same item with
//               room).
//               the output slot: craft ONE (when the output count is > 1;
//               a count-1 output behaves like the left click).
//
// The carried stack is owned by the panel's InventoryUI instance and
// rendered by the game as a floating icon under the cursor. The CraftingGrid
// is persistent state owned by the game: items left in the grid survive a
// close/reopen (see KNOWN_ISSUES.md — Minecraft returns them to the
// inventory on close).
#pragma once

#include <cstddef>
#include <vector>

#include "item/crafting_grid.hpp"
#include "item/inventory.hpp"
#include "item/item_registry.hpp"
#include "item/recipe.hpp"

namespace mc {

struct UIRect {
    int x = 0, y = 0, w = 0, h = 0;
    bool contains(int px, int py) const {
        return px >= x && px < x + w && py >= y && py < y + h;
    }
};

// The two crafting panels (Phase 17): the E inventory (2x2 grid + the
// shapeless recipe list) and the crafting table (3x3 grid, no list).
enum class PanelKind {
    Inventory,
    CraftingTable,
};

struct InventoryLayout {
    static constexpr int kSlot = 52;
    static constexpr int kGap = 4;
    static constexpr int kColumns = 9; // Inventory::kHotbarSize
    static constexpr int kTitleHeight = 30;
    static constexpr int kCraftWidth = 280;
    static constexpr int kCraftRowHeight = 46;
    static constexpr int kPanelPadding = 12;
    static constexpr int kSectionGap = 24;
    // Gaps around the crafting section (grid, output) — tighter than the
    // recipe-column gap because the section sits between the two.
    static constexpr int kCraftGap = 12;

    // 27 slot rects in slot-index order (0..26).
    std::vector<UIRect> slots;
    // Crafting grid cell rects: 4 (Inventory kind, the 2x2) or 9
    // (CraftingTable kind, the 3x3), row-major.
    std::vector<UIRect> gridCells;
    // Parallel to gridCells: the CraftingGrid slot each cell maps to
    // (row-major over the 3x3 model: the 2x2 uses slots 0, 1, 3, 4).
    std::vector<int> gridSlots;
    // The output slot (the matched recipe's result).
    UIRect output;
    // One row per SHAPELESS recipe (the CraftingTable kind has none).
    std::vector<UIRect> recipes;
    UIRect panel;
    // The "Inventory"/"Crafting Table" title and the "Crafting" header.
    UIRect title;
    UIRect craftHeader;

    // recipeCount: shapeless recipes only (Inventory kind); ignored for
    // the CraftingTable kind.
    static InventoryLayout compute(int windowWidth, int windowHeight,
                                   int recipeCount,
                                   PanelKind kind = PanelKind::Inventory);
    int slotAt(int px, int py) const;       // slot index or -1
    int gridCellAt(int px, int py) const;   // gridCells index or -1
    int recipeRowAt(int px, int py) const;  // row index or -1
    bool inOutput(int px, int py) const { return output.contains(px, py); }
};

// The pick-and-place + craft state machine over one inventory and one
// crafting grid (see the file header for the click semantics). The
// caller routes clicks through the layout (slot index, grid slot, output
// hit) so the layout stays a pure function.
class InventoryUI {
public:
    InventoryUI(Inventory& inventory, CraftingGrid& grid,
                const ItemRegistry& items, const RecipeRegistry& recipes)
        : m_inv(inventory), m_grid(grid), m_items(items),
          m_recipes(recipes) {}

    // slot: 0..26 or -1; gridSlot: 0..8 or -1; onOutput: the output slot
    // was hit. At most one of the three is "active" (the layout hit test
    // guarantees it). Returns true when the click crafted (consumed the
    // grid) — the game plays the craft sound on that cue.
    bool leftClick(int slot, int gridSlot, bool onOutput);
    bool rightClick(int slot, int gridSlot, bool onOutput);

    // The live match for the grid (shaped first, then shapeless — see
    // crafting_grid.hpp). Recomputed on every call (cheap: a handful of
    // recipes over 9 cells); the output slot renders from it.
    GridMatch match() const { return matchGridRecipe(m_grid, m_recipes); }

    const ItemStack& carry() const { return m_carry; }
    bool carrying() const { return !m_carry.empty(); }
    // The panel's grid (the game renders its cells + saves it).
    const CraftingGrid& grid() const { return m_grid; }
    CraftingGrid& grid() { return m_grid; }

private:
    // The shared pick-and-place / move-one rules, over one cell.
    void leftClickCell(ItemStack& cell);
    void rightClickCell(ItemStack& cell);
    // Output-slot click: crafts when the grid matches (see header).
    bool clickOutput(bool rightButton);

    Inventory& m_inv;
    CraftingGrid& m_grid;
    const ItemRegistry& m_items;
    const RecipeRegistry& m_recipes;
    ItemStack m_carry;
};

} // namespace mc
