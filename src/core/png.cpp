#include "core/png.hpp"

#include <cstring>

#include <zlib.h>

namespace mc {

namespace {

constexpr unsigned char kSignature[8] = {0x89, 'P',  'N', 'G',
                                         0x0D, 0x0A, 0x1A, 0x0A};

std::uint32_t readU32BE(const unsigned char* p) {
    return (static_cast<std::uint32_t>(p[0]) << 24) |
           (static_cast<std::uint32_t>(p[1]) << 16) |
           (static_cast<std::uint32_t>(p[2]) << 8) |
           static_cast<std::uint32_t>(p[3]);
}

struct Ihdr {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    int bitDepth = 0;
    int colorType = 0;
    int interlace = 0;
};

// Channels per pixel for each supported color type.
int channelsFor(int colorType) {
    switch (colorType) {
        case 0: return 1; // gray
        case 2: return 3; // RGB
        case 3: return 1; // palette index
        case 4: return 2; // gray + alpha
        case 6: return 4; // RGBA
    }
    return 0;
}

int paethPredictor(int a, int b, int c) {
    const int p = a + b - c;
    const int pa = std::abs(p - a);
    const int pb = std::abs(p - b);
    const int pc = std::abs(p - c);
    if (pa <= pb && pa <= pc) {
        return a;
    }
    if (pb <= pc) {
        return b;
    }
    return c;
}

// Reverse PNG row filters in place. `data` holds height rows of
// (1 + stride) bytes; channels is samples per pixel, bpp is the
// bytes-per-sample (1 or 2).
// Returns false on an invalid filter byte (corrupt data).
bool unfilter(std::vector<unsigned char>& data, int width, int height,
              int channels, int bpp) {
    const int stride = width * channels * bpp;
    // PNG filter offsets are in "complete sample" units (all channels of
    // one pixel), not single-component units.
    const int sample = channels * bpp;
    for (int y = 0; y < height; ++y) {
        unsigned char* row = data.data() + static_cast<std::size_t>(y) * (stride + 1);
        const unsigned char filter = row[0];
        unsigned char* cur = row + 1;
        const unsigned char* above =
            y > 0 ? row - (stride + 1) + 1 : nullptr;
        for (int x = 0; x < stride; ++x) {
            const int a = (x >= sample) ? static_cast<int>(cur[x - sample]) : 0;
            const int b = above ? static_cast<int>(above[x]) : 0;
            const int c =
                (above && x >= sample) ? static_cast<int>(above[x - sample]) : 0;
            switch (filter) {
                case 0: break; // None
                case 1: cur[x] = static_cast<unsigned char>(cur[x] + a); break;
                case 2: cur[x] = static_cast<unsigned char>(cur[x] + b); break;
                case 3: cur[x] = static_cast<unsigned char>(cur[x] + (a + b) / 2);
                        break;
                case 4: cur[x] = static_cast<unsigned char>(cur[x] +
                                                            paethPredictor(a, b, c));
                        break;
                default:
                    return false; // invalid filter byte
            }
        }
    }
    return true;
}

} // namespace

// ---------------------------------------------------------------------------
// Encoder (8-bit RGBA, filter None)
// ---------------------------------------------------------------------------

namespace {

std::string makePngChunk(const char* type, const unsigned char* data,
                         std::size_t len) {
    std::string out;
    const std::uint32_t l = static_cast<std::uint32_t>(len);
    out += static_cast<char>((l >> 24) & 0xFF);
    out += static_cast<char>((l >> 16) & 0xFF);
    out += static_cast<char>((l >> 8) & 0xFF);
    out += static_cast<char>(l & 0xFF);
    out += type;
    out.append(reinterpret_cast<const char*>(data), len);
    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, reinterpret_cast<const Bytef*>(type), 4);
    crc = crc32(crc, data, len);
    out += static_cast<char>((crc >> 24) & 0xFF);
    out += static_cast<char>((crc >> 16) & 0xFF);
    out += static_cast<char>((crc >> 8) & 0xFF);
    out += static_cast<char>(crc & 0xFF);
    return out;
}

} // namespace

