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
        if (key != "id" && key != "output" && key != "ingredients" &&
            key != "type" && key != "pattern" && key != "key") {
            MC_LOG_WARN("Recipe '" + name + "': unknown field '" + key +
                        "' ignored");
        }
    }

    // type: optional, "shapeless" (the default) or "shaped".
    RecipeType type = RecipeType::Shapeless;
    if (const Json* typeJson = def.find("type")) {
        if (!typeJson->isString()) {
            return MC_FAIL("recipe '" + name + "': 'type' must be a string");
        }
        const std::string t = typeJson->asString();
        if (t == "shapeless") {
            type = RecipeType::Shapeless;
        } else if (t == "shaped") {
            type = RecipeType::Shaped;
        } else {
            return MC_FAIL("recipe '" + name + "': unknown 'type' '" + t +
                           "' (expected 'shapeless' or 'shaped')");
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

    Recipe recipe;
    recipe.id = name;
    recipe.type = type;
    recipe.output = outId;
    recipe.outputCount = outN;

    if (type == RecipeType::Shaped) {
        // Shaped: 'pattern' (1..3 rows of 1..3 chars, space = empty) +
        // 'key' (one-char string -> item name), and no 'ingredients'.
        if (def.find("ingredients") != nullptr) {
            return MC_FAIL("recipe '" + name +
                           "': a shaped recipe must not have 'ingredients' "
                           "(use 'pattern' + 'key')");
        }
        const Json* patJson = def.find("pattern");
        if (patJson == nullptr || !patJson->isArray() ||
            patJson->size() == 0 || patJson->size() > 3) {
            return MC_FAIL("recipe '" + name +
                           "' requires a 'pattern' array of 1..3 rows");
        }
        bool anyCell = false;
        for (std::size_t r = 0; r < patJson->size(); ++r) {
            const Json& rowJson = patJson->at(r);
            if (!rowJson.isString() || rowJson.asString().empty() ||
                rowJson.asString().size() > 3) {
                return MC_FAIL("recipe '" + name +
                               "': pattern row #" + std::to_string(r) +
                               " must be a 1..3 character string");
            }
            const std::string row = rowJson.asString();
            for (const char ch : row) {
                if (ch != ' ') {
                    anyCell = true;
                }
            }
            recipe.pattern.push_back(row);
        }
        if (!anyCell) {
            return MC_FAIL("recipe '" + name +
                           "': the pattern must contain at least one "
                           "item cell");
        }
        const Json* keyJson = def.find("key");
        if (keyJson == nullptr || !keyJson->isObject()) {
            return MC_FAIL("recipe '" + name +
                           "' requires a 'key' object mapping pattern "
                           "chars to item names");
        }
        for (const auto& [k, v] : keyJson->members()) {
            if (k.size() != 1 || k[0] == ' ') {
                return MC_FAIL("recipe '" + name +
                               "': key entries must be single characters "
                               "(got '" + k + "')");
            }
            if (!v.isString() || v.asString().empty()) {
                return MC_FAIL("recipe '" + name + "': key entry '" + k +
                               "' must map to an item name");
            }
            const ItemID keyId = items.idForName(v.asString());
            if (!keyId.valid()) {
                return MC_FAIL("recipe '" + name + "': key entry '" + k +
                               "' uses unknown item '" + v.asString() +
                               "'");
            }
            recipe.key.emplace(k[0], keyId);
        }
        // Cross-check both directions: every pattern char is keyed, every
        // key char is used (an unused entry would be a silent typo).
        for (const std::string& row : recipe.pattern) {
            for (const char ch : row) {
                if (ch != ' ' && recipe.key.count(ch) == 0) {
                    return MC_FAIL("recipe '" + name +
                                   "': pattern char '" +
                                   std::string(1, ch) +
                                   "' has no 'key' entry");
                }
            }
        }
        for (const auto& [ch, id] : recipe.key) {
            bool used = false;
            for (const std::string& row : recipe.pattern) {
                if (row.find(ch) != std::string::npos) {
                    used = true;
                    break;
                }
            }
            if (!used) {
                return MC_FAIL("recipe '" + name + "': key entry '" +
                               std::string(1, ch) +
                               "' is not used by the pattern");
            }
        }
    } else {
        // Shapeless: 'ingredients' (non-empty array of { "item", "count" },
        // no item may appear twice), and no 'pattern'/'key'.
        if (def.find("pattern") != nullptr || def.find("key") != nullptr) {
            return MC_FAIL("recipe '" + name +
                           "': a shapeless recipe must not have 'pattern' "
                           "or 'key' (set \"type\": \"shaped\")");
        }
        const Json* ingJson = def.find("ingredients");
        if (ingJson == nullptr || !ingJson->isArray() ||
            ingJson->size() == 0) {
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
                               std::to_string(i) + " requires 'item' (an "
                               "item name)");
            }
            const ItemID ingId = items.idForName(ingItem->asString());
            if (!ingId.valid()) {
                return MC_FAIL("recipe '" + name + "' uses unknown "
                               "ingredient item '" +
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
                               std::to_string(i) + "'s 'count' must be "
                               ">= 1, got " +
                               std::to_string(ingN));
            }
            if (seen.count(ingId.value) != 0) {
                return MC_FAIL("recipe '" + name +
                               "': duplicate ingredient item '" +
                               ingItem->asString() +
                               "' (merge the counts into one entry)");
            }
            seen.insert(ingId.value);
            ingredients.emplace_back(ingId, ingN);
        }
        recipe.ingredients = std::move(ingredients);
    }
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
