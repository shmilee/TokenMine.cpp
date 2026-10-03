// Block texture atlas: packs 16x16 block tiles into one GPU texture
// (Minecraft-style). One texture = one bind per frame instead of one per
// block.
//
// The layout computation is a pure function (unit-testable without a GL
// context); the upload is a thin GPU step.
//
// UV convention: v=0 at the bottom of the texture (GL, no UNPACK_FLIP_Y),
// so packed tile row 0 (first in the pixel array) sits at v=0 and row r
// spans [r/rows, (r+1)/rows].
#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/error.hpp"
#include "core/math/vec2.hpp"
#include "core/png.hpp"
#include "renderer/texture.hpp"

namespace mc {

class AssetManager;

struct AtlasLayout {
    int tileWidth = 0;
    int tileHeight = 0;
    int columns = 0;
    int rows = 0;
    int atlasWidth = 0;  // tileWidth * columns
    int atlasHeight = 0; // tileHeight * rows

    struct Tile {
        int col = 0;
        int row = 0;

        bool operator==(const Tile& o) const {
            return col == o.col && row == o.row;
        }
    };
    std::unordered_map<std::string, Tile> tiles; // alphabetical insertion

    // Deterministic layout: tiles sorted by name, smallest square grid that
    // fits (rows = ceil(n / cols), cols = ceil(sqrt(n))).
    static Result<AtlasLayout> compute(const std::vector<std::string>& names,
                                       int tileSize = 16);

    bool contains(const std::string& name) const {
        return tiles.count(name) != 0;
    }

    // UV corners of a tile in [0,1] (v flipped for GL).
    static Vec2 uvMin(const AtlasLayout& layout, const std::string& name);
    static Vec2 uvMax(const AtlasLayout& layout, const std::string& name);
};

class TextureAtlas {
public:
    static constexpr int kTileSize = 16;

    TextureAtlas() = default;
    TextureAtlas(TextureAtlas&&) noexcept = default;
    TextureAtlas& operator=(TextureAtlas&&) noexcept = default;
    TextureAtlas(const TextureAtlas&) = delete;
    TextureAtlas& operator=(const TextureAtlas&) = delete;

    // Add one tile (must be exactly tileSize x tileSize).
    Result<void> addTile(const std::string& name, const PngImage& image);

    // Load every *.png in `relativeDir` (asset-root relative) as a tile.
    Result<void> loadDirectory(const AssetManager& assets,
                               const std::string& relativeDir);

    // Compute the layout and upload a single GL texture.
    Result<void> upload();

    bool ready() const { return m_ready; }
    const Texture& texture() const { return m_texture; }
    const AtlasLayout& layout() const { return m_layout; }
    bool contains(const std::string& name) const {
        return m_ready && m_layout.contains(name);
    }
    std::size_t tileCount() const { return m_tiles.size(); }

private:
    std::unordered_map<std::string, PngImage> m_tiles;
    AtlasLayout m_layout;
    Texture m_texture;
    bool m_ready = false;
};

} // namespace mc
