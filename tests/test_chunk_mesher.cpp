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

AtlasLayout makeLayout(const std::vector<std::string>& names) {
    auto r = AtlasLayout::compute(names);
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
    // 6 + 6 faces, minus the 1 shared face from each side: 10 faces.
    const auto data = mesh(world, reg, {0, 0, 0});
    EXPECT_EQ(data.opaqueVertices.size(), 40u);
    EXPECT_EQ(data.opaqueIndices.size(), 60u);
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
