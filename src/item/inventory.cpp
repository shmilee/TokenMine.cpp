#include "item/inventory.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace mc {

const ItemStack& Inventory::at(std::size_t slot) const {
    assert(slot < m_slots.size());
    return m_slots[slot];
}

ItemStack& Inventory::at(std::size_t slot) {
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

Result<std::vector<StarterSlot>> parseStarterSlots(const Json& hotbar,
                                                   const ItemRegistry& items) {
    std::vector<StarterSlot> out;
    // Absent section = no starter (the pre-GEN-2 hotbar.json shape).
    const Json* inv = hotbar.find("inventory");
    if (inv == nullptr) {
        return out;
    }
    if (!inv->isArray()) {
        return MC_FAIL("hotbar.json: 'inventory' must be an array of "
                       "{\"slot\": ..., \"item\": ..., \"count\": ...} "
                       "entries");
    }
    std::vector<int> usedSlots;
    for (std::size_t i = 0; i < inv->size(); ++i) {
        const Json& e = inv->at(i);
        const std::string at = "hotbar.json: inventory entry #" +
                               std::to_string(i);
        if (!e.isObject()) {
            return MC_FAIL(at + " must be an object {\"slot\": ..., "
                           "\"item\": ..., \"count\": ...}");
        }
        for (const auto& [key, value] : e.members()) {
            if (key != "slot" && key != "item" && key != "count") {
                return MC_FAIL(at + " has an unknown field '" + key + "'");
            }
        }
        const Json* slotJson = e.find("slot");
        const Json* itemJson = e.find("item");
        const Json* countJson = e.find("count");
        if (slotJson == nullptr || itemJson == nullptr ||
            countJson == nullptr) {
            return MC_FAIL(at + " requires the strict triple "
                           "{slot, item, count}");
        }
        const bool slotInt = slotJson->isNumber() &&
            slotJson->asDouble() == std::floor(slotJson->asDouble());
        const int slot =
            slotInt ? static_cast<int>(slotJson->asDouble()) : 0;
        if (!slotInt || slot < Inventory::kHotbarSize ||
            slot >= Inventory::kTotalSize) {
            return MC_FAIL(at + " slot must be a storage slot in [" +
                           std::to_string(Inventory::kHotbarSize) + ", " +
                           std::to_string(Inventory::kTotalSize - 1) +
                           "] (the hotbar slots are the 'items' fill)");
        }
        if (std::find(usedSlots.begin(), usedSlots.end(), slot) !=
            usedSlots.end()) {
            return MC_FAIL(at + " duplicates storage slot " +
                           std::to_string(slot));
        }
        usedSlots.push_back(slot);
        if (!itemJson->isString() || itemJson->asString().empty()) {
            return MC_FAIL(at + " item must be a non-empty item name");
        }
        const ItemID iid = items.idForName(itemJson->asString());
        if (!iid.valid()) {
            return MC_FAIL(at + " names unknown item '" +
                           itemJson->asString() + "'");
        }
        const bool countInt = countJson->isNumber() &&
            countJson->asDouble() == std::floor(countJson->asDouble());
        const int count =
            countInt ? static_cast<int>(countJson->asDouble()) : 0;
        if (!countInt || count < 1 || count > items.at(iid).maxStack) {
            return MC_FAIL(at + " count must be in [1, " +
                           std::to_string(items.at(iid).maxStack) +
                           "] (the item's max stack)");
        }
        out.push_back(StarterSlot {slot, iid, count});
    }
    return out;
}

} // namespace mc
