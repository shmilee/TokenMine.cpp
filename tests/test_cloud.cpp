// PRE-4 + UX-9a + UX-9b: the cloud layer's CPU-side contracts — the
// presence field (world/cloud's pure cloudMask: determinism pins, the
// 50% density band, the time-step drift) and the mesh builders (the
// full-window quad count, the y = kCloudHeight pin, the pure dirty
// flag), plus the render.clouds config key semantics (code default true,
// the render.fog / fire.particles getBool precedent — config.json is
// untouched). The GPU half (the unlit-white quads in the blending pass)
// is exercised by the gate's re-anchor protocol; the frame-locked step
// is a pure function of m_frameCount (hard rule 8), so same-frame-count
// runs are byte-identical without a separate unit seam.
//
// UX-9a: the two-level block grain — cloudSubMask (the 4-block sub-cells
// inside each on region cell, 75% density, world-anchored, same t step)
// and the sub-level mesh (4x4 quads per on sub-cell; the old
// cloudCellVertices output stays bit-identical via the shared emitter).
// The coverage pins come from a probe (regionOn 8268 / subOnTotal 99275
// over the 128x128 window at t=0 — overall coverage 0.3787, the 30-45%
// blocky target band; in-region sub ratio 0.7504).
//
// UX-9b: the third, independent THICKNESS level — cloudSubHeight
// (a uniform {1,2,3} column height for every on sub-cell, 0 for off
// sub-cells, world-anchored, same t step, the dedicated kSaltCloudSubHeight
// lattice). These are the CPU-side contract pins for the pure field:
// the fragment-shader cloud-column port (world-anchored, zero-upload)
// must reproduce these exact values pixel-for-pixel — this file is the
// oracle. The mesh path stays the UX-9a flat sheet.

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/config.hpp"
#include "world/cloud.hpp"
#include "world/coordinates.hpp" // floorDiv

using mc::buildCloudMesh;
using mc::cloudCellVertices;
using mc::cloudMask;
using mc::cloudSubCellVertices;
using mc::cloudSubHeight;
using mc::cloudSubMask;
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

// UX-9a: the sub presence field is a pure function — the pins. The mix
// of true/false at t=0 (incl. negative cells — the asU64 two's-complement
// convention) and the t>0 values (the golden-step advance is live at the
// sub level too) pin the whole hash chain (salt, lattice mapping,
// mod-4 roll).
TEST(CloudSubMaskTest, DeterministicPins) {
    EXPECT_TRUE(mc::cloudSubMask(0, 0, 0, 0, 0));
    EXPECT_TRUE(mc::cloudSubMask(1, 0, 0, 0, 0));
    EXPECT_TRUE(mc::cloudSubMask(0, 1, 0, 0, 0));
    EXPECT_TRUE(mc::cloudSubMask(0, 0, 1, 0, 0));
    EXPECT_TRUE(mc::cloudSubMask(1, 1, 3, 2, 0));
    EXPECT_TRUE(mc::cloudSubMask(-1, 0, 0, 0, 0)); // negative cells
    EXPECT_TRUE(mc::cloudSubMask(2, -3, 0, 0, 0));
    EXPECT_TRUE(mc::cloudSubMask(-5, -7, 3, 3, 0));
    EXPECT_FALSE(mc::cloudSubMask(-20, -20, 1, 0, 0));
    EXPECT_FALSE(mc::cloudSubMask(-20, -20, 2, 3, 0));
    EXPECT_FALSE(mc::cloudSubMask(-19, -20, 0, 2, 0));
    EXPECT_FALSE(mc::cloudSubMask(-19, -20, 3, 3, 0));
    // t > 0 (the probe values):
    EXPECT_TRUE(mc::cloudSubMask(0, 0, 0, 0, 1));
    EXPECT_FALSE(mc::cloudSubMask(1, 0, 0, 0, 1));
    EXPECT_FALSE(mc::cloudSubMask(1, 0, 0, 0, 2));
    EXPECT_TRUE(mc::cloudSubMask(1, 0, 0, 0, 3));
    EXPECT_TRUE(mc::cloudSubMask(-20, -20, 1, 0, 1));
    // The same arguments, called again — the cross-run identity.
    EXPECT_EQ(mc::cloudSubMask(-5, -7, 3, 3, 0), mc::cloudSubMask(-5, -7, 3, 3, 0));
}

