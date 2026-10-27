// Lighting tests: sky-light column scan + propagation (height, occlusion,
// horizontal attenuation, vertical free fall, water attenuation),
// block-light sources/attenuation/occlusion, cross-chunk propagation,
// dirty tracking of local updates, and the key invariant: a local re-light
// (updateLocal) after a single block change must produce exactly the same
// light as a full re-propagation (propagateAll) of the changed world.
#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "block/block_registry.hpp"
#include "core/json.hpp"
#include "renderer/texture_atlas.hpp"
#include "world/chunk_mesher.hpp"
#include "world/lighting.hpp"
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

// stone=opaque; water=non-opaque; glowstone=opaque emission 15;
// lantern=opaque emission 5. LGT-2: leaves=non-opaque light_opacity 2
// (the Minecraft leaf value); glass=non-opaque light_opacity 1 (the
// Minecraft glass value).
BlockRegistry makeRegistry() {
    BlockRegistry reg;
    auto r1 = reg.registerBlock(
        parseDef(cubeDef("stone", "{}",
                         R"({"top":"t","side":"t","bottom":"t"})")));
    auto r2 = reg.registerBlock(parseDef(
        cubeDef("water",
                R"({"opaque":false,"transparent":true,"solid":false,"fluid":"water"})",
                R"({"top":"t","side":"t","bottom":"t"})")));
    auto r3 = reg.registerBlock(
        parseDef(cubeDef("glowstone", R"({"light_emission":15})",
                         R"({"top":"t","side":"t","bottom":"t"})")));
    auto r4 = reg.registerBlock(
        parseDef(cubeDef("lantern", R"({"light_emission":5})",
                         R"({"top":"t","side":"t","bottom":"t"})")));
    auto r5 = reg.registerBlock(parseDef(
        cubeDef("leaves",
                R"({"opaque":false,"transparent":true,"solid":false,"light_opacity":2})",
                R"({"top":"t","side":"t","bottom":"t"})")));
    auto r6 = reg.registerBlock(parseDef(
        cubeDef("glass",
                R"({"opaque":false,"transparent":true,"light_opacity":1})",
                R"({"top":"t","side":"t","bottom":"t"})")));
    // LGT-5: the half-block form — OPAQUE (face culling unchanged, the
    // P85 slab rendering contract) but light_opacity 0 (light passes
    // like air; the shipped slab/stairs/wall/fence shape).
    auto r7 = reg.registerBlock(parseDef(cubeDef(
        "slab5", R"({"light_opacity":0})",
        R"({"top":"t","side":"t","bottom":"t"})")));
    if (!r1.ok() || !r2.ok() || !r3.ok() || !r4.ok() || !r5.ok() ||
        !r6.ok() || !r7.ok()) {
        ADD_FAILURE() << "test registry failed to build";
    }
    return reg;
}

BlockID idOf(const BlockRegistry& reg, const std::string& name) {
    const BlockID id = reg.idForName(name);
    if (!id.valid()) {
        ADD_FAILURE() << "unknown block '" << name << "'";
    }
    return id;
}

void fillBox(World& world, int x0, int y0, int z0, int x1, int y1, int z1,
             BlockID block) {
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            for (int z = z0; z <= z1; ++z) {
                if (!world.setBlock(x, y, z, block)) {
                    ADD_FAILURE() << "setBlock(" << x << "," << y << "," << z
                                  << ") failed";
                }
            }
        }
    }
}

// (sky, block) light of every cell of every chunk, chunk-major in sorted
// chunk order.
struct LightSnapshot {
    std::vector<std::pair<std::uint8_t, std::uint8_t>> cells;
    bool operator==(const LightSnapshot& o) const { return cells == o.cells; }
};

LightSnapshot snapshot(const World& world) {
    LightSnapshot s;
    const std::vector<ChunkPos> positions = world.chunkPositions();
    s.cells.reserve(positions.size() * Chunk::kVolume);
    for (const ChunkPos& pos : positions) {
        const Chunk* chunk = world.findChunk(pos);
        for (int ly = 0; ly < kChunkSize; ++ly) {
            for (int lz = 0; lz < kChunkSize; ++lz) {
                for (int lx = 0; lx < kChunkSize; ++lx) {
                    s.cells.emplace_back(chunk->skyLightAt(lx, ly, lz),
                                         chunk->blockLightAt(lx, ly, lz));
                }
            }
        }
    }
    return s;
}

// The two worlds must have identical light everywhere (same chunk layout).
void expectSameLight(const World& a, const World& b) {
    const LightSnapshot sa = snapshot(a);
    const LightSnapshot sb = snapshot(b);
    ASSERT_EQ(sa.cells.size(), sb.cells.size());
    std::vector<std::size_t> diffs;
    for (std::size_t i = 0; i < sa.cells.size(); ++i) {
        if (sa.cells[i] != sb.cells[i]) {
            diffs.push_back(i);
        }
    }
    std::string msg = "light differs in " + std::to_string(diffs.size()) +
                      " cells; first: ";
    for (std::size_t i = 0; i < diffs.size() && i < 8; ++i) {
        const std::size_t d = diffs[i];
        msg += "chunk " + std::to_string(d / Chunk::kVolume) + " local(" +
               std::to_string(d % 16) + "," + std::to_string((d / 16) % 16) +
               "," + std::to_string((d / 256) % 16) + ") local-update=" +
               std::to_string(sa.cells[d].first) + "/" +
               std::to_string(sa.cells[d].second) + " full=" +
               std::to_string(sb.cells[d].first) + "/" +
               std::to_string(sb.cells[d].second) + "; ";
    }
    EXPECT_TRUE(diffs.empty()) << msg;
}

// 2-chunk (x 0..31) scenario: ground at y=0, a step at y=1 for x 0..7,
// a wall crossing the chunk boundary at x=16 (y 1..6, z=8), plus optional
// extra content.
void buildScenario(World& world, const BlockRegistry& reg,
                   const std::optional<BlockPos>& glowstoneAt,
                   const std::optional<BlockPos>& stoneAt) {
    const BlockID stone = idOf(reg, "stone");
    const BlockID glowstone = idOf(reg, "glowstone");
    world.getOrCreateChunk({0, 0, 0});
    world.getOrCreateChunk({1, 0, 0});
    fillBox(world, 0, 0, 0, 31, 0, 15, stone);
    fillBox(world, 0, 1, 0, 7, 1, 15, stone);
    fillBox(world, 16, 1, 8, 16, 6, 8, stone);
    if (glowstoneAt) {
        world.setBlock(glowstoneAt->x, glowstoneAt->y, glowstoneAt->z, glowstone);
    }
    if (stoneAt) {
        world.setBlock(stoneAt->x, stoneAt->y, stoneAt->z, stone);
    }
}

} // namespace

// -- void queries ---------------------------------------------------------------

TEST(LightingTest, VoidLightQueries) {
    World world; // no chunks at all
    EXPECT_EQ(world.getSkyLight(0, 64, 0), 15);
    EXPECT_EQ(world.getSkyLight(100, 127, -100), 15);
    EXPECT_EQ(world.getSkyLight(0, kWorldHeight, 0), 15); // above the world
    EXPECT_EQ(world.getSkyLight(0, 200, 0), 15);
    EXPECT_EQ(world.getSkyLight(0, -1, 0), 0);            // below the world
    EXPECT_EQ(world.getBlockLight(0, 64, 0), 0);
    EXPECT_EQ(world.getBlockLight(0, -1, 0), 0);
    EXPECT_EQ(world.getBlockLight(0, 200, 0), 0);
}

// -- sky light ------------------------------------------------------------------

