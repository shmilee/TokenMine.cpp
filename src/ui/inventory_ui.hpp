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
//   +-------------------------------------------------------------+
//   |  Inventory          [g][g]   [out]     Crafting             |
//   |  [27][28]...[35]    [g][g]           [icon] planks x4       |
//   |  [18][19]...[26]                [icon] stick x4             |
//   |  [9 ][10]...[17]                                    |
//   |  [0 ][1 ]...[8 ]   (hotbar)                            |
//   +-------------------------------------------------------------+
// Storage = slots 9..35 in three rows (top to bottom), the hotbar = slots
// 0..8 in the bottom row — the same 36 Inventory slots the game already
// owns (no separate storage, UX-8f's full Minecraft backpack). The
// CraftingTable kind is the same panel
// with a 3x3 grid instead of 2x2 and the shaped recipe HINT list (CRAFT-4)
// instead of the shapeless craft list; BOTH panels split their recipe
// list over two columns (CRAFT-4 for the table, UX-8c for the E panel).
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

#include <algorithm>
#include <cstddef>
#include <string>
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

// The two crafting panels: the E inventory (2x2 grid + the shapeless
// recipe craft list) and the crafting table (3x3 grid + the shaped
// recipe HINT list, CRAFT-4 — display-only rows with the mini pattern).
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
    // UX-8f: the grid's row count — one storage row per kColumns of the
    // storage slots (27 storage = three rows) + the hotbar row (four).
    static constexpr int kStorageRows =
        (Inventory::kTotalSize - Inventory::kHotbarSize) / kColumns;
    static_assert((Inventory::kTotalSize - Inventory::kHotbarSize) %
                        kColumns == 0,
                  "the storage slots must fill whole rows");
    static constexpr int kGridRows = kStorageRows + 1;
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
    // CRAFT-4: the crafting table's recipe HINT column (shaped recipes,
    // two side-by-side columns of kCraftWidth) — the gap between them.
    static constexpr int kRecipeColumnGap = 8;

    // Fit factor (Phase 35): 1.0 when the window is at least the native
    // panel size, below that the whole panel — and the text and icons the
    // renderer draws from this factor — scales down proportionally, so
    // the panel always fits with >=1 px of margin on each side.
    double scale = 1.0;

    // 36 slot rects in slot-index order (0..35, the full kTotalSize).
    std::vector<UIRect> slots;
    // Crafting grid cell rects: 4 (Inventory kind, the 2x2) or 9
    // (CraftingTable kind, the 3x3), row-major.
    std::vector<UIRect> gridCells;
    // Parallel to gridCells: the CraftingGrid slot each cell maps to
    // (row-major over the 3x3 model: the 2x2 uses slots 0, 1, 3, 4).
    std::vector<int> gridSlots;
    // The output slot (the matched recipe's result).
    UIRect output;
    // One row per SHAPELESS recipe (Inventory kind) or per SHAPED recipe
    // (CraftingTable kind) — both kinds split the rows over two columns
    // (CRAFT-4 for the table, UX-8c for the E panel): the first
    // (recipeCount + 1) / 2 rows sit in the left column, the rest in the
    // right, row i - leftRows aligned with row i.
    std::vector<UIRect> recipes;
    UIRect panel;
    // The "Inventory"/"Crafting Table" title and the "Crafting" header.
    UIRect title;
    UIRect craftHeader;

    // recipeCount: shapeless recipes (Inventory kind) or shaped recipes
    // (CraftingTable kind, CRAFT-4).
    static InventoryLayout compute(int windowWidth, int windowHeight,
                                   int recipeCount,
                                   PanelKind kind = PanelKind::Inventory);
    int slotAt(int px, int py) const;       // slot index or -1
    int gridCellAt(int px, int py) const;   // gridCells index or -1
    int recipeRowAt(int px, int py) const;  // row index or -1
    bool inOutput(int px, int py) const { return output.contains(px, py); }
};

