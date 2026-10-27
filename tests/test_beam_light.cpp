// LGT-4: the beacon-beam light sampler (beamLightSample) — the cell
// selection (the air cell directly ABOVE the beacon block, (x, y+1, z),
// the beam's lowest cell — the beacon's own cell is opaque and always
// reads (0, 0), the gate 119 E2E defect), the 0..1 normalization, the
// World getters' fallbacks (void, below the world), day-independence
// (the function takes NO dayFactor — the fragment's existing formula
// applies the day/night scale) and purity (the per-frame resample is
// bit-identical). The per-draw override wiring (u_useLightOverride set +
// reset around each beam draw) is Game-level GL state: it is pinned by
// the E2E ×2 RAW runs (a reset leak would relight every subsequent draw —
// the PRE-6 / UXFIX-1 family).

#include <gtest/gtest.h>

#include <utility>

#include "world/beam_light.hpp"
#include "world/chunk.hpp"
#include "world/world.hpp"

using namespace mc;

namespace {

// A world with chunk (0,0,0) only. The beacon is queried at (2, 3, 4);
// the sampler reads the air cell above it, (2, 4, 4).
struct Fixture {
    World world;
    Fixture() { world.getOrCreateChunk({0, 0, 0}); }
    // The beam's sample cell (above the queried beacon cell).
    static constexpr int kSampleY = 4;
    void putSky(int v) {
        const BlockPos p {2, kSampleY, 4};
        const ChunkPos cp = worldToChunk(p);
        const BlockPos local = worldToChunkLocal(cp, p);
        world.getOrCreateChunk(cp).setSkyLightAt(local.x, local.y, local.z,
                                                 v);
    }
    void putBlock(int v) {
        const BlockPos p {2, kSampleY, 4};
        const ChunkPos cp = worldToChunk(p);
        const BlockPos local = worldToChunkLocal(cp, p);
        world.getOrCreateChunk(cp).setBlockLightAt(local.x, local.y, local.z,
                                                   v);
    }
};

// Open sky: the cell above the beacon holds full sky light, no block
// light — at day factor 1 the fragment's formula keeps the beam at the
// legacy constant-white value (the noon identity).
TEST(BeamLightSampleTest, OpenSkyFull) {
    Fixture f;
    f.putSky(15);
    const auto light = beamLightSample(f.world, 2, 3, 4);
    EXPECT_FLOAT_EQ(light.first, 1.0f);
    EXPECT_FLOAT_EQ(light.second, 0.0f);
}

// Glowstone adjacency: the cell above the beacon holds full block light,
// no sky — the fragment keeps block light un-reduced (the
// glowstone-shines-at-night discipline), so the beam is bright at deep
// night next to a glowstone.
TEST(BeamLightSampleTest, GlowstoneFullBlock) {
    Fixture f;
    f.putBlock(15);
    const auto light = beamLightSample(f.world, 2, 3, 4);
    EXPECT_FLOAT_EQ(light.first, 0.0f);
    EXPECT_FLOAT_EQ(light.second, 1.0f);
}

// A fully dark sample cell (cave interior above the beacon, no sources):
// (0, 0) — the fragment's ambient floor keeps the beam distinguishable
// from the void.
TEST(BeamLightSampleTest, FullDark) {
    Fixture f;
    const auto light = beamLightSample(f.world, 2, 3, 4);
    EXPECT_FLOAT_EQ(light.first, 0.0f);
    EXPECT_FLOAT_EQ(light.second, 0.0f);
}

// The OPAQUE-CELL REGRESSION (the gate 119 defect): a beacon whose OWN
// cell carries light reads (0, 0) — the sampler looks ABOVE the block,
// never at it. An open-sky world above a lit beacon cell is (1, 0), not
// the cell's stored values.
TEST(BeamLightSampleTest, OwnCellOpaqueAlwaysIgnored) {
    Fixture f;
    // The beacon's own cell (2, 3, 4) — whatever is stored there is
    // never read.
    const BlockPos own {2, 3, 4};
    const ChunkPos cp = worldToChunk(own);
    const BlockPos local = worldToChunkLocal(cp, own);
    auto& chunk = f.world.getOrCreateChunk(cp);
    chunk.setSkyLightAt(local.x, local.y, local.z, 15);
    chunk.setBlockLightAt(local.x, local.y, local.z, 15);
    // Empty air above -> (0, 0).
    EXPECT_FLOAT_EQ(beamLightSample(f.world, 2, 3, 4).first, 0.0f);
    EXPECT_FLOAT_EQ(beamLightSample(f.world, 2, 3, 4).second, 0.0f);
    // Open sky above -> (1, 0), regardless of the own-cell 15/15.
    f.putSky(15);
    const auto light = beamLightSample(f.world, 2, 3, 4);
    EXPECT_FLOAT_EQ(light.first, 1.0f);
    EXPECT_FLOAT_EQ(light.second, 0.0f);
}

// Normalization: the exact 0..15/15 values (sky 7 -> 7/15, block 3 ->
// 3/15) at the sample cell.
TEST(BeamLightSampleTest, ExactNormalizedValues) {
    Fixture f;
    f.putSky(7);
    f.putBlock(3);
    const auto light = beamLightSample(f.world, 2, 3, 4);
    EXPECT_FLOAT_EQ(light.first, 7.0f / 15.0f);
    EXPECT_FLOAT_EQ(light.second, 3.0f / 15.0f);
}

// The World getters' fallbacks: a sample cell whose chunk was never
// created (void around the world) reads open sky (15) / no block light
// (0); a sample cell below the world (beacon at y = -2 -> sample y = -1)
// reads 0 / 0.
TEST(BeamLightSampleTest, WorldFallbacks) {
    Fixture f; // only chunk (0,0,0) created
    auto light = beamLightSample(f.world, 100, 8, 100); // void above
    EXPECT_FLOAT_EQ(light.first, 1.0f);
    EXPECT_FLOAT_EQ(light.second, 0.0f);
    light = beamLightSample(f.world, 2, -2, 4); // below the world
    EXPECT_FLOAT_EQ(light.first, 0.0f);
    EXPECT_FLOAT_EQ(light.second, 0.0f);
}

// Day-independence: the function takes NO dayFactor — the raw light is
// returned and the fragment's existing formula (max(v_skyLight -
// (8.0/15.0) * (1.0 - u_dayFactor), v_blockLight), the LGT-3 affine
// retreat with the moonlight floor) applies the day/night reduction, so
// a beam brightens and dims with the world exactly like the chunks
// (moonlit at deep night, never all black). Calling it twice on an
// unchanged world is bit-identical (the per-frame resample is pure).
TEST(BeamLightSampleTest, DayIndependentAndPure) {
    Fixture f;
    f.putSky(9);
    f.putBlock(4);
    const auto a = beamLightSample(f.world, 2, 3, 4);
    const auto b = beamLightSample(f.world, 2, 3, 4);
    EXPECT_FLOAT_EQ(a.first, b.first);
    EXPECT_FLOAT_EQ(a.second, b.second);
    EXPECT_FLOAT_EQ(a.first, 9.0f / 15.0f);
    EXPECT_FLOAT_EQ(a.second, 4.0f / 15.0f);
}

} // namespace