TEST(LightingTest, SkyTopScan) {
    const BlockRegistry reg = makeRegistry();
    World world;
    fillBox(world, 0, 8, 0, 15, 8, 15, idOf(reg, "stone")); // slab at y=8
    Lighting::propagateAll(world, reg);
    EXPECT_EQ(world.getSkyLight(8, 9, 8), 15);  // above the slab
    EXPECT_EQ(world.getSkyLight(0, 15, 0), 15);
    EXPECT_EQ(world.getSkyLight(8, 8, 8), 0);   // the opaque slab itself
    EXPECT_EQ(world.getSkyLight(8, 7, 8), 0);   // below: sealed darkness
    EXPECT_EQ(world.getSkyLight(3, 0, 12), 0);
}

TEST(LightingTest, SkyGroundColumn) {
    const BlockRegistry reg = makeRegistry();
    World world;
    fillBox(world, 0, 0, 0, 15, 0, 15, idOf(reg, "stone")); // ground
    Lighting::propagateAll(world, reg);
    EXPECT_EQ(world.getSkyLight(8, 0, 8), 0);
    EXPECT_EQ(world.getSkyLight(8, 1, 8), 15);
    EXPECT_EQ(world.getSkyLight(0, 100, 0), 15);
}

TEST(LightingTest, SkyHorizontalAttenuationVerticalFree) {
    const BlockRegistry reg = makeRegistry();
    const BlockID stone = idOf(reg, "stone");
    World world;
    fillBox(world, 0, 0, 0, 15, 0, 15, stone);  // ground
    fillBox(world, 0, 10, 0, 3, 10, 15, stone); // roof over x 0..3
    Lighting::propagateAll(world, reg);
    // Horizontal: -1 per step from the open column at x=4.
    EXPECT_EQ(world.getSkyLight(4, 9, 8), 15);
    EXPECT_EQ(world.getSkyLight(3, 9, 8), 14);
    EXPECT_EQ(world.getSkyLight(2, 9, 8), 13);
    EXPECT_EQ(world.getSkyLight(1, 9, 8), 12);
    EXPECT_EQ(world.getSkyLight(0, 9, 8), 11);
    // Vertical: free (same values near the ground).
    EXPECT_EQ(world.getSkyLight(3, 1, 8), 14);
    EXPECT_EQ(world.getSkyLight(0, 5, 8), 11);
    // The roof is opaque and reads 0.
    EXPECT_EQ(world.getSkyLight(2, 10, 8), 0);
}

TEST(LightingTest, WaterAttenuatesSkyLightVertically) {
    // Enclosed water column: the open top admits sky 15, then each water
    // cell costs kFluidLightAttenuation on the way down (the walls block
    // side entry, so the column scan's values stand).
    const BlockRegistry reg = makeRegistry();
    const BlockID stone = idOf(reg, "stone");
    World world;
    fillBox(world, 7, 0, 7, 9, 0, 9, stone); // the floor
    fillBox(world, 7, 1, 7, 9, 5, 9, stone); // a sealed box (open top)
    for (int y = 1; y <= 5; ++y) {
        world.setBlock(8, y, 8, BlockID::air()); // hollow out the column
        ASSERT_TRUE(world.setFluid(8, y, 8, FluidKind::Water, kMaxFluidLevel));
    }
    Lighting::propagateAll(world, reg);
    EXPECT_EQ(world.getSkyLight(8, 5, 8), 12); // 15 - 3
    EXPECT_EQ(world.getSkyLight(8, 4, 8), 9);  // 12 - 3
    EXPECT_EQ(world.getSkyLight(8, 3, 8), 6);
    EXPECT_EQ(world.getSkyLight(8, 2, 8), 3);
    EXPECT_EQ(world.getSkyLight(8, 1, 8), 0);
    EXPECT_EQ(world.getSkyLight(8, 0, 8), 0); // opaque floor
}

TEST(LightingTest, WaterSideEntryRaisesColumn) {
    // The same column WITHOUT walls: open air beside the water spreads in
    // at 15 - 3 = 12 per cell (max wins), so the whole column reads 12.
    const BlockRegistry reg = makeRegistry();
    World world;
    world.setBlock(8, 0, 8, idOf(reg, "stone"));
    for (int y = 1; y <= 5; ++y) {
        ASSERT_TRUE(world.setFluid(8, y, 8, FluidKind::Water, kMaxFluidLevel));
    }
    Lighting::propagateAll(world, reg);
    for (int y = 1; y <= 5; ++y) {
        EXPECT_EQ(world.getSkyLight(8, y, 8), 12);
    }
    EXPECT_EQ(world.getSkyLight(8, 0, 8), 0);
}

TEST(LightingTest, BlockLightThroughWater) {
    // A glowstone beside a 4-cell puddle under a stone ceiling (which
    // blocks the cheaper air route over the puddle): entering each water
    // cell costs kFluidLightAttenuation (12, 9, 6, 3); air beyond the
    // puddle costs 1.
    const BlockRegistry reg = makeRegistry();
    World world;
    world.setBlock(8, 0, 8, idOf(reg, "glowstone"));
    for (int x = 8; x <= 14; ++x) {
        world.setBlock(x, 1, 8, idOf(reg, "stone")); // the ceiling
        world.setBlock(x, 0, 7, idOf(reg, "stone")); // side walls, so the
        world.setBlock(x, 0, 9, idOf(reg, "stone")); // light can't route
    }                                                     // around
    for (int x = 9; x <= 12; ++x) {
        ASSERT_TRUE(world.setFluid(x, 0, 8, FluidKind::Water, kMaxFluidLevel));
    }
    Lighting::propagateAll(world, reg);
    EXPECT_EQ(world.getBlockLight(8, 0, 8), 15); // the source keeps its value
    EXPECT_EQ(world.getBlockLight(9, 0, 8), 12);
    EXPECT_EQ(world.getBlockLight(10, 0, 8), 9);
    EXPECT_EQ(world.getBlockLight(11, 0, 8), 6);
    EXPECT_EQ(world.getBlockLight(12, 0, 8), 3);
    EXPECT_EQ(world.getBlockLight(13, 0, 8), 2); // air beyond: -1
    EXPECT_EQ(world.getBlockLight(14, 0, 8), 1);
}

// -- block light ----------------------------------------------------------------

TEST(LightingTest, BlockLightAttenuation) {
    const BlockRegistry reg = makeRegistry();
    World world;
    world.setBlock(8, 0, 8, idOf(reg, "glowstone"));
    Lighting::propagateAll(world, reg);
    // Manhattan-distance attenuation; the opaque source keeps its value.
    EXPECT_EQ(world.getBlockLight(8, 0, 8), 15);
    EXPECT_EQ(world.getBlockLight(9, 0, 8), 14);
    EXPECT_EQ(world.getBlockLight(12, 0, 8), 11);
    EXPECT_EQ(world.getBlockLight(9, 1, 8), 13);
    EXPECT_EQ(world.getBlockLight(9, 1, 9), 12);
    EXPECT_EQ(world.getBlockLight(8, 0, 0), 7);
    // Beyond range (Manhattan 16 > 15).
    EXPECT_EQ(world.getBlockLight(0, 0, 0), 0);
    // Sky light is independent: the air around is fully lit.
    EXPECT_EQ(world.getSkyLight(8, 1, 8), 15);
}

