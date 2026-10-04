#include "item/item_registry.hpp"

#include <algorithm>
#include <cassert>
#include <filesystem>

#include "core/logger.hpp"

namespace mc {

Result<void> ItemRegistry::buildBlockItems(const BlockRegistry& blocks) {
    for (const BlockID id : blocks.allIds()) {
        if (id == BlockID::air()) {
            continue; // air is a code sentinel, not a data block
        }
        const Block& block = blocks.at(id);
        if (m_names.count(block.name) != 0) {
            return MC_FAIL("duplicate item id '" + block.name +
                           "' (block item collides with an explicit item)");
        }
        Item item;
        item.name = block.name;
        item.type = ItemType::Block;
        item.block = id;
        item.breakSpeed = 1.0f;
        item.maxStack = 64;
        item.id = ItemID{static_cast<int>(m_items.size())};
        m_names.emplace(item.name, item.id.value);
        m_items.push_back(std::move(item));
    }
    return {};
}

void ItemRegistry::clear() {
    m_items.clear();
    m_names.clear();
}

Result<void> ItemRegistry::parseMaxStack(const Json& def, Item& out) {
    if (const Json* ms = def.find("max_stack")) {
        if (!ms->isNumber()) {
            return MC_FAIL("'max_stack' must be a number");
        }
        out.maxStack = ms->asInt();
    }
    if (out.maxStack < 1 || out.maxStack > 64) {
        return MC_FAIL("'max_stack' must be in [1, 64], got " +
                       std::to_string(out.maxStack));
    }
    return {};
}

Result<void> ItemRegistry::parseTool(const Json& def, Item& out) {
    if (const Json* tool = def.find("tool")) {
        if (!tool->isObject()) {
            return MC_FAIL("'tool' must be an object");
        }
        if (const Json* speed = tool->find("break_speed")) {
            if (!speed->isNumber()) {
                return MC_FAIL("'tool.break_speed' must be a number");
            }
            out.breakSpeed = static_cast<float>(speed->asDouble());
        }
    }
    if (out.breakSpeed <= 0.0f) {
        return MC_FAIL("'tool.break_speed' must be > 0, got " +
                       std::to_string(out.breakSpeed));
    }
    auto r = parseMaxStack(def, out);
    MC_TRY(r);
    return {};
}

Result<ItemID> ItemRegistry::registerItem(const Json& def) {
    if (!def.isObject()) {
        return MC_FAIL("item definition must be a JSON object");
    }

    const Json* idJson = def.find("id");
    if (idJson == nullptr || !idJson->isString() ||
        idJson->asString().empty()) {
        return MC_FAIL("item definition requires a non-empty string 'id'");
    }
    const std::string name = idJson->asString();

    if (m_names.count(name) != 0) {
        return MC_FAIL("duplicate item id '" + name + "'");
    }

    // Explicit items are tools or materials: block items are defined by
    // their block (buildBlockItems), so a separate block-item JSON would
    // be a conflicting second source of truth.
    const Json* typeJson = def.find("type");
    if (typeJson == nullptr || !typeJson->isString() ||
        (typeJson->asString() != "tool" &&
         typeJson->asString() != "material")) {
        return MC_FAIL("explicit item '" + name +
                       "' requires 'type': \"tool\" or \"material\" "
                       "(block items are defined by their block)");
    }
    const bool isTool = typeJson->asString() == "tool";
    if (!isTool && def.find("tool") != nullptr) {
        return MC_FAIL("item '" + name +
                       "' is a material: the 'tool' field is only "
                       "valid for type \"tool\"");
    }

    // Warn about unknown top-level keys (typos, future fields).
    for (const auto& [key, value] : def.members()) {
        if (key != "id" && key != "type" && key != "tool" &&
            key != "max_stack") {
            MC_LOG_WARN("Item '" + name + "': unknown field '" + key +
                        "' ignored");
        }
    }

    Item item;
    item.name = name;
    item.type = isTool ? ItemType::Tool : ItemType::Material;
    auto r = isTool ? parseTool(def, item) : parseMaxStack(def, item);
    MC_TRY(r);

    item.id = ItemID{static_cast<int>(m_items.size())};
    m_names.emplace(name, item.id.value);
    m_items.push_back(std::move(item));
    MC_LOG_DEBUG("Registered item '" + name + "' (id=" +
                 std::to_string(item.id.value) + ")");
    return item.id;
}

Result<void> ItemRegistry::loadDirectory(AssetManager& assets,
                                         const std::string& relativeDir) {
    auto dir = assets.resolve(relativeDir);
    MC_TRY(dir);
    if (!std::filesystem::is_directory(*dir)) {
        MC_LOG_WARN("Item directory not found: '" + (*dir).string() +
                    "' (no explicit items loaded)");
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
            return MC_FAIL("invalid JSON in item file '" + rel + "': " +
                           parsed.error().message);
        }
        auto id = registerItem(*parsed);
        if (!id.ok()) {
            return MC_FAIL("item file '" + rel + "': " + id.error().message);
        }
    }
    return {};
}

const Item& ItemRegistry::at(ItemID id) const {
    // Contract: caller holds a valid id from this registry.
    assert(id.valid() && id.value < static_cast<int>(m_items.size()));
    return m_items[static_cast<std::size_t>(id.value)];
}

const Item* ItemRegistry::find(ItemID id) const {
    if (!id.valid() || id.value >= static_cast<int>(m_items.size())) {
        return nullptr;
    }
    return &m_items[static_cast<std::size_t>(id.value)];
}

const Item* ItemRegistry::findByName(const std::string& name) const {
    const auto it = m_names.find(name);
    return it == m_names.end()
               ? nullptr
               : &m_items[static_cast<std::size_t>(it->second)];
}

ItemID ItemRegistry::idForName(const std::string& name) const {
    const auto it = m_names.find(name);
    return it == m_names.end() ? ItemID::invalid() : ItemID{it->second};
}

std::vector<ItemID> ItemRegistry::allIds() const {
    std::vector<ItemID> ids;
    ids.reserve(m_items.size());
    for (const Item& item : m_items) {
        ids.push_back(item.id);
    }
    return ids;
}

} // namespace mc