// UX-8c: the E panel's recipe row order — a STABLE partition of `hints`:
// every recipe `inv` can craft (canCraft, the click path's own gate)
// first, then the rest, each group keeping the registry order of
// `hints`. The click path and the render path both apply it (the single
// seam), so a row index means the same recipe in both. Pure: reads the
// inventory and registry only, deterministic.
std::vector<const Recipe*> orderedRecipeHints(
    const std::vector<const Recipe*>& hints, const Inventory& inv,
    const ItemRegistry& items);

// Phase 40: the furnace panel — the smelting GUI, opened by right-clicking
// a furnace in the world. Same panel chrome as the inventory screen: the
// 36 inventory slots + the smelting section in Minecraft's arrangement
// (input top-left, output top-right with the progress bar between, fuel
// bottom-left). No crafting of any kind happens here — the output fills
// by itself (the furnace tick) and is taken with an ordinary slot click.
struct FurnaceLayout {
    // The progress bar (Minecraft's arrow): native width; the fill is
    // progressSubsteps / smeltSubsteps of the width.
    static constexpr int kProgressWidth = 56;
    static constexpr int kProgressHeight = 12;

    double scale = 1.0;
    std::vector<UIRect> slots; // 36 rects in slot-index order (0..35)
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

    // slot: 0..35 or -1; furnaceSlot: 0..2 or -1 (the layout hit tests
    // are disjoint: at most one is active).
    void leftClick(int slot, int furnaceSlot);
    void rightClick(int slot, int furnaceSlot);
    // CRAFT-7: shift-click quick move (Minecraft's container-GUI
    // semantics): a furnace slot (input/fuel/output) moves its whole
    // stack into the player inventory (storage 9..35 first, then the
    // hotbar, merging same-item same-damage stacks); a click on the
    // player inventory is a no-op; carrying a stack is a no-op (the
    // carry is kept, Minecraft). Moving the input resets the progress
    // when the input item changes (the furnace.hpp invariant).
    void quickMove(int slot, int furnaceSlot);
    // CRAFT-7: double-click collect (Minecraft): with a carried stack
    // (the first click of the pair picked it up), gather every stack of
    // the SAME item and SAME damage in its SECTION (Minecraft: the
    // hotbar 0..8 and the main storage 9..35 are separate sections)
    // into the clicked slot (capped at maxStack), the remainder filling
    // the section's other slots in index order. No carry = no-op; the
    // furnace slots and the crafting grid are NOT part of the collect
    // (documented deviation: the grid).
    void doubleCollect(int slot);

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

// V2-12: the enchanting panel — the smelting-GUI twin (the furnace panel's
// layout pattern): the 36 inventory slots + a section with the ITEM slot
// (left), the LAPIS slot (top-middle, the enchant_material gate) and the
// THREE offer rows (right — "name roman xpCost"). No per-block persistence
// (the offers are rolled on demand, the item/lapis slots are session state
// returned to the inventory on close — the documented deviation from the
// furnace's per-block states).
struct EnchantLayout {
    // The offer row metrics (native): each row shows the first enchant's
    // name + roman level + the xp cost.
    static constexpr int kOfferRowHeight = 40;
    static constexpr int kOfferRowWidth = 170;
    static constexpr int kOfferColumnGap = 12;

    double scale = 1.0;
    std::vector<UIRect> slots; // 36 rects in slot-index order (0..35)
    UIRect panel;
    UIRect title;
    UIRect item; // the item slot (left)
    UIRect lapis; // the lapis slot (top-middle)
    UIRect offers[3]; // the three offer rows (right column, top to bottom)

    static EnchantLayout compute(int windowWidth, int windowHeight);
    int slotAt(int px, int py) const; // slot index or -1
    // 0=item, 1=lapis, 2..4=offer rows 0..1..2, -1=none (the hit tests are
    // disjoint: at most one is active).
    int panelSlotAt(int px, int py) const;
};

// V2-12: the pick-and-place state machine over the player inventory + the
// enchant panel's TWO slots (item, lapis) — the FurnaceUI cell semantics
// (the shared cellLeftClick / cellRightClick). The offer ROWS are NOT part
// of this state machine: the game handles them (the apply needs the
// player's XP + the xpSeed + the sound). The lapis slot accepts only
// `enchant_material` items (the data-driven gate, the item's flag).
class EnchantUI {
public:
    EnchantUI(Inventory& inventory, const ItemRegistry& items)
        : m_inv(inventory), m_items(items) {}

