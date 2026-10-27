// ChunkMesher tests: face culling (within and across chunks), pass
// assignment (opaque vs transparent), geometry conventions (local
// positions, outward normals) and per-face UV mapping.
#include <gtest/gtest.h>

#include <cmath>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "block/block_registry.hpp"
#include "renderer/texture_atlas.hpp"
#include "world/chunk_mesher.hpp"
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

// stone=opaque, glass/water=transparent (see ids).
BlockRegistry makeRegistry() {
    BlockRegistry reg;
    auto stone = reg.registerBlock(
        parseDef(cubeDef("stone", "{}", R"({"top":"t","side":"t","bottom":"t"})")));
    auto glass = reg.registerBlock(parseDef(
        cubeDef("glass", R"({"opaque":false,"transparent":true,"solid":false})",
                R"({"top":"t","side":"t","bottom":"t"})")));
    auto water = reg.registerBlock(parseDef(
        cubeDef("water",
                R"({"opaque":false,"transparent":true,"solid":false,"fluid":"water"})",
                R"({"top":"t","side":"t","bottom":"t"})")));
    // Phase 95: a wall (the 1.5-tall pillar shape).
    auto wall = reg.registerBlock(parseDef(
        R"({"id":"wall","model":{"shape":"wall","textures":{"top":"t","side":"t","bottom":"t"}}})"));
    // Phase 98: a fence (the crossed-posts shape, single side texture).
    auto fence = reg.registerBlock(parseDef(
        R"({"id":"fence","model":{"shape":"fence","textures":{"side":"t"}}})"));
    // Phase 99: a glass pane (the thin sheet, non-opaque + transparent).
    auto pane = reg.registerBlock(parseDef(
        R"({"id":"glass_pane","properties":{"opaque":false,"transparent":true},)"
        R"("model":{"shape":"glass_pane","textures":{"side":"t"}}})"));
    if (!stone.ok() || !glass.ok() || !water.ok() || !wall.ok() ||
        !fence.ok() || !pane.ok()) {
        ADD_FAILURE() << "test registry failed to build";
    }
    return reg;
}

// A 1-column strip tall enough (power of two) for every name; each name
// gets its own row (the tests only need distinct, valid UVs).
AtlasLayout makeLayout(const std::vector<std::string>& names) {
    int rows = 1;
    while (rows < static_cast<int>(names.size())) {
        rows *= 2;
    }
    std::string tiles = "{";
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (i != 0) {
            tiles += ",";
        }
        tiles += "\"" + names[i] + "\":[0," + std::to_string(i) + "]";
    }
    tiles += "}";
    auto r = AtlasLayout::parse(parseDef(
        std::string("{\"columns\":1,\"rows\":") + std::to_string(rows) +
        ",\"tiles\":" + tiles + "}"));
    if (!r.ok()) {
        ADD_FAILURE() << r.error().message;
    }
    return *r;
}

// 1 tile atlas: every UV is within [0,1].
const ChunkMeshData mesh(const World& world, const BlockRegistry& reg,
                         const ChunkPos& pos) {
    return ChunkMesher(world, reg, makeLayout({"t"})).mesh(pos);
}

// Counts vertices by face normal (4 per face for a full cube).
int countVerticesWithNormal(const std::vector<MeshVertex>& verts, Vec3 n) {
    int count = 0;
    for (const MeshVertex& v : verts) {
        if (v.normal == n) {
            ++count;
        }
    }
    return count;
}

// Collects the vertices carrying normal `n`, in emission order.
std::vector<MeshVertex> verticesWithNormal(const std::vector<MeshVertex>& verts,
                                           Vec3 n) {
    std::vector<MeshVertex> out;
    for (const MeshVertex& v : verts) {
        if (v.normal == n) {
            out.push_back(v);
        }
    }
    return out;
}

} // namespace

TEST(ChunkMesherTest, MissingChunkEmitsNothing) {
    BlockRegistry reg = makeRegistry();
    World world;
    const auto data = mesh(world, reg, {0, 0, 0});
    EXPECT_TRUE(data.opaqueEmpty());
    EXPECT_TRUE(data.transparentEmpty());
}

TEST(ChunkMesherTest, AllAirChunkEmitsNothing) {
    BlockRegistry reg = makeRegistry();
    World world;
    world.getOrCreateChunk({0, 0, 0});
    const auto data = mesh(world, reg, {0, 0, 0});
    EXPECT_TRUE(data.opaqueEmpty());
    EXPECT_TRUE(data.transparentEmpty());
}

TEST(ChunkMesherTest, SingleOpaqueBlockEmitsSixFaces) {
    BlockRegistry reg = makeRegistry();
    World world;
    world.setBlock(8, 4, 8, reg.idForName("stone"));
    const auto data = mesh(world, reg, {0, 0, 0});
    EXPECT_TRUE(data.transparentEmpty());
    EXPECT_EQ(data.opaqueVertices.size(), 24u);
    EXPECT_EQ(data.opaqueIndices.size(), 36u);
}

TEST(ChunkMesherTest, AdjacentBlocksCullSharedFace) {
    BlockRegistry reg = makeRegistry();
    World world;
    world.setBlock(8, 4, 8, reg.idForName("stone"));
    world.setBlock(9, 4, 8, reg.idForName("stone"));
    // 6 + 6 faces, minus the 1 shared face from each side: 10 visible
    // faces. Greedy merging (Phase 18): the coplanar same-tile same-light
    // pairs on the +Y, -Y, +Z and -Z planes merge into four 2x1 quads;
    // the two outer X faces stay single. 4 + 2 = 6 quads, 24 vertices.
    const auto data = mesh(world, reg, {0, 0, 0});
    EXPECT_EQ(data.opaqueVertices.size(), 24u);
    EXPECT_EQ(data.opaqueIndices.size(), 36u);
}

TEST(ChunkMesherTest, MergedQuadGeometryAndTiledUvs) {
    BlockRegistry reg = makeRegistry();
    World world;
    world.setBlock(8, 4, 8, reg.idForName("stone"));
    world.setBlock(9, 4, 8, reg.idForName("stone"));
    const auto data = mesh(world, reg, {0, 0, 0});
    const std::vector<MeshVertex> plusZ =
        verticesWithNormal(data.opaqueVertices, {0.0f, 0.0f, 1.0f});
    ASSERT_EQ(plusZ.size(), 4u); // one 2x1 quad, not two separate faces
    // Corner order (c0, c1, c2, c3) from the cube face table, extended by
    // the run width: the +Z run grows along +X from the seed at x=8.
    EXPECT_EQ(plusZ[0].position, (Vec3 {8.0f, 4.0f, 9.0f}));
    EXPECT_EQ(plusZ[1].position, (Vec3 {10.0f, 4.0f, 9.0f}));
    EXPECT_EQ(plusZ[2].position, (Vec3 {10.0f, 5.0f, 9.0f}));
    EXPECT_EQ(plusZ[3].position, (Vec3 {8.0f, 5.0f, 9.0f}));
    // Tiled UV convention (Phase 18): uv = the atlas TILE INDEX, uvLocal =
    // the position in tile units (this quad spans 2 repetitions of the
    // tile along u), tiled = 1 selects the wrapping shader path.
    for (const MeshVertex& v : plusZ) {
        EXPECT_EQ(v.uv, (Vec2 {0.0f, 0.0f}));
        EXPECT_FLOAT_EQ(v.tiled, 1.0f);
    }
    EXPECT_EQ(plusZ[0].uvLocal, (Vec2 {0.0f, 1.0f}));
    EXPECT_EQ(plusZ[1].uvLocal, (Vec2 {2.0f, 1.0f}));
    EXPECT_EQ(plusZ[2].uvLocal, (Vec2 {2.0f, 0.0f}));
    EXPECT_EQ(plusZ[3].uvLocal, (Vec2 {0.0f, 0.0f}));
}