// UX-9a: the t drift moves the sub field too (the PITS #4 advance is
// live at the sub level): the pinned cells flip between t steps.
TEST(CloudSubMaskTest, TimeStepMovesTheField) {
    EXPECT_NE(mc::cloudSubMask(1, 0, 0, 0, 0), mc::cloudSubMask(1, 0, 0, 0, 1));
    EXPECT_NE(mc::cloudSubMask(-20, -20, 1, 0, 0), mc::cloudSubMask(-20, -20, 1, 0, 1));
}

// UX-9a: the two-level coverage — region 50% x sub 75% = ~37.5% total
// (the 30-45% blocky target band). Probe-pinned exact values over the
// 128x128 region window at t=0 (x, z in [-64, 64)): regionOn = 8268,
// subOnTotal = 99275 (the sub level only rolls inside on regions — an
// off region contributes zero sub cells).
TEST(CloudSubMaskTest, TwoLevelCoveragePinned) {
    long regionOn = 0;
    long subOnTotal = 0;
    for (int z = -64; z < 64; ++z) {
        for (int x = -64; x < 64; ++x) {
            if (!mc::cloudMask(x, z, 0)) {
                continue;
            }
            ++regionOn;
            for (int sz = 0; sz < 4; ++sz) {
                for (int sx = 0; sx < 4; ++sx) {
                    if (mc::cloudSubMask(x, z, sx, sz, 0)) {
                        ++subOnTotal;
                    }
                }
            }
        }
    }
    EXPECT_EQ(regionOn, 8268L);
    EXPECT_EQ(subOnTotal, 99275L);
    // Overall coverage = sub-on / all sub cells (262144 = 128*128*16).
    const double coverage = static_cast<double>(subOnTotal) / 262144.0;
    EXPECT_GE(coverage, 0.30);
    EXPECT_LE(coverage, 0.45);
    // The in-region sub ratio = 75% band (the mod-4 < 3 roll).
    const double inRegion =
        static_cast<double>(subOnTotal) / (regionOn * 16);
    EXPECT_GE(inRegion, 0.70);
    EXPECT_LE(inRegion, 0.80);
}

// UX-9a: decorrelation — no hash collapse. Over the same 128x128
// window (t=0), each of the 16 (sx, sz) sub positions is on at a ratio
// strictly inside (0.5, 1.0) of the ON-REGION cells (a collapsed hash
// would pin some position always-on or always-off). The exact per-
// position counts (probe-pinned) additionally pin the lattice mapping
// (the world 4-block coordinates, not the sub index alone).
TEST(CloudSubMaskTest, SubPositionsDecorrelated) {
    long regionOn = 0;
    long perPos[16] = {0};
    for (int z = -64; z < 64; ++z) {
        for (int x = -64; x < 64; ++x) {
            if (!mc::cloudMask(x, z, 0)) {
                continue;
            }
            ++regionOn;
            for (int sz = 0; sz < 4; ++sz) {
                for (int sx = 0; sx < 4; ++sx) {
                    if (mc::cloudSubMask(x, z, sx, sz, 0)) {
                        ++perPos[sz * 4 + sx];
                    }
                }
            }
        }
    }
    const long pinned[16] = {6197, 6195, 6137, 6215, 6188, 6173, 6253,
                             6230, 6201, 6205, 6213, 6213, 6225, 6255,
                             6157, 6218};
    for (int i = 0; i < 16; ++i) {
        EXPECT_EQ(perPos[i], pinned[i]) << "sub position " << i;
        const double ratio = static_cast<double>(perPos[i]) / regionOn;
        EXPECT_GT(ratio, 0.5) << "sub position " << i;
        EXPECT_LT(ratio, 1.0) << "sub position " << i;
    }
}

