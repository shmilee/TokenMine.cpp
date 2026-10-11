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
    // Parse the optional furnace fields (Phase 40) into `out`:
    // `smeltable_to` (an item name; presence marks the item as
    // smeltable — the product's existence is cross-checked at game init),
    // `smelt_time` ((0, 100] s, default 10.0) and `fuel_time` ((0, 1000]
    // s; 0 = not fuel).
    static Result<void> parseFurnace(const Json& def, Item& out);
    // Parse the tool fields (`tool.break_speed` + `max_stack`) into `out`.
    static Result<void> parseTool(const Json& def, Item& out);

    std::deque<Item> m_items; // deque: stable references on push
    std::unordered_map<std::string, int> m_names;
};

} // namespace mc
