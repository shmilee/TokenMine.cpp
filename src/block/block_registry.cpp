#include "block/block_registry.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>

#include "core/logger.hpp"
#include "world/chunk.hpp" // fluidKindForName (the fluid vocabulary)

namespace mc {

namespace {

// The reserved air sentinel: id 0, "no block". Not data-driven (no JSON,
// no textures) — chunk storage defaults to it and the mesher never emits
// its faces. Registered in every BlockRegistry so at(BlockID::air()) and
// findByName("air") always work.
Block makeAirBlock() {
    Block air;
    air.id = BlockID::air();
    air.name = "air";
    air.properties.solid = false;
    air.properties.opaque = false;
    air.properties.transparent = false;
    air.properties.destroyable = false;
    air.properties.hardness = 0.0f;
    return air;
}

} // namespace

BlockRegistry::BlockRegistry() {
    m_blocks.push_back(makeAirBlock());
    m_names.emplace("air", BlockID::air().value);
}

void BlockRegistry::clear() {
    // Reset to the pristine state (built-in air sentinel only).
    *this = BlockRegistry{};
}

namespace {

// Typed field reader: returns an error naming the field when the JSON value
// has the wrong type (no silent fallbacks for malformed data files).
template <typename F>
Result<void> checkField(const Json& obj, const std::string& key,
                        const char* type, F&& apply) {
    const Json* v = obj.find(key);
    if (v == nullptr) {
        return {}; // absent = keep default
    }
    const bool typeOk = (std::string(type) == "string") ? v->isString()
                   : (std::string(type) == "number") ? v->isNumber()
                                                     : v->isBool();
    if (!typeOk) {
        return MC_FAIL("block property '" + key + "' must be a " + type);
    }
    apply(*v);
    return {};
}

} // namespace

Result<void> BlockRegistry::parseProperties(const Json& def,
                                            BlockProperties& out) {
    const Json* p = def.find("properties");
    if (p == nullptr) {
        return {}; // all defaults
    }
    if (!p->isObject()) {
        return MC_FAIL("'properties' must be an object");
    }

    auto r = checkField(*p, "solid", "boolean",
                        [&](const Json& v) { out.solid = v.asBool(); });
    MC_TRY(r);
    r = checkField(*p, "opaque", "boolean",
                   [&](const Json& v) { out.opaque = v.asBool(); });
    MC_TRY(r);
    r = checkField(*p, "transparent", "boolean",
                   [&](const Json& v) { out.transparent = v.asBool(); });
    MC_TRY(r);
    r = checkField(*p, "destroyable", "boolean",
                   [&](const Json& v) { out.destroyable = v.asBool(); });
    MC_TRY(r);
    r = checkField(*p, "flamable", "boolean",
                   [&](const Json& v) { out.flamable = v.asBool(); });
    MC_TRY(r);
    r = checkField(*p, "fluid", "string",
                   [&](const Json& v) { out.fluidName = v.asString(); });
    MC_TRY(r);
    r = checkField(*p, "hardness", "number",
                   [&](const Json& v) { out.hardness = v.asDouble(); });
    MC_TRY(r);
    r = checkField(*p, "light_emission", "number",
                   [&](const Json& v) { out.lightEmission = v.asInt(); });
    MC_TRY(r);
    r = checkField(*p, "drops", "string",
                   [&](const Json& v) { out.drops = v.asString(); });
    MC_TRY(r);
    r = checkField(*p, "drops_count", "number",
                   [&](const Json& v) { out.dropsCount = v.asInt(); });
    MC_TRY(r);
    r = checkField(*p, "interact", "string",
                   [&](const Json& v) { out.interact = v.asString(); });
    MC_TRY(r);
    r = checkField(*p, "dye_base", "string",
                   [&](const Json& v) { out.dyeBase = v.asString(); });
    MC_TRY(r);
    r = checkField(*p, "dye_color", "string",
                   [&](const Json& v) { out.dyeColor = v.asString(); });
    MC_TRY(r);
    r = checkField(*p, "grows_to", "string",
                   [&](const Json& v) { out.growsTo = v.asString(); });
    MC_TRY(r);
    r = checkField(*p, "growth_ticks", "number",
                   [&](const Json& v) { out.growthTicks = v.asInt(); });
    MC_TRY(r);
    r = checkField(*p, "grows_on", "string",
                   [&](const Json& v) { out.growsOn = v.asString(); });
    MC_TRY(r);
    // Planting placement restriction (Phase 53): an OPTIONAL array of block
    // names. When present, a placement of this block is allowed only when
    // the cell DIRECTLY BELOW the target holds one of the listed blocks (the
    // soil) — a placement-time gate, independent of the growth-time grows_on
    // gate (Phase 50). Absent = unrestricted (the pre-Phase-53 behavior); an
    // empty array is a no-op and rejected (the ammo discipline). Each name
    // must be a non-empty string (the block-existence cross-check happens at
    // game init, the grows_on discipline).
    if (const Json* po = p->find("plant_on")) {
        if (!po->isArray()) {
            return MC_FAIL("block property 'plant_on' must be an array of "
                           "block names");
        }
        if (po->size() == 0) {
            return MC_FAIL("block property 'plant_on' must not be an empty "
                           "array (omit it when the block can be placed "
                           "anywhere)");
        }
        for (std::size_t i = 0; i < po->size(); ++i) {
            const Json& v = po->at(i);
            if (!v.isString() || v.asString().empty()) {
                return MC_FAIL("block property 'plant_on[" +
                               std::to_string(i) + "]' must be a non-empty "
                               "block name string");
            }
            out.plantOn.push_back(v.asString());
        }
    }
    // Crop environment (Phase 50): integer fields with explicit
    // integer-ness + range checks (the restore_hunger discipline).
    if (const Json* gl = p->find("growth_light")) {
        if (!gl->isNumber()) {
            return MC_FAIL("block property 'growth_light' must be a number");
        }
        const double d = gl->asDouble();
        if (d != std::floor(d)) {
            return MC_FAIL("block property 'growth_light' must be an "
                           "integer (0..15)");
        }
        out.growthLight = static_cast<int>(d);
        if (out.growthLight < 0 || out.growthLight > 15) {
            return MC_FAIL("block property 'growth_light' must be in "
                           "[0, 15], got " + std::to_string(out.growthLight));
        }
    }
    if (const Json* ds = p->find("dry_slowdown")) {
        if (!ds->isNumber()) {
            return MC_FAIL("block property 'dry_slowdown' must be a number");
        }
        const double d = ds->asDouble();
        if (d != std::floor(d)) {
            return MC_FAIL("block property 'dry_slowdown' must be an "
                           "integer (1..64)");
        }
        out.drySlowdown = static_cast<int>(d);
        if (out.drySlowdown < 1 || out.drySlowdown > 64) {
            return MC_FAIL("block property 'dry_slowdown' must be in "
                           "[1, 64], got " + std::to_string(out.drySlowdown));
        }
    }
    // Chance secondary drop (Phase 49): an OPTIONAL object
    // {"item": <name>, "count": N, "chance": (0,1]} — an extra drop rolled
    // per break, independent of the primary `drops`. Parsed field-by-field
    // below (the three keys are a strict triple, validated after the loop).
    if (const Json* cd = p->find("chance_drop")) {
        if (!cd->isObject()) {
            return MC_FAIL("block property 'chance_drop' must be an object "
                           "{\"item\": ..., \"count\": ..., \"chance\": ...}");
        }
        for (const auto& [key, value] : cd->members()) {
            if (key != "item" && key != "count" && key != "chance") {
                return MC_FAIL("block property 'chance_drop' has an unknown "
                               "field '" + key + "'");
            }
        }
        r = checkField(*cd, "item", "string",
                       [&](const Json& v) { out.chanceDropItem = v.asString(); });
        MC_TRY(r);
        r = checkField(*cd, "count", "number",
                       [&](const Json& v) { out.chanceDropCount = v.asInt(); });
        MC_TRY(r);
        r = checkField(*cd, "chance", "number",
                       [&](const Json& v) { out.chanceDropChance = v.asDouble(); });
        MC_TRY(r);
    }
    // Chance secondary drop LIST (Phase 52): an OPTIONAL array of the same
    // strict triples as the single `chance_drop` above. Each entry rolls
    // independently (the entry-indexed salt — block/chance_drop). The array
    // is mutually exclusive with the single `chance_drop` (checked in the
    // validation pass, where both are known); an empty array is a no-op and
    // rejected (the ammo discipline).
    if (const Json* cdl = p->find("chance_drops")) {
        if (!cdl->isArray()) {
            return MC_FAIL("block property 'chance_drops' must be an array "
                           "of {\"item\": ..., \"count\": ..., \"chance\": ...} "
                           "objects");
        }
        if (cdl->size() == 0) {
            return MC_FAIL("block property 'chance_drops' must not be an "
                           "empty array (omit it when the block has no "
                           "chance drops)");
        }
        for (std::size_t i = 0; i < cdl->size(); ++i) {
            const Json& entry = cdl->at(i);
            const std::string at =
                "block property 'chance_drops[" + std::to_string(i) + "]'";
            if (!entry.isObject()) {
                return MC_FAIL(at + " must be an object {\"item\": ..., "
                                   "\"count\": ..., \"chance\": ...}");
            }
            for (const auto& [key, value] : entry.members()) {
                if (key != "item" && key != "count" && key != "chance") {
                    return MC_FAIL(at + " has an unknown field '" + key + "'");
                }
            }
            ChanceDrop cdEntry;
            r = checkField(entry, "item", "string",
                           [&](const Json& v) { cdEntry.item = v.asString(); });
            MC_TRY(r);
            r = checkField(entry, "count", "number",
                           [&](const Json& v) { cdEntry.count = v.asInt(); });
            MC_TRY(r);
            r = checkField(entry, "chance", "number",
                           [&](const Json& v) { cdEntry.chance = v.asDouble(); });
            MC_TRY(r);
            out.chanceDrops.push_back(std::move(cdEntry));
        }
    }
    // Water reaction (Phase 51): an OPTIONAL object
    // {"source": <block name>, "flowing": <block name>} — the blocks a cell
    // of this fluid converts into when 6-adjacent to a water cell (see
    // world/fluid). A strict pair of non-empty names on a fluid block; the
    // names are cross-checked at game init (the ammo pattern).
    if (const Json* wr = p->find("water_reaction")) {
        if (!wr->isObject()) {
            return MC_FAIL("block property 'water_reaction' must be an "
                           "object {\"source\": ..., \"flowing\": ...}");
        }
        for (const auto& [key, value] : wr->members()) {
            if (key != "source" && key != "flowing") {
                return MC_FAIL("block property 'water_reaction' has an "
                               "unknown field '" + key + "'");
            }
        }
        r = checkField(*wr, "source", "string",
                       [&](const Json& v) {
                           out.waterReactionSource = v.asString();
                       });
        MC_TRY(r);
        r = checkField(*wr, "flowing", "string",
                       [&](const Json& v) {
                           out.waterReactionFlowing = v.asString();
                       });
        MC_TRY(r);
        if (out.waterReactionSource.empty() ||
            out.waterReactionFlowing.empty()) {
            return MC_FAIL("block property 'water_reaction' requires both a "
                           "non-empty 'source' and 'flowing' block name "
                           "(a strict pair)");
        }
        if (out.fluidName.empty()) {
            return MC_FAIL("block property 'water_reaction' requires the "
                           "block to be a fluid ('fluid' property) — only a "
                           "fluid cell reacts to water");
        }
    }

    if (out.dropsCount < 1 || out.dropsCount > 64) {
        return MC_FAIL("'drops_count' must be in [1, 64], got " +
                       std::to_string(out.dropsCount));
    }
    if (out.drops.empty() && out.dropsCount != 1) {
        return MC_FAIL("'drops_count' without 'drops' is meaningless "
                       "(got " +
                       std::to_string(out.dropsCount) + ")");
    }
    if (!out.fluidName.empty() && !fluidKindForName(out.fluidName)) {
        return MC_FAIL("block property 'fluid' has an unknown fluid name '" +
                       out.fluidName + "' (vocabulary: \"water\", \"lava\")");
    }
    if (out.hardness < 0.0) {
        return MC_FAIL("'hardness' must be >= 0, got " +
                       std::to_string(out.hardness));
    }
    if (out.lightEmission < 0 || out.lightEmission > 15) {
        return MC_FAIL("'light_emission' must be in [0, 15], got " +
                       std::to_string(out.lightEmission));
    }
    if (!out.interact.empty() && out.interact != kInteractCraftingTable &&
        out.interact != kInteractFurnace) {
        return MC_FAIL("block property 'interact' is unsupported: '" +
                       out.interact + "' (only '" +
                       std::string(kInteractCraftingTable) + "' and '" +
                       std::string(kInteractFurnace) + "' exist)");
    }
    // Dye family (Phase 41): both fields are optional; an EXPLICIT empty
    // string is a typo (ABSENT is the "not dyeable" default — the ammo
    // discipline). dye_color requires dye_base; a dyeable block cannot
    // also open a GUI.
    if (const Json* db = p->find("dye_base")) {
        if (db->asString().empty()) {
            return MC_FAIL("block property 'dye_base' must be a non-empty "
                           "family name (omit it when the block is not "
                           "dyeable)");
        }
    }
    if (const Json* dc = p->find("dye_color")) {
        if (dc->asString().empty()) {
            return MC_FAIL("block property 'dye_color' must be a non-empty "
                           "color name (omit it when the block has no dye "
                           "color)");
        }
    }
    if (!out.dyeColor.empty() && out.dyeBase.empty()) {
        return MC_FAIL("block property 'dye_color' requires 'dye_base' "
                       "(got '" + out.dyeColor + "')");
    }
    if (!out.dyeBase.empty() && !out.interact.empty()) {
        return MC_FAIL("a block cannot be both dyeable ('dye_base: '" +
                       out.dyeBase + "') and interactive ('interact: '" +
                       out.interact + "')");
    }
    if (out.opaque && out.transparent) {
        return MC_FAIL("block cannot be both 'opaque' and 'transparent'");
    }
    // Crop growth (Phase 48): both fields are optional; an EXPLICIT empty
    // growsTo is a typo (ABSENT is the "not a crop" default — the ammo
    // discipline). The two fields are a strict pair: each one requires
    // the other (the game init cross-checks that growsTo names an existing
    // block; here the pairing + range + no self-reference are enforced).
    if (const Json* gt = p->find("grows_to")) {
        if (gt->asString().empty()) {
            return MC_FAIL("block property 'grows_to' must be a non-empty "
                           "block name (omit it when the block is mature "
                           "or not a crop)");
        }
        const Json* id = def.find("id");
        if (id != nullptr && id->isString() &&
            id->asString() == out.growsTo) {
            return MC_FAIL("block property 'grows_to' must not reference "
                           "the block itself ('" + out.growsTo + "')");
        }
    }
    if (out.growsTo.empty() && out.growthTicks != 0) {
        return MC_FAIL("'growth_ticks' without 'grows_to' is meaningless "
                       "(got " +
                       std::to_string(out.growthTicks) + ")");
    }
    if (!out.growsTo.empty() &&
        (out.growthTicks < 1 || out.growthTicks > 1000000)) {
        return MC_FAIL("'grows_to' requires 'growth_ticks' in [1, 1000000], "
                       "got " +
                       std::to_string(out.growthTicks));
    }
    // Chance secondary drop (Phase 49): a present 'chance_drop' object is a
    // strict TRIPLE — item (non-empty), count ([1,64]), chance ((0,1]).
    // (The item's existence + max-stack fit are cross-checked at game init,
    // the drops discipline.)
    if (p->find("chance_drop") != nullptr) {
        if (out.chanceDropItem.empty()) {
            return MC_FAIL("block property 'chance_drop.item' must be a "
                           "non-empty item name");
        }
        if (out.chanceDropCount < 1 || out.chanceDropCount > 64) {
            return MC_FAIL("block property 'chance_drop.count' must be in "
                           "[1, 64], got " +
                           std::to_string(out.chanceDropCount));
        }
        if (out.chanceDropChance <= 0.0 || out.chanceDropChance > 1.0) {
            return MC_FAIL("block property 'chance_drop.chance' must be in "
                           "(0, 1], got " +
                           std::to_string(out.chanceDropChance));
        }
    }
    // Chance drop list (Phase 52): MUTUALLY EXCLUSIVE with the single
    // 'chance_drop' (a block has at most one chance-drop system), and each
    // entry is the same strict triple — item (non-empty), count ([1,64]),
    // chance ((0,1]). (The items' existence + max-stack fit are
    // cross-checked at game init, the drops discipline.)
    if (p->find("chance_drops") != nullptr) {
        if (p->find("chance_drop") != nullptr) {
            return MC_FAIL("block 'chance_drops' (the list) is mutually "
                           "exclusive with 'chance_drop' (the single entry): "
                           "write exactly one of the two");
        }
        for (std::size_t i = 0; i < out.chanceDrops.size(); ++i) {
            const ChanceDrop& e = out.chanceDrops[i];
            const std::string at = "block property 'chance_drops[" +
                                   std::to_string(i) + "].";
            if (e.item.empty()) {
                return MC_FAIL(at + "item' must be a non-empty item name");
            }
            if (e.count < 1 || e.count > 64) {
                return MC_FAIL(at + "count' must be in [1, 64], got " +
                               std::to_string(e.count));
            }
            if (e.chance <= 0.0 || e.chance > 1.0) {
                return MC_FAIL(at + "chance' must be in (0, 1], got " +
                               std::to_string(e.chance));
            }
        }
    }
    // Soil requirement (Phase 50): an EXPLICIT empty grows_on is a typo
    // (ABSENT is the "no soil requirement" default — the ammo discipline);
    // the non-empty name is cross-checked at game init (it must reference
    // an existing block, the grows_to discipline).
    if (p->find("grows_on") != nullptr && out.growsOn.empty()) {
        return MC_FAIL("block property 'grows_on' must be a non-empty block "
                       "name (omit it when the crop needs no specific soil)");
    }
    return {};
}

Result<void> BlockRegistry::parseModel(const Json& def, BlockModel& out) {
    const Json* m = def.find("model");
    if (m == nullptr) {
        return MC_FAIL("block requires a 'model' object");
    }
    if (!m->isObject()) {
        return MC_FAIL("'model' must be an object");
    }

    // Shape is optional: full_cube is the default. Phase 45: "cross" =
    // two crossing vertical quads (cross plants, e.g. flowers).
    if (const Json* s = m->find("shape")) {
        if (!s->isString() || (s->asString() != "full_cube" &&
                               s->asString() != "cross")) {
            return MC_FAIL(std::string("'model.shape' is unsupported: '") +
                           s->asString() +
                           "' (only 'full_cube' and 'cross' exist)");
        }
        out.shape = (s->asString() == "cross") ? BlockModel::Shape::Cross
                                               : BlockModel::Shape::FullCube;
    }

    const Json* t = m->find("textures");
    if (t == nullptr || !t->isObject()) {
        return MC_FAIL("block requires a 'model.textures' object");
    }
    if (out.shape == BlockModel::Shape::Cross) {
        // Cross shapes: exactly one texture key ("cross"). The cube face
        // keys would be dead data (no cube faces are emitted), so they are
        // rejected (the ammo discipline).
        const Json* c = t->find("cross");
        if (c == nullptr || !c->isString() || c->asString().empty()) {
            return MC_FAIL("model.textures.cross is required for shape "
                           "'cross' and must be a non-empty string");
        }
        for (const char* face : {"top", "side", "bottom"}) {
            if (t->find(face) != nullptr) {
                return MC_FAIL(std::string("model.textures.") + face +
                               " is not valid for shape 'cross' (use a "
                               "single 'cross' texture)");
            }
        }
        out.textures.cross = c->asString();
        return {};
    }
    // Full cubes: exactly the three face-group keys (an explicit "cross"
    // key is a typo for a cross shape — rejected, not ignored).
    if (t->find("cross") != nullptr) {
        return MC_FAIL("model.textures.cross is only valid for shape "
                       "'cross' (full cubes use top/side/bottom)");
    }
    for (const char* face : {"top", "side", "bottom"}) {
        const Json* v = t->find(face);
        if (v == nullptr || !v->isString() || v->asString().empty()) {
            return MC_FAIL(std::string("model.textures.") + face +
                           " is required and must be a non-empty string");
        }
    }
    out.textures.top = t->find("top")->asString();
    out.textures.side = t->find("side")->asString();
    out.textures.bottom = t->find("bottom")->asString();
    return {};
}

Result<BlockID> BlockRegistry::registerBlock(const Json& def) {
    if (!def.isObject()) {
        return MC_FAIL("block definition must be a JSON object");
    }

    const Json* idJson = def.find("id");
    if (idJson == nullptr || !idJson->isString() || idJson->asString().empty()) {
        return MC_FAIL("block definition requires a non-empty string 'id'");
    }
    const std::string name = idJson->asString();

    if (m_names.count(name) != 0) {
        return MC_FAIL("duplicate block id '" + name + "'");
    }

    // Warn about unknown top-level keys (typos, future fields).
    for (const auto& [key, value] : def.members()) {
        if (key != "id" && key != "properties" && key != "model") {
            MC_LOG_WARN("Block '" + name + "': unknown field '" + key +
                        "' ignored");
        }
    }

    Block block;
    block.name = name;

    auto r = parseProperties(def, block.properties);
    MC_TRY(r);
    r = parseModel(def, block.model);
    MC_TRY(r);

    // Cross plants (Phase 45): they render as two blended quads in the
    // transparent pass and are walk-through, so the property combination is
    // forced (an opaque/solid cross would occlude faces/light it does not
    // visually cover).
    if (block.model.shape == BlockModel::Shape::Cross) {
        if (block.properties.opaque) {
            return MC_FAIL("cross-plant block '" + name +
                           "' must not be 'opaque' (it renders as two "
                           "blended quads)");
        }
        if (block.properties.solid) {
            return MC_FAIL("cross-plant block '" + name +
                           "' must not be 'solid' (cross plants are "
                           "walk-through)");
        }
        if (!block.properties.transparent) {
            return MC_FAIL("cross-plant block '" + name +
                           "' must be 'transparent' (it renders in the "
                           "blending pass)");
        }
    }

    block.id = BlockID{static_cast<int>(m_blocks.size())};
    m_names.emplace(name, block.id.value);
    m_blocks.push_back(std::move(block));
    MC_LOG_DEBUG("Registered block '" + name + "' (id=" +
                 std::to_string(block.id.value) + ")");
    return block.id;
}

Result<void> BlockRegistry::loadDirectory(AssetManager& assets,
                                          const std::string& relativeDir) {
    auto dir = assets.resolve(relativeDir);
    MC_TRY(dir);
    if (!std::filesystem::is_directory(*dir)) {
        MC_LOG_WARN("Block directory not found: '" + (*dir).string() +
                    "' (no blocks loaded)");
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
            return MC_FAIL("invalid JSON in block file '" + rel + "': " +
                           parsed.error().message);
        }
        auto id = registerBlock(*parsed);
        if (!id.ok()) {
            return MC_FAIL("block file '" + rel + "': " + id.error().message);
        }
    }
    return {};
}

