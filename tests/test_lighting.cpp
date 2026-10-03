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
// lantern=opaque emission 5.
BlockRegistry makeRegistry() {
    BlockRegistry reg;
    auto r1 = reg.registerBlock(
        parseDef(cubeDef("stone", "{}",
                         R"({"top":"t","side":"t","bottom":"t"})")));
    auto r2 = reg.registerBlock(parseDef(
        cubeDef("water",
                R"({"opaque":false,"transparent":true,"solid":false,"fluid":true})",
                R"({"top":"t","side":"t","bottom":"t"})")));
    auto r3 = reg.registerBlock(
        parseDef(cubeDef("glowstone", R"({"light_emission":15})",
                         R"({"top":"t","side":"t","bottom":"t"})")));
    auto r4 = reg.registerBlock(
        parseDef(cubeDef("lantern", R"({"light_emission":5})",
                         R"({"top":"t","side":"t","bottom":"t"})")));
    if (!r1.ok() || !r2.ok() || !r3.ok() || !r4.ok()) {
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
        ASSERT_TRUE(world.setFluid(8, y, 8, kMaxFluidLevel));
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
        ASSERT_TRUE(world.setFluid(8, y, 8, kMaxFluidLevel));
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
        ASSERT_TRUE(world.setFluid(x, 0, 8, kMaxFluidLevel));
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
                    ASSERT_TRUE(w.setFluid(x, y, z, kMaxFluidLevel));
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
    local.setFluid(8, 1, 8, 0);
    local.setFluid(6, 3, 6, 5);
    full.setFluid(8, 1, 8, 0);
    full.setFluid(6, 3, 6, 5);
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

    auto layoutResult = AtlasLayout::compute({"t"});
    ASSERT_TRUE(layoutResult.ok());
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
