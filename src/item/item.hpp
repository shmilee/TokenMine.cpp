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
// An ItemStack is an (item, count, damage) triple (the damage is the
// SURV-3 accumulated durability loss, 0 for items without durability);
// an empty slot is a stack with an invalid item and/or zero count.
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

// SURV-1 (armor): the four equipment slots, in the FIXED ORDER — the
// unequip sequence, the HUD row (left to right) and the save's write
// order. 0 = head, 1 = chest, 2 = legs, 3 = feet (Minecraft).
inline constexpr int kArmorSlotCount = 4;
inline const char* const kArmorSlotNames[kArmorSlotCount] = {
    "head", "chest", "legs", "feet"};
// The index of the named slot, -1 for an unknown name (the parse's
// vocabulary check — a typo fails loudly instead of equipping nothing).
inline int armorSlotIndex(const std::string& name) {
    for (int i = 0; i < kArmorSlotCount; ++i) {
        if (name == kArmorSlotNames[i]) {
            return i;
        }
    }
    return -1;
}
// The damage formula's constants (Minecraft, see Player::damage): each
// armor point reduces the incoming damage by 4 PERCENT points, and the
// equipped total caps at kArmorMaxPoints (the full diamond set is 3+6+
// 3+2 = 20).
inline constexpr float kArmorReductionPerPoint = 4.0f; // percent / point
inline constexpr int kArmorMaxPoints = 20;

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
    // Phase 77: the launcher's INFINITY (optional data field 'infinite',
    // a strict bool, default false; parse requires a non-empty 'ammo' —
    // without one, firing already consumes nothing, so the flag would be
    // a no-op typo). Minecraft's bow Infinity enchantment, data-driven
    // here: firing consumes NO ammo while the inventory holds at least
    // one `ammo` item (the shot is still gated on holding one — the one
    // arrow is never spent, see Game::fireBow).
    bool infinite = false;
    // SURV-5: the launcher's ARROW EFFECT (optional data field
    // 'arrow_effect', the strict {name, duration} pair — the
    // on_hit_effect discipline). Non-empty marks the launcher's arrows as
    // carrying the named status effect for `arrowEffectDuration` seconds:
    // the game resolves it to an id at fire time and the PROJECTILE
    // applies it to the hit MOB (Game::fireBow; a player's bow arrow
    // never damages the player, so the effect never reaches the player).
    // Only a LAUNCHER (shoot_speed > 0) may carry it — on any other item
    // the field would be dead (the ammo discipline). The effect name's
    // existence AND its `affects_mobs` flag are cross-checked at game
    // init (the ammo pattern). Empty = no arrow effect (every
    // pre-SURV-5 launcher — the shipped poison_bow is the first carrier).
    std::string arrowEffectName;
    double arrowEffectDuration = 0.0;
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
    // Phase 49 (drinkable in SURV-4): status effect granted when a full
    // CONSUME completes (optional data field 'grants_effect', an object
    // {"name": <effect>, "duration": <seconds>} — the on_hit_effect pair
    // discipline). grantsEffect non-empty marks the item as granting that
    // effect on consume (the golden apple grants regeneration; the SURV-4
    // potions grant their effect on a completed drink). The effect name is
    // cross-checked against the EffectRegistry at game init (the ammo
    // pattern). Only valid on FOOD (restoreHunger > 0) or a DRINKABLE
    // (drinkTime > 0) — the two consume channels; a non-consumable carrying
    // the field is a dead-field typo (mutual exclusion enforced at parse).
    // A duration of 0 marks an instant effect (settled on apply, no
    // lingering) — the 0 ⇔ instant equivalence is checked at game init.
    std::string grantsEffect;
    double grantsEffectDuration = 0.0;
    // Phase 56 (drinkable in SURV-4): the LIST form of the consume-grant
    // (optional data field 'grants_effects', an array of the same strict
    // {"name", "duration"} pairs). Mutually exclusive with the single
    // 'grants_effect' (the P52 chance_drops pattern); the entries are applied
    // IN ORDER when a full consume completes (the golden apple: regeneration
    // + absorption; the SURV-4 potions: their single effect). Same
    // food-or-drinkable and init cross-check disciplines as the single field.
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
    // Phase 60: flammable ignition (optional data field
    // 'ignites_flamable', a strict bool, default false). true marks the
    // item as an IGNITER: right-clicking a `flamable` block under the
    // crosshair burns it to air (NO drops — the Phase 58 lava-burn
    // semantics) and the item is NOT consumed (Minecraft: the flint and
    // steel survives its uses). Mutually exclusive with every other
    // right-click channel (food / drinkable / launcher / dye / fluid
    // bucket — the channel-ownership discipline). v1 scope: blocks only
    // (mobs still ignite from lava alone, Phase 58).
    bool ignitesFlamable = false;
    // Phase 67: per-block break efficiency (optional data fields
    // 'fast_against', a non-empty ARRAY of block names, +
    // 'fast_against_multiplier', a number > 0 — a STRICT pair: one
    // without the other is rejected, as is the empty list, and the
    // fields are only valid on type "tool"). Non-empty fastAgainst
    // marks the tool as FAST AGAINST the named blocks: breaking one of
    // them takes 1/fastAgainstMultiplier of the tool's normal break
    // time (Minecraft: axes chop wood 2x faster — the first per-block
    // tool category in this project, the KNOWN_ISSUES "no axe tools"
    // + "no per-block tool efficiency" entries). fastAgainst empty =
    // not fast against anything (every pre-Phase-67 item). Each name's
    // existence is cross-checked against the BlockRegistry at game
    // init (the grows_on discipline — a typo fails init loudly).
    std::vector<std::string> fastAgainst;
    float fastAgainstMultiplier = 0.0f;
    // SURV-1 (armor): the equipment slot (optional data fields
    // 'armor_slot' + 'armor_points', a STRICT pair, material-only — the
    // fast_against pairing discipline). armorSlot non-empty (a name from
    // the kArmorSlotNames vocabulary) marks the item as an ARMOR piece:
    // right-clicking with it selected equips it onto the player's slot
    // (the Game::tryArmor channel — an occupied slot swaps the piece
    // with the held one), an empty hand unequips the first worn slot
    // (the fixed order). The piece owns the right-click channel, so it
    // is mutually exclusive with every other channel owner (food /
    // drinkable / launcher / dye / fluid bucket / igniter — enforced at
    // parse). armorPoints is the piece's contribution to the damage
    // reduction (Minecraft's integer-floor formula in Player::damage;
    // the equipped total caps at kArmorMaxPoints). max_stack 1
    // (Minecraft: armor never stacks).
    std::string armorSlot;
    int armorPoints = 0;
    // SURV-3 (durability): the max accumulated damage before the item
    // breaks (an optional data field 'durability', a strict number > 0).
    // 0 = the item has no durability (the absent-field default; it never
    // wears and never breaks — every pre-SURV-3 item). The ALLOWED set is
    // the "has a use action" items: tools (mining / melee wear), armor
    // pieces (damage-reduction wear), ammo launchers (the bow family —
    // shot wear) and igniters (flint and steel — ignite wear); any other
    // item carrying the field is rejected at parse (the ammo discipline:
    // a no-op field is a typo). The value is a FLOAT like ItemStack::
    // damage (Minecraft accumulates fractional armor wear, e.g. 2.5).
    float durability = 0.0f;
    // Phase 15 UI: the item's 2D icon texture name (a file under
    // assets/textures/items/, validated against the filesystem at game
    // init). Empty for block items, which render a 3D block cube instead.
    std::string icon;
};

