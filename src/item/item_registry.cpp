#include "item/item_registry.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>

#include "core/logger.hpp"
#include "world/chunk.hpp" // fluidKindForName (the fluid vocabulary)

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

Result<void> ItemRegistry::parseFluidBucket(const Json& def, Item& out) {
    // Phase 47 (extended Phase 51): fluid buckets. `collects_fluid` (a
    // non-empty ARRAY of fluid names) marks the EMPTY side: the bucket
    // scoops every named fluid (Minecraft: one empty bucket scoops both
    // water and lava). The full bucket a scoop yields is DERIVED — the
    // unique item that pours that fluid back into this one (cross-checked
    // at game init, the ammo pattern). `pours_fluid` / `pours_becomes`
    // mark the FULL side (pour a source -> the empty bucket); the
    // `pours_becomes` value is an item NAME (resolved at game init). Fluid
    // names are checked against the two-kind core vocabulary ("water",
    // "lava" — any other name is a typo or an unimplemented fluid).
    auto parseFluidName = [](const char* key, const Json& value,
                             std::string& outName) -> Result<void> {
        if (!value.isString() || value.asString().empty()) {
            return MC_FAIL(std::string("'") + key +
                           "' must be a non-empty fluid name");
        }
        const std::string name = value.asString();
        if (!fluidKindForName(name)) {
            return MC_FAIL(std::string("'") + key + "': unknown fluid '" +
                           name + "' (vocabulary: \"water\", \"lava\")");
        }
        outName = name;
        return {};
    };
    auto parseBecomes = [](const char* key, const Json& value,
                           std::string& outName) -> Result<void> {
        if (!value.isString() || value.asString().empty()) {
            return MC_FAIL(std::string("'") + key +
                           "' must be a non-empty item name");
        }
        outName = value.asString();
        return {};
    };
    if (const Json* cf = def.find("collects_fluid")) {
        if (!cf->isArray() || cf->size() == 0) {
            return MC_FAIL("'collects_fluid' must be a non-empty array of "
                           "fluid names");
        }
        for (std::size_t i = 0; i < cf->size(); ++i) {
            std::string name;
            MC_TRY(parseFluidName("collects_fluid", cf->at(i), name));
            for (const std::string& other : out.collectsFluids) {
                if (other == name) {
                    return MC_FAIL("'collects_fluid' lists the fluid '" +
                                   name + "' twice");
                }
            }
            out.collectsFluids.push_back(name);
        }
    }
    if (const Json* pf = def.find("pours_fluid")) {
        MC_TRY(parseFluidName("pours_fluid", *pf, out.poursFluid));
    }
    if (const Json* pb = def.find("pours_becomes")) {
        MC_TRY(parseBecomes("pours_becomes", *pb, out.poursBecomes));
    }
    // The `*_becomes` field is a dead field without its `*_fluid` pair
    // (the dye_base/dye_color pairing discipline).
    if (!out.poursBecomes.empty() && out.poursFluid.empty()) {
        return MC_FAIL("'pours_becomes' requires a 'pours_fluid' "
                       "(a bucket side names the fluid it holds)");
    }
    // One item is ONE side of the pair: a collector does not also pour
    // (the two fields would fight over the same right-click action).
    if (!out.collectsFluids.empty() && !out.poursFluid.empty()) {
        return MC_FAIL("an item cannot both collect ('collects_fluid') "
                       "and pour ('pours_fluid') (one bucket item is one "
                       "side of the pair)");
    }
    // A bucket side owns the right-click: it cannot also be food or a
    // drinkable (the eat/drink channels would consume the click before
    // the bucket ever runs — a dead field, the food/drink discipline).
    if ((!out.collectsFluids.empty() || !out.poursFluid.empty()) &&
        (out.restoreHunger > 0 || out.drinkTime > 0.0f)) {
        return MC_FAIL("a fluid-bucket item ('collects_fluid' / "
                       "'pours_fluid') cannot be food or a drinkable "
                       "(the right-click channel is owned by the bucket)");
    }
    return {};
}

Result<void> ItemRegistry::parseIgnites(const Json& def, Item& out) {
    // Optional 'ignites_flamable' (Phase 60): a strict bool (default
    // false; the absent field is the "not an igniter" default, the
    // flamable-block-flag discipline). true marks the item as an IGNITER
    // (right-click burns `flamable` blocks to air, no drops, not
    // consumed). The field is a RIGHT-CLICK channel, so it is mutually
    // exclusive with every other channel owner (the fluid-bucket
    // exclusivity pattern) — the checks run after the other channels
    // parsed (the parse flow order).
    if (const Json* ig = def.find("ignites_flamable")) {
        if (!ig->isBool()) {
            return MC_FAIL("'ignites_flamable' must be a boolean");
        }
        out.ignitesFlamable = ig->asBool();
    }
    if (out.ignitesFlamable) {
        if (out.restoreHunger > 0) {
            return MC_FAIL("an item cannot both ignite flammable blocks "
                           "('ignites_flamable') and be food "
                           "('restore_hunger'): one item owns one "
                           "right-click channel");
        }
        if (out.drinkTime > 0.0f) {
            return MC_FAIL("an item cannot both ignite flammable blocks "
                           "('ignites_flamable') and be a drinkable "
                           "('drink_time'): one item owns one right-click "
                           "channel");
        }
        if (out.shootSpeed > 0.0f) {
            return MC_FAIL("an item cannot both ignite flammable blocks "
                           "('ignites_flamable') and be a launcher "
                           "('shoot_speed'): one item owns one right-click "
                           "channel");
        }
        if (!out.dyeColor.empty()) {
            return MC_FAIL("an item cannot both ignite flammable blocks "
                           "('ignites_flamable') and be a dye "
                           "('dye_color'): one item owns one right-click "
                           "channel");
        }
        if (!out.collectsFluids.empty() || !out.poursFluid.empty()) {
            return MC_FAIL("an item cannot both ignite flammable blocks "
                           "('ignites_flamable') and be a fluid bucket "
                           "('collects_fluid' / 'pours_fluid'): one item "
                           "owns one right-click channel");
        }
    }
    return {};
}

