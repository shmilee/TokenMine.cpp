#include "item/item_registry.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
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

Result<void> ItemRegistry::parseAttackDamage(const Json& def, Item& out) {
    // Optional 'attack_damage' (Phase 20): melee damage of a swing with
    // this item. Defaults to 1.0 (bare hand / block items).
    if (const Json* ad = def.find("attack_damage")) {
        if (!ad->isNumber()) {
            return MC_FAIL("'attack_damage' must be a number");
        }
        out.attackDamage = static_cast<float>(ad->asDouble());
    }
    if (out.attackDamage <= 0.0f || out.attackDamage > 100.0f) {
        return MC_FAIL("'attack_damage' must be in (0, 100], got " +
                       std::to_string(out.attackDamage));
    }
    return {};
}

Result<void> ItemRegistry::parseShoot(const Json& def, Item& out) {
    // Optional 'shoot_speed' / 'shoot_damage' (Phase 21): the item is a
    // projectile launcher (the bow) when shoot_speed > 0. While selected,
    // the right button draws the bow and the release fires a Projectile
    // with speed/damage scaled by the charge (Phase 24, see
    // Game::fireBow + core/bow.hpp); the left button is the bow's melee
    // swing.
    if (const Json* ss = def.find("shoot_speed")) {
        if (!ss->isNumber()) {
            return MC_FAIL("'shoot_speed' must be a number");
        }
        out.shootSpeed = static_cast<float>(ss->asDouble());
        if (out.shootSpeed <= 0.0f || out.shootSpeed > 100.0f) {
            return MC_FAIL("'shoot_speed' must be in (0, 100], got " +
                           std::to_string(out.shootSpeed));
        }
    }
    if (const Json* sd = def.find("shoot_damage")) {
        if (!sd->isNumber()) {
            return MC_FAIL("'shoot_damage' must be a number");
        }
        out.shootDamage = static_cast<float>(sd->asDouble());
        if (out.shootDamage < 0.0f || out.shootDamage > 100.0f) {
            return MC_FAIL("'shoot_damage' must be in [0, 100], got " +
                           std::to_string(out.shootDamage));
        }
    }
    return {};
}

Result<void> ItemRegistry::parseFood(const Json& def, Item& out) {
    // Optional 'restore_hunger' (Phase 23): the item is FOOD when the
    // field is present — right-clicking with it selected eats one
    // (restores this much hunger). Integer in [1, 20] (the player's max
    // hunger); the ABSENT field is the "not food" default, so an explicit
    // 0 is rejected as a typo (same discipline as shoot_speed).
    if (const Json* rh = def.find("restore_hunger")) {
        if (!rh->isNumber()) {
            return MC_FAIL("'restore_hunger' must be a number");
        }
        const double d = rh->asDouble();
        if (d != std::floor(d)) {
            return MC_FAIL("'restore_hunger' must be an integer (1..20)");
        }
        out.restoreHunger = static_cast<int>(d);
        if (out.restoreHunger < 1 || out.restoreHunger > 20) {
            return MC_FAIL("'restore_hunger' must be in [1, 20], got " +
                           std::to_string(out.restoreHunger));
        }
    }
    return {};
}

Result<void> ItemRegistry::parseAmmo(const Json& def, Item& out) {
    // Optional 'ammo' (Phase 24): the launcher's ammo item name. Presence
    // (a non-empty string) marks the launcher as CONSUMING ammo: firing
    // requires at least one in the inventory and consumes one. The absent
    // field is the "no ammo" default, so an explicit empty string is
    // rejected as a typo (same discipline as restore_hunger /
    // shoot_speed). The name is resolved against the ItemRegistry at game
    // init (the ammo item may be defined in a later file).
    if (const Json* am = def.find("ammo")) {
        if (!am->isString() || am->asString().empty()) {
            return MC_FAIL("'ammo' must be a non-empty item name");
        }
        out.ammo = am->asString();
    }
    return {};
}

Result<void> ItemRegistry::parseFurnace(const Json& def, Item& out) {
    // Optional 'smeltable_to' (Phase 40): the furnace product item name.
    // Presence (a non-empty string) marks the item as SMELTABLE; the
    // absent field is the "not smeltable" default, so an explicit empty
    // string is rejected as a typo (the ammo discipline). The name is
    // resolved against the ItemRegistry at game init (the product may be
    // defined in a later file).
    if (const Json* st = def.find("smeltable_to")) {
        if (!st->isString() || st->asString().empty()) {
            return MC_FAIL("'smeltable_to' must be a non-empty item name");
        }
        out.smeltableTo = st->asString();
    }
    // Optional 'smelt_time' (Phase 40): seconds per unit in the furnace,
    // a number in (0, 100] (Minecraft's 10 s default).
    if (const Json* stt = def.find("smelt_time")) {
        if (!stt->isNumber()) {
            return MC_FAIL("'smelt_time' must be a number");
        }
        const double d = stt->asDouble();
        out.smeltTime = static_cast<float>(d);
        if (out.smeltTime <= 0.0f || out.smeltTime > 100.0f) {
            return MC_FAIL("'smelt_time' must be in (0, 100], got " +
                           std::to_string(out.smeltTime));
        }
    }
    // Optional 'fuel_time' (Phase 40): seconds one unit burns in the
    // furnace, a number in (0, 1000]. The absent field (or fuelTime 0)
    // means "not fuel"; an explicit 0 is rejected as a typo (the
    // restore_hunger discipline).
    if (const Json* ft = def.find("fuel_time")) {
        if (!ft->isNumber()) {
            return MC_FAIL("'fuel_time' must be a number");
        }
        const double d = ft->asDouble();
        out.fuelTime = static_cast<float>(d);
        if (out.fuelTime <= 0.0f || out.fuelTime > 1000.0f) {
            return MC_FAIL("'fuel_time' must be in (0, 1000], got " +
                           std::to_string(out.fuelTime));
        }
    }
    return {};
}

