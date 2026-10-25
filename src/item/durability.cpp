#include "item/durability.hpp"

#include <algorithm>

#include "item/item_registry.hpp"

namespace mc {

bool applyDurability(ItemStack& stack, float wear, float durability) {
    if (stack.empty() || wear <= 0.0f || durability <= 0.0f) {
        return false; // empty / no wear / the item has no durability
    }
    stack.damage += wear;
    if (stack.damage >= durability) {
        stack = ItemStack {}; // the BREAK: the stack is removed (no drop)
        return true;
    }
    return false;
}

bool applyDurability(ItemStack& stack, float wear, const ItemRegistry& items) {
    return applyDurability(stack, wear, items.at(stack.item).durability);
}

float armorWearPerPiece(float incoming) {
    return std::max(kArmorWearMin, incoming / kArmorWearPerIncoming);
}

} // namespace mc
