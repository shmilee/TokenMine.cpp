// Phase 15 tests: the WAV parser and gain scaling (the pure part of the
// SoundBank — no SDL device involved).
// CRAFT-3: the spatialization pure functions (distanceGain / panGains /
// toStereo) + the disabled-bank degradation pin.
#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
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

// ---------------------------------------------------------------------------
// V2-14: the bat's ambient squeak (the sound set 41 -> 42).
// ---------------------------------------------------------------------------

namespace {

std::string readFileBytes(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::string();
    }
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

} // namespace

TEST(SoundBatTest, SqueakWavShipsAndParses) {
    // The squeak.wav (the generate_sounds.py's dedicated fixed seed 0xB441
    // — the sleep precedent): a 16-bit mono 44.1 kHz PCM (the bank's
    // strict format), 0.12 s (the chirp's length — the MC ambient's
    // short-cave-squeak figure), in the 0.1 ambient volume class (quieter
    // than the SFX taps, the ambience beds' class).
    const std::filesystem::path path = "assets/sounds/squeak.wav";
    ASSERT_TRUE(std::filesystem::exists(path))
        << "run the tests from the repository root";
    const std::string bytes = readFileBytes(path);
    ASSERT_FALSE(bytes.empty());
    std::vector<std::uint8_t> raw(bytes.begin(), bytes.end());
    auto sample = SoundBank::parseWav(raw);
    ASSERT_TRUE(sample.ok()) << sample.error().describe();
    EXPECT_EQ(sample->rate, 44100);
    // 0.12 s at 44100 Hz = 5292 samples (the generator's exact length).
    EXPECT_EQ(sample->pcm.size(), 5292u);
    // The chirp is not silence (the synthesis produced content)...
    bool any = false;
    for (const std::int16_t s : sample->pcm) {
        if (s != 0) {
            any = true;
            break;
        }
    }
    EXPECT_TRUE(any);
    // ...and it sits in the ambient volume class: the 0.1 tone under the
    // 0.15 noise whisper peaks at ~0.11 of the 16-bit range (3589 — the
    // pinned synthesis figure, the MC volume 0.1 reference + the grit).
    int peak = 0;
    for (const std::int16_t s : sample->pcm) {
        const int m = std::abs(s);
        if (m > peak) {
            peak = m;
        }
    }
    EXPECT_EQ(peak, 3589);
}

TEST(SoundBatTest, SoundDirectoryGrowsToSeventy) {
    // The sound set 41 -> 42 (the squeak — the V2-14 file) -> 43 (the
    // shear — the V2-15 file) -> 46 (the snow family — the three V2-16
    // files) -> 47 (the portal ambience — the V3-1 Phase A file) -> 49
    // (the portal trigger + travel — the V3-1 Phase B files) -> 60 (the
    // V3-7 Part 2 nether face — the nine material wavs + the
    // evaporate + the ambience_nether) -> 63 (the V3-3 nether mob
    // ambient family — the hoglin grunt + the piglin whisper + the
    // strider hiss, the 60 -> 63 conscious census update; -> 64 (the
    // V3-14a boom — the explosion's thud); -> 66 (the V3-14b ghast's
    // two voices — the ambient screech + the pre-shot roar); -> 67 (the
    // V3-14c barter chime — the piglin's gold-trade cue); -> 68 (the
    // V3-10a ambience_end — the fifth context's bed, the T4's twin);
    // -> 70 (the V3-10b pearl's two cues — the first throwable's
    // sound face: the throw's whoop + the teleport's shimmer). Each
    // phase's new files are the only additions, the
    // generate_sounds.py re-run leaves the others byte-identical, the
    // script's md5 discipline; the FNV pins hold each phase's bytes.
    std::size_t count = 0;
    for (const auto& entry :
         std::filesystem::directory_iterator("assets/sounds")) {
        if (entry.is_regular_file() && entry.path().extension() == ".wav") {
            ++count;
        }
    }
    EXPECT_EQ(count, 70u);
}

