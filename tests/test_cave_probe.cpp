#include <gtest/gtest.h>

#include <cmath>
#include <string>

#include "cave_probe.hpp"

using namespace mc;

// ---------------------------------------------------------------------------
// GEN-4 (cave fragmentation): probe statistics correctness + the calibrated
// final-vs-legacy pin. The heavy 9-combo calibration table lives in the
// standalone driver (tests/cave_probe_main.cpp → build/bin/cave_probe); this
// file pins the pure statistics functions and the shipped final parameters
// against the legacy GEN-1 tuning.
// ---------------------------------------------------------------------------

namespace {

} // namespace

// -- pure statistics functions (no world generation) -----------------------

TEST(CaveStatsTest, HandcraftedComponentCounting) {
    // A 5x5x5 grid with four hand-placed components:
    //   A = a 2x2x2 cube (8 cells, x/y/z in [0,1] — the small-volume
    //       BOUNDARY: exactly 8, NOT counted as a pocket);
    //   B = a 7-cell face-connected L along the far x edge (a pocket);
    //   C = one isolated cell;
    //   D = a 3-cell chain connected ONLY through corners (proves the
    //       26-neighbour — not 6-neighbour — connectivity).
    CaveSample s;
    s.nx = 5;
    s.ny = 5;
    s.nz = 5;
    s.carved.assign(125, 0);
    const auto set = [&](int x, int y, int z) {
        s.carved[s.index(x, y, z)] = 1;
    };
    for (int x = 0; x <= 1; ++x) {
        for (int y = 0; y <= 1; ++y) {
            for (int z = 0; z <= 1; ++z) {
                set(x, y, z); // A: 8 cells
            }
        }
    }
    set(4, 0, 0);
    set(4, 0, 1);
    set(4, 0, 2);
    set(4, 0, 3);
    set(4, 0, 4);
    set(4, 1, 4);
    set(4, 2, 4); // B: 7 cells, face-connected
    set(2, 4, 0); // C: 1 isolated cell
    set(0, 4, 2);
    set(1, 3, 3);
    set(2, 2, 4); // D: 3 cells, corner-only chain
    s.bandCells = 50;
    s.carvedCells = 19;
    s.surfaceOpenings = 2;

    const CaveStats st = caveStats(s);
    EXPECT_EQ(st.bandCells, 50);
    EXPECT_EQ(st.carvedCells, 19);
    EXPECT_DOUBLE_EQ(st.carveRate, 0.38);
    EXPECT_EQ(st.componentCount, 4);
    // Sorted volumes {1, 3, 7, 8} → p50 = v[2] = 7, p90 = v[3] = 8.
    EXPECT_DOUBLE_EQ(st.volumeP50, 7.0);
    EXPECT_DOUBLE_EQ(st.volumeP90, 8.0);
    EXPECT_EQ(st.volumeMax, 8);
    // Pockets = the cells of components with < 8 cells: B (7) + C (1) +
    // D (3) = 11. A (exactly 8) is NOT a pocket (strict inequality).
    EXPECT_EQ(st.smallCells, 11);
    EXPECT_DOUBLE_EQ(st.smallCellFraction, 11.0 / 19.0);
    EXPECT_EQ(st.surfaceOpenings, 2);
}

TEST(CaveStatsTest, EmptyAndFullGrids) {
    CaveSample empty;
    empty.nx = 2;
    empty.ny = 2;
    empty.nz = 2;
    empty.carved.assign(8, 0);
    empty.bandCells = 8;
    const CaveStats e = caveStats(empty);
    EXPECT_EQ(e.carvedCells, 0);
    EXPECT_DOUBLE_EQ(e.carveRate, 0.0);
    EXPECT_EQ(e.componentCount, 0);
    EXPECT_DOUBLE_EQ(e.volumeP50, 0.0);
    EXPECT_DOUBLE_EQ(e.volumeP90, 0.0);
    EXPECT_EQ(e.volumeMax, 0);
    EXPECT_EQ(e.smallCells, 0);
    EXPECT_DOUBLE_EQ(e.smallCellFraction, 0.0);

    CaveSample full;
    full.nx = 2;
    full.ny = 2;
    full.nz = 2;
    full.carved.assign(8, 1);
    full.bandCells = 8;
    full.carvedCells = 8;
    const CaveStats f = caveStats(full);
    EXPECT_DOUBLE_EQ(f.carveRate, 1.0);
    EXPECT_EQ(f.componentCount, 1);
    EXPECT_DOUBLE_EQ(f.volumeP50, 8.0);
    EXPECT_DOUBLE_EQ(f.volumeP90, 8.0);
    EXPECT_EQ(f.volumeMax, 8);
    EXPECT_EQ(f.smallCells, 0); // 8 is NOT < 8
    EXPECT_DOUBLE_EQ(f.smallCellFraction, 0.0);
}

