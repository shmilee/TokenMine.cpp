#include "core/config.hpp"

#include <fstream>
#include <sstream>
#include <vector>

#include "core/logger.hpp"

namespace mc {

namespace {

std::vector<std::string> splitDotted(const std::string& key) {
    std::vector<std::string> parts;
    std::string current;
    for (const char c : key) {
        if (c == '.') {
            if (!current.empty()) {
                parts.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) {
        parts.push_back(current);
    }
    return parts;
}

} // namespace

Result<Config> Config::load(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) {
        return MC_FAIL("cannot open config file '" + path.string() + "'");
    }
    std::ostringstream ss;
    ss << in.rdbuf();

    auto parsed = Json::parse(ss.str());
    if (!parsed.ok()) {
        return MC_FAIL("invalid JSON in config file '" + path.string() + "': " +
                       parsed.error().message);
    }
    if (!parsed->isObject()) {
        return MC_FAIL("config root must be a JSON object (file '" + path.string() + "')");
    }

    Config config;
    config.m_data = std::move(*parsed);
    config.m_path = path;
    return config;
}

Result<void> Config::save(const std::filesystem::path& path) const {
    std::ofstream out(path);
    if (!out) {
        return MC_FAIL("cannot open config file for writing: '" + path.string() + "'");
    }
    out << m_data.dump(2) << '\n';
    if (!out) {
        return MC_FAIL("write to config file failed: '" + path.string() + "'");
    }
    return {};
}

const Json* Config::find(const std::string& dottedKey) const {
    const Json* cur = &m_data;
    for (const auto& part : splitDotted(dottedKey)) {
        cur = cur->find(part);
        if (cur == nullptr) {
            return nullptr;
        }
    }
    return cur;
}

Json& Config::findOrCreate(const std::string& dottedKey) {
    Json* cur = &m_data;
    const auto parts = splitDotted(dottedKey);
    for (std::size_t i = 0; i + 1 < parts.size(); ++i) {
        Json* next = cur->find(parts[i]);
        if (next == nullptr) {
            (*cur)[parts[i]] = Json::object();
            next = cur->find(parts[i]);
        }
        if (!next->isObject()) {
            // A path component is not an object; overwrite it.
            (*cur)[parts[i]] = Json::object();
            next = cur->find(parts[i]);
        }
        cur = next;
    }
    return (*cur)[parts.back()];
}

std::string Config::getString(const std::string& key, const std::string& def) const {
    const Json* j = find(key);
    return (j && j->isString()) ? j->asString() : def;
}

int Config::getInt(const std::string& key, int def) const {
    const Json* j = find(key);
    return (j && j->isNumber()) ? j->asInt(def) : def;
}

std::int64_t Config::getInt64(const std::string& key, std::int64_t def) const {
    const Json* j = find(key);
    return (j && j->isNumber()) ? j->asInt64(def) : def;
}

double Config::getDouble(const std::string& key, double def) const {
    const Json* j = find(key);
    return (j && j->isNumber()) ? j->asDouble(def) : def;
}

bool Config::getBool(const std::string& key, bool def) const {
    const Json* j = find(key);
    return (j && j->isBool()) ? j->asBool() : def;
}

void Config::set(const std::string& key, Json value) {
    findOrCreate(key) = std::move(value);
}

std::string Config::defaultConfigJson() {
    const Json cfg = Json::parse(R"({
    "window": {
        "title": "MineCraft",
        "width": 1280,
        "height": 720,
        "fullscreen": false
    },
    "render": {
        "vsync": true,
        "fps_cap": 0
    },
    "world": {
        "seed": 123456789,
        "view_distance": 8
    },
    "audio": {
        "volume": 0.7
    }
})")
                              .value();
    return cfg.dump(2);
}

} // namespace mc
