#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "renderer/texture_atlas.hpp"

using namespace mc;

namespace {

std::vector<std::string> names(int n) {
    std::vector<std::string> v;
    for (int i = 0; i < n; ++i) {
        v.push_back("tile_" + std::to_string(i));
    }
    return v;
}

} // namespace

TEST(AtlasLayoutTest, SingleTile) {
    auto l = AtlasLayout::compute({"a"});
    ASSERT_TRUE(l.ok()) << l.error().message;
    EXPECT_EQ(l->columns, 1);
    EXPECT_EQ(l->rows, 1);
    EXPECT_EQ(l->atlasWidth, 16);
    EXPECT_EQ(l->atlasHeight, 16);
    EXPECT_EQ(l->tiles.at("a").col, 0);
    EXPECT_EQ(l->tiles.at("a").row, 0);
}

TEST(AtlasLayoutTest, GridSizes) {
    const auto l3 = AtlasLayout::compute({"a", "b", "c"});
    ASSERT_TRUE(l3.ok()) << l3.error().message;
    EXPECT_EQ(l3->columns, 2);
    EXPECT_EQ(l3->rows, 2); // 3 tiles need a 2x2 grid

    const auto l4 = AtlasLayout::compute({"a", "b", "c", "d"});
    ASSERT_TRUE(l4.ok());
    EXPECT_EQ(l4->columns, 2);
    EXPECT_EQ(l4->rows, 2);

    const auto l5 = AtlasLayout::compute(names(5));
    ASSERT_TRUE(l5.ok());
    EXPECT_EQ(l5->columns, 3); // ceil(sqrt(5))
    EXPECT_EQ(l5->rows, 2);

    const auto l17 = AtlasLayout::compute(names(17));
    ASSERT_TRUE(l17.ok());
    EXPECT_EQ(l17->columns, 5); // ceil(sqrt(17))
    EXPECT_EQ(l17->rows, 4);    // ceil(17/5)
    EXPECT_EQ(l17->atlasWidth, 5 * 16);
    EXPECT_EQ(l17->atlasHeight, 4 * 16);
}

TEST(AtlasLayoutTest, AlphabeticalOrderIndependentOfInputOrder) {
    auto l = AtlasLayout::compute({"c", "a", "b"});
    ASSERT_TRUE(l.ok()) << l.error().message;
    // Sorted: a -> (0,0), b -> (1,0), c -> (0,1).
    EXPECT_EQ(l->tiles.at("a").col, 0);
    EXPECT_EQ(l->tiles.at("a").row, 0);
    EXPECT_EQ(l->tiles.at("b").col, 1);
    EXPECT_EQ(l->tiles.at("b").row, 0);
    EXPECT_EQ(l->tiles.at("c").col, 0);
    EXPECT_EQ(l->tiles.at("c").row, 1);
    // Same layout as the sorted input.
    auto sorted = AtlasLayout::compute({"a", "b", "c"});
    ASSERT_TRUE(sorted.ok());
    EXPECT_EQ(l->tiles, sorted->tiles);
}

TEST(AtlasLayoutTest, RejectsDuplicates) {
    auto l = AtlasLayout::compute({"a", "a"});
    ASSERT_FALSE(l.ok());
    EXPECT_NE(l.error().message.find("duplicate"), std::string::npos);
}

TEST(AtlasLayoutTest, RejectsEmpty) {
    auto l = AtlasLayout::compute({});
    ASSERT_FALSE(l.ok());
}

TEST(AtlasLayoutTest, UvCorners2x2) {
    auto l = AtlasLayout::compute({"a", "b", "c", "d"});
    ASSERT_TRUE(l.ok()) << l.error().message;

    // "a" at (col 0, row 0): packed first, so it sits at the BOTTOM-left of
    // the texture (v spans the lower half).
    const Vec2 amin = AtlasLayout::uvMin(*l, "a");
    const Vec2 amax = AtlasLayout::uvMax(*l, "a");
    EXPECT_FLOAT_EQ(amin.x, 0.0f);
    EXPECT_FLOAT_EQ(amin.y, 0.0f);
    EXPECT_FLOAT_EQ(amax.x, 0.5f);
    EXPECT_FLOAT_EQ(amax.y, 0.5f);

    // "d" at (col 1, row 1): last row packed, so it sits at the TOP-right.
    const Vec2 dmin = AtlasLayout::uvMin(*l, "d");
    const Vec2 dmax = AtlasLayout::uvMax(*l, "d");
    EXPECT_FLOAT_EQ(dmin.x, 0.5f);
    EXPECT_FLOAT_EQ(dmin.y, 0.5f);
    EXPECT_FLOAT_EQ(dmax.x, 1.0f);
    EXPECT_FLOAT_EQ(dmax.y, 1.0f);

    // Tiles tile the full [0,1]x[0,1] space exactly (no overlaps/gaps).
    float covered[2][2] = {{0, 0}, {0, 0}};
    for (const char* n : {"a", "b", "c", "d"}) {
        const Vec2 mn = AtlasLayout::uvMin(*l, n);
        const Vec2 mx = AtlasLayout::uvMax(*l, n);
        covered[static_cast<int>(mn.x * 2)][static_cast<int>(mn.y * 2)] +=
            (mx.x - mn.x) * (mx.y - mn.y);
    }
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 2; ++x) {
            EXPECT_NEAR(covered[y][x], 0.25f, 1e-6f);
        }
    }
}

TEST(AtlasLayoutTest, UvUnknownTileThrows) {
    auto l = AtlasLayout::compute({"a"});
    ASSERT_TRUE(l.ok());
    EXPECT_THROW(AtlasLayout::uvMin(*l, "zzz"), std::out_of_range);
    EXPECT_THROW(AtlasLayout::uvMax(*l, "zzz"), std::out_of_range);
}