struct ItemStack {
    ItemID item = ItemID::invalid();
    int count = 0;
    // SURV-3 (durability): the accumulated damage (0 = full; the item
    // breaks when this reaches its data `durability` — the broken stack
    // is removed, so a saved/held stack always obeys
    // `damage < item.durability`). Copy semantics carry it: every stack
    // move (pick-and-place, Q drop, equip/swap/unequip, ground pickup)
    // moves the damage with the stack. Items without durability keep it
    // at 0 forever (the wear events are data-gated).
    float damage = 0.0f;

    bool empty() const { return count <= 0 || !item.valid(); }
    bool operator==(const ItemStack& o) const {
        return item == o.item && count == o.count && damage == o.damage;
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
// vs stone, axes vs wood, ...): Phase 67 adds the data-driven
// fast_against list (an item multiplies its breakSpeed for the blocks it
// names); the per-category rules beyond that (shovel vs dirt, the
// "inefficient tool is 10x slower" rule) are still the v1 — see
// docs/KNOWN_ISSUES.md.
inline constexpr double kBreakBaseSeconds = 1.5;

// The effective break-speed multiplier of `item` against `target`
// (Phase 67): 1.0 for the hand, block items and materials; for a tool,
// its own breakSpeed — times the fast_against multiplier when the target
// block is one the tool is fast against (Minecraft axes chop wood and
// leaves 2x faster; pickaxes stay flat).
inline float effectiveBreakSpeed(const Item& item, const Block& target) {
    if (item.type != ItemType::Tool) {
        return 1.0f;
    }
    float speed = item.breakSpeed;
    if (item.fastAgainstMultiplier > 0.0f) {
        for (const std::string& name : item.fastAgainst) {
            if (name == target.name) {
                speed *= item.fastAgainstMultiplier;
                break;
            }
        }
    }
    return speed;
}

inline double breakDurationSeconds(float hardness, float breakSpeed) {
    if (hardness <= 0.0f) {
        return 0.0;
    }
    return kBreakBaseSeconds * static_cast<double>(hardness) /
           static_cast<double>(breakSpeed);
}

} // namespace mc
