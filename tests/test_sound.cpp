// Phase 15 tests: the WAV parser and gain scaling (the pure part of the
// SoundBank — no SDL device involved).
// CRAFT-3: the spatialization pure functions (distanceGain / panGains /
// toStereo) + the disabled-bank degradation pin.
#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <utility>
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

// ---------------------------------------------------------------------------
// CRAFT-3: the spatialization pure functions (no SDL).
// ---------------------------------------------------------------------------

TEST(SoundSpatialTest, DistanceGainIsLinearClamped) {
    // 0 distance = full gain; the cutoff = silence; linear in between.
    EXPECT_FLOAT_EQ(SoundBank::distanceGain(0.0f, 16.0f), 1.0f);
    EXPECT_FLOAT_EQ(SoundBank::distanceGain(8.0f, 16.0f), 0.5f);
    EXPECT_FLOAT_EQ(SoundBank::distanceGain(16.0f, 16.0f), 0.0f);
    EXPECT_FLOAT_EQ(SoundBank::distanceGain(24.0f, 16.0f), 0.0f); // past
    EXPECT_FLOAT_EQ(SoundBank::distanceGain(-1.0f, 16.0f), 1.0f);  // clamped
    // Degenerate cutoff: only the listener's own position is audible.
    EXPECT_FLOAT_EQ(SoundBank::distanceGain(0.0f, 0.0f), 1.0f);
    EXPECT_FLOAT_EQ(SoundBank::distanceGain(1.0f, 0.0f), 0.0f);
    // The shipped constant.
    EXPECT_FLOAT_EQ(kSoundCutoffBlocks, 16.0f);
}

TEST(SoundSpatialTest, PanGainsLeftRightCenter) {
    // Listener at the origin facing -Z (yaw 0): right = +X (the
    // Camera::right convention).
    const Vec3 at {0.0f, 0.0f, 0.0f};
    {
        const auto [l, r] = SoundBank::panGains(Vec3{1, 0, 0}, at, 0.0f);
        EXPECT_FLOAT_EQ(l, 0.0f); // full right
        EXPECT_FLOAT_EQ(r, 1.0f);
    }
    {
        const auto [l, r] = SoundBank::panGains(Vec3{-1, 0, 0}, at, 0.0f);
        EXPECT_FLOAT_EQ(l, 1.0f); // full left
        EXPECT_FLOAT_EQ(r, 0.0f);
    }
    {
        const auto [l, r] = SoundBank::panGains(Vec3{0, 0, -1}, at, 0.0f);
        EXPECT_FLOAT_EQ(l, 0.5f); // ahead: centered
        EXPECT_FLOAT_EQ(r, 0.5f);
    }
    {
        const auto [l, r] = SoundBank::panGains(Vec3{0, 0, 1}, at, 0.0f);
        EXPECT_FLOAT_EQ(l, 0.5f); // behind: centered (the lateral pan
        EXPECT_FLOAT_EQ(r, 0.5f); // cannot tell front from back — v1)
    }
    {
        const auto [l, r] = SoundBank::panGains(Vec3{0, 0, 0}, at, 0.0f);
        EXPECT_FLOAT_EQ(l, 0.5f); // AT the listener: centered (no divide)
        EXPECT_FLOAT_EQ(r, 0.5f);
    }
}

