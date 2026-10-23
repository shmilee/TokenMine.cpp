// 99.11 fluid catch-up tests: the pure decision logic (changedCellColumns /
// planFluidLightJobs — the per-column pending set, the set-order budget,
// and the cross-kind window exclusion against the 99.10 streaming jobs)
// and the convergence properties of the catch-up re-light (the test seam:
// the jobs run Lighting::updateRegion on the main thread through
// columnLightBox with newTerrain = true + dirtyOut — the SAME call the
// worker runs — so a test drains the pending set in any order and
// compares the light against a full propagateAll of the final world).
//
// The oracle claims pinned here (the 99.11 design argument, see the
// header of world/light_job.hpp):
//   - per-column self-sufficiency: one column's re-light fixes that
//     column's light exactly (the light dependency region — <= 15
//     horizontal steps + the vertical run — is inside the column box's
//     ±kLightUpdateRadius footprint), so a fully drained pending set
//     lands on EXACTLY the propagateAll light of the final world;
//   - idempotency: re-draining after further churn converges again;
//   - confluence: two independent columns' jobs in either completion
//     order (plus the forced sequential shape) land on the same light.
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "block/block_registry.hpp"
#include "core/json.hpp"
#include "world/fluid.hpp"
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

// stone=opaque; fire = the burn pass's target (light_emission 15,
// non-opaque: the shipped fire.json's lighting-relevant fields).
BlockRegistry makeRegistry() {
    BlockRegistry reg;
    auto r1 = reg.registerBlock(
        parseDef(cubeDef("stone", "{}",
                         R"({"top":"t","side":"t","bottom":"t"})")));
    auto r2 = reg.registerBlock(
        parseDef(cubeDef("fire",
                         R"({"light_emission":15,"opaque":false,"transparent":true,"solid":false})",
                         R"({"top":"t","side":"t","bottom":"t"})")));
    if (!r1.ok() || !r2.ok()) {
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
                    ADD_FAILURE() << "setBlock(" << x << "," << y << ","
                                  << z << ") failed";
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
    EXPECT_TRUE(diffs.empty()) << "light differs in " << diffs.size()
                               << " cells";
}

// ONE chunk's light compared directly (the per-column self-sufficiency
// check: a processed column's chunk is exact even while its neighbors
// are still pending).
void expectSameChunkLight(const World& a, const World& b, const ChunkPos& p) {
    const Chunk* ca = a.findChunk(p);
    const Chunk* cb = b.findChunk(p);
    ASSERT_NE(ca, nullptr);
    ASSERT_NE(cb, nullptr);
    EXPECT_EQ(std::memcmp(ca->skyLightData(), cb->skyLightData(),
                          Chunk::kVolume),
              0)
        << "sky light of chunk (" << p.x << "," << p.y << "," << p.z
        << ") differs";
    EXPECT_EQ(std::memcmp(ca->blockLightData(), cb->blockLightData(),
                          Chunk::kVolume),
              0)
        << "block light of chunk (" << p.x << "," << p.y << "," << p.z
        << ") differs";
}

// The full-column box of a taken column, exactly as the game's fluid
// catch-up job computes it (columnLightBox; newTerrain = true — the
// 99.11 job's call shape, the 99.9 no-op skip disabled for the reason
// documented in postFluidLightJobs).
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

// The game's per-frame catch-up step, on one thread: take the plan,
// run the jobs (the worker's call shape), apply the net-changed chunks
// (collectLitColumns), and drop the taken columns from the pending set
// (postFluidLightJobs). Returns the taken set.
std::set<ColumnPos> drainFrame(World& w, const BlockRegistry& reg,
                               std::set<ColumnPos>& pending,
                               const std::set<ColumnPos>& streaming,
                               std::set<ChunkPos>* dirtyOut) {
    const LightJobPlan plan = planFluidLightJobs(pending, streaming);
    std::set<ColumnPos> taken;
    for (const std::vector<ColumnPos>& job : plan.jobs) {
        for (const ColumnPos& col : job) {
            taken.insert(col);
        }
    }
    runJobs(w, reg, plan.jobs, dirtyOut);
    if (dirtyOut != nullptr) {
        for (const ChunkPos& pos : *dirtyOut) {
            w.markChunkDirty(pos);
        }
    }
    for (const ColumnPos& col : taken) {
        pending.erase(col);
    }
    return taken;
}

// The coast fixture: 7 column-chunks (cx 0..6, y-layer 0 only, z 0..15)
// of stone floor (y=0). fillWater then places the 8-deep level-8 pool
// over the contiguous column range [waterFrom, waterTo) (y 1..8) — the
// settled end state of a coast film cascade; `changed` collects the
// written cells (the fluid tick's `changed`).
void buildFloor(World& w, const BlockID stone) {
    for (int cx = 0; cx <= 6; ++cx) {
        w.getOrCreateChunk({cx, 0, 0});
        fillBox(w, cx * kChunkSize, 0, 0, cx * kChunkSize + 15, 0, 15, stone);
    }
}

void fillWater(World& w, int waterFrom, int waterTo,
               std::vector<BlockPos>* changed) {
    if (changed != nullptr) {
        changed->clear();
    }
    for (int x = waterFrom * kChunkSize; x < waterTo * kChunkSize; ++x) {
        for (int z = 0; z < kChunkSize; ++z) {
            for (int y = 1; y <= 8; ++y) {
                if (!w.setFluid(x, y, z, FluidKind::Water, 8)) {
                    ADD_FAILURE() << "setFluid(" << x << "," << y << "," << z
                                  << ") failed";
                }
                if (changed != nullptr) {
                    changed->push_back({x, y, z});
                }
            }
        }
    }
}

} // namespace

