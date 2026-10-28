// Crafting recipes (Phase 12, shaped support Phase 17): data-driven
// recipe definitions + the pure craft logic.
//
// Two recipe kinds, both data-driven:
//   - shapeless: a bag of ingredients (order-independent) that yields one
//     output stack. Crafted from the recipe list in the inventory panel or
//     from a crafting grid (any placement of the bag).
//   - shaped (Phase 17): a pattern of item cells (1..3 rows, 1..3 chars
//     per row, space = empty) with a char -> item key. It matches a
//     crafting grid when the pattern fits at some offset and every grid
//     cell the pattern does not cover is empty (the grid model is 3x3; the
//     inventory panel exposes its top-left 2x2 — see crafting_grid.hpp).
//
// Recipe JSON (assets/data/recipes/*.json, one file per recipe):
// Shapeless:
// {
//   "id": "planks",
//   "output": { "item": "planks", "count": 4 },
//   "ingredients": [ { "item": "wood", "count": 1 } ]
// }
// Shaped ("type" defaults to "shapeless" when absent):
// {
//   "id": "crafting_table",
//   "type": "shaped",
//   "output": { "item": "crafting_table", "count": 1 },
//   "pattern": ["PP", "PP"],
//   "key": { "P": "planks" }
// }
// A shaped definition requires 'pattern' + 'key' and must NOT carry
// 'ingredients'; a shapeless one requires 'ingredients' and must not
// carry 'pattern'/'key'.
//
// Validation is strict and cross-checked against the ItemRegistry (which
// the game loads first): unknown item references, an empty ingredient
// list, out-of-range counts (the output must fit one stack of its item),
// duplicate ingredient items, pattern rows/chars beyond 3, a pattern char
// missing from the key, a key entry unused by the pattern, and an all-empty
// pattern all produce descriptive errors naming the recipe. Unknown fields
// warn but do not reject.
//
// craft() semantics (all-or-nothing, deterministic; shapeless recipes on
// the player's inventory):
//   - ingredients are consumed in recipe order, each from the lowest-index
//     slot upward (hotbar before storage);
//   - the whole operation is simulated on a copy of the inventory and
//     committed only when it fully succeeds — the output always has room
//     (even when the output item is also an ingredient), and a failed
//     craft leaves the inventory bit-for-bit unchanged.
// Grid crafting (shaped or shapeless) is in crafting_grid.hpp.
#pragma once

#include <cstddef>
#include <map>
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

enum class RecipeType {
    Shapeless, // bag of ingredients (order- and position-independent)
    Shaped,    // pattern of cells matched against a crafting grid
};

struct Recipe {
    std::string id;
    RecipeType type = RecipeType::Shapeless;
    ItemID output;
    int outputCount = 1;
    // Shapeless: (item, required count), recipe order. Order-independent
    // semantics. Empty for shaped recipes.
    std::vector<std::pair<ItemID, int>> ingredients;
    // Shaped: the pattern rows (1..3 rows, 1..3 chars each; a space is an
    // empty cell) and the char -> item key. Empty for shapeless recipes.
    std::vector<std::string> pattern;
    std::map<char, ItemID> key;
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

// CRAFT-4: a shaped recipe's pattern as a bag of (item, count) — one unit
// per covered cell (spaces excluded), entries in ascending ItemID order
// (two chars mapping to the same item merge into one entry). Empty for
// shapeless recipes (they carry `ingredients`, not a pattern).
std::vector<std::pair<ItemID, int>> patternBag(const Recipe& recipe);

// CRAFT-4: can `inv` supply every (item, count) of `bag`? No output-fit
// check — a grid craft's output goes to the cursor, not the inventory
// (the shaped hint rows color from this). Empty bag = true.
bool canSupply(const Inventory& inv, const std::vector<std::pair<ItemID, int>>& bag);

// Consumes the ingredients and adds the output to `inv`. All-or-nothing:
// on failure the inventory is unchanged and an error naming the recipe is
// returned.
Result<void> craft(Inventory& inv, const Recipe& recipe,
                   const ItemRegistry& items);

} // namespace mc
