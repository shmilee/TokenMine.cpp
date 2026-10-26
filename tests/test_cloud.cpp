// PRE-4: the cloud layer's CPU-side contracts — the presence field
// (world/cloud's pure cloudMask: determinism pins, the 50% density band,
// the time-step drift) and the mesh builders (the full-window quad count,
// the y = kCloudHeight pin, the pure dirty flag), plus the render.clouds
// config key semantics (code default true, the render.fog /
// fire.particles getBool precedent — config.json is untouched). The GPU
// half (the unlit-white quads in the blending pass) is exercised by the
// gate's re-anchor protocol; the frame-locked step is a pure function of
// m_frameCount (hard rule 8), so same-frame-count runs are byte-identical
// without a separate unit seam.

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>

#include "core/config.hpp"
#include "world/cloud.hpp"

using mc::buildCloudMesh;
using mc::cloudCellVertices;
using mc::cloudMask;
using mc::cloudMeshNeedsRebuild;
using mc::cloudTimeStep;

// The presence field is a pure function — the pins (the CRAFT-3 content-
// pin pattern): any drift in the hash (salt, advance, finalizer) is a
// contract break, not a "different sky".
TEST(CloudMaskTest, DeterministicPins) {
    EXPECT_TRUE(mc::cloudMask(0, 0, 0));
    EXPECT_TRUE(mc::cloudMask(1, 0, 0));
    EXPECT_FALSE(mc::cloudMask(0, 1, 0));
    EXPECT_FALSE(mc::cloudMask(1, 1, 0));
    EXPECT_TRUE(mc::cloudMask(-1, 0, 0)); // negative cells (floorDiv world)
    EXPECT_TRUE(mc::cloudMask(2, -3, 0));
    // The same arguments, called again — the cross-run identity the
    // ×2 RAW protocol relies on.
    EXPECT_EQ(mc::cloudMask(0, 0, 0), mc::cloudMask(0, 0, 0));
    EXPECT_EQ(mc::cloudMask(-1, 0, 0), mc::cloudMask(-1, 0, 0));
}

// The t drift moves the field: t=1 flips the two pinned cells (the PITS
// #4 advance is live — a constant-input finalizer would keep them).
TEST(CloudMaskTest, TimeStepMovesTheField) {
    EXPECT_FALSE(mc::cloudMask(0, 0, 1));
    EXPECT_TRUE(mc::cloudMask(1, 0, 1));
}

// The 50% density: over a 128x128 grid (16384 cells) the fraction of
// cloud cells sits in a tight band around 0.5 (the splitmix64 top bit).
TEST(CloudMaskTest, DensityBand) {
    int n = 0;
    for (int z = -64; z < 64; ++z) {
        for (int x = -64; x < 64; ++x) {
            if (mc::cloudMask(x, z, 0)) {
                ++n;
            }
        }
    }
    const double density = static_cast<double>(n) / 16384.0;
    EXPECT_GT(density, 0.45);
    EXPECT_LT(density, 0.55);
}

// The t=0 vs t=1 sets differ (neither identical nor inverted): the
// agree count over 4096 cells sits near half (two independent 50%
// fields agree ~50% of the time).
TEST(CloudMaskTest, DifferentTDifferentSets) {
    int agree = 0;
    for (int z = -32; z < 32; ++z) {
        for (int x = -32; x < 32; ++x) {
            if (mc::cloudMask(x, z, 0) == mc::cloudMask(x, z, 1)) {
                ++agree;
            }
        }
    }
    EXPECT_GT(agree, 1500);
    EXPECT_LT(agree, 2500);
}

// cloudTimeStep: the frame-locked drift — monotone non-decreasing with
// the exact period (6000 frames per cell step, the integer division).
TEST(CloudTimeStepTest, MonotoneAndPeriod) {
    EXPECT_EQ(mc::cloudTimeStep(0), 0);
    EXPECT_EQ(mc::cloudTimeStep(5999), 0);
    EXPECT_EQ(mc::cloudTimeStep(6000), 1);
    EXPECT_EQ(mc::cloudTimeStep(11999), 1);
    EXPECT_EQ(mc::cloudTimeStep(12000), 2);
    int prev = 0;
    for (int f = 0; f <= 13000; f += 997) { // prime stride: no aliasing
        const int t = mc::cloudTimeStep(f);
        EXPECT_GE(t, prev) << "not monotone at frame " << f;
        prev = t;
    }
}

