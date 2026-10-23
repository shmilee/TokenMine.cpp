// Lighting job planning (99.10) tests: the pure partition logic
// (lightRegionsIndependent / columnLightBox / planLightJobs — the
// parallel-safe criterion is the WRITE footprint of the column box ±
// kLightUpdateRadius PLUS the block-light BFS's one-chunk boundary-read
// ring, so the parallel gap is kLightParallelColumnGap = 4, not the
// LGT-1 write-only gap 3) and the bit-level equivalence of the two-job
// parallel path versus the forced one-job sequential path (the test
// seam: updateRegion's dirtyOut collects the workers' net-changed
// chunks job-locally, so a test runs "the jobs" on one thread in any
// order and compares light + dirty sets).
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "block/block_registry.hpp"
#include "core/json.hpp"
#include "world/light_job.hpp"
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

// stone=opaque; glowstone=emission 15; lantern=emission 5.
BlockRegistry makeRegistry() {
    BlockRegistry reg;
    auto r1 = reg.registerBlock(
        parseDef(cubeDef("stone", "{}",
                         R"({"top":"t","side":"t","bottom":"t"})")));
    auto r2 = reg.registerBlock(
        parseDef(cubeDef("glowstone", R"({"light_emission":15})",
                         R"({"top":"t","side":"t","bottom":"t"})")));
    auto r3 = reg.registerBlock(
        parseDef(cubeDef("lantern", R"({"light_emission":5})",
                         R"({"top":"t","side":"t","bottom":"t"})")));
    if (!r1.ok() || !r2.ok() || !r3.ok()) {
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
    EXPECT_TRUE(diffs.empty())
        << "light differs in " << diffs.size() << " cells";
}

// Clears the whole dirty set (propagateAll marks every chunk).
void clearAllDirty(World& w) {
    const std::vector<ChunkPos> dirty(w.dirtyChunks().begin(),
                                      w.dirtyChunks().end());
    for (const ChunkPos& p : dirty) {
        w.clearChunkDirty(p);
    }
}

// The full-column box of a pending column, exactly as the game's light
// job computes it (columnLightBox — the test's paths drive updateRegion
// through the SAME pure function the Game uses).
void runJobs(World& w, const BlockRegistry& reg,
             const std::vector<std::vector<ColumnPos>>& jobs,
             std::set<ChunkPos>* dirty) {
    for (const std::vector<ColumnPos>& job : jobs) {
        for (const ColumnPos& col : job) {
            const std::array<BlockPos, 2> box = columnLightBox(col);
            Lighting::updateRegion(w, reg, box[0], box[1],
                                   /*newTerrain=*/true, dirty);
        }
    }
}

} // namespace

// -- the criterion constants ---------------------------------------------------

TEST(LightJobTest, GapConstantsAreDerived) {
    // kLightUpdateRadius (16) is exactly one column, so the write reach
    // is 1 column out, and the parallel gap is write reach + the
    // one-chunk boundary-read ring, doubled: 4.
    EXPECT_EQ(kLightRegionReach, 1);
    EXPECT_EQ(kLightParallelColumnGap, 4);
    EXPECT_EQ(kLightParallelColumnGap, 2 * (kLightRegionReach + 1));
}

// -- lightRegionsIndependent: the endpoint table -------------------------------

