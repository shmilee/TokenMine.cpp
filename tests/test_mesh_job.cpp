// Mesh job (PIPE-1) tests: the worker-meshing building blocks that the
// Game wires into the pool — the throwaway snapshot World
// (insertSnapshotChunk: snapshot mesh == live mesh, void semantics) and
// the pure column selection (nextMeshColumns: nearest first, unlit /
// light-footprint / in-flight exclusions, cap).
//
// Not unit-tested here (Game-level, E2E base-gate covered): the
// integrateMeshJobs bookkeeping's GL half (m_columnRebuildPending), the
// unloadFarColumns in-flight protection (m_meshPending ->
// protectedColumns — a 2-line set insert, structurally the
// m_streamPending paradigm), and the frame pipeline order
// (gen -> mesh -> light posts; integrateMeshJobs before
// integrateStreamedColumns).
#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include "block/block_registry.hpp"
#include "renderer/texture_atlas.hpp"
#include "world/chunk.hpp"
#include "world/chunk_mesher.hpp"
#include "world/mesh_job.hpp"
#include "world/world.hpp"

using namespace mc;

namespace {

// The snapshot insertion (PIPE-1) copies whole Chunks into the throwaway
// World — pin the copy contract: Chunk must stay trivially copyable.
static_assert(std::is_trivially_copyable_v<Chunk>);
// The vertex comparison below is a byte compare — pin the contract.
static_assert(std::is_trivially_copyable_v<MeshVertex>);

// The column's chunk layers (the game.cpp constant is file-local; the
// same expression — a column spans the whole world height).
constexpr int kVCols = kWorldHeight / kChunkSize;

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

// stone/glass opaque-transparent pair, both fluids, and a stair (the
// per-instance facing fixture).
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
    auto lava = reg.registerBlock(parseDef(
        cubeDef("lava",
                R"({"opaque":false,"transparent":true,"solid":false,"fluid":"lava"})",
                R"({"top":"t","side":"t","bottom":"t"})")));
    // Phase 87: stairs (cube textures, like the wall shape).
    auto stairs = reg.registerBlock(parseDef(
        R"({"id":"stairs","model":{"shape":"stairs","textures":{"top":"t","side":"t","bottom":"t"}}})"));
    // GEN-2: a stateful cross (the sapling: 8 stages, the stage visual
    // scales the quads — the snapshot must carry the state byte).
    auto sapling = reg.registerBlock(parseDef(
        R"({"id":"sapling","properties":{"solid":false,"opaque":false,"transparent":true},)"
        R"("model":{"shape":"cross","state_count":8,"state_default":0,"textures":{"cross":"t"}}})"));
    if (!stone.ok() || !glass.ok() || !water.ok() || !lava.ok() ||
        !stairs.ok() || !sapling.ok()) {
        ADD_FAILURE() << "test registry failed to build";
    }
    return reg;
}

// 1 tile atlas: every UV is within [0,1].
AtlasLayout makeLayout() {
    auto r = AtlasLayout::parse(parseDef(
        "{\"columns\":1,\"rows\":1,\"tiles\":{\"t\":[0,0]}}"));
    if (!r.ok()) {
        ADD_FAILURE() << r.error().message;
    }
    return *r;
}

const AtlasLayout& kLayout() {
    static const AtlasLayout layout = makeLayout();
    return layout;
}

// Byte-compares two vertex streams (MeshVertex is trivially copyable).
void expectVerticesEqual(const std::vector<MeshVertex>& a,
                         const std::vector<MeshVertex>& b) {
    EXPECT_EQ(a.size(), b.size());
    if (a.size() != b.size()) {
        return;
    }
    EXPECT_EQ(0, std::memcmp(a.data(), b.data(),
                             a.size() * sizeof(MeshVertex)));
}

} // namespace