Result<void> ItemRegistry::parseDye(const Json& def, Item& out) {
    // Optional 'dye_color' (Phase 41): the dye's color name (e.g.
    // "red"). Presence (a non-empty string) marks the item as a DYE; the
    // absent field is the "not a dye" default, so an explicit empty
    // string is rejected as a typo (the ammo discipline). The color
    // vocabulary is data-defined (dye items name the colors); block dye
    // colors are cross-checked against it at game init.
    if (const Json* dc = def.find("dye_color")) {
        if (!dc->isString() || dc->asString().empty()) {
            return MC_FAIL("'dye_color' must be a non-empty color name");
        }
        out.dyeColor = dc->asString();
    }
    return {};
}

Result<void> ItemRegistry::parseDrink(const Json& def, Item& out) {
    // Optional 'drink_time' (Phase 43): seconds of HELD right button for
    // a full drink (Minecraft's milk: 1.6 s). Presence (a number in
    // (0, 10]) marks the item as a DRINKABLE container; the absent field
    // is the "not drinkable" default, so an explicit 0 is rejected as a
    // typo (the restore_hunger discipline).
    if (const Json* dt = def.find("drink_time")) {
        if (!dt->isNumber()) {
            return MC_FAIL("'drink_time' must be a number");
        }
        out.drinkTime = static_cast<float>(dt->asDouble());
        if (out.drinkTime <= 0.0f || out.drinkTime > 10.0f) {
            return MC_FAIL("'drink_time' must be in (0, 10], got " +
                           std::to_string(out.drinkTime));
        }
    }
    // Optional 'returns_item' (Phase 43): the container a completed
    // drink returns to the inventory (milk_bucket -> bucket). Presence
    // (a non-empty string) is valid only on a drinkable item — a
    // returns_item without a drink_time is a dead field, rejected (the
    // dye_base/dye_color pairing discipline). The name is resolved
    // against the ItemRegistry at game init (the ammo pattern).
    if (const Json* ri = def.find("returns_item")) {
        if (!ri->isString() || ri->asString().empty()) {
            return MC_FAIL("'returns_item' must be a non-empty item name");
        }
        out.returnsItem = ri->asString();
    }
    if (!out.returnsItem.empty() && out.drinkTime <= 0.0f) {
        return MC_FAIL("'returns_item' requires a 'drink_time' "
                       "(only a drinkable item returns a container)");
    }
    // Food and drinkable are mutually exclusive channels: one item cannot
    // be both (the restore_hunger/drink_time fields would fight over the
    // same right-button hold channel and the same completion path).
    if (out.restoreHunger > 0 && out.drinkTime > 0.0f) {
        return MC_FAIL("an item cannot be both food ('restore_hunger') "
                       "and a drinkable ('drink_time')");
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
    return parseMaxStack(def, out);
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
            key != "max_stack" && key != "attack_damage" &&
            key != "shoot_speed" && key != "shoot_damage" &&
            key != "restore_hunger" && key != "ammo" &&
            key != "smeltable_to" && key != "smelt_time" &&
            key != "fuel_time" && key != "dye_color" &&
            key != "drink_time" && key != "returns_item" &&
            key != "icon") {
            MC_LOG_WARN("Item '" + name + "': unknown field '" + key +
                        "' ignored");
        }
    }

    Item item;
    item.name = name;
    item.type = isTool ? ItemType::Tool : ItemType::Material;
    auto r = isTool ? parseTool(def, item) : parseMaxStack(def, item);
    MC_TRY(r);
    r = parseAttackDamage(def, item);
    MC_TRY(r);
    r = parseShoot(def, item);
    MC_TRY(r);
    r = parseFood(def, item);
    MC_TRY(r);
    r = parseAmmo(def, item);
    MC_TRY(r);
    r = parseFurnace(def, item);
    MC_TRY(r);
    r = parseDye(def, item);
    MC_TRY(r);
    r = parseDrink(def, item); // after parseFood: the exclusivity check
    MC_TRY(r);

    // Optional 2D icon (Phase 15 UI): a texture file name under
    // assets/textures/items/. Block items never set one (they render the
    // 3D block cube). The file-existence cross-check happens in the game
    // (same pattern as the block texture check).
    if (const Json* icon = def.find("icon")) {
        if (!icon->isString() || icon->asString().empty()) {
            return MC_FAIL("item '" + name +
                           "': 'icon' must be a non-empty string");
        }
        item.icon = icon->asString();
    }

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