// -- the calibrated final-vs-legacy pin (REAL assets) -----------------------
//
// Single-round calibration (GEN-4, zero-tuning-round discipline): the full
// 9-combo grid (cs {0.09,0.07,0.06,0.05} x t {0.06,0.07,0.08} x seeds
// {123456789,9,42}, + seed 7 spot check) was probed with build/bin/cave_probe
// on the standard 32x32 sample and a 96x96 cross-check sample. Selection
// (the design's rule, applied on the MULTI-SEED aggregate — a single seed
// swings the carve rate by +/-1.5pp, so the 4-6% density band is a
// world-level target, not a per-seed one): carve rate in [4%, 6%] + lowest
// pocket share + fewest components. The 96x96 aggregate picks
// cs 0.05 / t 0.06 (the 32x32 aggregate agrees):
//
//   metric (96x96, 4 seeds aggregate)        legacy 0.09/0.06   final 0.05/0.06
//   carve rate                               5.82%              5.12%  (in band)
//   components per sample                    27/35/21/34        18/6/7/8
//   median component volume (cells)          42/12/9/28         392/1554/412/457
//   <8-cell pocket cells (the user's pain)   25/42/23/20        6/0/3/1
//
// i.e. ~12% fewer carved cells, ~3x fewer components, a 9-130x larger
// MEDIAN cave, and ~91% fewer pocket cells — "fewer, bigger caves".
// t stays at 0.06 (the legacy value): the pocket-merging knob was not
// needed once cs is coarse, and it keeps the n2 short-circuit rate (and
// the carve density) at the legacy level.

namespace {

// The LEGACY GEN-1 tuning (the pre-GEN-4 shipped values).
constexpr float kGen4LegacyCs = 0.09f;
constexpr float kGen4LegacyT = 0.06f;
// The cross-check sample (96x96 = 6x6 chunks per layer; the standard
// kCaveSampleCols = 32 sample is also pinned below).
constexpr int kGen4LargeCols = 96;

} // namespace

