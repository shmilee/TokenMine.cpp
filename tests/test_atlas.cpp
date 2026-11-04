// Block texture atlas layout tests: the data-driven layout parse (strict
// validation: power-of-two grid, in-range cells, no shared cells) and the
// UV corner math. The layout is data, not computed — that is what keeps
// adding a texture from moving existing tiles' UVs.
#include <gtest/gtest.h>

#include <string>

#include "core/asset_manager.hpp"
#include "core/json.hpp"
#include "renderer/texture_atlas.hpp"

using namespace mc;

namespace {

Json parseJson(const std::string& text) {
    auto r = Json::parse(text);
    if (!r.ok()) {
        ADD_FAILURE() << "parse failed: " << r.error().message;
        return Json::object();
    }
    return *r;
}

AtlasLayout parseLayout(const std::string& text) {
    auto r = AtlasLayout::parse(parseJson(text));
    if (!r.ok()) {
        ADD_FAILURE() << r.error().message;
        return AtlasLayout {};
    }
    return *r;
}

} // namespace

TEST(AtlasLayoutTest, ParsesExplicitCells) {
    const AtlasLayout l = parseLayout(
        R"({"columns":2,"rows":2,"tiles":{
             "a":[0,0],"b":[1,0],"c":[0,1],"d":[1,1]}})");
    EXPECT_EQ(l.columns, 2);
    EXPECT_EQ(l.rows, 2);
    EXPECT_EQ(l.atlasWidth, 32);
    EXPECT_EQ(l.atlasHeight, 32);
    EXPECT_EQ(l.tiles.at("a"), (AtlasLayout::Tile {0, 0}));
    EXPECT_EQ(l.tiles.at("d"), (AtlasLayout::Tile {1, 1}));
    EXPECT_TRUE(l.contains("c"));
    EXPECT_FALSE(l.contains("zzz"));
}

TEST(AtlasLayoutTest, SingleTileOn1x1) {
    const AtlasLayout l = parseLayout(
        R"({"columns":1,"rows":1,"tiles":{"a":[0,0]}})");
    EXPECT_EQ(l.atlasWidth, 16);
    EXPECT_EQ(l.atlasHeight, 16);
}

TEST(AtlasLayoutTest, RejectsNonPowerOfTwoGrid) {
    const std::string tiles = R"("tiles":{"a":[0,0]})";
    EXPECT_FALSE(AtlasLayout::parse(
        parseJson(R"({"columns":3,"rows":2,)" + tiles + "}")).ok());
    EXPECT_FALSE(AtlasLayout::parse(
        parseJson(R"({"columns":4,"rows":5,)" + tiles + "}")).ok());
    EXPECT_FALSE(AtlasLayout::parse(
        parseJson(R"({"columns":0,"rows":4,)" + tiles + "}")).ok());
    EXPECT_FALSE(AtlasLayout::parse(
        parseJson(R"({"columns":-4,"rows":4,)" + tiles + "}")).ok());
}

TEST(AtlasLayoutTest, RejectsMissingOrEmptyFields) {
    EXPECT_FALSE(AtlasLayout::parse(
        parseJson(R"({"rows":2,"tiles":{"a":[0,0]}})")).ok());
    EXPECT_FALSE(AtlasLayout::parse(
        parseJson(R"({"columns":2,"tiles":{"a":[0,0]}})")).ok());
    EXPECT_FALSE(AtlasLayout::parse(
        parseJson(R"({"columns":2,"rows":2,"tiles":{}})")).ok());
    EXPECT_FALSE(AtlasLayout::parse(
        parseJson(R"({"columns":2,"rows":2})")).ok());
    EXPECT_FALSE(AtlasLayout::parse(parseJson(R"([1,2,3])")).ok());
}

TEST(AtlasLayoutTest, RejectsBadCellValues) {
    EXPECT_FALSE(AtlasLayout::parse(parseJson(
        R"({"columns":2,"rows":2,"tiles":{"a":[0]}})")).ok());
    EXPECT_FALSE(AtlasLayout::parse(parseJson(
        R"({"columns":2,"rows":2,"tiles":{"a":[0,0,0]}})")).ok());
    EXPECT_FALSE(AtlasLayout::parse(parseJson(
        R"({"columns":2,"rows":2,"tiles":{"a":"top-left"}})")).ok());
    EXPECT_FALSE(AtlasLayout::parse(parseJson(
        R"({"columns":2,"rows":2,"tiles":{"a":[0.5,0]}})")).ok());
    EXPECT_FALSE(AtlasLayout::parse(parseJson(
        R"({"columns":2,"rows":2,"tiles":{"a":[-1,0]}})")).ok());
}

