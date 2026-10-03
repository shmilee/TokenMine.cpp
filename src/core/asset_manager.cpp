#include "core/asset_manager.hpp"

#include <fstream>
#include <sstream>

#include "core/logger.hpp"

namespace mc {

namespace {

// path::starts_with is C++26; compare whole components manually (C++20).
bool pathStartsWith(const std::filesystem::path& p,
                    const std::filesystem::path& prefix) {
    auto it = p.begin();
    for (const auto& part : prefix) {
        if (it == p.end() || *it != part) {
            return false;
        }
        ++it;
    }
    return true;
}

} // namespace

Result<void> AssetManager::setRoot(const std::filesystem::path& root) {
    std::error_code ec;
    if (!std::filesystem::exists(root, ec) || ec) {
        return MC_FAIL("asset root does not exist: '" + root.string() + "'");
    }
    if (!std::filesystem::is_directory(root)) {
        return MC_FAIL("asset root is not a directory: '" + root.string() + "'");
    }
    m_root = std::filesystem::absolute(root).lexically_normal();
    MC_LOG_INFO("Asset root: " + m_root.string());
    return {};
}

Result<std::filesystem::path> AssetManager::resolve(const std::string& relative) const {
    if (relative.empty()) {
        return MC_FAIL("empty asset path");
    }
    if (relative[0] == '/' || relative[0] == '\\') {
        return MC_FAIL("asset paths must be relative, got: '" + relative + "'");
    }
    const std::filesystem::path candidate =
        (m_root / relative).lexically_normal();
    // Reject anything outside the root ("../" traversal). Component-wise
    // comparison, so a sibling directory like "<root>2/..." is not treated
    // as being inside the root.
    if (candidate != m_root && !pathStartsWith(candidate, m_root)) {
        return MC_FAIL("asset path escapes asset root: '" + relative + "'");
    }
    return candidate;
}

bool AssetManager::exists(const std::string& relative) const {
    auto p = resolve(relative);
    if (!p.ok()) {
        return false;
    }
    std::error_code ec;
    return std::filesystem::exists(*p, ec) && !ec;
}

Result<std::string> AssetManager::loadText(const std::string& relative) {
    auto p = resolve(relative);
    MC_TRY(p);

    {
        std::lock_guard lock(m_mutex);
        const auto it = m_textCache.find(relative);
        if (it != m_textCache.end()) {
            return it->second;
        }
    }

    std::ifstream in(*p, std::ios::binary);
    if (!in) {
        return MC_FAIL("cannot read asset file: '" + relative + "'");
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    const std::string text = ss.str();

    {
        std::lock_guard lock(m_mutex);
        m_textCache.emplace(relative, text);
    }
    MC_LOG_DEBUG("Loaded asset '" + relative + "' (" + std::to_string(text.size()) +
                 " bytes)");
    return text;
}

std::size_t AssetManager::textCacheSize() const {
    std::lock_guard lock(m_mutex);
    return m_textCache.size();
}

void AssetManager::clearCache() {
    std::lock_guard lock(m_mutex);
    m_textCache.clear();
}

} // namespace mc
