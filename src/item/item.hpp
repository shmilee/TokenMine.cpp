// Item and ItemStack: the data model of the inventory system (Phase 11).
//
// An Item is pure data, like a Block: identity (name + stable ItemID), a
// type, and type-specific behavior fields. Core systems read these fields
// and never switch on item identity.
//
// Two item kinds:
//   - Block items: one implicit item per data block (same name as the
//     block); placing one puts its block in the world, and it renders as
//     that block's cube when dropped. They are built from the
//     BlockRegistry (ItemRegistry::buildBlockItems) — there is no separate
//     JSON for them, the block definition is the single source of truth.
//   - Tool items: explicit data (assets/data/items/*.json); they carry a
//     break speed multiplier used by the hold-to-break rule.
//
// An ItemStack is an (item, count) pair; an empty slot is a stack with an
// invalid item and/or zero count.
#pragma once

#include <string>

#include "block/block.hpp"

namespace mc {

// Stable handle into an ItemRegistry. Invalid = "no item".
struct ItemID {
    int value = -1;

    constexpr bool valid() const { return value >= 0; }
    constexpr bool operator==(ItemID o) const { return value == o.value; }
    constexpr bool operator!=(ItemID o) const { return value != o.value; }
    static constexpr ItemID invalid() { return {-1}; }
};

enum class ItemType {
    Block, // placeable as its block (implicit per data block)
    Tool,  // affects break speed (explicit data)
};

struct Item {
    ItemID id = ItemID::invalid();
    std::string name;
    ItemType type = ItemType::Block;
    // ItemType::Block only: the block that is placed and rendered.
    BlockID block = BlockID::invalid();
    // ItemType::Tool only: break-speed multiplier (hand and block items
    // break at 1.0).
    float breakSpeed = 1.0f;
    // Maximum count in one stack (1 for tools, 64 for blocks by default).
    int maxStack = 64;
};

struct ItemStack {
    ItemID item = ItemID::invalid();
    int count = 0;

    bool empty() const { return count <= 0 || !item.valid(); }
    bool operator==(const ItemStack& o) const {
        return item == o.item && count == o.count;
    }
};

// Breaking rule (Phase 11): the time it takes to break a block with the
// currently selected item is
//     kBreakBaseSeconds × hardness / breakSpeed
// where hardness is the block's data value and breakSpeed the selected
// item's (hand and block items = 1.0). A hardness of 0 or below breaks
// instantly.
//
// Minecraft refines this with per-block-type tool effectiveness (pickaxes
// vs stone, ...); the flat multiplier is the v1 — see KNOWN_ISSUES.md.
inline constexpr double kBreakBaseSeconds = 1.5;

inline double breakDurationSeconds(float hardness, float breakSpeed) {
    if (hardness <= 0.0f) {
        return 0.0;
    }
    return kBreakBaseSeconds * static_cast<double>(hardness) /
           static_cast<double>(breakSpeed);
}

} // namespace mc