TEST(AtlasLayoutTest, RejectsOutOfRangeCell) {
    EXPECT_FALSE(AtlasLayout::parse(parseJson(
        R"({"columns":2,"rows":2,"tiles":{"a":[2,0]}})")).ok());
    EXPECT_FALSE(AtlasLayout::parse(parseJson(
        R"({"columns":2,"rows":2,"tiles":{"a":[0,2]}})")).ok());
}

TEST(AtlasLayoutTest, RejectsSharedCell) {
    auto r = AtlasLayout::parse(parseJson(
        R"({"columns":2,"rows":2,"tiles":{"a":[0,0],"b":[0,0]}})"));
    ASSERT_FALSE(r.ok());
    // The error names BOTH offenders (a data fix needs both names).
    EXPECT_NE(r.error().message.find("a"), std::string::npos);
    EXPECT_NE(r.error().message.find("b"), std::string::npos);
}

TEST(AtlasLayoutTest, UvCorners2x2) {
    const AtlasLayout l = parseLayout(
        R"({"columns":2,"rows":2,"tiles":{
             "a":[0,0],"b":[1,0],"c":[0,1],"d":[1,1]}})");

    // "a" at (col 0, row 0): packed first, so it sits at the BOTTOM-left of
    // the texture (v spans the lower half).
    const Vec2 amin = AtlasLayout::uvMin(l, "a");
    const Vec2 amax = AtlasLayout::uvMax(l, "a");
    EXPECT_FLOAT_EQ(amin.x, 0.0f);
    EXPECT_FLOAT_EQ(amin.y, 0.0f);
    EXPECT_FLOAT_EQ(amax.x, 0.5f);
    EXPECT_FLOAT_EQ(amax.y, 0.5f);

    // "d" at (col 1, row 1): last row packed, so it sits at the TOP-right.
    const Vec2 dmin = AtlasLayout::uvMin(l, "d");
    const Vec2 dmax = AtlasLayout::uvMax(l, "d");
    EXPECT_FLOAT_EQ(dmin.x, 0.5f);
    EXPECT_FLOAT_EQ(dmin.y, 0.5f);
    EXPECT_FLOAT_EQ(dmax.x, 1.0f);
    EXPECT_FLOAT_EQ(dmax.y, 1.0f);

    // The four tiles cover [0,1]x[0,1] exactly (no overlaps/gaps).
    float covered[2][2] = {{0, 0}, {0, 0}};
    for (const char* n : {"a", "b", "c", "d"}) {
        const Vec2 mn = AtlasLayout::uvMin(l, n);
        const Vec2 mx = AtlasLayout::uvMax(l, n);
        covered[static_cast<int>(mn.x * 2)][static_cast<int>(mn.y * 2)] +=
            (mx.x - mn.x) * (mx.y - mn.y);
    }
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 2; ++x) {
            EXPECT_NEAR(covered[y][x], 0.25f, 1e-6f);
        }
    }
}

TEST(AtlasLayoutTest, UvExactOnPowerOfTwoGrid) {
    // The 4x8 production grid: every UV corner must be an exact binary
    // fraction (the float pipeline then cannot round a sample across a tile
    // boundary — this is what keeps the baseline byte-identical).
    const AtlasLayout l = parseLayout(
        R"({"columns":4,"rows":8,"tiles":{"a":[0,0],"b":[3,7]}})");
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 4; ++c) {
            const float u0 = static_cast<float>(c) / 4.0f;
            const float v0 = static_cast<float>(r) / 8.0f;
            // Exact in float: u0*64 and v0*128 land on the integer tile
            // edges (a non-power-of-two grid would fail this, e.g. 0.6f).
            EXPECT_EQ(static_cast<int>(u0 * 64.0f), c * 16)
                << "col " << c;
            EXPECT_EQ(static_cast<int>(v0 * 128.0f), r * 16)
                << "row " << r;
        }
    }
    const Vec2 amin = AtlasLayout::uvMin(l, "a");
    const Vec2 bmax = AtlasLayout::uvMax(l, "b");
    EXPECT_FLOAT_EQ(amin.x, 0.0f);
    EXPECT_FLOAT_EQ(amin.y, 0.0f);
    EXPECT_FLOAT_EQ(bmax.x, 1.0f);
    EXPECT_FLOAT_EQ(bmax.y, 1.0f);
}

