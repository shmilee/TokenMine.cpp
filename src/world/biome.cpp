#include "world/biome.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

#include "core/logger.hpp"
#include "world/tree_shape.hpp"

namespace mc {

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
        return MC_FAIL("biome field '" + key + "' must be a " + type);
    }
    apply(*v);
    return {};
}

} // namespace

Result<void> BiomeRegistry::registerBiome(const Json& def) {
    if (!def.isObject()) {
        return MC_FAIL("biome definition must be a JSON object");
    }

    const Json* idJson = def.find("id");
    if (idJson == nullptr || !idJson->isString() || idJson->asString().empty()) {
        return MC_FAIL("biome definition requires a non-empty string 'id'");
    }
    const std::string name = idJson->asString();

    if (findByName(name) != nullptr) {
        return MC_FAIL("duplicate biome id '" + name + "'");
    }

    // Warn about unknown top-level keys (typos, future fields).
    for (const auto& [key, value] : def.members()) {
        if (key != "id" && key != "weight" && key != "surface" &&
            key != "subsurface" && key != "subsurface_depth" &&
            key != "tree_density" && key != "height_scale" &&
            key != "decorations" && key != "tree" &&
            key != "exclude_spawn_area") {
            MC_LOG_WARN("Biome '" + name + "': unknown field '" + key +
                        "' ignored");
        }
    }

    Biome biome;
    biome.name = name;

    auto r = checkField(def, "weight", "number",
                        [&](const Json& v) { biome.weight = v.asInt(); });
    MC_TRY(r);
    r = checkField(def, "surface", "string",
                   [&](const Json& v) { biome.surface = v.asString(); });
    MC_TRY(r);
    r = checkField(def, "subsurface", "string",
                   [&](const Json& v) { biome.subsurface = v.asString(); });
    MC_TRY(r);
    r = checkField(def, "subsurface_depth", "number",
                   [&](const Json& v) { biome.subsurfaceDepth = v.asInt(); });
    MC_TRY(r);
    r = checkField(def, "tree_density", "number",
                   [&](const Json& v) { biome.treeDensity = v.asDouble(); });
    MC_TRY(r);
    r = checkField(def, "height_scale", "number",
                   [&](const Json& v) { biome.heightScale = v.asDouble(); });
    MC_TRY(r);

    // Optional 'decorations' (Phase 45): an array of { "block",
    // "probability" } entries. Strict per entry (unknown keys are typos);
    // the block NAME is validated against the BlockRegistry at generation
    // setup (the biome file loads before the registry is cross-checked).
    if (const Json* decos = def.find("decorations")) {
        if (!decos->isArray()) {
            return MC_FAIL("biome '" + name +
                           "': 'decorations' must be an array");
        }
        for (std::size_t i = 0; i < decos->size(); ++i) {
            const Json& de = decos->at(i);
            const std::string label = "biome '" + name +
                                      "' decorations[" + std::to_string(i) +
                                      "]";
            if (!de.isObject()) {
                return MC_FAIL(label + " must be an object");
            }
            for (const auto& [key, value] : de.members()) {
                if (key != "block" && key != "probability") {
                    return MC_FAIL(label + " has unknown field '" + key +
                                   "' (only 'block' and 'probability' "
                                   "exist)");
                }
            }
            const Json* blk = de.find("block");
            if (blk == nullptr || !blk->isString() ||
                blk->asString().empty()) {
                return MC_FAIL(label + " requires a non-empty string "
                                   "'block'");
            }
            BiomeDecoration deco;
            deco.block = blk->asString();
            const Json* prob = de.find("probability");
            if (prob == nullptr || !prob->isNumber()) {
                return MC_FAIL(label + " requires a numeric "
                                   "'probability'");
            }
            deco.probability = static_cast<float>(prob->asDouble());
            biome.decorations.push_back(std::move(deco));
        }
    }

    // V2-16: 'exclude_spawn_area' — a STRICT bool (the render.weather
    // strict-read discipline): absent = the default false, present but
    // non-boolean = named rejection (a 1 / "true" typo would silently
    // change the spawn-zone selection).
    if (const Json* excl = def.find("exclude_spawn_area")) {
        if (!excl->isBool()) {
            return MC_FAIL("biome '" + name +
                           "': 'exclude_spawn_area' must be a boolean");
        }
        biome.excludeSpawnArea = excl->asBool();
    }

    // V2-16: the optional 'tree' object — a strict SIX-KEY
    // {"trunk", "leaves", "min_height", "height_range", "leaf_radius",
    //  "shape"}, the SAME field face as a sapling block's tree object
    // (the block_registry parsing precedent: every key present and
    // typed, unknown keys rejected by name, `tree.<key>` addressing).
    // The trunk / leaves NAMES are cross-checked against the
    // BlockRegistry at generation setup (WorldGenerator::create); the
    // shape vocabulary + the integer ranges are checked here.
    if (const Json* tr = def.find("tree")) {
        // The `biome '<name>': 'tree...'` addressing (the existing
        // field-message style, the sapling tree.<key> discipline).
        const std::string p = "biome '" + name + "': 'tree";
        if (!tr->isObject()) {
            return MC_FAIL(p + "' must be an object "
                            "{\"trunk\": ..., \"leaves\": ..., "
                            "\"min_height\": ..., \"height_range\": ..., "
                            "\"leaf_radius\": ..., \"shape\": ...}");
        }
        for (const auto& [key, value] : tr->members()) {
            if (key != "trunk" && key != "leaves" &&
                key != "min_height" && key != "height_range" &&
                key != "leaf_radius" && key != "shape") {
                return MC_FAIL(p + "' has an unknown field '" + key + "'");
            }
        }
        BiomeTreeSpec tree;
        // Strict six keys: every key present (a missing one is a no-op
        // typo — named).
        for (const char* key : {"trunk", "leaves", "min_height",
                                "height_range", "leaf_radius", "shape"}) {
            if (tr->find(key) == nullptr) {
                return MC_FAIL(p + "." + key + "' is required (a strict "
                                       "six-key object)");
            }
        }
        auto trc = checkField(*tr, "trunk", "string",
                              [&](const Json& v) { tree.trunk = v.asString(); });
        MC_TRY(trc);
        trc = checkField(*tr, "leaves", "string",
                         [&](const Json& v) { tree.leaves = v.asString(); });
        MC_TRY(trc);
        trc = checkField(*tr, "shape", "string",
                         [&](const Json& v) { tree.shape = v.asString(); });
        MC_TRY(trc);
        // The three integer fields: explicit integer-ness + range (the
        // sapling tree.<key> discipline).
        const struct { const char* key; int* out; int lo; int hi; } ints[] = {
            {"min_height", &tree.minHeight, 1, 16},
            {"height_range", &tree.heightRange, 0, 8},
            {"leaf_radius", &tree.leafRadius, 1, 2},
        };
        for (const auto& f : ints) {
            const Json* v = tr->find(f.key);
            if (!v->isNumber() || v->asDouble() != std::floor(v->asDouble())) {
                return MC_FAIL(p + "." + f.key + "' must be an integer in "
                                       "[" + std::to_string(f.lo) + ", " +
                                       std::to_string(f.hi) + "]");
            }
            *f.out = static_cast<int>(v->asDouble());
            if (*f.out < f.lo || *f.out > f.hi) {
                return MC_FAIL(p + "." + f.key + "' must be in [" +
                               std::to_string(f.lo) + ", " +
                               std::to_string(f.hi) + "], got " +
                               std::to_string(*f.out));
            }
        }
        if (tree.trunk.empty()) {
            return MC_FAIL(p + ".trunk' must be a non-empty block name");
        }
        if (tree.leaves.empty()) {
            return MC_FAIL(p + ".leaves' must be a non-empty block name");
        }
        if (!treeShapeValid(tree.shape)) {
            return MC_FAIL(p + ".shape' is unsupported: '" + tree.shape +
                           "' (only 'oak', 'spruce' and 'birch' exist)");
        }
        biome.tree = std::move(tree);
    }

    if (biome.weight < 1) {
        return MC_FAIL("biome '" + name + "': 'weight' must be >= 1, got " +
                       std::to_string(biome.weight));
    }
    if (biome.surface.empty() || biome.subsurface.empty()) {
        return MC_FAIL("biome '" + name + "': 'surface' and 'subsurface' "
                       "must be non-empty block names");
    }
    if (biome.subsurfaceDepth < 1 || biome.subsurfaceDepth > 16) {
        return MC_FAIL("biome '" + name + "': 'subsurface_depth' must be in "
                       "[1, 16], got " +
                       std::to_string(biome.subsurfaceDepth));
    }
    if (biome.treeDensity < 0.0f || biome.treeDensity > 1.0f) {
        return MC_FAIL("biome '" + name + "': 'tree_density' must be in "
                       "[0, 1]");
    }
    if (biome.heightScale <= 0.0f || biome.heightScale > 5.0f) {
        return MC_FAIL("biome '" + name + "': 'height_scale' must be in "
                       "(0, 5], got " + std::to_string(biome.heightScale));
    }
    for (std::size_t i = 0; i < biome.decorations.size(); ++i) {
        const float p = biome.decorations[i].probability;
        if (p < 0.0f || p > 1.0f) {
            return MC_FAIL("biome '" + name + "' decorations[" +
                           std::to_string(i) + "] 'probability' must be "
                           "in [0, 1], got " + std::to_string(p));
        }
    }

    m_biomes.push_back(std::move(biome));
    MC_LOG_DEBUG("Registered biome '" + name + "' (weight=" +
                 std::to_string(m_biomes.back().weight) + ")");
    return {};
}

Result<void> BiomeRegistry::loadDirectory(AssetManager& assets,
                                          const std::string& relativeDir) {
    auto dir = assets.resolve(relativeDir);
    MC_TRY(dir);
    if (!std::filesystem::is_directory(*dir)) {
        MC_LOG_WARN("Biome directory not found: '" + (*dir).string() +
                    "' (no biomes loaded)");
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
            return MC_FAIL("invalid JSON in biome file '" + rel + "': " +
                           parsed.error().message);
        }
        auto r = registerBiome(*parsed);
        if (!r.ok()) {
            return MC_FAIL("biome file '" + rel + "': " + r.error().message);
        }
    }
    return {};
}

const Biome* BiomeRegistry::findByName(const std::string& name) const {
    for (const Biome& b : m_biomes) {
        if (b.name == name) {
            return &b;
        }
    }
    return nullptr;
}

int BiomeRegistry::totalWeight() const {
    int total = 0;
    for (const Biome& b : m_biomes) {
        total += b.weight;
    }
    return total;
}

} // namespace mc