TEST(ChunkMesherTest, SingleFaceKeepsLegacyUv) {
    BlockRegistry reg = makeRegistry();
    World world;
    world.setBlock(8, 4, 8, reg.idForName("stone"));
    const auto data = mesh(world, reg, {0, 0, 0});
    // A lone block cannot merge: every face stays on the legacy
    // atlas-space UV path (tiled = 0), bit-identical to Phase 17.
    for (const MeshVertex& v : data.opaqueVertices) {
        EXPECT_FLOAT_EQ(v.tiled, 0.0f);
    }
    const std::vector<MeshVertex> plusZ =
        verticesWithNormal(data.opaqueVertices, {0.0f, 0.0f, 1.0f});
    ASSERT_EQ(plusZ.size(), 4u);
    // Tile "t" sits at [0,0] of the 1x1 test layout: uvMin (0,0),
    // uvMax (1,1). Corner order (u0,v1),(u1,v1),(u1,v0),(u0,v0).
    EXPECT_EQ(plusZ[0].uv, (Vec2 {0.0f, 1.0f}));
    EXPECT_EQ(plusZ[1].uv, (Vec2 {1.0f, 1.0f}));
    EXPECT_EQ(plusZ[2].uv, (Vec2 {1.0f, 0.0f}));
    EXPECT_EQ(plusZ[3].uv, (Vec2 {0.0f, 0.0f}));
}

TEST(ChunkMesherTest, FlatSlabTopMergesIntoOneQuad) {
    BlockRegistry reg = makeRegistry();
    World world;
    for (int x = 0; x < kChunkSize; ++x) {
        for (int z = 0; z < kChunkSize; ++z) {
            world.setBlock(x, 4, z, reg.idForName("stone"));
        }
    }
    const auto data = mesh(world, reg, {0, 0, 0});
    // 256 cells x 6 faces = 1536 potential faces. The 16x16 top is ONE
    // quad, the 16x16 bottom one, each of the four 16x1 side walls one:
    // 6 quads = 24 vertices.
    EXPECT_EQ(data.opaqueVertices.size(), 24u);
    EXPECT_EQ(data.opaqueIndices.size(), 36u);
    const std::vector<MeshVertex> top =
        verticesWithNormal(data.opaqueVertices, {0.0f, 1.0f, 0.0f});
    ASSERT_EQ(top.size(), 4u);
    // The 16x16 top run spans 16 tiles along both local axes.
    EXPECT_EQ(top[0].uvLocal, (Vec2 {0.0f, 1.0f}));
    EXPECT_EQ(top[1].uvLocal, (Vec2 {16.0f, 1.0f}));
    EXPECT_EQ(top[2].uvLocal, (Vec2 {16.0f, -15.0f}));
    EXPECT_EQ(top[3].uvLocal, (Vec2 {0.0f, -15.0f}));
}

TEST(ChunkMesherTest, DifferentFaceLightSplitsRun) {
    BlockRegistry reg = makeRegistry();
    World world;
    world.setBlock(8, 4, 8, reg.idForName("stone"));
    world.setBlock(9, 4, 8, reg.idForName("stone"));
    // A face bakes the light of the cell it borders: give the two cells
    // above different sky light so the +Y pair cannot merge.
    world.getOrCreateChunk({0, 0, 0}).setSkyLightAt(8, 5, 8, 15);
    const auto data = mesh(world, reg, {0, 0, 0});
    // Top: two single faces (light 15 vs 0); the -Y, +Z and -Z pairs
    // still merge (their bordering cells are all unlit); plus the two
    // outer X faces: 2 + 3 + 2 = 7 quads = 28 vertices.
    EXPECT_EQ(countVerticesWithNormal(data.opaqueVertices, {0.0f, 1.0f, 0.0f}),
              8);
    EXPECT_EQ(data.opaqueVertices.size(), 28u);
}

TEST(ChunkMesherTest, DifferentTilesSplitRun) {
    BlockRegistry reg;
    auto a = reg.registerBlock(parseDef(
        cubeDef("a", "{}", R"({"top":"a","side":"a","bottom":"a"})")));
    auto b = reg.registerBlock(parseDef(
        cubeDef("b", "{}", R"({"top":"b","side":"b","bottom":"b"})")));
    ASSERT_TRUE(a.ok());
    ASSERT_TRUE(b.ok());
    World world;
    world.setBlock(8, 4, 8, reg.idForName("a"));
    world.setBlock(9, 4, 8, reg.idForName("b"));
    const auto data = ChunkMesher(world, reg, makeLayout({"a", "b"})).mesh({0, 0, 0});
    // Different tiles: nothing merges — 10 single-face quads.
    EXPECT_EQ(data.opaqueVertices.size(), 40u);
    // Single faces keep the legacy atlas-space UV (tiled = 0): the +Z
    // pair shows tile "a" (row 0: v1 = 0.5) and tile "b" (row 1: v1 = 1).
    const std::vector<MeshVertex> plusZ =
        verticesWithNormal(data.opaqueVertices, {0.0f, 0.0f, 1.0f});
    ASSERT_EQ(plusZ.size(), 8u);
    EXPECT_FLOAT_EQ(plusZ[0].tiled, 0.0f);
    EXPECT_EQ(plusZ[0].uv, (Vec2 {0.0f, 0.5f})); // c0 = (u0, v1) of "a"
    EXPECT_EQ(plusZ[4].uv, (Vec2 {0.0f, 1.0f})); // c0 = (u0, v1) of "b"
}

TEST(ChunkMesherTest, TransparentPairKeepsBothBetweenFaces) {
    BlockRegistry reg = makeRegistry();
    World world;
    world.setBlock(8, 4, 8, reg.idForName("glass"));
    world.setBlock(9, 4, 8, reg.idForName("glass"));
    const auto data = mesh(world, reg, {0, 0, 0});
    EXPECT_TRUE(data.opaqueEmpty());
    // 12 visible faces (glass does not cull glass). The coplanar pairs on
    // the Y/Z planes merge into 4 quads; the 4 X-facing faces (the two
    // outer ones plus the two BETWEEN the pair, opposite winding, same
    // plane) stay single: 8 quads = 32 vertices.
    EXPECT_EQ(data.transparentVertices.size(), 32u);
    EXPECT_EQ(countVerticesWithNormal(data.transparentVertices,
                                      {1.0f, 0.0f, 0.0f}),
              8);
    EXPECT_EQ(countVerticesWithNormal(data.transparentVertices,
                                      {-1.0f, 0.0f, 0.0f}),
              8);
}

TEST(ChunkMesherTest, CrossChunkFaceIsCulled) {
    BlockRegistry reg = makeRegistry();
    World world;
    // Local x=15 in chunk 0 and local x=0 in chunk 1: the shared face
    // must be culled in BOTH meshes.
    world.setBlock(15, 4, 4, reg.idForName("stone"));
    world.setBlock(16, 4, 4, reg.idForName("stone"));
    // Each chunk holds exactly one block: 6 faces minus the shared one.
    const auto d0 = mesh(world, reg, {0, 0, 0});
    const auto d1 = mesh(world, reg, {1, 0, 0});
    EXPECT_EQ(d0.opaqueVertices.size(), 20u);
    EXPECT_EQ(d1.opaqueVertices.size(), 20u);
}

TEST(ChunkMesherTest, MissingNeighborChunkEmitsFace) {
    BlockRegistry reg = makeRegistry();
    World world;
    // Block at local x=0, no chunk to the west: the void reads as air,
    // so the face is drawn.
    world.setBlock(0, 4, 4, reg.idForName("stone"));
    const auto data = mesh(world, reg, {0, 0, 0});
    EXPECT_EQ(data.opaqueVertices.size(), 24u);
    EXPECT_EQ(countVerticesWithNormal(data.opaqueVertices, {-1.0f, 0.0f, 0.0f}), 4);
}

TEST(ChunkMesherTest, FluidNeighborDoesNotCull) {
    BlockRegistry reg = makeRegistry();
    World world;
    world.setBlock(8, 4, 8, reg.idForName("stone"));
    world.setFluid(8, 5, 8, FluidKind::Water, kMaxFluidLevel); // water on top of stone
    const auto data = mesh(world, reg, {0, 0, 0});
    // Stone keeps all 6 faces (fluid does not occlude).
    EXPECT_EQ(data.opaqueVertices.size(), 24u);
    // Water: 6 faces minus the bottom (stone is opaque below).
    EXPECT_EQ(data.transparentVertices.size(), 20u);
}