TEST(LightJobTest, RegionsIndependentEndpoints) {
    const ColumnPos origin{0, 0};
    // Gap >= 4 on EITHER axis -> independent (one separated axis leaves
    // no shared chunk to write or read).
    EXPECT_TRUE(lightRegionsIndependent({4, 0}, origin));
    EXPECT_TRUE(lightRegionsIndependent({-4, 0}, origin));
    EXPECT_TRUE(lightRegionsIndependent({0, 4}, origin));
    EXPECT_TRUE(lightRegionsIndependent({0, -4}, origin));
    EXPECT_TRUE(lightRegionsIndependent({3, 4}, origin)); // z saves it
    EXPECT_TRUE(lightRegionsIndependent({-4, -3}, origin));
    EXPECT_TRUE(lightRegionsIndependent({0, 60}, origin));  // LGT-1 dominant
    EXPECT_TRUE(lightRegionsIndependent({12, -15}, origin)); // LGT-1 dominant
    EXPECT_TRUE(lightRegionsIndependent({64, -60}, origin)); // vd80-scale
    // Gap 3 on both axes: write-disjoint (the LGT-1 footprint criterion)
    // but the block-light boundary-read ring still crosses -> dependent.
    EXPECT_FALSE(lightRegionsIndependent({3, 0}, origin));
    EXPECT_FALSE(lightRegionsIndependent({0, 3}, origin));
    EXPECT_FALSE(lightRegionsIndependent({3, 3}, origin));
    EXPECT_FALSE(lightRegionsIndependent({-3, -3}, origin));
    // Gap <= 2: the WRITE footprints themselves intersect -> dependent.
    EXPECT_FALSE(lightRegionsIndependent({2, 0}, origin));
    EXPECT_FALSE(lightRegionsIndependent({0, 2}, origin));
    EXPECT_FALSE(lightRegionsIndependent({2, 2}, origin));
    EXPECT_FALSE(lightRegionsIndependent({1, 0}, origin));
    EXPECT_FALSE(lightRegionsIndependent({0, 1}, origin));
    // Symmetry, and a column is never independent of itself.
    EXPECT_EQ(lightRegionsIndependent({5, -4}, origin),
              lightRegionsIndependent(origin, {5, -4}));
    EXPECT_EQ(lightRegionsIndependent({-3, 4}, origin),
              lightRegionsIndependent(origin, {-3, 4}));
    EXPECT_FALSE(lightRegionsIndependent(origin, origin));
}

// -- columnLightBox -------------------------------------------------------------

TEST(LightJobTest, ColumnLightBoxIsTheFullColumn) {
    const std::array<BlockPos, 2> box = columnLightBox({-2, 3});
    EXPECT_EQ(box[0], (BlockPos{-32, 0, 48}));
    EXPECT_EQ(box[1], (BlockPos{-17, kWorldHeight - 1, 63}));
    const std::array<BlockPos, 2> zero = columnLightBox({0, 0});
    EXPECT_EQ(zero[0], (BlockPos{0, 0, 0}));
    EXPECT_EQ(zero[1], (BlockPos{15, kWorldHeight - 1, 15}));
}

// -- planLightJobs: the partition ------------------------------------------------

TEST(LightJobTest, PlanEmptyAndSingleColumn) {
    EXPECT_TRUE(planLightJobs({}).jobs.empty());
    const LightJobPlan single = planLightJobs({{-1, 2}});
    ASSERT_EQ(single.jobs.size(), 1u);
    EXPECT_EQ(single.jobs[0], (std::vector<ColumnPos>{{-1, 2}}));
}

TEST(LightJobTest, PlanDependentPairStaysOneSequentialJob) {
    // gap 2 (footprints intersect), gap 3 (write-disjoint, read ring
    // crosses), and the LGT-1 merge-eligible family (|Δ| ≤ 2 both axes):
    // ONE job, the boxes in set order = the pre-99.10 behavior.
    const struct {
        std::set<ColumnPos> pending;
        std::vector<ColumnPos> expect;
    } cases[] = {
        {{{0, 0}, {2, 0}}, {{0, 0}, {2, 0}}},
        {{{0, 0}, {3, 0}}, {{0, 0}, {3, 0}}},
        {{{0, 0}, {3, 3}}, {{0, 0}, {3, 3}}},
        {{{0, 0}, {0, -2}}, {{0, -2}, {0, 0}}}, // set order, not input
        {{{0, 0}, {2, 2}}, {{0, 0}, {2, 2}}},   // LGT-1: the 0.1% family
        {{{0, 0}, {-3, 0}}, {{-3, 0}, {0, 0}}},
    };
    for (const auto& c : cases) {
        const LightJobPlan plan = planLightJobs(c.pending);
        ASSERT_EQ(plan.jobs.size(), 1u)
            << "pair (" << c.pending.begin()->x << ", "
            << c.pending.begin()->z << ") / (" << c.pending.rbegin()->x
            << ", " << c.pending.rbegin()->z << ")";
        EXPECT_EQ(plan.jobs[0], c.expect)
            << "pair (" << c.pending.begin()->x << ", "
            << c.pending.begin()->z << ") / (" << c.pending.rbegin()->x
            << ", " << c.pending.rbegin()->z << ")";
    }
}

