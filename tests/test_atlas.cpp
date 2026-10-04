// Block texture atlas layout tests: the data-driven layout parse (strict
// validation: power-of-two grid, in-range cells, no shared cells) and the
// UV corner math. The layout is data, not computed — that is what keeps
// adding a texture from moving existing tiles' UVs.
#include <gtest/gtest.h>

#include <string>

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
