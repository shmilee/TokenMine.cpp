// JSON-backed game configuration.
//
// Values are addressed with dotted keys ("window.width", "world.seed").
// Missing keys fall back to caller-supplied defaults; unknown keys are
// tolerated (forward compatibility) but logged at load time.
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "core/error.hpp"
#include "core/json.hpp"

namespace mc {

class Config {
public:
    // Load (and parse) the config file. Fails with a descriptive error when
    // the file cannot be read or contains invalid JSON.
    static Result<Config> load(const std::filesystem::path& path);

    // Serialize back to disk (pretty-printed, 2-space indent).
    Result<void> save(const std::filesystem::path& path) const;

    // Typed getters with fallback defaults.
    std::string getString(const std::string& key, const std::string& def) const;
    int getInt(const std::string& key, int def) const;
    std::int64_t getInt64(const std::string& key, std::int64_t def) const;
    double getDouble(const std::string& key, double def) const;
    bool getBool(const std::string& key, bool def) const;

    // Override a value (dotted key). Creates intermediate objects.
    void set(const std::string& key, Json value);

    const Json& raw() const { return m_data; }
    const std::filesystem::path& path() const { return m_path; }

    // The canonical default configuration, used to seed config/config.json.
    static std::string defaultConfigJson();

    // Public default construction (empty config; all getters fall back to
    // their defaults).
    Config() = default;

    // Look up a dotted key in the raw JSON (nullptr when absent).
    const Json* at(const std::string& dottedKey) const { return find(dottedKey); }

private:
    const Json* find(const std::string& dottedKey) const;
    Json& findOrCreate(const std::string& dottedKey);

    Json m_data = Json::object();
    std::filesystem::path m_path;
};

} // namespace mc