TEST(LightJobTest, PlanIndependentPairSplitsParallel) {
    const LightJobPlan plan = planLightJobs({{0, 0}, {4, 0}});
    ASSERT_EQ(plan.jobs.size(), 2u);
    EXPECT_EQ(plan.jobs[0], (std::vector<ColumnPos>{{0, 0}}));
    EXPECT_EQ(plan.jobs[1], (std::vector<ColumnPos>{{4, 0}}));
    // The LGT-1 dominant streaming pair (|Δz| = 60) splits too.
    const LightJobPlan plan2 = planLightJobs({{-1, -60}, {-1, 0}});
    ASSERT_EQ(plan2.jobs.size(), 2u);
    EXPECT_EQ(plan2.jobs[0], (std::vector<ColumnPos>{{-1, -60}}));
    EXPECT_EQ(plan2.jobs[1], (std::vector<ColumnPos>{{-1, 0}}));
    // Negative coordinates.
    const LightJobPlan plan3 = planLightJobs({{-8, 0}, {-4, 0}});
    ASSERT_EQ(plan3.jobs.size(), 2u);
    EXPECT_EQ(plan3.jobs[0], (std::vector<ColumnPos>{{-8, 0}}));
    EXPECT_EQ(plan3.jobs[1], (std::vector<ColumnPos>{{-4, 0}}));
}

TEST(LightJobTest, PlanThreeOrMoreColumnsStayOneJob) {
    // 3+ pending (multi-column first-touch frames): the conservative
    // sequential fallback — never more than two jobs per frame (hard
    // rule 7, revised).
    const LightJobPlan plan =
        planLightJobs({{0, 0}, {10, 0}, {20, 0}});
    ASSERT_EQ(plan.jobs.size(), 1u);
    EXPECT_EQ(plan.jobs[0],
              (std::vector<ColumnPos>{{0, 0}, {10, 0}, {20, 0}}));
    const LightJobPlan plan2 = planLightJobs({{0, 0}, {1, 0}, {50, 0}});
    ASSERT_EQ(plan2.jobs.size(), 1u);
    EXPECT_EQ(plan2.jobs[0],
              (std::vector<ColumnPos>{{0, 0}, {1, 0}, {50, 0}}));
}

// -- the dirtyOut seam -----------------------------------------------------------

