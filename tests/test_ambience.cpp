// PRE-4: the ambience's CPU-side contracts — the context decision
// (world/ambience's pure ambienceContext: cave priority over day/night,
// the 0.5 dayFactor cutoff pinned) and the schedule roll (the pure
// ambienceRoll: determinism pins on the shipped world seed, the 300-frame
// boundary, the ~0.5 probability band), the sound names, the 3 wav
// assets (existence + rate + duration), and the audio.ambience config
// key semantics (code default true — config.json is untouched). Audio
// never enters the frame state (the CRAFT-3 discipline): no screenshot
// or save assertion is possible — the gate's log evidence + the asset
// pins own the observability.

#include <gtest/gtest.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "core/config.hpp"
#include "core/sound.hpp"
#include "world/ambience.hpp"

using mc::ambienceContext;
using mc::ambienceRoll;
using mc::ambienceSoundName;
using mc::AmbienceContext;

// The context decision (the pure function): raw sky light 0 = no sky
// path = CAVE, and the cave takes PRIORITY over day/night (a cave at
// noon is a cave); otherwise the renderer's day/night split on the
// dayFactor's 0.5 cutoff (>= = day, the boundary pinned).
TEST(AmbienceContextTest, DecisionPinned) {
    // Cave priority: sky 0 wins at every dayFactor.
    EXPECT_EQ(ambienceContext(0, 0.9f), AmbienceContext::Cave);
    EXPECT_EQ(ambienceContext(0, 0.5f), AmbienceContext::Cave);
    EXPECT_EQ(ambienceContext(0, 0.0f), AmbienceContext::Cave);
    // Day: sky present + dayFactor >= 0.5 (the cutoff is INCLUSIVE).
    EXPECT_EQ(ambienceContext(15, 0.9f), AmbienceContext::Day);
    EXPECT_EQ(ambienceContext(15, 0.5f), AmbienceContext::Day);
    EXPECT_EQ(ambienceContext(1, 1.0f), AmbienceContext::Day);
    // Night: sky present + dayFactor < 0.5 (one step below the cutoff).
    EXPECT_EQ(ambienceContext(15, 0.1f), AmbienceContext::Night);
    EXPECT_EQ(ambienceContext(15, 0.0f), AmbienceContext::Night);
    EXPECT_EQ(ambienceContext(15, 0.499f), AmbienceContext::Night);
}

// The schedule roll on the SHIPPED world seed (123456789 — the gate
// S2's scene): the pins are the contract (the day/night runs' expected
// log-event frames derive from them). The frame advances by 300 (the
// roll cadence); the (299, day) vs (300, day) boundary proves the
// PITS #4 advance is live (a constant-input finalizer would tie them).
TEST(AmbienceRollTest, ShippedSeedPins) {
    const std::uint64_t seed = 123456789ULL;
    EXPECT_TRUE(ambienceRoll(seed, 0, AmbienceContext::Day));
    EXPECT_TRUE(ambienceRoll(seed, 300, AmbienceContext::Day));
    EXPECT_FALSE(ambienceRoll(seed, 600, AmbienceContext::Day));
    EXPECT_TRUE(ambienceRoll(seed, 900, AmbienceContext::Day));
    EXPECT_TRUE(ambienceRoll(seed, 0, AmbienceContext::Night));
    EXPECT_TRUE(ambienceRoll(seed, 300, AmbienceContext::Night));
    EXPECT_TRUE(ambienceRoll(seed, 600, AmbienceContext::Night));
    EXPECT_FALSE(ambienceRoll(seed, 900, AmbienceContext::Night));
    EXPECT_TRUE(ambienceRoll(seed, 0, AmbienceContext::Cave));
    EXPECT_FALSE(ambienceRoll(seed, 600, AmbienceContext::Cave));
    // The 299/300 boundary: different frames, different rolls.
    EXPECT_FALSE(ambienceRoll(seed, 299, AmbienceContext::Day));
    EXPECT_TRUE(ambienceRoll(seed, 300, AmbienceContext::Day));
    // The contexts are decorrelated (different states, the kAmbienceCtxStep
    // discipline — no context rides on another's stream): at frame 900
    // the day and night rolls disagree (the pins above).
    EXPECT_NE(ambienceRoll(seed, 900, AmbienceContext::Day),
              ambienceRoll(seed, 900, AmbienceContext::Night));
}