    // Bind the two panel slots this operates on (the game's session state,
    // cleared on open, returned on close).
    void bind(ItemStack& itemSlot, ItemStack& lapisSlot) {
        m_itemSlot = &itemSlot;
        m_lapisSlot = &lapisSlot;
    }

    // slot: 0..35 or -1; panelSlot: 0=item, 1=lapis, -1 (the layout hit
    // tests are disjoint: at most one is active). Returns true when the
    // ITEM slot's item CHANGED (the game re-rolls the offers on that cue).
    bool leftClick(int slot, int panelSlot);
    bool rightClick(int slot, int panelSlot);
    void quickMove(int slot, int panelSlot);

    const ItemStack& carry() const { return m_carry; }
    bool carrying() const { return !m_carry.empty(); }

private:
    void leftClickCell(ItemStack& cell);
    void rightClickCell(ItemStack& cell);
    ItemStack& cellAt(int panelSlot);
    // The lapis slot's data-driven gate (an empty placement or an
    // `enchant_material` item is accepted).
    bool lapisAccepts(const ItemStack& stack) const;
    Inventory& m_inv;
    const ItemRegistry& m_items;
    ItemStack* m_itemSlot = nullptr;
    ItemStack* m_lapisSlot = nullptr;
    ItemStack m_carry;
};

// V3-11b: the creative's E-panel face (the 物品栏 tab's layout + the
// search / scroll / grant state machine). The OUTER panel is the
// Inventory kind's (both tabs share it — the survival's layout and the
// 背包 tab's content stay bit-identical; the tabs live in the title
// row, the creative's default). No SDL, no GL — pure geometry + the
// Inventory's public slot operations, unit-testable without a game
// (the EnchantUI's model).
struct CreativeLayout {
    // Native (unscaled) design, in UI px (the InventoryLayout's twin).
    static constexpr int kTabWidth = 96;
    static constexpr int kTabHeight = 32;
    static constexpr int kTabGap = 8;
    static constexpr int kSearchWidth = 300;
    static constexpr int kSearchHeight = 32;
    static constexpr int kSearchGap = 16;
    static constexpr int kGridColumns = 6;
    static constexpr int kGridRows = 4;
    static constexpr int kVisible = kGridColumns * kGridRows;

    UIRect panel; // = the Inventory kind's outer (the shared rect)
    UIRect tabItems; // the title row's left button (active = 物品栏)
    UIRect tabBackpack; // the title row's second button (背包)
    UIRect search; // the search box (the 物品栏 tab only)
    std::vector<UIRect> cells; // kVisible grid cells (the 物品栏 tab)

    // The 物品栏 tab's layout. The outer panel + the fit factor come
    // from the E kind's compute (the recipeCount keeps the 背包 tab's
    // outer identical to the survival's), so both tabs share the exact
    // rect + scale.
    static CreativeLayout compute(int windowWidth, int windowHeight,
                                  int recipeCount);
    int tabAt(int px, int py) const; // 0 = 物品栏, 1 = 背包, -1 = none
    int cellAt(int px, int py) const; // 0..kVisible-1, -1 = none
};

// The creative tab's state machine (see the CreativeLayout header). The
// search text + the scroll are SESSION state (cleared on every open —
// the enchant slots' precedent; nothing is saved). The grant writes the
// player's SELECTED hotbar slot — the game passes the index, the UI
// never knows which slot is selected (the same data-source split as
// the EnchantUI's bind).
class CreativeTabUI {
public:
    // The search filter's cap (item names are short; 24 covers the
    // longest + headroom).
    static constexpr int kSearchTextMax = 24;

    CreativeTabUI(Inventory& inventory, const ItemRegistry& items)
        : m_inv(inventory), m_items(items) {}

    // Fresh session state (the openPanel's reset).
    void reset() {
        m_text.clear();
        m_scroll = 0;
        m_filtered.clear();
        m_dirty = true;
    }