// UX-9a: no fixed region/sub alignment — inside the on region cells the
// sub count varies from cell to cell (the probe over the 128x128 window
// at t=0: min = 5, max = 16 — full cells exist, so the sub layer is not
// a constant function of the region layer).
TEST(CloudSubMaskTest, SubCountVariesPerRegionCell) {
    int minSubs = 16;
    int maxSubs = 0;
    for (int z = -64; z < 64; ++z) {
        for (int x = -64; x < 64; ++x) {
            if (!mc::cloudMask(x, z, 0)) {
                continue;
            }
            int inCell = 0;
            for (int sz = 0; sz < 4; ++sz) {
                for (int sx = 0; sx < 4; ++sx) {
                    if (mc::cloudSubMask(x, z, sx, sz, 0)) {
                        ++inCell;
                    }
                }
            }
            if (inCell < minSubs) {
                minSubs = inCell;
            }
            if (inCell > maxSubs) {
                maxSubs = inCell;
            }
        }
    }
    EXPECT_EQ(minSubs, 5);
    EXPECT_EQ(maxSubs, 16); // at least one FULL sub cell (no fixed alignment)
    // The spec's disjunction (min==0 or max==16) holds via the max side.
    EXPECT_TRUE(minSubs == 0 || maxSubs == 16);
}

// UX-9b: the thickness field is a pure function — the pins (the CRAFT-3
// content-pin pattern): any drift in the hash chain (salt, lattice
// mapping, mod-3 roll, t advance) is a contract break, not a "different
// sky". The mix pins ON cells at {1,2,3} and OFF cells at 0, incl.
// negative world coordinates (the asU64 two's-complement convention) and
// t > 0 (the golden-step advance is live at the height level too).
TEST(CloudSubHeightTest, DeterministicPins) {
    // t = 0, ON cells (the {1,2,3} values pin the mod-3 roll):
    EXPECT_EQ(mc::cloudSubHeight(0, 0, 0), 2);
    EXPECT_EQ(mc::cloudSubHeight(1, 0, 0), 3);
    EXPECT_EQ(mc::cloudSubHeight(2, 2, 0), 1);
    EXPECT_EQ(mc::cloudSubHeight(-1, 0, 0), 2);  // negative world x
    EXPECT_EQ(mc::cloudSubHeight(-4, 3, 0), 1);  // region boundary from
    // t = 0, OFF cells (region off or sub off -> 0):
    EXPECT_EQ(mc::cloudSubHeight(3, 1, 0), 0);
    EXPECT_EQ(mc::cloudSubHeight(4, 4, 0), 0);
    EXPECT_EQ(mc::cloudSubHeight(-5, -7, 0), 0); // both coords negative
    EXPECT_EQ(mc::cloudSubHeight(8, 8, 0), 0);
    EXPECT_EQ(mc::cloudSubHeight(15, 15, 0), 0);
    // t > 0 (the probe values):
    EXPECT_EQ(mc::cloudSubHeight(-10, -10, 1), 2);
    EXPECT_EQ(mc::cloudSubHeight(-9, -10, 1), 3);
    EXPECT_EQ(mc::cloudSubHeight(2, 10, 1), 2);
    EXPECT_EQ(mc::cloudSubHeight(0, 0, 1), 0); // on at t=0, off at t=1
    EXPECT_EQ(mc::cloudSubHeight(4, 4, 2), 2);
    EXPECT_EQ(mc::cloudSubHeight(-8, -2, 2), 3);
    EXPECT_EQ(mc::cloudSubHeight(-2, 5, 3), 1);
    // The same arguments, called again — the cross-run identity.
    EXPECT_EQ(mc::cloudSubHeight(-10, -10, 1), mc::cloudSubHeight(-10, -10, 1));
}

// UX-9b: the t drift moves the thickness field (the PITS #4 advance is
// live at the height level): the pinned cells flip between t steps — a
// constant-input finalizer (no golden-step advance) would keep them.
TEST(CloudSubHeightTest, TimeStepMovesTheField) {
    EXPECT_NE(mc::cloudSubHeight(0, 0, 0), mc::cloudSubHeight(0, 0, 1));
    EXPECT_NE(mc::cloudSubHeight(1, 0, 0), mc::cloudSubHeight(1, 0, 1));
    EXPECT_NE(mc::cloudSubHeight(-10, -10, 1), mc::cloudSubHeight(-10, -10, 2));
}