TEST(SoundSpatialTest, PanGainsFollowsYawAndIgnoresHeight) {
    // yaw = +pi/2 = facing -X (positive turns left): right = -Z.
    const float yaw = static_cast<float>(M_PI / 2.0);
    const Vec3 at {0.0f, 0.0f, 0.0f};
    {
        const auto [l, r] = SoundBank::panGains(Vec3{0, 0, -1}, at, yaw);
        EXPECT_FLOAT_EQ(l, 0.0f); // -Z is now on the listener's RIGHT
        EXPECT_FLOAT_EQ(r, 1.0f);
    }
    {
        const auto [l, r] = SoundBank::panGains(Vec3{0, 0, 1}, at, yaw);
        EXPECT_FLOAT_EQ(l, 1.0f); // +Z is now LEFT
        EXPECT_FLOAT_EQ(r, 0.0f);
    }
    // +X is straight AHEAD at this yaw (the cos(yaw) ~ 1e-8 residue is
    // immaterial — the pan is centered to within the float floor).
    {
        const auto [l, r] = SoundBank::panGains(Vec3{1, 0, 0}, at, yaw);
        EXPECT_NEAR(l, 0.5f, 1e-6f);
        EXPECT_NEAR(r, 0.5f, 1e-6f);
    }
    // The height never pans: 100 blocks up on the right is still full
    // right (at yaw 0, +X).
    {
        const auto [l, r] = SoundBank::panGains(Vec3{1, 100, 0}, at, 0.0f);
        EXPECT_FLOAT_EQ(l, 0.0f);
        EXPECT_FLOAT_EQ(r, 1.0f);
    }
}

TEST(SoundSpatialTest, PanGainsLinear45DegreeTie) {
    // Diagonal front-right at yaw 0 (toward +X, -Z): lateral = 1/sqrt(2).
    const Vec3 at {0.0f, 0.0f, 0.0f};
    const auto [l, r] =
        SoundBank::panGains(Vec3{1, 0, -1}, at, 0.0f);
    const float t = 1.0f / std::sqrt(2.0f); // ~0.70710678
    EXPECT_NEAR(l, 0.5f - 0.5f * t, 1e-6f); // ~0.14644661
    EXPECT_NEAR(r, 0.5f + 0.5f * t, 1e-6f); // ~0.85355339
    EXPECT_NEAR(l + r, 1.0f, 1e-6f);        // constant-gain: sums to 1
}

TEST(SoundSpatialTest, ToStereoDuplicatesFor2D) {
    // gainL == gainR == 1 = the legacy 2D path: the mono sample DUPLICATED
    // to both channels (perceptually bit-equivalent to the pre-stereo
    // mono stream the device upmixed).
    const std::vector<std::int16_t> in = {100, -200, 32767, -32768, 0};
    auto out = SoundBank::toStereo(in, 1.0f, 1.0f);
    ASSERT_EQ(out.size(), 10u);
    EXPECT_EQ(out, (std::vector<std::int16_t> {
        100, 100, -200, -200, 32767, 32767, -32768, -32768, 0, 0}));
}

TEST(SoundSpatialTest, ToStereoPansAndClamps) {
    const std::vector<std::int16_t> in = {1000, -1000};
    auto out = SoundBank::toStereo(in, 0.0f, 1.0f); // full right
    EXPECT_EQ(out, (std::vector<std::int16_t> {0, 1000, 0, -1000}));
    out = SoundBank::toStereo(in, 0.5f, 0.5f); // centered
    EXPECT_EQ(out, (std::vector<std::int16_t> {500, 500, -500, -500}));
    out = SoundBank::toStereo({1000}, 0.25f, 0.75f); // asymmetric
    EXPECT_EQ(out, (std::vector<std::int16_t> {250, 750}));
    // Gains above 1 saturate to the 16-bit range (no overflow).
    out = SoundBank::toStereo({30000, -30000}, 2.0f, 2.0f);
    EXPECT_EQ(out, (std::vector<std::int16_t> {32767, 32767, -32768, -32768}));
}

TEST(SoundSpatialTest, ToStereoEmptyIsEmpty) {
    EXPECT_TRUE(SoundBank::toStereo({}, 1.0f, 1.0f).empty());
}

// The graceful degradation red line: the disabled bank (no audio device)
// is a FULL no-op — the CRAFT-3 stereoization must not change that path's
// semantics (both play arities, setListener included).
TEST(SoundBankTest, DisabledBankIsFullNoOp) {
    SoundBank bank;
    ASSERT_FALSE(bank.enabled());
    bank.setListener(Vec3{1, 2, 3}, 0.5f);
    bank.play("dig");
    bank.play("dig", Vec3{4, 5, 6});
    bank.play("unknown", std::nullopt);
    EXPECT_FALSE(bank.enabled()); // still disabled (the semantics are pinned)
    EXPECT_EQ(bank.sampleCount(), 0u);
}