// The ~0.5 probability: over 1000 rolls of one fixed context's frame
// sequence (300-frame stride, the schedule) the pass count sits in a
// wide band around half (the splitmix64 top half — no bias).
TEST(AmbienceRollTest, ProbabilityBand) {
    const std::uint64_t seed = 123456789ULL;
    int dayPass = 0, nightPass = 0;
    for (int i = 0; i < 1000; ++i) {
        const std::uint64_t frame = static_cast<std::uint64_t>(i) * 300;
        if (ambienceRoll(seed, frame, AmbienceContext::Day)) {
            ++dayPass;
        }
        if (ambienceRoll(seed, frame, AmbienceContext::Night)) {
            ++nightPass;
        }
    }
    EXPECT_GT(dayPass, 400);
    EXPECT_LT(dayPass, 600);
    EXPECT_GT(nightPass, 400);
    EXPECT_LT(nightPass, 600);
}

// The sound names (the code constants — the wav stems the SoundBank
// resolves; an unknown name is a silent no-op, the asset pin below owns
// the existence).
TEST(AmbienceSoundNameTest, NamesPinned) {
    EXPECT_STREQ(ambienceSoundName(AmbienceContext::Cave), "ambience_cave");
    EXPECT_STREQ(ambienceSoundName(AmbienceContext::Day), "ambience_day");
    EXPECT_STREQ(
        ambienceSoundName(AmbienceContext::Night), "ambience_night");
}

namespace {

// Reads a whole file (the test_item.cpp idiom — run from the repo root).
bool readFile(const std::string& path, std::vector<std::uint8_t>& out) {
    FILE* fp = std::fopen(path.c_str(), "rb");
    if (fp == nullptr) {
        return false;
    }
    char buf[4096];
    std::size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), fp)) > 0) {
        out.insert(out.end(), buf, buf + n);
    }
    std::fclose(fp);
    return true;
}

} // namespace

// The 3 ambience assets (tools/generate_sounds.py — the fixed-seed
// LCG/sine paradigm): existence, the 16-bit mono 44100 parse, and the
// 2-4 s duration band (the bed's length). The EXISTING 35 wavs'
// byte-identity is the generator's re-run evidence (md5 before/after,
// the CRAFT-3 discipline — build-time, not a unit seam).
TEST(AmbienceAssetsTest, ThreeWavsExistAndParse) {
    const char* names[3] = {"ambience_cave", "ambience_day",
                            "ambience_night"};
    for (const char* name : names) {
        std::vector<std::uint8_t> data;
        ASSERT_TRUE(readFile(std::string("assets/sounds/") + name + ".wav",
                             data))
            << "run from the repo root: " << name;
        auto r = mc::SoundBank::parseWav(data);
        ASSERT_TRUE(r.ok()) << name << ": " << r.error().message;
        EXPECT_EQ(r->rate, 44100) << name;
        const double seconds =
            static_cast<double>(r->pcm.size()) / 44100.0;
        EXPECT_GE(seconds, 2.0) << name;
        EXPECT_LE(seconds, 4.0) << name;
        // A bed is never silent: some audible content in the middle.
        bool audible = false;
        for (std::size_t i = r->pcm.size() / 4; i < r->pcm.size() * 3 / 4;
             ++i) {
            if (std::abs(r->pcm[i]) > 32) {
                audible = true;
                break;
            }
        }
        EXPECT_TRUE(audible) << name << " is silent in its middle";
    }
}

// audio.ambience: the key is absent from config.json by design — the
// code default (true) applies (the render.fog getBool precedent).
TEST(AmbienceConfigTest, MissingKeyDefaultsToOn) {
    mc::Config cfg;
    EXPECT_TRUE(cfg.getBool("audio.ambience", true));
}

// An explicit false silences the pass (the gate's "new-off" control:
// zero events, zero sounds, the v0.27.0 pipeline bit-identical).
TEST(AmbienceConfigTest, ExplicitFalseTurnsOff) {
    mc::Config cfg;
    cfg.set("audio.ambience", mc::Json(false));
    EXPECT_FALSE(cfg.getBool("audio.ambience", true));
}
