// Phase 15: the inventory screen — pure layout + click state machine.
//
// No SDL and no GL: the layout is a function of the window size and the
// recipe count, and the clicks mutate an Inventory through its public slot
// operations. Everything is deterministic, so the whole screen is unit-
// testable without a game.
//
// Screen layout (window 1280x720, panel centered):
//   +-----------------------------------------------+
//   |  Inventory                Crafting            |
//   |  [9 ][10]...[17]           [icon] planks x4   |
//   |  [18][19]...[26]           [icon] stick x1    |
//   |  [0 ][1 ]...[8 ]  (hotbar) [icon] wood. pick. |
//   +-----------------------------------------------+
// Storage = slots 9..26 in two rows (top to bottom), the hotbar = slots
// 0..8 in the bottom row — the same 27 Inventory slots the game already
// owns (no separate storage).
//
// Click semantics (Minecraft's minimal subset, documented in the class):
//   left click : pick up a whole stack / place or swap the carried one
//                (merging when the same item has room); a click outside
//                every slot drops the carried stack.
//   right click: move ONE item between the carried stack and a slot
//                (empty target, or the same item with room).
#pragma once

#include <cstddef>
#include <vector>

#include "item/inventory.hpp"
#include "item/item_registry.hpp"

namespace mc {

struct UIRect {
    int x = 0, y = 0, w = 0, h = 0;
    bool contains(int px, int py) const {
        return px >= x && px < x + w && py >= y && py < y + h;
    }
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

    // 27 slot rects in slot-index order (0..26).
    std::vector<UIRect> slots;
    // One row per recipe, in RecipeRegistry order.
    std::vector<UIRect> recipes;
    UIRect panel;
    // The "Inventory" title and the "Crafting" header.
    UIRect title;
    UIRect craftHeader;

    static InventoryLayout compute(int windowWidth, int windowHeight,
                                   int recipeCount);
    int slotAt(int px, int py) const;       // slot index or -1
    int recipeRowAt(int px, int py) const;  // row index or -1
};

// The pick-and-place state machine over one inventory (see the file
// header for the click semantics). The carried stack is owned here and
// rendered by the game as a floating icon under the cursor.
class InventoryUI {
public:
    InventoryUI(Inventory& inventory, const ItemRegistry& items)
        : m_inv(inventory), m_items(items) {}

    void leftClick(int slot);
    void rightClick(int slot);

    const ItemStack& carry() const { return m_carry; }
    bool carrying() const { return !m_carry.empty(); }

private:
    Inventory& m_inv;
    const ItemRegistry& m_items;
    ItemStack m_carry;
};

} // namespace mc