TEST(AtlasLayoutTest, UvUnknownTileThrows) {
    const AtlasLayout l = parseLayout(
        R"({"columns":1,"rows":1,"tiles":{"a":[0,0]}})");
    EXPECT_THROW(AtlasLayout::uvMin(l, "zzz"), std::out_of_range);
    EXPECT_THROW(AtlasLayout::uvMax(l, "zzz"), std::out_of_range);
}

TEST(AtlasLayoutTest, ShippedV25AtlasFullCensus) {
    // Per-entry census of the REAL block_atlas.json on the unchanged 16x8
    // grid (the V2-5 red line, extended per phase): the 99 pre-V2-7
    // entries (the 97 pre-V2-5 — the 90 V2-2 legacy + the 7 player tiles —
    // + pig_body / pig_face) keep their EXACT cells (zero UV shift = no
    // re-anchor of the byte-stable baseline) and the new ones sit in the
    // row-7 free run (V2-5: [8..9, 7]; V2-7: [10..12, 7]).
    // NOTE: the pork / cooked_pork food icons are standalone PNGs (the
    // every-shipped-food-icon convention: textures/items/*.png loaded by
    // the item-icon pass, NOT atlas tiles — an atlas entry would fail
    // the upload's both-directions cross-check).
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    auto text = assets.loadText("data/textures/block_atlas.json");
    ASSERT_TRUE(text.ok()) << text.error().message;
    const AtlasLayout l = parseLayout(*text);
    EXPECT_EQ(l.columns, 16);
    EXPECT_EQ(l.rows, 8);
    // V2-7: 99 -> 102 (the material batch's three tiles — gold_block /
    // diamond_block / furnace_front — join the row-7 free run [10..12, 7];
    // planks reuses its EXISTING tile, zero new texture). V2-12: 102 -> 104
    // (the enchantment batch's two tiles — enchanting_table / bookshelf —
    // join the row-2 free run [9, 2] / [14, 2]; zero existing shift).
    // V2-13: 104 -> 119 (the bed batch's 15 tiles — the dyed family, the
    // wool's vocabulary — join the free cells; zero existing shift).
    // V2-14: 119 -> 120 (the bat's single tile — the first free cell of
    // the (row, col) scan [10, 6]; zero existing shift).
    // V2-15: 120 -> 121 (the bare sheep skin's single tile — the next
    // free cell of the scan [11, 6]; zero existing shift).
    // V2-16: 121 -> 122 (the snow block's single tile — the next free
    // cell of the scan [12, 6]; zero existing shift).
    EXPECT_EQ(l.tiles.size(), 122u);
    const struct {
        const char* name;
        int col;
        int row;
    } preV25[] = {
        {"allium", 4, 4}, {"beacon", 9, 1}, {"bedrock", 0, 0},
        {"birch_leaves", 13, 2}, {"birch_sapling", 12, 1}, {"birch_trunk_side", 10, 2},
        {"birch_trunk_top", 11, 2}, {"cactus", 4, 3}, {"campfire", 12, 0},
        {"campfire_extinguished", 13, 0}, {"chicken_body", 1, 6}, {"chicken_head", 2, 6},
        {"chicken_legs", 8, 4}, {"coal_ore", 1, 0}, {"cobblestone", 9, 0},
        {"cornflower", 4, 6}, {"cow_body", 1, 5}, {"cow_head", 2, 5},
        {"cow_legs", 8, 5}, {"crafting_table_side", 0, 4}, {"crafting_table_top", 1, 4},
        {"diamond_ore", 2, 0}, {"dirt", 3, 0}, {"farmland_side", 7, 5},
        {"farmland_top", 7, 4}, {"farmland_wet", 9, 4}, {"fence", 8, 2},
        {"fire", 10, 0}, {"furnace_side", 1, 7}, {"furnace_top", 2, 7},
        {"glass", 0, 1}, {"glass_pane", 8, 3}, {"glowstone", 1, 1},
        {"gold_ore", 2, 1}, {"grass_side", 3, 1}, {"grass_top", 0, 2},
        {"gravel", 8, 0}, {"honey", 15, 1}, {"iron_block", 8, 1},
        {"iron_ore", 1, 2}, {"lapis_ore", 4, 2}, {"lava", 7, 6},
        {"leaves", 2, 2}, {"oak_sapling", 10, 1}, {"obsidian", 7, 7},
        {"pa_arm", 13, 5}, {"pb_shirt", 12, 5}, {"ph_face", 9, 5},
        {"ph_hair", 10, 5}, {"ph_side", 11, 5}, {"pl_boot", 15, 5},
        {"pl_pants", 14, 5}, {"planks", 2, 4}, {"sand", 3, 2},
        {"sandstone", 14, 0}, {"sheep_body", 3, 5}, {"sheep_head", 0, 6},
        {"sheep_legs", 8, 6}, {"skeleton_body", 3, 6}, {"skeleton_head", 0, 7},
        {"smooth_sandstone", 15, 0}, {"spider_body", 5, 7}, {"spider_head", 6, 7},
        {"spruce_leaves", 12, 2}, {"spruce_sapling", 11, 1}, {"spruce_trunk_side", 13, 1},
        {"spruce_trunk_top", 14, 1}, {"stone", 0, 3}, {"torch", 11, 0},
        {"tulip", 4, 5}, {"water", 1, 3}, {"wheat_1", 6, 2},
        {"wheat_2", 6, 3}, {"wheat_3", 6, 4}, {"wheat_4", 6, 5},
        {"wheat_5", 6, 6}, {"wheat_6", 7, 2}, {"wheat_7", 7, 3},
        {"wood_side", 2, 3}, {"wood_top", 3, 3}, {"wool", 3, 7},
        {"wool_black", 6, 1}, {"wool_blue", 4, 7}, {"wool_brown", 4, 1},
        {"wool_cyan", 5, 6}, {"wool_gray", 5, 1}, {"wool_green", 5, 2},
        {"wool_light_blue", 5, 5}, {"wool_magenta", 5, 4}, {"wool_orange", 6, 0},
        {"wool_pink", 7, 0}, {"wool_purple", 5, 3}, {"wool_red", 4, 0},
        {"wool_silver", 7, 1}, {"wool_yellow", 5, 0}, {"zombie_body", 0, 5},
        {"zombie_head", 3, 4}
    };
    ASSERT_EQ(sizeof(preV25) / sizeof(preV25[0]), 97u); // the full census
    for (const auto& e : preV25) {
        const auto it = l.tiles.find(e.name);
        ASSERT_NE(it, l.tiles.end()) << e.name;
        EXPECT_EQ(it->second.col, e.col) << e.name;
        EXPECT_EQ(it->second.row, e.row) << e.name;
    }
    const struct {
        const char* name;
        int col;
        int row;
    } v25[] = {
        {"pig_body", 8, 7}, {"pig_face", 9, 7},
    };
    for (const auto& e : v25) {
        const auto it = l.tiles.find(e.name);
        ASSERT_NE(it, l.tiles.end()) << e.name;
        EXPECT_EQ(it->second.col, e.col) << e.name;
        EXPECT_EQ(it->second.row, e.row) << e.name;
    }
    // V2-7: the material batch's three tiles continue the row-7 free run
    // after the pig's [8..9, 7] (design order: gold / diamond / furnace
    // front) — the 99 pre-V2-7 entries above keep their EXACT cells (the
    // zero-shift red line, no baseline re-anchor).
    const struct {
        const char* name;
        int col;
        int row;
    } v27[] = {
        {"gold_block", 10, 7}, {"diamond_block", 11, 7},
        {"furnace_front", 12, 7},
    };
    for (const auto& e : v27) {
        const auto it = l.tiles.find(e.name);
        ASSERT_NE(it, l.tiles.end()) << e.name;
        EXPECT_EQ(it->second.col, e.col) << e.name;
        EXPECT_EQ(it->second.row, e.row) << e.name;
    }
    // V2-12: the enchantment batch's two tiles (the enchanting table + the
    // bookshelf) join the row-2 free run [9, 2] / [14, 2] — the 102
    // pre-V2-12 entries above keep their EXACT cells (the zero-shift red
    // line, no baseline re-anchor).
    const struct {
        const char* name;
        int col;
        int row;
    } v212[] = {
        {"enchanting_table", 9, 2}, {"bookshelf", 14, 2},
    };
    for (const auto& e : v212) {
        const auto it = l.tiles.find(e.name);
        ASSERT_NE(it, l.tiles.end()) << e.name;
        EXPECT_EQ(it->second.col, e.col) << e.name;
        EXPECT_EQ(it->second.row, e.row) << e.name;
    }
    // V2-13: the bed batch's 15 tiles (the dyed bed family — the wool's
    // exact vocabulary) take the FREE cells of the 104-entry layout
    // (sorted name order onto the deterministic (row, col) free-cell
    // scan: the [15, 2] / row-3 [9..15] / row-4 [10..15] / [9..10, 6]
    // runs) — the 104 pre-V2-13 entries above keep their EXACT cells
    // (the zero-shift red line, no baseline re-anchor).
    const struct {
        const char* name;
        int col;
        int row;
    } v213[] = {
        {"bed", 15, 2}, {"bed_black", 9, 3}, {"bed_blue", 10, 3},
        {"bed_brown", 11, 3}, {"bed_cyan", 12, 3}, {"bed_gray", 13, 3},
        {"bed_green", 14, 3}, {"bed_light_blue", 15, 3},
        {"bed_magenta", 10, 4}, {"bed_orange", 11, 4}, {"bed_pink", 12, 4},
        {"bed_purple", 13, 4}, {"bed_red", 14, 4}, {"bed_silver", 15, 4},
        {"bed_yellow", 9, 6},
    };
    ASSERT_EQ(sizeof(v213) / sizeof(v213[0]), 15u);
    for (const auto& e : v213) {
        const auto it = l.tiles.find(e.name);
        ASSERT_NE(it, l.tiles.end()) << e.name;
        EXPECT_EQ(it->second.col, e.col) << e.name;
        EXPECT_EQ(it->second.row, e.row) << e.name;
    }
    // V2-14: the bat's single tile (the dark brown-black hide + wing
    // membrane — one tile serves body + head + both wings, no per-face
    // split) takes the first FREE cell of the deterministic (row, col)
    // scan [10, 6] — the 119 pre-V2-14 entries above keep their EXACT
    // cells (the zero-shift red line, no baseline re-anchor).
    const struct {
        const char* name;
        int col;
        int row;
    } v214[] = {
        {"bat", 10, 6},
    };
    for (const auto& e : v214) {
        const auto it = l.tiles.find(e.name);
        ASSERT_NE(it, l.tiles.end()) << e.name;
        EXPECT_EQ(it->second.col, e.col) << e.name;
        EXPECT_EQ(it->second.row, e.row) << e.name;
    }
    // V2-15: the bare sheep skin tile (the sheared sheep's body — the
    // 15 dyed colors REUSE the wool family's existing block tiles, zero
    // new texture for them) takes the next FREE cell of the
    // deterministic (row, col) scan [11, 6] — the 120 pre-V2-15 entries
    // above keep their EXACT cells (the zero-shift red line, no
    // baseline re-anchor).
    const struct {
        const char* name;
        int col;
        int row;
    } v215[] = {
        {"sheep_skin", 11, 6},
    };
    for (const auto& e : v215) {
        const auto it = l.tiles.find(e.name);
        ASSERT_NE(it, l.tiles.end()) << e.name;
        EXPECT_EQ(it->second.col, e.col) << e.name;
        EXPECT_EQ(it->second.row, e.row) << e.name;
    }
    // V2-16: the snow block's single tile (the snowy_taiga biome's
    // surface — the MC snow_block paradigm, one tile serves all six cube
    // faces) takes the next FREE cell of the deterministic (row, col)
    // scan [12, 6] — the 121 pre-V2-16 entries above keep their EXACT
    // cells (the zero-shift red line, no baseline re-anchor).
    const struct {
        const char* name;
        int col;
        int row;
    } v216[] = {
        {"snow", 12, 6},
    };
    for (const auto& e : v216) {
        const auto it = l.tiles.find(e.name);
        ASSERT_NE(it, l.tiles.end()) << e.name;
        EXPECT_EQ(it->second.col, e.col) << e.name;
        EXPECT_EQ(it->second.row, e.row) << e.name;
    }
}
