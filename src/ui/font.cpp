#include "ui/font.hpp"

#include <cstddef>

namespace mc {

GlyphUv glyphUv(char c) {
    int code = static_cast<unsigned char>(c);
    if (code < 32 || code > 126) {
        code = '?'; // missing glyphs render as the question mark
    }
    const int col = code % 8;
    const int row = code / 8;
    const int x0 = col * kFontCell + kGlyphOffset;
    const int y0 = row * kFontCell + kGlyphOffset;
    return GlyphUv {
        static_cast<float>(x0) / kFontAtlasWidth,
        static_cast<float>(y0) / kFontAtlasHeight,
        static_cast<float>(x0 + kGlyphSize) / kFontAtlasWidth,
        static_cast<float>(y0 + kGlyphSize) / kFontAtlasHeight};
}

double textWidth(const std::string& text, double scale) {
    if (scale <= 0.0) {
        scale = 1.0;
    }
    if (text.empty()) {
        return 0.0;
    }
    const std::size_t n = text.size();
    return static_cast<double>(n) * kGlyphSize * scale +
           static_cast<double>(n - 1) * scale;
}

void makeTextVertices(const std::string& text, int x, int y, double scale,
                      std::vector<MeshVertex>& out) {
    if (scale <= 0.0) {
        scale = 1.0;
    }
    const double adv = (kGlyphSize + 1) * scale; // glyph + one spacing px
    double cx = x;
    for (const char c : text) {
        const GlyphUv uv = glyphUv(c);
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