TEST(LightingTest, BlockLightOccludedAndRouted) {
    const BlockRegistry reg = makeRegistry();
    const BlockID stone = idOf(reg, "stone");
    const BlockID glowstone = idOf(reg, "glowstone");

    // Full-height plane: blocks everything on the far side.
    World world;
    world.setBlock(4, 0, 8, glowstone);
    fillBox(world, 8, 0, 0, 8, 15, 15, stone);
    Lighting::propagateAll(world, reg);
    EXPECT_EQ(world.getBlockLight(3, 0, 8), 14);  // source side
    EXPECT_EQ(world.getBlockLight(12, 0, 8), 0);  // far side: sealed

    // Short wall (y 0..3): light routes over the top.
    World routed;
    routed.setBlock(4, 0, 8, glowstone);
    fillBox(routed, 8, 0, 0, 8, 3, 15, stone);
    Lighting::propagateAll(routed, reg);
    // Cheapest path: up 4 (11), over 4 (7), over 2 (5), down 4 (1).
    EXPECT_EQ(routed.getBlockLight(10, 0, 8), 1);
}

TEST(LightingTest, DimEmissionRange) {
    const BlockRegistry reg = makeRegistry();
    World world;
    world.setBlock(8, 0, 8, idOf(reg, "lantern")); // emission 5
    Lighting::propagateAll(world, reg);
    EXPECT_EQ(world.getBlockLight(8, 0, 8), 5);
    EXPECT_EQ(world.getBlockLight(9, 0, 8), 4);
    EXPECT_EQ(world.getBlockLight(11, 0, 8), 2);
    EXPECT_EQ(world.getBlockLight(12, 0, 8), 1);
    EXPECT_EQ(world.getBlockLight(13, 0, 8), 0);
}

// -- cross-chunk ----------------------------------------------------------------

TEST(LightingTest, CrossChunkBlockLight) {
    const BlockRegistry reg = makeRegistry();
    World world;
    world.getOrCreateChunk({0, 0, 0});
    world.getOrCreateChunk({1, 0, 0});
    world.setBlock(15, 0, 8, idOf(reg, "glowstone")); // boundary cell
    Lighting::propagateAll(world, reg);
    EXPECT_EQ(world.getBlockLight(15, 0, 8), 15);
    EXPECT_EQ(world.getBlockLight(16, 0, 8), 14);
    EXPECT_EQ(world.getBlockLight(17, 0, 8), 13);
}

TEST(LightingTest, CrossChunkSkyLight) {
    const BlockRegistry reg = makeRegistry();
    const BlockID stone = idOf(reg, "stone");
    World world;
    world.getOrCreateChunk({0, 0, 0});
    world.getOrCreateChunk({1, 0, 0});
    // Slab at y=5 in both chunks; a 1x1 hole in chunk 1 at (16, 5, 8).
    fillBox(world, 0, 5, 0, 15, 5, 15, stone);
    fillBox(world, 16, 5, 0, 31, 5, 15, stone);
    world.setBlock(16, 5, 8, BlockID::air());
    Lighting::propagateAll(world, reg);
    // The column below the hole is open: full sky.
    EXPECT_EQ(world.getSkyLight(16, 4, 8), 15);
    EXPECT_EQ(world.getSkyLight(16, 6, 8), 15);
    // Light crosses the chunk boundary horizontally, -1 per step.
    EXPECT_EQ(world.getSkyLight(15, 4, 8), 14);
    EXPECT_EQ(world.getSkyLight(14, 4, 8), 13);
}

// -- propagateAll properties ------------------------------------------------------

TEST(LightingTest, PropagateAllIsIdempotent) {
    const BlockRegistry reg = makeRegistry();
    World world;
    buildScenario(world, reg, std::optional<BlockPos>{BlockPos{10, 3, 8}},
                  std::nullopt);
    Lighting::propagateAll(world, reg);
    const LightSnapshot first = snapshot(world);
    Lighting::propagateAll(world, reg);
    EXPECT_EQ(snapshot(world), first);
}

// -- updateLocal vs propagateAll (the key invariant) ------------------------------

TEST(LightingTest, UpdateLocalAfterBreakingGlowstone) {
    const BlockRegistry reg = makeRegistry();
    World local, full;
    buildScenario(local, reg, std::optional<BlockPos>{BlockPos{10, 3, 8}},
                  std::nullopt);
    buildScenario(full, reg, std::nullopt, std::nullopt);
    Lighting::propagateAll(local, reg);
    local.setBlock(10, 3, 8, BlockID::air());
    Lighting::updateLocal(local, reg, {10, 3, 8});
    Lighting::propagateAll(full, reg);
    expectSameLight(local, full);
}

TEST(LightingTest, UpdateLocalAfterPlacingStone) {
    const BlockRegistry reg = makeRegistry();
    World local, full;
    buildScenario(local, reg, std::nullopt, std::nullopt);
    buildScenario(full, reg, std::nullopt, std::optional<BlockPos>{BlockPos{20, 2, 8}});
    Lighting::propagateAll(local, reg);
    local.setBlock(20, 2, 8, idOf(reg, "stone"));
    Lighting::updateLocal(local, reg, {20, 2, 8});
    Lighting::propagateAll(full, reg);
    expectSameLight(local, full);
}

TEST(LightingTest, UpdateLocalAfterPlacingGlowstone) {
    const BlockRegistry reg = makeRegistry();
    World local, full;
    buildScenario(local, reg, std::nullopt, std::nullopt);
    buildScenario(full, reg, std::optional<BlockPos>{BlockPos{22, 4, 8}},
                  std::nullopt);
    Lighting::propagateAll(local, reg);
    local.setBlock(22, 4, 8, idOf(reg, "glowstone"));
    Lighting::updateLocal(local, reg, {22, 4, 8});
    Lighting::propagateAll(full, reg);
    expectSameLight(local, full);
}

// Change far from a light source: the local update must restore the source's
// light across the re-lit box from the (unchanged) boundary, not lose it.
TEST(LightingTest, UpdateLocalKeepsDistantSourceLight) {
    const BlockRegistry reg = makeRegistry();
    World local, full;
    buildScenario(local, reg, std::optional<BlockPos>{BlockPos{10, 3, 8}},
                  std::nullopt);
    buildScenario(full, reg, std::optional<BlockPos>{BlockPos{10, 3, 8}},
                  std::optional<BlockPos>{BlockPos{30, 1, 8}});
    Lighting::propagateAll(local, reg);
    local.setBlock(30, 1, 8, idOf(reg, "stone"));
    Lighting::updateLocal(local, reg, {30, 1, 8});
    Lighting::propagateAll(full, reg);
    expectSameLight(local, full);
}

// -- updateRegion (batched cell changes, e.g. a fluid tick) ------------------------

TEST(LightingTest, UpdateRegionAfterFluidChange) {
    const BlockRegistry reg = makeRegistry();
    const BlockID stone = idOf(reg, "stone");
    World local, full;
    // A walled 4x4 pool, 3 deep, in the middle of chunk {0,0,0}; an
    // unrelated empty chunk at {3,0,0}.
    const auto buildPool = [&](World& w) {
        w.getOrCreateChunk({0, 0, 0});
        w.getOrCreateChunk({3, 0, 0});
        fillBox(w, 0, 0, 0, 15, 0, 15, stone);
        for (int x = 6; x <= 9; ++x) {
            for (int z = 6; z <= 9; ++z) {
                for (int y = 1; y <= 3; ++y) {
                    ASSERT_TRUE(w.setFluid(x, y, z, FluidKind::Water, kMaxFluidLevel));
                }
            }
        }
        for (int y = 1; y <= 3; ++y) {
            for (int x = 5; x <= 10; ++x) {
                for (int z = 5; z <= 10; ++z) {
                    if (x >= 6 && x <= 9 && z >= 6 && z <= 9) {
                        continue;
                    }
                    w.setBlock(x, y, z, stone); // the walls
                }
            }
        }
    };
    buildPool(local);
    buildPool(full);
    Lighting::propagateAll(local, reg);
    Lighting::propagateAll(full, reg);

    // Two fluid changes: drain the pool's bottom center, add a shallow
    // puddle cell at the pool's corner. The bounding box spans both.
    local.setFluid(8, 1, 8, FluidKind::Water, 0);
    local.setFluid(6, 3, 6, FluidKind::Water, 5);
    full.setFluid(8, 1, 8, FluidKind::Water, 0);
    full.setFluid(6, 3, 6, FluidKind::Water, 5);
    local.findChunkMut({0, 0, 0})->clearDirty();
    local.findChunkMut({3, 0, 0})->clearDirty();
    Lighting::updateRegion(local, reg, {6, 1, 6}, {8, 3, 8});
    Lighting::propagateAll(full, reg);
    expectSameLight(local, full);

    // Net-change dirty tracking: the pool chunk's light changed (dirty);
    // the far chunk is outside the re-lit footprint (stays clean).
    EXPECT_TRUE(local.findChunk({0, 0, 0})->dirty());
    EXPECT_FALSE(local.findChunk({3, 0, 0})->dirty());
}