// Group 1: the snapshot World (the job's private 3x3 column
// neighborhood) meshes bit-identically to the live World — across chunk
// and column boundaries, for both fluids, the transparent pass, the
// per-instance stair facing, and the baked light fields.
TEST(MeshJobTest, SnapshotMeshMatchesLiveWorldMesh) {
    BlockRegistry reg = makeRegistry();
    World live;
    // The full 3x3 column neighborhood around (0,0), all 8 layers — the
    // job's snapshot shape.
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dz = -1; dz <= 1; ++dz) {
            for (int cy = 0; cy < kVCols; ++cy) {
                live.getOrCreateChunk({dx, cy, dz});
            }
        }
    }
    const BlockID stone = reg.idForName("stone");
    const BlockID glass = reg.idForName("glass");
    const BlockID stair = reg.idForName("stairs");
    // Content: boundary-crossing cubes, the transparent pass, a stair
    // with a per-instance state (facing 3 = west, the block default is 0).
    live.setBlock(15, 64, 15, stone); // a corner cell: touches 3 columns
    live.setBlock(16, 64, 0, stone);  // into column (1, 0)
    live.setBlock(8, 100, 8, glass);  // transparent pass, layer 6
    live.setBlock(0, 112, 0, stair);  // column (-1, 0), layer 7
    live.setBlockState(0, 112, 0, 3);
    // Fluids of both kinds (same-KIND culling reads the neighbor cell).
    live.setFluid(10, 60, 10, FluidKind::Water, 8);
    live.setFluid(20, 60, 10, FluidKind::Lava, 4);
    // Light gradients in the center column (own-cell light + the
    // neighbor-cell light a face bakes).
    for (int cy = 0; cy < kVCols; ++cy) {
        Chunk* c = live.findChunkMut({0, cy, 0});
        c->setSkyLightAt(8, 0, 8, static_cast<std::uint8_t>(cy));
        c->setBlockLightAt(8, 0, 8,
                           static_cast<std::uint8_t>(kVCols - 1 - cy));
    }
    // The snapshot (the job's private World): the same 3x3x8 through the
    // PIPE-1 insertion (no live-World bookkeeping).
    World snap;
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dz = -1; dz <= 1; ++dz) {
            for (int cy = 0; cy < kVCols; ++cy) {
                const ChunkPos cp{dx, cy, dz};
                const Chunk* c = live.findChunk(cp);
                if (c != nullptr) {
                    snap.insertSnapshotChunk(cp, *c);
                }
            }
        }
    }
    // All 8 center-column chunks: bit-identical meshes (both passes).
    for (int cy = 0; cy < kVCols; ++cy) {
        const ChunkPos cp{0, cy, 0};
        const ChunkMeshData a =
            ChunkMesher(live, reg, kLayout()).mesh(cp);
        const ChunkMeshData b =
            ChunkMesher(snap, reg, kLayout()).mesh(cp);
        expectVerticesEqual(a.opaqueVertices, b.opaqueVertices);
        EXPECT_EQ(a.opaqueIndices, b.opaqueIndices);
        expectVerticesEqual(a.transparentVertices, b.transparentVertices);
        EXPECT_EQ(a.transparentIndices, b.transparentIndices);
    }
    // A column OUTSIDE the 3x3 neighborhood (3, 0) must not change the
    // center column's mesh — the snapshot's read set is sufficient.
    live.setBlock(48, 64, 8, stone);
    {
        const ChunkMeshData a =
            ChunkMesher(live, reg, kLayout()).mesh({0, 4, 0});
        const ChunkMeshData b =
            ChunkMesher(snap, reg, kLayout()).mesh({0, 4, 0});
        expectVerticesEqual(a.opaqueVertices, b.opaqueVertices);
        EXPECT_EQ(a.opaqueIndices, b.opaqueIndices);
        expectVerticesEqual(a.transparentVertices, b.transparentVertices);
        EXPECT_EQ(a.transparentIndices, b.transparentIndices);
    }
}

// Group 1b (GEN-2): the snapshot carries the per-instance STATE byte —
// a staged sapling (the stateful cross) meshes bit-identically from the
// snapshot and the live world (the worker mesher reads the stage through
// the same World API; a snapshot that dropped the state array would mesh
// the full-scale quad instead of the stage-scaled one).
TEST(MeshJobTest, SnapshotCarriesSaplingStage) {
    BlockRegistry reg = makeRegistry();
    World live;
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dz = -1; dz <= 1; ++dz) {
            for (int cy = 0; cy < kVCols; ++cy) {
                live.getOrCreateChunk({dx, cy, dz});
            }
        }
    }
    const BlockID sapling = reg.idForName("sapling");
    // Stage 3 of 8 (scale 0.5) in the center column.
    live.setBlock(8, 64, 8, sapling);
    live.setBlockState(8, 64, 8, 3);
    World snap;
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dz = -1; dz <= 1; ++dz) {
            for (int cy = 0; cy < kVCols; ++cy) {
                const ChunkPos cp{dx, cy, dz};
                const Chunk* c = live.findChunk(cp);
                if (c != nullptr) {
                    snap.insertSnapshotChunk(cp, *c);
                }
            }
        }
    }
    const ChunkPos cp{0, 4, 0}; // layer 4 holds y = 64
    const ChunkMeshData a = ChunkMesher(live, reg, kLayout()).mesh(cp);
    const ChunkMeshData b = ChunkMesher(snap, reg, kLayout()).mesh(cp);
    expectVerticesEqual(a.transparentVertices, b.transparentVertices);
    EXPECT_EQ(a.transparentIndices, b.transparentIndices);
    // And the stage actually scaled the quad (not the full cell): the
    // center cell's quad corner sits at chunk-local (8.25, 0.25, 8.25) —
    // the 0.5 scale around the cell centre (local (0.5, 0.5, 0.5)).
    bool sawScaledCorner = false;
    for (const MeshVertex& v : a.transparentVertices) {
        if (v.position ==
            Vec3 {8.25f, 0.25f, 8.25f}) {
            sawScaledCorner = true;
        }
    }
    EXPECT_TRUE(sawScaledCorner);
}

