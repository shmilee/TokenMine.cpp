#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "core/png.hpp"

#include <zlib.h>

using namespace mc;

namespace {

// Build a valid PNG chunk (length + type + data + CRC) for synthetic tests.
std::string makeChunk(const char* type, const unsigned char* data,
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

std::string readAll(const std::string& name) {
    const std::filesystem::path p(MC_TEST_DATA_DIR "/" + name);
    std::ifstream in(p, std::ios::binary);
    EXPECT_TRUE(in) << "missing fixture: " << p.string();
    return std::string((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
}

// Compare against expected RGBA bytes (row-major).
void expectPixels(const PngImage& img, const std::vector<int>& expected) {
    ASSERT_EQ(img.rgba.size(), expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        EXPECT_EQ(static_cast<int>(img.rgba[i]), expected[i])
            << "byte offset " << i;
    }
}

} // namespace

TEST(PngTest, Rgba8) {
    auto img = Png::decode(readAll("rgba8_2x2.png"));
    ASSERT_TRUE(img.ok()) << img.error().describe();
    EXPECT_EQ(img->width, 2);
    EXPECT_EQ(img->height, 2);
    expectPixels(*img, {
        255, 0, 0, 255,   //
        0, 255, 0, 128,   //
        0, 0, 255, 64,    //
        255, 255, 0, 32,
    });
}

TEST(PngTest, Rgb8) {
    auto img = Png::decode(readAll("rgb8_2x2.png"));
    ASSERT_TRUE(img.ok()) << img.error().describe();
    // Alpha must be filled with opaque.
    expectPixels(*img, {
        10, 20, 30, 255,  //
        40, 50, 60, 255,  //
        70, 80, 90, 255,  //
        100, 110, 120, 255,
    });
}

TEST(PngTest, Palette8) {
    auto img = Png::decode(readAll("palette8_2x2.png"));
    ASSERT_TRUE(img.ok()) << img.error().describe();
    // palette = [(1,2,3), (4,5,6)], pixels = [0, 1, 1, 0]
    expectPixels(*img, {
        1, 2, 3, 255, //
        4, 5, 6, 255, //
        4, 5, 6, 255, //
        1, 2, 3, 255,
    });
}

TEST(PngTest, Gray8) {
    auto img = Png::decode(readAll("gray8_2x2.png"));
    ASSERT_TRUE(img.ok()) << img.error().describe();
    expectPixels(*img, {
        10, 10, 10, 255, //
        20, 20, 20, 255, //
        30, 30, 30, 255, //
        200, 200, 200, 255,
    });
}

TEST(PngTest, GrayAlpha8) {
    auto img = Png::decode(readAll("grayalpha8_2x2.png"));
    ASSERT_TRUE(img.ok()) << img.error().describe();
    expectPixels(*img, {
        10, 10, 10, 255, //
        20, 20, 20, 128, //
        30, 30, 30, 64,  //
        200, 200, 200, 32,
    });
}

TEST(PngTest, Rgba16DownconvertsHighByte) {
    auto img = Png::decode(readAll("rgba16_2x2.png"));
    ASSERT_TRUE(img.ok()) << img.error().describe();
    // 0x1234 -> 0x12 (18), 0x5678 -> 0x56 (86), 0x9ABC -> 0x9A (154),
    // 0xFF00 -> 0xFF (255)
    expectPixels(*img, {
        18, 86, 154, 255, //
        0, 1, 2, 3,       //
        17, 34, 51, 68,   //
        85, 102, 119, 136,
    });
}

TEST(PngTest, AllRowFilters) {
    // 4x4 RGBA8 where row y used filter y%5. Expected pixels come from the
    // generator's ground-truth dump (cross-checked with ImageMagick).
    const std::filesystem::path expectedPath(
        std::string(MC_TEST_DATA_DIR) + "/filters4x4_expected.txt");
    std::ifstream in(expectedPath);
    ASSERT_TRUE(in) << "missing fixture: " << expectedPath.string();

    std::vector<int> expected;
    int value = 0;
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ss(line);
        while (ss >> value) {
            expected.push_back(value);
        }
    }
    ASSERT_EQ(expected.size(), 4u * 4u * 4u);

    auto img = Png::decode(readAll("filters4x4.png"));
    ASSERT_TRUE(img.ok()) << img.error().describe();
    expectPixels(*img, expected);
}

TEST(PngTest, RejectsNotAPng) {
    const auto img = Png::decode(readAll("notapng.png"));
    ASSERT_FALSE(img.ok());
    EXPECT_NE(img.error().message.find("signature"), std::string::npos);
}

TEST(PngTest, RejectsTruncated) {
    const auto img = Png::decode(readAll("truncated.png"));
    ASSERT_FALSE(img.ok());
    EXPECT_NE(img.error().message.find("truncated"), std::string::npos);
}

TEST(PngTest, RejectsCrcMismatch) {
    const auto img = Png::decode(readAll("crc_corrupt.png"));
    ASSERT_FALSE(img.ok());
    EXPECT_NE(img.error().message.find("CRC"), std::string::npos);
}

TEST(PngTest, EncodeDecodeRoundTrip) {
    PngImage image;
    image.width = 4;
    image.height = 3;
    image.rgba.resize(4 * 3 * 4);
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 4; ++x) {
            std::uint8_t* p =
                image.rgba.data() + (y * 4 + x) * 4;
            p[0] = static_cast<std::uint8_t>(x * 60);
            p[1] = static_cast<std::uint8_t>(y * 80);
            p[2] = static_cast<std::uint8_t>((x + y) * 40);
            p[3] = static_cast<std::uint8_t>(255 - (x * 31 + y * 17) % 256);
        }
    }

