// V2-18 AO (v2.19.0): the classic Minecraft vertex AO pure function
// family (world/ao) — the 8-combo level table, the 4-tier MC-exact
// factors, the occluder height bands (opaque ∧ slab/stairs/wall/fence
// ranges, the closed endpoints), the per-vertex 3-query rule across all
// six face directions and the slab mid-plane rule, void = no occlusion,
// and the render.ao strict config read (the weatherEnabled discipline).
#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>

#include "block/block_registry.hpp"
#include "core/config.hpp"
#include "world/ao.hpp"
#include "world/coordinates.hpp"
#include "world/world.hpp"

using namespace mc;

namespace {

Json parseDef(const std::string& text) {
    auto r = Json::parse(text);
    if (!r.ok()) {
        ADD_FAILURE() << "parse failed: " << r.error().message;
        return Json::object();
    }
    return *r;
}

std::string cubeDef(const std::string& id, const std::string& properties,
                    const std::string& textures) {
    return std::string("{\"id\":\"") + id +
           "\",\"properties\":" + properties +
           ",\"model\":{\"textures\":" + textures + "}}";
}

// The AO test registry: full opaque (stone), non-opaque (glass), the
// stateful slab (state_count 3, default 0), the stateless slab, stairs
// (implicit state, default facing north), wall, fence — the occluder
// band's full shape set.
BlockRegistry makeRegistry() {
    BlockRegistry reg;
    auto stone = reg.registerBlock(
        parseDef(cubeDef("stone", "{}", R"({"top":"t","side":"t","bottom":"t"})")));
    auto glass = reg.registerBlock(parseDef(
        cubeDef("glass", R"({"opaque":false,"transparent":true,"solid":false})",
                R"({"top":"t","side":"t","bottom":"t"})")));
    auto slab3 = reg.registerBlock(parseDef(
        R"({"id":"slab3","model":{"shape":"slab","state_count":3,)"
        R"("state_default":0,"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    auto slab0 = reg.registerBlock(parseDef(
        R"({"id":"slab0","model":{"shape":"slab",)"
        R"("textures":{"top":"t","side":"t","bottom":"t"}}})"));
    auto stairs = reg.registerBlock(parseDef(
        R"({"id":"stairs","model":{"shape":"stairs",)"
        R"("textures":{"top":"t","side":"t","bottom":"t"}}})"));
    auto wall = reg.registerBlock(parseDef(
        R"({"id":"wall","model":{"shape":"wall","textures":{"top":"t","side":"t","bottom":"t"}}})"));
    auto fence = reg.registerBlock(parseDef(
        R"({"id":"fence","model":{"shape":"fence","textures":{"side":"t"}}})"));
    if (!stone.ok() || !glass.ok() || !slab3.ok() || !slab0.ok() ||
        !stairs.ok() || !wall.ok() || !fence.ok()) {
        ADD_FAILURE() << "test registry failed to build";
    }
    return reg;
}

// The four-tier factor table (Minecraft's exact values).
void expectFactors() {
    EXPECT_FLOAT_EQ(aoFactor(0), 1.0f);   // kAoFull
    EXPECT_FLOAT_EQ(aoFactor(1), 0.75f);  // kAoOne
    EXPECT_FLOAT_EQ(aoFactor(2), 0.5f);   // kAoTwo
    EXPECT_FLOAT_EQ(aoFactor(3), 0.3f);   // kAoCorner (the two-side case)
}

} // namespace

// ---------------------------------------------------------------------
// The pure tables (no world).
// ---------------------------------------------------------------------

// The 8 neighbor-combo level table: side1∧side2 = the corner darkness
// (3, factor 0.3 — Minecraft's exact special case), otherwise the plain
// count (0..2).
TEST(AoTest, CornerLevelTableAll8Combos) {
    for (int s1 = 0; s1 <= 1; ++s1) {
        for (int s2 = 0; s2 <= 1; ++s2) {
            for (int sc = 0; sc <= 1; ++sc) {
                const bool a = (s1 != 0), b = (s2 != 0), c = (sc != 0);
                const std::uint8_t expected =
                    (a && b) ? 3 : static_cast<std::uint8_t>(s1 + s2 + sc);
                EXPECT_EQ(aoCornerLevel(a, b, c), expected)
                    << "combo (" << s1 << "," << s2 << "," << sc << ")";
            }
        }
    }
}

TEST(AoTest, FactorTableIsTheMcFourTiers) {
    expectFactors();
    EXPECT_FLOAT_EQ(kAoFull, 1.0f);
    EXPECT_FLOAT_EQ(kAoOne, 0.75f);
    EXPECT_FLOAT_EQ(kAoTwo, 0.5f);
    EXPECT_FLOAT_EQ(kAoCorner, 0.3f);
}

// The occluder band by shape (the opaque flag + the kSlabHalf bands —
// the Entity::solidRange values, closed).
TEST(AoTest, OccluderBandByShapeAndState) {
    const BlockRegistry reg = makeRegistry();
    const Block& stone = reg.at(reg.idForName("stone"));
    const Block& glass = reg.at(reg.idForName("glass"));
    const Block& slab3 = reg.at(reg.idForName("slab3"));
    const Block& slab0 = reg.at(reg.idForName("slab0"));
    const Block& stairs = reg.at(reg.idForName("stairs"));
    const Block& wall = reg.at(reg.idForName("wall"));
    const Block& fence = reg.at(reg.idForName("fence"));

    // Full opaque = the whole cell.
    const AoBand full = aoOccluderBand(stone, 0);
    EXPECT_FALSE(full.empty());
    EXPECT_FLOAT_EQ(full.lo, 0.0f);
    EXPECT_FLOAT_EQ(full.hi, 1.0f);
    // Non-opaque = no band (glass never occludes).
    EXPECT_TRUE(aoOccluderBand(glass, 0).empty());
    // Slab bands by state: 0 = [0, 0.5], 1 = [0.5, 1], >= 2 = [0, 1].
    const AoBand s0 = aoOccluderBand(slab3, 0);
    EXPECT_FLOAT_EQ(s0.lo, 0.0f);
    EXPECT_FLOAT_EQ(s0.hi, kSlabHalf);
    const AoBand s1 = aoOccluderBand(slab3, 1);
    EXPECT_FLOAT_EQ(s1.lo, kSlabHalf);
    EXPECT_FLOAT_EQ(s1.hi, 1.0f);
    const AoBand s2 = aoOccluderBand(slab3, 2);
    EXPECT_FLOAT_EQ(s2.lo, 0.0f);
    EXPECT_FLOAT_EQ(s2.hi, 1.0f);
    // The stateless slab = the bottom half, whatever `state` says (the
    // caller never reads a state for it — pass 0).
    const AoBand sless = aoOccluderBand(slab0, 0);
    EXPECT_FLOAT_EQ(sless.lo, 0.0f);
    EXPECT_FLOAT_EQ(sless.hi, kSlabHalf);
    // Stairs (deviation 2: the slope is not refined) / wall / fence =
    // the whole cell (their within-cell solid regions span y 0..1).
    for (const Block* b : {&stairs, &wall, &fence}) {
        const AoBand band = aoOccluderBand(*b, 0);
        EXPECT_FALSE(band.empty());
        EXPECT_FLOAT_EQ(band.lo, 0.0f);
        EXPECT_FLOAT_EQ(band.hi, 1.0f);
    }
}

// The band test: closed interval, the vertex height in the QUERY's row
// (y - rowOffset). The slab seam vertex at 0.5 counts for both the
// [0, 0.5] and the [0.5, 1] band; the row-above/below offsets shift.
TEST(AoTest, BandCoversClosedEndpoints) {
    const AoBand full {0.0f, 1.0f};
    const AoBand bottom {0.0f, kSlabHalf};
    const AoBand top {kSlabHalf, 1.0f};
    const AoBand none = AoBand::none();

    // Same row (offset 0): the closed slab bands.
    EXPECT_TRUE(aoBandCovers(0.0f, 0, bottom));    // the band's lo end
    EXPECT_TRUE(aoBandCovers(kSlabHalf, 0, bottom)); // the seam (hi end)
    EXPECT_TRUE(aoBandCovers(kSlabHalf, 0, top));   // the seam (lo end)
    EXPECT_TRUE(aoBandCovers(1.0f, 0, top));       // the band's hi end
    EXPECT_FALSE(aoBandCovers(0.25f, 0, top));     // below the top slab
    EXPECT_FALSE(aoBandCovers(0.75f, 0, bottom));  // above the bottom slab
    // The row above (offset +1): the face's top vertex (y 1) sits at the
    // query row's bottom (0) — a full block / bottom slab there covers
    // it, a top slab (band starting at 0.5) does not.
    EXPECT_TRUE(aoBandCovers(1.0f, 1, full));
    EXPECT_TRUE(aoBandCovers(1.0f, 1, bottom));
    EXPECT_FALSE(aoBandCovers(1.0f, 1, top));
    // The row below (offset -1): the face's bottom vertex (y 0) sits at
    // the query row's top (1).
    EXPECT_TRUE(aoBandCovers(0.0f, -1, full));
    EXPECT_TRUE(aoBandCovers(0.0f, -1, top));
    EXPECT_FALSE(aoBandCovers(0.0f, -1, bottom));
    // The slab mid-plane (y 0.5) against the row above: 0.5 - 1 = -0.5
    // is outside every band — the state-0 slab's top face never AO's.
    EXPECT_FALSE(aoBandCovers(kSlabHalf, 1, full));
    EXPECT_FALSE(aoBandCovers(kSlabHalf, 1, top));
    // The empty band never covers.
    EXPECT_FALSE(aoBandCovers(0.5f, 0, none));
    EXPECT_FALSE(aoBandCovers(0.0f, 0, none));
}

// ---------------------------------------------------------------------
// The per-vertex 3-query rule against a world (all six directions, the
// slab mid-plane, void).
// ---------------------------------------------------------------------

TEST(AoTest, LoneBlockAllCornersLevelZero) {
    const BlockRegistry reg = makeRegistry();
    const BlockID stone = reg.idForName("stone");
    World world;
    world.setBlock(8, 4, 8, stone);
    const BlockPos cell {8, 4, 8};
    // All 6 faces x 4 corners: no in-front neighbor is opaque -> 0.
    for (const CubeFace& face : kCubeFaces) {
        for (int k = 0; k < 4; ++k) {
            EXPECT_EQ(aoVertexLevel(world, reg, cell, face.normal,
                                    face.corner[k][0], face.corner[k][1],
                                    face.corner[k][2],
                                    face.corner[k][1]),
                      0)
                << "face (" << face.dx << "," << face.dy << "," << face.dz
                << ") corner " << k;
        }
    }
}

// The +X face with all three checks of the bottom-front corner filled:
// the corner level 3, the neighbors' levels 1, the opposite corner 0 —
// the e directions (away from the cell at the vertex) pinned concretely.
TEST(AoTest, PlusXFaceCornerQuerySet) {
    const BlockRegistry reg = makeRegistry();
    const BlockID stone = reg.idForName("stone");
    World world;
    world.setBlock(8, 4, 8, stone); // B
    // The viewer V = (9,4,8); the bottom-front vertex (corner (1,0,0))
    // checks (9,3,8), (9,4,7) and (9,3,7).
    world.setBlock(9, 3, 8, stone);
    world.setBlock(9, 4, 7, stone);
    world.setBlock(9, 3, 7, stone);
    const BlockPos cell {8, 4, 8};
    const Vec3 n {1.0f, 0.0f, 0.0f};
    // corner c1 = (1,0,0): all three solid -> the two-side special case
    // (level 3, not the count 3 — same here, pinned below via the
    // partial case).
    EXPECT_EQ(aoVertexLevel(world, reg, cell, n, 1, 0, 0, 0.0f), 3);
    // corner c0 = (1,0,1): only (9,3,8) solid -> 1.
    EXPECT_EQ(aoVertexLevel(world, reg, cell, n, 1, 0, 1, 0.0f), 1);
    // corner c2 = (1,1,0): only (9,4,7) solid -> 1.
    EXPECT_EQ(aoVertexLevel(world, reg, cell, n, 1, 1, 0, 1.0f), 1);
    // corner c3 = (1,1,1): all three (9,5,8)/(9,4,9)/(9,5,9) air -> 0.
    EXPECT_EQ(aoVertexLevel(world, reg, cell, n, 1, 1, 1, 1.0f), 0);
}

// The two-side special case: BOTH sides solid + the corner AIR is level
// 3 (factor 0.3) — the plain count would say 2 (0.5).
TEST(AoTest, TwoSidesWithAirCornerIsLevelThree) {
    const BlockRegistry reg = makeRegistry();
    const BlockID stone = reg.idForName("stone");
    World world;
    world.setBlock(8, 4, 8, stone);
    world.setBlock(9, 3, 8, stone); // side1 of the c1 corner
    world.setBlock(9, 4, 7, stone); // side2
    // (9,3,7) stays air.
    const BlockPos cell {8, 4, 8};
    EXPECT_EQ(aoVertexLevel(world, reg, cell, {1.0f, 0.0f, 0.0f}, 1, 0, 0,
                            0.0f),
              3);
}

// The -Z face direction (the stair riser's look): the top vertex checks
// the row above — a block above the viewer darkens it, a block in the
// same row beside does not reach the top vertex's height.
TEST(AoTest, MinusZFaceTopVertexQueriesRowAbove) {
    const BlockRegistry reg = makeRegistry();
    const BlockID stone = reg.idForName("stone");
    World world;
    world.setBlock(8, 4, 8, stone);
    // The -Z face's viewer V = (8,4,7); the top-left vertex (corner
    // (0,1,0)) checks (7,4,7), (8,5,7), (7,5,7).
    world.setBlock(8, 5, 7, stone);
    const BlockPos cell {8, 4, 8};
    EXPECT_EQ(aoVertexLevel(world, reg, cell, {0.0f, 0.0f, -1.0f}, 0, 1, 0,
                            1.0f),
              1);
    // The bottom-left vertex (corner (0,0,0)) checks (7,4,7), (8,3,7),
    // (7,3,7): all air -> 0.
    EXPECT_EQ(aoVertexLevel(world, reg, cell, {0.0f, 0.0f, -1.0f}, 0, 0, 0,
                            0.0f),
              0);
}

// The slab mid-plane rule (Minecraft's half-block same-height): the
// state-0 slab's TOP face (y+0.5) queries the row above, whose bands
// start at y+1 — a full block in that row never covers the mid-plane
// vertex; the slab's SIDE top vertex (y 0.5) meets same-row neighbor
// bands (closed endpoints).
TEST(AoTest, SlabMidPlaneTopFaceNeverCovered) {
    const BlockRegistry reg = makeRegistry();
    const BlockID slab = reg.idForName("slab3");
    World world;
    world.setBlock(8, 4, 8, slab);
    world.setBlockState(8, 4, 8, 0);
    // The row above the slab (y 5): full blocks where the top face's
    // c0 corner checks (7,5,8), (8,5,7), (7,5,7).
    world.setBlock(7, 5, 8, reg.idForName("stone"));
    world.setBlock(8, 5, 7, reg.idForName("stone"));
    world.setBlock(7, 5, 7, reg.idForName("stone"));
    const BlockPos cell {8, 4, 8};
    // The TOP face (normal +Y, the mid-plane yLocal 0.5): level 0.
    EXPECT_EQ(aoVertexLevel(world, reg, cell, {0.0f, 1.0f, 0.0f}, 0, 1, 0,
                            kSlabHalf),
              0);
    EXPECT_EQ(aoVertexLevel(world, reg, cell, {0.0f, 1.0f, 0.0f}, 1, 1, 0,
                            kSlabHalf),
              0);
    // The SIDE top vertex (+X face, corner (1,1,0), yLocal 0.5): the
    // row-above check (9,5,8) is air; the same-row check (9,4,7) gets a
    // bottom slab -> the band [0, 0.5] covers 0.5 (closed endpoint) ->
    // level 1.
    world.setBlock(9, 4, 7, slab);
    world.setBlockState(9, 4, 7, 0);
    EXPECT_EQ(aoVertexLevel(world, reg, cell, {1.0f, 0.0f, 0.0f}, 1, 1, 0,
                            kSlabHalf),
              1);
    // A TOP slab beside (band [0.5, 1]) also covers the 0.5 seam.
    world.setBlockState(9, 4, 7, 1);
    EXPECT_EQ(aoVertexLevel(world, reg, cell, {1.0f, 0.0f, 0.0f}, 1, 1, 0,
                            kSlabHalf),
              1);
    // A full block beside covers the mid vertex too (0.5 in [0, 1]).
    world.setBlock(9, 4, 7, reg.idForName("stone"));
    EXPECT_EQ(aoVertexLevel(world, reg, cell, {1.0f, 0.0f, 0.0f}, 1, 1, 0,
                            kSlabHalf),
              1);
}

// The state-1 slab's TOP face sits ON the cell row (y 1.0) and gets AO
// like a full top face (the mid-plane rule's contrast pair).
TEST(AoTest, TopSlabTopFaceGetsAoLikeFull) {
    const BlockRegistry reg = makeRegistry();
    const BlockID slab = reg.idForName("slab3");
    const BlockID stone = reg.idForName("stone");
    World world;
    world.setBlock(8, 4, 8, slab);
    world.setBlockState(8, 4, 8, 1);
    world.setBlock(7, 5, 8, stone);
    world.setBlock(8, 5, 7, stone);
    world.setBlock(7, 5, 7, stone);
    const BlockPos cell {8, 4, 8};
    // The +Y face's c0 corner (yLocal 1.0 = the row's bottom 0): all
    // three solid -> 3.
    EXPECT_EQ(aoVertexLevel(world, reg, cell, {0.0f, 1.0f, 0.0f}, 0, 1, 0,
                            1.0f),
              3);
}

// Void: below the world (y < 0) and above it (y >= kWorldHeight) read
// air — the row-below / row-above checks at the world edges never
// occlude.
TEST(AoTest, VoidRowsNeverOcclude) {
    const BlockRegistry reg = makeRegistry();
    const BlockID stone = reg.idForName("stone");
    World world;
    world.setBlock(8, 0, 8, stone); // the bottom row
    const BlockPos bottom {8, 0, 8};
    // The -Y face (bottom): the viewer row is y = -1 = void -> 0.
    EXPECT_EQ(aoVertexLevel(world, reg, bottom, {0.0f, -1.0f, 0.0f}, 0, 0, 0,
                            0.0f),
              0);
    EXPECT_EQ(aoVertexLevel(world, reg, bottom, {0.0f, -1.0f, 0.0f}, 1, 0, 0,
                            0.0f),
              0);
    world.setBlock(8, kWorldHeight - 1, 8, stone); // the top row
    const BlockPos top {8, kWorldHeight - 1, 8};
    // The +Y face (top): the viewer row is y = kWorldHeight = void -> 0.
    EXPECT_EQ(aoVertexLevel(world, reg, top, {0.0f, 1.0f, 0.0f}, 0, 1, 0,
                            1.0f),
              0);
    EXPECT_EQ(aoVertexLevel(world, reg, top, {0.0f, 1.0f, 0.0f}, 1, 1, 1,
                            1.0f),
              0);
}

// The slab occluder's STATE is read from the world (the emitSlab rule:
// the unset sentinel falls back to the block's state default).
TEST(AoTest, SlabOccluderStateReadFromWorld) {
    const BlockRegistry reg = makeRegistry();
    const BlockID slab = reg.idForName("slab3");
    World world;
    world.setBlock(8, 4, 8, slab);
    world.setBlockState(8, 4, 8, 0);
    const BlockPos cell {8, 4, 8};
    // A neighbor slab with the state UNSET (default 0 = the bottom
    // band): the side top vertex (0.5) is covered (closed endpoint).
    world.setBlock(9, 4, 7, slab); // the state byte stays unset
    EXPECT_EQ(aoVertexLevel(world, reg, cell, {1.0f, 0.0f, 0.0f}, 1, 1, 0,
                            kSlabHalf),
              1);
    // The same neighbor at state 2 (the double slab, full band) also
    // covers; at state 1 (the top slab) the bottom vertex (yLocal 0,
    // the query's yInRow 0) is NOT covered (the band starts at 0.5).
    world.setBlockState(9, 4, 7, 2);
    EXPECT_EQ(aoVertexLevel(world, reg, cell, {1.0f, 0.0f, 0.0f}, 1, 0, 0,
                            0.0f),
              1);
    world.setBlockState(9, 4, 7, 1);
    EXPECT_EQ(aoVertexLevel(world, reg, cell, {1.0f, 0.0f, 0.0f}, 1, 0, 0,
                            0.0f),
              0);
    // The row-above bottom slab covers the face's TOP vertex (yInRow 0
    // in the bottom slab's row) — (9,4,7) cleared first, so the same-row
    // check is air (only the row-above side counts).
    world.setBlock(9, 4, 7, BlockID::air());
    world.setBlock(9, 5, 8, slab);
    world.setBlockState(9, 5, 8, 0);
    EXPECT_EQ(aoVertexLevel(world, reg, cell, {1.0f, 0.0f, 0.0f}, 1, 1, 0,
                            1.0f),
              1);
}

// ---------------------------------------------------------------------
// The 8-fetch cell corner levels (the greedy mask build's path).
// ---------------------------------------------------------------------

TEST(AoTest, CellCornerLevelsMatchPerVertexRule) {
    const BlockRegistry reg = makeRegistry();
    const BlockID stone = reg.idForName("stone");
    World world;
    world.setBlock(8, 4, 8, stone);
    world.setBlock(9, 3, 8, stone);
    world.setBlock(9, 4, 7, stone);
    world.setBlock(9, 3, 7, stone);
    const BlockPos cell {8, 4, 8};
    // The +X face: the per-cell 4-corner result must equal the four
    // per-vertex calls (the shared-fetch implementation's contract).
    std::array<std::uint8_t, 4> cellAo {};
    const CubeFace plusX = kCubeFaces[4]; // {1,0,0}
    aoCellCornerLevels(world, reg, cell, plusX, cellAo);
    for (int k = 0; k < 4; ++k) {
        EXPECT_EQ(cellAo[k], aoVertexLevel(world, reg, cell, plusX.normal,
                                           plusX.corner[k][0],
                                           plusX.corner[k][1],
                                           plusX.corner[k][2],
                                           plusX.corner[k][1]))
            << "corner " << k;
    }
    // The concrete values (the PlusXFaceCornerQuerySet world): c0 (1,
    // c1 (3), c2 (1), c3 (0).
    EXPECT_EQ(cellAo[0], 1);
    EXPECT_EQ(cellAo[1], 3);
    EXPECT_EQ(cellAo[2], 1);
    EXPECT_EQ(cellAo[3], 0);
    // An empty world: all four corners 0 on every face.
    World empty;
    empty.setBlock(8, 4, 8, stone);
    for (const CubeFace& face : kCubeFaces) {
        std::array<std::uint8_t, 4> z {};
        aoCellCornerLevels(empty, reg, cell, face, z);
        EXPECT_EQ(z[0], 0)
            << "face (" << face.dx << "," << face.dy << "," << face.dz << ")";
        EXPECT_EQ(z[1], 0);
        EXPECT_EQ(z[2], 0);
        EXPECT_EQ(z[3], 0);
    }
}

// ---------------------------------------------------------------------
// render.ao — the strict config read (the weatherEnabled discipline).
// ---------------------------------------------------------------------

TEST(AoConfigTest, MissingKeyDefaultsToOn) {
    Config cfg;
    auto r = aoEnabled(cfg);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_TRUE(*r);
}

TEST(AoConfigTest, ExplicitValues) {
    Config cfg;
    cfg.set("render.ao", Json(true));
    auto r = aoEnabled(cfg);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_TRUE(*r);
    cfg.set("render.ao", Json(false));
    auto r2 = aoEnabled(cfg);
    ASSERT_TRUE(r2.ok()) << r2.error().message;
    EXPECT_FALSE(*r2);
}

TEST(AoConfigTest, NonBoolNamed) {
    // NAMED and rejected at init (the Config getters' silent fallback
    // would hide a typo — the S6 gate arm).
    Config cfg;
    cfg.set("render.ao", Json(1));
    auto r = aoEnabled(cfg);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("render.ao"), std::string::npos);
    cfg.set("render.ao", Json("true"));
    auto r2 = aoEnabled(cfg);
    ASSERT_FALSE(r2.ok());
    EXPECT_NE(r2.error().message.find("render.ao"), std::string::npos);
}
