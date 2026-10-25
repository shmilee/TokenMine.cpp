// Phase 15 UI tests: the pixel font (glyph UV mapping, text width, vertex
// generation). Phase 35: the atlas is assets/textures/font.png (256x512,
// 32px cells indexed by ASCII code, 16x16 glyph at an 8px cell offset) and
// the text scale is a double (fractional scales downscale the glyphs).
// PRE-1: UTF-8 decoding + the CJK glyph set (atlas rows 0..3, the dead
// ASCII control cells) and the byte pin of the generated atlas.
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
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

// ----------------------------------------------------------------------
// PRE-1: UTF-8 decoding + the CJK glyph set (atlas rows 0..3 = the dead
// ASCII control cells; the ASCII 32..126 mapping stays bit-identical).
// ----------------------------------------------------------------------

namespace {

// Decodes the whole string into codepoints; `expected` alternates
// "cp" values and -1 for malformed bytes (each rendering one '?').
void expectDecoded(const std::string& text, const std::vector<int64_t>& e) {
    std::vector<int64_t> got;
    std::size_t pos = 0;
    while (pos < text.size()) {
        const auto cp = utf8NextCodepoint(text, pos);
        got.push_back(cp ? static_cast<int64_t>(*cp) : -1);
    }
    EXPECT_EQ(got, e);
}

// The cell formula the pre-PRE-1 code used for ASCII (pinned red line).
GlyphUv legacyAsciiUv(int c) {
    const int col = c % 8;
    const int row = c / 8;
    const int x0 = col * kFontCell + kGlyphOffset;
    const int y0 = row * kFontCell + kGlyphOffset;
    return GlyphUv {
        static_cast<float>(x0) / kFontAtlasWidth,
        static_cast<float>(y0) / kFontAtlasHeight,
        static_cast<float>(x0 + kGlyphSize) / kFontAtlasWidth,
        static_cast<float>(y0 + kGlyphSize) / kFontAtlasHeight};
}

} // namespace

TEST(FontTest, Utf8DecodeSingleByte) {
    std::size_t pos = 0;
    EXPECT_EQ(utf8NextCodepoint("A", pos), 0x41u);
    EXPECT_EQ(pos, 1u);
    EXPECT_EQ(utf8NextCodepoint("A", pos), std::nullopt);
    EXPECT_EQ(pos, 1u); // at the end: nullopt, pos unchanged
}

TEST(FontTest, Utf8DecodeTwoByte) {
    // U+00E9 (e-acute): the legacy byte-wise '?' pair decodes to one
    // codepoint now (it is not in the CJK set, so it renders one '?').
    expectDecoded("\xC3\xA9", {0xE9});
    expectDecoded("\xC2\x80", {0x80}); // minimal 2-byte (no overlongs)
}

TEST(FontTest, Utf8DecodeThreeByteCjk) {
    // 你 = U+4F60, ？ = U+FF1F.
    expectDecoded("\xE4\xBD\xA0", {0x4F60});
    expectDecoded("\xEF\xBC\x9F", {0xFF1F});
}

TEST(FontTest, Utf8DecodeFourByte) {
    expectDecoded("\xF0\x9F\x98\x80", {0x1F600}); // emoji (not in set)
    expectDecoded("\xF0\x90\x80\x80", {0x10000}); // minimal 4-byte
}

TEST(FontTest, Utf8DecodeTruncated) {
    // Each truncated byte resuming is itself malformed (stray
    // continuation), so 2 bytes -> two '?'.
    expectDecoded("\xE4\xBD", {-1, -1});
    expectDecoded("\xC3", {-1});
    expectDecoded("\xF0\x9F", {-1, -1});
}

TEST(FontTest, Utf8DecodeOverlong) {
    // 2-byte overlong of U+00, 3-byte overlong, 4-byte overlong: every
    // byte is consumed as its own '?'.
    expectDecoded("\xC0\x80", {-1, -1});
    expectDecoded("\xC1\xBF", {-1, -1});
    expectDecoded("\xE0\x80\x80", {-1, -1, -1});
    expectDecoded("\xF0\x80\x80\x80", {-1, -1, -1, -1});
}

