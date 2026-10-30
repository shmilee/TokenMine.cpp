#include "ui/item_tooltip.hpp"

#include <cstdio>

namespace mc {

ItemTooltip itemTooltip(const Item& item, const ItemStack& stack) {
    ItemTooltip tip;
    tip.title = item.name;
    if (stack.count > 1) {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "%s x%d", item.name.c_str(),
                      stack.count);
        tip.title = buf;
    }
    if (item.durability > 0.0f) {
        // The same d/max readout as the F3 held line (full durability
        // shows 0/max — the tooltip, unlike the HUD strip, is text).
        char buf[96];
        std::snprintf(buf, sizeof(buf), "%.1f/%.0f", stack.damage,
                      item.durability);
        tip.durability = buf;
    }
    return tip;
}

} // namespace mc