    auto encoded = Png::encode(image);
    ASSERT_TRUE(encoded.ok()) << encoded.error().describe();
    // A valid encoder must produce something a standard viewer accepts;
    // the strongest check is that our decoder (and its verified fixtures)
    // round-trips it exactly.
    auto decoded = Png::decode(*encoded);
    ASSERT_TRUE(decoded.ok()) << decoded.error().describe();
    EXPECT_EQ(decoded->width, 4);
    EXPECT_EQ(decoded->height, 3);
    EXPECT_EQ(decoded->rgba, image.rgba);
}

TEST(PngTest, EncodeRejectsInvalid) {
    PngImage empty; // zero size
    EXPECT_FALSE(Png::encode(empty).ok());

    PngImage mismatched;
    mismatched.width = 2;
    mismatched.height = 2;
    mismatched.rgba.assign(8, 0); // 2x2 RGBA needs 16 bytes
    EXPECT_FALSE(Png::encode(mismatched).ok());
}

TEST(PngTest, RejectsInterlaced) {
    // Valid signature + CRC-correct IHDR with interlace=1.
    const unsigned char ihdr[13] = {
        0, 0, 0, 8, // width 8
        0, 0, 0, 8, // height 8
        8,  2,      // bit depth 8, color type 2 (RGB)
        0,  0,      // compression 0, filter 0
        1,          // interlace: Adam7
    };
    std::string png("\x89PNG\r\n\x1a\n", 8);
    png += makeChunk("IHDR", ihdr, sizeof(ihdr));

    const auto img = Png::decode(png);
    ASSERT_FALSE(img.ok());
    EXPECT_NE(img.error().message.find("interlaced"), std::string::npos);
}

TEST(PngTest, RejectsUnsupportedBitDepth) {
    const unsigned char ihdr[13] = {
        0, 0, 0, 8, //
        0, 0, 0, 8, //
        4,  2,      // bit depth 4, color type 2
        0,  0,      // compression, filter
        0,          // interlace
    };
    std::string png("\x89PNG\r\n\x1a\n", 8);
    png += makeChunk("IHDR", ihdr, sizeof(ihdr));

    const auto img = Png::decode(png);
    ASSERT_FALSE(img.ok());
    EXPECT_NE(img.error().message.find("bit depth"), std::string::npos);
}