// Group 2: the snapshot World's void semantics — the six read faces
// (block / fluid both kinds / sky light / block light / block state)
// match an empty live World at out-of-range, below-world and
// above-world positions (the same World class, same void).
TEST(MeshJobTest, SnapshotVoidMatchesEmptyWorld) {
    BlockRegistry reg = makeRegistry();
    World emptyLive; // the reference void
    World snap;      // NOT empty (a probe must be outside its content)
    snap.getOrCreateChunk({0, 0, 0});
    snap.setBlock(8, 64, 8, reg.idForName("stone"));
    const std::vector<std::array<int, 3>> probes = {
        {{1000, 64, 0}},    // out-of-range column (x)
        {{-1000, 0, 0}},    // out-of-range column (negative x)
        {{8, -1, 8}},       // below the world
        {{8, kWorldHeight, 8}},           // at the top edge (y = 128)
        {{8, kWorldHeight + 64, 8}},     // above the world
        {{0, 0, 1000}},                  // out-of-range column (z)
    };
    for (const auto& p : probes) {
        const int x = p[0], y = p[1], z = p[2];
        EXPECT_EQ(snap.getBlock(x, y, z), emptyLive.getBlock(x, y, z));
        EXPECT_EQ(snap.getFluid(x, y, z), emptyLive.getFluid(x, y, z));
        EXPECT_EQ(snap.getFluidOfKind(x, y, z, FluidKind::Water),
                  emptyLive.getFluidOfKind(x, y, z, FluidKind::Water));
        EXPECT_EQ(snap.getFluidOfKind(x, y, z, FluidKind::Lava),
                  emptyLive.getFluidOfKind(x, y, z, FluidKind::Lava));
        EXPECT_EQ(snap.getSkyLight(x, y, z), emptyLive.getSkyLight(x, y, z));
        EXPECT_EQ(snap.getBlockLight(x, y, z),
                  emptyLive.getBlockLight(x, y, z));
        EXPECT_EQ(snap.getBlockState(x, y, z),
                  emptyLive.getBlockState(x, y, z));
    }
    // The pinned void values themselves (implementation contract):
    EXPECT_EQ(emptyLive.getBlock(1000, 64, 0), BlockID::air());
    EXPECT_EQ(emptyLive.getSkyLight(8, kWorldHeight, 8), 15); // open sky
    EXPECT_EQ(emptyLive.getBlockLight(8, kWorldHeight, 8), 0);
    EXPECT_EQ(emptyLive.getSkyLight(8, -1, 8), 0); // below the world
    EXPECT_EQ(emptyLive.getBlockLight(8, -1, 8), 0);
    EXPECT_EQ(emptyLive.getBlockState(1000, 64, 0),
              Chunk::kBlockStateUnset); // uncreated reads "unset"
    // A CREATED but unset stateful cell reads "unset" (255) — the chunk
    // fills it with kBlockStateUnset, so created-unset and void are the
    // same read (the mesher falls back to the block default either way):
    EXPECT_EQ(snap.getBlockState(0, 0, 0), Chunk::kBlockStateUnset);
    World emptyProbe;
    emptyProbe.getOrCreateChunk({0, 0, 0});
    EXPECT_EQ(emptyProbe.getBlockState(0, 0, 0), Chunk::kBlockStateUnset);
}

// Group 3: the selection rules (world/mesh_job.hpp) — nearest first
// (columnDistanceSq, ties by (x, z)), the unlit / light-footprint /
// in-flight exclusions, the cap.
TEST(MeshJobTest, NextMeshColumnsNearestFirst) {
    const ColumnPos player{0, 0};
    const std::set<ColumnPos> dirty = {{2, 0}, {0, 2}, {1, 0}, {0, 1},
                                       {2, 1}, {3, 0}};
    const std::vector<ColumnPos> all =
        nextMeshColumns(dirty, player, {}, {}, {}, 10);
    // distanceSq: (0,1)/(1,0) 1; (0,2)/(2,0) 4; (2,1) 5; (3,0) 9.
    const std::vector<ColumnPos> expect = {{0, 1}, {1, 0}, {0, 2}, {2, 0},
                                           {2, 1}, {3, 0}};
    EXPECT_EQ(all, expect);
    // The cap takes the nearest.
    EXPECT_EQ(nextMeshColumns(dirty, player, {}, {}, {}, 2),
              (std::vector<ColumnPos>{{0, 1}, {1, 0}}));
    EXPECT_TRUE(nextMeshColumns(dirty, player, {}, {}, {}, 0).empty());
    EXPECT_TRUE(nextMeshColumns({}, player, {}, {}, {}, 10).empty());
}

