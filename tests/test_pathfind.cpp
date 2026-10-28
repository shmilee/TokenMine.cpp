// ENT-6: the pure BFS pathfinding (entity/pathfind) — the standable
// predicate + the route search (deterministic, frame-locked safe).

#include <gtest/gtest.h>

#include "block/block_registry.hpp"
#include "entity/pathfind.hpp"
#include "world/world.hpp"

using namespace mc;

namespace {

Json parseJson(const std::string& text) {
    auto r = Json::parse(text);
    if (!r.ok()) {
        ADD_FAILURE() << "parse failed: " << r.error().message;
        return Json::object();
    }
    return *r;
}

// stone: the FullCube solid floor material; slab: a non-FullCube floor
// (the integer-lattice deviation's rejection side).
BlockRegistry makeBlocks() {
    BlockRegistry blocks;
    auto r = blocks.registerBlock(parseJson(
        R"({"id":"stone","model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    if (!r.ok()) {
        ADD_FAILURE() << r.error().message;
    }
    r = blocks.registerBlock(parseJson(
        R"({"id":"slab","model":{"shape":"slab",)"
        R"("textures":{"top":"t","side":"t","bottom":"t"}}})"));
    if (!r.ok()) {
        ADD_FAILURE() << r.error().message;
    }
    return blocks;
}

struct Fixture {
    BlockRegistry blocks = makeBlocks();
    World world;
    BlockID stone;
    BlockID slab;

    Fixture() {
        stone = blocks.idForName("stone");
        slab = blocks.idForName("slab");
    }
    void floor(int x0, int x1, int z0, int z1, int y) {
        for (int x = x0; x <= x1; ++x) {
            for (int z = z0; z <= z1; ++z) {
                world.setBlock(x, y, z, stone);
            }
        }
    }
};

} // namespace

// -- the standable predicate -------------------------------------------------

TEST(PathfindStandableTest, StandableOnFullCubeFloor) {
    Fixture f;
    f.floor(0, 5, 0, 3, 0); // top surface y1
    EXPECT_TRUE(cellStandable(f.world, f.blocks, 0, 1, 0, 2));
}

TEST(PathfindStandableTest, NotStandableOnSlabFloor) {
    Fixture f;
    f.world.setBlock(0, 0, 0, f.slab); // top surface y0.5 — off the lattice
    EXPECT_FALSE(cellStandable(f.world, f.blocks, 0, 1, 0, 2));
}

TEST(PathfindStandableTest, NotStandableFeetInSolid) {
    Fixture f;
    f.floor(0, 5, 0, 3, 0);
    f.world.setBlock(0, 1, 0, f.stone);
    EXPECT_FALSE(cellStandable(f.world, f.blocks, 0, 1, 0, 2));
}

TEST(PathfindStandableTest, NotStandableHeadroomBlocked) {
    Fixture f;
    f.floor(0, 5, 0, 3, 0);
    f.world.setBlock(0, 2, 0, f.stone); // bodyCells 2: the head cell
    EXPECT_FALSE(cellStandable(f.world, f.blocks, 0, 1, 0, 2));
    // A 1-cell body (the chicken) fits.
    EXPECT_TRUE(cellStandable(f.world, f.blocks, 0, 1, 0, 1));
}

TEST(PathfindStandableTest, NotStandableWithoutFloor) {
    Fixture f;
    // Air column, no block below the feet.
    EXPECT_FALSE(cellStandable(f.world, f.blocks, 0, 1, 0, 2));
}

// -- the route search ---------------------------------------------------------

TEST(PathfindBFSTest, FlatStraightLine) {
    Fixture f;
    f.floor(0, 5, 0, 3, 0);
    const PathNode from{0, 1, 0, true};
    const PathNode target{4, 1, 0, true};
    auto r = pathfindBFS(f.world, f.blocks, from, target, 2, kPathMaxNodes);
    ASSERT_TRUE(r.ok());
    // The zone (within 1 of the target) ends the path at (3,1,0).
    ASSERT_EQ(r.value().size(), 3u);
    for (std::size_t i = 0; i < r.value().size(); ++i) {
        EXPECT_EQ(r.value()[i], (PathNode{static_cast<int>(i + 1), 1, 0, true}));
    }
    for (const PathNode& n : r.value()) {
        EXPECT_TRUE(cellStandable(f.world, f.blocks, n.x, n.y, n.z, 2));
    }
}

// A 2-high wall at x2 (z0..2) forces the route around its far end (z3):
// the shortest standable corridor is 8 nodes.
TEST(PathfindBFSTest, AroundWall) {
    Fixture f;
    f.floor(0, 5, 0, 3, 0);
    for (int y = 1; y <= 2; ++y) {
        for (int z = 0; z <= 2; ++z) {
            f.world.setBlock(2, y, z, f.stone);
        }
    }
    const PathNode from{0, 1, 0, true};
    const PathNode target{4, 1, 0, true};
    auto r = pathfindBFS(f.world, f.blocks, from, target, 2, kPathMaxNodes);
    ASSERT_TRUE(r.ok());
    ASSERT_EQ(r.value().size(), 8u);
    // Every node is standable (the wall cells are not: feet-in-solid),
    // and the last is inside the target zone.
    for (const PathNode& n : r.value()) {
        EXPECT_TRUE(cellStandable(f.world, f.blocks, n.x, n.y, n.z, 2));
    }
    const PathNode last = r.value().back();
    EXPECT_LE(std::abs(last.x - target.x), 1);
    EXPECT_LE(std::abs(last.y - target.y), 1);
    EXPECT_LE(std::abs(last.z - target.z), 1);
}

TEST(PathfindBFSTest, SealedBoxNoPath) {
    Fixture f;
    f.floor(0, 10, 0, 4, 0);
    // A sealed room at x0..5, z0..4 (walls y1..3, ceiling y4).
    for (int y = 1; y <= 3; ++y) {
        for (int z = 0; z <= 4; ++z) {
            f.world.setBlock(0, y, z, f.stone);
            f.world.setBlock(5, y, z, f.stone);
        }
        for (int x = 0; x <= 5; ++x) {
            f.world.setBlock(x, y, 0, f.stone);
            f.world.setBlock(x, y, 4, f.stone);
        }
    }
    for (int x = 0; x <= 5; ++x) {
        for (int z = 0; z <= 4; ++z) {
            f.world.setBlock(x, 4, z, f.stone);
        }
    }
    const PathNode from{2, 1, 2, true};
    const PathNode target{8, 1, 2, true};
    auto r = pathfindBFS(f.world, f.blocks, from, target, 2, kPathMaxNodes);
    EXPECT_FALSE(r.ok());
}

// A 1-deep pit (x1..3, floor at y0; the base floor at y1 is absent in
// the pit columns): the route falls in, crosses, and the target zone
// (within 1 of (4,2,0)) ends it at the pit's east rim — 3 nodes.
TEST(PathfindBFSTest, OneDeepPitFallAndCross) {
    Fixture f;
    f.floor(0, 0, 0, 0, 1);  // base floor west of the pit, top surface y2
    f.floor(4, 6, 0, 0, 1);  // base floor east of the pit
    f.floor(1, 3, 0, 0, 0);  // pit floor, top surface y1
    const PathNode from{0, 2, 0, true};
    const PathNode target{4, 2, 0, true};
    auto r = pathfindBFS(f.world, f.blocks, from, target, 2, kPathMaxNodes);
    ASSERT_TRUE(r.ok());
    ASSERT_EQ(r.value().size(), 3u);
    EXPECT_EQ(r.value()[0], (PathNode{1, 1, 0, true})); // the fall
    EXPECT_EQ(r.value()[2], (PathNode{3, 1, 0, true})); // the pit bottom rim
    for (const PathNode& n : r.value()) {
        EXPECT_TRUE(cellStandable(f.world, f.blocks, n.x, n.y, n.z, 2));
    }
}

// A 2-deep pit (x1..3, NO floor — the world bottom) has no standable
// corridor: the route goes around at z1 (5 nodes, never into the pit).
TEST(PathfindBFSTest, DeepPitAvoided) {
    Fixture f;
    f.floor(0, 6, 0, 1, 1); // base floor, top surface y2, z0..1
    // The pit: x1..3 at z0 — remove the base floor there.
    for (int x = 1; x <= 3; ++x) {
        f.world.setBlock(x, 1, 0, BlockID::air());
    }
    const PathNode from{0, 2, 0, true};
    const PathNode target{4, 2, 0, true};
    auto r = pathfindBFS(f.world, f.blocks, from, target, 2, kPathMaxNodes);
    ASSERT_TRUE(r.ok());
    // The zone ends the route at (3,2,1) (within 1 of the target).
    ASSERT_EQ(r.value().size(), 4u);
    for (const PathNode& n : r.value()) {
        // Never in the pit (x1..3, z0): every node is standable AND not
        // a pit cell.
        EXPECT_TRUE(cellStandable(f.world, f.blocks, n.x, n.y, n.z, 2));
        EXPECT_TRUE(!(n.z == 0 && n.x >= 1 && n.x <= 3));
    }
}

// A 1-tall raised platform (x2..4, floor at y1, top surface y2): the
// step-up edge crosses the 1.0 rise at x2 (no jump needed).
TEST(PathfindBFSTest, StepUpRoute) {
    Fixture f;
    f.floor(0, 1, 0, 0, 0); // approach, top surface y1
    for (int x = 2; x <= 4; ++x) {
        f.world.setBlock(x, 1, 0, f.stone);
    }
    const PathNode from{0, 1, 0, true};
    const PathNode target{4, 2, 0, true};
    auto r = pathfindBFS(f.world, f.blocks, from, target, 2, kPathMaxNodes);
    ASSERT_TRUE(r.ok());
    ASSERT_EQ(r.value().size(), 3u);
    EXPECT_EQ(r.value()[1], (PathNode{2, 2, 0, true})); // the step-up node
    for (const PathNode& n : r.value()) {
        EXPECT_TRUE(cellStandable(f.world, f.blocks, n.x, n.y, n.z, 2));
    }
}

TEST(PathfindBFSTest, DeterministicSameInputSamePath) {
    Fixture f;
    f.floor(0, 5, 0, 3, 0);
    for (int y = 1; y <= 2; ++y) {
        for (int z = 0; z <= 2; ++z) {
            f.world.setBlock(2, y, z, f.stone);
        }
    }
    const PathNode from{0, 1, 0, true};
    const PathNode target{4, 1, 0, true};
    auto a = pathfindBFS(f.world, f.blocks, from, target, 2, kPathMaxNodes);
    auto b = pathfindBFS(f.world, f.blocks, from, target, 2, kPathMaxNodes);
    ASSERT_TRUE(a.ok());
    ASSERT_TRUE(b.ok());
    EXPECT_EQ(a.value(), b.value());
}

TEST(PathfindBFSTest, FromInTargetZoneEmptyPath) {
    Fixture f;
    f.floor(0, 5, 0, 3, 0);
    const PathNode from{4, 1, 0, true};
    const PathNode target{4, 1, 0, true};
    auto r = pathfindBFS(f.world, f.blocks, from, target, 2, kPathMaxNodes);
    ASSERT_TRUE(r.ok());
    EXPECT_TRUE(r.value().empty());
}

TEST(PathfindBFSTest, FromNotStandableFails) {
    Fixture f;
    f.floor(0, 5, 0, 3, 0);
    // Feet inside the floor stone (y0).
    const PathNode from{0, 0, 0, true};
    const PathNode target{4, 1, 0, true};
    auto r = pathfindBFS(f.world, f.blocks, from, target, 2, kPathMaxNodes);
    EXPECT_FALSE(r.ok());
}

TEST(PathfindBFSTest, NodeCapExceeded) {
    Fixture f;
    f.floor(0, 6, 0, 0, 0);
    const PathNode from{0, 1, 0, true};
    const PathNode target{6, 1, 0, true};
    // The route needs 5 nodes (the zone ends it at (5,1,0)); the cap
    // counts expanded nodes including the start, so 4 must refuse and
    // 6 succeeds.
    auto r = pathfindBFS(f.world, f.blocks, from, target, 2, 4);
    EXPECT_FALSE(r.ok());
    auto ok = pathfindBFS(f.world, f.blocks, from, target, 2, 6);
    ASSERT_TRUE(ok.ok());
    EXPECT_EQ(ok.value().size(), 5u);
}