Result<void> ItemRegistry::parseGrant(const Json& def, Item& out) {
    // Optional 'grants_effect' (Phase 49, golden apple): the status effect
    // granted when a full EAT completes — {"name": <effect>, "duration":
    // <seconds>}. Both keys are required (the on_hit_effect pair discipline);
    // the effect name is cross-checked against the EffectRegistry at game
    // init (the ammo pattern). Only FOOD grants an effect on eat (a
    // drinkable CLEARS effects on drink, so the channels are exclusive).
    if (const Json* ge = def.find("grants_effect")) {
        if (!ge->isObject()) {
            return MC_FAIL("'grants_effect' must be an object "
                           "{\"name\": ..., \"duration\": ...}");
        }
        for (const auto& [key, value] : ge->members()) {
            if (key != "name" && key != "duration") {
                return MC_FAIL("'grants_effect' has an unknown field '" +
                               key + "'");
            }
        }
        const Json* nm = ge->find("name");
        if (nm == nullptr || !nm->isString() || nm->asString().empty()) {
            return MC_FAIL("'grants_effect' requires a non-empty string "
                           "'name' (effect id)");
        }
        const Json* du = ge->find("duration");
        if (du == nullptr || !du->isNumber() || du->asDouble() <= 0.0) {
            return MC_FAIL("'grants_effect' requires 'duration' (seconds) > 0");
        }
        // Only food grants an effect on eat (the drinkable clears effects,
        // so the two channels are mutually exclusive).
        if (out.restoreHunger <= 0) {
            return MC_FAIL("'grants_effect' requires 'restore_hunger' (only "
                           "food grants an effect on eat)");
        }
        out.grantsEffect = nm->asString();
        out.grantsEffectDuration = du->asDouble();
    }
    // Phase 56: the LIST form — an array of the same strict pairs,
    // mutually exclusive with the single field (the P52 chance_drops
    // pattern), applied IN ORDER on a full eat.
    if (const Json* gel = def.find("grants_effects")) {
        if (!gel->isArray()) {
            return MC_FAIL("'grants_effects' must be an array of "
                           "{\"name\": ..., \"duration\": ...} pairs");
        }
        if (gel->size() == 0) {
            return MC_FAIL("'grants_effects' must not be an empty array");
        }
        if (!out.grantsEffect.empty()) {
            return MC_FAIL("'grants_effects' is mutually exclusive with "
                           "'grants_effect' (pick one)");
        }
        // Only food grants an effect on eat (same as the single field).
        if (out.restoreHunger <= 0) {
            return MC_FAIL("'grants_effects' requires 'restore_hunger' "
                           "(only food grants an effect on eat)");
        }
        for (std::size_t i = 0; i < gel->size(); ++i) {
            const Json& e = gel->at(i);
            if (!e.isObject()) {
                return MC_FAIL("'grants_effects[" + std::to_string(i) +
                               "]' must be an object");
            }
            for (const auto& [key, value] : e.members()) {
                if (key != "name" && key != "duration") {
                    return MC_FAIL("'grants_effects[" + std::to_string(i) +
                                   "]' has an unknown field '" + key + "'");
                }
            }
            const Json* nm = e.find("name");
            if (nm == nullptr || !nm->isString() || nm->asString().empty()) {
                return MC_FAIL("'grants_effects[" + std::to_string(i) +
                               "]' requires a non-empty string 'name'");
            }
            const Json* du = e.find("duration");
            if (du == nullptr || !du->isNumber() || du->asDouble() <= 0.0) {
                return MC_FAIL("'grants_effects[" + std::to_string(i) +
                               "]' requires 'duration' (seconds) > 0");
            }
            Item::GrantEffect g;
            g.name = nm->asString();
            g.duration = du->asDouble();
            out.grantsEffects.push_back(std::move(g));
        }
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
            key != "collects_fluid" &&
            key != "pours_fluid" && key != "pours_becomes" &&
            key != "grants_effect" &&
            key != "grants_effects" &&
            key != "ignites_flamable" &&
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
    r = parseGrant(def, item); // after parseFood/parseDrink: food-only check
    MC_TRY(r);
    r = parseFluidBucket(def, item); // after parseFood/parseDrink: the
                                     // channel-ownership exclusivity
    MC_TRY(r);
    r = parseIgnites(def, item); // last: the channel-ownership exclusivity
                                 // checks need every other channel parsed
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

const Item* ItemRegistry::fullBucketFor(const std::string& fluidName,
                                        const std::string& emptyName) const {
    for (const Item& item : m_items) {
        if (item.poursFluid == fluidName && item.poursBecomes == emptyName) {
            return &item;
        }
    }
    return nullptr;
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