// One cell's quad: exactly 4 vertices in the corner order, every y at
// kCloudHeight, the unlit-white lightmap (skyLight 0 / blockLight 1),
// the +Y normals (both rendered faces get the top-face shade — the beam
// pattern), the zeroed tail fields (the tiling/ripple paths inert).
TEST(CloudCellVerticesTest, GeometryPinned) {
    std::vector<mc::MeshVertex> v;
    mc::cloudCellVertices(3, -2, v);
    ASSERT_EQ(v.size(), 4u);
    // cell (3, -2): x in [48, 64), z in [-32, -16).
    const float ys = static_cast<float>(mc::kCloudHeight);
    EXPECT_FLOAT_EQ(v[0].position.x, 48.0f);
    EXPECT_FLOAT_EQ(v[0].position.z, -32.0f);
    EXPECT_FLOAT_EQ(v[1].position.x, 64.0f);
    EXPECT_FLOAT_EQ(v[1].position.z, -32.0f);
    EXPECT_FLOAT_EQ(v[2].position.x, 64.0f);
    EXPECT_FLOAT_EQ(v[2].position.z, -16.0f);
    EXPECT_FLOAT_EQ(v[3].position.x, 48.0f);
    EXPECT_FLOAT_EQ(v[3].position.z, -16.0f);
    for (const mc::MeshVertex& c : v) {
        EXPECT_FLOAT_EQ(c.position.y, ys);
        EXPECT_FLOAT_EQ(c.normal.x, 0.0f);
        EXPECT_FLOAT_EQ(c.normal.y, 1.0f);
        EXPECT_FLOAT_EQ(c.normal.z, 0.0f);
        EXPECT_FLOAT_EQ(c.skyLight, 0.0f);
        EXPECT_FLOAT_EQ(c.blockLight, 1.0f);
        EXPECT_FLOAT_EQ(c.tiled, 0.0f);
        EXPECT_FLOAT_EQ(c.water, 0.0f);
        EXPECT_FLOAT_EQ(c.uv.x, 0.0f);
        EXPECT_FLOAT_EQ(c.uv.y, 0.0f);
    }
}

// The full window mesh: one quad per cloud cell — 4 vertices + 6 indices
// per cell, in window-scan order. The cloud-cell count is computed
// INDEPENDENTLY from cloudMask (the consistency pin), and every vertex
// sits at kCloudHeight (the "vertex y always 108" contract).
TEST(BuildCloudMeshTest, QuadCountAndHeight) {
    const int camX = 1, camZ = 0; // the spawn-point camera cell
    const int t = 0;
    int cells = 0;
    const int half = mc::kCloudWindowCells / 2;
    for (int dz = -half; dz < half; ++dz) {
        for (int dx = -half; dx < half; ++dx) {
            if (mc::cloudMask(camX + dx, camZ + dz, t)) {
                ++cells;
            }
        }
    }
    EXPECT_GT(cells, 0); // the window holds cloud cells at t=0
    std::vector<mc::MeshVertex> verts;
    std::vector<std::uint32_t> idx;
    mc::buildCloudMesh(camX, camZ, t, verts, idx);
    EXPECT_EQ(verts.size(), 4u * static_cast<std::size_t>(cells));
    EXPECT_EQ(idx.size(), 6u * static_cast<std::size_t>(cells));
    for (const mc::MeshVertex& c : verts) {
        EXPECT_FLOAT_EQ(c.position.y,
                        static_cast<float>(mc::kCloudHeight));
    }
    // The index range is legal (every index addresses a vertex).
    for (std::uint32_t i : idx) {
        EXPECT_LT(i, verts.size());
    }
}

// The pure dirty flag (the rebuild predicate): invalid = rebuild; same
// camera cell + same t = NO rebuild (the no-input ×2 RAW byte-identity
// relies on this — one build on the first draw, then static); any move
// (cellX, cellZ or t) = rebuild.
TEST(CloudMeshNeedsRebuildTest, DirtyFlagContract) {
    EXPECT_TRUE(mc::cloudMeshNeedsRebuild(false, 0, 0, 0, 0, 0, 0));
    EXPECT_FALSE(mc::cloudMeshNeedsRebuild(true, 0, 0, 0, 0, 0, 0));
    EXPECT_TRUE(mc::cloudMeshNeedsRebuild(true, 0, 0, 0, 1, 0, 0));
    EXPECT_TRUE(mc::cloudMeshNeedsRebuild(true, 0, 0, 0, 0, 1, 0));
    EXPECT_TRUE(mc::cloudMeshNeedsRebuild(true, 0, 0, 0, 0, 0, 1));
    EXPECT_FALSE(mc::cloudMeshNeedsRebuild(true, -5, 7, 3, -5, 7, 3));
}

// render.clouds: the key is absent from config.json by design — the code
// default (true) applies (the render.fog precedent).
TEST(CloudsConfigTest, MissingKeyDefaultsToOn) {
    mc::Config cfg;
    EXPECT_TRUE(cfg.getBool("render.clouds", true));
}

// An explicit false turns the clouds off (the gate's "new-off" control:
// the v0.27.0 pipeline, bit-identical by construction).
TEST(CloudsConfigTest, ExplicitFalseTurnsOff) {
    mc::Config cfg;
    cfg.set("render.clouds", mc::Json(false));
    EXPECT_FALSE(cfg.getBool("render.clouds", true));
}