// ---------------------------------------------------------------------------
// V2-15: the shears' cut (the sound set 42 -> 43).
// ---------------------------------------------------------------------------

TEST(SoundShearTest, ShearWavShipsAndParses) {
    // The shear.wav (the generate_sounds.py's dedicated fixed seed
    // 0x5B4A — the sleep / squeak precedent): a 16-bit mono 44.1 kHz PCM
    // (the bank's strict format), 0.18 s (the double-snick's length), in
    // the SFX volume class.
    const std::filesystem::path path = "assets/sounds/shear.wav";
    ASSERT_TRUE(std::filesystem::exists(path))
        << "run the tests from the repository root";
    const std::string bytes = readFileBytes(path);
    ASSERT_FALSE(bytes.empty());
    std::vector<std::uint8_t> raw(bytes.begin(), bytes.end());
    auto sample = SoundBank::parseWav(raw);
    ASSERT_TRUE(sample.ok()) << sample.error().describe();
    EXPECT_EQ(sample->rate, 44100);
    // 0.18 s at 44100 Hz = 7938 samples (the generator's exact length).
    EXPECT_EQ(sample->pcm.size(), 7938u);
    // The snick is not silence (the synthesis produced content)...
    bool any = false;
    for (const std::int16_t s : sample->pcm) {
        if (s != 0) {
            any = true;
            break;
        }
    }
    EXPECT_TRUE(any);
    // ...and its peak is the pinned synthesis figure (the 0.55 bright
    // catch layer under the 0.9 master, the SFX class).
    int peak = 0;
    for (const std::int16_t s : sample->pcm) {
        const int m = std::abs(s);
        if (m > peak) {
            peak = m;
        }
    }
    EXPECT_EQ(peak, 15899);
}

// ---------------------------------------------------------------------------
// V2-16: the snow sound family (the sound set 43 -> 46).
// ---------------------------------------------------------------------------

namespace {

// One shipped wav: existence, the 16-bit mono 44.1 kHz parse, the exact
// synthesis length, and the pinned peak (non-silent + byte-stable — the
// squeak / shear pin paradigm).
void expectWavPin(const std::string& name, std::size_t samples, int peak) {
    const std::filesystem::path path = "assets/sounds/" + name + ".wav";
    ASSERT_TRUE(std::filesystem::exists(path))
        << "run the tests from the repository root";
    const std::string bytes = readFileBytes(path);
    ASSERT_FALSE(bytes.empty());
    std::vector<std::uint8_t> raw(bytes.begin(), bytes.end());
    auto sample = SoundBank::parseWav(raw);
    ASSERT_TRUE(sample.ok()) << sample.error().describe();
    EXPECT_EQ(sample->rate, 44100);
    EXPECT_EQ(sample->pcm.size(), samples);
    int pk = 0;
    for (const std::int16_t s : sample->pcm) {
        const int m = std::abs(s);
        if (m > pk) {
            pk = m;
        }
    }
    EXPECT_EQ(pk, peak);
}

} // namespace

TEST(SoundSnowTest, SnowMaterialWavsShipAndParse) {
    // The snow family (the generate_sounds.py's dedicated fixed seeds
    // 0x75001 / 0x75111 / 0x75222 — the sand-family paradigm one step
    // softer: powder, not grains — the double lowpass, the quiet
    // amplitudes): a single advancing LCG per file (the paradigm the
    // _noise helper's legacy per-sample rebuild never carried), in the
    // SFX volume class.
    expectWavPin("snow_dig", 4851, 9125);    // 0.11 s
    expectWavPin("snow_place", 5292, 19350); // 0.12 s
    expectWavPin("snow_break", 7938, 8864);  // 0.18 s
}

// ---------------------------------------------------------------------------
// V2-16 follow-up: the _noise helper repair (the 11 material wavs).
// ---------------------------------------------------------------------------

