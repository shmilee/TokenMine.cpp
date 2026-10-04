#include "ui/inventory_ui.hpp"

#include <algorithm>

namespace mc {

InventoryLayout InventoryLayout::compute(int windowWidth, int windowHeight,
                                         int recipeCount, PanelKind kind) {
    InventoryLayout out;
    const bool table = kind == PanelKind::CraftingTable;
    const int gridW = kColumns * kSlot + (kColumns - 1) * kGap;
    // 27 slots = 18 storage (two rows of nine) + the nine-slot hotbar row.
    const int gridH = 3 * kSlot + 2 * kGap;

    // The crafting section: the grid (2x2 = the inventory's top-left cells
    // of the 3x3 model, 3x3 for the table) + the output slot.
    const int gridCols = table ? 3 : 2;
    const int gridSectionW = gridCols * kSlot + (gridCols - 1) * kGap;
    const int sectionW = gridSectionW + kCraftGap + kSlot;
    const int panelW = 2 * kPanelPadding + gridW + kCraftGap + sectionW +
                       (table ? 0 : kCraftGap + kCraftWidth);
    const int craftH =
        table ? 0 : kTitleHeight + recipeCount * kCraftRowHeight;
    const int panelH = 2 * kPanelPadding + std::max(kTitleHeight + gridH,
                                                    craftH);
    out.panel = UIRect {(windowWidth - panelW) / 2,
                        (windowHeight - panelH) / 2, panelW, panelH};

    const int gridX = out.panel.x + kPanelPadding;
    const int gridY = out.panel.y + kPanelPadding + kTitleHeight;
    out.title = UIRect {gridX, out.panel.y + kPanelPadding, gridW,
                        kTitleHeight};

    // slots[i] is the rect of inventory slot i: the bottom row is the
    // hotbar (slots 0..8, left to right); above it, two storage rows hold
    // slots 9..17 (top) and 18..26.
    for (int slot = 0; slot < Inventory::kTotalSize; ++slot) {
        const int row =
            (slot < Inventory::kHotbarSize) ? 2 : (slot - Inventory::kHotbarSize) / kColumns;
        const int col =
            (slot < Inventory::kHotbarSize) ? slot : (slot - Inventory::kHotbarSize) % kColumns;
        out.slots.push_back(UIRect {
            gridX + col * (kSlot + kGap),
            gridY + row * (kSlot + kGap), kSlot, kSlot});
    }

    // Crafting grid cells, row-major over the top-left gridCols x gridCols
    // corner of the 3x3 model (cell (r, c) = grid slot r*3 + c).
    for (int r = 0; r < gridCols; ++r) {
        for (int c = 0; c < gridCols; ++c) {
            out.gridCells.push_back(UIRect {
                gridX + gridW + kCraftGap + c * (kSlot + kGap),
                gridY + (gridH - gridSectionW) / 2 + r * (kSlot + kGap),
                kSlot, kSlot});
            out.gridSlots.push_back(r * CraftingGrid::kColumns + c);
        }
    }
    // The output slot, vertically centered against the slot rows.
    out.output = UIRect {
        gridX + gridW + kCraftGap + gridSectionW + kCraftGap,
        gridY + (gridH - kSlot) / 2, kSlot, kSlot};

    // The shapeless recipe list (the table panel has none — the grid IS
    // its crafting UI).
    if (!table) {
        const int craftX = out.output.x + kSlot + kCraftGap;
        out.craftHeader = UIRect {craftX, out.panel.y + kPanelPadding,
                                  kCraftWidth, kTitleHeight};
        for (int i = 0; i < recipeCount; ++i) {
            out.recipes.push_back(UIRect {
                craftX, out.panel.y + kPanelPadding + kTitleHeight +
                            i * kCraftRowHeight,
                kCraftWidth, kCraftRowHeight});
        }
    }
    return out;
}

int InventoryLayout::slotAt(int px, int py) const {
    for (std::size_t i = 0; i < slots.size(); ++i) {
        if (slots[i].contains(px, py)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int InventoryLayout::gridCellAt(int px, int py) const {
    for (std::size_t i = 0; i < gridCells.size(); ++i) {
        if (gridCells[i].contains(px, py)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int InventoryLayout::recipeRowAt(int px, int py) const {
    for (std::size_t i = 0; i < recipes.size(); ++i) {
        if (recipes[i].contains(px, py)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// ---------------------------------------------------------------------------

void InventoryUI::leftClickCell(ItemStack& cell) {
    const ItemStack cur = cell; // copy: the cell is about to change
    if (m_carry.empty()) {
        if (cur.empty()) {
            return;
        }
        m_carry = cur;
        cell = ItemStack {};
        return;
    }
    if (cur.empty()) {
        cell = m_carry;
        m_carry = ItemStack {};
        return;
    }
    if (cur.item == m_carry.item) {
        const int maxStack = m_items.at(cur.item).maxStack;
        const int room = maxStack - cur.count;
        const int moved = std::min(m_carry.count, room);
        if (moved > 0) {
            cell = ItemStack {cur.item, cur.count + moved};
            m_carry.count -= moved;
            if (m_carry.count <= 0) {
                m_carry = ItemStack {};
            }
        }
        // No room and nothing moved: the stack stays in the hand.
        return;
    }
    // Different item: swap the cell's stack with the carried one.
    cell = m_carry;
    m_carry = cur;
}

void InventoryUI::rightClickCell(ItemStack& cell) {
    const ItemStack cur = cell;
    if (m_carry.empty()) {
        if (cur.empty()) {
            return;
        }
        m_carry = ItemStack {cur.item, 1};
        cell.count -= 1;
        if (cell.count <= 0) {
            cell = ItemStack {};
        }
        return;
    }
    if (cur.empty()) {
        cell = ItemStack {m_carry.item, 1};
        m_carry.count -= 1;
        if (m_carry.count <= 0) {
            m_carry = ItemStack {};
        }
        return;
    }
    if (cur.item == m_carry.item) {
        const int maxStack = m_items.at(cur.item).maxStack;
        if (cur.count < maxStack) {
            cell = ItemStack {cur.item, cur.count + 1};
            m_carry.count -= 1;
            if (m_carry.count <= 0) {
                m_carry = ItemStack {};
            }
        }
        return;
    }
    // Different item, occupied cell: no-op (minimal v1 semantics).
}

bool InventoryUI::clickOutput(bool rightButton) {
    const GridMatch m = match();
    if (!m.matched()) {
        return false;
    }
    const Recipe& recipe = *m.recipe;
    const int maxStack = m_items.at(recipe.output).maxStack;
    const int n = rightButton ? std::min(1, recipe.outputCount)
                              : recipe.outputCount;
    if (!m_carry.empty() &&
        (m_carry.item != recipe.output || m_carry.count + n > maxStack)) {
        return false; // the output cannot hold foreign/overflowing stacks
    }
    consumeGrid(m_grid, m);
    m_carry.item = recipe.output;
    m_carry.count += n;
    return true;
}

bool InventoryUI::leftClick(int slot, int gridSlot, bool onOutput) {
    if (onOutput) {
        return clickOutput(false);
    }
    if (gridSlot >= 0) {
        leftClickCell(m_grid.at(gridSlot));
        return false;
    }
    if (slot >= 0) {
        leftClickCell(m_inv.at(slot));
        return false;
    }
    m_carry = ItemStack {}; // click outside every slot: drop the stack
    return false;
}

bool InventoryUI::rightClick(int slot, int gridSlot, bool onOutput) {
    if (onOutput) {
        return clickOutput(true);
    }
    if (gridSlot >= 0) {
        rightClickCell(m_grid.at(gridSlot));
        return false;
    }
    if (slot >= 0) {
        rightClickCell(m_inv.at(slot));
        return false;
    }
    return false; // right click outside the slots does nothing
}

} // namespace mc