// -- the budget constant -------------------------------------------------------

TEST(FluidLightTest, BudgetConstantIsNamed) {
    // The design's B: 2 fluid columns per frame (sized against the
    // step-0 coast churn ~100-170 changed columns/tick >> B, and the
    // 99.10 flush floor ~8.4 ms/box).
    EXPECT_EQ(kFluidLightColumnsPerFrame, 2);
}

// -- changedCellColumns: the per-column merge ----------------------------------

TEST(FluidLightTest, ChangedCellColumnsMergesPerColumn) {
    // Every cell of one column (any x within it, any y) = ONE entry.
    EXPECT_EQ(changedCellColumns({{0, 5, 0}, {15, 127, 1}, {1, 0, 15}}),
              (std::set<ColumnPos>{{0, 0}}));
    // Boundary-adjacent cells in two columns = two entries, set order.
    EXPECT_EQ(changedCellColumns({{15, 0, 15}, {16, 0, 16}}),
              (std::set<ColumnPos>{{0, 0}, {1, 1}}));
    // Negative coordinates: floorDiv (hard rule 1) — cells x=-1 and
    // x=-16 are BOTH in column -1; z=31 in column 1, z=0 in column 0.
    EXPECT_EQ(changedCellColumns({{-1, 5, 31}, {-16, 5, 0}}),
              (std::set<ColumnPos>{{-1, 0}, {-1, 1}}));
    // A settled world changes nothing.
    EXPECT_TRUE(changedCellColumns({}).empty());
}

// -- planFluidLightJobs: budget + set order ------------------------------------

TEST(FluidLightTest, PlanEmptyPending) {
    EXPECT_TRUE(planFluidLightJobs({}, {}).jobs.empty());
    // Pending but every candidate blocked by a streaming column: no jobs,
    // nothing taken (the entries stay pending for a later frame).
    EXPECT_TRUE(planFluidLightJobs({{0, 0}}, {{0, 0}}).jobs.empty());
}

TEST(FluidLightTest, PlanTakesFirstBudgetInSetOrder) {
    // 5 pending (inserted OUT of order): the frame takes exactly the
    // first 2 in set order — never more (the budget), never another pair
    // (the set order is the only order).
    std::set<ColumnPos> pending;
    pending.insert({14, 0});
    pending.insert({2, 0});
    pending.insert({0, 0});
    pending.insert({9, 0});
    pending.insert({5, 0});
    const LightJobPlan plan = planFluidLightJobs(pending, {});
    std::set<ColumnPos> taken;
    for (const std::vector<ColumnPos>& job : plan.jobs) {
        for (const ColumnPos& col : job) {
            taken.insert(col);
        }
    }
    EXPECT_EQ(taken, (std::set<ColumnPos>{{0, 0}, {2, 0}}));
}

TEST(FluidLightTest, PlanBudgetTruncationFrames) {
    // The design's truncation: 5 pending -> frame 1 takes the first 2,
    // frame 2 the next 2, frame 3 the last 1 (the per-frame selection,
    // repeated until the set drains).
    std::set<ColumnPos> pending{{0, 0}, {2, 0}, {5, 0}, {9, 0}, {14, 0}};
    std::vector<std::set<ColumnPos>> perFrame;
    while (!pending.empty()) {
        const LightJobPlan plan = planFluidLightJobs(pending, {});
        ASSERT_FALSE(plan.jobs.empty());
        std::set<ColumnPos> taken;
        for (const std::vector<ColumnPos>& job : plan.jobs) {
            for (const ColumnPos& col : job) {
                taken.insert(col);
            }
        }
        for (const ColumnPos& col : taken) {
            pending.erase(col);
        }
        perFrame.push_back(taken);
    }
    ASSERT_EQ(perFrame.size(), 3u);
    EXPECT_EQ(perFrame[0], (std::set<ColumnPos>{{0, 0}, {2, 0}}));
    EXPECT_EQ(perFrame[1], (std::set<ColumnPos>{{5, 0}, {9, 0}}));
    EXPECT_EQ(perFrame[2], (std::set<ColumnPos>{{14, 0}}));
}

