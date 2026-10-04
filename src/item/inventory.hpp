// Inventory: fixed-slot container of ItemStacks (Phase 11).
//
// Layout: the first kHotbarSize slots are the hotbar (Tab cycles them;
// right-click places the selected one; Q drops one from the selected
// one). The remaining slots are storage: pickup fills them when the
// hotbar has no room. Storage slots are invisible until the inventory UI
// lands in Phase 15 (items that end up there are still held by the
// player, just not selectable yet).
//
// add() is deterministic: it first merges into an existing stack of the
// same item (slots in ascending order, so the hotbar is searched before
// storage), then places into the first empty slot (same order).
// Precondition: count <= the item's max stack (the game guarantees it —
// dropped items cap at their stack size and Q-drops take one at a time).
#pragma once

#include <cstddef>
#include <vector>

#include "item/item.hpp"
#include "item/item_registry.hpp"

namespace mc {

class Inventory {
public:
    static constexpr int kHotbarSize = 9;  // Minecraft's hotbar
    static constexpr int kTotalSize = 27;  // 9 hotbar + 18 storage

    explicit Inventory(
        std::size_t size = static_cast<std::size_t>(kTotalSize))
        : m_slots(size) {}

    std::size_t size() const { return m_slots.size(); }
    int hotbarSize() const { return kHotbarSize; }

    const ItemStack& at(std::size_t slot) const; // asserts in range
    bool isEmpty(std::size_t slot) const { return m_slots[slot].empty(); }

    // Merges `count` of `item` as described above. Returns the slot the
    // items landed in, or -1 when there is no room (every stack of the
    // item is full and no slot is empty).
    int add(ItemID item, int count, const ItemRegistry& items);

    // Removes up to `n` from the slot (clamped to what is there). Returns
    // the number actually removed; the slot becomes empty at zero.
    int take(std::size_t slot, int n);

    // Sets a slot (initialization only — the game fills the starting
    // hotbar this way).
    void set(std::size_t slot, const ItemStack& stack);

    // True when a full stack of `item` would not fit (no empty slot and
    // every stack of the item is full).
    bool isFull(ItemID item, const ItemRegistry& items) const;

private:
    std::vector<ItemStack> m_slots;
};

} // namespace mc
