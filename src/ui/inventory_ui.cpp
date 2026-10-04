#include "ui/inventory_ui.hpp"

#include <algorithm>

namespace mc {

InventoryLayout InventoryLayout::compute(int windowWidth, int windowHeight,
                                         int recipeCount) {
    InventoryLayout out;
    const int gridW = kColumns * kSlot + (kColumns - 1) * kGap;
    // 27 slots = 18 storage (two rows of nine) + the nine-slot hotbar row.
    const int gridH = 3 * kSlot + 2 * kGap;
    const int panelW = 2 * kPanelPadding + gridW + kSectionGap + kCraftWidth;
    const int craftH =
        kTitleHeight + recipeCount * kCraftRowHeight;
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

    const int craftX = gridX + gridW + kSectionGap;
    out.craftHeader = UIRect {craftX, out.panel.y + kPanelPadding,
                              kCraftWidth, kTitleHeight};
    for (int i = 0; i < recipeCount; ++i) {
        out.recipes.push_back(UIRect {
            craftX, out.panel.y + kPanelPadding + kTitleHeight +
                        i * kCraftRowHeight,
            kCraftWidth, kCraftRowHeight});
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

int InventoryLayout::recipeRowAt(int px, int py) const {
    for (std::size_t i = 0; i < recipes.size(); ++i) {
        if (recipes[i].contains(px, py)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// ---------------------------------------------------------------------------

void InventoryUI::leftClick(int slot) {
    if (slot < 0) {
        m_carry = ItemStack {}; // click outside the slots: drop the stack
        return;
    }
    const ItemStack cur = m_inv.at(slot); // copy: the slot is about to change
    if (m_carry.empty()) {
        if (cur.empty()) {
            return;
        }
        m_carry = cur;
        m_inv.setSlot(slot, ItemStack {});
        return;
    }
    if (cur.empty()) {
        m_inv.setSlot(slot, m_carry);
        m_carry = ItemStack {};
        return;
    }
    if (cur.item == m_carry.item) {
        const int maxStack = m_items.at(cur.item).maxStack;
        const int room = maxStack - cur.count;
        const int moved = std::min(m_carry.count, room);
        if (moved > 0) {
            m_inv.setSlot(slot, ItemStack {cur.item, cur.count + moved});
            m_carry.count -= moved;
            if (m_carry.count <= 0) {
                m_carry = ItemStack {};
            }
        }
        // No room and nothing moved: the stack stays in the hand.
        return;
    }
    // Different item: swap the slot's stack with the carried one.
    m_inv.setSlot(slot, m_carry);
    m_carry = cur;
}

void InventoryUI::rightClick(int slot) {
    if (slot < 0) {
        return;
    }
    const ItemStack cur = m_inv.at(slot);
    if (m_carry.empty()) {
        if (cur.empty()) {
            return;
        }
        m_carry = ItemStack {cur.item, 1};
        m_inv.take(slot, 1);
        return;
    }
    if (cur.empty()) {
        m_inv.setSlot(slot, ItemStack {m_carry.item, 1});
        m_carry.count -= 1;
        if (m_carry.count <= 0) {
            m_carry = ItemStack {};
        }
        return;
    }
    if (cur.item == m_carry.item) {
        const int maxStack = m_items.at(cur.item).maxStack;
        if (cur.count < maxStack) {
            m_inv.setSlot(slot, ItemStack {cur.item, cur.count + 1});
            m_carry.count -= 1;
            if (m_carry.count <= 0) {
                m_carry = ItemStack {};
            }
        }
        return;
    }
    // Different item, occupied slot: no-op (minimal v1 semantics).
}

} // namespace mc