TEST(FluidLightTest, PlanDependentPairStaysOneJob) {
    // The 99.10 criterion through the fluid plan (regression): gap 3 on
    // both axes = write-disjoint but the boundary-read ring crosses ->
    // ONE sequential job; the boxes in set order.
    const LightJobPlan plan = planFluidLightJobs({{0, 0}, {3, 0}}, {});
    ASSERT_EQ(plan.jobs.size(), 1u);
    EXPECT_EQ(plan.jobs[0], (std::vector<ColumnPos>{{0, 0}, {3, 0}}));
    const LightJobPlan plan2 = planFluidLightJobs({{0, 0}, {2, 0}}, {});
    ASSERT_EQ(plan2.jobs.size(), 1u);
    EXPECT_EQ(plan2.jobs[0], (std::vector<ColumnPos>{{0, 0}, {2, 0}}));
}

TEST(FluidLightTest, PlanIndependentPairSplitsParallel) {
    // Gap >= 4 on either axis -> two parallel jobs (the 99.10 criterion
    // through the fluid plan).
    const LightJobPlan plan = planFluidLightJobs({{0, 0}, {4, 0}}, {});
    ASSERT_EQ(plan.jobs.size(), 2u);
    EXPECT_EQ(plan.jobs[0], (std::vector<ColumnPos>{{0, 0}}));
    EXPECT_EQ(plan.jobs[1], (std::vector<ColumnPos>{{4, 0}}));
    const LightJobPlan plan2 = planFluidLightJobs({{-1, -60}, {-1, 0}}, {});
    ASSERT_EQ(plan2.jobs.size(), 2u);
    EXPECT_EQ(plan2.jobs[0], (std::vector<ColumnPos>{{-1, -60}}));
    EXPECT_EQ(plan2.jobs[1], (std::vector<ColumnPos>{{-1, 0}}));
}

TEST(FluidLightTest, PlanSkipsStreamingBlockedCandidates) {
    // Cross-kind window exclusion: a candidate NOT independent of a
    // streaming job's column is skipped this frame (stays pending) and
    // does NOT consume the budget.
    // (0,0) vs streaming (2,0): gap 2 -> blocked; (10,0): gap 8 -> taken.
    const LightJobPlan plan = planFluidLightJobs({{0, 0}, {10, 0}},
                                                 {{2, 0}});
    std::set<ColumnPos> taken;
    for (const std::vector<ColumnPos>& job : plan.jobs) {
        for (const ColumnPos& col : job) {
            taken.insert(col);
        }
    }
    EXPECT_EQ(taken, (std::set<ColumnPos>{{10, 0}}));
    // Both candidates blocked (gap 1 and gap 3 — the read ring crosses):
    // no jobs at all.
    EXPECT_TRUE(planFluidLightJobs({{1, 0}, {3, 0}}, {{0, 0}}).jobs.empty());
    // The budget applies AFTER the exclusion: (0,0) blocked, the first
    // two eligible ((5,0),(9,0) — independent of each other) are taken.
    const LightJobPlan plan2 =
        planFluidLightJobs({{0, 0}, {5, 0}, {9, 0}}, {{0, 1}});
    std::set<ColumnPos> taken2;
    for (const std::vector<ColumnPos>& job : plan2.jobs) {
        for (const ColumnPos& col : job) {
            taken2.insert(col);
        }
    }
    EXPECT_EQ(taken2, (std::set<ColumnPos>{{5, 0}, {9, 0}}));
    EXPECT_EQ(plan2.jobs.size(), 2u); // gap 4: they split parallel
}

// -- the convergence oracle: coast pool, budgeted drain -> propagateAll --------
//
// 7-column pool, water placed in one "tick" (14336 changed cells -> 7
// pending columns). The drain runs the game's per-frame step
// (planFluidLightJobs + the jobs' updateRegion call + reap) at
// B = 2 columns/frame. After the set is empty, the light must be
// EXACTLY the full propagation of the final world — the idempotent
// convergence the design argues (per-column self-sufficiency: a column's
// light dependency region is inside its relight footprint).

