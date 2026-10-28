#include "ui/font.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace mc {

namespace {

// A cell's 16x16 rect normalized to atlas UVs (v0 = the glyph top).
GlyphUv cellUv(int col, int row) {
    const int x0 = col * kFontCell + kGlyphOffset;
    const int y0 = row * kFontCell + kGlyphOffset;
    return GlyphUv {
        static_cast<float>(x0) / kFontAtlasWidth,
        static_cast<float>(y0) / kFontAtlasHeight,
        static_cast<float>(x0 + kGlyphSize) / kFontAtlasWidth,
        static_cast<float>(y0 + kGlyphSize) / kFontAtlasHeight};
}

} // namespace

GlyphUv glyphUv(uint32_t cp) {
    if (cp >= 32 && cp <= 126) {
        // ASCII: the original cell (c % 8, c / 8), untouched.
        return cellUv(static_cast<int>(cp) % 8, static_cast<int>(cp) / 8);
    }
    // CRAFT-4: kCjkGlyphs is codepoint-sorted with an EXPLICIT atlas
    // cell per entry (the original 32 fill the dead cells 0..31 of rows
    // 0..3; 作 0x4F5C takes the dead ASCII-DEL cell 127 — see font.hpp).
    const auto it = std::lower_bound(
        std::begin(kCjkGlyphs), std::end(kCjkGlyphs), cp,
        [](const CjkGlyph& g, uint32_t c) { return g.cp < c; });
    if (it != std::end(kCjkGlyphs) && it->cp == cp) {
        return cellUv(it->cell % 8, it->cell / 8);
    }
    return cellUv('?' % 8, '?' / 8);
}

std::optional<uint32_t> utf8NextCodepoint(const std::string& text,
                                          std::size_t& pos) {
    if (pos >= text.size()) {
        return std::nullopt;
    }
    const auto byte = [&text](std::size_t i) {
        return static_cast<unsigned char>(text[i]);
    };
    const unsigned char c0 = byte(pos);
    if (c0 < 0x80) {
        ++pos;
        return c0;
    }
    const auto skipOne = [&pos]() -> std::optional<uint32_t> {
        // Malformed byte: the caller renders its own '?' and resumes
        // one byte later (the legacy byte-wise fallback behavior).
        ++pos;
        return std::nullopt;
    };
    const std::size_t n = text.size();
    // Sequence length + the first byte's strict range (the guards reject
    // overlong encodings, UTF-8 surrogates and codepoints > U+10FFFF).
    int len = 0;
    unsigned int cp = 0;
    if (c0 >= 0xC2 && c0 <= 0xDF) {
        len = 2;
        cp = c0 & 0x1Fu;
    } else if (c0 == 0xE0) {
        len = 3;
        cp = 0; // second byte must be 0xA0..0xBF (checked below)
    } else if (c0 >= 0xE1 && c0 <= 0xEC) {
        len = 3;
        cp = c0 & 0x0Fu;
    } else if (c0 == 0xED) {
        len = 3;
        cp = 0x0Du; // second byte must be 0x80..0x9F (no surrogates)
    } else if (c0 >= 0xEE && c0 <= 0xEF) {
        len = 3;
        cp = c0 & 0x0Fu;
    } else if (c0 == 0xF0) {
        len = 4;
        cp = 0; // second byte must be 0x90..0xBF
    } else if (c0 >= 0xF1 && c0 <= 0xF3) {
        len = 4;
        cp = c0 & 0x07u;
    } else if (c0 == 0xF4) {
        len = 4;
        cp = 0x04u; // c0 & 0x07; second byte must be 0x80..0x8F (<= U+10FFFF)
    } else {
        // 0x80..0xC1 (continuation / overlong) and 0xF5..0xFF (no 5/6-byte
        // UTF-8) are never a valid sequence start.
        return skipOne();
    }
    if (pos + static_cast<std::size_t>(len) > n) {
        return skipOne(); // truncated
    }
    unsigned int lo = 0x80, hi = 0xBF;
    if (c0 == 0xE0) {
        lo = 0xA0;
    } else if (c0 == 0xED) {
        hi = 0x9F;
    } else if (c0 == 0xF0) {
        lo = 0x90;
    } else if (c0 == 0xF4) {
        hi = 0x8F;
    }
    const unsigned char c1 = byte(pos + 1);
    if (c1 < lo || c1 > hi) {
        return skipOne();
    }
    cp = (cp << 6) | (c1 & 0x3Fu);
    for (int i = 2; i < len; ++i) {
        const unsigned char ci = byte(pos + static_cast<std::size_t>(i));
        if (ci < 0x80 || ci > 0xBF) {
            return skipOne();
        }
        cp = (cp << 6) | (ci & 0x3Fu);
    }
    pos += static_cast<std::size_t>(len);
    return cp;
}

double textWidth(const std::string& text, double scale) {
    if (scale <= 0.0) {
        scale = 1.0;
    }
    if (text.empty()) {
        return 0.0;
    }
    // PRE-1: the width counts DECODED codepoints, not bytes — a CJK
    // character is one 16px advance, not three.
    std::size_t pos = 0;
    std::size_t count = 0;
    while (pos < text.size()) {
        utf8NextCodepoint(text, pos);
        ++count;
    }
    return static_cast<double>(count) * kGlyphSize * scale +
           static_cast<double>(count - 1) * scale;
}

std::string fitLabel(const std::string& name, const std::string& suffix,
                     double scale, int availPx) {
    if (textWidth(name + suffix, scale) <= availPx) {
        return name;
    }
    const int nameAvail =
        std::max(0, availPx - static_cast<int>(textWidth(suffix, scale)));
    std::string out = name;
    // Trim one codepoint at a time (item ids are ASCII data; the decoder
    // keeps this well-formed regardless).
    while (!out.empty() && textWidth(out, scale) > nameAvail) {
        std::size_t pos = 0;
        std::size_t last = 0; // start of the final codepoint
        while (pos < out.size()) {
            last = pos;
            utf8NextCodepoint(out, pos);
        }
        out.erase(last);
    }
    return out;
}

void makeTextVertices(const std::string& text, int x, int y, double scale,
                      std::vector<MeshVertex>& out) {
    if (scale <= 0.0) {
        scale = 1.0;
    }
    const double adv = (kGlyphSize + 1) * scale; // glyph + one spacing px
    double cx = x;
    // PRE-1: iterate the decoded UTF-8 codepoints; a malformed byte
    // decodes to nullopt and renders its own '?' (the decoder advanced
    // one byte per call).
    std::size_t pos = 0;
    while (pos < text.size()) {
        const uint32_t cp = utf8NextCodepoint(text, pos).value_or('?');
        const GlyphUv uv = glyphUv(cp);
        const float w = static_cast<float>(kGlyphSize * scale);
        const float x0 = static_cast<float>(cx);
        const float y0 = static_cast<float>(y);
        const auto put = [&](float px, float py, float u, float vv) {
            MeshVertex vert;
            vert.position = Vec3 {px, py, 0.0f};
            vert.uv = Vec2 {u, vv};
            out.push_back(vert);
        };
        // Two triangles; screen y grows downward, so the glyph's top edge
        // (v0) is at the smaller screen y.
        put(x0, y0, uv.u0, uv.v0);
        put(x0 + w, y0, uv.u1, uv.v0);
        put(x0 + w, y0 + w, uv.u1, uv.v1);
        put(x0, y0, uv.u0, uv.v0);
        put(x0 + w, y0 + w, uv.u1, uv.v1);
        put(x0, y0 + w, uv.u0, uv.v1);
        cx += adv;
    }
}

} // namespace mc
