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

namespace {

bool isPowerOfTwo(int n) { return n > 0 && (n & (n - 1)) == 0; }

} // namespace

Result<AtlasLayout> AtlasLayout::parse(const Json& def, int tileSize) {
    if (tileSize <= 0) {
        return MC_FAIL("invalid tile size " + std::to_string(tileSize));
    }
    if (!def.isObject()) {
        return MC_FAIL("atlas layout must be a JSON object");
    }
    for (const auto& [key, value] : def.members()) {
        if (key != "columns" && key != "rows" && key != "tiles") {
            MC_LOG_WARN("atlas layout: unknown field '" + key + "' ignored");
        }
    }

    const Json* colsJson = def.find("columns");
    const Json* rowsJson = def.find("rows");
    if (colsJson == nullptr || !colsJson->isNumber() ||
        rowsJson == nullptr || !rowsJson->isNumber()) {
        return MC_FAIL("atlas layout requires integer 'columns' and 'rows'");
    }
    const double cd = colsJson->asDouble();
    const double rd = rowsJson->asDouble();
    if (cd != std::floor(cd) || rd != std::floor(rd) || !isPowerOfTwo(static_cast<int>(cd)) ||
        !isPowerOfTwo(static_cast<int>(rd))) {
        return MC_FAIL("atlas layout 'columns' and 'rows' must be powers of two "
                       "(exact UV fractions, pixel-stable sampling); got " +
                       std::to_string(static_cast<int>(cd)) + "x" +
                       std::to_string(static_cast<int>(rd)));
    }
    const int cols = static_cast<int>(cd);
    const int rows = static_cast<int>(rd);

    const Json* tilesJson = def.find("tiles");
    if (tilesJson == nullptr || !tilesJson->isObject() ||
        tilesJson->members().empty()) {
        return MC_FAIL("atlas layout requires a non-empty 'tiles' object "
                       "mapping tile names to [col, row] cells");
    }

    AtlasLayout layout;
    layout.tileWidth = tileSize;
    layout.tileHeight = tileSize;
    layout.columns = cols;
    layout.rows = rows;
    layout.atlasWidth = cols * tileSize;
    layout.atlasHeight = rows * tileSize;
    std::unordered_map<int, std::string> cellOwner; // row*cols+col -> name
    for (const auto& [name, cell] : tilesJson->members()) {
        if (name.empty()) {
            return MC_FAIL("atlas layout has a tile with an empty name");
        }
        if (!cell.isArray() || cell.size() != 2 || !cell.at(0).isNumber() ||
            !cell.at(1).isNumber()) {
            return MC_FAIL("atlas layout tile '" + name +
                           "' must be a [col, row] array of two integers");
        }
        const double cx = cell.at(0).asDouble();
        const double cy = cell.at(1).asDouble();
        const int c = static_cast<int>(cx);
        const int r = static_cast<int>(cy);
        if (cx != std::floor(cx) || cy != std::floor(cy) || c < 0 ||
            c >= cols || r < 0 || r >= rows) {
            return MC_FAIL("atlas layout tile '" + name + "' cell [" +
                           std::to_string(c) + ", " + std::to_string(r) +
                           "] lies outside the " + std::to_string(cols) + "x" +
                           std::to_string(rows) + " grid");
        }
        const int idx = r * cols + c;
        if (cellOwner.count(idx) != 0) {
            return MC_FAIL("atlas layout: tiles '" + cellOwner[idx] + "' and '" +
                           name + "' share cell [" + std::to_string(c) + ", " +
                           std::to_string(r) + "]");
        }
        cellOwner[idx] = name;
        layout.tiles[name] = Tile {c, r};
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

Result<void> TextureAtlas::upload(const Json& layoutDef) {
    auto layout = AtlasLayout::parse(layoutDef, kTileSize);
    MC_TRY(layout);

    // Cross-check the layout against the loaded tiles in both directions:
    // every PNG needs a cell, and every named cell needs a PNG.
    for (const auto& [name, tile] : m_tiles) {
        (void)tile;
        if (!layout->contains(name)) {
            return MC_FAIL("texture tile '" + name +
                           "' has no entry in the atlas layout");
        }
    }
    for (const auto& [name, pos] : layout->tiles) {
        (void)pos;
        if (m_tiles.count(name) == 0) {
            return MC_FAIL("atlas layout entry '" + name +
                           "' has no texture tile (no '" + name +
                           ".png' in the texture directory)");
        }
    }

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
