// Phase 15 UI tests: the pixel font (glyph UV mapping, text width, vertex
// generation). Phase 35: the atlas is assets/textures/font.png (32px cells
// indexed by ASCII code, 16x16 glyph at an 8px cell offset) and the text
// scale is a double (fractional scales downscale the glyphs). PRE-1: UTF-8
// decoding + the CJK glyph set (atlas rows 0..3, the dead ASCII control
// cells) and the byte pin of the generated atlas. UX-6: the CJK set grew
// 33 -> 392 (the atlas 256x512 -> 256x2080; the top 512 rows are the
// pre-UX-6 atlas, byte-identical red line).
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include <zlib.h>

#include "core/png.hpp"
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

// FNV-1a 64 over the file/region bytes (the determinism-pin convention).
std::string fnv1a64Hex(const std::string& data) {
    std::uint64_t h = 1469598103934665603ULL; // FNV-1a 64 offset basis
    for (char ch : data) {
        h ^= static_cast<unsigned char>(ch);
        h *= 1099511628211ULL;
    }
    char hex[17];
    std::snprintf(hex, sizeof(hex), "%016llx",
                  static_cast<unsigned long long>(h));
    return std::string(hex);
}

// UX-6 red line helper: re-encode a 256x512 RGBA region byte-identically
// to the pre-UX-6 tools/generate_font.py writer — signature + IHDR
// (256,512,8,6,0,0,0) + one IDAT (per-row filter None + zlib level 6, the
// Python zlib.compress default) + IEND, big-endian chunk lengths and
// CRC-32. Hashing the result pins the top-512 rows against the old
// whole-file FNV (a collision would be ~2^-64, so equality IS
// byte-identity of the region).
std::string legacyWriterStylePng(const std::vector<std::uint8_t>& rgba,
                                 int w, int h) {
    auto be32 = [](std::uint32_t v) {
        std::string s(4, '\0');
        s[0] = static_cast<char>(v >> 24);
        s[1] = static_cast<char>(v >> 16);
        s[2] = static_cast<char>(v >> 8);
        s[3] = static_cast<char>(v);
        return s;
    };
    auto chunk = [&be32](const char* tag, const std::string& body) {
        uLong crc = crc32(0L, Z_NULL, 0);
        crc = crc32(crc, reinterpret_cast<const Bytef*>(tag), 4);
        crc = crc32(crc, reinterpret_cast<const Bytef*>(body.data()),
                    static_cast<uInt>(body.size()));
        return be32(static_cast<std::uint32_t>(body.size())) + tag + body +
               be32(static_cast<std::uint32_t>(crc));
    };
    std::string ihdr = be32(static_cast<std::uint32_t>(w)) +
                       be32(static_cast<std::uint32_t>(h));
    ihdr += static_cast<char>(8);   // bit depth
    ihdr += static_cast<char>(6);   // color type RGBA
    ihdr += static_cast<char>(0);   // compression
    ihdr += static_cast<char>(0);   // filter method
    ihdr += static_cast<char>(0);   // no interlace
    std::string raw;
    raw.reserve(static_cast<std::size_t>(h) * (static_cast<std::size_t>(w) * 4 + 1));
    for (int y = 0; y < h; ++y) {
        raw += static_cast<char>(0); // filter: None
        raw.append(reinterpret_cast<const char*>(
                       rgba.data() + static_cast<std::size_t>(y) * w * 4),
                   static_cast<std::size_t>(w) * 4);
    }
    uLongf clen = compressBound(static_cast<uLong>(raw.size()));
    std::vector<Bytef> buf(clen);
    const int zr = compress2(buf.data(), &clen,
                             reinterpret_cast<const Bytef*>(raw.data()),
                             static_cast<uLong>(raw.size()), 6);
    EXPECT_EQ(zr, Z_OK);
    return std::string("\x89PNG\r\n\x1a\n") + chunk("IHDR", ihdr) +
           chunk("IDAT", std::string(reinterpret_cast<const char*>(buf.data()),
                                     clen)) +
           chunk("IEND", std::string());
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
    // Pinned cells: the PRE-1 entries keep their cells (the sorted index
    // within the original 32); x0 = col*32+8, y0 = row*32+8.
    expectUv(glyphUv(0x201C), 8, 8);     // " cell 0 -> (0,0)
    expectUv(glyphUv(0x3002), 72, 8);    // 。 cell 2 -> (2,0)
    expectUv(glyphUv(0x4F60), 136, 8);   // 死 cell 4 -> (4,0)
    expectUv(glyphUv(0x9700), 8, 104);   // 包 cell 24 -> (0,3)
    expectUv(glyphUv(0xFF0C), 136, 104); // ， cell 28 -> (4,3)
    expectUv(glyphUv(0xFF1F), 232, 104); // ？ cell 31 -> (7,3)
    // CRAFT-4: 作 (the PRE-1 omission — the table title rendered 工?台)
    // takes the dead ASCII-DEL cell 127 = (col 7, row 15).
    expectUv(glyphUv(0x4F5C), 232, 488);
    // UX-6: the extended set starts at cell 128 (row 16, col 0) and its
    // last glyph 默 takes cell 486 (row 60, col 6); 镐 (cell 465) sits at
    // (col 1, row 58).
    expectUv(glyphUv(0x00B7), 8, 16 * kFontCell + kGlyphOffset);
    expectUv(glyphUv(0x9550), kFontCell + kGlyphOffset,
             58 * kFontCell + kGlyphOffset);
    expectUv(glyphUv(0x9ED8), 6 * kFontCell + kGlyphOffset,
             60 * kFontCell + kGlyphOffset);
    // Every entry stays inside the atlas; the legacy 33 inside the
    // legacy region (rows 0..15), the extended 359 inside rows 16..60.
    for (const CjkGlyph& g : kCjkGlyphs) {
        const GlyphUv uv = glyphUv(g.cp);
        EXPECT_GE(uv.v0, 0.0f);
        EXPECT_LT(uv.v1, 1.0f);
        if (g.cell >= 128) {
            EXPECT_LE(uv.v1,
                      static_cast<float>(61 * kFontCell) / kFontAtlasHeight);
        } else {
            EXPECT_LE(uv.v1,
                      static_cast<float>(16 * kFontCell) / kFontAtlasHeight);
        }
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
    // UX-6: the set grew 33 -> 392 (codepoint order, the charset
    // endpoints pinned); the legacy 33 keep their exact cells, and the
    // extended 359 take cells 128..486 in codepoint-rank order.
    ASSERT_EQ(std::size(kCjkGlyphs), 392u);
    for (std::size_t i = 1; i < std::size(kCjkGlyphs); ++i) {
        EXPECT_GT(kCjkGlyphs[i].cp, kCjkGlyphs[i - 1].cp); // sorted, unique
    }
    EXPECT_EQ(kCjkGlyphs[0].cp, 0x00B7u);   // charset first: ·
    EXPECT_EQ(kCjkGlyphs[391].cp, 0xFF1Fu); // charset last: ？
    std::size_t legacyCount = 0;
    for (const CjkGlyph& g : kCjkGlyphs) {
        EXPECT_GT(g.cp, 0x7Fu);
        EXPECT_LE(g.cp, 0xFFFFu); // BMP only (the atlas is 2-byte wide)
        // CRAFT-4: 作 takes the dead ASCII-DEL cell 127; the PRE-1 32
        // keep their cells 0..31; UX-6: the extended glyphs start at
        // cell 128 (row 16).
        if (g.cell < 32) {
            ++legacyCount;
        } else if (g.cp == 0x4F5C) {
            EXPECT_EQ(g.cell, 127);
            ++legacyCount;
        } else {
            EXPECT_GE(g.cell, 128);
            EXPECT_LE(g.cell, 486); // rows 16..60
        }
    }
    EXPECT_EQ(legacyCount, 33u);
    // The legacy set (semantic pin): 19 localized-UI characters + 10
    // Chinese punctuation marks + the 4-character reserve.
    const std::vector<uint32_t> legacyExpected = {
        // 你死了按重生熔炉工作台背包合成需要关闭
        0x4F60, 0x6B7B, 0x4E86, 0x6309, 0x91CD, 0x751F, 0x7194, 0x7089,
        0x5DE5, 0x4F5C, 0x53F0, 0x80CC, 0x5305, 0x5408, 0x6210, 0x9700,
        0x8981, 0x5173, 0x95ED,
        // ，。！？：；“”（）
        0xFF0C, 0x3002, 0xFF01, 0xFF1F, 0xFF1A, 0xFF1B, 0x201C, 0x201D,
        0xFF08, 0xFF09,
        // reserve: 物品方块
        0x7269, 0x54C1, 0x65B9, 0x5757};
    ASSERT_EQ(legacyExpected.size(), 33u);
    std::vector<uint32_t> got(legacyExpected);
    std::sort(got.begin(), got.end());
    std::vector<uint32_t> legacyTab;
    for (const CjkGlyph& g : kCjkGlyphs) {
        if (g.cell < 128) {
            legacyTab.push_back(g.cp);
        }
    }
    EXPECT_EQ(got, legacyTab);
    // The extended 359 fill cells 128..486, one per codepoint in order:
    // walking the sorted table, every cell >= 128 must be the next one.
    int nextCell = 128;
    std::size_t extendedCount = 0;
    for (const CjkGlyph& g : kCjkGlyphs) {
        if (g.cell >= 128) {
            EXPECT_EQ(g.cell, nextCell);
            ++nextCell;
            ++extendedCount;
        }
    }
    EXPECT_EQ(extendedCount, 359u);
    EXPECT_EQ(nextCell, 487); // max used cell 486; rows 61..64 = headroom
}

TEST(FontTest, CjkExtendedGlyphCellsPinned) {
    // UX-6 tutorial vocabulary: exact cells for the key extended glyphs
    // (the UX-7 onboarding text must never render '?').
    const std::vector<std::pair<uint32_t, int>> pinned = {
        {0x50F5, 172}, // 僵
        {0x5E8A, 260}, // 床
        {0x5251, 192}, // 剑
        {0x9550, 465}, // 镐
        {0x7761, 380}, // 睡
        {0x5065, 171}, // 健
        {0x5EB7, 264}, // 康
        {0x5B89, 243}, // 安
        {0x5168, 177}, // 全
        {0x5E87, 259}, // 庇
        {0x62A4, 284}, // 护
        {0x6240, 281}, // 所
        {0x6316, 287}, // 挖
        {0x6324, 288}, // 挤
        {0x5976, 237}, // 奶
        {0x6BD2, 340}, // 毒
        {0x7BAD, 395}, // 箭
        {0x5CA9, 255}, // 岩
        {0x706B, 352}, // 火
        {0x71C3, 359}, // 燃
        {0x70E7, 355}}; // 烧
    // expectUv pins the EXACT cell of every codepoint — a '?' fallback
    // would address cell 36 (row 4), which none of the pinned cells are,
    // so the UV pin alone proves table membership.
    for (const auto& [cp, cell] : pinned) {
        const int col = cell % 8, row = cell / 8;
        expectUv(glyphUv(cp), col * kFontCell + kGlyphOffset,
                 row * kFontCell + kGlyphOffset);
    }
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

TEST(FontTest, FitLabelKeepsSuffixAndTrimsName) {
    // At scale 1.0 each codepoint is 17 px (16 + 1 spacing).
    // "planks" (6) + " x4" (3) = 9 x 17 - 1 = 152 px fits 160.
    EXPECT_EQ(fitLabel("planks", " x4", 1.0, 160), "planks");
    // 152 > 140: the suffix (51 px) is kept, the name trims to 140 - 51
    // = 89 px = 5 codepoints ("plank" = 85 px; 6 = 102 px > 89).
    EXPECT_EQ(fitLabel("planks", " x4", 1.0, 140), "plank");
    // Zero room: the name empties, the suffix still fits-checks.
    EXPECT_EQ(fitLabel("planks", " x4", 1.0, 0), "");
    // An empty name stays empty.
    EXPECT_EQ(fitLabel("", " x4", 1.0, 10), "");
    // Fractional scale (the hint rows' real regime): 0.5 -> 8.5 px/char.
    // "crafting_table" (14) + " x1" (3) = 17 x 8.5 - 0.5 = 144 px > 120.
    // Suffix = 3 x 8.5 - 0.5 = 25 px, name budget 120 - 25 = 95 px =
    // 11 chars (11 x 8.5 - 0.5 = 93; 12 = 101.5 > 95) -> "crafting_ta".
    EXPECT_EQ(fitLabel("crafting_table", " x1", 0.5, 120), "crafting_ta");
}

TEST(FontTest, FontPngBytesPinned) {
    // Determinism pin: the generated atlas is byte-stable (re-running
    // tools/generate_font.py must not change it). FNV-1a 64 over the
    // file bytes; the hex below is the UX-6 atlas (256x2080: the legacy
    // 512 rows pixel-identical + 359 extended glyphs in cells 128..486).
    std::ifstream in("assets/textures/font.png", std::ios::binary);
    ASSERT_TRUE(in);
    const std::string data((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
    ASSERT_FALSE(data.empty());
    EXPECT_EQ(fnv1a64Hex(data), "027c6b820b2818ad");
}

TEST(FontTest, FontPngIsTheUx6Atlas) {
    // UX-6: the atlas grew 256x512 -> 256x2080 (65 rows of 32px cells;
    // rows 16..60 = the extended CJK set, rows 61..64 = headroom). The
    // size constants and the actual file dimensions are pinned so a
    // silent reshape fails loudly.
    EXPECT_EQ(kFontAtlasWidth, 256);
    EXPECT_EQ(kFontAtlasHeight, 2080);
    EXPECT_EQ(kFontCell, 32);
    EXPECT_EQ(kGlyphSize, 16);
    EXPECT_EQ(kGlyphOffset, 8);
    std::ifstream in("assets/textures/font.png", std::ios::binary);
    ASSERT_TRUE(in);
    const std::string data((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
    const auto img = Png::decode(data);
    ASSERT_TRUE(img.ok()) << img.error().describe();
    EXPECT_EQ(img.value().width, 256);
    EXPECT_EQ(img.value().height, 2080);
    EXPECT_EQ(img.value().rgba.size(),
              static_cast<std::size_t>(256) * 2080 * 4u);
}

TEST(FontTest, Top512RowsAreTheLegacyAtlas) {
    // UX-6 red line: the top 512 rows of the 256x2080 atlas are the
    // pre-UX-6 256x512 atlas — ASCII rows 4..15 and the legacy CJK rows
    // 0..3 + 作 (cell 127) included, pixel-for-pixel. Re-encoded in the
    // generator's byte format (filter None + zlib level 6), the region
    // must hash to the PINNED pre-UX-6 whole-file FNV: since that writer
    // is deterministic, equal hashes ARE byte-identity of the region.
    std::ifstream in("assets/textures/font.png", std::ios::binary);
    ASSERT_TRUE(in);
    const std::string data((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
    const auto img = Png::decode(data);
    ASSERT_TRUE(img.ok()) << img.error().describe();
    EXPECT_EQ(img.value().width, 256);
    EXPECT_EQ(img.value().height, 2080);
    std::vector<std::uint8_t> top(256u * 512u * 4u);
    std::copy_n(img.value().rgba.begin(), top.size(), top.begin());
    EXPECT_EQ(fnv1a64Hex(legacyWriterStylePng(top, 256, 512)),
              "74b5d1418347dab6");
}
