// Item and ItemStack: the data model of the inventory system (Phase 11).
//
// An Item is pure data, like a Block: identity (name + stable ItemID), a
// type, and type-specific behavior fields. Core systems read these fields
// and never switch on item identity.
//
// Three item kinds:
//   - Block items: one implicit item per data block (same name as the
//     block); placing one puts its block in the world, and it renders as
//     that block's cube when dropped. They are built from the
//     BlockRegistry (ItemRegistry::buildBlockItems) — there is no separate
//     JSON for them, the block definition is the single source of truth.
//   - Tool items: explicit data (assets/data/items/*.json); they carry a
//     break speed multiplier used by the hold-to-break rule.
//   - Material items: explicit data (assets/data/items/*.json); generic
//     non-placeable items (ingots, planks, sticks, ...) that are crafting
//     inputs/outputs and block drops (e.g. ore -> ingot).
//
// Tools and materials share one property: no in-world representation
// (dropped ones are invisible entities — see docs/KNOWN_ISSUES.md). All
// non-block-item code paths (placement, rendering, break speed) dispatch
// on ItemType, so adding a new explicit kind is data + one enum value.
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
    Block,    // placeable as its block (implicit per data block)
    Tool,     // affects break speed (explicit data)
    Material, // generic non-placeable item (explicit data)
};