TEST(FluidLightTest, CatchUpConvergesToFullPropagation) {
    const BlockRegistry reg = makeRegistry();
    const BlockID stone = idOf(reg, "stone");

    // The reference: floor + pool, one full propagation (the target).
    World reference;
    buildFloor(reference, stone);
    fillWater(reference, 0, 7, nullptr);
    Lighting::propagateAll(reference, reg);

    // The incremental twin in its PRE-TICK state: the dry floor fully
    // lit, then the tick's water arrives (the changed cells).
    World incremental;
    buildFloor(incremental, stone);
    Lighting::propagateAll(incremental, reg);
    std::vector<BlockPos> changed;
    fillWater(incremental, 0, 7, &changed);
    ASSERT_EQ(changed.size(), std::size_t(7) * kChunkSize * kChunkSize * 8u);

    std::set<ChunkPos> dirty;
    std::set<ColumnPos> pending = changedCellColumns(changed);
    ASSERT_EQ(pending.size(), 7u);

    int frames = 0;
    while (!pending.empty()) {
        const std::set<ColumnPos> taken =
            drainFrame(incremental, reg, pending, {}, &dirty);
        ASSERT_FALSE(taken.empty()) << "the budget must take a column";
        ++frames;
        if (frames == 1) {
            // Per-column self-sufficiency: the FIRST frame's first column
            // (set order: (0,0)) is already EXACT, while the far columns
            // are still pending (their chunks are not yet touched).
            expectSameChunkLight(incremental, reference, {0, 0, 0});
            EXPECT_NE(pending.size(), 0u);
        }
    }
    // 7 columns at 2/frame: frames of 2, 2, 2, 1.
    EXPECT_EQ(frames, 4);
    // The full drain lands on exactly the full propagation.
    expectSameLight(incremental, reference);
    // The net change was collected (the chunks re-mesh with the new
    // light — the dirtyOut is non-trivial: every water column's chunk
    // changed from the dry light).
    EXPECT_EQ(dirty.count({0, 0, 0}), 1u);
    EXPECT_EQ(dirty.count({6, 0, 0}), 1u);
    // The water attenuation is right (spot-check against the reference's
    // formula: an 8-deep level-8 pool reads 15 - 8*kFluidLightAttenuation
    // = 0 at the bottom, 12 at the top surface cell).
    EXPECT_EQ(incremental.getSkyLight(8, 8, 8), reference.getSkyLight(8, 8, 8));
    EXPECT_EQ(incremental.getSkyLight(8, 8, 8), 12u);
    EXPECT_EQ(incremental.getSkyLight(8, 1, 8), 0u);
    EXPECT_EQ(incremental.getSkyLight(8, 0, 8), 0u); // the floor

    // Idempotency under further churn: the film retreats from the west
    // column (its water drains in the next "tick"). One pending column,
    // one drain frame, and the light converges again — exactly the
    // propagateAll light of the NEW world.
    std::vector<BlockPos> changed2;
    for (int x = 0; x < kChunkSize; ++x) {
        for (int z = 0; z < kChunkSize; ++z) {
            for (int y = 1; y <= 8; ++y) {
                if (!incremental.setFluid(x, y, z, FluidKind::Water, 0)) {
                    ADD_FAILURE() << "drain setFluid failed";
                }
                changed2.push_back({x, y, z});
            }
        }
    }
    std::set<ColumnPos> pending2 = changedCellColumns(changed2);
    ASSERT_EQ(pending2, (std::set<ColumnPos>{{0, 0}}));
    dirty.clear();
    const std::set<ColumnPos> taken2 =
        drainFrame(incremental, reg, pending2, {}, &dirty);
    EXPECT_EQ(taken2, (std::set<ColumnPos>{{0, 0}}));
    ASSERT_TRUE(pending2.empty());

    World reference2;
    buildFloor(reference2, stone);
    fillWater(reference2, 1, 7, nullptr);
    Lighting::propagateAll(reference2, reg);
    expectSameLight(incremental, reference2);
}

// -- confluence: two fluid columns' jobs in either completion order ------------