TEST(ChunkMesherTest, FluidToFluidFacesCulled) {
    BlockRegistry reg = makeRegistry();
    World world;
    world.getOrCreateChunk({0, 0, 0});
    world.setFluid(8, 4, 8, FluidKind::Water, kMaxFluidLevel);
    world.setFluid(9, 4, 8, FluidKind::Water, kMaxFluidLevel);
    const auto data = mesh(world, reg, {0, 0, 0});
    // Two full cells: 6 + 6 faces, minus the shared face from each side
    // (fluid-to-fluid is culled, unlike same-kind non-opaque blocks).
    EXPECT_EQ(data.transparentVertices.size(), 40u);
    // The shared face is gone from both sides.
    EXPECT_EQ(countVerticesWithNormal(data.transparentVertices, {1.0f, 0.0f, 0.0f}),
              4); // only the east face of cell (9,4,8)
    EXPECT_EQ(countVerticesWithNormal(data.transparentVertices, {-1.0f, 0.0f, 0.0f}),
              4); // only the west face of cell (8,4,8)
}

TEST(ChunkMesherTest, FluidTopFaceAtLevelHeight) {
    // A level-4 fluid cell: the top face sits at y = 4 + 4/8 and the side
    // faces run from y = 4 to y = 4.5 (the scaled-cube geometry).
    BlockRegistry reg = makeRegistry();
    World world;
    world.getOrCreateChunk({0, 0, 0});
    world.setFluid(8, 4, 8, FluidKind::Water, 4);
    const auto data = mesh(world, reg, {0, 0, 0});
    // All 6 faces (isolated cell: even the bottom toward empty air).
    EXPECT_EQ(data.transparentVertices.size(), 24u);
    int top = 0;
    int sideTopCorners = 0;
    for (const MeshVertex& v : data.transparentVertices) {
        if (v.normal == Vec3{0.0f, 1.0f, 0.0f}) {
            EXPECT_FLOAT_EQ(v.position.y, 4.5f);
            ++top;
        }
        if (v.normal.x != 0.0f || v.normal.z != 0.0f) {
            // Side corners: the bottom ring at y=4, the top ring at y=4.5.
            EXPECT_TRUE(std::fabs(v.position.y - 4.0f) < 1e-6f ||
                        std::fabs(v.position.y - 4.5f) < 1e-6f);
            if (std::fabs(v.position.y - 4.5f) < 1e-6f) {
                ++sideTopCorners;
            }
        }
    }
    EXPECT_EQ(top, 4);
    EXPECT_EQ(sideTopCorners, 8); // 4 side faces x 2 top corners
}

TEST(ChunkMesherTest, OpaqueBlockAboveCullsFluidTopFace) {
    BlockRegistry reg = makeRegistry();
    World world;
    world.setBlock(8, 5, 8, reg.idForName("stone"));
    world.setFluid(8, 4, 8, FluidKind::Water, kMaxFluidLevel);
    const auto data = mesh(world, reg, {0, 0, 0});
    // Fluid: top culled (opaque above); 4 sides + bottom (empty below).
    EXPECT_EQ(data.transparentVertices.size(), 20u);
    EXPECT_EQ(countVerticesWithNormal(data.transparentVertices,
                                      {0.0f, 1.0f, 0.0f}),
              0);
    // Stone: all 6 faces (the fluid below does not occlude).
    EXPECT_EQ(data.opaqueVertices.size(), 24u);
}

TEST(ChunkMesherTest, FluidFaceBakesOwnCellLight) {
    // A fluid face is lit by its own cell's light, not the neighbor's (the
    // cell behind a water face is the water).
    BlockRegistry reg = makeRegistry();
    World world;
    world.getOrCreateChunk({0, 0, 0});
    world.setFluid(8, 4, 8, FluidKind::Water, kMaxFluidLevel);
    Chunk& chunk = world.getOrCreateChunk({0, 0, 0});
    chunk.setSkyLightAt(8, 4, 8, 5);  // the fluid cell
    chunk.setSkyLightAt(8, 5, 8, 15); // the air above (a different value)
    const auto data = mesh(world, reg, {0, 0, 0});
    int checked = 0;
    for (const MeshVertex& v : data.transparentVertices) {
        if (v.normal == Vec3{0.0f, 1.0f, 0.0f}) {
            EXPECT_FLOAT_EQ(v.skyLight, 5.0f / 15.0f);
            ++checked;
        }
    }
    EXPECT_EQ(checked, 4);
}

// The scaled (partial-height) fluid faces must keep the unit cube's
// CCW-from-outside winding (the scale is a pure y shrink of the corner
// table).
TEST(ChunkMesherTest, FluidFaceWindingIsCounterClockwiseFromOutside) {
    BlockRegistry reg = makeRegistry();
    World world;
    world.getOrCreateChunk({0, 0, 0});
    world.setFluid(8, 4, 8, FluidKind::Water, 4); // partial height: sides are 0.5 tall
    const auto data = mesh(world, reg, {0, 0, 0});

    const std::vector<Vec3> axes = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0},
                                    {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    for (const Vec3 n : axes) {
        std::size_t start = data.transparentVertices.size();
        for (std::size_t i = 0; i + 3 < data.transparentVertices.size(); ++i) {
            bool run = true;
            for (int k = 0; k < 4; ++k) {
                if (data.transparentVertices[i + k].normal != n) {
                    run = false;
                    break;
                }
            }
            if (run) {
                start = i;
                break;
            }
        }
        ASSERT_LT(start, data.transparentVertices.size())
            << "no face with normal (" << n.x << "," << n.y << "," << n.z << ")";
        const MeshVertex& v0 = data.transparentVertices[start];
        const MeshVertex& v1 = data.transparentVertices[start + 1];
        const MeshVertex& v2 = data.transparentVertices[start + 2];
        const Vec3 e1 = v1.position - v0.position;
        const Vec3 e2 = v2.position - v0.position;
        EXPECT_GT(dot(cross(e1, e2), n), 0.0f)
            << "fluid face wound clockwise from outside: normal (" << n.x
            << "," << n.y << "," << n.z << ")";
    }
}

// ============================================================================
//   PRE-3 — water ripples: the emitFluid water-top vertex mark
// ============================================================================

namespace {

// Counts the vertices carrying the water-top mark (water == 1.0).
int countWaterMarked(const std::vector<MeshVertex>& verts) {
    int count = 0;
    for (const MeshVertex& v : verts) {
        if (v.water == 1.0f) {
            ++count;
        }
    }
    return count;
}

} // namespace

TEST(ChunkMesherTest, WaterTopFaceIsMarkedForRipples) {
    // Isolated water cell (air on all six sides): exactly the TOP face's
    // four vertices carry water = 1; every other face (sides, bottom) is 0.
    BlockRegistry reg = makeRegistry();
    World world;
    world.getOrCreateChunk({0, 0, 0});
    world.setFluid(8, 4, 8, FluidKind::Water, kMaxFluidLevel);
    const auto data = mesh(world, reg, {0, 0, 0});
    ASSERT_EQ(data.transparentVertices.size(), 24u); // all 6 faces
    EXPECT_EQ(countWaterMarked(data.transparentVertices), 4);
    const Vec3 up {0.0f, 1.0f, 0.0f};
    for (const MeshVertex& v : data.transparentVertices) {
        if (v.water == 1.0f) {
            EXPECT_EQ(v.normal, up);
            EXPECT_FLOAT_EQ(v.position.y, 5.0f); // the full-level top plane
        } else {
            EXPECT_FLOAT_EQ(v.water, 0.0f);
        }
    }
}

TEST(ChunkMesherTest, CoveredWaterTopFaceIsUnmarked) {
    // A level-8 cell stacked on a level-8 cell: the LOWER cell's top face
    // is culled (never emitted, so it can never be marked); only the upper
    // cell's exposed top (the y = 6 plane) carries the four marks.
    BlockRegistry reg = makeRegistry();
    World world;
    world.getOrCreateChunk({0, 0, 0});
    world.setFluid(8, 4, 8, FluidKind::Water, kMaxFluidLevel);
    world.setFluid(8, 5, 8, FluidKind::Water, kMaxFluidLevel);
    const auto data = mesh(world, reg, {0, 0, 0});
    EXPECT_EQ(countWaterMarked(data.transparentVertices), 4);
    for (const MeshVertex& v : data.transparentVertices) {
        if (v.water == 1.0f) {
            EXPECT_FLOAT_EQ(v.position.y, 6.0f); // the upper cell's top only
        }
    }
}