TEST(FontTest, Utf8DecodeSurrogateRejected) {
    // U+D800 encoded: the second byte 0xA0 is outside the 0x80..0x9F
    // guard, then the strays follow.
    expectDecoded("\xED\xA0\x80", {-1, -1, -1});
    // Validly-shaped 3-byte just under/above the surrogate range.
    expectDecoded("\xED\x9F\xBF", {0xD7FF});
    expectDecoded("\xEE\x80\x80", {0xE000});
}

TEST(FontTest, Utf8DecodeAbove10FFFFRejected) {
    expectDecoded("\xF4\x90\x80\x80", {-1, -1, -1, -1});
    expectDecoded("\xF4\x8F\xBF\xBF", {0x10FFFF}); // the maximum
    expectDecoded("\xF5\x80\x80\x80", {-1, -1, -1, -1});
}

TEST(FontTest, Utf8DecodeStrayAndInvalidStarts) {
    expectDecoded("\x80\x81", {-1, -1});
    expectDecoded("\xFF", {-1});
    expectDecoded("\xC1", {-1});
    // Malformed bytes never skip valid text: the 你 after a stray is
    // still decoded.
    expectDecoded(std::string("\x80") + "\xE4\xBD\xA0", {-1, 0x4F60});
}

TEST(FontTest, Utf8DecodeMixedString) {
    // "你a？" + one stray + "B" (hex escapes must not abut hex letters)
    const std::string mixed = std::string("\xE4\xBD\xA0") + "a" +
                              std::string("\xEF\xBC\x9F") +
                              std::string("\x80") + "B";
    expectDecoded(mixed, {0x4F60, 0x61, 0xFF1F, -1, 0x42});
}

TEST(FontTest, GlyphUvAsciiBitIdentical) {
    // The red line: every ASCII 32..126 UV is exactly the legacy cell.
    for (int c = 32; c <= 126; ++c) {
        const GlyphUv legacy = legacyAsciiUv(c);
        const GlyphUv uv = glyphUv(static_cast<uint32_t>(c));
        EXPECT_FLOAT_EQ(uv.u0, legacy.u0);
        EXPECT_FLOAT_EQ(uv.v0, legacy.v0);
        EXPECT_FLOAT_EQ(uv.u1, legacy.u1);
        EXPECT_FLOAT_EQ(uv.v1, legacy.v1);
    }
    // Control characters (0..31) keep their legacy '?' mapping too —
    // they do NOT address the CJK cells that live in rows 0..3.
    const GlyphUv q = glyphUv('?');
    for (uint32_t c = 0; c < 32; ++c) {
        const GlyphUv uv = glyphUv(c);
        EXPECT_FLOAT_EQ(uv.u0, q.u0);
        EXPECT_FLOAT_EQ(uv.v0, q.v0);
        EXPECT_FLOAT_EQ(uv.u1, q.u1);
        EXPECT_FLOAT_EQ(uv.v1, q.v1);
    }
}

TEST(FontTest, GlyphUvCjkCellPositions) {
    // Pinned cells: the i-th codepoint-sorted entry is cell (i % 8,
    // i / 8); x0 = col*32+8, y0 = row*32+8.
    expectUv(glyphUv(0x201C), 8, 8);     // " idx 0 -> cell (0,0)
    expectUv(glyphUv(0x3002), 72, 8);    // 。 idx 2 -> cell (2,0)
    expectUv(glyphUv(0x4F60), 136, 8);   // 你 idx 4 -> cell (4,0)
    expectUv(glyphUv(0x9700), 8, 104);   // 需 idx 24 -> cell (0,3)
    expectUv(glyphUv(0xFF0C), 136, 104); // ， idx 28 -> cell (4,3)
    expectUv(glyphUv(0xFF1F), 232, 104); // ？ idx 31 -> cell (7,3)
    // All 32 set entries stay inside the atlas rows 0..3.
    for (uint32_t cp : kCjkGlyphs) {
        const GlyphUv uv = glyphUv(cp);
        EXPECT_GE(uv.v0, 0.0f);
        EXPECT_LE(uv.v1, 4.0f * kFontCell / kFontAtlasHeight);
    }
}