TEST(FluidLightTest, RelightOrderIndependent) {
    // Two water pools with a dry 3-column gap between them (columns 0
    // and 4 — lightRegionsIndependent: gap 4). The plan splits them into
    // two jobs; the workers' completion order is unspecified. Running the
    // two jobs in plan order, in REVERSE order, or forced into one
    // sequential job must all land on the same light — and on the full
    // propagation.
    const BlockRegistry reg = makeRegistry();
    const BlockID stone = idOf(reg, "stone");

    const auto buildTwin = [&]() {
        World w;
        // The pool columns, dry and lit (the pre-tick state).
        for (int cx = 0; cx <= 6; ++cx) {
            w.getOrCreateChunk({cx, 0, 0});
            fillBox(w, cx * kChunkSize, 0, 0, cx * kChunkSize + 15, 0, 15,
                    stone);
        }
        Lighting::propagateAll(w, reg);
        // The tick: water in columns 0 and 4 only.
        fillWater(w, 0, 1, nullptr);
        fillWater(w, 4, 5, nullptr);
        return w;
    };

    World reference = buildTwin();
    Lighting::propagateAll(reference, reg);

    const std::set<ColumnPos> pending{{0, 0}, {4, 0}};
    const LightJobPlan plan = planFluidLightJobs(pending, {});
    ASSERT_EQ(plan.jobs.size(), 2u); // gap 4: the parallel shape

    World p1 = buildTwin(); // plan order
    World p2 = buildTwin(); // reverse order (the parallel completion
                            // order is unspecified — it must not matter)
    World p3 = buildTwin(); // forced one sequential job
    std::set<ChunkPos> d1, d2, d3;
    runJobs(p1, reg, plan.jobs, &d1);
    std::vector<std::vector<ColumnPos>> reversed = plan.jobs;
    std::swap(reversed[0], reversed[1]);
    runJobs(p2, reg, reversed, &d2);
    runJobs(p3, reg, {{std::vector<ColumnPos>{{0, 0}, {4, 0}}}}, &d3);

    expectSameLight(p1, p2);
    expectSameLight(p1, p3);
    expectSameLight(p1, reference);
    EXPECT_EQ(d1, d2);
    EXPECT_EQ(d1, d3);
    EXPECT_FALSE(d1.empty());
}

// -- the burn pass: a fire cell's column in the pending set ----------------------

TEST(FluidLightTest, BurnedFireColumnRelightLandsEmissionFifteen) {
    // The burn pass turns a flamable block into the FIRE block via
    // world.setBlock (bypassing the Game choke point) and the cell rides
    // the tick's `changed` — so its column joins the pending set. The
    // catch-up re-light must land the fire's light-15 emission (and its
    // decay) exactly as the full propagation does.
    const BlockRegistry reg = makeRegistry();
    const BlockID stone = idOf(reg, "stone");
    const BlockID fire = idOf(reg, "fire");

    const auto build = [&](World& w) {
        for (int cx = 0; cx <= 2; ++cx) {
            w.getOrCreateChunk({cx, 0, 0});
            fillBox(w, cx * kChunkSize, 0, 0, cx * kChunkSize + 15, 0, 15,
                    stone);
        }
    };
    World incremental, reference;
    build(incremental);
    build(reference);
    Lighting::propagateAll(incremental, reg);
    Lighting::propagateAll(reference, reg);

    // The burn: (8,1,8) becomes fire (both worlds — the reference gets
    // the full propagation of the burned world).
    const BlockPos burned{8, 1, 8};
    ASSERT_TRUE(incremental.setBlock(burned.x, burned.y, burned.z, fire));
    ASSERT_TRUE(reference.setBlock(burned.x, burned.y, burned.z, fire));
    Lighting::propagateAll(reference, reg);

    // The game's tick-side step: the burned cell in `changed` -> its
    // column in the pending set -> one drain frame.
    std::set<ColumnPos> pending = changedCellColumns({burned});
    ASSERT_EQ(pending, (std::set<ColumnPos>{{0, 0}}));
    std::set<ChunkPos> dirty;
    const std::set<ColumnPos> taken =
        drainFrame(incremental, reg, pending, {}, &dirty);
    EXPECT_EQ(taken, (std::set<ColumnPos>{{0, 0}}));
    ASSERT_TRUE(pending.empty());

    // The fire's light 15 (the emission source keeps its value) + the
    // four dry neighbors at 14 (entering an air cell costs 1); the floor
    // stone (full opacity) is not entered: 0.
    EXPECT_EQ(incremental.getBlockLight(burned.x, burned.y, burned.z), 15u);
    EXPECT_EQ(incremental.getBlockLight(9, 1, 8), 14u);
    EXPECT_EQ(incremental.getBlockLight(7, 1, 8), 14u);
    EXPECT_EQ(incremental.getBlockLight(8, 2, 8), 14u);
    EXPECT_EQ(incremental.getBlockLight(8, 0, 8), 0u);
    // Exact against the full propagation, and the net change was
    // collected (the chunk re-meshes with the new light).
    expectSameLight(incremental, reference);
    EXPECT_EQ(dirty.count({0, 0, 0}), 1u);
}