// UX-9b: the height level is the SAME on/off set as the presence field,
// restated in world coordinates: over a -32..32 world sub window at t=0,
// h > 0 iff (region on AND sub on) — zero mismatches in both directions,
// and the ON count (1536) is pinned. This is what lets the GLSL port
// reuse the mask bits and add only the height roll.
TEST(CloudSubHeightTest, MatchesOnOffLevels) {
    long on = 0, onButH0 = 0, offButHnz = 0;
    for (int z = -32; z < 32; ++z) {
        for (int x = -32; x < 32; ++x) {
            const int rx = mc::floorDiv(x, 4), rz = mc::floorDiv(z, 4);
            const int sx = x - rx * 4, sz = z - rz * 4;
            const bool present = mc::cloudMask(rx, rz, 0) &&
                                 mc::cloudSubMask(rx, rz, sx, sz, 0);
            const int h = mc::cloudSubHeight(x, z, 0);
            if (present) {
                ++on;
                if (h == 0) {
                    ++onButH0;
                }
            } else if (h != 0) {
                ++offButHnz;
            }
        }
    }
    EXPECT_EQ(on, 1536L);
    EXPECT_EQ(onButH0, 0L);
    EXPECT_EQ(offButHnz, 0L);
}

// UX-9b: the height distribution over the SAME 128x128 region window the
// coverage pins use (t=0): the ON count is EXACTLY the subOnTotal pin
// (99275 — the third level refines the same set), and the {1,2,3}
// counts are probe-pinned exact (the mod-3 roll is uniform — each tier
// sits in the [0.25, 0.40] band).
TEST(CloudSubHeightTest, HeightDistributionPinned) {
    long on = 0, h1 = 0, h2 = 0, h3 = 0;
    for (int z = -64; z < 64; ++z) {
        for (int x = -64; x < 64; ++x) {
            if (!mc::cloudMask(x, z, 0)) {
                continue;
            }
            for (int sz = 0; sz < 4; ++sz) {
                for (int sx = 0; sx < 4; ++sx) {
                    if (!mc::cloudSubMask(x, z, sx, sz, 0)) {
                        continue;
                    }
                    ++on;
                    switch (mc::cloudSubHeight(x * 4 + sx, z * 4 + sz, 0)) {
                        case 1:
                            ++h1;
                            break;
                        case 2:
                            ++h2;
                            break;
                        case 3:
                            ++h3;
                            break;
                        default:
                            ADD_FAILURE() << "off sub rolled a height at ("
                                          << x * 4 + sx << ", " << z * 4 + sz
                                          << ")";
                    }
                }
            }
        }
    }
    EXPECT_EQ(on, 99275L); // == the TwoLevelCoveragePinned subOnTotal pin
    EXPECT_EQ(h1, 33111L);
    EXPECT_EQ(h2, 32873L);
    EXPECT_EQ(h3, 33291L);
    EXPECT_EQ(h1 + h2 + h3, on);
    // The mod-3 roll is uniform: each tier in the [0.25, 0.40] band.
    EXPECT_GE(static_cast<double>(h1) / on, 0.25);
    EXPECT_LE(static_cast<double>(h1) / on, 0.40);
    EXPECT_GE(static_cast<double>(h2) / on, 0.25);
    EXPECT_LE(static_cast<double>(h2) / on, 0.40);
    EXPECT_GE(static_cast<double>(h3) / on, 0.25);
    EXPECT_LE(static_cast<double>(h3) / on, 0.40);
}