// -- updateLocal dirty tracking ----------------------------------------------------

TEST(LightingTest, UpdateLocalDirtyTracking) {
    const BlockRegistry reg = makeRegistry();
    const BlockID glowstone = idOf(reg, "glowstone");
    const BlockID stone = idOf(reg, "stone");
    const ChunkPos pos{0, 0, 0};

    // (a) A light-changing edit marks the chunk dirty.
    World world;
    world.getOrCreateChunk(pos);
    world.setBlock(8, 8, 8, glowstone);
    Lighting::propagateAll(world, reg);
    world.findChunkMut(pos)->clearDirty();
    world.setBlock(8, 8, 8, BlockID::air()); // break: setBlock dirties
    world.findChunkMut(pos)->clearDirty();
    Lighting::updateLocal(world, reg, {8, 8, 8});
    EXPECT_TRUE(world.findChunk(pos)->dirty());

    // (b) Re-running the same update is a no-op: stays clean.
    world.findChunkMut(pos)->clearDirty();
    Lighting::updateLocal(world, reg, {8, 8, 8});
    EXPECT_FALSE(world.findChunk(pos)->dirty());

    // (c) Breaking air inside a sealed box changes no light: stays clean.
    World sealed;
    sealed.getOrCreateChunk(pos);
    fillBox(sealed, 7, 7, 7, 9, 9, 9, stone);
    sealed.setBlock(8, 8, 8, BlockID::air()); // hollow center
    Lighting::propagateAll(sealed, reg);
    EXPECT_EQ(sealed.getSkyLight(8, 8, 8), 0); // sealed: dark
    EXPECT_EQ(sealed.getBlockLight(8, 8, 8), 0);
    sealed.findChunkMut(pos)->clearDirty();
    sealed.setBlock(8, 8, 8, BlockID::air()); // no-op edit (dirties chunk)
    sealed.findChunkMut(pos)->clearDirty();
    Lighting::updateLocal(sealed, reg, {8, 8, 8});
    EXPECT_FALSE(sealed.findChunk(pos)->dirty());
}

// -- mesher light baking -----------------------------------------------------------

TEST(LightingTest, MesherBakesFaceLight) {
    // Faces are emitted only toward non-opaque neighbors, so a 0-light face
    // needs a DARK air cell: a sealed hollow pocket.
    const BlockRegistry reg = makeRegistry();
    const BlockID stone = idOf(reg, "stone");
    World world;
    fillBox(world, 0, 0, 0, 15, 0, 15, stone); // ground
    // Sealed 3x3x3 shell at (7..9)³, hollow at (8,8,8).
    fillBox(world, 7, 7, 7, 9, 9, 9, stone);
    world.setBlock(8, 8, 8, BlockID::air());
    Lighting::propagateAll(world, reg);
    ASSERT_EQ(world.getSkyLight(8, 8, 8), 0); // sanity: the pocket is dark

    auto layoutResult =
        AtlasLayout::parse(parseDef(R"({"columns":1,"rows":1,"tiles":{"t":[0,0]}})"));
    ASSERT_TRUE(layoutResult.ok()) << layoutResult.error().message;
    const ChunkMeshData data =
        ChunkMesher(world, reg, *layoutResult).mesh({0, 0, 0});

    // The six shell faces bordering the hollow cell are emitted (the
    // neighbor is air) and must bake light 0.
    bool sawDark = false;
    // Ground top faces far from the shell border open air: full sky.
    bool sawLit = false;
    const float kEps = 1e-6f;
    for (const MeshVertex& v : data.opaqueVertices) {
        const bool inner =
            (v.normal == Vec3{-1.0f, 0.0f, 0.0f} &&
             std::fabs(v.position.x - 9.0f) < kEps) ||
            (v.normal == Vec3{1.0f, 0.0f, 0.0f} &&
             std::fabs(v.position.x - 8.0f) < kEps) ||
            (v.normal == Vec3{0.0f, -1.0f, 0.0f} &&
             std::fabs(v.position.y - 9.0f) < kEps) ||
            (v.normal == Vec3{0.0f, 1.0f, 0.0f} &&
             std::fabs(v.position.y - 8.0f) < kEps) ||
            (v.normal == Vec3{0.0f, 0.0f, -1.0f} &&
             std::fabs(v.position.z - 9.0f) < kEps) ||
            (v.normal == Vec3{0.0f, 0.0f, 1.0f} &&
             std::fabs(v.position.z - 8.0f) < kEps);
        if (inner) {
            sawDark = true;
            EXPECT_FLOAT_EQ(v.skyLight, 0.0f);
            EXPECT_FLOAT_EQ(v.blockLight, 0.0f);
        }
        if (v.normal == Vec3{0.0f, 1.0f, 0.0f} &&
            std::fabs(v.position.y - 1.0f) < kEps && v.position.x < 6.0f) {
            sawLit = true;
            EXPECT_FLOAT_EQ(v.skyLight, 1.0f);
            EXPECT_FLOAT_EQ(v.blockLight, 0.0f);
        }
    }
    EXPECT_TRUE(sawDark);
    EXPECT_TRUE(sawLit);
}

// -- streaming (Phase 14): insert a generated column, re-light its box ----
// The streaming pipeline integrates a worker-generated column and then
// re-lights it with updateRegion(column box). The result must be exactly
// the light a full propagation of the world INCLUDING the column would
// produce. The pre-existing chunks hold no light sources, so no stale
// (void-lit) values can leak from outside the region — the documented
// sealed-cave limitation does not apply here.

namespace {

// Three columns (x 0..47): ground at y=0, a step at y=1 for x 0..7, a wall
// at x=32 (y 1..6, z=8) inside the third column, a glowstone at (40, 1, 8)
// inside the third column. `withThird` builds the third column in the world
// directly; otherwise it is left for the test to insert as a prebuilt
// chunk.
void buildStreamingScenario(World& world, const BlockRegistry& reg, bool withThird) {
    const BlockID stone = idOf(reg, "stone");
    const BlockID glow = idOf(reg, "glowstone");
    world.getOrCreateChunk({0, 0, 0});
    world.getOrCreateChunk({1, 0, 0});
    if (withThird) {
        world.getOrCreateChunk({2, 0, 0});
    }
    fillBox(world, 0, 0, 0, 31, 0, 15, stone);
    fillBox(world, 0, 1, 0, 7, 1, 15, stone);
    if (withThird) {
        fillBox(world, 32, 0, 0, 47, 0, 15, stone);
        fillBox(world, 32, 1, 8, 32, 6, 8, stone);
        world.setBlock(40, 1, 8, glow);
    }
}

// The prebuilt lower third column as a scratch chunk (what a worker
// produces).
Chunk buildThirdColumnChunk(const BlockRegistry& reg) {
    const BlockID stone = idOf(reg, "stone");
    const BlockID glow = idOf(reg, "glowstone");
    Chunk chunk;
    for (int lz = 0; lz < kChunkSize; ++lz) {
        for (int lx = 0; lx < kChunkSize; ++lx) {
            chunk.set(lx, 0, lz, stone); // ground (world x 32..47)
        }
    }
    for (int ly = 1; ly <= 6; ++ly) {
        chunk.set(0, ly, 8, stone); // the wall
    }
    chunk.set(8, 1, 8, glow); // (40, 1, 8)
    return chunk;
}

} // namespace

