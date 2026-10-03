// Minimal PNG decoder (dependency-free except zlib).
//
// Supports the subset needed for game textures:
//   - color types: 0 (gray), 2 (RGB), 3 (palette), 4 (gray+alpha), 6 (RGBA)
//   - bit depths: 8 and 16 (16-bit is down-converted to 8)
//   - all 5 row filters (None/Sub/Up/Average/Paeth)
//   - CRC verification on every chunk
// Rejects (with a descriptive error): interlaced PNGs, other bit depths,
// 1/2/4-bit gray and palette images (not used by game assets).
//
// Output is always 8-bit RGBA, rows top to bottom.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/error.hpp"

namespace mc {

struct PngImage {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba; // width * height * 4
};

class Png {
public:
    // Decode PNG file contents.
    static Result<PngImage> decode(const std::string& bytes);

    // Encode an 8-bit RGBA image as PNG (filter None). Used for screenshots
    // and test fixtures.
    static Result<std::string> encode(const PngImage& image);
};

} // namespace mc