TEST(ChunkMesherTest, LavaTopFaceIsNotMarked) {
    // The kind gate: lava renders through the same emitFluid path but the
    // mark is water-only (the documented deviation — lava does not shimmer).
    BlockRegistry reg = makeRegistry();
    auto lava = reg.registerBlock(parseDef(cubeDef(
        "lava",
        R"({"opaque":false,"transparent":true,"solid":false,"fluid":"lava"})",
        R"({"top":"t","side":"t","bottom":"t"})")));
    ASSERT_TRUE(lava.ok()) << lava.error().message;
    World world;
    world.getOrCreateChunk({0, 0, 0});
    world.setFluid(8, 4, 8, FluidKind::Lava, kMaxFluidLevel);
    const auto data = mesh(world, reg, {0, 0, 0});
    ASSERT_EQ(data.transparentVertices.size(), 24u); // all 6 faces emitted
    EXPECT_EQ(countWaterMarked(data.transparentVertices), 0);
    for (const MeshVertex& v : data.transparentVertices) {
        EXPECT_FLOAT_EQ(v.water, 0.0f);
    }
}

TEST(ChunkMesherTest, PartialLevelWaterTopFaceIsMarked) {
    // A level-4 cell: the top face sits at y = 4 + 0.5 (the scaled-cube
    // geometry) and is still marked — the shimmer covers partial levels.
    BlockRegistry reg = makeRegistry();
    World world;
    world.getOrCreateChunk({0, 0, 0});
    world.setFluid(8, 4, 8, FluidKind::Water, 4);
    const auto data = mesh(world, reg, {0, 0, 0});
    ASSERT_EQ(data.transparentVertices.size(), 24u);
    EXPECT_EQ(countWaterMarked(data.transparentVertices), 4);
    for (const MeshVertex& v : data.transparentVertices) {
        if (v.water == 1.0f) {
            EXPECT_FLOAT_EQ(v.position.y, 4.5f); // h = level/8 = 0.5
        }
    }
}

namespace {

// A registry with a water block whose texture sits at a NON-TRIVIAL tile
// position (the "w" tile of a 1x2 layout = [0,1]), so the tiled encoding
// (tile index + tile units) is distinguishable from the legacy
// atlas-space UVs (the 1x1 layout of the other fluid tests would make
// them coincide numerically).
BlockRegistry makeWaterTileRegistry() {
    BlockRegistry reg;
    auto water = reg.registerBlock(parseDef(cubeDef(
        "water",
        R"({"opaque":false,"transparent":true,"solid":false,"fluid":"water"})",
        R"({"top":"w","side":"w","bottom":"w"})")));
    if (!water.ok()) {
        ADD_FAILURE() << water.error().message;
    }
    return reg;
}

} // namespace

TEST(ChunkMesherTest, WaterTopFaceUsesTiledEncoding) {
    // PRE-3 (gate-110 fix): the water top face is emitted with the TILED
    // encoding — a_uv = the atlas TILE INDEX (the tile origin, all four
    // corners identical), a_uvLocal = the corner's position in TILE UNITS,
    // a_tiled = 1, a_water = 1. The fragment's fract wrap then keeps the
    // ripple's sample window inside the water tile (a boundary-grid
    // floor() on the legacy atlas-space UVs mis-identifies the tile at
    // the max corner and shimmers in the neighbour's pixels — the
    // brown-pixel bug the first gate run caught).
    BlockRegistry reg = makeWaterTileRegistry();
    World world;
    world.getOrCreateChunk({0, 0, 0});
    world.setFluid(8, 4, 8, FluidKind::Water, kMaxFluidLevel);
    const AtlasLayout layout = makeLayout({"t", "w"}); // w at [0,1]
    const auto data = ChunkMesher(world, reg, layout).mesh({0, 0, 0});
    const AtlasLayout::Tile w = layout.tiles.at("w");
    ASSERT_EQ(w.col, 0);
    ASSERT_EQ(w.row, 1);
    const Vec2 tileOrigin {static_cast<float>(w.col),
                           static_cast<float>(w.row)};
    const float tileW = 1.0f / static_cast<float>(layout.columns);
    const float tileH = 1.0f / static_cast<float>(layout.rows);
    const Vec2 u0 = AtlasLayout::uvMin(layout, "w");
    const Vec2 u1 = AtlasLayout::uvMax(layout, "w");
    std::set<std::pair<float, float>> locals;
    int marked = 0;
    for (const MeshVertex& v : data.transparentVertices) {
        if (v.water != 1.0f) {
            continue;
        }
        ++marked;
        EXPECT_FLOAT_EQ(v.tiled, 1.0f);
        // The tile origin on ALL FOUR corners (not the legacy atlas uv).
        EXPECT_EQ(v.uv, tileOrigin);
        // (tile index + tile units) is bit-equal to the v0.26.0 legacy
        // corner UV (power-of-two tile fractions are exact in binary):
        // every corner recombines to a legacy corner of the tile.
        const Vec2 legacy {(v.uv.x + v.uvLocal.x) * tileW,
                           (v.uv.y + v.uvLocal.y) * tileH};
        EXPECT_TRUE((legacy.x == u0.x || legacy.x == u1.x) &&
                    (legacy.y == u0.y || legacy.y == u1.y));
        locals.emplace(v.uvLocal.x, v.uvLocal.y);
    }
    EXPECT_EQ(marked, 4);
    // Exactly the four tile-unit corners, once each.
    EXPECT_EQ(locals.size(), 4u);
    EXPECT_EQ(locals.count({0.0f, 1.0f}), 1u);
    EXPECT_EQ(locals.count({1.0f, 1.0f}), 1u);
    EXPECT_EQ(locals.count({1.0f, 0.0f}), 1u);
    EXPECT_EQ(locals.count({0.0f, 0.0f}), 1u);
}

TEST(ChunkMesherTest, WaterSidesAndBottomKeepLegacyEncoding) {
    // The NON-top faces of a water cell (4 sides + bottom = 20 vertices)
    // keep the pre-PRE-3 legacy encoding untouched: atlas-space uv on the
    // tile corners, tiled = 0, water = 0 — only the top face moved to the
    // tiled convention (the ripple's seam).
    BlockRegistry reg = makeWaterTileRegistry();
    World world;
    world.getOrCreateChunk({0, 0, 0});
    world.setFluid(8, 4, 8, FluidKind::Water, kMaxFluidLevel);
    const AtlasLayout layout = makeLayout({"t", "w"});
    const auto data = ChunkMesher(world, reg, layout).mesh({0, 0, 0});
    ASSERT_EQ(data.transparentVertices.size(), 24u);
    const Vec2 u0 = AtlasLayout::uvMin(layout, "w");
    const Vec2 u1 = AtlasLayout::uvMax(layout, "w");
    const Vec3 up {0.0f, 1.0f, 0.0f};
    int legacy = 0;
    for (const MeshVertex& v : data.transparentVertices) {
        if (v.normal == up) {
            continue; // the tiled top face (the other test)
        }
        ++legacy;
        EXPECT_FLOAT_EQ(v.tiled, 0.0f);
        EXPECT_FLOAT_EQ(v.water, 0.0f);
        EXPECT_TRUE(v.uv.x == u0.x || v.uv.x == u1.x);
        EXPECT_TRUE(v.uv.y == u0.y || v.uv.y == u1.y);
    }
    EXPECT_EQ(legacy, 20);
}

TEST(ChunkMesherTest, OpaqueNeighborCullsTransparentFace) {
    BlockRegistry reg = makeRegistry();
    World world;
    world.setBlock(8, 4, 8, reg.idForName("stone"));
    world.setBlock(8, 5, 8, reg.idForName("glass"));
    const auto data = mesh(world, reg, {0, 0, 0});
    EXPECT_EQ(data.opaqueVertices.size(), 24u);
    // Glass: bottom face culled by the opaque stone.
    EXPECT_EQ(data.transparentVertices.size(), 20u);
    EXPECT_EQ(countVerticesWithNormal(data.transparentVertices,
                                      {0.0f, -1.0f, 0.0f}),
              0);
}

TEST(ChunkMesherTest, TransparentBlocksGoToTransparentPass) {
    BlockRegistry reg = makeRegistry();
    World world;
    world.setBlock(8, 4, 8, reg.idForName("glass"));
    const auto data = mesh(world, reg, {0, 0, 0});
    EXPECT_TRUE(data.opaqueEmpty());
    EXPECT_EQ(data.transparentVertices.size(), 24u);
}