// UX-9b: decorrelation — the height tier does not track the sub
// position (a collapsed hash would pin some (sx,sz) to one tier). Over
// the same 128x128 window (t=0), the probe-pinned exact per-position
// per-tier counts: every one of the 48 cells of the 16 (sx,sz) x 3-tier
// table sits in a tight band (2000..2150, ~on/16/3), and each tier's
// per-position SUM is EXACTLY the SubPositionsDecorrelated on-count pin
// (6197/6195/... — the height level refines the same on set).
TEST(CloudSubHeightTest, HeightDecorrelatedFromOn) {
    static const long pinned[16][3] = {
        {2045, 2081, 2071}, {2056, 2027, 2112}, {2076, 2023, 2038},
        {2095, 2120, 2000}, {2053, 2078, 2057}, {2050, 2012, 2111},
        {2098, 2037, 2118}, {2071, 2044, 2115}, {2043, 2028, 2130},
        {2030, 2121, 2054}, {2082, 2059, 2072}, {2045, 2082, 2086},
        {2148, 2010, 2067}, {2057, 2082, 2116}, {2058, 2024, 2075},
        {2104, 2045, 2069}};
    long perPos[16][3] = {{0}};
    for (int z = -64; z < 64; ++z) {
        for (int x = -64; x < 64; ++x) {
            if (!mc::cloudMask(x, z, 0)) {
                continue;
            }
            for (int sz = 0; sz < 4; ++sz) {
                for (int sx = 0; sx < 4; ++sx) {
                    if (!mc::cloudSubMask(x, z, sx, sz, 0)) {
                        continue;
                    }
                    const int h = mc::cloudSubHeight(x * 4 + sx, z * 4 + sz, 0);
                    ASSERT_GE(h, 1) << "off sub at (" << x << ", " << z << ")";
                    ASSERT_LE(h, 3);
                    ++perPos[sz * 4 + sx][h - 1];
                }
            }
        }
    }
    for (int p = 0; p < 16; ++p) {
        for (int tier = 0; tier < 3; ++tier) {
            EXPECT_EQ(perPos[p][tier], pinned[p][tier])
                << "position " << p << " tier " << tier + 1;
            // No tier dominates or vanishes at a position.
            EXPECT_GT(perPos[p][tier], 1500L)
                << "position " << p << " tier " << tier + 1;
            EXPECT_LT(perPos[p][tier], 2600L)
                << "position " << p << " tier " << tier + 1;
        }
    }
    // Cross-level consistency: the tier sums per position are EXACTLY the
    // on-count pins of SubPositionsDecorrelated (the height refines the
    // same set — the two levels cannot drift apart).
    static const long onPinned[16] = {6197, 6195, 6137, 6215, 6188, 6173,
                                      6253, 6230, 6201, 6205, 6213, 6213,
                                      6225, 6255, 6157, 6218};
    for (int p = 0; p < 16; ++p) {
        EXPECT_EQ(perPos[p][0] + perPos[p][1] + perPos[p][2], onPinned[p])
            << "position " << p;
    }
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
// UX-9a: this test pins the cloudCellVertices output BIT-IDENTICAL to
// PRE-4 (the shared emitQuad refactor must not move a single byte).
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

// UX-9a: one sub-cell's quad — 4 vertices in the same corner order /
// normal / lightmap convention, 4-block corners world-aligned (x0 =
// cellX*16 + subX*4, z0 = cellZ*16 + subZ*4, the world 4-block lattice),
// the same unlit-white fields. The negative-cell case pins the floor
// convention for the sub origin too.
TEST(CloudSubCellVerticesTest, GeometryPinned) {
    const float ys = static_cast<float>(mc::kCloudHeight);
    // cell (3, -2), sub (1, 2): x in [52, 56), z in [-24, -20).
    std::vector<mc::MeshVertex> v;
    mc::cloudSubCellVertices(3, -2, 1, 2, v);
    ASSERT_EQ(v.size(), 4u);
    EXPECT_FLOAT_EQ(v[0].position.x, 52.0f);
    EXPECT_FLOAT_EQ(v[0].position.z, -24.0f);
    EXPECT_FLOAT_EQ(v[1].position.x, 56.0f);
    EXPECT_FLOAT_EQ(v[1].position.z, -24.0f);
    EXPECT_FLOAT_EQ(v[2].position.x, 56.0f);
    EXPECT_FLOAT_EQ(v[2].position.z, -20.0f);
    EXPECT_FLOAT_EQ(v[3].position.x, 52.0f);
    EXPECT_FLOAT_EQ(v[3].position.z, -20.0f);
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
        // World 4-block alignment (the block grain): both coordinates of
        // every corner are multiples of 4.
        EXPECT_EQ(static_cast<int>(c.position.x) % 4, 0);
        EXPECT_EQ(static_cast<int>(c.position.z) % 4, 0);
    }
    // A negative-cell sub origin: cell (-1, 0), sub (3, 0) -> x in
    // [-4, 0), z in [0, 4).
    std::vector<mc::MeshVertex> v2;
    mc::cloudSubCellVertices(-1, 0, 3, 0, v2);
    ASSERT_EQ(v2.size(), 4u);
    EXPECT_FLOAT_EQ(v2[0].position.x, -4.0f);
    EXPECT_FLOAT_EQ(v2[0].position.z, 0.0f);
    EXPECT_FLOAT_EQ(v2[1].position.x, 0.0f);
    EXPECT_FLOAT_EQ(v2[1].position.z, 0.0f);
    EXPECT_FLOAT_EQ(v2[2].position.x, 0.0f);
    EXPECT_FLOAT_EQ(v2[2].position.z, 4.0f);
    EXPECT_FLOAT_EQ(v2[3].position.x, -4.0f);
    EXPECT_FLOAT_EQ(v2[3].position.z, 4.0f);
}

