// Fire-spread tests (Phase 66, Fire v2): the deterministic per-roll
// ignitions of the flamable 5-neighbor cells of live fire cells
// (fireSpreadIgnitions — 4 horizontal + above, NEVER downward, Minecraft).
// The pure function is registry-aware (it reads each neighbor's `flamable`
// property), so the fixture registers a flamable wood, an inert stone and
// the non-flamable fire block. chance == 1.0 makes every flamable spread
// neighbor ignite (the roll is skipped), which turns the stochastic spread
// into an exact, fully deterministic set for precise assertions.
#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "block/block_registry.hpp"
#include "core/json.hpp"
#include "world/fire_spread.hpp"
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

// A registry with a flamable wood, an inert stone and the fire block
// (the spread source; the fire is NOT flamable — it cannot re-ignite).
BlockRegistry fireRegistry() {
    BlockRegistry reg;
    const std::string wood =
        "{\"id\":\"wood\",\"properties\":{\"hardness\":2.0,"
        "\"drops\":\"wood\",\"flamable\":true},"
        "\"model\":{\"textures\":{\"top\":\"wood_top\",\"side\":\"wood_side\","
        "\"bottom\":\"wood_top\"}}}";
    const std::string stone =
        "{\"id\":\"stone\",\"properties\":{\"hardness\":1.5,"
        "\"drops\":\"cobblestone\"},"
        "\"model\":{\"textures\":{\"top\":\"stone\",\"side\":\"stone\","
        "\"bottom\":\"stone\"}}}";
    const std::string fire =
        "{\"id\":\"fire\",\"properties\":{\"hardness\":0.0,"
        "\"solid\":false,\"opaque\":false,\"transparent\":true,"
        "\"light_emission\":15},"
        "\"model\":{\"shape\":\"cross\",\"textures\":{\"cross\":\"fire\"}}}";
    for (const std::string& def : {wood, stone, fire}) {
        auto r = reg.registerBlock(parseDef(def));
        if (!r.ok()) {
            throw std::runtime_error(r.error().message);
        }
    }
    return reg;
}

// A world with a fire at (8, 1, 8) and — when requested — flamable wood in
// each of the six 3D neighbors (the five spread neighbors + below).
World fireWorld(BlockID fire, BlockID wood, bool withBelow) {
    World world;
    world.getOrCreateChunk({0, 0, 0});
    world.setBlock(8, 1, 8, fire);
    const int xs[4] = {9, 7, 8, 8};
    const int zs[4] = {8, 8, 9, 7};
    for (int i = 0; i < 4; ++i) {
        world.setBlock(xs[i], 1, zs[i], wood);
    }
    world.setBlock(8, 2, 8, wood); // above
    if (withBelow) {
        world.setBlock(8, 0, 8, wood); // below (never a spread target)
    }
    return world;
}

} // namespace

TEST(FireSpreadTest, EmptySetYieldsNoIgnitions) {
    World world;
    world.getOrCreateChunk({0, 0, 0});
    const BlockRegistry reg = fireRegistry();
    EXPECT_TRUE(
        fireSpreadIgnitions(world, reg, {}, 42, 0, 1.0).empty());
}

TEST(FireSpreadTest, ChanceZeroYieldsNoIgnitions) {
    const BlockRegistry reg = fireRegistry();
    const World world = fireWorld(reg.idForName("fire"),
                                  reg.idForName("wood"), true);
    EXPECT_TRUE(
        fireSpreadIgnitions(world, reg, {{8, 1, 8}}, 42, 0, 0.0).empty());
    // A mid probability still rolls (the no-ignition is the roll's
    // outcome, not the gate) — the chance-0 gate is exact.
    for (std::uint64_t f = 0; f < 60; ++f) {
        EXPECT_TRUE(
            fireSpreadIgnitions(world, reg, {{8, 1, 8}}, 42, f, 0.0).empty());
    }
}

TEST(FireSpreadTest, ChanceOneIgnitesExactlyTheFiveSpreadNeighbors) {
    const BlockRegistry reg = fireRegistry();
    const World world = fireWorld(reg.idForName("fire"),
                                  reg.idForName("wood"), true);
    const std::vector<BlockPos> got =
        fireSpreadIgnitions(world, reg, {{8, 1, 8}}, 7, 0, 1.0);
    // All five spread neighbors (+X, -X, +Z, -Z, +Y) — and NOT the cell
    // below (fire never spreads downward, Minecraft), even though it
    // holds flamable wood. The output is position-sorted (deterministic
    // apply order).
    const std::vector<BlockPos> want = {{7, 1, 8}, {8, 1, 7}, {8, 1, 9},
                                        {8, 2, 8}, {9, 1, 8}};
    EXPECT_EQ(got, want);
}

