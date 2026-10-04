// Phase 15 UI tests: the pixel font (glyph UV mapping, text width, vertex
// generation). The font atlas is assets/textures/font.png (128x256, 16px
// cells indexed by ASCII code, 8x8 glyph at a 4px cell offset).
#include <gtest/gtest.h>

#include <vector>

#include "renderer/mesh.hpp"
#include "ui/font.hpp"

using namespace mc;

namespace {

void expectUv(const GlyphUv& uv, int x0, int y0) {
    EXPECT_FLOAT_EQ(uv.u0, static_cast<float>(x0) / kFontAtlasWidth);
    EXPECT_FLOAT_EQ(uv.v0, static_cast<float>(y0) / kFontAtlasHeight);
    EXPECT_FLOAT_EQ(uv.u1,
                    static_cast<float>(x0 + kGlyphSize) / kFontAtlasWidth);
    EXPECT_FLOAT_EQ(uv.v1,
                    static_cast<float>(y0 + kGlyphSize) / kFontAtlasHeight);
}

} // namespace

TEST(FontTest, GlyphUvIsAsciiIndexedCell) {
    // 'Q' = 81: cell (1, 10) -> x0 = 1*16+4 = 20, y0 = 10*16+4 = 164.
    expectUv(glyphUv('Q'), 20, 164);
    // ' ' = 32: cell (0, 4) -> x0 = 4, y0 = 68.
    expectUv(glyphUv(' '), 4, 68);
    // '~' = 126: cell (6, 15) -> x0 = 100, y0 = 244 (the last cell).
    expectUv(glyphUv('~'), 100, 244);
}

TEST(FontTest, GlyphUvStaysInsideTheAtlas) {
    for (int c = 32; c <= 126; ++c) {
        const GlyphUv uv = glyphUv(static_cast<char>(c));
        EXPECT_GT(uv.u0, 0.0f);
        EXPECT_LT(uv.u1, 1.0f);
        EXPECT_GT(uv.v0, 0.0f);
        EXPECT_LT(uv.v1, 1.0f);
        EXPECT_LT(uv.u0, uv.u1);
        EXPECT_LT(uv.v0, uv.v1);
    }
}

TEST(FontTest, UnknownCharactersMapToQuestionMark) {
    const GlyphUv q = glyphUv('?');
    for (char c : {'\x01', '\x7f', '\x80', '\xff'}) {
        const GlyphUv uv = glyphUv(c);
        EXPECT_FLOAT_EQ(uv.u0, q.u0);
        EXPECT_FLOAT_EQ(uv.v0, q.v0);
        EXPECT_FLOAT_EQ(uv.u1, q.u1);
        EXPECT_FLOAT_EQ(uv.v1, q.v1);
    }
}

TEST(FontTest, TextWidth) {
    EXPECT_EQ(textWidth("", 1), 0);
    EXPECT_EQ(textWidth("A", 1), 8);
    EXPECT_EQ(textWidth("AB", 1), 17); // 8 + 1 spacing + 8
    EXPECT_EQ(textWidth("AB", 2), 34); // 16 + 2 + 16
    EXPECT_EQ(textWidth("AB", 0), 17); // scale clamps to 1
    EXPECT_EQ(textWidth("AB", -3), 17);
}

TEST(FontTest, MakeTextVerticesSingleGlyph) {
    std::vector<MeshVertex> v;
    makeTextVertices("A", 10, 20, 1, v);
    ASSERT_EQ(v.size(), 6u); // two triangles, non-indexed
    // Top-left corner at (x, y); the glyph spans 8px in both axes.
    EXPECT_FLOAT_EQ(v[0].position.x, 10.0f);
    EXPECT_FLOAT_EQ(v[0].position.y, 20.0f);
    EXPECT_FLOAT_EQ(v[1].position.x, 18.0f);
    EXPECT_FLOAT_EQ(v[2].position.y, 28.0f);
    // UVs: top edge = v0 (the glyph's top, smaller v), bottom = v1.
    const GlyphUv a = glyphUv('A');
    EXPECT_FLOAT_EQ(v[0].uv.x, a.u0);
    EXPECT_FLOAT_EQ(v[0].uv.y, a.v0);
    EXPECT_FLOAT_EQ(v[1].uv.x, a.u1);
    EXPECT_FLOAT_EQ(v[1].uv.y, a.v0);
    EXPECT_FLOAT_EQ(v[2].uv.x, a.u1);
    EXPECT_FLOAT_EQ(v[2].uv.y, a.v1);
    EXPECT_FLOAT_EQ(v[5].uv.x, a.u0);
    EXPECT_FLOAT_EQ(v[5].uv.y, a.v1);
}

TEST(FontTest, MakeTextVerticesAdvanceAndScale) {
    std::vector<MeshVertex> v;
    makeTextVertices("AB", 0, 0, 2, v);
    ASSERT_EQ(v.size(), 12u);
    // Second glyph starts at the advance (8+1)*scale = 18.
    EXPECT_FLOAT_EQ(v[6].position.x, 18.0f);
    // At scale 2 the glyph is 16px wide/tall.
    EXPECT_FLOAT_EQ(v[1].position.x, 16.0f);
    EXPECT_FLOAT_EQ(v[2].position.y, 16.0f);
}