TEST(ChunkMesherTest, MixedBlockTypesSplitByPass) {
    BlockRegistry reg = makeRegistry();
    World world;
    world.setBlock(7, 4, 8, reg.idForName("stone"));
    world.setFluid(8, 4, 8, FluidKind::Water, kMaxFluidLevel); // water directly next to the stone
    const auto data = mesh(world, reg, {0, 0, 0});
    // Stone: 6 faces (the fluid neighbor does not occlude). Water: 5 faces
    // (the west face is culled by the opaque stone).
    EXPECT_EQ(data.opaqueVertices.size(), 24u);
    EXPECT_EQ(data.transparentVertices.size(), 20u);
}

TEST(ChunkMesherTest, VerticalCrossChunkFaceIsCulled) {
    BlockRegistry reg = makeRegistry();
    World world;
    world.setBlock(4, 15, 4, reg.idForName("stone")); // top row of lower chunk
    world.setBlock(4, 16, 4, reg.idForName("stone")); // bottom row of upper
    // Each chunk holds exactly one block: 6 faces minus the shared one.
    const auto d0 = mesh(world, reg, {0, 0, 0});
    const auto d1 = mesh(world, reg, {0, 1, 0});
    EXPECT_EQ(d0.opaqueVertices.size(), 20u);
    EXPECT_EQ(d1.opaqueVertices.size(), 20u);
}

TEST(ChunkMesherTest, BelowWorldBottomEmitsFace) {
    BlockRegistry reg = makeRegistry();
    World world;
    // y=-1 is out of world: reads air, bottom face is drawn.
    world.setBlock(4, 0, 4, reg.idForName("stone"));
    const auto data = mesh(world, reg, {0, 0, 0});
    EXPECT_EQ(data.opaqueVertices.size(), 24u);
    EXPECT_EQ(countVerticesWithNormal(data.opaqueVertices, {0.0f, -1.0f, 0.0f}), 4);
}

TEST(ChunkMesherTest, PositionsAreChunkLocal) {
    BlockRegistry reg = makeRegistry();
    World world;
    // World (16, 3, 16) == chunk (1, 0, 1), local (0, 3, 0).
    world.setBlock(16, 3, 16, reg.idForName("stone"));
    const auto data = mesh(world, reg, {1, 0, 1});
    ASSERT_EQ(data.opaqueVertices.size(), 24u);
    for (const MeshVertex& v : data.opaqueVertices) {
        EXPECT_GE(v.position.x, 0.0f);
        EXPECT_LE(v.position.x, 16.0f);
        EXPECT_GE(v.position.y, 0.0f);
        EXPECT_LE(v.position.y, 16.0f);
        EXPECT_GE(v.position.z, 0.0f);
        EXPECT_LE(v.position.z, 16.0f);
    }
    // The lone block occupies exactly local [0,1] x [3,4] x [0,1].
    float minX = 100.0f, minY = 100.0f, minZ = 100.0f;
    float maxX = -100.0f, maxY = -100.0f, maxZ = -100.0f;
    for (const MeshVertex& v : data.opaqueVertices) {
        minX = std::min(minX, v.position.x);
        minY = std::min(minY, v.position.y);
        minZ = std::min(minZ, v.position.z);
        maxX = std::max(maxX, v.position.x);
        maxY = std::max(maxY, v.position.y);
        maxZ = std::max(maxZ, v.position.z);
    }
    EXPECT_FLOAT_EQ(minX, 0.0f);
    EXPECT_FLOAT_EQ(maxX, 1.0f);
    EXPECT_FLOAT_EQ(minY, 3.0f);
    EXPECT_FLOAT_EQ(maxY, 4.0f);
    EXPECT_FLOAT_EQ(minZ, 0.0f);
    EXPECT_FLOAT_EQ(maxZ, 1.0f);
}

// GL front faces are CCW viewed from outside. For each emitted face, the
// triangle (v0, v1, v2) must sweep toward the face normal:
// cross(v1 - v0, v2 - v0) must point along the normal. This is the guard
// for the Phase 2 +Y/-Y winding bug (top faces culled from above, bottom
// faces visible from above).
TEST(ChunkMesherTest, FaceWindingIsCounterClockwiseFromOutside) {
    BlockRegistry reg = makeRegistry();
    World world;
    world.setBlock(8, 4, 8, reg.idForName("stone"));
    const auto data = mesh(world, reg, {0, 0, 0});
    ASSERT_EQ(data.opaqueVertices.size(), 24u);

    const std::vector<Vec3> axes = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0},
                                    {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    for (const Vec3 n : axes) {
        // The 4 vertices of one face are emitted contiguously; find the run.
        std::size_t start = data.opaqueVertices.size();
        for (std::size_t i = 0; i + 3 < data.opaqueVertices.size(); ++i) {
            bool run = true;
            for (int k = 0; k < 4; ++k) {
                if (data.opaqueVertices[i + k].normal != n) {
                    run = false;
                    break;
                }
            }
            if (run) {
                start = i;
                break;
            }
        }
        ASSERT_LT(start, data.opaqueVertices.size())
            << "no face with normal (" << n.x << "," << n.y << "," << n.z << ")";
        const MeshVertex& v0 = data.opaqueVertices[start];
        const MeshVertex& v1 = data.opaqueVertices[start + 1];
        const MeshVertex& v2 = data.opaqueVertices[start + 2];
        const Vec3 e1 = v1.position - v0.position;
        const Vec3 e2 = v2.position - v0.position;
        EXPECT_GT(dot(cross(e1, e2), n), 0.0f)
            << "face wound clockwise from outside: normal (" << n.x << ","
            << n.y << "," << n.z << ")";
    }
}

TEST(ChunkMesherTest, FaceNormalsPointOutward) {
    BlockRegistry reg = makeRegistry();
    World world;
    world.setBlock(8, 4, 8, reg.idForName("stone"));
    const auto data = mesh(world, reg, {0, 0, 0});
    const std::vector<Vec3> axes = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0},
                                    {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    for (const Vec3 n : axes) {
        EXPECT_EQ(countVerticesWithNormal(data.opaqueVertices, n), 4)
            << "normal (" << n.x << "," << n.y << "," << n.z << ")";
    }
    // The +Y face of a block at y=4 sits at y=5.
    for (const MeshVertex& v : data.opaqueVertices) {
        if (v.normal == Vec3{0.0f, 1.0f, 0.0f}) {
            EXPECT_FLOAT_EQ(v.position.y, 5.0f);
        }
    }
}

TEST(ChunkMesherTest, UvsCoverTheFaceTile) {
    BlockRegistry reg = makeRegistry();
    World world;
    world.setBlock(8, 4, 8, reg.idForName("stone"));
    const AtlasLayout layout = makeLayout({"t"});
    const auto data = ChunkMesher(world, reg, layout).mesh({0, 0, 0});
    const Vec2 u0 = AtlasLayout::uvMin(layout, "t");
    const Vec2 u1 = AtlasLayout::uvMax(layout, "t");
    ASSERT_FALSE(data.opaqueVertices.empty());
    for (const MeshVertex& v : data.opaqueVertices) {
        EXPECT_GE(v.uv.x, u0.x);
        EXPECT_LE(v.uv.x, u1.x);
        EXPECT_GE(v.uv.y, u0.y);
        EXPECT_LE(v.uv.y, u1.y);
    }
}

TEST(ChunkMesherTest, PerFaceTextureSelection) {
    // Two-tile atlas: names sort to a = u[0, 0.5), b = u[0.5, 1].
    // Block with top="b", side/bottom="a": each face group must sample
    // its own tile.
    BlockRegistry reg = makeRegistry();
    auto grass2 = reg.registerBlock(
        parseDef(cubeDef("grass2", "{}",
                         R"({"top":"b","side":"a","bottom":"a"})")));
    ASSERT_TRUE(grass2.ok()) << grass2.error().message;
    World world;
    world.setBlock(8, 4, 8, *grass2);

    const AtlasLayout layout = makeLayout({"a", "b"});
    const auto data = ChunkMesher(world, reg, layout).mesh({0, 0, 0});
    ASSERT_EQ(data.opaqueVertices.size(), 24u);

    const Vec2 a0 = AtlasLayout::uvMin(layout, "a");
    const Vec2 a1 = AtlasLayout::uvMax(layout, "a");
    const Vec2 b0 = AtlasLayout::uvMin(layout, "b");
    const Vec2 b1 = AtlasLayout::uvMax(layout, "b");

    for (const MeshVertex& v : data.opaqueVertices) {
        if (v.normal == Vec3{0.0f, 1.0f, 0.0f}) {
            EXPECT_GE(v.uv.x, b0.x);
            EXPECT_LE(v.uv.x, b1.x); // top face -> tile "b"
        } else {
            EXPECT_GE(v.uv.x, a0.x);
            EXPECT_LE(v.uv.x, a1.x); // sides + bottom -> tile "a"
        }
    }
}