TEST(MeshJobTest, NextMeshColumnsExclusions) {
    const ColumnPos player{0, 0};
    const std::set<ColumnPos> dirty = {{0, 1}, {1, 0}, {0, 2}, {1, 3}};
    // An UNLIT column (its own light job not done) never posts.
    const std::set<ColumnPos> unlit = {{0, 1}};
    EXPECT_EQ(nextMeshColumns(dirty, player, unlit, {}, {}, 10),
              (std::vector<ColumnPos>{{1, 0}, {0, 2}, {1, 3}}));
    // A column within the lighting job's footprint (Chebyshev 1 of a
    // light-pending column) never posts — its snapshot would bake stale
    // light; the light job re-marks it dirty, so it re-queues later.
    // (0, 2) and (1, 3) are both Chebyshev 1 of (0, 3): excluded.
    const std::set<ColumnPos> lightPending = {{0, 3}};
    EXPECT_EQ(nextMeshColumns(dirty, player, {}, lightPending, {}, 10),
              (std::vector<ColumnPos>{{0, 1}, {1, 0}}));
    // ...while Chebyshev 2 clears the footprint.
    EXPECT_EQ(nextMeshColumns({{2, 1}}, player, {}, lightPending, {}, 10),
              (std::vector<ColumnPos>{{2, 1}}));
    // An IN-FLIGHT column never posts twice.
    const std::set<ColumnPos> inFlight = {{1, 0}};
    EXPECT_EQ(nextMeshColumns(dirty, player, {}, {}, inFlight, 10),
              (std::vector<ColumnPos>{{0, 1}, {0, 2}, {1, 3}}));
}

// Group 4: the integration bookkeeping's World half — the 8-chunk
// clear-dirty empties the column from the worklist, and a post-integrate
// edit re-marks it so the selection re-queues the column (the GL half,
// m_columnRebuildPending, is Game-level: E2E base-gate covered).
TEST(MeshJobTest, IntegrationClearsDirtyAndEditRequeues) {
    World live;
    for (int cy = 0; cy < kVCols; ++cy) {
        live.getOrCreateChunk({0, cy, 0}); // fresh chunks are dirty
    }
    BlockRegistry reg = makeRegistry();
    live.setBlock(8, 64, 8, reg.idForName("stone"));
    // The column projection of the dirty set: the whole column is dirty.
    std::set<ColumnPos> dirtyCols;
    for (const ChunkPos& c : live.dirtyChunks()) {
        dirtyCols.insert({c.x, c.z});
    }
    EXPECT_EQ(dirtyCols, (std::set<ColumnPos>{{0, 0}}));
    // Integrate (the job done): clear the 8 chunks' dirty flags.
    for (int cy = 0; cy < kVCols; ++cy) {
        live.clearChunkDirty({0, cy, 0});
    }
    EXPECT_TRUE(live.dirtyChunks().empty());
    // The worklist the job was queued from (dirtyCols, still holding the
    // column) must NOT re-queue while the job is in flight:
    EXPECT_TRUE(nextMeshColumns(dirtyCols, {0, 0}, {}, {},
                                {{0, 0}}, 2)
                    .empty()); // in-flight -> not re-queued
    // A post-integrate edit re-marks the column dirty...
    live.setBlock(9, 64, 8, reg.idForName("stone"));
    dirtyCols.clear();
    for (const ChunkPos& c : live.dirtyChunks()) {
        dirtyCols.insert({c.x, c.z});
    }
    // ...and once the job is no longer in flight, it re-queues.
    EXPECT_EQ(nextMeshColumns(dirtyCols, {0, 0}, {}, {}, {}, 2),
              (std::vector<ColumnPos>{{0, 0}}));
}

// Group 5 (E2E-covered, documented): unloadFarColumns protects in-flight
// MESH columns (m_meshPending -> protectedColumns, the m_streamPending
// paradigm). Game-level — the base18 A/B run exercises it (no in-flight
// column may vanish mid-fill).
//
// Group 6: the trivially-copyable pins are the static_asserts at the top
// of this file (Chunk for the snapshot copy, MeshVertex for the byte
// compare).
