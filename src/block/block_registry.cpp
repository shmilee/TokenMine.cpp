#include "block/block_registry.hpp"

#include <algorithm>
#include <cassert>
#include <filesystem>

#include "core/logger.hpp"

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
    r = checkField(*p, "fluid", "boolean",
                   [&](const Json& v) { out.fluid = v.asBool(); });
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

    if (out.dropsCount < 1 || out.dropsCount > 64) {
        return MC_FAIL("'drops_count' must be in [1, 64], got " +
                       std::to_string(out.dropsCount));
    }
    if (out.drops.empty() && out.dropsCount != 1) {
        return MC_FAIL("'drops_count' without 'drops' is meaningless "
                       "(got " +
                       std::to_string(out.dropsCount) + ")");
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
    if (out.opaque && out.transparent) {
        return MC_FAIL("block cannot be both 'opaque' and 'transparent'");
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

    // Shape is optional: full_cube is the default.
    if (const Json* s = m->find("shape")) {
        if (!s->isString() || s->asString() != "full_cube") {
            return MC_FAIL(std::string("'model.shape' is unsupported: '") +
                           s->asString() + "' (only 'full_cube' exists)");
        }
        out.shape = BlockModel::Shape::FullCube;
    }

    const Json* t = m->find("textures");
    if (t == nullptr || !t->isObject()) {
        return MC_FAIL("block requires 'model.textures' with top/side/bottom");
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

std::optional<BlockID> BlockRegistry::fluidBlock() const {
    // The air sentinel is first; the first fluid property block wins (the
    // game defines exactly one).
    for (const Block& b : m_blocks) {
        if (b.properties.fluid) {
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
    }
    return names;
}

} // namespace mc
