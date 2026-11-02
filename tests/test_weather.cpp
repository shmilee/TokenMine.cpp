// V2-4 weather (rain/thunder) — the pure state machine + presentation
// contracts (the campfire pattern: the unit tests own the logic). The
// design's eight groups:
//   1. the stream sequence pins (seed 123456789 — the shipped default
//      world seed, the gate scene: the first 5 full cycles, every event
//      frame + post-event state; the thunder salt's independence);
//   2. the initial-CLEAR no-rain guarantee (>= kClearMin frames of rain
//      silence from every seed — the 60f no-input baseline's structural
//      immunity);
//   3. the --weather rain initial state (the fixed kRainMin opening
//      segment, the main stream unconsumed, the streams taking over);
//   4. the rain drop position oracle (field determinism x2, bounds, the
//      period-16 seamless wrap);
//   5. the extinguish predicate (fire / campfire cases + off-immunity —
//      the mob's rain extinguish, the third case, lives in
//      test_mob.cpp's MobFireTest.ExtinguishedInRain over the live
//      entity path);
//   6. the soilIsWet raining variant (+ the crop growth pace release);
//   7. the render.weather strict read (the non-boolean named);
//   8. the --weather CLI word validation (the unknown word named).
// Plus the thunder pulse's 5-frame linear decay pins and the thunder
// wav asset pin (the ambience asset pattern — the fixed-seed generator,
// the existing 39 wavs' byte-identity is the build-time md5 evidence).
#include <gtest/gtest.h>

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "block/block_registry.hpp"
#include "core/json.hpp"
#include "core/sound.hpp"
#include "core/weather.hpp"
#include "world/fire_spread.hpp"
#include "world/growth.hpp"
#include "world/world.hpp"

using namespace mc;