// -- cross plants (Phase 45) ---------------------------------------------------

// A registry with a cross-plant block (non-solid, non-opaque, transparent —
// the combination the block registry enforces for the cross shape).
BlockRegistry makeCrossRegistry() {
    BlockRegistry reg = makeRegistry();
    auto flower = reg.registerBlock(parseDef(
        R"({"id":"flower","properties":{"solid":false,"opaque":false,)"
        R"("transparent":true},"model":{"shape":"cross",)"
        R"("textures":{"cross":"f"}}})"));
    if (!flower.ok()) {
        ADD_FAILURE() << flower.error().message;
    }
    return reg;
}

TEST(ChunkMesherTest, CrossBlockEmitsTwoQuadsToTransparentPass) {
    BlockRegistry reg = makeCrossRegistry();
    World world;
    world.setBlock(8, 4, 8, reg.idForName("flower"));
    const AtlasLayout layout = makeLayout({"t", "f"});
    const auto data = ChunkMesher(world, reg, layout).mesh({0, 0, 0});
    // Two quads: 8 vertices, 12 indices — all in the transparent pass.
    EXPECT_TRUE(data.opaqueEmpty());
    EXPECT_EQ(data.transparentVertices.size(), 8u);
    EXPECT_EQ(data.transparentIndices.size(), 12u);
}

// The quad corners are the two cell diagonals (chunk-local):
//   quad A: (0,0,0) (1,0,1) (1,1,1) (0,1,0)
//   quad B: (1,0,0) (0,0,1) (0,1,1) (1,1,0)
TEST(ChunkMesherTest, CrossQuadCornersAreCellDiagonals) {
    BlockRegistry reg = makeCrossRegistry();
    World world;
    world.setBlock(8, 4, 8, reg.idForName("flower"));
    const auto data = ChunkMesher(world, reg, makeLayout({"t", "f"}))
                          .mesh({0, 0, 0});
    const std::vector<Vec3> expected = {
        {8, 4, 8},  {9, 4, 9},  {9, 5, 9},  {8, 5, 8},
        {9, 4, 8},  {8, 4, 9},  {8, 5, 9},  {9, 5, 8},
    };
    ASSERT_EQ(data.transparentVertices.size(), expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        EXPECT_EQ(data.transparentVertices[i].position, expected[i]);
    }
}

// The cross texture spans the full tile on every vertex (u covers the tile
// width, v the tile height — the texture is upright: v1 at the bottom).
TEST(ChunkMesherTest, CrossUvCoversTheTile) {
    BlockRegistry reg = makeCrossRegistry();
    World world;
    world.setBlock(8, 4, 8, reg.idForName("flower"));
    const AtlasLayout layout = makeLayout({"t", "f"});
    const auto data = ChunkMesher(world, reg, layout).mesh({0, 0, 0});
    const Vec2 f0 = AtlasLayout::uvMin(layout, "f");
    const Vec2 f1 = AtlasLayout::uvMax(layout, "f");
    ASSERT_FALSE(data.transparentVertices.empty());
    for (const MeshVertex& v : data.transparentVertices) {
        EXPECT_TRUE(v.uv.x == f0.x || v.uv.x == f1.x);
        EXPECT_TRUE(v.uv.y == f0.y || v.uv.y == f1.y);
    }
    // Bottom corners carry the tile's v1 (upright texture).
    int bottom = 0;
    for (const MeshVertex& v : data.transparentVertices) {
        if (v.position.y == 4.0f) {
            EXPECT_FLOAT_EQ(v.uv.y, f1.y);
            ++bottom;
        }
    }
    EXPECT_EQ(bottom, 4);
}

// The cross cell is lit by its own light (like the fluid cells), not the
// neighbor's.
TEST(ChunkMesherTest, CrossBakesOwnCellLight) {
    BlockRegistry reg = makeCrossRegistry();
    World world;
    world.setBlock(8, 4, 8, reg.idForName("flower"));
    Chunk& chunk = world.getOrCreateChunk({0, 0, 0});
    chunk.setSkyLightAt(8, 4, 8, 5);  // the cross cell
    chunk.setSkyLightAt(8, 5, 8, 15); // the air above (a different value)
    const auto data =
        ChunkMesher(world, reg, makeLayout({"t", "f"})).mesh({0, 0, 0});
    ASSERT_EQ(data.transparentVertices.size(), 8u);
    for (const MeshVertex& v : data.transparentVertices) {
        EXPECT_FLOAT_EQ(v.skyLight, 5.0f / 15.0f);
    }
}

// The cross quad normals are up (full-brightness face shade) and the
// non-opaque cross block never culls an adjacent cube face.
TEST(ChunkMesherTest, CrossBlockDoesNotCullNeighborFace) {
    BlockRegistry reg = makeCrossRegistry();
    World world;
    world.setBlock(7, 4, 8, reg.idForName("stone"));
    world.setBlock(8, 4, 8, reg.idForName("flower"));
    const auto data =
        ChunkMesher(world, reg, makeLayout({"t", "f"})).mesh({0, 0, 0});
    // The stone's +X face (toward the cross) is still emitted ...
    int plusX = 0;
    for (const MeshVertex& v : data.opaqueVertices) {
        if (v.normal == Vec3{1.0f, 0.0f, 0.0f}) {
            ++plusX;
        }
    }
    EXPECT_EQ(plusX, 4);
    // ... and the cross still emits its two quads.
    EXPECT_EQ(data.transparentVertices.size(), 8u);
}

// ============================================================================
//   Phase 96 — wall connection (the seamless merged wall geometry)
// ============================================================================

TEST(ChunkMesherTest, Phase96WallAirKeepsFullSideFaces) {
    // A lone wall: every side face spans the full wall height (y 0..1.5) —
    // the conservative baseline (no neighbor to share with).
    BlockRegistry reg = makeRegistry();
    World world;
    world.setBlock(8, 4, 8, reg.idForName("wall"));
    const auto data = mesh(world, reg, {0, 0, 0});
    EXPECT_TRUE(data.transparentEmpty());
    EXPECT_EQ(data.opaqueVertices.size(), 24u);
    // The +Z side (air neighbor) reaches both the bottom (y=4) and the cap
    // top (y=5.5).
    const std::vector<MeshVertex> plusZ =
        verticesWithNormal(data.opaqueVertices, {0.0f, 0.0f, 1.0f});
    ASSERT_EQ(plusZ.size(), 4u);
    float minY = 99.0f, maxY = -99.0f;
    for (const MeshVertex& v : plusZ) {
        minY = v.position.y < minY ? v.position.y : minY;
        maxY = v.position.y > maxY ? v.position.y : maxY;
    }
    EXPECT_EQ(minY, 4.0f);
    EXPECT_EQ(maxY, 5.5f);
}

TEST(ChunkMesherTest, Phase96WallWallCullsSharedFace) {
    // Two adjacent walls: the shared side faces are fully internal (both
    // pillars span y 0..1.5) -> culled from BOTH (the merged wall line).
    BlockRegistry reg = makeRegistry();
    World world;
    world.setBlock(8, 4, 8, reg.idForName("wall"));
    world.setBlock(9, 4, 8, reg.idForName("wall"));
    const auto data = mesh(world, reg, {0, 0, 0});
    // 24 + 24 - 8 (one shared side face dropped from each) = 40.
    EXPECT_EQ(data.opaqueVertices.size(), 40u);
    // The only -X face left is the x=8 wall's AIR side (the x=9 wall's -X
    // toward it is culled); symmetric for +X.
    const std::vector<MeshVertex> minusX =
        verticesWithNormal(data.opaqueVertices, {-1.0f, 0.0f, 0.0f});
    ASSERT_EQ(minusX.size(), 4u);
    for (const MeshVertex& v : minusX) {
        EXPECT_EQ(v.position.x, 8.0f);
    }
    const std::vector<MeshVertex> plusX =
        verticesWithNormal(data.opaqueVertices, {1.0f, 0.0f, 0.0f});
    ASSERT_EQ(plusX.size(), 4u);
    for (const MeshVertex& v : plusX) {
        EXPECT_EQ(v.position.x, 10.0f);
    }
}

