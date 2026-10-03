#include "world/biome.hpp"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "core/logger.hpp"

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
            key != "tree_density" && key != "height_scale") {
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