TEST(FontTest, GlyphUvUnknownCodepointsMapToQuestionMark) {
    const GlyphUv q = glyphUv('?');
    // In-set lookalikes that are NOT in the table (one step off), the
    // é from a legacy Latin-1 byte, a 4-byte emoji, and 0x10000.
    for (uint32_t cp : {0x6D4B, 0x4F61, 0x201B, 0xE9, 0x1F600, 0x10000}) {
        const GlyphUv uv = glyphUv(cp);
        EXPECT_FLOAT_EQ(uv.u0, q.u0);
        EXPECT_FLOAT_EQ(uv.v0, q.v0);
        EXPECT_FLOAT_EQ(uv.u1, q.u1);
        EXPECT_FLOAT_EQ(uv.v1, q.v1);
    }
}

TEST(FontTest, CjkTablePinned) {
    ASSERT_EQ(std::size(kCjkGlyphs), 32u);
    for (std::size_t i = 1; i < std::size(kCjkGlyphs); ++i) {
        EXPECT_GT(kCjkGlyphs[i], kCjkGlyphs[i - 1]); // sorted, unique
    }
    for (uint32_t cp : kCjkGlyphs) {
        EXPECT_GT(cp, 0x7Fu);
        EXPECT_LE(cp, 0xFFFFu); // BMP only (the atlas is 2-byte wide)
    }
    // The exact set (semantic pin): 18 localized-UI characters + 10
    // Chinese punctuation marks + the 4-character reserve.
    const std::vector<uint32_t> expected = {
        // 你死了按重生熔炉工作台背包合成需要关闭
        0x4F60, 0x6B7B, 0x4E86, 0x6309, 0x91CD, 0x751F, 0x7194, 0x7089,
        0x5DE5, 0x53F0, 0x80CC, 0x5305, 0x5408, 0x6210, 0x9700, 0x8981,
        0x5173, 0x95ED,
        // ，。！？：；“”（）
        0xFF0C, 0x3002, 0xFF01, 0xFF1F, 0xFF1A, 0xFF1B, 0x201C, 0x201D,
        0xFF08, 0xFF09,
        // reserve: 物品方块
        0x7269, 0x54C1, 0x65B9, 0x5757};
    ASSERT_EQ(expected.size(), 32u);
    std::vector<uint32_t> got(expected);
    std::sort(got.begin(), got.end());
    std::vector<uint32_t> tab(std::begin(kCjkGlyphs), std::end(kCjkGlyphs));
    EXPECT_EQ(got, tab);
}

TEST(FontTest, TextWidthCountsCodepointsNotBytes) {
    // CJK: one 16px advance per character (the monospace discipline).
    EXPECT_DOUBLE_EQ(textWidth("你"), 16.0);
    EXPECT_DOUBLE_EQ(textWidth("你好"), 33.0);
    EXPECT_DOUBLE_EQ(textWidth("你", 2.0), 32.0);
    EXPECT_DOUBLE_EQ(textWidth("你好", 2.0), 66.0);
    // Mixed text: the codepoint count drives the width.
    EXPECT_DOUBLE_EQ(textWidth("a你"), 33.0);
    EXPECT_DOUBLE_EQ(textWidth("按 R 重生"), 101.0); // 6 codepoints
    EXPECT_DOUBLE_EQ(textWidth("你死了！"), 67.0);   // 4 codepoints
    // ASCII is unchanged: byte count == codepoint count.
    EXPECT_DOUBLE_EQ(textWidth("AB"), 33.0);
}

