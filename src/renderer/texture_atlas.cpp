#include "renderer/texture_atlas.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>

#include "core/asset_manager.hpp"
#include "core/logger.hpp"

namespace mc {

// ---------------------------------------------------------------------------
// Layout (pure)
// ---------------------------------------------------------------------------

Result<AtlasLayout> AtlasLayout::compute(const std::vector<std::string>& names,
                                         int tileSize) {
    if (names.empty()) {
        return MC_FAIL("atlas layout with zero tiles");
    }
    if (tileSize <= 0) {
        return MC_FAIL("invalid tile size " + std::to_string(tileSize));
    }

    std::vector<std::string> sorted = names;
    std::sort(sorted.begin(), sorted.end());
    for (std::size_t i = 1; i < sorted.size(); ++i) {
        if (sorted[i] == sorted[i - 1]) {
            return MC_FAIL("duplicate atlas tile name: '" + sorted[i] + "'");
        }
    }

    const std::size_t n = sorted.size();
    const int cols = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(n))));
    const int rows = static_cast<int>((n + static_cast<std::size_t>(cols) - 1) /
                                      static_cast<std::size_t>(cols));

    AtlasLayout layout;
    layout.tileWidth = tileSize;
    layout.tileHeight = tileSize;
    layout.columns = cols;
    layout.rows = rows;
    layout.atlasWidth = cols * tileSize;
    layout.atlasHeight = rows * tileSize;
    for (std::size_t i = 0; i < n; ++i) {
        layout.tiles[sorted[i]] = Tile{static_cast<int>(i % static_cast<std::size_t>(cols)),
                                       static_cast<int>(i / static_cast<std::size_t>(cols))};
    }
    return layout;
}

// GL convention (glTexImage2D, no UNPACK_FLIP_Y): the first pixel row of the
// upload array is the BOTTOM of the texture (v = 0). The packer writes tile
// row 0 first, so row 0 sits at v = 0 and row r spans
// [r/rows, (r+1)/rows]. Within a tile, the PNG's top row maps to the tile's
// v_min — which the cube builder assigns to the face's top edge, so block
// art is not vertically flipped.
Vec2 AtlasLayout::uvMin(const AtlasLayout& layout, const std::string& name) {
    const Tile t = layout.tiles.at(name);
    const float u0 = static_cast<float>(t.col) / layout.columns;
    const float v0 = static_cast<float>(t.row) / layout.rows;
    return {u0, v0};
}

Vec2 AtlasLayout::uvMax(const AtlasLayout& layout, const std::string& name) {
    const Tile t = layout.tiles.at(name);
    const float u1 = static_cast<float>(t.col + 1) / layout.columns;
    const float v1 = static_cast<float>(t.row + 1) / layout.rows;
    return {u1, v1};
}

// ---------------------------------------------------------------------------
// Atlas (GPU)
// ---------------------------------------------------------------------------

Result<void> TextureAtlas::addTile(const std::string& name,
                                   const PngImage& image) {
    if (name.empty()) {
        return MC_FAIL("atlas tile with empty name");
    }
    if (image.width != kTileSize || image.height != kTileSize) {
        return MC_FAIL("atlas tile '" + name + "' must be " +
                       std::to_string(kTileSize) + "x" +
                       std::to_string(kTileSize) + ", got " +
                       std::to_string(image.width) + "x" +
                       std::to_string(image.height));
    }
    if (m_tiles.count(name) != 0) {
        return MC_FAIL("duplicate atlas tile name: '" + name + "'");
    }
    m_tiles.emplace(name, image);
    return {};
}

Result<void> TextureAtlas::loadDirectory(const AssetManager& assets,
                                         const std::string& relativeDir) {
    auto dir = assets.resolve(relativeDir);
    MC_TRY(dir);
    if (!std::filesystem::is_directory(*dir)) {
        return MC_FAIL("texture directory not found: '" + (*dir).string() + "'");
    }

    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(*dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".png") {
            files.push_back(entry.path());
        }
    }
    if (files.empty()) {
        return MC_FAIL("no PNG files in texture directory: '" +
                       (*dir).string() + "'");
    }
    std::sort(files.begin(), files.end());

    for (const auto& file : files) {
        const auto rel =
            std::filesystem::relative(file, assets.root()).generic_string();
        std::string name = file.stem().string();

        std::ifstream in(file, std::ios::binary);
        if (!in) {
            return MC_FAIL("cannot open texture file: '" + rel + "'");
        }
        const std::string bytes(
            (std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        auto image = Png::decode(bytes);
        MC_TRY(image);

        auto added = addTile(name, *image);
        MC_TRY(added);
    }
    MC_LOG_INFO("Texture atlas: loaded " + std::to_string(m_tiles.size()) +
                " tiles from '" + relativeDir + "'");
    return {};
}

Result<void> TextureAtlas::upload() {
    std::vector<std::string> names;
    names.reserve(m_tiles.size());
    for (const auto& [name, tile] : m_tiles) {
        (void)tile;
        names.push_back(name);
    }

    auto layout = AtlasLayout::compute(names, kTileSize);
    MC_TRY(layout);
    m_layout = std::move(*layout);

    // Pack pixels (top-to-bottom rows, matching the UV mapping).
    std::vector<std::uint8_t> pixels(
        static_cast<std::size_t>(m_layout.atlasWidth) *
            m_layout.atlasHeight * 4,
        0);
    for (const auto& [name, tile] : m_tiles) {
        const AtlasLayout::Tile pos = m_layout.tiles.at(name);
        const int dstX = pos.col * kTileSize;
        const int dstY = pos.row * kTileSize;
        for (int y = 0; y < kTileSize; ++y) {
            std::copy_n(
                tile.rgba.data() + static_cast<std::size_t>(y) * kTileSize * 4,
                static_cast<std::size_t>(kTileSize) * 4,
                pixels.data() +
                    (static_cast<std::size_t>(dstY + y) *
                         static_cast<std::size_t>(m_layout.atlasWidth) +
                     static_cast<std::size_t>(dstX)) *
                        4);
        }
    }

    auto texture = Texture::createFromPixels(m_layout.atlasWidth,
                                             m_layout.atlasHeight,
                                             pixels.data());
    MC_TRY(texture);
    m_texture = std::move(*texture);
    m_ready = true;
    MC_LOG_INFO("Texture atlas uploaded: " +
                std::to_string(m_layout.atlasWidth) + "x" +
                std::to_string(m_layout.atlasHeight) + " (" +
                std::to_string(m_tiles.size()) + " tiles)");
    return {};
}

} // namespace mc