TEST(ChunkMesherTest, Phase96WallFullBlockEmitsCapBandOnly) {
    // A wall beside a full opaque block: the block's face toward the wall
    // is culled (the wall is opaque), so the wall's lower band (y 0..1) on
    // that side is internal too -> the wall emits only the exposed cap band
    // (y 1..1.5, above the block's top).
    BlockRegistry reg = makeRegistry();
    World world;
    world.setBlock(8, 4, 8, reg.idForName("wall"));
    world.setBlock(9, 4, 8, reg.idForName("stone"));
    const auto data = mesh(world, reg, {0, 0, 0});
    // The wall keeps 6 faces (its block-side is one cap-band quad, 4 verts)
    // + the stone keeps 5 (its -X is culled): 24 + 20 = 44.
    EXPECT_EQ(data.opaqueVertices.size(), 44u);
    // The stone's -X face is gone: the only -X verts are the wall's air
    // side (x=8), still the FULL height.
    const std::vector<MeshVertex> minusX =
        verticesWithNormal(data.opaqueVertices, {-1.0f, 0.0f, 0.0f});
    ASSERT_EQ(minusX.size(), 4u);
    float minY = 99.0f, maxY = -99.0f;
    for (const MeshVertex& v : minusX) {
        EXPECT_EQ(v.position.x, 8.0f);
        minY = v.position.y < minY ? v.position.y : minY;
        maxY = v.position.y > maxY ? v.position.y : maxY;
    }
    EXPECT_EQ(minY, 4.0f);
    EXPECT_EQ(maxY, 5.5f);
    // The wall's +X side (toward the stone) is the CAP BAND only: 4 verts,
    // all y in [5.0, 5.5] at x=9 (the stone's own +X at x=10 is full).
    const std::vector<MeshVertex> plusX =
        verticesWithNormal(data.opaqueVertices, {1.0f, 0.0f, 0.0f});
    ASSERT_EQ(plusX.size(), 8u);
    int wallBand = 0;
    float bandMinY = 99.0f, bandMaxY = -99.0f;
    for (const MeshVertex& v : plusX) {
        if (v.position.x == 9.0f) {
            ++wallBand;
            bandMinY = v.position.y < bandMinY ? v.position.y : bandMinY;
            bandMaxY = v.position.y > bandMaxY ? v.position.y : bandMaxY;
        }
    }
    EXPECT_EQ(wallBand, 4);
    EXPECT_EQ(bandMinY, 5.0f); // the lower band (y 0..1) is internal
    EXPECT_EQ(bandMaxY, 5.5f);
}

// ============================================================================
//   Phase 98 — fence (crossed posts + connection bars)
// ============================================================================

TEST(ChunkMesherTest, Phase98LoneFenceEmitsPosts) {
    // A lone fence: two crossed posts, 6 faces each (4 sides + top + bottom,
    // no opaque neighbors) = 12 quads = 48 vertices, all in the opaque
    // sub-mesh.
    BlockRegistry reg = makeRegistry();
    World world;
    world.setBlock(8, 4, 8, reg.idForName("fence"));
    const auto data = mesh(world, reg, {0, 0, 0});
    EXPECT_TRUE(data.transparentEmpty());
    EXPECT_EQ(data.opaqueVertices.size(), 48u);
}

TEST(ChunkMesherTest, Phase98FencePairEmitsBars) {
    // Two adjacent fences (an X line): each emits its posts (12 quads) +
    // one connection bar toward its neighbor (4 quads — the open-ended box)
    // = 16 quads each -> 32 quads = 128 vertices.
    BlockRegistry reg = makeRegistry();
    World world;
    world.setBlock(8, 4, 8, reg.idForName("fence"));
    world.setBlock(9, 4, 8, reg.idForName("fence"));
    const auto data = mesh(world, reg, {0, 0, 0});
    EXPECT_EQ(data.opaqueVertices.size(), 128u);
}

TEST(ChunkMesherTest, Phase98FenceBesideBlockKeepsBlockFace) {
    // A fence beside a full block: the fence emits a bar toward the block,
    // and the block's face toward the fence is NOT culled (the fence is thin
    // — Phase 98 greedy fix: only opaque NON-fence neighbors occlude).
    BlockRegistry reg = makeRegistry();
    World world;
    world.setBlock(8, 4, 8, reg.idForName("fence"));
    world.setBlock(9, 4, 8, reg.idForName("stone"));
    const auto data = mesh(world, reg, {0, 0, 0});
    // Fence: 12 post quads + 1 bar (4 quads) = 16 quads = 64 vertices;
    // stone: 6 faces = 24 vertices. 22 quads = 88 vertices. (If the stone's
    // -X face were wrongly culled by the thin fence, the total would be 84.)
    EXPECT_EQ(data.opaqueVertices.size(), 88u);
    // The stone's -X face (toward the fence) survives: 4 verts at x=9.
    int stoneMinusX = 0;
    for (const MeshVertex& v : data.opaqueVertices) {
        if (v.normal == Vec3{-1.0f, 0.0f, 0.0f} && v.position.x == 9.0f) {
            ++stoneMinusX;
        }
    }
    EXPECT_EQ(stoneMinusX, 4);
}

TEST(ChunkMesherTest, Phase98FenceNoBarTowardWall) {
    // A fence beside a WALL: no bar (the bar only reaches a fence / full
    // block — the wall pillar covers the gap) -> the fence keeps its 12
    // post quads; the wall keeps its 6 faces: 18 quads = 72 vertices.
    BlockRegistry reg = makeRegistry();
    World world;
    world.setBlock(8, 4, 8, reg.idForName("fence"));
    world.setBlock(9, 4, 8, reg.idForName("wall"));
    const auto data = mesh(world, reg, {0, 0, 0});
    EXPECT_EQ(data.opaqueVertices.size(), 72u);
}

// ============================================================================
//   Phase 99 — glass pane (the 1/16-thick sheet)
// ============================================================================

TEST(ChunkMesherTest, Phase99LoneGlassPaneEmitsSheetAndRims) {
    // A lone pane: the two sheet sides + four rim slivers = 6 quads = 24
    // vertices, all in the TRANSPARENT sub-mesh (blended, double-sided).
    BlockRegistry reg = makeRegistry();
    World world;
    world.setBlock(8, 4, 8, reg.idForName("glass_pane"));
    const auto data = mesh(world, reg, {0, 0, 0});
    EXPECT_TRUE(data.opaqueEmpty());
    EXPECT_EQ(data.transparentVertices.size(), 24u);
    // The sheet sides carry the +-Z normals, full 1x1 extent at the
    // z-centered plane (z 8.46875 / 8.53125 for the cell at z=8).
    const std::vector<MeshVertex> plusZ =
        verticesWithNormal(data.transparentVertices, {0.0f, 0.0f, 1.0f});
    ASSERT_EQ(plusZ.size(), 4u);
    for (const MeshVertex& v : plusZ) {
        EXPECT_FLOAT_EQ(v.position.z, 8.53125f);
    }
}

TEST(ChunkMesherTest, Phase99GlassPanePairCullsSharedRim) {
    // Two panes in an X row: each emits its 2 sheet sides + 3 rim slivers
    // (the shared boundary rim is culled — the transparent version of the
    // Phase 96 connection culling: two coplanar blended rims would
    // double-draw a darker seam) = 5 quads each -> 10 quads = 40 vertices.
    BlockRegistry reg = makeRegistry();
    World world;
    world.setBlock(8, 4, 8, reg.idForName("glass_pane"));
    world.setBlock(9, 4, 8, reg.idForName("glass_pane"));
    const auto data = mesh(world, reg, {0, 0, 0});
    EXPECT_TRUE(data.opaqueEmpty());
    EXPECT_EQ(data.transparentVertices.size(), 40u);
    // The shared boundary (x=9.0) carries NO +-X rim verts: the only +-X
    // rims are the two outer ends (x=8.0 and x=10.0), 4 + 4 = 8 verts.
    int sideRims = 0;
    for (const MeshVertex& v : data.transparentVertices) {
        if (v.normal == Vec3{1.0f, 0.0f, 0.0f} ||
            v.normal == Vec3{-1.0f, 0.0f, 0.0f}) {
            ++sideRims;
        }
    }
    EXPECT_EQ(sideRims, 8);
}

// -- GEN-2: the stageful cross (the sapling) scales with its state -------