TEST(CaveProbeTest, Gen4ShippedFinalBeatsLegacy) {
    CaveProbeWorld w;
    ASSERT_TRUE(w.load()) << "run the tests from the repository root";
    // The SHIPPED worldgen.json cave values = the final (loaded, not
    // hardcoded here — the content pin in test_worldgen_config.cpp).
    const float cs = w.params().caveNoiseScale;
    const float t = w.params().caveTubeThreshold;
    const std::int64_t seed = 123456789; // the GEN-1 calibration seed

    auto finalA = w.probe(cs, t, seed, kCaveSampleX0, kCaveSampleZ0,
                          kGen4LargeCols);
    auto finalB = w.probe(cs, t, seed, kCaveSampleX0, kCaveSampleZ0,
                          kGen4LargeCols);
    auto legacy = w.probe(kGen4LegacyCs, kGen4LegacyT, seed, kCaveSampleX0,
                          kCaveSampleZ0, kGen4LargeCols);
    ASSERT_TRUE(finalA.ok()) << finalA.error().message;
    ASSERT_TRUE(finalB.ok()) << finalB.error().message;
    ASSERT_TRUE(legacy.ok()) << legacy.error().message;

    // Determinism: the same (params, seed) → identical statistics.
    EXPECT_EQ(finalA->bandCells, finalB->bandCells);
    EXPECT_EQ(finalA->carvedCells, finalB->carvedCells);
    EXPECT_EQ(finalA->componentCount, finalB->componentCount);
    EXPECT_DOUBLE_EQ(finalA->carveRate, finalB->carveRate);
    EXPECT_DOUBLE_EQ(finalA->volumeP50, finalB->volumeP50);
    EXPECT_DOUBLE_EQ(finalA->volumeP90, finalB->volumeP90);
    EXPECT_EQ(finalA->volumeMax, finalB->volumeMax);
    EXPECT_EQ(finalA->smallCells, finalB->smallCells);
    EXPECT_DOUBLE_EQ(finalA->smallCellFraction, finalB->smallCellFraction);
    EXPECT_EQ(finalA->surfaceOpenings, finalB->surfaceOpenings);

    // The pinned final numbers (96x96, seed 123456789 — from the
    // calibration table; the exact-pin guards against parameter drift
    // long before it moves the world meaningfully). GEN-8 re-pin: the
    // mountain passes raise a few columns, so the per-column carve band
    // [caveMinY, height+guard) grows and the carve set shifts slightly —
    // every DIRECTION pin below still holds exactly.
    EXPECT_EQ(finalA->bandCells, 353587);
    EXPECT_EQ(finalA->carvedCells, 12216);
    EXPECT_DOUBLE_EQ(finalA->carveRate, 12216.0 / 353587.0);
    EXPECT_EQ(finalA->componentCount, 18);
    EXPECT_DOUBLE_EQ(finalA->volumeP50, 419.0);
    EXPECT_DOUBLE_EQ(finalA->volumeP90, 2385.0);
    EXPECT_EQ(finalA->volumeMax, 2762);
    EXPECT_EQ(finalA->smallCells, 6);
    EXPECT_EQ(finalA->surfaceOpenings, 408);

    // The calibration conclusion (final vs legacy, same seed + sample):
    EXPECT_LT(finalA->smallCellFraction, legacy->smallCellFraction);
    EXPECT_LT(finalA->componentCount, legacy->componentCount);
    EXPECT_LT(finalA->carveRate, legacy->carveRate);
    EXPECT_GT(finalA->volumeP50, legacy->volumeP50);
    // (The 4-6% density band is a WORLD-level target: a single seed
    // swings the rate by +/-1pp, so it is pinned on the multi-seed
    // aggregate in Gen4DirectionHoldsAcrossSeeds, not per seed.)
    // Legacy reference numbers (the pre-GEN-4 behaviour, pinned so the
    // comparison above cannot silently drift). GEN-8 re-pin: the raised
    // columns grow the band the legacy tuning carves, shifting its
    // component count 27 → 26 (the direction pins above unchanged).
    EXPECT_EQ(legacy->carvedCells, 18914);
    EXPECT_EQ(legacy->componentCount, 26);
    EXPECT_EQ(legacy->smallCells, 25);
    EXPECT_DOUBLE_EQ(legacy->volumeP50, 42.0);
}