TEST(FireSpreadTest, NonFlammableNeighborsNeverIgnite) {
    const BlockRegistry reg = fireRegistry();
    const BlockID stone = reg.idForName("stone");
    World world;
    world.getOrCreateChunk({0, 0, 0});
    world.setBlock(8, 1, 8, reg.idForName("fire"));
    world.setBlock(9, 1, 8, stone);
    world.setBlock(8, 2, 8, stone);
    for (std::uint64_t f = 0; f < 20; ++f) {
        EXPECT_TRUE(
            fireSpreadIgnitions(world, reg, {{8, 1, 8}}, 1, f, 1.0).empty());
    }
}

TEST(FireSpreadTest, DeterministicIdenticalIgnitions) {
    const BlockRegistry reg = fireRegistry();
    const World world = fireWorld(reg.idForName("fire"),
                                  reg.idForName("wood"), false);
    for (int i = 0; i < 5; ++i) {
        const std::vector<BlockPos> a =
            fireSpreadIgnitions(world, reg, {{8, 1, 8}}, 99, 123, 0.5);
        const std::vector<BlockPos> b =
            fireSpreadIgnitions(world, reg, {{8, 1, 8}}, 99, 123, 0.5);
        EXPECT_EQ(a, b);
    }
}

TEST(FireSpreadTest, FrameAdvanceDecorrelatesRolls) {
    const BlockRegistry reg = fireRegistry();
    // ONE flamable spread neighbor only ((9, 1, 8), the +X cell): the
    // result is a pure per-frame coin flip for that single roll.
    World world;
    world.getOrCreateChunk({0, 0, 0});
    world.setBlock(8, 1, 8, reg.idForName("fire"));
    world.setBlock(9, 1, 8, reg.idForName("wood"));
    // At chance 0.5 over 30 frames the neighbor passes sometimes and not
    // always (P(all 30 pass) = P(all 30 fail) = 2^-30 — not a flaky
    // bound). The roll state advances with the frame (PITS #4).
    int passes = 0;
    for (std::uint64_t f = 0; f < 30; ++f) {
        const std::vector<BlockPos> got =
            fireSpreadIgnitions(world, reg, {{8, 1, 8}}, 5, f, 0.5);
        passes += static_cast<int>(got.size() == 1 && got[0] == BlockPos{9, 1, 8});
    }
    EXPECT_GT(passes, 0);
    EXPECT_LT(passes, 30);
}

TEST(FireSpreadTest, SharedFlammableNeighborIgnitesOnce) {
    const BlockRegistry reg = fireRegistry();
    const BlockID fire = reg.idForName("fire");
    const BlockID wood = reg.idForName("wood");
    // Two fires flanking one wood cell: (8, 1, 8) is the +X neighbor of
    // (7, 1, 8) and the -X neighbor of (9, 1, 8). Both rolls (different
    // fire-cell hashes) may pass, but the cell ignites ONCE.
    World world;
    world.getOrCreateChunk({0, 0, 0});
    world.setBlock(7, 1, 8, fire);
    world.setBlock(9, 1, 8, fire);
    world.setBlock(8, 1, 8, wood);
    const std::vector<BlockPos> got =
        fireSpreadIgnitions(world, reg, {{7, 1, 8}, {9, 1, 8}}, 11, 4, 1.0);
    EXPECT_EQ(got, (std::vector<BlockPos>{{8, 1, 8}}));
}

TEST(FireSpreadTest, NoFlamableBlocksFastExits) {
    // A registry without ANY flamable block: nothing can ever ignite,
    // regardless of the chance (the lava burn pass uses the same gate).
    BlockRegistry reg;
    const std::string stone =
        "{\"id\":\"stone\",\"properties\":{\"hardness\":1.5},"
        "\"model\":{\"textures\":{\"top\":\"stone\",\"side\":\"stone\","
        "\"bottom\":\"stone\"}}}";
    const std::string fire =
        "{\"id\":\"fire\",\"properties\":{\"hardness\":0.0,"
        "\"solid\":false,\"opaque\":false,\"transparent\":true},"
        "\"model\":{\"shape\":\"cross\",\"textures\":{\"cross\":\"fire\"}}}";
    for (const std::string& def : {stone, fire}) {
        auto r = reg.registerBlock(parseDef(def));
        if (!r.ok()) {
            throw std::runtime_error(r.error().message);
        }
    }
    World world;
    world.getOrCreateChunk({0, 0, 0});
    world.setBlock(8, 1, 8, reg.idForName("fire"));
    EXPECT_TRUE(
        fireSpreadIgnitions(world, reg, {{8, 1, 8}}, 1, 0, 1.0).empty());
}
