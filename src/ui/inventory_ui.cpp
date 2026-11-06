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
    if (cur.item == carry.item && cur.damage == carry.damage &&
        cur.enchants == carry.enchants) {
        // SURV-3: same-damage stacks only (the Inventory::add rule) — a
        // repaired piece does not join a worn one. V2-12: same-enchant
        // only — an enchanted stack does not join a plain one (or one with
        // a different enchantment set).
        const int maxStack = items.at(cur.item).maxStack;
        const int room = maxStack - cur.count;
        const int moved = std::min(carry.count, room);
        if (moved > 0) {
            cell.count += moved; // keep cell.enchants (== carry's — merged)
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
        // CRAFT-7: right-click takes the HALF stack (Minecraft):
        // floor(count/2), minimum 1 — a 1-stack gives itself, a 2- or
        // 3-stack gives 1 (the v1 "take one" behavior), a 64-stack
        // gives 32. SURV-3: damage follows the units. V2-12: the
        // enchantments follow the units too.
        const int take = std::max(1, cur.count / 2);
        carry = ItemStack {cur.item, take, cur.damage};
        carry.enchants = cur.enchants;
        cell.count -= take;
        if (cell.count <= 0) {
            cell = ItemStack {};
        }
        return;
    }
    if (cur.empty()) {
        cell = ItemStack {carry.item, 1, carry.damage}; // SURV-3: damage
        cell.enchants = carry.enchants; // V2-12: enchants follow the unit
        carry.count -= 1;
        if (carry.count <= 0) {
            carry = ItemStack {};
        }
        return;
    }
    if (cur.item == carry.item && cur.damage == carry.damage &&
        cur.enchants == carry.enchants) {
        // SURV-3: same-damage stacks only; V2-12: same-enchant only (the
        // Inventory::add rule).
        const int maxStack = items.at(cur.item).maxStack;
        if (cur.count < maxStack) {
            cell.count += 1; // keep cell.enchants (== carry's — merged)
            carry.count -= 1;
            if (carry.count <= 0) {
                carry = ItemStack {};
            }
        }
        return;
    }
    // Different item, occupied cell: no-op (minimal v1 semantics).
}

// CRAFT-7: the shift-click move — move as much of src as possible into
// the inventory slots listed in order (the caller fixes the index
// order), merging same-item same-damage stacks up to the max (the
// SURV-3 gate); a stack may split across several slots (Minecraft).
// src is cleared when fully moved.
void moveStackIntoOrder(ItemStack& src, Inventory& inv,
                        const ItemRegistry& items,
                        const std::vector<int>& order) {
    if (src.empty()) {
        return;
    }
    const int maxStack = items.at(src.item).maxStack;
    for (const int i : order) {
        if (src.empty()) {
            break;
        }
        const ItemStack& cell = inv.at(i);
        if (cell.empty()) {
            const int put = std::min(src.count, maxStack);
            ItemStack moved {src.item, put, src.damage};
            moved.enchants = src.enchants; // V2-12: the stack's enchants
            inv.setSlot(i, std::move(moved));
            src.count -= put;
        } else if (cell.item == src.item && cell.damage == src.damage &&
                   cell.enchants == src.enchants && // V2-12: same-enchant
                   cell.count < maxStack) {
            const int put = std::min(src.count, maxStack - cell.count);
            ItemStack merged {cell.item, cell.count + put, cell.damage};
            merged.enchants = cell.enchants; // V2-12: keep the cell's
            inv.setSlot(i, std::move(merged));
            src.count -= put;
        }
    }
    if (src.count <= 0) {
        src = ItemStack {};
    }
}

// CRAFT-7: the destination order for a quick move over the whole
// player inventory: storage (hotbar..last) first — Minecraft puts a
// container's items into the main inventory before the hotbar — then
// the hotbar.
std::vector<int> inventorySlotOrder(const Inventory& inv, bool hotbarFirst) {
    std::vector<int> order;
    const int hb = inv.hotbarSize();
    const int n = static_cast<int>(inv.size());
    if (hotbarFirst) {
        for (int i = 0; i < hb; ++i) {
            order.push_back(i);
        }
        for (int i = hb; i < n; ++i) {
            order.push_back(i);
        }
    } else {
        for (int i = hb; i < n; ++i) {
            order.push_back(i);
        }
        for (int i = 0; i < hb; ++i) {
            order.push_back(i);
        }
    }
    return order;
}