namespace {

Json parseDef(const std::string& text) {
    auto r = Json::parse(text);
    if (!r.ok()) {
        throw std::runtime_error(r.error().message);
    }
    return *r;
}

// Reads a whole file (the test_item.cpp idiom — run from the repo root).
bool readFile(const std::string& path, std::vector<std::uint8_t>& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    out.assign(std::istreambuf_iterator<char>(in),
               std::istreambuf_iterator<char>());
    return !out.empty();
}

// The crop pace fixture (group 6): a 2-stage chain crop_a -> crop_b
// (b mature) with growth_ticks 1 + grows_on soil + dry_slowdown 2 — the
// dry pace's modulus is 2, the wet/rain pace 1 (every frame passes).
BlockRegistry paceRegistry() {
    BlockRegistry reg;
    auto soilR = reg.registerBlock(parseDef(
        R"({"id":"soil","properties":{"hardness":1.0},"model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    if (!soilR.ok()) {
        throw std::runtime_error(soilR.error().message);
    }
    auto aR = reg.registerBlock(parseDef(
        R"({"id":"crop_a","properties":{"hardness":0.0,"grows_to":"crop_b","growth_ticks":1,"grows_on":"soil","dry_slowdown":2},"model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    if (!aR.ok()) {
        throw std::runtime_error(aR.error().message);
    }
    auto bR = reg.registerBlock(parseDef(
        R"({"id":"crop_b","properties":{"hardness":0.0},"model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    if (!bR.ok()) {
        throw std::runtime_error(bR.error().message);
    }
    return reg;
}

} // namespace

// ---------------------------------------------------------------------------
// Group 1: the stream sequence pins (seed 123456789, the first 5 cycles).
// ---------------------------------------------------------------------------

TEST(WeatherStreamTest, FirstFiveCyclesPinned) {
    // The pins are the contract (the gate's S2 rain/clear scenes'
    // expected log-event frames derive from them): any drift in the
    // streams (the PITS #4 advance, the roll reduction, the flip order,
    // the thunder re-roll) breaks one.
    std::uint64_t stream = 123456789ULL;
    std::uint64_t thunderStream = stream + kSaltWeatherThunder;
    WeatherState s = weatherInitialState(false, stream, thunderStream);
    // The initial segment: CLEAR #1, duration 10203 (in [9000, 18000)).
    EXPECT_FALSE(s.raining);
    EXPECT_EQ(s.framesLeft, 10203);
    EXPECT_EQ(s.thunderLeft, 0);
    // (frame, enteredRain, exitedRain, thunder, post framesLeft, post
    // thunderLeft) — the post framesLeft is the CURRENT segment's
    // remaining count after this frame's step (the flip frame carries
    // the NEW segment's full duration).
    struct Pin {
        int frame;
        bool entered, exited, thunder;
        int framesLeft, thunderLeft;
    };
    const Pin pins[] = {
        // Rain 1 (f 10203..14638, duration 4436):
        {10203, true, false, false, 4436, 741},
        {10944, false, false, true, 3695, 1388},
        {12332, false, false, true, 2307, 1020},
        {13352, false, false, true, 1287, 1777},
        {14639, false, true, false, 10724, 0},
        // Rain 2 (f 25363..29921, duration 4559):
        {25363, true, false, false, 4559, 1485},
        {26848, false, false, true, 3074, 1166},
        {28014, false, false, true, 1908, 659},
        {28673, false, false, true, 1249, 1751},
        {29922, false, true, false, 9918, 0},
        // Rain 3 (f 39840..44940, duration 5101):
        {39840, true, false, false, 5101, 1719},
        {41559, false, false, true, 3382, 1395},
        {42954, false, false, true, 1987, 1456},
        {44410, false, false, true, 531, 1751},
        {44941, false, true, false, 16515, 0},
        // Rain 4 (f 61456..66768, duration 5313):
        {61456, true, false, false, 5313, 1292},
        {62748, false, false, true, 4021, 1471},
        {64219, false, false, true, 2550, 1206},
        {65425, false, false, true, 1344, 1211},
        {66636, false, false, true, 133, 1317},
        {66769, false, true, false, 13919, 0},
        // Rain 5 (f 80688..85444, duration 4757):
        {80688, true, false, false, 4757, 798},
        {81486, false, false, true, 3959, 1573},
        {83059, false, false, true, 2386, 867},
        {83926, false, false, true, 1519, 1794},
        {85445, false, true, false, 16824, 0},
    };
    int current = 0;
    for (const Pin& pin : pins) {
        for (int f = current + 1; f <= pin.frame; ++f) {
            const WeatherEvent ev =
                weatherStep(s, stream, thunderStream);
            if (f == pin.frame) {
                EXPECT_EQ(ev.enteredRain, pin.entered) << "f" << f;
                EXPECT_EQ(ev.exitedRain, pin.exited) << "f" << f;
                EXPECT_EQ(ev.thunder, pin.thunder) << "f" << f;
                EXPECT_EQ(s.framesLeft, pin.framesLeft) << "f" << f;
                EXPECT_EQ(s.thunderLeft, pin.thunderLeft) << "f" << f;
            } else {
                // No UNPINNED event between pins (a drift would fire one
                // early/late — the sequence is fully pinned).
                EXPECT_FALSE(ev.enteredRain) << "f" << f;
                EXPECT_FALSE(ev.exitedRain) << "f" << f;
                EXPECT_FALSE(ev.thunder) << "f" << f;
            }
        }
        current = pin.frame;
    }
    // The stream states after the run (the full-consumption pin — the
    // main stream advanced by 1 + 2*5 rolls, the thunder by 1 + 3+3+3+4+3
    // = 14 + 5 initial... the pin is the whole state, one value each):
    EXPECT_EQ(stream, 0xCC623AF87F8F21FCULL);
    EXPECT_EQ(thunderStream, 0xFA8CFC37F6BAACA7ULL);
}

TEST(WeatherStreamTest, SegmentRangesAndThunderIndependence) {
    // The ranges over the pinned 5 cycles (group 1's bands): every clear
    // in [9000, 18000), every rain in [3000, 6000), every thunder gap in
    // [600, 1800) — the alternation strict (the events never pair).
    const int clears[] = {10203, 10724, 9918, 16515, 13919, 16824};
    const int rains[] = {4436, 4559, 5101, 5313, 4757};
    for (const int c : clears) {
        EXPECT_GE(c, kClearMin);
        EXPECT_LT(c, kClearMax);
    }
    for (const int r : rains) {
        EXPECT_GE(r, kRainMin);
        EXPECT_LT(r, kRainMax);
    }
    // The thunder salt's independence (the design's "雷 salt 独立"): the
    // first thunder gap (741, pinned above) is rolled from the
    // dedicated-salt stream — the MAIN stream's consumption (the initial
    // clear roll, 10203) does not influence it: the --weather rain
    // override (a fresh main stream, never consumed) rolls the SAME first
    // gap, and its opening thunder lands on the same frame of rain.
    std::uint64_t mainA = 123456789ULL;
    std::uint64_t thunderA = mainA + kSaltWeatherThunder;
    const WeatherState natural =
        weatherInitialState(false, mainA, thunderA); // consumes mainA
    std::uint64_t mainB = 123456789ULL;
    std::uint64_t thunderB = mainB + kSaltWeatherThunder;
    const WeatherState override =
        weatherInitialState(true, mainB, thunderB); // consumes thunderB only
    EXPECT_EQ(natural.framesLeft, 10203);   // the main stream's roll
    EXPECT_EQ(mainB, 123456789ULL);         // the override leaves it be
    EXPECT_EQ(override.framesLeft, kRainMin);
    EXPECT_EQ(override.thunderLeft, 741);   // == natural's first gap
    EXPECT_EQ(natural.thunderLeft, 0);      // the schedule is RAIN-only
    // The fresh-thunder-stream first gap, directly: one advance + mix
    // from 123456789 + kSaltWeatherThunder must equal the pinned 741
    // (the dedicated salt's value family — a collision with any other
    // system's salt would shift it).
    std::uint64_t thunderC = 123456789ULL + kSaltWeatherThunder;
    const WeatherState viaC =
        weatherInitialState(true, mainB, thunderC);
    EXPECT_EQ(viaC.thunderLeft, override.thunderLeft);
}

// ---------------------------------------------------------------------------
// Group 2: the initial CLEAR — no rain before 9000 frames (every seed).
// ---------------------------------------------------------------------------

TEST(WeatherStreamTest, InitialClearNoRainBefore9000) {
    // The first segment is ALWAYS CLEAR with duration >= kClearMin
    // (9000 frames = 150 s): the 60-frame no-input baseline (the base
    // gate) never sees a rain event — the structural-immunity root.
    // Over five seeds (0, the config default, and three adversarial
    // values): no enteredRain / no thunder in the first 8999 steps, the
    // state stays clear, and the duration sits in the clear band.
    const std::uint64_t seeds[] = {0ULL, 1ULL, 42ULL, 123456789ULL,
                                   0xDEADBEEFCAFEBABEULL};
    for (const std::uint64_t seed : seeds) {
        std::uint64_t stream = seed;
        std::uint64_t thunderStream = seed + kSaltWeatherThunder;
        WeatherState s =
            weatherInitialState(false, stream, thunderStream);
        EXPECT_FALSE(s.raining) << "seed " << seed;
        EXPECT_GE(s.framesLeft, kClearMin) << "seed " << seed;
        EXPECT_LT(s.framesLeft, kClearMax) << "seed " << seed;
        for (int f = 1; f <= 8999; ++f) {
            const WeatherEvent ev =
                weatherStep(s, stream, thunderStream);
            EXPECT_FALSE(ev.enteredRain) << "seed " << seed << " f" << f;
            EXPECT_FALSE(ev.thunder) << "seed " << seed << " f" << f;
            EXPECT_FALSE(s.raining) << "seed " << seed << " f" << f;
        }
    }
}

// ---------------------------------------------------------------------------
// Group 3: the --weather rain initial state.
// ---------------------------------------------------------------------------

TEST(WeatherOverrideTest, RainInitialState) {
    // The override: RAIN from frame 0 with the FIXED kRainMin opening
    // duration (the design decision — reproducible), the first thunder
    // gap rolled from the independent salt stream; the MAIN stream is
    // left unconsumed (the override touches the initial state only —
    // the streams take over from there, the pinned natural clear 10203
    // follows the override's rain).
    std::uint64_t stream = 123456789ULL;
    std::uint64_t thunderStream = stream + kSaltWeatherThunder;
    const std::uint64_t streamBefore = stream;
    const WeatherState s = weatherInitialState(true, stream, thunderStream);
    EXPECT_TRUE(s.raining);
    EXPECT_EQ(s.framesLeft, kRainMin);
    EXPECT_GE(s.thunderLeft, kThunderMin);
    EXPECT_LT(s.thunderLeft, kThunderMax);
    EXPECT_EQ(stream, streamBefore); // the main stream unconsumed
    // The run: the first thunder at s.thunderLeft (741, group 1's pin),
    // the exit at exactly kRainMin with the NATURAL first clear's
    // duration (10203 — the main stream's first roll, untouched by the
    // override: "其后走流" pinned).
    WeatherState r = s;
    int thunders = 0;
    bool exited = false;
    for (int f = 1; f <= kRainMin; ++f) {
        const WeatherEvent ev = weatherStep(r, stream, thunderStream);
        if (ev.thunder) {
            ++thunders;
        }
        if (ev.exitedRain) {
            exited = true;
            EXPECT_EQ(f, kRainMin);
            EXPECT_EQ(r.framesLeft, 10203);
            EXPECT_FALSE(r.raining);
        }
    }
    EXPECT_TRUE(exited);
    EXPECT_EQ(thunders, 2); // 741 + 2129 (the pinned gap pair)
    // The override's clear -> the natural rain 1 (4436) follows: the
    // sequence is the no-override run's tail, one segment earlier.
    bool entered = false;
    for (int f = kRainMin + 1; f <= kRainMin + 10203; ++f) {
        const WeatherEvent ev = weatherStep(r, stream, thunderStream);
        if (ev.enteredRain) {
            entered = true;
            EXPECT_EQ(f, kRainMin + 10203);
            EXPECT_EQ(r.framesLeft, 4436); // group 1's rain 1 duration
        }
    }
    EXPECT_TRUE(entered);
}

// ---------------------------------------------------------------------------
// Group 4: the rain drop field + the position oracle.
// ---------------------------------------------------------------------------

TEST(RainDropTest, FieldDeterministicAndBounded) {
    // The field is a pure function of the world seed (the dedicated
    // kSaltRainDrop stream): same seed x2 -> same field, another seed ->
    // another field, every parameter in its documented range.
    const auto a = makeRainDrops(123456789);
    const auto b = makeRainDrops(123456789);
    const auto c = makeRainDrops(123456790);
    ASSERT_EQ(a.size(), static_cast<std::size_t>(kRainDropCount));
    ASSERT_EQ(b.size(), a.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        EXPECT_EQ(a[i].offset, b[i].offset) << "i" << i;
        EXPECT_FLOAT_EQ(a[i].phase, b[i].phase) << "i" << i;
        EXPECT_GE(a[i].offset.x, 0.0f) << "i" << i;
        EXPECT_LT(a[i].offset.x, kRainBoxSize) << "i" << i;
        EXPECT_GE(a[i].offset.y, 0.0f) << "i" << i;
        EXPECT_LT(a[i].offset.y, kRainBoxSize) << "i" << i;
        EXPECT_GE(a[i].offset.z, 0.0f) << "i" << i;
        EXPECT_LT(a[i].offset.z, kRainBoxSize) << "i" << i;
        EXPECT_GE(a[i].phase, 0.0f) << "i" << i;
        EXPECT_LT(a[i].phase, 1.0f) << "i" << i;
    }
    bool anyDifferent = false;
    for (std::size_t i = 0; i < a.size() && !anyDifferent; ++i) {
        anyDifferent =
            a[i].offset != c[i].offset || a[i].phase != c[i].phase;
    }
    EXPECT_TRUE(anyDifferent); // the seed decorrelates the fields
}

TEST(RainDropTest, FieldPinned) {
    // The first four drops of the shipped seed (the hexfloat pins — the
    // content-pin pattern: any drift in the stream (salt, advance,
    // reduction) is a contract break).
    const auto a = makeRainDrops(123456789);
    EXPECT_FLOAT_EQ(a[0].offset.x, 0x1.0e77f8p+3f);
    EXPECT_FLOAT_EQ(a[0].offset.y, 0x1.9450f6p+3f);
    EXPECT_FLOAT_EQ(a[0].offset.z, 0x1.dd8fcp+3f);
    EXPECT_FLOAT_EQ(a[0].phase, 0x1.96e9eep-1f);
    EXPECT_FLOAT_EQ(a[1].offset.x, 0x1.07372ep+2f);
    EXPECT_FLOAT_EQ(a[1].offset.y, 0x1.3c8b08p+3f);
    EXPECT_FLOAT_EQ(a[1].offset.z, 0x1.11fef8p+2f);
    EXPECT_FLOAT_EQ(a[1].phase, 0x1.defab6p-4f);
    EXPECT_FLOAT_EQ(a[2].offset.x, 0x1.8ed0c8p+3f);
    EXPECT_FLOAT_EQ(a[2].offset.y, 0x1.219a44p-2f);
    EXPECT_FLOAT_EQ(a[2].offset.z, 0x1.376d7cp+3f);
    EXPECT_FLOAT_EQ(a[2].phase, 0x1.7f07dep-1f);
    EXPECT_FLOAT_EQ(a[3].offset.x, 0x1.75698ep+3f);
    EXPECT_FLOAT_EQ(a[3].offset.y, 0x1.8392c4p+0f);
    EXPECT_FLOAT_EQ(a[3].offset.z, 0x1.47a01cp+3f);
    EXPECT_FLOAT_EQ(a[3].phase, 0x1.54f49cp-3f);
}

TEST(RainDropTest, PositionOraclePure) {
    // Same frame + same params -> same position, x2 (the design's
    // "同 frame 同参 ×2 同位"): the oracle is a pure function, and the
    // position stays in its documented shape (x/z constant, y wrapped in
    // the box; the period-16 identity — one full box crossing, the
    // seamless wrap).
    const auto drops = makeRainDrops(42);
    for (std::size_t i = 0; i < drops.size(); i += 37) {
        const RainDrop& d = drops[i];
        for (int f = 0; f <= 40; f += 7) {
            const Vec3 p1 = rainDropPosition(d, f);
            const Vec3 p2 = rainDropPosition(d, f); // the x2 identity
            EXPECT_EQ(p1, p2) << "i" << i << " f" << f;
            EXPECT_FLOAT_EQ(p1.x, d.offset.x) << "i" << i << " f" << f;
            EXPECT_FLOAT_EQ(p1.z, d.offset.z) << "i" << i << " f" << f;
            EXPECT_GE(p1.y, 0.0f) << "i" << i << " f" << f;
            EXPECT_LT(p1.y, kRainBoxSize) << "i" << i << " f" << f;
            // The period: kRainFallPeriodFrames frames = one box crossing
            // (fallSpeed * period = boxSize) -> the position repeats.
            EXPECT_EQ(rainDropPosition(d, f + kRainFallPeriodFrames), p1)
                << "i" << i << " f" << f;
        }
    }
    // A pinned walk (drop 0 of the shipped seed): the y advance + the
    // wrap at the box edge (at f=0 the phase offset is already applied:
    // y = (offset.y + phase * boxSize) mod boxSize — the full expression,
    // not the bare offset).
    const auto shipped = makeRainDrops(123456789);
    EXPECT_FLOAT_EQ(rainDropPosition(shipped[0], 0).y, 0x1.2b3ae4p+3f);
    EXPECT_FLOAT_EQ(rainDropPosition(shipped[0], 5).y, 0x1.cb3ae8p+3f);
    EXPECT_FLOAT_EQ(rainDropPosition(shipped[0], 10).y, 0x1.acebap+1f);
    EXPECT_FLOAT_EQ(rainDropPosition(shipped[0], 15).y, 0x1.0b3ae8p+3f);
    EXPECT_FLOAT_EQ(rainDropPosition(shipped[0], 20).y, 0x1.ab3ae8p+3f);
}

// ---------------------------------------------------------------------------
// Group 5: the extinguish predicate (fire / campfire + off-immunity).
// ---------------------------------------------------------------------------

TEST(ExtinguishPredicateTest, WaterOnlyBaseline) {
    // off-immunity (raining = false, the default = the pre-V2-4 water-
    // only semantics, bit-identical): a dry scene does NOT extinguish;
    // water in a 6-neighbor (ANY level — the Phase 64 rule) does;
    // distance 2 is not 6-adjacent; lava never extinguishes (the fire
    // beside the pool is the lava's product — the Phase 57 rule).
    World world;
    world.getOrCreateChunk({0, 0, 0});
    const BlockPos pos {8, 1, 8};
    EXPECT_FALSE(extinguishedByWaterOrRain(world, pos));
    EXPECT_FALSE(extinguishedByWaterOrRain(world, pos, false));
    world.setFluid(9, 1, 8, FluidKind::Water, 1); // any level counts
    EXPECT_TRUE(extinguishedByWaterOrRain(world, pos));
    world.setFluid(9, 1, 8, FluidKind::Water, 0);
    EXPECT_FALSE(extinguishedByWaterOrRain(world, pos));
    world.setFluid(10, 1, 8, FluidKind::Water, 8); // distance 2: no
    EXPECT_FALSE(extinguishedByWaterOrRain(world, pos));
    world.setFluid(10, 1, 8, FluidKind::Water, 0);
    world.setFluid(9, 1, 8, FluidKind::Lava, 8); // lava never
    EXPECT_FALSE(extinguishedByWaterOrRain(world, pos));
}

TEST(ExtinguishPredicateTest, RainExtinguishesDry) {
    // The V2-4 disjunct, the shared fire-family rule: the DRY scene
    // extinguishes while it rains — for the fire-block consumer AND the
    // campfire consumer (one predicate, the two call sites; the mob's
    // effect consumer is the third case, test_mob.cpp). Rain never
    // unwets a water-extinguished cell (the disjunction), and the rain
    // frame is the same one the world sees (no roof-shade check — the
    // documented deviation).
    World world;
    world.getOrCreateChunk({0, 0, 0});
    const BlockPos pos {8, 1, 8};
    EXPECT_FALSE(extinguishedByWaterOrRain(world, pos));
    EXPECT_TRUE(extinguishedByWaterOrRain(world, pos, true));
    world.setFluid(9, 1, 8, FluidKind::Water, 8);
    EXPECT_TRUE(extinguishedByWaterOrRain(world, pos, false));
    EXPECT_TRUE(extinguishedByWaterOrRain(world, pos, true));
    world.setFluid(9, 1, 8, FluidKind::Lava, 8);
    EXPECT_TRUE(extinguishedByWaterOrRain(world, pos, true));
    EXPECT_FALSE(extinguishedByWaterOrRain(world, pos, false));
}

// ---------------------------------------------------------------------------
// Group 6: the soilIsWet raining variant (+ the growth pace release).
// ---------------------------------------------------------------------------

TEST(WeatherSoilTest, RainWetsDrySoil) {
    World world;
    world.getOrCreateChunk({0, 0, 0});
    const BlockPos soil {8, 1, 8};
    // The default (and explicit false) = the pre-V2-4 dry semantics.
    EXPECT_FALSE(soilIsWet(world, soil));
    EXPECT_FALSE(soilIsWet(world, soil, false));
    // Rain wets every soil (no water present, no roof-shade check).
    EXPECT_TRUE(soilIsWet(world, soil, true));
    // The water source still wets (the disjunction both ways).
    world.setFluid(9, 1, 8, FluidKind::Water, 8);
    EXPECT_TRUE(soilIsWet(world, soil, false));
    EXPECT_TRUE(soilIsWet(world, soil, true));
    world.setFluid(9, 1, 8, FluidKind::Water, 0);
    EXPECT_FALSE(soilIsWet(world, soil, false));
    EXPECT_TRUE(soilIsWet(world, soil, true));
    // cropIsWet delegates with the raining extension (the −1y soil cell).
    const BlockPos crop {8, 2, 8};
    EXPECT_FALSE(cropIsWet(world, crop));
    EXPECT_FALSE(cropIsWet(world, crop, false));
    EXPECT_TRUE(cropIsWet(world, crop, true));
}

TEST(WeatherSoilTest, RainReleasesTheDrySlowdown) {
    // The pace contract (growth_ticks 1 + dry_slowdown 2): the dry crop
    // rolls at modulus 2 (111 of 200 frames pass, the deterministic
    // pin), the raining crop at the FULL pace (every frame — the 2x
    // release, the design's "雨中生长 2×"). The default argument is the
    // dry semantics bit-identical; with water present the raining flag
    // adds nothing (the disjunction — the already-wet pace is full).
    const BlockRegistry reg = paceRegistry();
    World world;
    world.getOrCreateChunk({0, 0, 0});
    world.setBlock(8, 2, 8, reg.idForName("crop_a"));
    world.setBlock(8, 1, 8, reg.idForName("soil"));
    std::map<BlockPos, BlockID> crops;
    crops[{8, 2, 8}] = reg.idForName("crop_a");
    int dry = 0, raining = 0, def = 0;
    for (int f = 0; f < 200; ++f) {
        dry += cropGrowthSteps(world, reg, crops, 42,
                               static_cast<std::uint64_t>(f)).size();
        raining += cropGrowthSteps(world, reg, crops, 42,
                                   static_cast<std::uint64_t>(f), true)
                       .size();
        def += cropGrowthSteps(world, reg, crops, 42,
                               static_cast<std::uint64_t>(f)).size();
    }
    EXPECT_EQ(raining, 200); // the full pace (every frame passes)
    EXPECT_EQ(dry, 111);     // the pinned dry-pace count (modulus 2)
    EXPECT_EQ(def, dry);     // the default argument = the old semantics
    // The disjunction: water present + raining = the water's pace
    // (unchanged by the flag).
    world.setFluid(10, 1, 8, FluidKind::Water, 8);
    int waterRaining = 0;
    for (int f = 0; f < 200; ++f) {
        waterRaining += cropGrowthSteps(world, reg, crops, 42,
                                        static_cast<std::uint64_t>(f), true)
                            .size();
    }
    EXPECT_EQ(waterRaining, 200);
}

// ---------------------------------------------------------------------------
// The thunder pulse (the presentation pure function).
// ---------------------------------------------------------------------------

TEST(ThunderPulseTest, FiveFrameLinearDecay) {
    // +0.15 on the strike frame, linear decay over 5 frames, 0 before /
    // after (the 4-ULP EXPECT_FLOAT_EQ — the (k-n)/k * level reduction's
    // rounding at ages 1..4 sits within 4 ulps of the decimal pins;
    // age 0 is exact (1.0 * level) and the zeros are exact).
    EXPECT_FLOAT_EQ(thunderPulse(100, -1), 0.0f); // no strike yet
    EXPECT_FLOAT_EQ(thunderPulse(99, 100), 0.0f); // before the event
    EXPECT_FLOAT_EQ(thunderPulse(100, 100), 0.15f);
    EXPECT_FLOAT_EQ(thunderPulse(101, 100), 0.12f);
    EXPECT_FLOAT_EQ(thunderPulse(102, 100), 0.09f);
    EXPECT_FLOAT_EQ(thunderPulse(103, 100), 0.06f);
    EXPECT_FLOAT_EQ(thunderPulse(104, 100), 0.03f);
    EXPECT_FLOAT_EQ(thunderPulse(105, 100), 0.0f); // the window closed
    EXPECT_FLOAT_EQ(thunderPulse(1000000, 100), 0.0f);
    // Monotone decay within the window.
    EXPECT_GT(thunderPulse(100, 100), thunderPulse(101, 100));
    EXPECT_GT(thunderPulse(101, 100), thunderPulse(102, 100));
    EXPECT_GT(thunderPulse(102, 100), thunderPulse(103, 100));
    EXPECT_GT(thunderPulse(103, 100), thunderPulse(104, 100));
    EXPECT_GT(thunderPulse(104, 100), 0.0f);
}

// ---------------------------------------------------------------------------
// Group 7: the render.weather strict read.
// ---------------------------------------------------------------------------

TEST(WeatherConfigTest, MissingKeyDefaultsToOn) {
    // The key is absent from config.json by design — the code default
    // (true) applies (the render.clouds precedent).
    Config cfg;
    auto r = weatherEnabled(cfg);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_TRUE(*r);
}

TEST(WeatherConfigTest, ExplicitFalseTurnsOff) {
    Config cfg;
    cfg.set("render.weather", Json(false));
    auto r = weatherEnabled(cfg);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_FALSE(*r);
}

TEST(WeatherConfigTest, NonBoolNamed) {
    // The strict read (the SAVE-1 discipline): a present non-boolean is
    // NAMED and rejected at init (the Config getters' silent fallback is
    // the trap the strict read exists to avoid).
    Config cfg;
    cfg.set("render.weather", Json("yes"));
    auto r = weatherEnabled(cfg);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("render.weather"), std::string::npos);
    cfg.set("render.weather", Json(1));
    auto r2 = weatherEnabled(cfg);
    ASSERT_FALSE(r2.ok());
    EXPECT_NE(r2.error().message.find("render.weather"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Group 8: the --weather CLI word validation.
// ---------------------------------------------------------------------------

TEST(WeatherCliTest, WordsPinned) {
    // The closed word (the parseDifficulty precedent): "clear" / "rain"
    // accepted, the unknown value named and rejected.
    auto clear = parseWeather("clear");
    ASSERT_TRUE(clear.ok()) << clear.error().message;
    EXPECT_FALSE(*clear);
    auto rain = parseWeather("rain");
    ASSERT_TRUE(rain.ok()) << rain.error().message;
    EXPECT_TRUE(*rain);
    auto fog = parseWeather("fog");
    ASSERT_FALSE(fog.ok());
    EXPECT_NE(fog.error().message.find("fog"), std::string::npos);
    EXPECT_NE(fog.error().message.find("clear"), std::string::npos);
    EXPECT_NE(fog.error().message.find("rain"), std::string::npos);
    auto empty = parseWeather("");
    ASSERT_FALSE(empty.ok());
    EXPECT_NE(empty.error().message.find("--weather"), std::string::npos);
}

// ---------------------------------------------------------------------------
// The thunder wav asset (the ambience asset pattern — the fixed-seed
// generator; the existing 39 wavs' byte-identity is the build-time md5
// evidence, the CRAFT-3 discipline).
// ---------------------------------------------------------------------------

TEST(WeatherAssetsTest, ThunderWavExistsAndParses) {
    std::vector<std::uint8_t> data;
    ASSERT_TRUE(readFile("assets/sounds/thunder.wav", data))
        << "run from the repo root: thunder.wav";
    auto r = SoundBank::parseWav(data);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(r->rate, 44100);
    const double seconds = static_cast<double>(r->pcm.size()) / 44100.0;
    // The ~3.5 s roll (the 2-6 s one-shot band).
    EXPECT_GE(seconds, 2.0);
    EXPECT_LE(seconds, 6.0);
    // A thunder is never silent: audible content in the middle.
    bool audible = false;
    for (std::size_t i = r->pcm.size() / 4; i < r->pcm.size() * 3 / 4;
         ++i) {
        if (std::abs(static_cast<int>(r->pcm[i])) > 32) {
            audible = true;
            break;
        }
    }
    EXPECT_TRUE(audible) << "thunder.wav is silent in its middle";
}