TEST(LightingTest, InsertedColumnRelightMatchesFullPropagation) {
    const BlockRegistry reg = makeRegistry();
    World incremental, full;
    buildStreamingScenario(incremental, reg, false);
    buildStreamingScenario(full, reg, true);
    Lighting::propagateAll(incremental, reg);
    Lighting::propagateAll(full, reg);

    // The column arrives pre-built; insert it and re-light its box,
    // exactly as the streaming pipeline does.
    ASSERT_TRUE(incremental.insertChunk({2, 0, 0},
                                        buildThirdColumnChunk(reg)));
    Lighting::updateRegion(incremental, reg, {32, 0, 0}, {47, 15, 15},
                           /*newTerrain=*/true);
    expectSameLight(incremental, full);
}

// -- LGT-2: per-block light opacity (the leaf light attenuation) ---------
// A dry cell attenuates light by its block's light_opacity (0..15) — air
// 0, glass 1, leaves 2, opaque 15 (derived) — and a full-opacity cell
// cuts propagation; a fluid cell always costs kFluidLightAttenuation.
// The "enclosed test cell" helper: a stone column (derived opacity 15)
// around the column under test, from y=1 up past the canopy, so the ONLY
// light path into the test cell (8,1,8) is vertical through the canopy
// (no horizontal routing can raise it above the rescan value).

namespace {

// A canopy of `block` at (8, y, 8) for y = 2..2+layers-1, the enclosed
// test cell at (8,1,8), full propagation. The stone enclosure runs from
// the floor (y=0 — an open y=0 row would let light route underneath) up
// past the canopy top, so the ONLY path into the test cell is vertical
// through the canopy (the rescan value, no horizontal boost).
int underCanopySky(const BlockRegistry& reg, const std::string& blockName,
                   int layers) {
    World world;
    const BlockID stone = idOf(reg, "stone");
    const BlockID block = idOf(reg, blockName);
    const int top = 2 + layers;
    for (int y = 0; y <= top; ++y) {
        world.setBlock(7, y, 8, stone);
        world.setBlock(9, y, 8, stone);
        world.setBlock(8, y, 7, stone);
        world.setBlock(8, y, 9, stone);
    }
    for (int y = 2; y < top; ++y) {
        world.setBlock(8, y, 8, block);
    }
    Lighting::propagateAll(world, reg);
    return world.getSkyLight(8, 1, 8);
}

} // namespace

// 1. Sky light through a leaf canopy: -2 per layer vertically (15 - 2*n,
// dead beyond 8 layers: 15 - 16 < 0).
TEST(LightingTest, SkyVerticalThroughLeaves) {
    const BlockRegistry reg = makeRegistry();
    EXPECT_EQ(underCanopySky(reg, "leaves", 1), 13); // 15 - 2
    EXPECT_EQ(underCanopySky(reg, "leaves", 2), 11); // 15 - 4
    EXPECT_EQ(underCanopySky(reg, "leaves", 6), 3);  // 15 - 12
    EXPECT_EQ(underCanopySky(reg, "leaves", 8), 0);  // 15 - 16 < 0: dead
}

// 2. Sky light through a 15-wide leaf WALL horizontally: -2 per cell from
// the open source column; the 8th cell is dead (15 - 16 < 0).
TEST(LightingTest, SkyHorizontalThroughLeafWall) {
    const BlockRegistry reg = makeRegistry();
    const BlockID stone = idOf(reg, "stone");
    const BlockID leaves = idOf(reg, "leaves");
    World world;
    // The wall: x 0..14 at y=8, the FULL z extent (no side routing); a
    // stone roof + floor seal the corridor over the wall region, the
    // source column at x=15 stays open.
    for (int z = 0; z < kChunkSize; ++z) {
        for (int x = 0; x <= 14; ++x) {
            world.setBlock(x, 8, z, leaves);
            world.setBlock(x, 7, z, stone);
            world.setBlock(x, 9, z, stone);
        }
    }
    Lighting::propagateAll(world, reg);
    EXPECT_EQ(world.getSkyLight(15, 8, 8), 15); // the open source column
    EXPECT_EQ(world.getSkyLight(14, 8, 8), 13); // 15 - 2
    EXPECT_EQ(world.getSkyLight(13, 8, 8), 11); // 15 - 4
    EXPECT_EQ(world.getSkyLight(8, 8, 8), 1);   // 15 - 2*7
    EXPECT_EQ(world.getSkyLight(7, 8, 8), 0);   // 15 - 2*8 < 0: dead
    EXPECT_EQ(world.getSkyLight(0, 8, 8), 0);   // the far end: sealed
}

// 3. Block light through a leaf wall (glowstone 15 behind it): the far
// wall cell reads 15 - 2*n — 1 layer 13, 3 layers 9, 6 layers 3, 8 layers
// 0 (dead: 15 - 16 < 0); an OPAQUE wall of the same thickness is the 0
// control. The wall is FULL-HEIGHT and full-z (a sealed corridor would
// let the light route over the roof), so the y=8 row is the only cheap
// path. The measurement cell is the wall's far (west) face cell — the
// cell directly beyond it pays one more air step.
TEST(LightingTest, BlockLightThroughLeaves) {
    const BlockRegistry reg = makeRegistry();
    const BlockID stone = idOf(reg, "stone");
    const BlockID leaves = idOf(reg, "leaves");
    const BlockID glowstone = idOf(reg, "glowstone");
    const struct {
        int layers;
        bool opaque;
        int farFace;
        int beyond;
    } cases[] = {
        {1, false, 13, 12},  // beyond = 13 - 1 (one air step)
        {3, false, 9, 8},    // 15 - 6
        {6, false, 3, 2},    // 15 - 12
        {8, false, 0, 0},    // 15 - 16 < 0: dead
        {8, true, 0, 0},     // opaque control: sealed
    };
    for (const auto& c : cases) {
        World world;
        const int xWest = 16 - c.layers; // the wall: x xWest..15, all y
        for (int z = 0; z < kChunkSize; ++z) {
            for (int y = 0; y < kWorldHeight; ++y) {
                for (int x = xWest; x <= 15; ++x) {
                    world.setBlock(x, y, z, c.opaque ? stone : leaves);
                }
            }
        }
        world.setBlock(16, 8, 8, glowstone); // the source (holds its 15)
        Lighting::propagateAll(world, reg);
        EXPECT_EQ(world.getBlockLight(16, 8, 8), 15)
            << "layers " << c.layers; // the source keeps its value
        EXPECT_EQ(world.getBlockLight(xWest, 8, 8), c.farFace)
            << "layers " << c.layers;
        EXPECT_EQ(world.getBlockLight(xWest - 1, 8, 8), c.beyond)
            << "layers " << c.layers;
    }
}

// 4. Glass costs 1 per vertical sky step — the documented change point
// from the pre-LGT-2 FREE vertical pass (which would read 15).
TEST(LightingTest, GlassVerticalCostOne) {
    const BlockRegistry reg = makeRegistry();
    EXPECT_EQ(underCanopySky(reg, "glass", 1), 14); // 15 - 1
    EXPECT_EQ(underCanopySky(reg, "glass", 3), 12); // 15 - 3
}