TEST(LightJobTest, DirtyOutMatchesWorldMarking) {
    // The worker path (dirtyOut) must catch exactly the net-changed
    // chunks the default path marks into the World — light bit-identical,
    // and the worker path leaves the shared set untouched.
    const BlockRegistry reg = makeRegistry();
    const BlockID stone = idOf(reg, "stone");
    const BlockID glow = idOf(reg, "glowstone");
    const auto buildBase = [&](World& w) {
        for (int cx = 0; cx <= 1; ++cx) {
            for (int cz = 0; cz <= 1; ++cz) {
                w.getOrCreateChunk({cx, 0, cz});
            }
        }
        fillBox(w, 0, 0, 0, 31, 0, 31, stone); // ground
    };
    // The incoming column (2, 0): ground + a glowstone on the WEST face
    // (local x = 0) so its light reaches the diagonal pre-existing chunk
    // (1, 0, 1) — a chunk the insertion does NOT mark.
    const auto makeColumn = [&]() {
        Chunk c;
        for (int x = 0; x < kChunkSize; ++x) {
            for (int z = 0; z < kChunkSize; ++z) {
                c.set(x, 0, z, stone);
            }
        }
        c.set(0, 1, 8, glow);
        return c;
    };
    World workerPath, defaultPath;
    buildBase(workerPath);
    buildBase(defaultPath);
    Lighting::propagateAll(workerPath, reg);
    Lighting::propagateAll(defaultPath, reg);
    clearAllDirty(workerPath);
    clearAllDirty(defaultPath);
    ASSERT_TRUE(workerPath.insertChunk({2, 0, 0}, makeColumn()));
    ASSERT_TRUE(defaultPath.insertChunk({2, 0, 0}, makeColumn()));
    // The identical insertions mark the identical set.
    ASSERT_EQ(workerPath.dirtyChunks(), defaultPath.dirtyChunks());
    const std::set<ChunkPos> insertMarks = workerPath.dirtyChunks();

    std::set<ChunkPos> dirty;
    runJobs(workerPath, reg, {{std::vector<ColumnPos>{{2, 0}}}}, &dirty);
    runJobs(defaultPath, reg, {{std::vector<ColumnPos>{{2, 0}}}}, nullptr);
    expectSameLight(workerPath, defaultPath);
    // The worker path never touched the shared set...
    EXPECT_EQ(workerPath.dirtyChunks(), insertMarks);
    // ...and the collected set is exactly the net-changed chunks (all of
    // them, including ones the insertion already marked). The default
    // path's final set is the union.
    std::set<ChunkPos> unionMarks;
    for (const ChunkPos& p : insertMarks) {
        unionMarks.insert(p);
    }
    for (const ChunkPos& p : dirty) {
        unionMarks.insert(p);
    }
    EXPECT_EQ(defaultPath.dirtyChunks(), unionMarks);
    // The net change is non-trivial: the diagonal chunk (1,0,1) — outside
    // the insertion's face-neighbor marks, reached only through the
    // boundary source — landed in the collected set, alongside the new
    // chunk and its face neighbor.
    EXPECT_EQ(dirty.count({1, 0, 1}), 1u);
    EXPECT_EQ(dirty.count({1, 0, 0}), 1u);
    EXPECT_EQ(dirty.count({2, 0, 0}), 1u);
    EXPECT_FALSE(dirty.empty());
}

// -- the dependent pair: one sequential job, bit-identical to pre-99.10 ---------

TEST(LightJobTest, DependentPairSequentialUnchanged) {
    // Two new columns 2 apart (footprints intersect): the plan is ONE
    // job, two boxes sequential. That path must be bit-identical to the
    // pre-99.10 worker (default marking) — light AND the World's dirty
    // set after the main thread applies the collected positions.
    const BlockRegistry reg = makeRegistry();
    const BlockID stone = idOf(reg, "stone");
    const BlockID glow = idOf(reg, "glowstone");
    const auto buildBase = [&](World& w) {
        for (int cx = -2; cx <= 4; ++cx) {
            for (int cz = -1; cz <= 1; ++cz) {
                if ((cx == 0 && cz == 0) || (cx == 2 && cz == 0)) {
                    continue; // the two new columns: NO chunk may exist
                }
                w.getOrCreateChunk({cx, 0, cz});
                fillBox(w, cx * kChunkSize, 0, cz * kChunkSize,
                        cx * kChunkSize + 15, 0, cz * kChunkSize + 15,
                        stone);
            }
        }
        // A ceiling over the west + middle (chunks -2 and 1): new column
        // (0,0)'s ceiling portion arrives with its prebuilt chunk, column
        // (2,0) stays open — mixed sky.
        fillBox(w, -32, 5, 0, -1, 5, 15, stone);
        fillBox(w, 16, 5, 0, 31, 5, 15, stone);
    };
    const auto makeColumn = [&](int localGlowY, bool ceiling) {
        Chunk c;
        for (int x = 0; x < kChunkSize; ++x) {
            for (int z = 0; z < kChunkSize; ++z) {
                c.set(x, 0, z, stone);
                if (ceiling) {
                    c.set(x, 5, z, stone);
                }
            }
        }
        c.set(8, localGlowY, 8, glow);
        return c;
    };
    const auto insertBoth = [&](World& w) {
        ASSERT_TRUE(w.insertChunk({0, 0, 0}, makeColumn(1, true)));
        ASSERT_TRUE(w.insertChunk({2, 0, 0}, makeColumn(3, false)));
    };
    World collected, sequential;
    buildBase(collected);
    buildBase(sequential);
    Lighting::propagateAll(collected, reg);
    Lighting::propagateAll(sequential, reg);
    clearAllDirty(collected);
    clearAllDirty(sequential);
    insertBoth(collected);
    insertBoth(sequential);
    ASSERT_EQ(collected.dirtyChunks(), sequential.dirtyChunks());

    // The plan: one job, set order — the pre-99.10 shape.
    const LightJobPlan plan = planLightJobs({{0, 0}, {2, 0}});
    ASSERT_EQ(plan.jobs.size(), 1u);
    ASSERT_EQ(plan.jobs[0].size(), 2u);
    EXPECT_EQ(plan.jobs[0][0], (ColumnPos{0, 0}));
    EXPECT_EQ(plan.jobs[0][1], (ColumnPos{2, 0}));

    std::set<ChunkPos> dirty;
    runJobs(collected, reg, plan.jobs, &dirty);
    runJobs(sequential, reg, plan.jobs, nullptr);
    expectSameLight(collected, sequential);
    for (const ChunkPos& p : dirty) {
        collected.markChunkDirty(p); // what collectLitColumns does
    }
    EXPECT_EQ(collected.dirtyChunks(), sequential.dirtyChunks());
}

