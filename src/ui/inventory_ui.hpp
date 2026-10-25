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
// close/reopen (see docs/KNOWN_ISSUES.md — Minecraft returns them to the
// inventory on close).
#pragma once

#include <cstddef>
#include <vector>

#include "item/crafting_grid.hpp"
#include "item/furnace.hpp"
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
    // Native (unscaled) design, in UI px. Sizing is font-aware: the 16x16
    // glyph (Phase 35) at scale 1 is 16 px tall, at 1.5 (the title) 24 px.
    static constexpr int kSlot = 52;
    static constexpr int kGap = 4;
    static constexpr int kColumns = 9; // Inventory::kHotbarSize
    static constexpr int kTitleHeight = 40;
    // Title text is drawn at 1.5x the body font scale (24 px native vs the
    // body's 16 px). 2.0x was too large at ui.scale 2 — the 40.8 physical
    // px title reached 62% of the 66 px slot height (user feedback).
    static constexpr double kTitleFontScale = 1.5;
    static constexpr int kCraftWidth = 280;
    static constexpr int kCraftRowHeight = 52;
    static constexpr int kPanelPadding = 12;
    // Gaps around the crafting section (grid, output) — tighter than the
    // recipe-column gap because the section sits between the two.
    static constexpr int kCraftGap = 12;

    // Fit factor (Phase 35): 1.0 when the window is at least the native
    // panel size, below that the whole panel — and the text and icons the
    // renderer draws from this factor — scales down proportionally, so
    // the panel always fits with >=1 px of margin on each side.
    double scale = 1.0;

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

// Phase 40: the furnace panel — the smelting GUI, opened by right-clicking
// a furnace in the world. Same panel chrome as the inventory screen: the
// 27 inventory slots + the smelting section in Minecraft's arrangement
// (input top-left, output top-right with the progress bar between, fuel
// bottom-left). No crafting of any kind happens here — the output fills
// by itself (the furnace tick) and is taken with an ordinary slot click.
struct FurnaceLayout {
    // The progress bar (Minecraft's arrow): native width; the fill is
    // progressSubsteps / smeltSubsteps of the width.
    static constexpr int kProgressWidth = 56;
    static constexpr int kProgressHeight = 12;

    double scale = 1.0;
    std::vector<UIRect> slots; // 27 rects in slot-index order (0..26)
    UIRect panel;
    UIRect title;
    // The smelting section slots, in furnaceSlotAt order (0=input,
    // 1=fuel, 2=output).
    UIRect input;
    UIRect fuel;
    UIRect output;
    UIRect progress; // the progress bar (between input and output)

    static FurnaceLayout compute(int windowWidth, int windowHeight);
    int slotAt(int px, int py) const;        // slot index or -1
    int furnaceSlotAt(int px, int py) const; // 0=input, 1=fuel, 2=output, -1
};

// The pick-and-place state machine over the player inventory and ONE
// furnace's three slots — the InventoryUI cell semantics (see the file
// header) with no output action (the output slot is an ordinary slot).
// A click that changes the INPUT item resets the smelt progress (the
// furnace.hpp progress invariant). Phase 44: the furnace is BINDABLE —
// the game binds the state of the block the panel was opened on
// (per-block furnace states).
class FurnaceUI {
public:
    FurnaceUI(Inventory& inventory, const ItemRegistry& items)
        : m_inv(inventory), m_items(items) {}

    // Bind the furnace state this panel operates on (the game calls this
    // when the furnace panel opens; the bound state is stable for the
    // life of the panel — block interaction is paused while a panel is
    // open).
    void bind(FurnaceState& furnace) { m_furnace = &furnace; }

    // slot: 0..26 or -1; furnaceSlot: 0..2 or -1 (the layout hit tests
    // are disjoint: at most one is active).
    void leftClick(int slot, int furnaceSlot);
    void rightClick(int slot, int furnaceSlot);

    const ItemStack& carry() const { return m_carry; }
    bool carrying() const { return !m_carry.empty(); }

private:
    void leftClickCell(ItemStack& cell);
    void rightClickCell(ItemStack& cell);
    ItemStack& cellAt(int furnaceSlot);
    // Reset the progress when the input's item changes (furnace.hpp).
    void inputChanged(const ItemStack& before);

    Inventory& m_inv;
    // The bound furnace state (see bind). Non-null while the furnace panel
    // is open.
    FurnaceState* m_furnace = nullptr;
    const ItemRegistry& m_items;
    ItemStack m_carry;
};

// The pick-and-place + craft state machine over one inventory and ONE
// crafting grid (see the file header for the click semantics). The
// caller routes clicks through the layout (slot index, grid slot, output
// hit) so the layout stays a pure function. CRAFT-1: the grid is
// BINDABLE (the FurnaceUI::bind pattern) — the E panel's instance is
// bound to the player's own grid for its whole life; the table panel's
// instance is re-bound to the clicked block's per-block grid on every
// open (the game owns the per-block states, the panel renders the bound
// one — zero layout/visual change, the data source only).
class InventoryUI {
public:
    InventoryUI(Inventory& inventory, const ItemRegistry& items,
                const RecipeRegistry& recipes)
        : m_inv(inventory), m_items(items), m_recipes(recipes) {}

    // Bind the crafting grid this panel operates on. The bound grid is
    // stable for the life of the panel (block interaction is paused while
    // a panel is open). Must be called before any click/match/grid() use.
    void bind(CraftingGrid& grid) { m_grid = &grid; }

    // slot: 0..26 or -1; gridSlot: 0..8 or -1; onOutput: the output slot
    // was hit. At most one of the three is "active" (the layout hit test
    // guarantees it). Returns true when the click crafted (consumed the
    // grid) — the game plays the craft sound on that cue.
    bool leftClick(int slot, int gridSlot, bool onOutput);
    bool rightClick(int slot, int gridSlot, bool onOutput);

    // The live match for the grid (shaped first, then shapeless — see
    // crafting_grid.hpp). Recomputed on every call (cheap: a handful of
    // recipes over 9 cells); the output slot renders from it.
    GridMatch match() const {
        return matchGridRecipe(*m_grid, m_recipes);
    }

    const ItemStack& carry() const { return m_carry; }
    bool carrying() const { return !m_carry.empty(); }
    // The panel's grid (the game renders its cells + saves it).
    const CraftingGrid& grid() const { return *m_grid; }
    CraftingGrid& grid() { return *m_grid; }

private:
    // The shared pick-and-place / move-one rules, over one cell.
    void leftClickCell(ItemStack& cell);
    void rightClickCell(ItemStack& cell);
    // Output-slot click: crafts when the grid matches (see header).
    bool clickOutput(bool rightButton);

    Inventory& m_inv;
    // The bound grid (see bind). Non-null once bound (the game binds the
    // E panel in its constructor and the table panel on every open).
    CraftingGrid* m_grid = nullptr;
    const ItemRegistry& m_items;
    const RecipeRegistry& m_recipes;
    ItemStack m_carry;
};

} // namespace mc