TEST(FontTest, TextWidthMalformedBytesRenderQuestionMarks) {
    // A truncated 2-byte sequence is one '?'; two strays are two.
    EXPECT_DOUBLE_EQ(textWidth("\xC3"), 16.0);
    EXPECT_DOUBLE_EQ(textWidth("\x80\x80"), 33.0);
    // Legacy Latin-1 "cafe" (C3 A9 = é): one glyph, not two.
    EXPECT_DOUBLE_EQ(textWidth(std::string("caf") + "\xC3\xA9"), 67.0);
}

TEST(FontTest, MakeTextVerticesCjkGlyph) {
    std::vector<MeshVertex> v;
    makeTextVertices("你", 10, 20, 1.0, v);
    ASSERT_EQ(v.size(), 6u);
    EXPECT_FLOAT_EQ(v[0].position.x, 10.0f);
    EXPECT_FLOAT_EQ(v[0].position.y, 20.0f);
    EXPECT_FLOAT_EQ(v[1].position.x, 26.0f);
    const GlyphUv ni = glyphUv(0x4F60);
    EXPECT_FLOAT_EQ(v[0].uv.x, ni.u0);
    EXPECT_FLOAT_EQ(v[0].uv.y, ni.v0);
    EXPECT_FLOAT_EQ(v[1].uv.x, ni.u1);
    EXPECT_FLOAT_EQ(v[5].uv.y, ni.v1);
}

TEST(FontTest, MakeTextVerticesMixedAdvance) {
    std::vector<MeshVertex> v;
    makeTextVertices("a你", 0, 0, 1.0, v);
    ASSERT_EQ(v.size(), 12u);
    // Same advance as "ab": the second glyph starts at 17 px.
    EXPECT_FLOAT_EQ(v[6].position.x, 17.0f);
    const GlyphUv a = glyphUv('a');
    const GlyphUv ni = glyphUv(0x4F60);
    EXPECT_FLOAT_EQ(v[0].uv.x, a.u0);
    EXPECT_FLOAT_EQ(v[0].uv.y, a.v0);
    EXPECT_FLOAT_EQ(v[6].uv.x, ni.u0);
    EXPECT_FLOAT_EQ(v[6].uv.y, ni.v0);
}

TEST(FontTest, MakeTextVerticesMalformedRendersQuestion) {
    // A truncated 你 (E4 BD): two malformed bytes = two '?' glyphs.
    std::vector<MeshVertex> v;
    makeTextVertices("\xE4\xBD", 0, 0, 1.0, v);
    ASSERT_EQ(v.size(), 12u);
    const GlyphUv q = glyphUv('?');
    EXPECT_FLOAT_EQ(v[0].uv.x, q.u0);
    EXPECT_FLOAT_EQ(v[0].uv.y, q.v0);
    EXPECT_FLOAT_EQ(v[6].uv.x, q.u0);
    EXPECT_FLOAT_EQ(v[6].uv.y, q.v0);
}

TEST(FontTest, FontPngBytesPinned) {
    // Determinism pin: the generated atlas is byte-stable (re-running
    // tools/generate_font.py must not change it). FNV-1a 64 over the
    // file bytes; the hex below is the PRE-1 atlas (the ASCII rows
    // 4..15 pixel-identical to the Phase 35 file + the 32 CJK glyphs
    // in the dead rows 0..3).
    std::ifstream in("assets/textures/font.png", std::ios::binary);
    ASSERT_TRUE(in);
    const std::string data((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
    ASSERT_FALSE(data.empty());
    uint64_t h = 1469598103934665603ULL; // FNV-1a 64 offset basis
    for (char ch : data) {
        h ^= static_cast<unsigned char>(ch);
        h *= 1099511628211ULL;
    }
    char hex[17];
    std::snprintf(hex, sizeof(hex), "%016llx",
                  static_cast<unsigned long long>(h));
    EXPECT_STREQ(hex, "3e4d748347f5f658");
}
