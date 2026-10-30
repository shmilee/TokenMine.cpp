// Registry of all item definitions (Phase 11).
//
// Two sources:
//   - implicit block items: one per data block (same name as the block),
//     built from the BlockRegistry via buildBlockItems(). Run this BEFORE
//     loadDirectory so an explicit item colliding with a block name is
//     rejected;
//   - explicit items (tools and materials): assets/data/items/*.json, one
//     file per item:
//
// {
//   "id": "wooden_pickaxe",
//   "type": "tool",
//   "tool": { "break_speed": 2.0 },   // tools only; > 0, default 1.0
//   "max_stack": 1                    // 1..64, default 64
// }
//
// Materials have no tool-specific fields: { "id": "stick", "type": "material" }.
//
// Validation is strict (same discipline as the block registry): type
// errors, out-of-range values and duplicate ids all produce descriptive
// errors naming the item. Unknown fields are tolerated (with a warning)
// for forward compatibility.
#pragma once

#include <cstddef>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

#include "block/block_registry.hpp"
#include "core/asset_manager.hpp"
#include "core/error.hpp"
#include "core/json.hpp"
#include "item/item.hpp"

namespace mc {

class ItemRegistry {
public:
    ItemRegistry() = default;

    // Registers one implicit block item per data block in `blocks` (the
    // air sentinel is skipped — it is a code sentinel, not a data block).
    Result<void> buildBlockItems(const BlockRegistry& blocks);

    // Parse and register one explicit item. Assigns the next free id.
    Result<ItemID> registerItem(const Json& def);

    // Load and register every *.json file in `relativeDir` under the asset
    // root (sorted for determinism). Non-const: text loading updates the
    // AssetManager cache.
    Result<void> loadDirectory(AssetManager& assets,
                               const std::string& relativeDir =
                                   "data/items");

    // -- lookups -------------------------------------------------------------
    const Item& at(ItemID id) const; // asserts valid id
    const Item* find(ItemID id) const;
    const Item* findByName(const std::string& name) const;
    ItemID idForName(const std::string& name) const; // invalid() when absent
    // The FULL bucket that pours `fluidName` (e.g. "lava") back into the
    // EMPTY item named `emptyName` — the derived companion of a collect
    // (Phase 51: the empty bucket's 'collects_becomes' field was removed,
    // the pair resolves from the full side). nullptr when absent; the
    // game-init cross-check guarantees exactly one for every fluid an
    // empty bucket names. A small-registry linear scan (per click, never
    // in a hot loop).
    const Item* fullBucketFor(const std::string& fluidName,
                              const std::string& emptyName) const;

    std::size_t count() const { return m_items.size(); }
    std::vector<ItemID> allIds() const;

