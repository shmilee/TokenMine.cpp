// Phase 15 UI tests: the pixel font (glyph UV mapping, text width, vertex
// generation). Phase 35: the atlas is assets/textures/font.png (256x512,
// 32px cells indexed by ASCII code, 16x16 glyph at an 8px cell offset) and
// the text scale is a double (fractional scales downscale the glyphs).
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
    // 'Q' = 81: cell (1, 10) -> x0 = 1*32+8 = 40, y0 = 10*32+8 = 328.
    expectUv(glyphUv('Q'), 40, 328);
    // ' ' = 32: cell (0, 4) -> x0 = 8, y0 = 4*32+8 = 136.
    expectUv(glyphUv(' '), 8, 136);
    // '~' = 126: cell (6, 15) -> x0 = 6*32+8 = 200, y0 = 15*32+8 = 488
    // (the last cell).
    expectUv(glyphUv('~'), 200, 488);
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
    EXPECT_DOUBLE_EQ(textWidth(""), 0.0);
    EXPECT_DOUBLE_EQ(textWidth("A"), 16.0);
    EXPECT_DOUBLE_EQ(textWidth("AB"), 33.0); // 16 + 1 spacing + 16
    EXPECT_DOUBLE_EQ(textWidth("AB", 2.0), 66.0); // 32 + 2 + 32
    EXPECT_DOUBLE_EQ(textWidth("AB", 0.5), 16.5); // fractional: 8+0.5+8
    EXPECT_DOUBLE_EQ(textWidth("AB", 0.0), 33.0); // scale clamps to 1
    EXPECT_DOUBLE_EQ(textWidth("AB", -3.0), 33.0);
}

TEST(FontTest, MakeTextVerticesSingleGlyph) {
    std::vector<MeshVertex> v;
    makeTextVertices("A", 10, 20, 1.0, v);
    ASSERT_EQ(v.size(), 6u); // two triangles, non-indexed
    // Top-left corner at (x, y); the glyph spans 16px in both axes.
    EXPECT_FLOAT_EQ(v[0].position.x, 10.0f);
    EXPECT_FLOAT_EQ(v[0].position.y, 20.0f);
    EXPECT_FLOAT_EQ(v[1].position.x, 26.0f);
    EXPECT_FLOAT_EQ(v[2].position.y, 36.0f);
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
    makeTextVertices("AB", 0, 0, 2.0, v);
    ASSERT_EQ(v.size(), 12u);
    // Second glyph starts at the advance (16+1)*scale = 34.
    EXPECT_FLOAT_EQ(v[6].position.x, 34.0f);
    // At scale 2 the glyph is 32px wide/tall.
    EXPECT_FLOAT_EQ(v[1].position.x, 32.0f);
    EXPECT_FLOAT_EQ(v[2].position.y, 32.0f);
}

TEST(FontTest, MakeTextVerticesFractionalScale) {
    std::vector<MeshVertex> v;
    makeTextVertices("AB", 0, 0, 0.5, v);
    ASSERT_EQ(v.size(), 12u);
    // The glyph is 8px wide/tall; the advance is (16+1)*0.5 = 8.5.
    EXPECT_FLOAT_EQ(v[1].position.x, 8.0f);
    EXPECT_FLOAT_EQ(v[2].position.y, 8.0f);
    EXPECT_FLOAT_EQ(v[6].position.x, 8.5f);
}
