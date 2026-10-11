// ChunkMesher tests: face culling (within and across chunks), pass
// assignment (opaque vs transparent), geometry conventions (local
// positions, outward normals) and per-face UV mapping.
#include <gtest/gtest.h>

#include <cmath>
#include <string>
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
                R"({"opaque":false,"transparent":true,"solid":false,"fluid":true})",
                R"({"top":"t","side":"t","bottom":"t"})")));
    if (!stone.ok() || !glass.ok() || !water.ok()) {
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
    world.setFluid(8, 5, 8, kMaxFluidLevel); // water on top of stone
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
    world.setFluid(8, 4, 8, kMaxFluidLevel);
    world.setFluid(9, 4, 8, kMaxFluidLevel);
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
    world.setFluid(8, 4, 8, 4);
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
    world.setFluid(8, 4, 8, kMaxFluidLevel);
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
    world.setFluid(8, 4, 8, kMaxFluidLevel);
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
    world.setFluid(8, 4, 8, 4); // partial height: sides are 0.5 tall
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
    world.setFluid(8, 4, 8, kMaxFluidLevel); // water directly next to the stone
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