// The full window mesh (UX-9a: sub-level quads): one flat 4x4 quad per
// ON SUB-cell of every ON region cell — 4 vertices + 6 indices per quad,
// in window-scan order (z outer, x inner; sub z outer / sub x inner
// inside a cell). The quad count is pinned BOTH ways: the probe-pinned
// exact value (cam (1,0) t=0 -> 3582) and INDEPENDENTLY recomputed from
// cloudMask ∧ cloudSubMask (the consistency pin). Every vertex sits at
// kCloudHeight; every quad is 4-block aligned and stays INSIDE its on
// region cell (a quad never leaks past its clump domain).
TEST(BuildCloudMeshTest, QuadCountAndHeight) {
    const int camX = 1, camZ = 0; // the spawn-point camera cell
    const int t = 0;
    long quads = 0;
    const int half = mc::kCloudWindowCells / 2;
    for (int dz = -half; dz < half; ++dz) {
        for (int dx = -half; dx < half; ++dx) {
            const int cx = camX + dx, cz = camZ + dz;
            if (!mc::cloudMask(cx, cz, t)) {
                continue;
            }
            for (int sz = 0; sz < 4; ++sz) {
                for (int sx = 0; sx < 4; ++sx) {
                    if (mc::cloudSubMask(cx, cz, sx, sz, t)) {
                        ++quads;
                    }
                }
            }
        }
    }
    EXPECT_EQ(quads, 3582L); // the probe-pinned exact value
    std::vector<mc::MeshVertex> verts;
    std::vector<std::uint32_t> idx;
    mc::buildCloudMesh(camX, camZ, t, verts, idx);
    EXPECT_EQ(verts.size(), 4u * static_cast<std::size_t>(quads));
    EXPECT_EQ(idx.size(), 6u * static_cast<std::size_t>(quads));
    for (const mc::MeshVertex& c : verts) {
        EXPECT_FLOAT_EQ(c.position.y, static_cast<float>(mc::kCloudHeight));
    }
    // The index range is legal (every index addresses a vertex).
    for (std::uint32_t i : idx) {
        EXPECT_LT(i, verts.size());
    }
    // Every quad: the 4-block-aligned corner pattern (x0, z0), (x0+4,
    // z0), (x0+4, z0+4), (x0, z0+4), and the corner's region cell
    // (floor(x0/16), floor(z0/16)) must be ON (no quad leaks outside
    // its clump domain — the sub level only rolls inside on regions).
    for (std::size_t q = 0; q * 4 < verts.size(); ++q) {
        const mc::MeshVertex& a = verts[q * 4 + 0];
        const mc::MeshVertex& b = verts[q * 4 + 1];
        const mc::MeshVertex& c = verts[q * 4 + 2];
        const mc::MeshVertex& d = verts[q * 4 + 3];
        const float x0 = a.position.x, z0 = a.position.z;
        EXPECT_EQ(static_cast<int>(x0) % 4, 0) << "quad " << q;
        EXPECT_EQ(static_cast<int>(z0) % 4, 0) << "quad " << q;
        EXPECT_FLOAT_EQ(b.position.x, x0 + 4.0f);
        EXPECT_FLOAT_EQ(b.position.z, z0);
        EXPECT_FLOAT_EQ(c.position.x, x0 + 4.0f);
        EXPECT_FLOAT_EQ(c.position.z, z0 + 4.0f);
        EXPECT_FLOAT_EQ(d.position.x, x0);
        EXPECT_FLOAT_EQ(d.position.z, z0 + 4.0f);
        const int rcx = static_cast<int>(std::floor(x0 / 16.0f));
        const int rcz = static_cast<int>(std::floor(z0 / 16.0f));
        EXPECT_TRUE(mc::cloudMask(rcx, rcz, t))
            << "quad " << q << " leaks outside its region cell (" << rcx
            << ", " << rcz << ")";
    }
}

// UX-9a: the rebuild key is UNCHANGED by the sub level — the dirty flag
// still keys on camera cell + t (the no-input ×2 RAW byte-identity and
// the per-16-blocks walking cost rely on it).
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