TEST(SoundNoiseHelperRepairTest, NoiseWavsShipTextured) {
    // The per-sample-LCG-rebuild defect (round 1 = the _noise helper's 11
    // material consumers; round 2 = the generic dig / break_sfx, which
    // inlined the same pattern) returned a CONSTANT stream: the
    // lowpassed consumers went SILENT when the constant was negative
    // (the shipped sand_break / gravel_break / dig / break, peak 0 —
    // lowpass3's max(0.0, ...) clamp) and degenerated into texture-free
    // DC ramps otherwise. The repair (one advancing instance, the snow
    // family's paradigm) changed EXACTLY these 13 files' bytes — every
    // other wav stays byte-identical (the md5 census vs git HEAD); the
    // lengths are unchanged (the durations are untouched). The exact
    // peak pins are the regression guard: a constant stream cannot
    // reproduce them (0 for the silent quartet, a different single
    // figure for the DC ramp pair).
    const struct {
        const char* name;
        std::size_t samples;
        int peak;
    } want[] = {
        // round 2: the generic set (the no-sound-material fallback —
        // the most-consumed sounds in the game).
        {"dig", 4410, 14460},         // 0.10 s (was silent)
        {"break", 7938, 16060},       // 0.18 s (was silent)
        // round 1: the _noise helper's material consumers.
        {"cloth_break", 6615, 7167},  // 0.15 s
        {"cloth_dig", 5292, 6199},    // 0.12 s
        {"glass_break", 7938, 25919}, // 0.18 s
        {"gravel_break", 9702, 16709}, // 0.22 s (was silent)
        {"gravel_dig", 4410, 14985},  // 0.10 s
        {"sand_break", 8820, 14721},  // 0.20 s (was silent)
        {"sand_dig", 5292, 9099},     // 0.12 s
        {"stone_break", 7056, 30294}, // 0.16 s
        {"stone_dig", 3528, 17104},   // 0.08 s
        {"wood_break", 7056, 14618},  // 0.16 s
        {"wood_dig", 3969, 13567},    // 0.09 s
    };
    ASSERT_EQ(sizeof(want) / sizeof(want[0]), 13u); // the full damage set
    for (const auto& e : want) {
        expectWavPin(e.name, e.samples, e.peak);
    }
    // The census is the V3-10b pin (70 files — the directory count:
    // the V3-10a's 68 + the pearl's two cues, the first throwable's
    // sound face — the repair itself edits in place, no other
    // additions).
    std::size_t count = 0;
    for (const auto& entry :
         std::filesystem::directory_iterator("assets/sounds")) {
        if (entry.is_regular_file() && entry.path().extension() == ".wav") {
            ++count;
        }
    }
    EXPECT_EQ(count, 70u);
}

// ---------------------------------------------------------------------------
// V3-14c: the piglin's barter chime (the sound set 66 -> 67).
// ---------------------------------------------------------------------------

TEST(SoundBarterTest, BarterWavShipsAndParses) {
    // The barter.wav (the generate_sounds.py's dedicated fixed seed
    // 0x6A11 — the smelt-ding precedent): a 16-bit mono 44.1 kHz PCM
    // (the bank's strict format), 0.16 s (the chime's length), the SFX
    // volume class. The exact peak pin is the PITS 382 regression
    // guard: a silent / degenerate synthesis cannot reproduce it.
    expectWavPin("barter", 7056, 20792);
}

// ---------------------------------------------------------------------------
// V3-14a: the explosion's thud (the sound set 63 -> 64).
// ---------------------------------------------------------------------------

TEST(SoundExplosionTest, BoomWavShipsAndParses) {
    // The boom.wav (the generate_sounds.py's dedicated fixed seed
    // 0xB007 — the squeak / sleep precedent): a 16-bit mono 44.1 kHz
    // PCM (the bank's strict format), 0.9 s (the concussion's length),
    // the SFX volume class. The exact peak pin is the PITS 382
    // regression guard: a silent / degenerate synthesis cannot
    // reproduce it.
    expectWavPin("boom", 39690, 32574);
}