// CRAFT-7: the double-click collect over ONE player-inventory section
// (shared by the crafting panels and the furnace panel). Minecraft
// sections the screen: the hotbar (0..8) and the main storage (9..35,
// the full UX-8f backpack) are SEPARATE collect sections —
// double-clicking a slot collects from
// its own section only. With a carried stack (the first click of the
// pair picked it up), gather every same-item same-damage stack of the
// section into the clicked slot (capped at maxStack), the remainder
// filling the section's other slots in index order. The total always
// fits (it came from the very slots being refilled), so no overflow
// exists; the defensive carry keeps it total regardless.
void collectInventoryInto(Inventory& inv, const ItemRegistry& items,
                          ItemStack& carry, int slot) {
    if (slot < 0 || slot >= static_cast<int>(inv.size())) {
        return;
    }
    if (carry.empty()) {
        return;
    }
    const ItemID id = carry.item;
    const float dmg = carry.damage;
    const int maxStack = items.at(id).maxStack;
    // The section bounds: the hotbar or the main storage.
    const int hb = inv.hotbarSize();
    const int lo = slot < hb ? 0 : hb;
    const int hi = slot < hb ? hb : static_cast<int>(inv.size());
    if (!inv.at(slot).empty() &&
        !(inv.at(slot).item == id && inv.at(slot).damage == dmg)) {
        return; // defensive: the clicked slot holds a foreign item
    }
    int remaining = carry.count;
    for (int i = lo; i < hi; ++i) {
        if (inv.at(i).item == id && inv.at(i).damage == dmg) {
            remaining += inv.at(i).count;
            inv.setSlot(i, ItemStack {});
        }
    }
    carry = ItemStack {};
    if (remaining > 0) {
        const int put = std::min(remaining, maxStack);
        inv.setSlot(slot, ItemStack {id, put, dmg});
        remaining -= put;
    }
    for (int i = lo; i < hi && remaining > 0; ++i) {
        if (i == slot) {
            continue;
        }
        const ItemStack& cell = inv.at(i);
        if (cell.empty() ||
            (cell.item == id && cell.damage == dmg && cell.count < maxStack)) {
            const int put = std::min(remaining, maxStack - cell.count);
            inv.setSlot(i, ItemStack {id, cell.count + put, dmg});
            remaining -= put;
        }
    }
    if (remaining > 0) {
        carry = ItemStack {id, remaining, dmg};
    }
}

} // namespace

