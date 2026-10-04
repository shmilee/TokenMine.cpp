// Phase 15 tests: the WAV parser and gain scaling (the pure part of the
// SoundBank — no SDL device involved).
#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "core/sound.hpp"

using namespace mc;

namespace {

void pushU32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    out.push_back(v & 0xFF);
    out.push_back((v >> 8) & 0xFF);
    out.push_back((v >> 16) & 0xFF);
    out.push_back((v >> 24) & 0xFF);
}

void pushU16(std::vector<std::uint8_t>& out, std::uint16_t v) {
    out.push_back(v & 0xFF);
    out.push_back((v >> 8) & 0xFF);
}

void pushTag(std::vector<std::uint8_t>& out, const char* tag) {
    out.insert(out.end(), tag, tag + 4);
}

// A minimal RIFF/WAVE file with the requested (possibly invalid) fmt
// fields, `pcm` as the data payload, and an optional unknown "LIST" chunk
// in between.
std::vector<std::uint8_t> makeWav(const std::vector<std::int16_t>& pcm,
                                  int rate = 44100, int channels = 1,
                                  int bits = 16, std::uint16_t format = 1,
                                  bool withListChunk = false,
                                  bool withDataChunk = true) {
    std::vector<std::uint8_t> out;
    pushTag(out, "RIFF");
    pushU32(out, 36 + pcm.size() * 2 + (withListChunk ? 12 : 0));
    pushTag(out, "WAVE");
    pushTag(out, "fmt ");
    pushU32(out, 16);
    pushU16(out, format);
    pushU16(out, static_cast<std::uint16_t>(channels));
    pushU32(out, static_cast<std::uint32_t>(rate));
    pushU32(out, static_cast<std::uint32_t>(rate * channels * bits / 8));
    pushU16(out, static_cast<std::uint16_t>(channels * bits / 8));
    pushU16(out, static_cast<std::uint16_t>(bits));
    if (withListChunk) {
        pushTag(out, "LIST");
        pushU32(out, 4);
        pushTag(out, "INFO");
    }
    if (withDataChunk) {
        pushTag(out, "data");
        pushU32(out, static_cast<std::uint32_t>(pcm.size() * 2));
        for (const std::int16_t s : pcm) {
            pushU16(out, static_cast<std::uint16_t>(s));
        }
    }
    return out;
}

const std::vector<std::int16_t> kSamples = {0, 100, -100, 32767, -32768};

} // namespace

TEST(SoundWavTest, ParsesValidMono16Bit) {
    auto r = SoundBank::parseWav(makeWav(kSamples));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(r->rate, 44100);
    EXPECT_EQ(r->pcm, kSamples);
}

TEST(SoundWavTest, SkipsUnknownChunks) {
    auto r = SoundBank::parseWav(makeWav(kSamples, 44100, 1, 16, 1, true));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(r->pcm, kSamples);
}

TEST(SoundWavTest, RejectsNonRiffData) {
    auto r = SoundBank::parseWav({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
    EXPECT_FALSE(r.ok());
    r = SoundBank::parseWav({});
    EXPECT_FALSE(r.ok());
}

TEST(SoundWavTest, RejectsNonPcmFormat) {
    auto r = SoundBank::parseWav(makeWav(kSamples, 44100, 1, 16, 3));
    EXPECT_FALSE(r.ok());
}

TEST(SoundWavTest, RejectsStereo) {
    auto r = SoundBank::parseWav(makeWav(kSamples, 44100, 2));
    EXPECT_FALSE(r.ok());
}

TEST(SoundWavTest, Rejects8Bit) {
    auto r = SoundBank::parseWav(makeWav(kSamples, 44100, 1, 8));
    EXPECT_FALSE(r.ok());
}

TEST(SoundWavTest, RejectsOtherRates) {
    auto r = SoundBank::parseWav(makeWav(kSamples, 22050));
    EXPECT_FALSE(r.ok());
}

TEST(SoundWavTest, RejectsMissingChunks) {
    EXPECT_FALSE(SoundBank::parseWav(makeWav(kSamples, 44100, 1, 16, 1, false,
                                             false)).ok());
    // fmt only, no data:
    {
        std::vector<std::uint8_t> out;
        pushTag(out, "RIFF");
        pushU32(out, 36);
        pushTag(out, "WAVE");
        pushTag(out, "fmt ");
        pushU32(out, 16);
        pushU16(out, 1);
        pushU16(out, 1);
        pushU32(out, 44100);
        pushU32(out, 88200);
        pushU16(out, 2);
        pushU16(out, 16);
        EXPECT_FALSE(SoundBank::parseWav(out).ok());
    }
}

TEST(SoundWavTest, RejectsTruncatedData) {
    auto wav = makeWav(kSamples);
    wav.resize(wav.size() - 3); // chop the payload
    EXPECT_FALSE(SoundBank::parseWav(wav).ok());
}

TEST(SoundApplyGainTest, ScalesAndClamps) {
    const std::vector<std::int16_t> in = {1000, -1000, 30000, -30000, 0};
    auto out = SoundBank::applyGain(in, 0.5f);
    EXPECT_EQ(out, (std::vector<std::int16_t> {500, -500, 15000, -15000, 0}));
    out = SoundBank::applyGain(in, 2.0f);
    EXPECT_EQ(out[2], 32767);  // clamped high
    EXPECT_EQ(out[3], -32768); // clamped low
    out = SoundBank::applyGain(in, 0.0f);
    for (const std::int16_t v : out) {
        EXPECT_EQ(v, 0);
    }
}