    // The frame's typed text (the Input's frameText twin — the
    // consumer-side filter: only [a-z0-9_] survives, uppercased input
    // lowercased, capped at kSearchTextMax — the item names' charset,
    // the vocabulary's twin).
    void appendFrameText(const std::string& frame);
    void backspace();
    const std::string& text() const { return m_text; }

    // The wheel's ticks (one tick = one item, clamped to the filtered
    // list's tail; an empty delta is a no-op).
    void scrollBy(int delta);
    int scroll() const { return m_scroll; }

    // The filtered list: every item whose NAME starts with the search
    // text (empty text = every grantable item), name-ascending
    // (deterministic — the registry's id order is build-order, the
    // name order is the player's). Calling it re-filters when the text
    // changed AND re-clamps the scroll to the tail (the text can
    // shrink the list under a scrolled window).
    const std::vector<ItemID>& filtered() const;
    // The visible window's START index (= the scroll — the invariant
    // is that it is always clamped to [0, tail], the filtered() twin).
    int visibleStart() const { return m_scroll; }

    // Grant: cell = the 0-based index into the VISIBLE window (the
    // layout's cellAt), targetSlot = the player's selected hotbar slot
    // (0..8). full = a max-stack stack (the left click), single = one
    // (the right click). Same item with room = merge (the damage /
    // enchants drop — a creative grant carries neither); a different
    // item, or a full same-item slot, = replace / no-op. Returns true
    // when the slot CHANGED (the game's pickup-sound cue).
    bool grant(int targetSlot, int cell, bool full);

private:
    // const (the filtered() twin — the mutable state carries it).
    void refreshFiltered() const;
    Inventory& m_inv;
    const ItemRegistry& m_items;
    std::string m_text;
    // mutable: the filtered() const path re-clamps the scroll when the
    // text shrinks the list under a scrolled window (the state machine
    // is read-through for the renderer).
    mutable int m_scroll = 0;
    mutable std::vector<ItemID> m_filtered;
    mutable bool m_dirty = true;
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

    // slot: 0..35 or -1; gridSlot: 0..8 or -1; onOutput: the output slot
    // was hit. At most one of the three is "active" (the layout hit test
    // guarantees it). Returns true when the click crafted (consumed the
    // grid) — the game plays the craft sound on that cue.
    bool leftClick(int slot, int gridSlot, bool onOutput);
    bool rightClick(int slot, int gridSlot, bool onOutput);
    // CRAFT-7: shift-click quick move (Minecraft's E-screen semantics):
    // a hotbar slot (0..8) moves its whole stack into the storage
    // (9..35), a storage slot into the hotbar, a grid cell into the
    // storage (then the hotbar if it does not fit) — each destination
    // fills in index order, merging same-item same-damage stacks up to
    // the max (a stack may split across several slots, Minecraft).
    // The output slot and a carried stack are no-ops (the carry is
    // kept). Deterministic over the slot index order.
    void quickMove(int slot, int gridSlot, bool onOutput);
    // CRAFT-7: double-click collect (Minecraft): with a carried stack
    // (the first click of the pair picked it up), gather every stack of
    // the SAME item and SAME damage in its SECTION (Minecraft: the
    // hotbar 0..8 and the main storage 9..35 are separate sections)
    // into the clicked slot (capped at maxStack), the remainder filling
    // the section's other slots in index order. No carry = no-op; the
    // grid and the output are NOT part of the collect.
    void doubleCollect(int slot);

    // The live match for the grid: the recipe match (shaped first, then
    // shapeless — see crafting_grid.hpp) and, when NO recipe matches, the
    // UX-5 material-repair fallback (matchRepair: the damaged repairable
    // tool + its material in the grid). The recipe always wins (the
    // repair is a pure fallback — see crafting_grid.hpp). Recomputed on
    // every call (cheap: a handful of recipes over 9 cells); the output
    // slot renders from it.
    GridMatch match() const {
        GridMatch m = matchGridRecipe(*m_grid, m_recipes);
        if (!m.matched()) {
            m.repair = matchRepair(*m_grid, m_items);
        }
        return m;
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