// 5. Regression: air stays free vertically / 1 horizontally, water keeps
// the fixed 3 (the fluid path — the field is not read for fluids).
TEST(LightingTest, AirAndWaterUnchanged) {
    const BlockRegistry reg = makeRegistry();
    const BlockID stone = idOf(reg, "stone");
    // (a) Air: an enclosed empty column reads 15 at the test cell (the
    //     canopy tests' geometry with no canopy).
    EXPECT_EQ(underCanopySky(reg, "air", 0), 15);
    // (b) Air horizontally: the sealed corridor (roof + floor over
    //     x 0..14, open source column at x=15) reads 15 - (15 - x).
    World corridor;
    for (int z = 0; z < kChunkSize; ++z) {
        for (int x = 0; x <= 14; ++x) {
            corridor.setBlock(x, 7, z, stone);
            corridor.setBlock(x, 9, z, stone);
        }
    }
    Lighting::propagateAll(corridor, reg);
    EXPECT_EQ(corridor.getSkyLight(15, 8, 8), 15);
    EXPECT_EQ(corridor.getSkyLight(14, 8, 8), 14); // -1 per air step
    EXPECT_EQ(corridor.getSkyLight(8, 8, 8), 8);
    EXPECT_EQ(corridor.getSkyLight(0, 8, 8), 0);   // 15 - 15: dead at 0
    // (c) Water vertically: the sealed column keeps the fixed 3 per cell
    //     (the pre-LGT-2 formula).
    World pool;
    fillBox(pool, 7, 0, 7, 9, 0, 9, stone); // the floor
    fillBox(pool, 7, 1, 7, 9, 5, 9, stone); // a sealed box (open top)
    for (int y = 1; y <= 5; ++y) {
        pool.setBlock(8, y, 8, BlockID::air());
        ASSERT_TRUE(pool.setFluid(8, y, 8, FluidKind::Water, kMaxFluidLevel));
    }
    Lighting::propagateAll(pool, reg);
    EXPECT_EQ(pool.getSkyLight(8, 5, 8), 12);
    EXPECT_EQ(pool.getSkyLight(8, 4, 8), 9);
    EXPECT_EQ(pool.getSkyLight(8, 3, 8), 6);
    EXPECT_EQ(pool.getSkyLight(8, 2, 8), 3);
    EXPECT_EQ(pool.getSkyLight(8, 1, 8), 0);
}

// 6. The key invariant with the new attenuation: a batch edit of leaf +
// glass cells — the incremental updateRegion (the rescan with per-cell
// opacity + the BFS) must produce EXACTLY the full-propagation light.
TEST(LightingTest, UpdateRegionWithLeavesMatchesFullPropagation) {
    const BlockRegistry reg = makeRegistry();
    const BlockID stone = idOf(reg, "stone");
    const BlockID leaves = idOf(reg, "leaves");
    const BlockID glass = idOf(reg, "glass");
    const BlockID glowstone = idOf(reg, "glowstone");
    const auto buildBase = [&](World& w) {
        w.getOrCreateChunk({0, 0, 0});
        fillBox(w, 0, 0, 0, 15, 0, 15, stone); // ground
        w.setBlock(4, 2, 4, glowstone);        // a source near the canopy
    };
    const auto applyEdit = [&](World& w) {
        // A 4-layer leaf canopy at (8, y 1..4, 8) + a 2-layer glass
        // column at (12, y 1..2, 12).
        for (int y = 1; y <= 4; ++y) {
            w.setBlock(8, y, 8, leaves);
        }
        for (int y = 1; y <= 2; ++y) {
            w.setBlock(12, y, 12, glass);
        }
    };
    World local, full;
    buildBase(local);
    buildBase(full);
    applyEdit(full);
    Lighting::propagateAll(local, reg);
    Lighting::propagateAll(full, reg);
    applyEdit(local);
    Lighting::updateRegion(local, reg, {8, 1, 8}, {12, 4, 12});
    expectSameLight(local, full);
}

// -- LGT-5: half-block light transmission (slab / stairs / wall / fence) --
// A half-block cell (opaque true + light_opacity 0) attenuates light
// EXACTLY like air: vertical sky passes free, every other entry costs
// 1 (max(1, opacity)) — no extra attenuation, no block.

// 1. Sky light through a half-block canopy: 15 for any number of layers
//    (air-like vertical free pass), vs stone (derived 15) = dead, vs
//    leaves (opacity 2) = -2/layer.
TEST(LightingTest, SkyVerticalThroughHalfBlock) {
    const BlockRegistry reg = makeRegistry();
    EXPECT_EQ(underCanopySky(reg, "slab5", 1), 15);
    EXPECT_EQ(underCanopySky(reg, "slab5", 5), 15);
    EXPECT_EQ(underCanopySky(reg, "stone", 1), 0);  // opaque: dead
    EXPECT_EQ(underCanopySky(reg, "leaves", 1), 13);  // the leaf contrast
}

// 2. Block light through a half-block layer (a torch-level source 15
//    below a sealed slab5 layer): the layer adds NO attenuation beyond
//    the air-like entry cost — the cell above the layer reads exactly
//    what an air cell would; a stone layer at the same place reads 0.
TEST(LightingTest, BlockLightThroughHalfBlockLayer) {
    const BlockRegistry reg = makeRegistry();
    const BlockID stone = idOf(reg, "stone");
    const BlockID slab = idOf(reg, "slab5");
    const BlockID glow = idOf(reg, "glowstone");
    // A sealed 3x3x3 box: stone floor/walls/ceiling (y 0..4, shell),
    // glowstone at (1,0,1) replaced... build the shell, then the source
    // at y=1 on the inside floor and the layer cell at y=2.
    const auto build = [&](World& w, bool withSlab) {
        fillBox(w, 0, 0, 0, 2, 4, 2, stone);      // the shell + fill
        for (int x = 0; x <= 2; ++x) {             // hollow the interior
            for (int z = 0; z <= 2; ++z) {
                w.setBlock(x, 1, z, BlockID::air());
                w.setBlock(x, 2, z, BlockID::air());
                w.setBlock(x, 3, z, BlockID::air());
            }
        }
        w.setBlock(1, 1, 1, glow);                 // the source (15)
        if (withSlab) {
            w.setBlock(1, 2, 1, slab);             // the test layer
        }
    };
    World airWorld, slabWorld, stoneWorld;
    build(airWorld, false);
    build(slabWorld, true);
    build(stoneWorld, true);
    stoneWorld.setBlock(1, 2, 1, stone);           // the opaque contrast
    Lighting::propagateAll(airWorld, reg);
    Lighting::propagateAll(slabWorld, reg);
    Lighting::propagateAll(stoneWorld, reg);
    // Directly above the layer cell: air 13 (15-1-1 vertical), slab 13
    // (the layer adds NO attenuation beyond the air-like entry cost =
    // the LGT-5 assertion), stone 11 (the vertical path is dead — only
    // the side route (0,1,1)→(0,2,1)→(0,3,1)→(1,3,1) survives: 15-4).
    EXPECT_EQ(airWorld.getBlockLight(1, 3, 1), 13);
    EXPECT_EQ(slabWorld.getBlockLight(1, 3, 1), 13);
    EXPECT_EQ(stoneWorld.getBlockLight(1, 3, 1), 11);
    // The layer cell ITSELF reads the air-like entry (14) in slab, 0 in
    // stone (15 - 15 does not enter).
    EXPECT_EQ(slabWorld.getBlockLight(1, 2, 1), 14);
    EXPECT_EQ(stoneWorld.getBlockLight(1, 2, 1), 0);
}

