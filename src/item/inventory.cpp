#include "item/inventory.hpp"

#include <algorithm>
#include <cassert>

namespace mc {

const ItemStack& Inventory::at(std::size_t slot) const {
    assert(slot < m_slots.size());
    return m_slots[slot];
}

int Inventory::add(ItemID item, int count, const ItemRegistry& items) {
    if (!item.valid() || count <= 0) {
        return -1;
    }
    const int maxStack = items.at(item).maxStack;

    // 1) Merge into an existing stack of the same item (ascending slot
    //    order — the hotbar is searched before storage).
    for (std::size_t i = 0; i < m_slots.size(); ++i) {
        ItemStack& s = m_slots[i];
        if (s.item == item && s.count + count <= maxStack) {
            s.count += count;
            return static_cast<int>(i);
        }
    }
    // 2) First empty slot.
    for (std::size_t i = 0; i < m_slots.size(); ++i) {
        if (m_slots[i].empty()) {
            m_slots[i] = ItemStack {item, count};
            return static_cast<int>(i);
        }
    }
    return -1;
}

int Inventory::take(std::size_t slot, int n) {
    assert(slot < m_slots.size());
    ItemStack& s = m_slots[slot];
    if (s.empty()) {
        return 0;
    }
    const int removed = std::min(n, s.count);
    s.count -= removed;
    if (s.count <= 0) {
        s = ItemStack {};
    }
    return removed;
}

void Inventory::setSlot(std::size_t slot, const ItemStack& stack) {
    assert(slot < m_slots.size());
    m_slots[slot] = stack;
}

bool Inventory::isFull(ItemID item, const ItemRegistry& items) const {
    if (!item.valid()) {
        return true;
    }
    const int maxStack = items.at(item).maxStack;
    for (const ItemStack& s : m_slots) {
        if (s.empty() || (s.item == item && s.count < maxStack)) {
            return false; // room in an empty slot or a partial stack
        }
    }
    return true;
}

} // namespace mc
