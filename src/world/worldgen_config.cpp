#include "world/worldgen_config.hpp"

#include <string>
#include <vector>

#include "core/logger.hpp"

namespace mc {

namespace {

// Section field readers. Each validates the JSON type and produces an error
// that names both the section and the key (no silent fallbacks for a key
// that is present but has the wrong type). A missing key keeps the default.
template <typename F>
Result<void> readField(const Json& section, const char* sectionName,
                       const char* key, const char* type, F&& apply) {
    const Json* v = section.find(key);
    if (v == nullptr) {
        return {};
    }
    const std::string typeStr(type);
    const bool typeOk = typeStr == "string"    ? v->isString()
        : typeStr == "number"                  ? v->isNumber()
        : typeStr == "bool"                    ? v->isBool()
        : typeStr == "array"                   ? v->isArray()
                                               : false;
    if (!typeOk) {
        return MC_FAIL(std::string("'") + sectionName + "." + key + "' must be a " +
                       type);
    }
    apply(*v);
    return {};
}

// Warn about keys in a section that we do not recognize (typos, future
// fields). Forward compatibility, mirroring the block registry behavior.
void warnUnknown(const Json& section, const char* sectionName,
                 const std::vector<std::string>& known) {
    for (const auto& [key, value] : section.members()) {
        bool found = false;
        for (const std::string& k : known) {
            if (k == key) {
                found = true;
                break;
            }
        }
        if (!found) {
            MC_LOG_WARN(std::string("Worldgen section '") + sectionName +
                        "': unknown field '" + key + "' ignored");
        }
    }
}

} // namespace

Result<WorldgenParams> WorldgenParams::parse(const Json& def) {
    if (!def.isObject()) {
        return MC_FAIL("worldgen config must be a JSON object");
    }

    WorldgenParams p;

    // -- terrain ---------------------------------------------------------
    if (const Json* t = def.find("terrain")) {
        if (!t->isObject()) {
            return MC_FAIL("'terrain' must be an object");
        }
        warnUnknown(*t, "terrain",
                    {"base_height", "height_amplitude", "sea_level",
                     "height_noise_scale", "height_octaves", "bottom_block",
                     "deep_block", "water_block"});
        auto r = readField(*t, "terrain", "base_height", "number",
                           [&](const Json& v) { p.baseHeight = v.asInt(); });
        MC_TRY(r);
        r = readField(*t, "terrain", "height_amplitude", "number",
                      [&](const Json& v) { p.heightAmplitude = v.asInt(); });
        MC_TRY(r);
        r = readField(*t, "terrain", "sea_level", "number",
                      [&](const Json& v) { p.seaLevel = v.asInt(); });
        MC_TRY(r);
        r = readField(*t, "terrain", "height_noise_scale", "number",
                      [&](const Json& v) { p.heightNoiseScale = v.asDouble(); });
        MC_TRY(r);
        r = readField(*t, "terrain", "height_octaves", "number",
                      [&](const Json& v) { p.heightOctaves = v.asInt(); });
        MC_TRY(r);
        r = readField(*t, "terrain", "bottom_block", "string",
                      [&](const Json& v) { p.bottomBlock = v.asString(); });
        MC_TRY(r);
        r = readField(*t, "terrain", "deep_block", "string",
                      [&](const Json& v) { p.deepBlock = v.asString(); });
        MC_TRY(r);
        r = readField(*t, "terrain", "water_block", "string",
                      [&](const Json& v) { p.waterBlock = v.asString(); });
        MC_TRY(r);
    }

    // -- biomes ----------------------------------------------------------
    if (const Json* b = def.find("biomes")) {
        if (!b->isObject()) {
            return MC_FAIL("'biomes' must be an object");
        }
        warnUnknown(*b, "biomes", {"noise_scale"});
        auto r = readField(*b, "biomes", "noise_scale", "number",
                           [&](const Json& v) { p.biomeNoiseScale = v.asDouble(); });
        MC_TRY(r);
    }

    // -- caves -----------------------------------------------------------
    if (const Json* c = def.find("caves")) {
        if (!c->isObject()) {
            return MC_FAIL("'caves' must be an object");
        }
        warnUnknown(*c, "caves",
                    {"enabled", "noise_scale", "threshold", "min_y",
                     "surface_guard"});
        auto r = readField(*c, "caves", "enabled", "bool",
                           [&](const Json& v) { p.cavesEnabled = v.asBool(); });
        MC_TRY(r);
        r = readField(*c, "caves", "noise_scale", "number",
                      [&](const Json& v) { p.caveNoiseScale = v.asDouble(); });
        MC_TRY(r);
        r = readField(*c, "caves", "threshold", "number",
                      [&](const Json& v) { p.caveThreshold = v.asDouble(); });
        MC_TRY(r);
        r = readField(*c, "caves", "min_y", "number",
                      [&](const Json& v) { p.caveMinY = v.asInt(); });
        MC_TRY(r);
        r = readField(*c, "caves", "surface_guard", "number",
                      [&](const Json& v) { p.surfaceGuard = v.asInt(); });
        MC_TRY(r);
    }

    // -- ores ------------------------------------------------------------
    if (const Json* o = def.find("ores")) {
        if (!o->isObject()) {
            return MC_FAIL("'ores' must be an object");
        }
        warnUnknown(*o, "ores", {"lattice", "veins"});
        auto r = readField(*o, "ores", "lattice", "number",
                           [&](const Json& v) { p.oreLattice = v.asInt(); });
        MC_TRY(r);
        if (const Json* veins = o->find("veins")) {
            if (!veins->isArray()) {
                return MC_FAIL("'ores.veins' must be an array");
            }
            for (std::size_t i = 0; i < veins->size(); ++i) {
                const Json& ve = veins->at(i);
                if (!ve.isObject()) {
                    return MC_FAIL("ore vein #" + std::to_string(i) +
                                   " must be an object");
                }
                OreConfig ore;
                const std::string label = "ores.veins[" + std::to_string(i) + "]";
                r = readField(ve, label.c_str(), "block", "string",
                              [&](const Json& v) { ore.block = v.asString(); });
                MC_TRY(r);
                r = readField(ve, label.c_str(), "probability", "number",
                              [&](const Json& v) { ore.probability = v.asDouble(); });
                MC_TRY(r);
                r = readField(ve, label.c_str(), "vein_size", "number",
                              [&](const Json& v) { ore.veinSize = v.asInt(); });
                MC_TRY(r);
                r = readField(ve, label.c_str(), "max_y", "number",
                              [&](const Json& v) { ore.maxY = v.asInt(); });
                MC_TRY(r);
                if (ore.block.empty()) {
                    return MC_FAIL(label + ": 'block' must be a non-empty string");
                }
                if (ore.probability < 0.0f || ore.probability > 1.0f) {
                    return MC_FAIL(label + ": 'probability' must be in [0, 1]");
                }
                if (ore.veinSize < 1 || ore.veinSize > 32) {
                    return MC_FAIL(label + ": 'vein_size' must be in [1, 32]");
                }
                if (ore.maxY < 1 || ore.maxY > 127) {
                    return MC_FAIL(label + ": 'max_y' must be in [1, 127]");
                }
                p.ores.push_back(std::move(ore));
            }
        }
    }

    // -- trees -----------------------------------------------------------
    if (const Json* tr = def.find("trees")) {
        if (!tr->isObject()) {
            return MC_FAIL("'trees' must be an object");
        }
        warnUnknown(*tr, "trees",
                    {"lattice", "trunk", "leaves", "min_height",
                     "height_range"});
        auto r = readField(*tr, "trees", "lattice", "number",
                           [&](const Json& v) { p.treeLattice = v.asInt(); });
        MC_TRY(r);
        r = readField(*tr, "trees", "trunk", "string",
                      [&](const Json& v) { p.treeTrunk = v.asString(); });
        MC_TRY(r);
        r = readField(*tr, "trees", "leaves", "string",
                      [&](const Json& v) { p.treeLeaves = v.asString(); });
        MC_TRY(r);
        r = readField(*tr, "trees", "min_height", "number",
                      [&](const Json& v) { p.treeMinHeight = v.asInt(); });
        MC_TRY(r);
        r = readField(*tr, "trees", "height_range", "number",
                      [&](const Json& v) { p.treeHeightRange = v.asInt(); });
        MC_TRY(r);
    }

    // -- cross-field validation -----------------------------------------
    if (p.baseHeight < 1 || p.baseHeight > 120) {
        return MC_FAIL("'terrain.base_height' must be in [1, 120]");
    }
    if (p.heightAmplitude < 0 || p.heightAmplitude > 60) {
        return MC_FAIL("'terrain.height_amplitude' must be in [0, 60]");
    }
    if (p.seaLevel < 1 || p.seaLevel > 120) {
        return MC_FAIL("'terrain.sea_level' must be in [1, 120]");
    }
    if (p.heightNoiseScale <= 0.0f) {
        return MC_FAIL("'terrain.height_noise_scale' must be > 0");
    }
    if (p.heightOctaves < 1 || p.heightOctaves > 8) {
        return MC_FAIL("'terrain.height_octaves' must be in [1, 8]");
    }
    if (p.bottomBlock.empty() || p.deepBlock.empty() ||
        p.waterBlock.empty()) {
        return MC_FAIL("terrain block names (bottom/deep/water) must be "
                       "non-empty");
    }
    if (p.biomeNoiseScale <= 0.0f) {
        return MC_FAIL("'biomes.noise_scale' must be > 0");
    }
    if (p.caveNoiseScale <= 0.0f) {
        return MC_FAIL("'caves.noise_scale' must be > 0");
    }
    if (p.caveThreshold < 0.0f || p.caveThreshold >= 1.0f) {
        return MC_FAIL("'caves.threshold' must be in [0, 1)");
    }
    if (p.caveMinY < 0 || p.caveMinY > 120) {
        return MC_FAIL("'caves.min_y' must be in [0, 120]");
    }
    if (p.surfaceGuard < 0 || p.surfaceGuard > 15) {
        return MC_FAIL("'caves.surface_guard' must be in [0, 15]");
    }
    if (p.oreLattice < 1 || p.oreLattice > 32) {
        return MC_FAIL("'ores.lattice' must be in [1, 32]");
    }
    if (p.treeLattice < 1 || p.treeLattice > 16) {
        return MC_FAIL("'trees.lattice' must be in [1, 16]");
    }
    if (p.treeTrunk.empty() || p.treeLeaves.empty()) {
        return MC_FAIL("'trees.trunk' and 'trees.leaves' must be non-empty");
    }
    if (p.treeMinHeight < 1 || p.treeMinHeight > 16) {
        return MC_FAIL("'trees.min_height' must be in [1, 16]");
    }
    if (p.treeHeightRange < 0 || p.treeHeightRange > 16) {
        return MC_FAIL("'trees.height_range' must be in [0, 16]");
    }

    return p;
}

Result<WorldgenParams> WorldgenParams::load(AssetManager& assets,
                                            const std::string& relativePath) {
    auto text = assets.loadText(relativePath);
    MC_TRY(text);
    auto parsed = Json::parse(*text);
    if (!parsed.ok()) {
        return MC_FAIL("invalid JSON in worldgen config '" + relativePath +
                       "': " + parsed.error().message);
    }
    return parse(*parsed);
}

} // namespace mc