TEST(LightingTest, InsertedTwoChunkColumnRelightMatchesFullPropagation) {
    const BlockRegistry reg = makeRegistry();
    const BlockID stone = idOf(reg, "stone");
    const BlockID glow = idOf(reg, "glowstone");

    // Same content, but the third column spans two vertical chunks
    // (y 0..31): the ground + wall + glowstone as in the one-chunk case,
    // a second glowstone in the upper chunk, and a stone ceiling at y=31
    // that occludes the sky for the whole column.
    const auto build = [&](World& world) {
        buildStreamingScenario(world, reg, true);
        world.getOrCreateChunk({2, 1, 0});
        world.setBlock(40, 25, 8, glow); // upper-chunk glowstone
        fillBox(world, 32, 31, 0, 47, 31, 15, stone); // the ceiling
    };
    World full;
    build(full);
    // The incremental world starts WITHOUT the third column (both layers);
    // buildStreamingScenario(withThird=false) is exactly that.
    World incremental;
    buildStreamingScenario(incremental, reg, false);

    Lighting::propagateAll(incremental, reg);
    Lighting::propagateAll(full, reg);

    // Both layers of the column arrive pre-built and are inserted at once
    // (the pipeline integrates a whole column), then the merged box is
    // re-lit.
    Chunk lower = buildThirdColumnChunk(reg);
    Chunk upper;
    upper.set(8, 9, 8, glow); // (40, 25, 8)
    for (int lz = 0; lz < kChunkSize; ++lz) {
        for (int lx = 0; lx < kChunkSize; ++lx) {
            upper.set(lx, 15, lz, stone); // the ceiling (world y=31)
        }
    }
    ASSERT_TRUE(incremental.insertChunk({2, 0, 0}, std::move(lower)));
    ASSERT_TRUE(incremental.insertChunk({2, 1, 0}, std::move(upper)));
    Lighting::updateRegion(incremental, reg, {32, 0, 0}, {47, 31, 15},
                           /*newTerrain=*/true);
    expectSameLight(incremental, full);
}

// -- LGT-3: the night retreat is an AFFINE transform of the stored field --
//
// The sky field is affine in its source level S (every sky source shares
// one level, the per-cell costs are source-independent): the source-S
// field of a world equals max(0, stored - (kMaxLight - S)) of the noon
// field, cell by cell (lossless). The seam: propagateAll's skySourceLevel
// (default kMaxLight — production behavior unchanged).

// The LGT-3 main fixture (one chunk): a cave under a stone roof over
// x 0..9 (air y 1..4; light enters from the open x=10 face at cost 1 per
// air step), a glass column in the opening (x=10, z=8, y 1..4; cost 1
// per cell), a leaves wall (x=5, z 8..9, y 1..2; cost 2), a stone box
// with an OPEN-TOP water interior (walls x 1..4, z 1..4, y 1..3; water
// x 2..3, z 2..3, y 1..3 — fluid cost 3 per cell), and a stone floor.
// The stored values span 0..15 and include an active 1/0 boundary at
// (2,2,2)/(2,1,2) (the water column's cost 14/17 steps).
void buildLgt3CaveScenario(World& world, const BlockRegistry& reg) {
    const BlockID stone = idOf(reg, "stone");
    const BlockID glass = idOf(reg, "glass");
    const BlockID leaves = idOf(reg, "leaves");
    world.getOrCreateChunk({0, 0, 0});
    fillBox(world, 0, 0, 0, 15, 0, 15, stone);   // floor
    fillBox(world, 0, 5, 0, 9, 5, 15, stone);    // cave roof (x 0..9)
    fillBox(world, 10, 1, 8, 10, 4, 8, glass);   // glass column
    fillBox(world, 5, 1, 8, 5, 2, 9, leaves);    // leaves wall
    fillBox(world, 1, 1, 1, 4, 3, 4, stone);     // box walls (open top)
    for (int x = 2; x <= 3; ++x) {
        for (int z = 2; z <= 3; ++z) {
            for (int y = 1; y <= 3; ++y) {
                ASSERT_TRUE(world.setBlock(x, y, z, BlockID::air()));
                ASSERT_TRUE(world.setFluid(x, y, z, FluidKind::Water,
                                           kMaxFluidLevel));
            }
        }
    }
}

TEST(LightingTest, AffineTransformMatchesDirectPropagation) {
    const BlockRegistry reg = makeRegistry();
    // The noon field (production propagateAll, source kMaxLight).
    World noon;
    buildLgt3CaveScenario(noon, reg);
    Lighting::propagateAll(noon, reg);

    // Fixture sanity: the stored field is non-trivial (open 15, glass
    // attenuation, the leaves wall, the cave air gradient, the water
    // column's cost-3 steps with its active 1/0 boundary at cost 14/17,
    // the opaque floor).
    EXPECT_EQ(noon.getSkyLight(12, 3, 8), 15); // open to the sky
    EXPECT_EQ(noon.getSkyLight(10, 4, 8), 14); // glass, one cell down
    EXPECT_EQ(noon.getSkyLight(10, 1, 8), 14); // glass bottom (horizontal
                                               // routing beats the rescan)
    EXPECT_EQ(noon.getSkyLight(9, 4, 8), 13);  // cave air beside the glass
    EXPECT_EQ(noon.getSkyLight(5, 1, 8), 8);   // the leaves wall (cost 2)
    EXPECT_EQ(noon.getSkyLight(0, 4, 8), 4);   // cave far end, air path
    EXPECT_EQ(noon.getSkyLight(0, 1, 2), 5);   // box-corner air (free
                                               // vertical drop)
    EXPECT_EQ(noon.getSkyLight(2, 4, 2), 7);   // air above the water box
    EXPECT_EQ(noon.getSkyLight(2, 3, 2), 4);   // water step (-3)
    EXPECT_EQ(noon.getSkyLight(2, 2, 2), 1);   // water, cost 14
    EXPECT_EQ(noon.getSkyLight(2, 1, 2), 0);   // water, cost 17 (the 1/0)
    EXPECT_EQ(noon.getSkyLight(10, 0, 8), 0);  // opaque floor

    // The confluence: for each source level S, a DIRECT propagation with
    // skySourceLevel S (a twin world, the same geometry) must equal the
    // affine retreat max(0, stored - (15 - S)) of the noon field, cell by
    // cell. S = 7 is the PRODUCTION deep-night level (the kNightSkyLevel
    // moonlight floor of the day cycle), S = 10 an intermediate ramp
    // level, S = 0 the seam's mathematical property below the floor.
    // Block light is source-independent (the seam must not touch it).
    for (const int S : {7, 10, 0}) {
        World low;
        buildLgt3CaveScenario(low, reg);
        Lighting::propagateAll(low, reg, S);
        const LightSnapshot storedSnap = snapshot(noon);
        const LightSnapshot fieldSnap = snapshot(low);
        ASSERT_EQ(storedSnap.cells.size(), fieldSnap.cells.size());
        const int delta = kMaxLight - S;
        std::size_t skyDiffs = 0;
        std::size_t blockDiffs = 0;
        for (std::size_t i = 0; i < fieldSnap.cells.size(); ++i) {
            const int expect = std::max(
                0, static_cast<int>(storedSnap.cells[i].first) - delta);
            if (fieldSnap.cells[i].first !=
                static_cast<std::uint8_t>(expect)) {
                ++skyDiffs;
            }
            if (fieldSnap.cells[i].second != storedSnap.cells[i].second) {
                ++blockDiffs;
            }
        }
        EXPECT_EQ(skyDiffs, 0u) << "S = " << S
                                << " direct propagation differs from the "
                                   "affine retreat of the noon field";
        EXPECT_EQ(blockDiffs, 0u) << "S = " << S
                                  << " touched the block light";
    }
}