InventoryLayout InventoryLayout::compute(int windowWidth, int windowHeight,
                                         int recipeCount, PanelKind kind) {
    InventoryLayout out;
    const bool table = kind == PanelKind::CraftingTable;

    // Native (unscaled) design sizes.
    const int gridW = kColumns * kSlot + (kColumns - 1) * kGap;
    // UX-8f: the full 36-slot grid — 27 storage (three rows of nine) +
    // the nine-slot hotbar row (four rows total, kGridRows).
    const int gridH = kGridRows * kSlot + (kGridRows - 1) * kGap;

    // The crafting section: the grid (2x2 = the inventory's top-left cells
    // of the 3x3 model, 3x3 for the table) + the output slot.
    const int gridCols = table ? 3 : 2;
    const int gridSectionW = gridCols * kSlot + (gridCols - 1) * kGap;
    const int sectionW = gridSectionW + kCraftGap + kSlot;
    // The recipe list (right of the output): two side-by-side columns of
    // kCraftWidth for BOTH panels — CRAFT-4's table geometry generalized
    // to the E panel (UX-8c: 32 shapeless rows in one column would shrink
    // the panel fit factor to ~0.43, the row text to unusably small at
    // ui.scale 2.0).
    const int listW = 2 * kCraftWidth + kRecipeColumnGap;
    const int nativePanelW = 2 * kPanelPadding + gridW + kCraftGap +
                             sectionW + kCraftGap + listW;
    // Rows per column (both kinds split recipeCount left/right: the left
    // column holds the first (recipeCount + 1) / 2 rows).
    const int leftRows = (recipeCount + 1) / 2;
    const int listH = kTitleHeight + leftRows * kCraftRowHeight;
    const int nativePanelH = 2 * kPanelPadding +
                             std::max(kTitleHeight + gridH, listH);

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
    // hotbar (slots 0..8, left to right); above it, three storage rows
    // hold slots 27..35 (top), 18..26 and 9..17 (UX-8f).
    for (int slot = 0; slot < Inventory::kTotalSize; ++slot) {
        const int row =
            (slot < Inventory::kHotbarSize)
                ? kStorageRows
                : (slot - Inventory::kHotbarSize) / kColumns;
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

    // The recipe list: both panels split their rows over the two columns
    // (the table's shaped hint rows, CRAFT-4, and the E panel's shapeless
    // craft rows, UX-8c — row i sits in the left column while
    // i < leftRows, the right one after).
    {
        const double craftX = outputX + slotLen + craftGap;
        const double colGap = kRecipeColumnGap * s;
        out.craftHeader = UIRect {static_cast<int>(std::lround(craftX)),
                                  static_cast<int>(std::lround(panelY + pad)),
                                  scaledLength(listW * s),
                                  scaledLength(titleH)};
        for (int i = 0; i < recipeCount; ++i) {
            const int column = (i >= leftRows) ? 1 : 0;
            const int rowIdx = (column == 0) ? i : i - leftRows;
            const double x = craftX + column * (craftW + colGap);
            out.recipes.push_back(UIRect {
                static_cast<int>(std::lround(x)),
                static_cast<int>(std::lround(panelY + pad + titleH +
                                             rowIdx * rowH)),
                scaledLength(craftW), scaledLength(rowH)});
        }
    }
    return out;
}

// UX-8c: the E panel's stable craftable-first partition (see the
// declaration). Two passes over the SAME input order: every canCraft
// row first, then the rest — registry order kept inside each group, so
// the order is a pure function of (hints, inv, items).
std::vector<const Recipe*> orderedRecipeHints(
    const std::vector<const Recipe*>& hints, const Inventory& inv,
    const ItemRegistry& items) {
    std::vector<const Recipe*> out;
    out.reserve(hints.size());
    for (const Recipe* r : hints) {
        if (canCraft(inv, *r, items)) {
            out.push_back(r);
        }
    }
    for (const Recipe* r : hints) {
        if (!canCraft(inv, *r, items)) {
            out.push_back(r);
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
    // UX-5: the material-repair fallback (match() fills m.repair only
    // when NO recipe matched — the recipe branch below is the winner).
    // The grid is not recipe-consumed (there is no recipe): the repair
    // runs in place (the tool's cell takes the reduced damage, one
    // material unit is spent) and the repaired tool — the anvil's
    // result — leaves the grid into the carry.
    if (m.repair.has_value()) {
        const ItemStack& tool = m_grid->at(m.repair->itemSlot);
        const Item& item = m_items.at(tool.item);
        // The carry guard (the recipe path's twin, the SURV-3
        // same-damage merge rule): a foreign carried stack, a
        // different-damage same item, or a stack that would overflow
        // maxStack all reject the click (every max-stack-1 tool
        // rejects ANY non-empty carry — 1 + 1 > 1).
        if (!m_carry.empty() &&
            (m_carry.item != tool.item ||
             m_carry.damage != m.repair->outputDamage ||
             m_carry.count + tool.count > item.maxStack)) {
            return false;
        }
        applyRepair(*m_grid, *m.repair, m_items);
        // Take the repaired tool out of its cell (the result slot): the
        // stack rides the cursor (merging into an equal carried stack —
        // the guard made the damage equal, so the assignment is a no-op
        // there), the cell empties. rightButton and left behave alike
        // (the tool count is the whole repairable unit).
        const ItemStack& repaired = m_grid->at(m.repair->itemSlot);
        m_carry.item = repaired.item;
        m_carry.count += repaired.count;
        m_carry.damage = repaired.damage;
        m_grid->setSlot(m.repair->itemSlot, ItemStack {});
        return true;
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

void InventoryUI::quickMove(int slot, int gridSlot, bool onOutput) {
    // CRAFT-7: shift-click (Minecraft's E-screen semantics). A carried
    // stack is never quick-moved (the carry is kept); the output slot
    // keeps the craft actions only.
    if (carrying() || onOutput) {
        return;
    }
    if (gridSlot >= 0) {
        // Grid cell -> the player inventory, storage first (Minecraft).
        moveStackIntoOrder(m_grid->at(gridSlot), m_inv, m_items,
                           inventorySlotOrder(m_inv, /*hotbarFirst=*/false));
        return;
    }
    if (slot >= 0) {
        // Hotbar slot -> storage; storage slot -> hotbar (index order).
        const std::vector<int> order = slot < m_inv.hotbarSize()
            ? inventorySlotOrder(m_inv, /*hotbarFirst=*/false)
            : inventorySlotOrder(m_inv, /*hotbarFirst=*/true);
        moveStackIntoOrder(m_inv.at(slot), m_inv, m_items, order);
    }
}

void InventoryUI::doubleCollect(int slot) {
    // CRAFT-7: double-click collect (player inventory only — the grid
    // and the output are not part of the collect; see the header).
    collectInventoryInto(m_inv, m_items, m_carry, slot);
}

// ---------------------------------------------------------------------------
// Phase 40: the furnace panel

FurnaceLayout FurnaceLayout::compute(int windowWidth, int windowHeight) {
    FurnaceLayout out;
    // Native (unscaled) design sizes: the slot grid is the inventory
    // screen's (9 columns x 4 rows, UX-8f's full 36 slots); the smelting
    // section replaces the crafting grid + output + recipe column.
    const int slot = InventoryLayout::kSlot;
    const int gap = InventoryLayout::kGap;
    const int pad = InventoryLayout::kPanelPadding;
    const int titleH = InventoryLayout::kTitleHeight;
    const int gridW = InventoryLayout::kColumns * slot +
                      (InventoryLayout::kColumns - 1) * gap;
    const int gridH = InventoryLayout::kGridRows * slot +
                      (InventoryLayout::kGridRows - 1) * gap;
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

    // The 36 inventory slots (the inventory screen's order: the hotbar
    // row 0..8 bottom, storage rows 9..17 / 18..26 / 27..35 above,
    // UX-8f).
    for (int sl = 0; sl < Inventory::kTotalSize; ++sl) {
        const int row =
            (sl < Inventory::kHotbarSize)
                ? InventoryLayout::kStorageRows
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

void FurnaceUI::quickMove(int slot, int furnaceSlot) {
    // CRAFT-7: shift-click (Minecraft's container-GUI semantics): only
    // a FURNACE slot quick-moves (into the player inventory, storage
    // first); a click on the player inventory is a no-op; a carried
    // stack is never quick-moved. Moving the input resets the progress
    // when the input item changes (the furnace.hpp invariant).
    if (slot >= 0 || furnaceSlot < 0 || carrying() || m_furnace == nullptr) {
        return;
    }
    const ItemStack before = m_furnace->input;
    moveStackIntoOrder(cellAt(furnaceSlot), m_inv, m_items,
                       inventorySlotOrder(m_inv, /*hotbarFirst=*/false));
    if (furnaceSlot == 0) {
        inputChanged(before);
    }
}

void FurnaceUI::doubleCollect(int slot) {
    // CRAFT-7: double-click collect (player inventory only — the
    // furnace slots are a separate Minecraft section, not collected).
    collectInventoryInto(m_inv, m_items, m_carry, slot);
}

// ---------------------------------------------------------------------------
// V2-12: the enchanting panel layout + UI (the furnace panel's twin).
// ---------------------------------------------------------------------------

EnchantLayout EnchantLayout::compute(int windowWidth, int windowHeight) {
    EnchantLayout out;
    const int slot = InventoryLayout::kSlot;
    const int gap = InventoryLayout::kGap;
    const int pad = InventoryLayout::kPanelPadding;
    const int titleH = InventoryLayout::kTitleHeight;
    const int gridW = InventoryLayout::kColumns * slot +
                      (InventoryLayout::kColumns - 1) * gap;
    const int gridH = InventoryLayout::kGridRows * slot +
                      (InventoryLayout::kGridRows - 1) * gap;
    // The section: the item + lapis slots side by side (left) + the three
    // offer rows (right column).
    const int offerColH = 3 * kOfferRowHeight + 2 * gap;
    const int sectionW = 2 * slot + gap + kOfferColumnGap + kOfferRowWidth;
    const int sectionH = std::max(offerColH, slot);
    const int nativePanelW = 2 * pad + gridW + InventoryLayout::kCraftGap +
                             sectionW;
    const int nativePanelH = 2 * pad + std::max(titleH + gridH, sectionH);

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
    out.panel = UIRect {(windowWidth - static_cast<int>(std::lround(panelW))) / 2,
                        (windowHeight - static_cast<int>(std::lround(panelH))) / 2,
                        scaledLength(panelW), scaledLength(panelH)};

    const double panelX = (windowWidth - panelW) / 2.0;
    const double panelY = (windowHeight - panelH) / 2.0;
    const double gridX = panelX + padLen;
    const double gridY = panelY + padLen + titleLen;
    out.title = UIRect {static_cast<int>(std::lround(gridX)),
                        static_cast<int>(std::lround(panelY + padLen)),
                        scaledLength(gW), scaledLength(titleLen)};

    // The 36 inventory slots (the inventory screen's order).
    for (int sl = 0; sl < Inventory::kTotalSize; ++sl) {
        const int row =
            (sl < Inventory::kHotbarSize)
                ? InventoryLayout::kStorageRows
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

    // The section: vertically centered against the slot rows.
    const double secX = gridX + gW + InventoryLayout::kCraftGap * s;
    const double secY = gridY + (gH - secH) / 2.0;
    // The item + lapis slots (vertically centered in the section).
    const double slotY = secY + (secH - slotLen) / 2.0;
    out.item = UIRect {static_cast<int>(std::lround(secX)),
                       static_cast<int>(std::lround(slotY)),
                       scaledLength(slotLen), scaledLength(slotLen)};
    out.lapis = UIRect {static_cast<int>(std::lround(secX + slotLen + gapLen)),
                        static_cast<int>(std::lround(slotY)),
                        scaledLength(slotLen), scaledLength(slotLen)};
    // The three offer rows (right column, top to bottom, centered).
    const double offerX = secX + 2 * (slotLen + gapLen) +
                          kOfferColumnGap * s;
    const double offerRowH = kOfferRowHeight * s;
    const double offerColHpx = 3 * offerRowH + 2 * gapLen;
    const double offerY0 = secY + (secH - offerColHpx) / 2.0;
    for (int r = 0; r < 3; ++r) {
        out.offers[r] = UIRect {static_cast<int>(std::lround(offerX)),
                                static_cast<int>(std::lround(
                                    offerY0 + r * (offerRowH + gapLen))),
                                scaledLength(kOfferRowWidth * s),
                                scaledLength(offerRowH)};
    }
    return out;
}

int EnchantLayout::slotAt(int px, int py) const {
    for (std::size_t i = 0; i < slots.size(); ++i) {
        if (slots[i].contains(px, py)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int EnchantLayout::panelSlotAt(int px, int py) const {
    if (item.contains(px, py)) {
        return 0;
    }
    if (lapis.contains(px, py)) {
        return 1;
    }
    for (int r = 0; r < 3; ++r) {
        if (offers[r].contains(px, py)) {
            return 2 + r;
        }
    }
    return -1;
}

// -- the EnchantUI state machine --------------------------------------------

void EnchantUI::leftClickCell(ItemStack& cell) {
    cellLeftClick(cell, m_carry, m_items);
}

void EnchantUI::rightClickCell(ItemStack& cell) {
    cellRightClick(cell, m_carry, m_items);
}

ItemStack& EnchantUI::cellAt(int panelSlot) {
    // panelSlot 0=item, 1=lapis (the caller guarantees the range).
    return panelSlot == 0 ? *m_itemSlot : *m_lapisSlot;
}

// The lapis slot's data-driven gate: only an `enchant_material` item may
// occupy it (the core never hardcodes "lapis"). A placed non-material is
// not accepted (the cell click is a no-op, the carry is kept — the
// furnace input's smeltable-gate twin).
bool EnchantUI::lapisAccepts(const ItemStack& stack) const {
    if (stack.empty()) {
        return true; // an empty placement (a pickup) is always allowed
    }
    return m_items.at(stack.item).enchantMaterial;
}

bool EnchantUI::leftClick(int slot, int panelSlot) {
    if (panelSlot >= 0) {
        const ItemStack before = *m_itemSlot;
        if (panelSlot == 1 && !lapisAccepts(m_carry)) {
            // A non-enchant-material in the hand cannot go to the lapis
            // slot (the gate); the click is consumed, the carry is kept.
            return false;
        }
        leftClickCell(cellAt(panelSlot));
        if (panelSlot == 0 && *m_itemSlot != before) {
            return true; // the item slot changed (the game re-rolls offers)
        }
        return false;
    }
    if (slot >= 0) {
        leftClickCell(m_inv.at(slot));
        return false;
    }
    m_carry = ItemStack {}; // click outside every slot: drop the stack
    return false;
}

bool EnchantUI::rightClick(int slot, int panelSlot) {
    if (panelSlot >= 0) {
        const ItemStack before = *m_itemSlot;
        if (panelSlot == 1 && !m_carry.empty() &&
            !lapisAccepts(m_carry)) {
            return false; // the gate (a non-material cannot enter the
                          // lapis slot, even one-at-a-time)
        }
        rightClickCell(cellAt(panelSlot));
        if (panelSlot == 0 && *m_itemSlot != before) {
            return true;
        }
        return false;
    }
    if (slot >= 0) {
        rightClickCell(m_inv.at(slot));
        return false;
    }
    return false; // right click outside the slots does nothing
}

void EnchantUI::quickMove(int slot, int panelSlot) {
    // CRAFT-7: shift-click — only a PANEL slot quick-moves (into the
    // player inventory, storage first); a click on the player inventory is
    // a no-op; a carried stack is never quick-moved.
    if (slot >= 0 || panelSlot < 0 || carrying() || m_itemSlot == nullptr) {
        return;
    }
    ItemStack& cell = cellAt(panelSlot);
    if (panelSlot == 1 && !lapisAccepts(cell)) {
        // (defensive: the lapis slot never holds a non-material)
        return;
    }
    moveStackIntoOrder(cell, m_inv, m_items,
                       inventorySlotOrder(m_inv, /*hotbarFirst=*/false));
}

// =========================================================================
// V3-11b: the creative tab (layout + state machine)
// =========================================================================

CreativeLayout CreativeLayout::compute(int windowWidth, int windowHeight,
                                       int recipeCount) {
    CreativeLayout out;
    // The shared outer: the E kind's compute (the recipeCount keeps the
    // 背包 tab's panel IDENTICAL to the survival's — the creative's
    // face is the title row's tabs + the 物品栏 tab's content only).
    const InventoryLayout inv =
        InventoryLayout::compute(windowWidth, windowHeight, recipeCount,
                                 PanelKind::Inventory);
    out.panel = inv.panel;
    const double s = inv.scale;
    const int pad = scaledLength(InventoryLayout::kPanelPadding * s);
    const int titleH = scaledLength(InventoryLayout::kTitleHeight * s);
    const int tabW = scaledLength(kTabWidth * s);
    const int tabH = scaledLength(kTabHeight * s);
    const int tabGap = scaledLength(kTabGap * s);
    // The tabs sit in the title row (vertically centered, left-aligned
    // like the title text).
    const int tx = inv.panel.x + pad;
    const int ty = inv.panel.y + pad + (titleH - tabH) / 2;
    out.tabItems = UIRect {tx, ty, tabW, tabH};
    out.tabBackpack = UIRect {tx + tabW + tabGap, ty, tabW, tabH};
    // The 物品栏 tab's content: the search box under the title row, then
    // the kGridColumns x kGridRows grid (left-aligned like the slots).
    const int searchW = scaledLength(kSearchWidth * s);
    const int searchH = scaledLength(kSearchHeight * s);
    const int searchGap = scaledLength(kSearchGap * s);
    const int searchY = inv.panel.y + pad + titleH + searchGap;
    out.search = UIRect {tx, searchY, searchW, searchH};
    const int cell = scaledLength(InventoryLayout::kSlot * s);
    const int gap = scaledLength(InventoryLayout::kGap * s);
    const int gridY = searchY + searchH + searchGap;
    for (int r = 0; r < kGridRows; ++r) {
        for (int c = 0; c < kGridColumns; ++c) {
            out.cells.push_back(UIRect {tx + c * (cell + gap),
                                        gridY + r * (cell + gap), cell,
                                        cell});
        }
    }
    return out;
}

int CreativeLayout::tabAt(int px, int py) const {
    if (tabItems.contains(px, py)) {
        return 0;
    }
    if (tabBackpack.contains(px, py)) {
        return 1;
    }
    return -1;
}

int CreativeLayout::cellAt(int px, int py) const {
    for (int i = 0; i < static_cast<int>(cells.size()); ++i) {
        if (cells[i].contains(px, py)) {
            return i;
        }
    }
    return -1;
}

namespace {
// The search's vocabulary: ASCII [a-z0-9_] (the item names' charset —
// an uppercased character lowercases, everything else is dropped).
bool isSearchChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
}
} // namespace

void CreativeTabUI::appendFrameText(const std::string& frame) {
    bool changed = false;
    for (const char c0 : frame) {
        if (m_text.size() >= kSearchTextMax) {
            break;
        }
        const char c = (c0 >= 'A' && c0 <= 'Z')
            ? static_cast<char>(c0 + ('a' - 'A'))
            : c0;
        if (isSearchChar(c)) {
            m_text.push_back(c);
            changed = true;
        }
    }
    if (changed) {
        m_dirty = true;
    }
}

void CreativeTabUI::backspace() {
    if (m_text.empty()) {
        return;
    }
    m_text.pop_back();
    m_dirty = true;
}

void CreativeTabUI::refreshFiltered() const {
    if (!m_dirty) {
        return;
    }
    m_dirty = false;
    m_filtered.clear();
    for (const ItemID id : m_items.allIds()) {
        const std::string& name = m_items.at(id).name;
        if (name.size() >= m_text.size() &&
            name.compare(0, m_text.size(), m_text) == 0) {
            m_filtered.push_back(id);
        }
    }
    std::sort(m_filtered.begin(), m_filtered.end(),
              [this](ItemID a, ItemID b) {
                  return m_items.at(a).name < m_items.at(b).name;
              });
    // The text can shrink the list under a scrolled window — re-clamp.
    const int limit = std::max(0, static_cast<int>(m_filtered.size()) -
                                       CreativeLayout::kVisible);
    if (m_scroll > limit) {
        m_scroll = limit;
    }
}

const std::vector<ItemID>& CreativeTabUI::filtered() const {
    refreshFiltered();
    return m_filtered;
}

void CreativeTabUI::scrollBy(int delta) {
    if (delta == 0) {
        return;
    }
    const int limit = std::max(0, static_cast<int>(filtered().size()) -
                                       CreativeLayout::kVisible);
    m_scroll += delta;
    if (m_scroll < 0) {
        m_scroll = 0;
    } else if (m_scroll > limit) {
        m_scroll = limit;
    }
}

bool CreativeTabUI::grant(int targetSlot, int cell, bool full) {
    if (targetSlot < 0 || cell < 0 ||
        cell >= CreativeLayout::kVisible) {
        return false;
    }
    const std::vector<ItemID>& list = filtered();
    const int idx = visibleStart() + cell;
    if (idx >= static_cast<int>(list.size())) {
        return false; // a window cell past the filtered tail
    }
    const ItemID id = list[idx];
    const Item& item = m_items.at(id);
    ItemStack& slot = m_inv.at(static_cast<std::size_t>(targetSlot));
    if (slot.item == id) {
        if (slot.count >= item.maxStack) {
            return false; // the slot is already full of the item
        }
        slot.count = std::min(item.maxStack,
                              slot.count + (full ? item.maxStack : 1));
        // A creative grant carries no damage / enchants (the fresh
        // stack's rule) — the merge keeps the item + count only.
        slot.damage = 0.0f;
        slot.enchants.clear();
        return true;
    }
    slot = ItemStack {id, full ? item.maxStack : 1, 0.0f, {}};
    return true;
}

} // namespace mc