const Block& BlockRegistry::at(BlockID id) const {
    // Contract: caller holds a valid id from this registry.
    assert(id.valid() && id.value < static_cast<int>(m_blocks.size()));
    return m_blocks[static_cast<std::size_t>(id.value)];
}

const Block* BlockRegistry::find(BlockID id) const {
    if (!id.valid() || id.value >= static_cast<int>(m_blocks.size())) {
        return nullptr;
    }
    return &m_blocks[static_cast<std::size_t>(id.value)];
}

const Block* BlockRegistry::findByName(const std::string& name) const {
    const auto it = m_names.find(name);
    return it == m_names.end() ? nullptr : &m_blocks[static_cast<std::size_t>(it->second)];
}

BlockID BlockRegistry::idForName(const std::string& name) const {
    const auto it = m_names.find(name);
    return it == m_names.end() ? BlockID::invalid() : BlockID{it->second};
}

std::optional<BlockID> BlockRegistry::fluidBlock(
    const std::string& fluidName) const {
    // The reference block of a fluid KIND (properties.fluid names the
    // kind): its textures light the mesher's fluid faces and its
    // light_emission makes cells of that kind glow (see world/lighting).
    // nullopt = the kind has no reference block.
    for (const Block& b : m_blocks) {
        if (b.properties.fluidName == fluidName) {
            return b.id;
        }
    }
    return std::nullopt;
}

std::vector<BlockID> BlockRegistry::allIds() const {
    std::vector<BlockID> ids;
    ids.reserve(m_blocks.size());
    for (const auto& b : m_blocks) {
        ids.push_back(b.id);
    }
    return ids;
}

std::set<std::string> BlockRegistry::requiredTextures() const {
    std::set<std::string> names;
    for (const auto& b : m_blocks) {
        // Empty names mean "no texture" (the built-in air sentinel) and
        // never appear in a valid data block (parseModel rejects them).
        if (!b.model.textures.top.empty()) {
            names.insert(b.model.textures.top);
        }
        if (!b.model.textures.side.empty()) {
            names.insert(b.model.textures.side);
        }
        if (!b.model.textures.bottom.empty()) {
            names.insert(b.model.textures.bottom);
        }
        if (!b.model.textures.cross.empty()) {
            names.insert(b.model.textures.cross);
        }
    }
    return names;
}

} // namespace mc