// ---------------------------------------------------------------------------
// V3-14b: the ghast's two voices (the sound set 64 -> 66).
// ---------------------------------------------------------------------------

TEST(SoundGhastTest, GhastAmbientWavShipsAndParses) {
    // The ghast.wav (the generate_sounds.py's dedicated fixed seed
    // 0x6600 — the squeak / sleep precedent): a 16-bit mono 44.1 kHz
    // PCM (the bank's strict format), ~1.4 s (the wail's length), the
    // 0.5 ambient volume class (the portal_ambient's twin — the
    // nether's second voice). The exact peak pin is the PITS 382
    // regression guard: a silent / degenerate synthesis cannot
    // reproduce it.
    expectWavPin("ghast", 61739, 16734);
}

TEST(SoundGhastTest, GhastRoarWavShipsAndParses) {
    // The ghast_roar.wav (seed 0x6E0B): the pre-fireball warning,
    // 0.5 s, the SFX volume class (the ignite's twin — the shot's cue,
    // not ambience). The exact peak pin is the PITS 382 regression
    // guard.
    expectWavPin("ghast_roar", 22050, 25351);
}

// ---------------------------------------------------------------------------
// V3-1 Phase A: the nether portal's ambience (the sound set 46 -> 47).
// ---------------------------------------------------------------------------

TEST(SoundPortalTest, PortalAmbientWavShipsAndParses) {
    // The portal_ambient.wav (the generate_sounds.py's dedicated fixed
    // seed 0xC4F1 — the squeak / sleep precedent): a 16-bit mono
    // 44.1 kHz PCM (the bank's strict format), 1.2 s (the hum's length),
    // in the 0.5 ambient volume class (the squeak's 0.1 class one step
    // up — the master amplitude bakes the volume). The exact peak pin is
    // the PITS 382 regression guard: a silent / degenerate (constant or
    // DC-ramp) synthesis cannot reproduce it.
    expectWavPin("portal_ambient", 52920, 12920);
}

// ---------------------------------------------------------------------------
// V3-1 Phase B: the travel pipeline's cues (the sound set 47 -> 49).
// ---------------------------------------------------------------------------

TEST(SoundPortalTravelTest, TriggerAndTravelWavsShipAndParse) {
    // The portal_trigger.wav (the generate_sounds.py's dedicated fixed
    // seed 0xC5A2 — the squeak / sleep precedent): the entry cue (the
    // Minecraft block.portal.trigger class — the short vortex swish as
    // the countdown arms), 0.35 s, in the SFX volume class. The
    // portal_travel.wav (seed 0xC6B3): the travel cue (the Minecraft
    // block.portal.travel class — the fading-out vortex as the
    // dimension switch completes), 1.0 s, in the 0.5 ambient volume
    // class (the portal_ambient's twin). The exact peak pins are the
    // PITS 382 regression guards: a silent / degenerate (constant or
    // DC-ramp) synthesis cannot reproduce them.
    expectWavPin("portal_trigger", 15434, 16177); // 0.35 s
    expectWavPin("portal_travel", 44100, 10830);  // 1.0 s
}

// ---------------------------------------------------------------------------
// V3-7 Part 2 (v3.7.0): the nether material families + the evaporation
// cue (the sound set 49 -> 60: the nine material wavs + the evaporate
// + the ambience_nether — the last one's pin lives with the ambience
// assets in test_ambience.cpp).
// ---------------------------------------------------------------------------

TEST(SoundNetherFamilyTest, NetherMaterialWavsShipAndParse) {
    // The nether family (the generate_sounds.py's dedicated fixed seeds
    // 0x8E001 / 0x8E111 / 0x8E222 — the netherrack / magma_block /
    // ancient_debris sound_material reassignment, T2): the muffled low
    // nether-rock face (the sand-family paradigm, one step darker — the
    // double lowpass's rectified texture is the family's signature, the
    // stone's bright face replaced). The exact peak pins are the PITS
    // 382 regression guards: a silent / degenerate (constant or DC-ramp)
    // synthesis cannot reproduce them.
    expectWavPin("nether_dig", 3969, 15638);   // 0.09 s
    expectWavPin("nether_place", 4851, 21340); // 0.11 s
    expectWavPin("nether_break", 6615, 18887); // 0.15 s
}