// A stored-0 cell means min-cost >= 15 (truly dark at EVERY source level
// <= 15) — the retreat must never light one (losslessness). The counter-
// example construction is a cost exactly 14/15 boundary: a tunnel whose
// far end sits one step beyond the light's reach.
TEST(LightingTest, StoredZeroIsLossless) {
    const BlockRegistry reg = makeRegistry();
    const BlockID stone = idOf(reg, "stone");
    const auto build = [&](World& world) {
        world.getOrCreateChunk({0, 0, 0});
        world.getOrCreateChunk({1, 0, 0});
        fillBox(world, 0, 0, 0, 31, 0, 15, stone);    // floor
        fillBox(world, 0, 2, 0, 29, 2, 15, stone);    // roof (x 0..29)
        // x 30..31 stay open: the tunnel cell (x, 1, z) reads
        // 15 - (30 - x) — (16, 1, z) = 1 (cost 14), (15, 1, z) = 0
        // (cost exactly 15, one step beyond the lit neighbor).
    };
    World noon;
    build(noon);
    Lighting::propagateAll(noon, reg);
    EXPECT_EQ(noon.getSkyLight(30, 1, 8), 15);
    EXPECT_EQ(noon.getSkyLight(16, 1, 8), 1);
    EXPECT_EQ(noon.getSkyLight(15, 1, 8), 0); // the cost-15 boundary cell
    EXPECT_EQ(noon.getSkyLight(14, 1, 8), 0);

    for (const int S : {10, 7, 0}) {
        World low;
        build(low);
        Lighting::propagateAll(low, reg, S);
        const LightSnapshot storedSnap = snapshot(noon);
        const LightSnapshot fieldSnap = snapshot(low);
        ASSERT_EQ(storedSnap.cells.size(), fieldSnap.cells.size());
        // Every stored-0 cell (including the cost-15 boundary) stays 0.
        std::size_t litFromDark = 0;
        for (std::size_t i = 0; i < fieldSnap.cells.size(); ++i) {
            if (storedSnap.cells[i].first == 0 &&
                fieldSnap.cells[i].first != 0) {
                ++litFromDark;
            }
        }
        EXPECT_EQ(litFromDark, 0u) << "S = " << S << " lit a stored-0 cell";
    }
    // The retreat contour itself: at S = 10 the lit end stops at the
    // stored-6 cell, at S = 7 at the stored-9 cell.
    World s10;
    build(s10);
    Lighting::propagateAll(s10, reg, 10);
    EXPECT_EQ(s10.getSkyLight(21, 1, 8), 1); // stored 6 -> 6 - 5
    EXPECT_EQ(s10.getSkyLight(20, 1, 8), 0); // stored 5 -> 0
    World s7;
    build(s7);
    Lighting::propagateAll(s7, reg, 7);
    EXPECT_EQ(s7.getSkyLight(24, 1, 8), 1); // stored 9 -> 9 - 8
    EXPECT_EQ(s7.getSkyLight(23, 1, 8), 0); // stored 8 -> 0
}

TEST(LightingTest, EffectiveSkyLightIntegerPins) {
    // delta = floor((15 - kNightSkyLevel) * (1 - dayFactor)) =
    // floor(8 * (1 - df)): 0 -> 8, 0.25 -> 6, 0.5 -> 4, 0.75 -> 2,
    // 1 -> 0. The grid of the spec: df in {0, 0.25, 0.5, 0.75, 1} x
    // stored in {0, 7, 15} (the deep-night column is the moonlight
    // floor: 15 -> 7, 7 -> 0, 8 -> 0).
    EXPECT_EQ(effectiveSkyLight(0, 0.0f), 0);
    EXPECT_EQ(effectiveSkyLight(0, 0.25f), 0);
    EXPECT_EQ(effectiveSkyLight(0, 0.5f), 0);
    EXPECT_EQ(effectiveSkyLight(0, 0.75f), 0);
    EXPECT_EQ(effectiveSkyLight(0, 1.0f), 0);

    EXPECT_EQ(effectiveSkyLight(7, 0.0f), 0);   // 7 - 8 < 0
    EXPECT_EQ(effectiveSkyLight(7, 0.25f), 1);  // 7 - 6
    EXPECT_EQ(effectiveSkyLight(7, 0.5f), 3);   // 7 - 4
    EXPECT_EQ(effectiveSkyLight(7, 0.75f), 5);  // 7 - 2
    EXPECT_EQ(effectiveSkyLight(7, 1.0f), 7);

    EXPECT_EQ(effectiveSkyLight(15, 0.0f), 7);  // the MC moonlight floor
    EXPECT_EQ(effectiveSkyLight(15, 0.25f), 9); // 15 - 6
    EXPECT_EQ(effectiveSkyLight(15, 0.5f), 11); // 15 - 4
    EXPECT_EQ(effectiveSkyLight(15, 0.75f), 13);  // 15 - 2
    EXPECT_EQ(effectiveSkyLight(15, 1.0f), 15);
}

TEST(LightingTest, MidnightNoonIdentity) {
    // Noon: the identity — and value-identical to the pre-LGT-3 scale
    // floor(stored * 1) for every stored level (the gate S1 self-proof).
    // Deep night: the MC moonlight floor — the stored level minus
    // (kMaxLight - kNightSkyLevel) = 8, clamped at 0 (15 -> 7, 8 -> 0,
    // 7 -> 0). Deep night is NO LONGER value-identical to the old scale
    // floor(stored * 0) = 0 — the moonlight is the feature (the 10-27
    // user verdict: an all-black night is unplayable). The float mirror
    // (the shader's normalized form) matches.
    for (int stored = 0; stored <= kMaxLight; ++stored) {
        const std::uint8_t s = static_cast<std::uint8_t>(stored);
        const int legacyNoon = static_cast<int>(std::floor(stored * 1.0));
        const int expectedNight = std::max(
            0, stored - (kMaxLight - kNightSkyLevel));
        EXPECT_EQ(effectiveSkyLight(s, 1.0f), stored) << "stored " << stored;
        EXPECT_EQ(effectiveSkyLight(s, 1.0f), std::max(0, legacyNoon));
        EXPECT_EQ(effectiveSkyLight(s, 0.0f),
                  static_cast<std::uint8_t>(expectedNight))
            << "stored " << stored;
        // The float mirror in the shader's normalized units (stored / 15):
        // noon identity, deep night the 7/15 floor (1 ulp tolerance —
        // the normalized subtraction need not be exact per level).
        EXPECT_FLOAT_EQ(effectiveSkyLightF(1.0f, 1.0f), 1.0f);
        EXPECT_NEAR(effectiveSkyLightF(static_cast<float>(stored) / 15.0f,
                                       0.0f),
                    static_cast<float>(expectedNight) / 15.0f,
                    1e-6f);
    }
    // Named deep-night pins: the moonlight floor itself.
    EXPECT_EQ(effectiveSkyLight(15, 0.0f), kNightSkyLevel); // 15 -> 7
    EXPECT_EQ(effectiveSkyLight(8, 0.0f), 0);               // 8 -> 0
    EXPECT_EQ(effectiveSkyLight(7, 0.0f), 0);               // 7 -> 0
    // The fractional mirror at the open sky (1.0): noon 1.0, deep night
    // exactly 7/15 (1.0 - fl(8/15) is representable), below-floor 0.
    EXPECT_FLOAT_EQ(effectiveSkyLightF(1.0f, 0.0f), 7.0f / 15.0f);
    EXPECT_FLOAT_EQ(effectiveSkyLightF(0.5f, 0.0f), 0.0f);
    // The fractional retreat through the ramps (the superset of
    // Minecraft's quantized steps): a half night removes 8/15 * 1/2.
    EXPECT_NEAR(effectiveSkyLightF(1.0f, 0.5f), 1.0f - 0.5f *
                                              (8.0f / 15.0f), 1e-6f);
}