Result<std::string> Png::encode(const PngImage& image) {
    if (image.width <= 0 || image.height <= 0 ||
        image.rgba.size() !=
            static_cast<std::size_t>(image.width) * image.height * 4) {
        return MC_FAIL("invalid image dimensions/size for PNG encoding");
    }

    std::string out("\x89PNG\r\n\x1a\n", 8);

    unsigned char ihdr[13];
    const auto put32 = [](unsigned char* p, std::uint32_t v) {
        p[0] = static_cast<unsigned char>((v >> 24) & 0xFF);
        p[1] = static_cast<unsigned char>((v >> 16) & 0xFF);
        p[2] = static_cast<unsigned char>((v >> 8) & 0xFF);
        p[3] = static_cast<unsigned char>(v & 0xFF);
    };
    put32(ihdr + 0, static_cast<std::uint32_t>(image.width));
    put32(ihdr + 4, static_cast<std::uint32_t>(image.height));
    ihdr[8] = 8;   // bit depth
    ihdr[9] = 6;   // color type: RGBA
    ihdr[10] = 0;  // compression
    ihdr[11] = 0;  // filter
    ihdr[12] = 0;  // interlace: none
    out += makePngChunk("IHDR", ihdr, 13);

    // Raw scanlines, each prefixed with filter byte 0 (None).
    const int stride = image.width * 4;
    std::string raw;
    raw.reserve(static_cast<std::size_t>(image.height) * (stride + 1));
    for (int y = 0; y < image.height; ++y) {
        raw.push_back('\0');
        raw.append(reinterpret_cast<const char*>(image.rgba.data()) +
                       static_cast<std::size_t>(y) * stride,
                   static_cast<std::size_t>(stride));
    }

    uLongf destLen = compressBound(static_cast<uLong>(raw.size()));
    std::vector<Bytef> compressed(destLen);
    const int zr = compress2(compressed.data(), &destLen,
                             reinterpret_cast<const Bytef*>(raw.data()),
                             static_cast<uLong>(raw.size()), Z_BEST_SPEED);
    if (zr != Z_OK) {
        return MC_FAIL("zlib compress failed during PNG encoding");
    }
    out += makePngChunk("IDAT", compressed.data(), destLen);
    out += makePngChunk("IEND", reinterpret_cast<const unsigned char*>(""), 0);
    return out;
}