namespace {

// A cross registry + a STATEFUL cross (the sapling fixture: 8 states,
// default 0 — the shipped sapling pair).
BlockRegistry makeSaplingRegistry() {
    BlockRegistry reg = makeCrossRegistry();
    auto sapling = reg.registerBlock(parseDef(
        R"({"id":"sapling","properties":{"solid":false,"opaque":false,)"
        R"("transparent":true},"model":{"shape":"cross","state_count":8,)"
        R"("state_default":0,"textures":{"cross":"sap"}}})"));
    if (!sapling.ok()) {
        ADD_FAILURE() << sapling.error().message;
    }
    return reg;
}

} // namespace

TEST(ChunkMesherTest, SaplingCrossScalesWithStage) {
    // Stage 3 of 8: scale = (3+1)/8 = 0.5 — every quad corner re-anchored
    // around the cell centre (8.5, 4.5, 8.5 for the cell at (8, 4, 8)):
    // the unit diagonal shrinks to half, centred.
    BlockRegistry reg = makeSaplingRegistry();
    World world;
    world.setBlock(8, 4, 8, reg.idForName("sapling"));
    world.setBlockState(8, 4, 8, 3);
    const auto data =
        ChunkMesher(world, reg, makeLayout({"t", "f", "sap"})).mesh({0, 0, 0});
    ASSERT_EQ(data.transparentVertices.size(), 8u);
    // Quad A at half scale: (8.25,4.25,8.25) (8.75,4.25,8.75)
    // (8.75,4.75,8.75) (8.25,4.75,8.25) — the cell centre (8.5,4.5,8.5)
    // plus (corner - centre) x 0.5.
    const std::vector<Vec3> expectedA = {
        {8.25f, 4.25f, 8.25f}, {8.75f, 4.25f, 8.75f},
        {8.75f, 4.75f, 8.75f}, {8.25f, 4.75f, 8.25f},
    };
    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(data.transparentVertices[i].position, expectedA[i]);
    }
    // Stage 7 (the final): scale 1 — the full-cell diagonals again.
    world.setBlockState(8, 4, 8, 7);
    const auto full =
        ChunkMesher(world, reg, makeLayout({"t", "f", "sap"})).mesh({0, 0, 0});
    const std::vector<Vec3> expectedFull = {
        {8, 4, 8},  {9, 4, 9},  {9, 5, 9},  {8, 5, 8},
        {9, 4, 8},  {8, 4, 9},  {8, 5, 9},  {9, 5, 8},
    };
    ASSERT_EQ(full.transparentVertices.size(), expectedFull.size());
    for (std::size_t i = 0; i < expectedFull.size(); ++i) {
        EXPECT_EQ(full.transparentVertices[i].position, expectedFull[i]);
    }
}

TEST(ChunkMesherTest, StatelessCrossRegressionScaleOne) {
    // A cross WITHOUT a state pair (the wheat / torch / flower pattern)
    // keeps the EXACT pre-GEN-2 positions: the cell diagonals, full cell
    // — the structural-immunity contract (a fresh world is stateless, so
    // the gate never fires). The sentinel state read must not perturb it.
    BlockRegistry reg = makeCrossRegistry();
    World world;
    world.setBlock(8, 4, 8, reg.idForName("flower"));
    // Even a stray state byte on the cell (impossible in a fresh world —
    // the choke point never writes one to a stateless block) must not
    // change the mesh: the gate is the block's stateCount, not the byte.
    world.setBlockState(8, 4, 8, 3);
    const auto data =
        ChunkMesher(world, reg, makeLayout({"t", "f"})).mesh({0, 0, 0});
    const std::vector<Vec3> expected = {
        {8, 4, 8},  {9, 4, 9},  {9, 5, 9},  {8, 5, 8},
        {9, 4, 8},  {8, 4, 9},  {8, 5, 9},  {9, 5, 8},
    };
    ASSERT_EQ(data.transparentVertices.size(), expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        EXPECT_EQ(data.transparentVertices[i].position, expected[i]);
    }
}

// ---------------------------------------------------------------------------
// GEN-9: the moisture top-face variant (model.moisture_top) — the farmland
// wet/dry visual. The +Y face is meshed with the WET tile while the cell's
// soil is wet (water within Chebyshev radius 4 at the same y, the
// soilIsWet predicate) and with the dry `textures.top` otherwise. Wet and
// dry neighbors never greedy-merge (the tile differs).
// ---------------------------------------------------------------------------

namespace {

BlockRegistry moistureRegistry() {
    BlockRegistry reg;
    auto farmland = reg.registerBlock(parseDef(
        R"({"id":"farmland","model":{"textures":{"top":"dry_t","side":"t",)"
        R"("bottom":"t"},"moisture_top":"wet_t"}})"));
    if (!farmland.ok()) {
        ADD_FAILURE() << farmland.error().message;
    }
    return reg;
}

// Every top-face vertex must sit inside the tile's UV rectangle.
bool topVerticesInside(const ChunkMeshData& data, const AtlasLayout& layout,
                       const std::string& tile) {
    const Vec2 u0 = AtlasLayout::uvMin(layout, tile);
    const Vec2 u1 = AtlasLayout::uvMax(layout, tile);
    const auto top = verticesWithNormal(data.opaqueVertices,
                                        Vec3 {0.0f, 1.0f, 0.0f});
    if (top.empty()) {
        return false;
    }
    for (const MeshVertex& v : top) {
        if (v.uv.x < u0.x - 1e-6 || v.uv.x > u1.x + 1e-6 ||
            v.uv.y < u0.y - 1e-6 || v.uv.y > u1.y + 1e-6) {
            return false;
        }
    }
    return true;
}

} // namespace

TEST(ChunkMesherTest, MoistureTopUsesWetTileWhenWet) {
    const BlockRegistry reg = moistureRegistry();
    const AtlasLayout layout = makeLayout({"dry_t", "t", "wet_t"});
    World world;
    world.setBlock(8, 1, 8, reg.idForName("farmland"));
    world.setFluid(11, 1, 8, FluidKind::Water, 8); // 3 away, the soil's y
    const auto data = ChunkMesher(world, reg, layout).mesh({0, 0, 0});
    EXPECT_TRUE(topVerticesInside(data, layout, "wet_t"));
    EXPECT_FALSE(topVerticesInside(data, layout, "dry_t"));
}

TEST(ChunkMesherTest, MoistureTopUsesDryTileWhenDry) {
    const BlockRegistry reg = moistureRegistry();
    const AtlasLayout layout = makeLayout({"dry_t", "t", "wet_t"});
    World world;
    world.setBlock(8, 1, 8, reg.idForName("farmland"));
    const auto data = ChunkMesher(world, reg, layout).mesh({0, 0, 0});
    EXPECT_TRUE(topVerticesInside(data, layout, "dry_t"));
    EXPECT_FALSE(topVerticesInside(data, layout, "wet_t"));
}

TEST(ChunkMesherTest, MoistureWetDryNeighborsDoNotMerge) {
    // Two adjacent farmland tops: the west one is dry (water 5 from it),
    // the east one is wet (water 4 from it) — the top faces carry
    // DIFFERENT tiles, so the greedy merge keeps them as two quads
    // (8 top vertices instead of the 4 a merged run would emit).
    const BlockRegistry reg = moistureRegistry();
    const AtlasLayout layout = makeLayout({"dry_t", "t", "wet_t"});
    World world;
    world.setBlock(7, 1, 8, reg.idForName("farmland"));
    world.setBlock(8, 1, 8, reg.idForName("farmland"));
    world.setFluid(12, 1, 8, FluidKind::Water, 8); // 4 from (8,1,8), 5 from (7,1,8)
    const auto data = ChunkMesher(world, reg, layout).mesh({0, 0, 0});
    const auto top = verticesWithNormal(data.opaqueVertices,
                                        Vec3 {0.0f, 1.0f, 0.0f});
    EXPECT_EQ(top.size(), 8u);
    // Each quad is homogeneous: the wet tile's vertices and the dry
    // tile's vertices are 4 each (no mixed run).
    int wet = 0, dry = 0;
    const Vec2 w0 = AtlasLayout::uvMin(layout, "wet_t");
    const Vec2 d0 = AtlasLayout::uvMin(layout, "dry_t");
    for (const MeshVertex& v : top) {
        if (v.uv.y >= w0.y - 1e-6) {
            ++wet;
        } else if (v.uv.y <= d0.y + 0.3333f) {
            ++dry;
        }
    }
    EXPECT_EQ(wet, 4);
    EXPECT_EQ(dry, 4);
}