// -- the disjoint pair: 2-job vs reversed 2-job vs forced 1-job -----------------
// Fixture: new columns (0,0) and (4,0) (gap 4 = bit-level independent).
// The pre-existing part is source-free stone (ground + a ceiling over the
// west half + a wall), so no stale value can leak from outside the
// regions (the InsertedColumnRelightMatchesFullPropagation pattern).

namespace {

void buildDisjointBase(World& w, const BlockID stone) {
    for (int cx = -2; cx <= 6; ++cx) {
        for (int cz = -1; cz <= 1; ++cz) {
            if ((cx == 0 && cz == 0) || (cx == 4 && cz == 0)) {
                continue; // the two new columns: NO chunk may exist
            }
            w.getOrCreateChunk({cx, 0, cz});
            fillBox(w, cx * kChunkSize, 0, cz * kChunkSize,
                    cx * kChunkSize + 15, 0, cz * kChunkSize + 15, stone);
        }
    }
    // Ceiling over x -32..47 MINUS new column (0,0)'s portion (x 0..15
    // arrives with its prebuilt chunk); column (4,0) stays open.
    fillBox(w, -32, 5, 0, -1, 5, 15, stone);
    fillBox(w, 16, 5, 0, 47, 5, 15, stone);
    // A wall in the corridor between the two new columns.
    fillBox(w, 32, 1, 8, 32, 6, 8, stone);
}

Chunk makeDisjointColumn(const BlockID stone, const BlockID glow,
                         int localGlowY, bool pillar, bool ceiling) {
    Chunk c;
    for (int x = 0; x < kChunkSize; ++x) {
        for (int z = 0; z < kChunkSize; ++z) {
            c.set(x, 0, z, stone);
            if (ceiling) {
                c.set(x, 5, z, stone);
            }
        }
    }
    c.set(8, localGlowY, 8, glow);
    if (pillar) {
        for (int y = 1; y <= 6; ++y) {
            c.set(4, y, 4, stone);
        }
    }
    return c;
}

} // namespace