TEST(CaveProbeTest, Gen4DirectionHoldsAcrossSeeds) {
    // All three calibration seeds (96x96): the final beats the legacy on
    // EVERY metric, per seed (not just the aggregate) — and the multi-
    // seed aggregate density sits in the 4-6% design band (a single seed
    // swings the rate by +/-1pp, so the band is a world-level target).
    CaveProbeWorld w;
    ASSERT_TRUE(w.load()) << "run the tests from the repository root";
    const float cs = w.params().caveNoiseScale;
    const float t = w.params().caveTubeThreshold;
    // GEN-8 re-pin: the mountain passes raise a few columns (the band
    // grows, the carve sets shift) — the per-seed DIRECTION pins below
    // all still hold exactly.
    const struct {
        std::int64_t seed;
        int finalCarved, finalComps, finalSmall;
        double finalP50;
        int legacyCarved, legacyComps, legacySmall;
    } want[] = {
        {123456789, 12216, 18, 6, 419.0, 18914, 26, 25},
        {9, 19408, 6, 0, 1554.0, 22022, 35, 38},
        {42, 18531, 7, 3, 412.0, 20734, 19, 25},
    };
    long finalCarved = 0, finalBand = 0;
    long legacyCarved = 0;
    for (const auto& row : want) {
        auto finalR = w.probe(cs, t, row.seed, kCaveSampleX0, kCaveSampleZ0,
                              kGen4LargeCols);
        auto legacyR = w.probe(kGen4LegacyCs, kGen4LegacyT, row.seed,
                               kCaveSampleX0, kCaveSampleZ0, kGen4LargeCols);
        ASSERT_TRUE(finalR.ok()) << finalR.error().message;
        ASSERT_TRUE(legacyR.ok()) << legacyR.error().message;
        const CaveStats& f = *finalR;
        const CaveStats& l = *legacyR;
        EXPECT_EQ(f.carvedCells, row.finalCarved) << "seed " << row.seed;
        EXPECT_EQ(f.componentCount, row.finalComps) << "seed " << row.seed;
        EXPECT_EQ(f.smallCells, row.finalSmall) << "seed " << row.seed;
        EXPECT_DOUBLE_EQ(f.volumeP50, row.finalP50) << "seed " << row.seed;
        EXPECT_EQ(l.carvedCells, row.legacyCarved) << "seed " << row.seed;
        EXPECT_EQ(l.componentCount, row.legacyComps) << "seed " << row.seed;
        EXPECT_EQ(l.smallCells, row.legacySmall) << "seed " << row.seed;
        EXPECT_LT(f.smallCells, l.smallCells) << "seed " << row.seed;
        EXPECT_LT(f.componentCount, l.componentCount) << "seed " << row.seed;
        EXPECT_LT(f.carveRate, l.carveRate) << "seed " << row.seed;
        finalCarved += f.carvedCells;
        finalBand += f.bandCells;
        legacyCarved += l.carvedCells;
    }
    // The pinned 3-seed aggregates (the 96x96 calibration table):
    // final = 50155 carved in a 1045910-cell band (4.80% — in band),
    // legacy = 61670 (5.90%). GEN-8 re-pin: the mountain passes RAISE a
    // few columns (the band is per-column [caveMinY, height+guard)) so
    // the band grows ~11.5k cells and both carve sets shift up slightly
    // (the direction pins above all still hold exactly).
    EXPECT_EQ(finalBand, 1045910);
    EXPECT_EQ(finalCarved, 50155);
    EXPECT_EQ(legacyCarved, 61670);
    const double finalRate = static_cast<double>(finalCarved) / finalBand;
    const double legacyRate = static_cast<double>(legacyCarved) / finalBand;
    EXPECT_GE(finalRate, 0.04);
    EXPECT_LE(finalRate, 0.06);
    EXPECT_LT(finalRate, legacyRate);
}

TEST(CaveProbeTest, Gen4DesignSample32Aggregate) {
    // The design's standard 32x32 sample, aggregated over the three
    // calibration seeds: the final's density is in the 4-6% band and its
    // fragmentation is strictly lower than the legacy's.
    CaveProbeWorld w;
    ASSERT_TRUE(w.load()) << "run the tests from the repository root";
    const float cs = w.params().caveNoiseScale;
    const float t = w.params().caveTubeThreshold;
    const std::int64_t seeds[] = {123456789, 9, 42};
    long finalCarved = 0, finalSmall = 0, finalComps = 0;
    long legacyCarved = 0, legacySmall = 0, legacyComps = 0;
    long band = 0;
    for (const std::int64_t seed : seeds) {
        auto f = w.probe(cs, t, seed);
        auto l = w.probe(kGen4LegacyCs, kGen4LegacyT, seed);
        ASSERT_TRUE(f.ok()) << f.error().message;
        ASSERT_TRUE(l.ok()) << l.error().message;
        band += f->bandCells;
        EXPECT_EQ(f->bandCells, l->bandCells)
            << "the band is a terrain function (seed-independent per seed)";
        finalCarved += f->carvedCells;
        finalSmall += f->smallCells;
        finalComps += f->componentCount;
        legacyCarved += l->carvedCells;
        legacySmall += l->smallCells;
        legacyComps += l->componentCount;
    }
    // Pinned aggregates (the 32x32 calibration table). GEN-8 re-pin:
    // the raised columns grow the band (~+1.5k cells) and shift both
    // carve sets up slightly; the final density 4255/105219 = 4.04%
    // stays in the 4-6% design band (at its lower edge).
    EXPECT_EQ(band, 105219);
    EXPECT_EQ(finalCarved, 4255);
    EXPECT_EQ(finalSmall, 2);
    EXPECT_EQ(finalComps, 8);
    EXPECT_EQ(legacyCarved, 5746);
    EXPECT_EQ(legacySmall, 10);
    EXPECT_EQ(legacyComps, 22);
    const double finalRate = static_cast<double>(finalCarved) / band;
    EXPECT_GE(finalRate, 0.04);
    EXPECT_LE(finalRate, 0.06);
    EXPECT_LT(static_cast<double>(finalSmall) / finalCarved,
              static_cast<double>(legacySmall) / legacyCarved);
}
