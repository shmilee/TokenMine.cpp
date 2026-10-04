#include "item/recipe.hpp"

#include <algorithm>
#include <filesystem>
#include <unordered_set>
#include <utility>

#include "core/logger.hpp"

namespace mc {

namespace {

// Runs the full craft (consume ingredients + add output) on the passed
// BY-VALUE copy of the inventory. Ok result = the resulting inventory.
Result<Inventory> simulateCraft(Inventory inv, const Recipe& recipe,
                                const ItemRegistry& items) {
    // Consume each ingredient (recipe order) from the lowest-index slot
    // upward — deterministic, hotbar before storage.
    for (const auto& [item, need] : recipe.ingredients) {
        int remaining = need;
        for (std::size_t i = 0; i < inv.size() && remaining > 0; ++i) {
            if (inv.at(i).item != item) {
                continue;
            }
            const int taken = std::min(remaining, inv.at(i).count);
            inv.take(i, taken);
            remaining -= taken;
        }
        if (remaining > 0) {
            return MC_FAIL("missing " + std::to_string(remaining) +
                           " of ingredient");
        }
    }
    // The output must fit (consumption only freed space; checking it after
    // consumption is exact even when the output is also an ingredient).
    if (inv.add(recipe.output, recipe.outputCount, items) < 0) {
        return MC_FAIL("no room for the output");
    }
    return inv;
}

} // namespace

bool canCraft(const Inventory& inv, const Recipe& recipe,
              const ItemRegistry& items) {
    return simulateCraft(inv, recipe, items).ok();
}

Result<void> craft(Inventory& inv, const Recipe& recipe,
                   const ItemRegistry& items) {
    auto r = simulateCraft(inv, recipe, items);
    if (!r.ok()) {
        return MC_FAIL("cannot craft '" + recipe.id + "': " +
                       r.error().message);
    }
    inv = std::move(*r);
    return {};
}

// -- RecipeRegistry -----------------------------------------------------------

Result<void> RecipeRegistry::registerRecipe(const Json& def,
                                            const ItemRegistry& items) {
    if (!def.isObject()) {
        return MC_FAIL("recipe definition must be a JSON object");
    }

    const Json* idJson = def.find("id");
    if (idJson == nullptr || !idJson->isString() || idJson->asString().empty()) {
        return MC_FAIL("recipe definition requires a non-empty string 'id'");
    }
    const std::string name = idJson->asString();

    if (m_names.count(name) != 0) {
        return MC_FAIL("duplicate recipe id '" + name + "'");
    }

    // Warn about unknown top-level keys (typos, future fields).
    for (const auto& [key, value] : def.members()) {
        if (key != "id" && key != "output" && key != "ingredients") {
            MC_LOG_WARN("Recipe '" + name + "': unknown field '" + key +
                        "' ignored");
        }
    }

    // output: { "item": known item name, "count": 1..maxStack(item) }
    const Json* outJson = def.find("output");
    if (outJson == nullptr || !outJson->isObject()) {
        return MC_FAIL("recipe '" + name + "' requires an 'output' object");
    }
    for (const auto& [key, value] : outJson->members()) {
        if (key != "item" && key != "count") {
            MC_LOG_WARN("Recipe '" + name + "': unknown 'output' field '" +
                        key + "' ignored");
        }
    }
    const Json* outItem = outJson->find("item");
    if (outItem == nullptr || !outItem->isString() ||
        outItem->asString().empty()) {
        return MC_FAIL("recipe '" + name +
                       "' requires 'output.item' (an item name)");
    }
    const ItemID outId = items.idForName(outItem->asString());
    if (!outId.valid()) {
        return MC_FAIL("recipe '" + name + "' outputs unknown item '" +
                       outItem->asString() + "'");
    }
    const Json* outCount = outJson->find("count");
    if (outCount == nullptr || !outCount->isNumber()) {
        return MC_FAIL("recipe '" + name +
                       "' requires 'output.count' (a number)");
    }
    const int outN = outCount->asInt();
    const int outMax = items.at(outId).maxStack;
    if (outN < 1 || outN > outMax) {
        return MC_FAIL("recipe '" + name + "': 'output.count' must be in "
                       "[1, " + std::to_string(outMax) +
                       "] (max stack of '" + outItem->asString() +
                       "'), got " + std::to_string(outN));
    }

    // ingredients: non-empty array of { "item", "count" }, no item may
    // appear twice (merge the counts into one entry).
    const Json* ingJson = def.find("ingredients");
    if (ingJson == nullptr || !ingJson->isArray() || ingJson->size() == 0) {
        return MC_FAIL("recipe '" + name +
                       "' requires a non-empty 'ingredients' array");
    }
    std::vector<std::pair<ItemID, int>> ingredients;
    std::unordered_set<int> seen;
    for (std::size_t i = 0; i < ingJson->size(); ++i) {
        const Json& entry = ingJson->at(i);
        if (!entry.isObject()) {
            return MC_FAIL("recipe '" + name + "': ingredient #" +
                           std::to_string(i) + " must be an object");
        }
        const Json* ingItem = entry.find("item");
        if (ingItem == nullptr || !ingItem->isString() ||
            ingItem->asString().empty()) {
            return MC_FAIL("recipe '" + name + "': ingredient #" +
                           std::to_string(i) + " requires 'item' (an item "
                           "name)");
        }
        const ItemID ingId = items.idForName(ingItem->asString());
        if (!ingId.valid()) {
            return MC_FAIL("recipe '" + name + "' uses unknown ingredient "
                           "item '" +
                           ingItem->asString() + "'");
        }
        const Json* ingCount = entry.find("count");
        if (ingCount == nullptr || !ingCount->isNumber()) {
            return MC_FAIL("recipe '" + name + "': ingredient #" +
                           std::to_string(i) +
                           " requires 'count' (a number)");
        }
        const int ingN = ingCount->asInt();
        if (ingN < 1) {
            return MC_FAIL("recipe '" + name + "': ingredient #" +
                           std::to_string(i) + "'s 'count' must be >= 1, "
                           "got " +
                           std::to_string(ingN));
        }
        if (seen.count(ingId.value) != 0) {
            return MC_FAIL("recipe '" + name + "': duplicate ingredient "
                           "item '" +
                           ingItem->asString() + "' (merge the counts into "
                           "one entry)");
        }
        seen.insert(ingId.value);
        ingredients.emplace_back(ingId, ingN);
    }

    Recipe recipe;
    recipe.id = name;
    recipe.output = outId;
    recipe.outputCount = outN;
    recipe.ingredients = std::move(ingredients);
    m_names.emplace(name, m_recipes.size());
    m_recipes.push_back(std::move(recipe));
    MC_LOG_DEBUG("Registered recipe '" + name + "'");
    return {};
}

Result<void> RecipeRegistry::loadDirectory(AssetManager& assets,
                                           const ItemRegistry& items,
                                           const std::string& relativeDir) {
    auto dir = assets.resolve(relativeDir);
    MC_TRY(dir);
    if (!std::filesystem::is_directory(*dir)) {
        MC_LOG_WARN("Recipe directory not found: '" + (*dir).string() +
                    "' (no recipes loaded)");
        return {};
    }

    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(*dir)) {
        if (entry.is_regular_file() &&
            entry.path().extension() == ".json") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());

    for (const auto& file : files) {
        // AssetManager keys are relative to the root.
        const auto rel =
            std::filesystem::relative(file, assets.root()).generic_string();
        auto text = assets.loadText(rel);
        MC_TRY(text);
        auto parsed = Json::parse(*text);
        if (!parsed.ok()) {
            return MC_FAIL("invalid JSON in recipe file '" + rel + "': " +
                           parsed.error().message);
        }
        auto r = registerRecipe(*parsed, items);
        if (!r.ok()) {
            return MC_FAIL("recipe file '" + rel + "': " + r.error().message);
        }
    }
    return {};
}

const Recipe* RecipeRegistry::find(const std::string& id) const {
    const auto it = m_names.find(id);
    return it == m_names.end() ? nullptr : &m_recipes[it->second];
}

} // namespace mc
