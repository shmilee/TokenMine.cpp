// V3-7 (format 29): the save payload's on-disk gzip encoding — the
// deterministic container (the hand-pinned kGzipHeader + raw deflate +
// CRC32/ISIZE trailer: byte-reproducible output, the determinism
// first-class rule), the strict decode (the header pinned
// byte-for-byte, the body inflated, the trailer double-checked — every
// corruption class named, carrying the file path), the magic probe
// (the sole encoding criterion — legacy plaintext reads straight
// through, zero migration), the save.compression strict config read,
// and the none-bit-identical identity (gunzip(gzip file) ≡ the
// same-state plaintext file, byte-for-byte).
#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "block/block_registry.hpp"
#include "core/config.hpp"
#include "core/json.hpp"
#include "core/save.hpp"
#include "core/status_effect.hpp"
#include "item/inventory.hpp"
#include "item/item_registry.hpp"
#include "player/drown.hpp"

using namespace mc;

namespace {

Json parseJson(const std::string& text) {
    auto r = Json::parse(text);
    if (!r.ok()) {
        ADD_FAILURE() << "parse failed: " << r.error().message;
        return Json::object();
    }
    return *r;
}

// stone + dirt (the block edits the fixture save carries).
BlockRegistry makeBlocks() {
    BlockRegistry blocks;
    blocks.registerBlock(parseJson(
        R"({"id":"stone","model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    blocks.registerBlock(parseJson(
        R"({"id":"dirt","model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    return blocks;
}

// A fixture whose save serializes AND parses (the identity test
// re-parses the simulated format-28 file, so every ALWAYS-written
// format-28 field carries a real value, not a sentinel): the
// oxygen / xp / respawn sentinels are replaced with the game's real
// range values.
struct Fixture {
    BlockRegistry blocks = makeBlocks();
    ItemRegistry items;
    EffectRegistry effects; // empty: the fixture save carries no effects

    Fixture() {
        auto r = items.buildBlockItems(blocks);
        if (!r.ok()) {
            ADD_FAILURE() << r.error().message;
        }
    }
};

SaveGame makeSave(const Fixture& f) {
    SaveGame s;
    s.seed = 42;
    s.time = 0.123456789;
    s.playerPos = {1.5f, 53.25f, -2.75f};
    s.playerVelocity = {0.0f, 0.0f, 0.0f};
    s.playerYaw = 0.785f;
    s.playerPitch = -0.4f;
    s.playerHealth = 20.0f;
    s.playerAbsorption = 0.0f;
    s.playerHunger = 20.0f;
    s.playerSaturation = 0.0f;
    s.playerOxygen = kOxygenMax;
    s.playerXp = 0;
    s.playerXpSeed = 0;
    // A bed-form respawn (the cell's center — the floor-reconstruction
    // round trip is exact, y in [0, kWorldHeight)).
    s.playerRespawn = {3.5f, 54.5f, -7.5f};
    s.selectedSlot = 0;
    s.slots.resize(Inventory::kTotalSize);
    s.slots[0] = ItemStack {f.items.idForName("stone"), 64};
    s.blockEdits = {{BlockPos {0, 54, -1}, "dirt"}};
    return s;
}

// The real format-29 payload (serializeSave's dump + the trailing
// newline — exactly what writeSave hands to the encoder).
std::string realPayload(const Fixture& f) {
    auto j = serializeSave(makeSave(f), f.items);
    if (!j.ok()) {
        ADD_FAILURE() << j.error().message;
        return std::string();
    }
    return j->dump(2) + '\n';
}

std::string kGzipHeaderString() {
    return std::string(reinterpret_cast<const char*>(kGzipHeader.data()),
                       kGzipHeader.size());
}

// A ~1 KB synthetic payload (50 varied lines — exactly 1140 bytes).
std::string oneKiloPayload() {
    std::string s;
    for (int i = 0; i < 50; ++i) {
        s += "0123456789abcdefghij" + std::to_string(i) + "\n";
    }
    return s;
}

// A ~100 KB synthetic payload (2100 JSON-ish lines — the save's
// journal-table compression class).
std::string oneHundredKiloPayload() {
    std::string s;
    for (int i = 0; i < 2100; ++i) {
        s += "{\"cell\":" + std::to_string(i) +
             ",\"tag\":\"abcdefghijklmnopqrstuvwxyz\"},\n";
    }
    return s;
}

// The container's body length (raw deflate between the 10-byte header
// and the 8-byte trailer).
std::size_t bodyLengthOf(const std::string& container) {
    return container.size() - kGzipHeader.size() - 8;
}

std::size_t countOccurrences(const std::string& hay, const std::string& needle) {
    std::size_t count = 0;
    std::size_t pos = 0;
    while ((pos = hay.find(needle, pos)) != std::string::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

// A copy of a JSON object minus one key (Json has no erase — the
// test_save.cpp withoutKey twin).
Json withoutKey(const Json& root, const std::string& key) {
    Json out = Json::object();
    for (const auto& [k, v] : root.members()) {
        if (k != key) {
            out[k] = v;
        }
    }
    return out;
}

} // namespace

// -- 1. round trip ---------------------------------------------------------------

TEST(GzipRoundTripTest, ThirtyTwoBytesRoundTrip) {
    const std::string payload(32, 'A');
    auto gz = gzipEncode(payload);
    ASSERT_TRUE(gz.ok()) << gz.error().message;
    auto back = gzipDecode(*gz, "roundtrip32");
    ASSERT_TRUE(back.ok()) << back.error().message;
    EXPECT_EQ(*back, payload);
}

TEST(GzipRoundTripTest, OneKiloByteRoundTrip) {
    const std::string payload = oneKiloPayload();
    ASSERT_EQ(payload.size(), 1140u); // the ~1 KB synthetic's exact size
    auto gz = gzipEncode(payload);
    ASSERT_TRUE(gz.ok()) << gz.error().message;
    auto back = gzipDecode(*gz, "roundtrip1k");
    ASSERT_TRUE(back.ok()) << back.error().message;
    EXPECT_EQ(*back, payload);
}

TEST(GzipRoundTripTest, OneHundredKiloByteRoundTrip) {
    const std::string payload = oneHundredKiloPayload();
    ASSERT_GE(payload.size(), 100000u); // the ~100 KB synthetic's floor
    ASSERT_LE(payload.size(), 110000u);
    auto gz = gzipEncode(payload);
    ASSERT_TRUE(gz.ok()) << gz.error().message;
    auto back = gzipDecode(*gz, "roundtrip100k");
    ASSERT_TRUE(back.ok()) << back.error().message;
    EXPECT_EQ(*back, payload);
}

TEST(GzipRoundTripTest, RealSaveFixtureRoundTrip) {
    const Fixture f;
    const std::string payload = realPayload(f);
    ASSERT_FALSE(payload.empty());
    // The plaintext (none-mode) file starts with '{' — the probe must
    // route it to the legacy path.
    EXPECT_FALSE(looksLikeGzip(payload));
    auto gz = gzipEncode(payload);
    ASSERT_TRUE(gz.ok()) << gz.error().message;
    EXPECT_TRUE(looksLikeGzip(*gz));
    auto back = gzipDecode(*gz, "roundtripsave");
    ASSERT_TRUE(back.ok()) << back.error().message;
    // The §1 identity: gunzip(gzip file) ≡ the same-state plaintext
    // file, byte-for-byte.
    EXPECT_EQ(*back, payload);
}

// -- 2. determinism pin ----------------------------------------------------------

TEST(GzipDeterminismTest, HeaderBytesPinned) {
    const std::string payload = oneKiloPayload();
    auto gz = gzipEncode(payload);
    ASSERT_TRUE(gz.ok()) << gz.error().message;
    // The first 10 bytes equal kGzipHeader byte-for-byte (the pinned
    // container header: magic + CM + FLG=0 + MTIME=0 + XFL=0 + OS=3).
    for (std::size_t i = 0; i < kGzipHeader.size(); ++i) {
        EXPECT_EQ(static_cast<std::uint8_t>((*gz)[i]), kGzipHeader[i])
            << "header byte " << i;
    }
    // The trailer is the CRC32/ISIZE pair (8 bytes) after the body.
    EXPECT_GE(gz->size(), kGzipHeader.size() + 2 + 8);
}

TEST(GzipDeterminismTest, SameInputYieldsIdenticalOutput) {
    const std::string payload = oneKiloPayload();
    auto a = gzipEncode(payload);
    auto b = gzipEncode(payload);
    ASSERT_TRUE(a.ok()) << a.error().message;
    ASSERT_TRUE(b.ok()) << b.error().message;
    EXPECT_EQ(*a, *b); // byte-for-byte (the determinism first-class rule)
}

TEST(GzipDeterminismTest, DifferentInputsYieldDifferentOutputs) {
    auto a = gzipEncode(oneKiloPayload());
    auto b = gzipEncode(std::string(1140, 'Z')); // same size, other bytes
    ASSERT_TRUE(a.ok()) << a.error().message;
    ASSERT_TRUE(b.ok()) << b.error().message;
    EXPECT_NE(*a, *b);
}

TEST(GzipDeterminismTest, EmptyPayloadIsTheMinContainer) {
    // The minimum container: the 10-byte header + the 2-byte empty
    // raw-deflate body (a final stored block, zero length) + the
    // 8-byte trailer (CRC32 of the empty payload = 0, ISIZE = 0).
    auto gz = gzipEncode(std::string());
    ASSERT_TRUE(gz.ok()) << gz.error().message;
    const std::string expected =
        kGzipHeaderString() + std::string("\x03\x00", 2) +
        std::string(8, '\0');
    ASSERT_EQ(expected.size(), 20u);
    EXPECT_EQ(*gz, expected);
    auto back = gzipDecode(*gz, "empty");
    ASSERT_TRUE(back.ok()) << back.error().message;
    EXPECT_TRUE(back->empty());
}

// -- 3. the magic probe ------------------------------------------------------------

TEST(GzipMagicTest, ProbeRecognizesOnlyTheGzipMagic) {
    // 1f 8b + anything = true...
    EXPECT_TRUE(looksLikeGzip(std::string("\x1f\x8b", 2) + "rest"));
    // ...every other leading-byte pair = false.
    EXPECT_FALSE(looksLikeGzip(std::string("\x1f\x00", 2) + "rest"));
    EXPECT_FALSE(looksLikeGzip(std::string("\x00\x8b", 2) + "rest"));
    EXPECT_FALSE(looksLikeGzip("{\n  \"format\""));
    EXPECT_FALSE(looksLikeGzip(std::string("\x1f", 1))); // one byte
    EXPECT_FALSE(looksLikeGzip(std::string())); // empty
}

TEST(GzipMagicTest, LegacyPlaintextSaveIsNotGzip) {
    const Fixture f;
    const std::string payload = realPayload(f);
    ASSERT_FALSE(payload.empty());
    // A real plaintext save's first two bytes are '{' + '\n' — the
    // legacy (format 28 and older) read path, zero migration.
    EXPECT_EQ(payload[0], '{');
    EXPECT_EQ(payload[1], '\n');
    EXPECT_FALSE(looksLikeGzip(payload));
}

// -- 4. the strict decode's named rejections ----------------------------------------

TEST(GzipRejectTest, NonJsonContentDecodesButJsonParseFails) {
    const std::string payload = "this is not JSON, it is just text";
    auto gz = gzipEncode(payload);
    ASSERT_TRUE(gz.ok()) << gz.error().message;
    // The gzip layer decodes it fine (the encoding is content-agnostic)...
    auto back = gzipDecode(*gz, "nonjson");
    ASSERT_TRUE(back.ok()) << back.error().message;
    EXPECT_EQ(*back, payload);
    // ...and the JSON layer names it (the readSaveFile path's existing
    // "invalid JSON in save file" rejection, unchanged).
    auto parsed = Json::parse(*back);
    EXPECT_FALSE(parsed.ok());
}

TEST(GzipRejectTest, TruncatedContainersAreNamed) {
    const std::string what = "truncated-slot.json";
    auto gz = gzipEncode(oneKiloPayload());
    ASSERT_TRUE(gz.ok()) << gz.error().message;
    const std::string valid = *gz;
    // 1) header only (10 bytes < the 20-byte minimum).
    auto r1 = gzipDecode(kGzipHeaderString(), what);
    ASSERT_FALSE(r1.ok());
    EXPECT_NE(r1.error().message.find("truncated gzip"), std::string::npos)
        << r1.error().message;
    EXPECT_NE(r1.error().message.find(what), std::string::npos);
    // 2) 19 bytes (one under the minimum).
    auto r2 = gzipDecode(kGzipHeaderString() + std::string(9, '\0'), what);
    ASSERT_FALSE(r2.ok());
    EXPECT_NE(r2.error().message.find("truncated gzip"), std::string::npos)
        << r2.error().message;
    // 3) header + half the body (no trailer — the stream ends before
    //    its final block).
    const std::size_t half = kGzipHeader.size() + bodyLengthOf(valid) / 2;
    auto r3 = gzipDecode(valid.substr(0, half), what);
    ASSERT_FALSE(r3.ok());
    EXPECT_NE(r3.error().message.find("corrupt gzip body"), std::string::npos)
        << r3.error().message;
    EXPECT_NE(r3.error().message.find(what), std::string::npos);
}

TEST(GzipRejectTest, HeaderDeviationsAreNamed) {
    const std::string what = "header-slot.json";
    auto gz = gzipEncode(oneKiloPayload());
    ASSERT_TRUE(gz.ok()) << gz.error().message;
    std::string valid = *gz;
    const std::array<int, 4> deviations = {2, 3, 8, 9};
    const std::array<std::uint8_t, 4> values = {0x01, 0x04, 0x02, 0xFF};
    // byte 2: CM != deflate; byte 3: FLG != 0; byte 8: XFL != 0;
    // byte 9: OS != Unix (the system gzip CLI's foreign header).
    for (std::size_t i = 0; i < deviations.size(); ++i) {
        std::string deviated = valid;
        deviated[static_cast<std::size_t>(deviations[i])] =
            static_cast<char>(values[i]);
        auto r = gzipDecode(deviated, what);
        ASSERT_FALSE(r.ok()) << "byte " << deviations[i];
        EXPECT_NE(r.error().message.find("unsupported gzip header"),
                  std::string::npos)
            << r.error().message;
        EXPECT_NE(r.error().message.find(what), std::string::npos);
    }
}

TEST(GzipRejectTest, TrailerMismatchesAreNamed) {
    const std::string what = "trailer-slot.json";
    auto gz = gzipEncode(oneKiloPayload());
    ASSERT_TRUE(gz.ok()) << gz.error().message;
    std::string valid = *gz;
    // 1) a CRC32 bit flip (the last 4 bytes hold the CRC, little-endian
    //    — the most significant byte of the field is second from the
    //    end... flip the trailer's FIRST byte = the CRC's LSB).
    std::string crcFlipped = valid;
    crcFlipped[crcFlipped.size() - 8] ^= 0x01;
    auto r1 = gzipDecode(crcFlipped, what);
    ASSERT_FALSE(r1.ok());
    EXPECT_NE(r1.error().message.find("corrupt gzip trailer"),
              std::string::npos)
        << r1.error().message;
    EXPECT_NE(r1.error().message.find("CRC32"), std::string::npos);
    EXPECT_NE(r1.error().message.find(what), std::string::npos);
    // 2) an ISIZE bit flip (the trailer's LAST byte = the size's LSB).
    std::string isizeFlipped = valid;
    isizeFlipped[isizeFlipped.size() - 1] ^= 0x01;
    auto r2 = gzipDecode(isizeFlipped, what);
    ASSERT_FALSE(r2.ok());
    EXPECT_NE(r2.error().message.find("corrupt gzip trailer"),
              std::string::npos)
        << r2.error().message;
    EXPECT_NE(r2.error().message.find("ISIZE"), std::string::npos);
    EXPECT_NE(r2.error().message.find(what), std::string::npos);
}

// -- 5. the save.compression strict config read -------------------------------------

TEST(SaveCompressionConfigTest, AbsentDefaultsToGzip) {
    Config config; // no save section (the committed config's shape)
    auto r = saveCompression(config);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(*r, SaveCompression::Gzip);
}

TEST(SaveCompressionConfigTest, GzipAndNoneWordsAccepted) {
    Config gzipConfig;
    gzipConfig.set("save.compression", Json(std::string("gzip")));
    auto rg = saveCompression(gzipConfig);
    ASSERT_TRUE(rg.ok()) << rg.error().message;
    EXPECT_EQ(*rg, SaveCompression::Gzip);
    Config noneConfig;
    noneConfig.set("save.compression", Json(std::string("none")));
    auto rn = saveCompression(noneConfig);
    ASSERT_TRUE(rn.ok()) << rn.error().message;
    EXPECT_EQ(*rn, SaveCompression::None);
}

TEST(SaveCompressionConfigTest, NonStringValuesAreNamed) {
    Config boolConfig;
    boolConfig.set("save.compression", Json(true));
    auto rb = saveCompression(boolConfig);
    ASSERT_FALSE(rb.ok());
    EXPECT_NE(rb.error().message.find("save.compression"), std::string::npos)
        << rb.error().message;
    Config numberConfig;
    numberConfig.set("save.compression", Json(1));
    auto rn = saveCompression(numberConfig);
    ASSERT_FALSE(rn.ok());
    EXPECT_NE(rn.error().message.find("save.compression"), std::string::npos)
        << rn.error().message;
}

TEST(SaveCompressionConfigTest, UnknownOrEmptyWordsAreNamed) {
    Config lzmaConfig;
    lzmaConfig.set("save.compression", Json(std::string("lzma")));
    auto rl = saveCompression(lzmaConfig);
    ASSERT_FALSE(rl.ok());
    EXPECT_NE(rl.error().message.find("save.compression"), std::string::npos)
        << rl.error().message;
    EXPECT_NE(rl.error().message.find("lzma"), std::string::npos);
    Config emptyConfig;
    emptyConfig.set("save.compression", Json(std::string("")));
    auto re = saveCompression(emptyConfig);
    ASSERT_FALSE(re.ok());
    EXPECT_NE(re.error().message.find("save.compression"), std::string::npos)
        << re.error().message;
}

// -- 6. the 29 -> 31 schema delta (V3-11a re-based the V3-7 identity:
//        format 29 and 31 are NO LONGER schema-identical — 30 adds the
//        dimension key + the nether segments, 31 adds player.creative.
//        The delta of the same state is EXACTLY the whitelisted keys
//        {format, player.dimension, nether, nether_entities,
//        player.creative} — a simulated v3.3.0 (format-29) file
//        re-parses to the same state (the overworld + survival
//        migrations), and re-serializing returns to the format-31
//        payload byte-for-byte) ---------------------------------

TEST(GzipNoneIdentityTest,
     TwentyNineToThirtyOneDeltaIsTheWhitelist) {
    const Fixture f;
    const SaveGame s = makeSave(f);
    auto j31 = serializeSave(s, f.items);
    ASSERT_TRUE(j31.ok()) << j31.error().message;
    const std::string p31 = j31->dump(2) + '\n';
    // The format marker appears exactly ONCE in the whole file (the
    // root key — the "nether" segment's keys are blocks / fluids /
    // states, no nested "format" string).
    EXPECT_EQ(countOccurrences(p31, "\"format\""), 1u);
    EXPECT_EQ(countOccurrences(p31, "\"format\": 31"), 1u);
    // The format-30/31 keys are present (the ALWAYS-written discipline).
    EXPECT_EQ(countOccurrences(p31, "\"dimension\""), 1u);
    EXPECT_EQ(countOccurrences(p31, "\"nether\""), 1u);
    EXPECT_EQ(countOccurrences(p31, "\"nether_entities\""), 1u);
    EXPECT_EQ(countOccurrences(p31, "\"creative\""), 1u);
    // The simulated v3.3.0 (format-29) file of the same state: the
    // whitelisted keys stripped + the format key flipped.
    Json j = *j31;
    Json nether = withoutKey(j, "nether");
    nether = withoutKey(nether, "nether_entities");
    const Json* player = nether.find("player");
    ASSERT_TRUE(player != nullptr && player->isObject());
    Json p = withoutKey(*player, "dimension");
    p = withoutKey(p, "creative");
    nether = withoutKey(nether, "player");
    nether["player"] = std::move(p);
    nether["format"] = Json(29);
    const std::string p29 = nether.dump(2) + '\n';
    ASSERT_NE(p29, p31);
    EXPECT_EQ(countOccurrences(p29, "\"format\": 29"), 1u);
    // The format-29 file parses to the SAME state (the migration:
    // dimension -> overworld, creative -> false, the nether segments
    // -> empty), and re-serializing it returns to the format-31
    // payload with every other key unchanged — the current-none file
    // ≡ the v3.3.0 file except the whitelisted keys.
    auto jp29 = Json::parse(p29);
    ASSERT_TRUE(jp29.ok()) << jp29.error().message;
    auto sp29 = parseSave(*jp29, f.items, f.blocks, f.effects, s.seed);
    ASSERT_TRUE(sp29.ok()) << sp29.error().message;
    EXPECT_EQ(sp29->playerDimension, "overworld");
    EXPECT_FALSE(sp29->playerCreative);
    auto j29out = serializeSave(*sp29, f.items);
    ASSERT_TRUE(j29out.ok()) << j29out.error().message;
    EXPECT_EQ(j29out->dump(2) + '\n', p31);
}
