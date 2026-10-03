// Central asset access for all game resources (textures, shaders, sounds,
// JSON data). Callers always refer to assets by a path relative to the asset
// root ("textures/blocks/stone.png"), never by an absolute filesystem path.
//
// Phase 0 provides: root management, safe path resolution (no traversal
// outside the root), text loading with caching. Binary loading (textures,
// audio) is added in Phase 1.
#pragma once

#include <cstddef>
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>

#include "core/error.hpp"

namespace mc {

class AssetManager {
public:
    // The root directory must exist and be a directory.
    Result<void> setRoot(const std::filesystem::path& root);
    const std::filesystem::path& root() const { return m_root; }

    // Map a relative asset path to an absolute one, rejecting anything that
    // would escape the asset root (absolute paths, ".." traversal).
    Result<std::filesystem::path> resolve(const std::string& relative) const;

    bool exists(const std::string& relative) const;

    // Load a text asset (shader, JSON, ...). Results are cached.
    Result<std::string> loadText(const std::string& relative);

    std::size_t textCacheSize() const;
    void clearCache();

private:
    std::filesystem::path m_root;
    mutable std::mutex m_mutex; // guards m_textCache
    std::unordered_map<std::string, std::string> m_textCache;
};

} // namespace mc