    void clear();

private:
    // Parse `max_stack` into `out` (starts at defaults).
    static Result<void> parseMaxStack(const Json& def, Item& out);
    // Parse the optional `attack_damage` (Phase 20) into `out`.
    static Result<void> parseAttackDamage(const Json& def, Item& out);
    // Parse the optional launch fields `shoot_speed` / `shoot_damage`
    // (Phase 21) into `out`. shoot_speed > 0 marks the item as a
    // projectile launcher (the bow).
    static Result<void> parseShoot(const Json& def, Item& out);
    // Parse the optional food field `restore_hunger` (Phase 23) into
    // `out`. Presence marks the item as food (restoreHunger in [1, 20]).
    static Result<void> parseFood(const Json& def, Item& out);
    // Parse the optional launcher ammo field `ammo` (Phase 24) into
    // `out` (an item name; presence marks the launcher as consuming
    // ammo). The name's existence is cross-checked at game init (the
    // ammo item may be defined in a later file).
    static Result<void> parseAmmo(const Json& def, Item& out);
    // Parse the optional launcher arrow-effect field `arrow_effect`
    // (SURV-5) into `out`: the strict {name, duration} pair (the
    // on_hit_effect discipline, duration > 0), only valid on a LAUNCHER
    // (shoot_speed > 0 — a non-launcher's arrows never fly, the dead-
    // field discipline). The effect name's existence + its `affects_mobs`
    // flag are cross-checked at game init (the EffectRegistry is not
    // available at parse, the grants_effect pattern).
    static Result<void> parseArrowEffect(const Json& def, Item& out);
    // Parse the optional furnace fields (Phase 40) into `out`:
    // `smeltable_to` (an item name; presence marks the item as
    // smeltable — the product's existence is cross-checked at game init),
    // `smelt_time` ((0, 100] s, default 10.0) and `fuel_time` ((0, 1000]
    // s; 0 = not fuel).
    static Result<void> parseFurnace(const Json& def, Item& out);
    // Parse the optional dye field `dye_color` (Phase 41) into `out`
    // (a color name; presence marks the item as a dye — block dye
    // colors are cross-checked against the dye items at game init).
    static Result<void> parseDye(const Json& def, Item& out);
    // Parse the optional drinkable fields (Phase 43) into `out`:
    // `drink_time` ((0, 10] s; presence marks the item as a drinkable
    // container — the consume channel's duration) and `returns_item`
    // (a non-empty item name returned on a completed drink; only valid
    // together with `drink_time`, the product's existence is
    // cross-checked at game init). `drink_time` and `restore_hunger`
    // are mutually exclusive (an item is food OR a drinkable).
    static Result<void> parseDrink(const Json& def, Item& out);
    // Parse the optional fluid-bucket fields (Phase 47, kinds in
    // Phase 51) into `out`: `collects_fluid` (a non-empty array of fluid
    // names — the EMPTY side; the full bucket a scoop yields is derived
    // from the full side, see fullBucketFor) and `pours_fluid` +
    // `pours_becomes` (the FULL side). Fluid names are restricted to the
    // two-kind vocabulary ("water", "lava"); `pours_becomes` is a
    // non-empty item name (its existence + pair symmetry are
    // cross-checked at game init). The two sides are mutually exclusive,
    // as is any bucket side with food/drinkable (the right-click channel
    // owner).
    static Result<void> parseFluidBucket(const Json& def, Item& out);
    // Parse the optional 'ignites_flamable' field (Phase 60) into `out`:
    // a strict bool (default false; non-bool rejected by name). true
    // marks the item as an IGNITER (right-click burns `flamable` blocks
    // to air, no drops, the item is not consumed). Mutually exclusive
    // with every other right-click channel (food / drinkable / launcher
    // / dye / fluid bucket — the channel-ownership discipline).
    static Result<void> parseIgnites(const Json& def, Item& out);
    // Parse the optional 'grants_effect' field (Phase 49) into `out`: the
    // status effect granted when a full CONSUME completes (golden apple ->
    // regeneration; SURV-4 potions -> their effect). A strict {name,
    // duration} pair (the on_hit_effect discipline); the effect name is
    // cross-checked at game init. Only valid on food OR a drinkable
    // (restore_hunger > 0 || drink_time > 0) — the two consume channels; a
    // non-consumable carrying the field is a dead-field typo. A duration of
    // 0 marks an instant effect (settled on apply); the 0 ⇔ instant
    // equivalence is enforced at game init (the EffectRegistry is not
    // available at parse).
    static Result<void> parseGrant(const Json& def, Item& out);
    // Parse the tool fields (`tool.break_speed` + `max_stack`) into `out`.
    static Result<void> parseTool(const Json& def, Item& out);
    // Parse the optional 'fast_against' + 'fast_against_multiplier' pair
    // (Phase 67, per-block tool efficiency) into `out`: a non-empty array
    // of block names + a number > 0, a STRICT pair (one without the other
    // rejected, the empty list rejected), only valid on type "tool"
    // (the break-speed channel — a material holding the fields is a
    // typo). The block names' existence is cross-checked at game init
    // (the grows_on discipline).
    static Result<void> parseFastAgainst(const Json& def, bool isTool,
                                         Item& out);
    // Parse the optional 'armor_slot' + 'armor_points' pair (SURV-1,
    // armor) into `out`: a slot vocabulary name (head/chest/legs/feet —
    // the fixed kArmorSlotNames) + an integer in [0, kArmorMaxPoints],
    // a STRICT pair (one without the other rejected, as is an unknown
    // slot name), only valid on type "material" (a tool holding the
    // fields is a typo — armor is a right-click channel, not a
    // break-speed one). The piece owns the right-click channel (equip /
    // swap / unequip), so it is mutually exclusive with every other
    // channel owner (food / drinkable / launcher / dye / fluid bucket /
    // igniter — the channel-ownership discipline, the parseIgnites
    // pattern; the checks run after all the other channels parsed, the
    // parse flow order).
    static Result<void> parseArmor(const Json& def, bool isTool, Item& out);
    // Parse the optional 'durability' field (SURV-3) into `out`: a strict
    // number > 0 (the accumulated damage at which the item breaks). The
    // ALLOWED set = the "has a use action" items — a tool (mining / melee
    // wear), an armor piece (damage-reduction wear), an ammo launcher
    // (shot wear) or an igniter (ignite wear); any other item carrying the
    // field is rejected and named (the ammo discipline: a no-op field is
    // a typo). Runs LAST in the parse flow (the allowed-set check needs
    // the armor / ammo / igniter fields parsed).
    static Result<void> parseDurability(const Json& def, Item& out);
    // Parse the optional 'repair_with' field (UX-5, material repair)
    // into `out`: a strict non-empty item name (the ammo discipline —
    // the absent field is the "not repairable" default, so an explicit
    // empty string or a non-string is a typo, rejected and named). The
    // pairing (the item must carry a durability) and the material's
    // existence are cross-checked at game init (checkRepairMaterials —
    // the material may be defined in a later file, the ammo pattern).
    static Result<void> parseRepair(const Json& def, Item& out);
    // Parse the optional 'model' section (CRAFT-2, the per-item 3D
    // model) into `out`: a strict object with 'texture' (a non-empty
    // file name under assets/textures/items/ — the existence is
    // cross-checked at game init, the icon-file discipline) + 'elements'
    // (a non-empty array of 1..16 axis-aligned boxes: 'from' / 'to'
    // arrays of 3 numbers in [0, 1] with from < to on every axis, an
    // optional 'uv' array of 4 numbers in [0, 1] with u0 < u1 and
    // v0 < v1 = the face's sub-rectangle of the texture, absent = the
    // full texture). Unknown keys at every level are named and rejected;
    // the explicit empty elements array is rejected (the no-op
    // discipline). Pure rendering data: it owns no right-click channel,
    // so it is not mutually exclusive with any other field.
    static Result<void> parseModel(const Json& def, Item& out);

    std::deque<Item> m_items; // deque: stable references on push
    std::unordered_map<std::string, int> m_names;
};

} // namespace mc