Result<PngImage> Png::decode(const std::string& bytes) {
    if (bytes.size() < 8 || std::memcmp(bytes.data(), kSignature, 8) != 0) {
        return MC_FAIL("not a PNG file (bad signature)");
    }

    Ihdr ihdr;
    bool haveIhdr = false;
    bool haveIend = false;
    std::vector<unsigned char> palette; // 3 bytes per entry
    std::vector<unsigned char> idat;
    std::size_t pos = 8;

    while (pos + 8 <= bytes.size()) {
        const std::uint32_t len = readU32BE(reinterpret_cast<const unsigned char*>(bytes.data()) + pos);
        const char type[5] = {bytes[pos + 4], bytes[pos + 5], bytes[pos + 6],
                              bytes[pos + 7], 0};
        if (pos + 8 + len + 4 > bytes.size()) {
            return MC_FAIL(std::string("truncated PNG chunk '") + type + "'");
        }
        const unsigned char* data =
            reinterpret_cast<const unsigned char*>(bytes.data()) + pos + 8;

        // CRC covers type + data.
        uLong crc = crc32(0L, Z_NULL, 0);
        crc = crc32(crc, reinterpret_cast<const Bytef*>(type), 4);
        crc = crc32(crc, data, len);
        const std::uint32_t storedCrc =
            readU32BE(reinterpret_cast<const unsigned char*>(bytes.data()) + pos + 8 + len);
        if (crc != storedCrc) {
            return MC_FAIL(std::string("CRC mismatch in PNG chunk '") + type + "'");
        }

        if (std::memcmp(type, "IHDR", 4) == 0) {
            if (len != 13 || haveIhdr) {
                return MC_FAIL("invalid IHDR chunk");
            }
            ihdr.width = readU32BE(data);
            ihdr.height = readU32BE(data + 4);
            ihdr.bitDepth = data[8];
            ihdr.colorType = data[9];
            const int compression = data[10];
            const int filterMethod = data[11];
            ihdr.interlace = data[12];
            if (compression != 0 || filterMethod != 0) {
                return MC_FAIL("unsupported PNG compression/filter method");
            }
            if (ihdr.width == 0 || ihdr.height == 0) {
                return MC_FAIL("PNG has zero width or height");
            }
            if (ihdr.interlace != 0) {
                return MC_FAIL("interlaced (Adam7) PNGs are not supported");
            }
            // Fail fast on unsupported formats, before needing image data.
            if (channelsFor(ihdr.colorType) == 0) {
                return MC_FAIL("unsupported PNG color type " +
                               std::to_string(ihdr.colorType));
            }
            if (ihdr.colorType == 3 && ihdr.bitDepth != 8) {
                return MC_FAIL("palette PNGs with bit depth " +
                               std::to_string(ihdr.bitDepth) +
                               " are not supported (only 8)");
            }
            if (ihdr.colorType != 3 && (ihdr.bitDepth != 8 && ihdr.bitDepth != 16)) {
                return MC_FAIL("unsupported PNG bit depth " +
                               std::to_string(ihdr.bitDepth) +
                               " (only 8 and 16 are supported)");
            }
            haveIhdr = true;
        } else if (std::memcmp(type, "PLTE", 4) == 0) {
            if (len % 3 != 0 || len / 3 > 256) {
                return MC_FAIL("invalid PLTE chunk");
            }
            palette.assign(data, data + len);
        } else if (std::memcmp(type, "IDAT", 4) == 0) {
            idat.insert(idat.end(), data, data + len);
        } else if (std::memcmp(type, "IEND", 4) == 0) {
            haveIend = true;
        }
        // Other chunks (gAMA, tEXt, ...) are ignored.

        pos += 8 + len + 4;
        if (haveIend) {
            break;
        }
    }

    if (!haveIhdr) {
        return MC_FAIL("PNG missing IHDR chunk");
    }
    if (!haveIend) {
        return MC_FAIL("PNG missing IEND chunk (truncated file?)");
    }
    if (idat.empty()) {
        return MC_FAIL("PNG missing IDAT chunk");
    }

    // Format validation (color type / bit depth) already happened in the
    // IHDR handler above.
    const int channels = channelsFor(ihdr.colorType);
    const bool sixteenBit = (ihdr.bitDepth == 16);

    // -- inflate -------------------------------------------------------------
    const int width = static_cast<int>(ihdr.width);
    const int height = static_cast<int>(ihdr.height);
    const int bpp = sixteenBit ? 2 : 1;
    const int stride = width * channels * bpp;
    std::vector<unsigned char> raw(static_cast<std::size_t>(height) * (stride + 1));

    z_stream strm {};
    if (inflateInit(&strm) != Z_OK) {
        return MC_FAIL("zlib inflateInit failed");
    }
    strm.next_in = reinterpret_cast<Bytef*>(idat.data());
    strm.avail_in = static_cast<uInt>(idat.size());
    strm.next_out = raw.data();
    strm.avail_out = static_cast<uInt>(raw.size());
    const int zr = inflate(&strm, Z_FINISH);
    inflateEnd(&strm);
    if (zr != Z_STREAM_END || strm.total_out != raw.size()) {
        return MC_FAIL("zlib inflate failed (corrupt or truncated IDAT data)");
    }

    // -- unfilter ------------------------------------------------------------
    if (!unfilter(raw, width, height, channels, bpp)) {
        return MC_FAIL("invalid PNG row filter (corrupt data)");
    }

    // -- convert to 8-bit RGBA ------------------------------------------------
    PngImage image;
    image.width = width;
    image.height = height;
    image.rgba.resize(static_cast<std::size_t>(width) * height * 4);

    // Both bit depths store the most significant byte first (16-bit is
    // big-endian in PNG; 8-bit has only one byte), so the 8-bit sample is
    // always the first byte of the sample.
    const auto sample8 = [](const unsigned char* p) -> unsigned char {
        return p[0];
    };

    for (int y = 0; y < height; ++y) {
        const unsigned char* row =
            raw.data() + static_cast<std::size_t>(y) * (stride + 1) + 1;
        for (int x = 0; x < width; ++x) {
            const unsigned char* p = row + x * channels * bpp;
            unsigned char r = 0, g = 0, b = 0, a = 255;
            switch (ihdr.colorType) {
                case 0: { // gray
                    const unsigned char v = sample8(p);
                    r = g = b = v;
                    break;
                }
                case 2: { // RGB
                    r = sample8(p);
                    g = sample8(p + 1 * bpp);
                    b = sample8(p + 2 * bpp);
                    break;
                }
                case 3: { // palette
                    const unsigned char idx = p[0];
                    if (static_cast<std::size_t>(idx) * 3 + 2 >= palette.size()) {
                        return MC_FAIL("palette index out of range");
                    }
                    r = palette[idx * 3];
                    g = palette[idx * 3 + 1];
                    b = palette[idx * 3 + 2];
                    break;
                }
                case 4: { // gray + alpha
                    const unsigned char v = sample8(p);
                    r = g = b = v;
                    a = sample8(p + 1 * bpp);
                    break;
                }
                case 6: { // RGBA
                    r = sample8(p);
                    g = sample8(p + 1 * bpp);
                    b = sample8(p + 2 * bpp);
                    a = sample8(p + 3 * bpp);
                    break;
                }
            }
            unsigned char* dst =
                image.rgba.data() + (static_cast<std::size_t>(y) * width + x) * 4;
            dst[0] = r;
            dst[1] = g;
            dst[2] = b;
            dst[3] = a;
        }
    }

    return image;
}

} // namespace mc