struct Item {
    ItemID id = ItemID::invalid();
    std::string name;
    ItemType type = ItemType::Block;
    // ItemType::Block only: the block that is placed and rendered.
    BlockID block = BlockID::invalid();
    // ItemType::Tool only: break-speed multiplier (hand, block items and
    // materials break at 1.0).
    float breakSpeed = 1.0f;
    // Maximum count in one stack (1 for tools, 64 for blocks/materials by
    // default).
    int maxStack = 64;
    // Phase 20: melee damage when the player swings with this item
    // (optional data field 'attack_damage'; default 1.0 = bare hand /
    // block items / materials).
    float attackDamage = 1.0f;
    // Phase 21: projectile launching (optional data fields 'shoot_speed'
    // / 'shoot_damage'). shootSpeed > 0 marks the item as a LAUNCHER
    // (the bow). Both default 0 = not a launcher (every pre-Phase-21
    // item).
    float shootSpeed = 0.0f;
    float shootDamage = 0.0f;
    // Phase 24: the launcher's AMMO item (optional data field 'ammo', an
    // item name). Empty = the launcher fires without consuming anything
    // (every pre-Phase-24 launcher). While a launcher is selected, the
    // right button draws the bow and the release fires ONE arrow only if
    // the inventory still holds at least one `ammo` item; firing consumes
    // one from the lowest-index slot that holds it (see
    // Game::fireBow). The name is cross-checked against the ItemRegistry
    // at game init (same pattern as the icon file check).
    std::string ammo;
    // Phase 23: food (optional data field 'restore_hunger', integer in
    // [1, 20]). restoreHunger > 0 marks the item as FOOD: while it is
    // selected, the right button eats one (restores this much hunger and
    // consumes the stack entry) instead of placing/interacting. 0 = not
    // food (every pre-Phase-23 item; explicit 0 is rejected as a typo,
    // the absent field is the "not food" default).
    int restoreHunger = 0;
    // Phase 40: furnace smelting (optional data fields 'smeltable_to' /
    // 'smelt_time'). smeltableTo non-empty marks the item as SMELTABLE:
    // it names the product item (cross-checked against the ItemRegistry
    // at game init — the ammo pattern). smeltTime (seconds, (0, 100]) is
    // how long one unit takes; default 10.0 (Minecraft's furnace time),
    // only meaningful when smeltable.
    std::string smeltableTo;
    float smeltTime = 10.0f;
    // Phase 40: furnace fuel (optional data field 'fuel_time'). fuelTime
    // (seconds, (0, 1000]) is how long one unit burns; 0 = not fuel (the
    // absent-field default; an explicit 0 is rejected as a typo, the
    // restore_hunger discipline).
    float fuelTime = 0.0f;
    // Phase 41: dye (optional data field 'dye_color'). dyeColor non-empty
    // marks the item as a DYE of that color: right-clicking it on a
    // dye-family block (BlockProperties::dyeBase) recolors the block to
    // dyeVariantName(base, color) and consumes one dye. The color must
    // exist as a dye item color for every dye-family base block (the
    // game-init cross-check keeps the family complete — see
    // Game::init).
    std::string dyeColor;
    // Phase 43: drinkable container (optional data field 'drink_time').
    // drinkTime (seconds, (0, 10]) > 0 marks the item as DRINKABLE:
    // right-clicking it selected starts the shared consume channel (the
    // eat channel, generalized to a data-driven duration); completing it
    // consumes one, plays the "milk" sound, returns `returnsItem` to the
    // inventory (when set) and clears the player's status effects (v1:
    // the hurt flash). Mutually exclusive with 'restore_hunger' (an item
    // is either food or a drinkable, not both).
    float drinkTime = 0.0f;
    // Phase 43: the container a completed drink RETURNS to the inventory
    // (optional data field 'returns_item', a non-empty item name). Only
    // valid on a drinkable item (rejected otherwise); the name's
    // existence is cross-checked at game init (the ammo pattern).
    std::string returnsItem;
    // Phase 49: status effect granted when a full EAT completes (optional
    // data field 'grants_effect', an object {"name": <effect>, "duration":
    // <seconds>} — the on_hit_effect pair discipline). grantsEffect non-empty
    // marks the item as granting that effect on eat (the golden apple grants
    // regeneration). The effect name is cross-checked against the
    // EffectRegistry at game init (the ammo pattern). Only valid on FOOD
    // (restoreHunger > 0): a drinkable CLEARS effects on drink, so it cannot
    // also grant one (mutual exclusion enforced at parse).
    std::string grantsEffect;
    double grantsEffectDuration = 0.0;
    // Phase 56: the LIST form of the eat-grant (optional data field
    // 'grants_effects', an array of the same strict {"name", "duration"}
    // pairs). Mutually exclusive with the single 'grants_effect' (the P52
    // chance_drops pattern); the entries are applied IN ORDER when a full
    // eat completes (the golden apple: regeneration + absorption). Same
    // food-only and init cross-check disciplines as the single field.
    struct GrantEffect {
        std::string name;
        double duration = 0.0;
    };
    std::vector<GrantEffect> grantsEffects;
    // Phase 47 (kinds in Phase 51): fluid bucket, COLLECT side (optional
    // data field 'collects_fluid', a non-empty ARRAY of fluid names —
    // vocabulary: "water", "lava"). Non-empty marks the item as an EMPTY
    // bucket that scoops each named fluid: right-clicking a level-8
    // (source) cell of one of those fluids in reach removes the source and
    // swaps one held item for the FULL bucket of that fluid — the unique
    // item that pours the fluid back into this one (derived; the pair is
    // cross-checked at game init, the ammo pattern). One empty bucket
    // therefore scoops every fluid it names (Minecraft semantics).
    // Mutually exclusive with poursFluid (one item is one side of the
    // pair) and with food/drinkable (the right-click channel owner).
    std::vector<std::string> collectsFluids;
    // Phase 47: fluid bucket, POUR side (optional data fields
    // 'pours_fluid' + 'pours_becomes'). poursFluid non-empty marks the
    // item as a FULL bucket of the named fluid: right-clicking a valid
    // air cell (the placement-target face cell, no fluid, not
    // overlapping the player) places a level-8 source and swaps one held
    // item for one 'poursBecomes' item (the empty bucket). Same
    // exclusivity as the collect side.
    std::string poursFluid;
    std::string poursBecomes;
    // Phase 15 UI: the item's 2D icon texture name (a file under
    // assets/textures/items/, validated against the filesystem at game
    // init). Empty for block items, which render a 3D block cube instead.
    std::string icon;
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
// vs stone, ...); the flat multiplier is the v1 — see docs/KNOWN_ISSUES.md.
inline constexpr double kBreakBaseSeconds = 1.5;

inline double breakDurationSeconds(float hardness, float breakSpeed) {
    if (hardness <= 0.0f) {
        return 0.0;
    }
    return kBreakBaseSeconds * static_cast<double>(hardness) /
           static_cast<double>(breakSpeed);
}

} // namespace mc
