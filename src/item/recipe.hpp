// Crafting recipes (Phase 12): data-driven recipe definitions + the pure
// craft logic.
//
// A recipe is a shapeless bag of ingredients (order-independent) that
// yields one output stack. There is no in-game crafting UI or trigger yet
// (Phase 15): the Game loads and validates the recipe data at startup, and
// canCraft/craft are the deterministic core the future UI will call.
//
// Recipe JSON (assets/data/recipes/*.json, one file per recipe):
// {
//   "id": "iron_pickaxe",
//   "output": { "item": "iron_pickaxe", "count": 1 },
//   "ingredients": [
//     { "item": "iron_ingot", "count": 3 },
//     { "item": "stick", "count": 2 }
//   ]
// }
//
// Validation is strict and cross-checked against the ItemRegistry (which
// the game loads first): unknown item references, an empty ingredient
// list, out-of-range counts (the output must fit one stack of its item),
// and duplicate ingredient items all produce descriptive errors naming the
// recipe. Unknown fields warn but do not reject.
//
// craft() semantics (all-or-nothing, deterministic):
//   - ingredients are consumed in recipe order, each from the lowest-index
//     slot upward (hotbar before storage);
//   - the whole operation is simulated on a copy of the inventory and
//     committed only when it fully succeeds — the output always has room
//     (even when the output item is also an ingredient), and a failed
//     craft leaves the inventory bit-for-bit unchanged.
#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/asset_manager.hpp"
#include "core/error.hpp"
#include "core/json.hpp"
#include "item/inventory.hpp"
#include "item/item.hpp"
#include "item/item_registry.hpp"

namespace mc {

struct Recipe {
    std::string id;
    ItemID output;
    int outputCount = 1;
    // (item, required count), recipe order. Order-independent semantics.
    std::vector<std::pair<ItemID, int>> ingredients;
};

class RecipeRegistry {
public:
    RecipeRegistry() = default;

    // Parse and register one recipe, cross-checking item references and
    // stack bounds against `items`.
    Result<void> registerRecipe(const Json& def, const ItemRegistry& items);

    // Load and register every *.json file in `relativeDir` under the asset
    // root (sorted for determinism). Non-const: text loading updates the
    // AssetManager cache.
    Result<void> loadDirectory(AssetManager& assets,
                               const ItemRegistry& items,
                               const std::string& relativeDir =
                                   "data/recipes");

    // -- lookups -------------------------------------------------------------
    std::size_t count() const { return m_recipes.size(); }
    // Stable across calls (no runtime mutation after load).
    const std::vector<Recipe>& all() const { return m_recipes; }
    const Recipe* find(const std::string& id) const;

    void clear() {
        m_recipes.clear();
        m_names.clear();
    }

private:
    std::vector<Recipe> m_recipes;
    std::unordered_map<std::string, std::size_t> m_names;
};

// Can `inv` satisfy every ingredient AND fit the output (see the
// file-header semantics)?
bool canCraft(const Inventory& inv, const Recipe& recipe,
              const ItemRegistry& items);

// Consumes the ingredients and adds the output to `inv`. All-or-nothing:
// on failure the inventory is unchanged and an error naming the recipe is
// returned.
Result<void> craft(Inventory& inv, const Recipe& recipe,
                   const ItemRegistry& items);

} // namespace mc