TEST(SoundNetherFamilyTest, NyliumMaterialWavsShipAndParse) {
    // The nylium family (seeds 0x8F001 / 0x8F111 / 0x8F222 — the
    // crimson / warped nylium reassignment, T2): the soft wet
    // mushroom-stone face (the grass precedent's fieldless generic
    // events replaced — the double lowpass's one step darker).
    expectWavPin("nylium_dig", 4410, 12158);   // 0.10 s
    expectWavPin("nylium_place", 5292, 18996); // 0.12 s
    expectWavPin("nylium_break", 7938, 11706); // 0.18 s
}

TEST(SoundNetherFamilyTest, StemMaterialWavsShipAndParse) {
    // The stem family (seeds 0x90001 / 0x90111 / 0x90222 — the crimson
    // / warped stem reassignment, T2): the mid woody nether-stem face
    // (the wood family's one step crisper — the single lowpass's
    // brighter texture).
    expectWavPin("stem_dig", 3528, 15450);   // 0.08 s
    expectWavPin("stem_place", 3969, 22796); // 0.09 s
    expectWavPin("stem_break", 6174, 18182); // 0.14 s
}

TEST(SoundNetherFamilyTest, EvaporateWavShipsAndParses) {
    // T1 (v3.7.0): the nether's water evaporation cue (seed 0x9151 —
    // the water bucket empties in the nether, water cannot exist
    // there): the hiss (the double-lowpassed noise's fast decay) over
    // the quiet descending bubble tone, 0.40 s, in the SFX volume
    // class (the splash / ignite precedent). The exact peak pin is
    // the PITS 382 regression guard.
    expectWavPin("evaporate", 17640, 13764); // 0.40 s
}

// ---------------------------------------------------------------------------
// V3-3 (v3.8.0): the nether mob ambient family (the sound set 60 -> 63).
// The three per-mob ambients — the runAi ground roll plays them
// spatialized at the mob (the squeak's ground twin; the magma cube is
// the documented no-ambient).
// ---------------------------------------------------------------------------

TEST(SoundNetherMobTest, HoglinGruntWavShipsAndParses) {
    // The hoglin's ambient grunt (the generate_sounds.py's dedicated
    // fixed seed 0x9201): the 120 -> 70 Hz low glide with the 5 Hz
    // larynx tremolo under the quiet double-lowpassed grit, 0.30 s, in
    // the 0.25 ambient volume class (the squeak's 0.1 class one step
    // up — the master amplitude bakes the volume). The exact peak pin
    // is the PITS 382 regression guard.
    expectWavPin("hoglin_grunt", 13230, 4794); // 0.30 s
}

TEST(SoundNetherMobTest, PiglinWhisperWavShipsAndParses) {
    // The piglin's ambient whisper (seed 0x9202): the 2100 -> 2600 Hz
    // trill over the quiet double-lowpassed murmur bed, 0.25 s, in the
    // squeak's 0.1 ambient volume class (the piglin's whisper sits
    // under the hoglin's grunt).
    expectWavPin("piglin_whisper", 11025, 1616); // 0.25 s
}

TEST(SoundNetherMobTest, StriderHissWavShipsAndParses) {
    // The strider's ambient hiss (seed 0x9203): the lava-walker's
    // breath — the single-lowpassed hiss (brighter than the whisper's
    // double-lowpassed murmur) over the quiet 300 -> 150 Hz undertone,
    // 0.35 s, in the 0.2 ambient volume class (the evaporate cue's
    // quieter ambient twin).
    expectWavPin("strider_hiss", 15434, 3298); // 0.35 s
}
