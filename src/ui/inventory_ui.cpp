#include "ui/inventory_ui.hpp"

#include <algorithm>
#include <cmath>

namespace mc {

namespace {

int scaledLength(double v) {
    // One length of the scaled layout: >=1 px so rects stay non-empty.
    return std::max(1, static_cast<int>(std::lround(v)));
}

// The shared pick-and-place / move-one rules over one cell — the
// InventoryUI and the Phase 40 FurnaceUI use the exact same semantics.
void cellLeftClick(ItemStack& cell, ItemStack& carry,
                   const ItemRegistry& items) {
    const ItemStack cur = cell; // copy: the cell is about to change
    if (carry.empty()) {
        if (cur.empty()) {
            return;
        }
        carry = cur;
        cell = ItemStack {};
        return;
    }
    if (cur.empty()) {
        cell = carry;
        carry = ItemStack {};
        return;
    }
    if (cur.item == carry.item && cur.damage == carry.damage) {
        // SURV-3: same-damage stacks only (the Inventory::add rule) — a
        // repaired piece does not join a worn one.
        const int maxStack = items.at(cur.item).maxStack;
        const int room = maxStack - cur.count;
        const int moved = std::min(carry.count, room);
        if (moved > 0) {
            cell = ItemStack {cur.item, cur.count + moved, cur.damage};
            carry.count -= moved;
            if (carry.count <= 0) {
                carry = ItemStack {};
            }
        }
        // No room and nothing moved: the stack stays in the hand.
        return;
    }
    // Different item: swap the cell's stack with the carried one.
    cell = carry;
    carry = cur;
}

void cellRightClick(ItemStack& cell, ItemStack& carry,
                    const ItemRegistry& items) {
    const ItemStack cur = cell;
    if (carry.empty()) {
        if (cur.empty()) {
            return;
        }
        carry = ItemStack {cur.item, 1, cur.damage}; // SURV-3: damage
                                                     // follows the unit
        cell.count -= 1;
        if (cell.count <= 0) {
            cell = ItemStack {};
        }
        return;
    }
    if (cur.empty()) {
        cell = ItemStack {carry.item, 1, carry.damage}; // SURV-3: damage
                                                        // follows the unit
        carry.count -= 1;
        if (carry.count <= 0) {
            carry = ItemStack {};
        }
        return;
    }
    if (cur.item == carry.item && cur.damage == carry.damage) {
        // SURV-3: same-damage stacks only (the Inventory::add rule).
        const int maxStack = items.at(cur.item).maxStack;
        if (cur.count < maxStack) {
            cell = ItemStack {cur.item, cur.count + 1, cur.damage};
            carry.count -= 1;
            if (carry.count <= 0) {
                carry = ItemStack {};
            }
        }
        return;
    }
    // Different item, occupied cell: no-op (minimal v1 semantics).
}

} // namespace

InventoryLayout InventoryLayout::compute(int windowWidth, int windowHeight,
                                         int recipeCount, PanelKind kind) {
    InventoryLayout out;
    const bool table = kind == PanelKind::CraftingTable;

    // Native (unscaled) design sizes.
    const int gridW = kColumns * kSlot + (kColumns - 1) * kGap;
    // 27 slots = 18 storage (two rows of nine) + the nine-slot hotbar row.
    const int gridH = 3 * kSlot + 2 * kGap;

    // The crafting section: the grid (2x2 = the inventory's top-left cells
    // of the 3x3 model, 3x3 for the table) + the output slot.
    const int gridCols = table ? 3 : 2;
    const int gridSectionW = gridCols * kSlot + (gridCols - 1) * kGap;
    const int sectionW = gridSectionW + kCraftGap + kSlot;
    const int nativePanelW = 2 * kPanelPadding + gridW + kCraftGap + sectionW +
                             (table ? 0 : kCraftGap + kCraftWidth);
    const int craftH =
        table ? 0 : kTitleHeight + recipeCount * kCraftRowHeight;
    const int nativePanelH = 2 * kPanelPadding +
                             std::max(kTitleHeight + gridH, craftH);

    // Fit factor (Phase 35): the panel is designed at the native size;
    // when the window is smaller, EVERYTHING (slots, gaps, padding, title,
    // rows — and the text/icons the renderer draws from `out.scale`)
    // scales down proportionally so the panel fits with >=1 px of margin
    // on each side. Lengths are carried as doubles and rounded only at
    // the rect boundaries, so the parts can never sum past the panel.
    double s = 1.0;
    if (windowWidth > 2 && windowWidth - 2 < nativePanelW) {
        s = std::min(s,
                     static_cast<double>(windowWidth - 2) / nativePanelW);
    }
    if (windowHeight > 2 && windowHeight - 2 < nativePanelH) {
        s = std::min(s,
                     static_cast<double>(windowHeight - 2) / nativePanelH);
    }
    out.scale = s;

    const double slotLen = kSlot * s;
    const double gap = kGap * s;
    const double pad = kPanelPadding * s;
    const double titleH = kTitleHeight * s;
    const double rowH = kCraftRowHeight * s;
    const double craftW = kCraftWidth * s;
    const double craftGap = kCraftGap * s;
    const double gW = gridW * s;
    const double gH = gridH * s;
    const double gsecW = gridSectionW * s;
    const double panelW = nativePanelW * s;
    const double panelH = nativePanelH * s;
    const int panelPixW = static_cast<int>(std::lround(panelW));
    const int panelPixH = static_cast<int>(std::lround(panelH));
    // s keeps panelPixW/H <= window-2, so both origins are >= 0.
    out.panel = UIRect {(windowWidth - panelPixW) / 2,
                        (windowHeight - panelPixH) / 2, panelPixW, panelPixH};

    // Exact (unrounded) origins: every child rect rounds from the same
    // doubles, so siblings stay consistent with the panel.
    const double panelX = (windowWidth - panelW) / 2.0;
    const double panelY = (windowHeight - panelH) / 2.0;
    const double gridX = panelX + pad;
    const double gridY = panelY + pad + titleH;
    out.title = UIRect {static_cast<int>(std::lround(gridX)),
                        static_cast<int>(std::lround(panelY + pad)),
                        scaledLength(gW), scaledLength(titleH)};

    // slots[i] is the rect of inventory slot i: the bottom row is the
    // hotbar (slots 0..8, left to right); above it, two storage rows hold
    // slots 9..17 (top) and 18..26.
    for (int slot = 0; slot < Inventory::kTotalSize; ++slot) {
        const int row =
            (slot < Inventory::kHotbarSize) ? 2 : (slot - Inventory::kHotbarSize) / kColumns;
        const int col =
            (slot < Inventory::kHotbarSize) ? slot : (slot - Inventory::kHotbarSize) % kColumns;
        out.slots.push_back(UIRect {
            static_cast<int>(std::lround(gridX + col * (slotLen + gap))),
            static_cast<int>(std::lround(gridY + row * (slotLen + gap))),
            scaledLength(slotLen), scaledLength(slotLen)});
    }

    // Crafting grid cells, row-major over the top-left gridCols x gridCols
    // corner of the 3x3 model (cell (r, c) = grid slot r*3 + c).
    const double gridSectionX = gridX + gW + craftGap;
    const double gridSectionY = gridY + (gH - gsecW) / 2.0;
    for (int r = 0; r < gridCols; ++r) {
        for (int c = 0; c < gridCols; ++c) {
            out.gridCells.push_back(UIRect {
                static_cast<int>(std::lround(gridSectionX + c * (slotLen + gap))),
                static_cast<int>(std::lround(gridSectionY + r * (slotLen + gap))),
                scaledLength(slotLen), scaledLength(slotLen)});
            out.gridSlots.push_back(r * CraftingGrid::kColumns + c);
        }
    }
    // The output slot, vertically centered against the slot rows.
    const double outputX = gridSectionX + gsecW + craftGap;
    out.output = UIRect {
        static_cast<int>(std::lround(outputX)),
        static_cast<int>(std::lround(gridY + (gH - slotLen) / 2.0)),
        scaledLength(slotLen), scaledLength(slotLen)};

    // The shapeless recipe list (the table panel has none — the grid IS
    // its crafting UI).
    if (!table) {
        const double craftX = outputX + slotLen + craftGap;
        out.craftHeader = UIRect {static_cast<int>(std::lround(craftX)),
                                  static_cast<int>(std::lround(panelY + pad)),
                                  scaledLength(craftW), scaledLength(titleH)};
        for (int i = 0; i < recipeCount; ++i) {
            out.recipes.push_back(UIRect {
                static_cast<int>(std::lround(craftX)),
                static_cast<int>(std::lround(panelY + pad + titleH +
                                             i * rowH)),
                scaledLength(craftW), scaledLength(rowH)});
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
    cellLeftClick(cell, m_carry, m_items);
}

void InventoryUI::rightClickCell(ItemStack& cell) {
    cellRightClick(cell, m_carry, m_items);
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
    consumeGrid(*m_grid, m);
    m_carry.item = recipe.output;
    m_carry.count += n;
    return true;
}

bool InventoryUI::leftClick(int slot, int gridSlot, bool onOutput) {
    if (onOutput) {
        return clickOutput(false);
    }
    if (gridSlot >= 0) {
        leftClickCell(m_grid->at(gridSlot));
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
        rightClickCell(m_grid->at(gridSlot));
        return false;
    }
    if (slot >= 0) {
        rightClickCell(m_inv.at(slot));
        return false;
    }
    return false; // right click outside the slots does nothing
}

// ---------------------------------------------------------------------------
// Phase 40: the furnace panel

FurnaceLayout FurnaceLayout::compute(int windowWidth, int windowHeight) {
    FurnaceLayout out;
    // Native (unscaled) design sizes: the slot grid is the inventory
    // screen's (9 columns x 3 rows); the smelting section replaces the
    // crafting grid + output + recipe column.
    const int slot = InventoryLayout::kSlot;
    const int gap = InventoryLayout::kGap;
    const int pad = InventoryLayout::kPanelPadding;
    const int titleH = InventoryLayout::kTitleHeight;
    const int gridW = InventoryLayout::kColumns * slot +
                      (InventoryLayout::kColumns - 1) * gap;
    const int gridH = 3 * slot + 2 * gap;
    const int sectionW = 2 * slot + 2 * gap + kProgressWidth;
    const int sectionH = 2 * slot + gap;
    const int nativePanelW = 2 * pad + gridW + InventoryLayout::kCraftGap +
                             sectionW;
    const int nativePanelH = 2 * pad + std::max(titleH + gridH, sectionH);

    // Fit factor (the InventoryLayout rule): below the native size the
    // whole panel scales down so it fits with >=1 px margin per side.
    double s = 1.0;
    if (windowWidth > 2 && windowWidth - 2 < nativePanelW) {
        s = std::min(s,
                     static_cast<double>(windowWidth - 2) / nativePanelW);
    }
    if (windowHeight > 2 && windowHeight - 2 < nativePanelH) {
        s = std::min(s,
                     static_cast<double>(windowHeight - 2) / nativePanelH);
    }
    out.scale = s;

    const double slotLen = slot * s;
    const double gapLen = gap * s;
    const double padLen = pad * s;
    const double titleLen = titleH * s;
    const double gW = gridW * s;
    const double gH = gridH * s;
    const double secH = sectionH * s;
    const double panelW = nativePanelW * s;
    const double panelH = nativePanelH * s;
    const int panelPixW = static_cast<int>(std::lround(panelW));
    const int panelPixH = static_cast<int>(std::lround(panelH));
    out.panel = UIRect {(windowWidth - panelPixW) / 2,
                        (windowHeight - panelPixH) / 2, panelPixW, panelPixH};

    // Exact (unrounded) origins: every child rect rounds from the same
    // doubles, so siblings stay consistent with the panel.
    const double panelX = (windowWidth - panelW) / 2.0;
    const double panelY = (windowHeight - panelH) / 2.0;
    const double gridX = panelX + padLen;
    const double gridY = panelY + padLen + titleLen;
    out.title = UIRect {static_cast<int>(std::lround(gridX)),
                        static_cast<int>(std::lround(panelY + padLen)),
                        scaledLength(gW), scaledLength(titleLen)};

    // The 27 inventory slots (the inventory screen's order: the hotbar
    // row 0..8 bottom, storage rows 9..17 / 18..26 above).
    for (int sl = 0; sl < Inventory::kTotalSize; ++sl) {
        const int row =
            (sl < Inventory::kHotbarSize)
                ? 2
                : (sl - Inventory::kHotbarSize) / InventoryLayout::kColumns;
        const int col =
            (sl < Inventory::kHotbarSize)
                ? sl
                : (sl - Inventory::kHotbarSize) % InventoryLayout::kColumns;
        out.slots.push_back(UIRect {
            static_cast<int>(std::lround(gridX + col * (slotLen + gapLen))),
            static_cast<int>(std::lround(gridY + row * (slotLen + gapLen))),
            scaledLength(slotLen), scaledLength(slotLen)});
    }

    // The smelting section, vertically centered against the slot rows:
    // input top-left, output top-right, fuel bottom-left, the progress
    // bar between input and output on the top row (Minecraft's layout).
    const double secX = gridX + gW + InventoryLayout::kCraftGap * s;
    const double secY = gridY + (gH - secH) / 2.0;
    const double progressW = kProgressWidth * s;
    const double progressH = kProgressHeight * s;
    out.input = UIRect {static_cast<int>(std::lround(secX)),
                        static_cast<int>(std::lround(secY)),
                        scaledLength(slotLen), scaledLength(slotLen)};
    out.output = UIRect {
        static_cast<int>(std::lround(
            secX + slotLen + 2 * gapLen + progressW)), // gap on both sides
        static_cast<int>(std::lround(secY)),
        scaledLength(slotLen), scaledLength(slotLen)};
    out.fuel = UIRect {static_cast<int>(std::lround(secX)),
                       static_cast<int>(std::lround(secY + slotLen + gapLen)),
                       scaledLength(slotLen), scaledLength(slotLen)};
    out.progress = UIRect {
        static_cast<int>(std::lround(secX + slotLen + gapLen)),
        static_cast<int>(std::lround(secY + (slotLen - progressH) / 2.0)),
        scaledLength(progressW), scaledLength(progressH)};
    return out;
}

int FurnaceLayout::slotAt(int px, int py) const {
    for (std::size_t i = 0; i < slots.size(); ++i) {
        if (slots[i].contains(px, py)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int FurnaceLayout::furnaceSlotAt(int px, int py) const {
    if (input.contains(px, py)) {
        return 0;
    }
    if (fuel.contains(px, py)) {
        return 1;
    }
    if (output.contains(px, py)) {
        return 2;
    }
    return -1;
}

// ---------------------------------------------------------------------------

void FurnaceUI::leftClickCell(ItemStack& cell) {
    cellLeftClick(cell, m_carry, m_items);
}

void FurnaceUI::rightClickCell(ItemStack& cell) {
    cellRightClick(cell, m_carry, m_items);
}

ItemStack& FurnaceUI::cellAt(int furnaceSlot) {
    switch (furnaceSlot) {
        case 0:
            return m_furnace->input;
        case 1:
            return m_furnace->fuel;
        default:
            return m_furnace->output;
    }
}

void FurnaceUI::inputChanged(const ItemStack& before) {
    // The progress belongs to the input's item: a changed item (including
    // removal) loses the progress (the furnace.hpp invariant — the tick
    // re-establishes progressFor on the next substep).
    if (before.item != m_furnace->input.item) {
        m_furnace->progressSubsteps = 0;
    }
}

void FurnaceUI::leftClick(int slot, int furnaceSlot) {
    if (furnaceSlot >= 0) {
        const ItemStack before = m_furnace->input;
        leftClickCell(cellAt(furnaceSlot));
        if (furnaceSlot == 0) {
            inputChanged(before);
        }
        return;
    }
    if (slot >= 0) {
        leftClickCell(m_inv.at(slot));
        return;
    }
    m_carry = ItemStack {}; // click outside every slot: drop the stack
}

void FurnaceUI::rightClick(int slot, int furnaceSlot) {
    if (furnaceSlot >= 0) {
        const ItemStack before = m_furnace->input;
        rightClickCell(cellAt(furnaceSlot));
        if (furnaceSlot == 0) {
            inputChanged(before);
        }
        return;
    }
    if (slot >= 0) {
        rightClickCell(m_inv.at(slot));
        return;
    }
    // right click outside the slots does nothing
}

} // namespace mc