TEST(LightJobTest, DisjointPairPathsBitIdentical) {
    const BlockRegistry reg = makeRegistry();
    const BlockID stone = idOf(reg, "stone");
    const BlockID glow = idOf(reg, "glowstone");

    // The plan splits the pair (gap 4) — the 2-job shape.
    const LightJobPlan plan = planLightJobs({{0, 0}, {4, 0}});
    ASSERT_EQ(plan.jobs.size(), 2u);
    ASSERT_EQ(plan.jobs[0], (std::vector<ColumnPos>{{0, 0}}));
    ASSERT_EQ(plan.jobs[1], (std::vector<ColumnPos>{{4, 0}}));

    const std::set<ColumnPos> both{{0, 0}, {4, 0}};
    const auto insertBoth = [&](World& w) {
        ASSERT_TRUE(w.insertChunk({0, 0, 0},
                                  makeDisjointColumn(stone, glow, 1, false,
                                                     true)));
        ASSERT_TRUE(w.insertChunk({4, 0, 0},
                                  makeDisjointColumn(stone, glow, 3, true,
                                                     false)));
    };
    // P1: the two jobs in PLAN order.  P2: the same two jobs in REVERSE
    // order (the parallel completion order is unspecified — independence
    // means the final light must not depend on it).  P3: forced ONE job,
    // two boxes sequential (the pre-99.10 behavior).
    const auto buildTwin = [&]() {
        World w;
        buildDisjointBase(w, stone);
        Lighting::propagateAll(w, reg);
        clearAllDirty(w);
        insertBoth(w);
        return w;
    };
    World p1 = buildTwin();
    World p2 = buildTwin();
    World p3 = buildTwin();
    std::set<ChunkPos> d1, d2, d3;
    runJobs(p1, reg, plan.jobs, &d1);
    std::vector<std::vector<ColumnPos>> reversed = plan.jobs;
    std::swap(reversed[0], reversed[1]);
    runJobs(p2, reg, reversed, &d2);
    runJobs(p3, reg,
            {{std::vector<ColumnPos>(both.begin(), both.end())}}, &d3);
    expectSameLight(p1, p2);
    expectSameLight(p1, p3);
    expectSameLight(p2, p3);
    // The collected net changes agree across the paths, and after the
    // main thread applies them (collectLitColumns) the World dirty sets
    // agree too.
    EXPECT_EQ(d1, d2);
    EXPECT_EQ(d1, d3);
    EXPECT_FALSE(d1.empty());
    const auto apply = [](World& w, const std::set<ChunkPos>& d) {
        for (const ChunkPos& pos : d) {
            w.markChunkDirty(pos);
        }
    };
    apply(p1, d1);
    apply(p2, d2);
    apply(p3, d3);
    EXPECT_EQ(p1.dirtyChunks(), p2.dirtyChunks());
    EXPECT_EQ(p1.dirtyChunks(), p3.dirtyChunks());
}

TEST(LightJobTest, DisjointPairMatchesFullPropagation) {
    // Oracle: the parallel 2-job path on the incremental world must land
    // on exactly the light a full propagation of the FINAL world (all
    // columns present) produces.
    const BlockRegistry reg = makeRegistry();
    const BlockID stone = idOf(reg, "stone");
    const BlockID glow = idOf(reg, "glowstone");

    // The reference: everything built in one world, one propagateAll.
    World reference;
    buildDisjointBase(reference, stone);
    ASSERT_TRUE(reference.insertChunk(
        {0, 0, 0}, makeDisjointColumn(stone, glow, 1, false, true)));
    ASSERT_TRUE(reference.insertChunk(
        {4, 0, 0}, makeDisjointColumn(stone, glow, 3, true, false)));
    Lighting::propagateAll(reference, reg);

    // The incremental twin: base lit, columns inserted, 2-job path.
    World incremental;
    buildDisjointBase(incremental, stone);
    Lighting::propagateAll(incremental, reg);
    ASSERT_TRUE(incremental.insertChunk(
        {0, 0, 0}, makeDisjointColumn(stone, glow, 1, false, true)));
    ASSERT_TRUE(incremental.insertChunk(
        {4, 0, 0}, makeDisjointColumn(stone, glow, 3, true, false)));
    const LightJobPlan plan = planLightJobs({{0, 0}, {4, 0}});
    ASSERT_EQ(plan.jobs.size(), 2u);
    std::set<ChunkPos> dirty;
    runJobs(incremental, reg, plan.jobs, &dirty);
    expectSameLight(incremental, reference);
}
