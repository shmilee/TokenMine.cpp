// Entity system tests (Phase 10): the shared AABB collision physics (via
// the concrete ItemEntity), item merge/despawn rules, projectile
// impact/despawn behavior, and the dropped-item geometry conventions.
#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "block/block_registry.hpp"
#include "core/json.hpp"
#include "entity/entity.hpp"
#include "entity/item_entity.hpp"
#include "entity/item_mesh.hpp"
#include "entity/projectile.hpp"
#include "player/player.hpp"
#include "renderer/texture_atlas.hpp"
#include "world/world.hpp"

using namespace mc;

namespace {

constexpr double kDt = 1.0 / 60.0;

Json parseJson(const std::string& text) {
    auto r = Json::parse(text);
    if (!r.ok()) {
        ADD_FAILURE() << "parse failed: " << r.error().message;
        return Json::object();
    }
    return *r;
}

// stone + dirt (two distinct solid blocks, for the merge tests) + water
// (non-solid).
BlockRegistry makeBlocks() {
    BlockRegistry blocks;
    auto r = blocks.registerBlock(parseJson(
        R"({"id":"stone","model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    if (!r.ok()) {
        ADD_FAILURE() << r.error().message;
    }
    r = blocks.registerBlock(parseJson(
        R"({"id":"dirt","model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    if (!r.ok()) {
        ADD_FAILURE() << r.error().message;
    }
    r = blocks.registerBlock(parseJson(
        R"({"id":"water","properties":{"solid":false,"opaque":false,)"
        R"("transparent":true},"model":{"textures":{"top":"w","side":"w","bottom":"w"}}})"));
    if (!r.ok()) {
        ADD_FAILURE() << r.error().message;
    }
    // Phase 85: a bottom slab (solid in its lower half only).
    r = blocks.registerBlock(parseJson(
        R"({"id":"slab","model":{"shape":"slab","textures":)"
        R"({"top":"t","side":"t","bottom":"t"}}})"));
    if (!r.ok()) {
        ADD_FAILURE() << r.error().message;
    }
    // Phase 95: a wall (solid in y 0..1.5, a 1.5-block-tall barrier).
    r = blocks.registerBlock(parseJson(
        R"({"id":"wall","model":{"shape":"wall","textures":)"
        R"({"top":"t","side":"t","bottom":"t"}}})"));
    if (!r.ok()) {
        ADD_FAILURE() << r.error().message;
    }
    // Phase 98: a fence (the SAME solid region as the wall — the thin posts
    // are mesh-only; the collision is a full 1.5 barrier).
    r = blocks.registerBlock(parseJson(
        R"({"id":"fence","model":{"shape":"fence","textures":{"side":"t"}}})"));
    if (!r.ok()) {
        ADD_FAILURE() << r.error().message;
    }
    // Phase 99: a glass pane (a full-cell solid 1.0 — the 1/16 sheet is
    // mesh-only; JUMPABLE, unlike the wall/fence 1.5 barrier). Non-opaque +
    // transparent are FORCED by the shape (light passes through the sheet).
    r = blocks.registerBlock(parseJson(
        R"({"id":"glass_pane","properties":{"opaque":false,"transparent":true},)"
        R"("model":{"shape":"glass_pane","textures":{"side":"t"}}})"));
    if (!r.ok()) {
        ADD_FAILURE() << r.error().message;
    }
    // Phase 87: a north-facing stair (solid height varies with z: full at
    // the -Z end, zero at the +Z end).
    r = blocks.registerBlock(parseJson(
        R"({"id":"stairs","model":{"shape":"stairs","textures":)"
        R"({"top":"t","side":"t","bottom":"t"}}})"));
    if (!r.ok()) {
        ADD_FAILURE() << r.error().message;
    }
    // Phase 89: a south-facing stair (full at the +Z end, zero at -Z) — the
    // opposite slope, for the 4-direction test.
    r = blocks.registerBlock(parseJson(
        R"({"id":"stairs_south","model":{"shape":"stairs","stair_facing":2,)"
        R"("textures":{"top":"t","side":"t","bottom":"t"}}})"));
    if (!r.ok()) {
        ADD_FAILURE() << r.error().message;
    }
    return blocks;
}

// A solid stone floor covering x,z in [-16, 16) at y=0 (top surface y=1).
struct Fixture {
    BlockRegistry blocks = makeBlocks();
    World world;
    BlockID stone;
    BlockID dirt;

    Fixture() {
        stone = blocks.idForName("stone");
        dirt = blocks.idForName("dirt");
        for (int x = -16; x < 16; ++x) {
            for (int z = -16; z < 16; ++z) {
                world.setBlock(x, 0, z, stone);
            }
        }
    }

    // A wall of stone in the x=const plane.
    void wallX(int wx, int y0, int y1, int z0, int z1) {
        for (int y = y0; y < y1; ++y) {
            for (int z = z0; z < z1; ++z) {
                world.setBlock(wx, y, z, stone);
            }
        }
    }

    // `item` defaults to dirt (a second distinct item, for merge tests);
    // names match the block items (stone/dirt are registered above).
    ItemEntity makeItem(Vec3 pos, const std::string& item = "dirt",
                        int maxStack = 64, int count = 1) const {
        return ItemEntity(world, blocks, item, maxStack, count, pos);
    }

    void run(Entity& e, int n, double dt = kDt) {
        for (int i = 0; i < n; ++i) {
            e.update(dt);
        }
    }
};

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
    auto r = AtlasLayout::parse(parseJson(
        std::string("{\"columns\":1,\"rows\":") + std::to_string(rows) +
        ",\"tiles\":" + tiles + "}"));
    if (!r.ok()) {
        ADD_FAILURE() << r.error().message;
    }
    return *r;
}

} // namespace

// -- shared physics (through ItemEntity: the simplest concrete entity) -----

TEST(EntityTest, FallsAndRestsExactlyOnFloor) {
    Fixture f;
    ItemEntity item = f.makeItem({8.5f, 10.0f, 8.5f});
    f.run(item, 120); // 2 s: a 9-block fall takes < 1 s
    EXPECT_FLOAT_EQ(item.position().y, 1.0f);
    EXPECT_EQ(item.velocity().y, 0.0f);
    EXPECT_TRUE(item.onGround());
}

TEST(EntityTest, RestsWithoutJitter) {
    Fixture f;
    ItemEntity item = f.makeItem({8.5f, 1.0f, 8.5f}); // bottom on the surface
    f.run(item, 240); // 4 s
    // The Y resolve clamps to the exact integer boundary (same as the
    // player): no drift.
    EXPECT_FLOAT_EQ(item.position().y, 1.0f);
    EXPECT_FLOAT_EQ(item.position().x, 8.5f);
    EXPECT_FLOAT_EQ(item.position().z, 8.5f);
    EXPECT_TRUE(item.onGround());
}

TEST(EntityTest, WallBlocksMovementAndClampsFlush) {
    Fixture f;
    f.wallX(12, 1, 6, -16, 16); // wall in the plane x = 12 (cells [12, 13))
    // Close enough that the friction-limited drift (4/2.5 = 1.6 blocks total)
    // reaches the wall (0.375 blocks away).
    ItemEntity item = f.makeItem({11.5f, 1.0f, 8.5f});
    item.setVelocity({4.0f, 0.0f, 0.0f});
    f.run(item, 60);
    // Clamped flush: item max.x (x + size/2) == wall min.x (12).
    EXPECT_FLOAT_EQ(item.position().x, 12.0f - ItemEntity::kSize * 0.5f);
    EXPECT_EQ(item.velocity().x, 0.0f);
    EXPECT_LT(item.position().x, 12.0f); // never passed through
}

TEST(EntityTest, SlidesAlongWall) {
    Fixture f;
    f.wallX(12, 1, 6, -16, 16);
    ItemEntity item = f.makeItem({11.5f, 1.0f, 10.0f});
    item.setVelocity({4.0f, 0.0f, -4.0f});
    f.run(item, 60); // 1 s
    // The blocked axis stays clamped, the other keeps moving: sliding.
    EXPECT_FLOAT_EQ(item.position().x, 12.0f - ItemEntity::kSize * 0.5f);
    EXPECT_LT(item.position().z, 9.0f);
    EXPECT_TRUE(item.onGround());
}

TEST(EntityTest, HighSpeedFallDoesNotTunnelThroughFloor) {
    Fixture f;
    ItemEntity item = f.makeItem({8.5f, 10.0f, 8.5f});
    f.run(item, 44); // build up fall speed (~23.5 blocks/s)
    ASSERT_LT(item.position().y, 1.5f);
    ASSERT_GT(item.position().y, 1.0f); // still above the floor
    item.update(0.25); // one large tick: ~7.7 blocks of motion
    EXPECT_FLOAT_EQ(item.position().y, 1.0f); // landed, not tunneled through
    EXPECT_TRUE(item.onGround());
}

// -- ItemEntity rules ---------------------------------------------------------

TEST(ItemEntityTest, FrictionStopsHorizontalDrift) {
    Fixture f;
    ItemEntity item = f.makeItem({0.5f, 1.0f, 0.5f});
    item.setVelocity({4.0f, 0.0f, 0.0f});
    f.run(item, 300); // 5 s: long past the decay (total drift 4/2.5 = 1.6)
    EXPECT_EQ(item.velocity().x, 0.0f);
    EXPECT_TRUE(item.onGround());
    EXPECT_GT(item.position().x, 0.5f); // it did drift before stopping
    const float xAfter = item.position().x;
    f.run(item, 60);
    // Truly at rest: no sub-pixel creep.
    EXPECT_FLOAT_EQ(item.position().x, xAfter);
}

TEST(ItemEntityTest, DespawnAfterAge) {
    Fixture f;
    ItemEntity item = f.makeItem({8.5f, 1.0f, 8.5f});
    f.run(item, 119, 1.0); // 119 s
    EXPECT_FALSE(item.shouldDespawn());
    item.update(1.0); // 120 s
    EXPECT_TRUE(item.shouldDespawn());
}

TEST(ItemEntityTest, DespawnBelowKillPlane) {
    BlockRegistry blocks = makeBlocks();
    World world; // no floor at all: the void
    ItemEntity item(world, blocks, "stone", 64, 1, {8.5f, 10.0f, 8.5f});
    int ticks = 0;
    while (!item.shouldDespawn() && ticks < 500) {
        item.update(kDt);
        ++ticks;
    }
    EXPECT_LT(ticks, 500);
    EXPECT_TRUE(item.belowKillPlane());
}

TEST(ItemEntityTest, MergesSameBlockWithinDistance) {
    Fixture f;
    ItemEntity a = f.makeItem({8.0f, 1.0f, 8.0f});
    ItemEntity b = f.makeItem({8.5f, 1.0f, 8.0f}); // 0.5 apart
    EXPECT_TRUE(b.canMergeInto(a));
    a.absorb(b);
    EXPECT_EQ(a.count(), 2);
    // The absorbed entity is untouched — the game removes it.
    EXPECT_EQ(b.count(), 1);
}

TEST(ItemEntityTest, NoMergeDifferentItem) {
    Fixture f;
    ItemEntity a = f.makeItem({8.0f, 1.0f, 8.0f}, "stone");
    ItemEntity b = f.makeItem({8.5f, 1.0f, 8.0f}, "dirt");
    EXPECT_FALSE(b.canMergeInto(a));
}

TEST(ItemEntityTest, NoMergeBeyondDistance) {
    Fixture f;
    ItemEntity a = f.makeItem({8.0f, 1.0f, 8.0f});
    ItemEntity b = f.makeItem({9.0f, 1.0f, 8.0f}); // 1.0 apart
    EXPECT_FALSE(b.canMergeInto(a));
}

// -- player <-> item separation (Phase 16) ----------------------------------

TEST(ItemEntityTest, SeparateFromNoOverlapIsNoop) {
    Fixture f;
    ItemEntity item = f.makeItem({8.5f, 1.125f, 8.5f});
    // A player AABB 4 blocks away: no 3D overlap, nothing moves.
    EXPECT_FALSE(item.separateFrom({12.0f, 1.0f, 8.25f},
                                   {12.6f, 2.8f, 8.85f}));
    EXPECT_EQ(item.position(), (Vec3{8.5f, 1.125f, 8.5f}));
}

TEST(ItemEntityTest, SeparateFromTieGoesToXAxis) {
    Fixture f;
    // Item (0.25 cube) at (8.625, 1.125, 8.5): AABB x [8.5, 8.75],
    // y [1.125, 1.375], z [8.375, 8.625]. Player AABB x [8.5, 9.0],
    // y [1.0, 2.8], z [8.3, 8.7]: all three overlaps are 0.25 (tied ->
    // the x axis). The item center (8.625) sits left of the player
    // center (8.75): the push is -x, the item's right face (8.75) onto
    // the player's left face (8.5) -> center 8.375, no residual overlap.
    ItemEntity item = f.makeItem({8.625f, 1.125f, 8.5f});
    EXPECT_TRUE(item.separateFrom({8.5f, 1.0f, 8.3f}, {9.0f, 2.8f, 8.7f}));
    EXPECT_FLOAT_EQ(item.position().x, 8.375f);
    EXPECT_FLOAT_EQ(item.position().y, 1.125f);
    EXPECT_FLOAT_EQ(item.position().z, 8.5f);
}

TEST(ItemEntityTest, SeparateFromPrefersShallowestAxis) {
    Fixture f;
    // Item (0.25 cube) at (9.0625, 1.125, 8.5): AABB x [8.9375, 9.1875]
    // (straddling the player's right face at 9.0), y [1.125, 1.375],
    // z [8.375, 8.625] (both fully inside). Player AABB x [8.4, 9.0],
    // y [1.0, 2.8], z [8.3, 8.7]: overlaps x 0.0625 (shallowest),
    // y 0.25, z 0.25 -> the x axis. The item center (9.0625) sits right
    // of the player center (8.7): the push is +x, the item's left face
    // (8.9375) onto the player's right face (9.0) -> center 9.125.
    ItemEntity item = f.makeItem({9.0625f, 1.125f, 8.5f});
    EXPECT_TRUE(item.separateFrom({8.4f, 1.0f, 8.3f}, {9.0f, 2.8f, 8.7f}));
    EXPECT_FLOAT_EQ(item.position().x, 9.125f);
    EXPECT_FLOAT_EQ(item.position().y, 1.125f);
    EXPECT_FLOAT_EQ(item.position().z, 8.5f);
}

TEST(ItemEntityTest, StackCappedAtMax) {
    Fixture f;
    ItemEntity a = f.makeItem({8.0f, 1.0f, 8.0f});
    for (int i = 0; i < ItemEntity::kMaxStack - 1; ++i) {
        ItemEntity c = f.makeItem({8.1f, 1.0f, 8.0f});
        ASSERT_TRUE(c.canMergeInto(a));
        a.absorb(c);
    }
    EXPECT_EQ(a.count(), ItemEntity::kMaxStack);
    ItemEntity d = f.makeItem({8.2f, 1.0f, 8.0f});
    EXPECT_FALSE(d.canMergeInto(a)); // 64 + 1 overflows the stack
}

TEST(ItemEntityTest, MergeRespectsItemMaxStack) {
    // A tool item with max_stack 1 (the game passes the ItemRegistry value):
    // two tools never merge into a stack of 2.
    Fixture f;
    ItemEntity a = f.makeItem({8.0f, 1.0f, 8.0f}, "pick", 1);
    ItemEntity b = f.makeItem({8.5f, 1.0f, 8.0f}, "pick", 1);
    EXPECT_FALSE(b.canMergeInto(a)); // 1 + 1 > maxStack 1
}

TEST(ItemEntityTest, PickupDelayCountsDown) {
    Fixture f;
    ItemEntity item = f.makeItem({8.5f, 1.0f, 8.5f});
    EXPECT_FALSE(item.pickupReady()); // fresh spawn: the delay is active
    // 0.25 is exact in binary, so the two updates subtract the 0.5 s delay
    // with no floating-point residue (0.5 - 0.25 - 0.25 == 0.0).
    item.update(0.25); // half the delay
    EXPECT_FALSE(item.pickupReady());
    item.update(0.25); // total 0.5 s: the delay is exactly exhausted
    EXPECT_TRUE(item.pickupReady());
    f.run(item, 60);
    EXPECT_TRUE(item.pickupReady()); // stays ready
}

// -- Projectile ----------------------------------------------------------------

TEST(ProjectileTest, LaunchArcsAlongDirectionAndSticksOnFloor) {
    Fixture f;
    Projectile arrow(f.world, f.blocks, {0.5f, 5.0f, 0.5f}, {1.0f, 0.0f, 0.0f});
    f.run(arrow, 30); // 0.5 s: mid-flight
    EXPECT_FALSE(arrow.dead());
    EXPECT_FALSE(arrow.stuck());
    EXPECT_GT(arrow.position().x, 6.0f); // ~0.5 + 15 * 0.5
    EXPECT_NEAR(arrow.position().y, 5.0f - 0.5f * 20.0f * 0.25f, 0.3f);
    f.run(arrow, 90); // 2 s total: long past the floor impact (~0.6 s)
    // Phase 24: a floor impact STICKS the arrow (the Phase 10 "撞墙钉住"
    // intent) — it is not consumed.
    EXPECT_FALSE(arrow.dead());
    EXPECT_TRUE(arrow.stuck());
    EXPECT_FLOAT_EQ(arrow.position().y, 1.0f); // stuck where it hit the floor
    EXPECT_LT(arrow.position().x, 12.0f);      // hit before running on
}

TEST(ProjectileTest, HurtPlayerFlagDefaultsFalseAndSettable) {
    // Phase 27: the player's own bow arrows (the Phase 21 default) never
    // hit the player (no self-damage); mob-shot arrows (the skeleton) are
    // flagged at launch so the game's swept test runs them against the
    // player's AABB.
    Fixture f;
    Projectile bowArrow(f.world, f.blocks, {0.5f, 5.0f, 0.5f},
                        {1.0f, 0.0f, 0.0f});
    EXPECT_FALSE(bowArrow.hurtPlayer());
    Projectile skeletonArrow(f.world, f.blocks, {0.5f, 5.0f, 0.5f},
                             {1.0f, 0.0f, 0.0f}, 15.0f, 2.0f,
                             /*hurtPlayer=*/true);
    EXPECT_TRUE(skeletonArrow.hurtPlayer());
    EXPECT_FLOAT_EQ(skeletonArrow.damage(), 2.0f);
}

TEST(ProjectileTest, SourceDefaultsNullAndSettable) {
    // Phase 27: the swept arrow-vs-mob test skips the source — without it
    // a mob-shot arrow (spawned inside the shooter's own AABB) would hit
    // its shooter on the next frame (t = 0: starts inside the box).
    Fixture f;
    Projectile bowArrow(f.world, f.blocks, {0.5f, 5.0f, 0.5f},
                        {1.0f, 0.0f, 0.0f});
    EXPECT_EQ(bowArrow.source(), nullptr);
    Projectile mobArrow(f.world, f.blocks, {0.5f, 5.0f, 0.5f},
                        {1.0f, 0.0f, 0.0f}, 15.0f, 2.0f,
                        /*hurtPlayer=*/true);
    EXPECT_EQ(mobArrow.source(), nullptr);
    mobArrow.setSource(nullptr);
    EXPECT_EQ(mobArrow.source(), nullptr);
}

TEST(ProjectileTest, SticksOnWallHit) {
    Fixture f;
    f.wallX(12, 0, 20, -16, 16);
    Projectile arrow(f.world, f.blocks, {8.5f, 5.0f, 8.5f}, {1.0f, 0.0f, 0.0f});
    f.run(arrow, 30); // the wall is ~0.23 s away
    EXPECT_FALSE(arrow.dead());
    EXPECT_TRUE(arrow.stuck());
    // Stuck flush against the wall face (x = 12 - size/2).
    EXPECT_FLOAT_EQ(arrow.position().x, 12.0f - Projectile::kSize * 0.5f);
}

TEST(ProjectileTest, StuckArrowKeepsIncomingDirection) {
    // The collision resolver zeroes the blocked axis of the velocity, so
    // the render orientation needs the incoming direction (stuckDir).
    // A HORIZONTAL shot is pulled down by gravity during flight, so the
    // arrow strikes the wall below its launch angle: stuckDir is the
    // pre-impact velocity direction (unit, x-dominant, y < 0), NOT the
    // raw launch direction and NOT the zeroed live velocity.
    Fixture f;
    f.wallX(12, 0, 20, -16, 16);
    Projectile arrow(f.world, f.blocks, {8.5f, 5.0f, 8.5f}, {1.0f, 0.0f, 0.0f});
    f.run(arrow, 30);
    ASSERT_TRUE(arrow.stuck());
    const Vec3 dir = arrow.stuckDir();
    EXPECT_NEAR(length(dir), 1.0f, 1e-5f);
    EXPECT_GT(dir.x, 0.9f);
    EXPECT_LT(dir.y, -0.2f); // gravity bent the arc down over the flight
    EXPECT_NEAR(dir.z, 0.0f, 1e-6f);
    // ... while the live velocity's blocked (x) axis is zeroed.
    EXPECT_FLOAT_EQ(arrow.velocity().x, 0.0f);
}

TEST(ProjectileTest, StuckArrowDoesNotMoveAndAgesOut) {
    // A stuck arrow is static (no physics) but keeps aging. Phase 42: a
    // stuck arrow ages out at kStuckMaxAge (100 s, Minecraft's stuck
    // arrow), NOT the 30 s kMaxAge of flying arrows — it must survive
    // the flight cap and despawn only past the stuck cap.
    Fixture f;
    f.wallX(12, 0, 20, -16, 16);
    Projectile arrow(f.world, f.blocks, {8.5f, 5.0f, 8.5f}, {1.0f, 0.0f, 0.0f});
    f.run(arrow, 30);
    ASSERT_TRUE(arrow.stuck());
    const Vec3 stuckAt = arrow.position();
    const double ageAtStuck = arrow.age();
    f.run(arrow, 120); // 2 s more: a flying arrow would travel ~30 blocks
    EXPECT_EQ(arrow.position(), stuckAt); // but this one did not move
    EXPECT_FALSE(arrow.shouldDespawn());  // 2 s < 30 s
    // Past the FLYING cap (age > kMaxAge): a flying arrow would be gone
    // by now; the stuck one keeps aging.
    f.run(arrow,
          static_cast<int>((Projectile::kMaxAge - ageAtStuck) / kDt) + 120);
    EXPECT_FALSE(arrow.dead());
    EXPECT_FALSE(arrow.shouldDespawn()); // ~kMaxAge + 2 s < kStuckMaxAge
    // ... and despawns once the STUCK cap is reached.
    f.run(arrow,
          static_cast<int>((Projectile::kStuckMaxAge - arrow.age()) / kDt) +
              60);
    EXPECT_FALSE(arrow.dead());            // it aged out, it was not killed
    EXPECT_TRUE(arrow.shouldDespawn());
    EXPECT_GE(arrow.age(), ageAtStuck + Projectile::kStuckMaxAge - 0.1);
}

TEST(ProjectileTest, ReleasedStuckArrowFallsAndRejoinsFlight) {
    // Phase 42: the game releases a stuck arrow when the block it hangs
    // in is broken or replaced (Game::setBlock -> release()): the arrow
    // drops from where it hung with ZERO velocity and rejoins flight
    // physics (gravity + collision), sticking again on the next solid.
    Fixture f;
    f.wallX(12, 0, 20, -16, 16);
    Projectile arrow(f.world, f.blocks, {8.5f, 5.0f, 8.5f}, {1.0f, 0.0f, 0.0f});
    f.run(arrow, 30);
    ASSERT_TRUE(arrow.stuck());
    const Vec3 stuckAt = arrow.position();
    arrow.release(); // what Game::setBlock does for the broken cell
    EXPECT_FALSE(arrow.stuck());
    const Vec3 zero = {0.0f, 0.0f, 0.0f};
    EXPECT_EQ(arrow.velocity(), zero); // it drops
    EXPECT_EQ(arrow.position(), stuckAt); // release does not move it
    f.run(arrow, 30); // 0.5 s of free fall: y drops ~0.5 * 20 * 0.25 = 2.5
    EXPECT_LT(arrow.position().y, stuckAt.y);
    EXPECT_GT(arrow.position().y, 1.0f); // not yet on the floor
    // Flight physics is live again: it sticks on the floor like any
    // other impact (Phase 24).
    f.run(arrow, 120);
    EXPECT_TRUE(arrow.stuck());
    EXPECT_FLOAT_EQ(arrow.position().y, 1.0f);
}

TEST(ProjectileTest, ReleaseIsANoopWhenNotStuck) {
    // Phase 42: release() only acts on stuck arrows (a flying arrow's
    // velocity and motion are untouched).
    Fixture f;
    f.wallX(12, 0, 20, -16, 16);
    Projectile arrow(f.world, f.blocks, {8.5f, 5.0f, 8.5f}, {1.0f, 0.0f, 0.0f});
    f.run(arrow, 5); // mid-flight
    EXPECT_FALSE(arrow.stuck());
    const Vec3 v = arrow.velocity();
    arrow.release();
    EXPECT_EQ(arrow.velocity(), v);
}

TEST(ProjectileTest, PowerAndAmmoItemRideWithTheArrow) {
    // Phase 42: the shot's charge (power — scales the mob-hit knockback)
    // and the launcher's ammo item name (stuck-arrow pickup returns it)
    // ride with the arrow. Defaults: full power, not pickable (the
    // Phase 10 bare projectile).
    Fixture f;
    Projectile plain(f.world, f.blocks, {0.5f, 5.0f, 0.5f}, {1.0f, 0.0f, 0.0f});
    EXPECT_FLOAT_EQ(plain.power(), 1.0f);
    EXPECT_TRUE(plain.ammoItem().empty());
    Projectile shot(f.world, f.blocks, {0.5f, 5.0f, 0.5f}, {1.0f, 0.0f, 0.0f},
                    15.0f * 0.4f, 5.0f * 0.4f, /*hurtPlayer=*/false, 0.4f,
                    "arrow");
    EXPECT_FLOAT_EQ(shot.power(), 0.4f);
    EXPECT_EQ(shot.ammoItem(), "arrow");
}

TEST(ProjectileTest, EffectIdDefaultsNone) {
    // SURV-5: the constructor's effect args default to (-1, 0.0) — every
    // pre-SURV-5 call site builds a no-effect arrow (bit-identical
    // behavior: the game applies nothing on hit).
    Fixture f;
    Projectile plain(f.world, f.blocks, {0.5f, 5.0f, 0.5f}, {1.0f, 0.0f, 0.0f});
    EXPECT_EQ(plain.effectId(), -1);
    EXPECT_DOUBLE_EQ(plain.effectDuration(), 0.0);
    // The full 9-arg legacy shape (power + ammo) also leaves the effect
    // absent (the two new params are trailing defaults).
    Projectile legacy(f.world, f.blocks, {0.5f, 5.0f, 0.5f}, {1.0f, 0.0f, 0.0f},
                      15.0f, 5.0f, /*hurtPlayer=*/false, 0.6f, "arrow");
    EXPECT_EQ(legacy.effectId(), -1);
    EXPECT_DOUBLE_EQ(legacy.effectDuration(), 0.0);
}

TEST(ProjectileTest, EffectIdRidesWithTheArrow) {
    // SURV-5: the resolved effect id + duration ride with the arrow
    // (an int, not a name — zero heap allocation; the id is the
    // EffectRegistry index the game resolved at fire time).
    Fixture f;
    Projectile poisonArrow(f.world, f.blocks, {0.5f, 5.0f, 0.5f},
                           {1.0f, 0.0f, 0.0f}, 15.0f, 5.0f,
                           /*hurtPlayer=*/false, 1.0f, "arrow",
                           /*effectId=*/3, /*effectDuration=*/60.0);
    EXPECT_EQ(poisonArrow.effectId(), 3);
    EXPECT_DOUBLE_EQ(poisonArrow.effectDuration(), 60.0);
    Projectile witherArrow(f.world, f.blocks, {0.5f, 5.0f, 0.5f},
                           {1.0f, 0.0f, 0.0f}, 15.0f, 2.0f,
                           /*hurtPlayer=*/true, 1.0f, "arrow",
                           /*effectId=*/1, /*effectDuration=*/10.0);
    EXPECT_EQ(witherArrow.effectId(), 1);
    EXPECT_DOUBLE_EQ(witherArrow.effectDuration(), 10.0);
}

TEST(ProjectileTest, DespawnBelowKillPlaneNotOnImpact) {
    BlockRegistry blocks = makeBlocks();
    World world; // the void: no solid anywhere
    Projectile arrow(world, blocks, {8.5f, 10.0f, 8.5f}, {0.0f, 0.0f, 1.0f});
    int ticks = 0;
    while (!arrow.shouldDespawn() && ticks < 1000) {
        arrow.update(kDt);
        ++ticks;
    }
    EXPECT_LT(ticks, 1000);
    EXPECT_TRUE(arrow.belowKillPlane());
    EXPECT_FALSE(arrow.dead()); // no solid was ever touched
}

TEST(ProjectileTest, KillConsumesItForDespawn) {
    // Phase 21: a mob hit consumes the arrow (no block impact, no
    // sticking) — the same removal path as an impact. The per-hit damage
    // rides with the projectile (set at launch from the item data).
    BlockRegistry blocks = makeBlocks();
    World world;
    Projectile arrow(world, blocks, {8.5f, 10.0f, 8.5f}, {0.0f, 0.0f, 1.0f},
                     15.0f, 5.0f);
    EXPECT_FLOAT_EQ(arrow.damage(), 5.0f);
    EXPECT_FALSE(arrow.dead());
    EXPECT_FALSE(arrow.shouldDespawn());
    arrow.kill();
    EXPECT_TRUE(arrow.dead());
    EXPECT_TRUE(arrow.shouldDespawn());
}

namespace {

// The Phase 27 formula (arc-only, no leading): the regression anchor the
// Phase 38 solver must reproduce bit-for-close for a stationary target.
Vec3 p27Aim(Vec3 origin, Vec3 target, float speed) {
    Vec3 to = target - origin;
    const float dist = std::sqrt(dot(to, to));
    to.y += 0.5f * Projectile::kGravity * (dist / speed) * (dist / speed);
    return normalized(to);
}

// The closest the arrow's ballistic path ever gets to the target's straight
// line (sampled at 10 Hz over 3 s) — the miss distance the swept hit-test
// has to cover, independent of any flight-time convention.
float aimMiss(Vec3 origin, Vec3 dir, float speed, Vec3 target, Vec3 vel) {
    float best = 1e9f;
    for (int i = 1; i <= 300; ++i) {
        const float t = 0.01f * static_cast<float>(i);
        const Vec3 arrow = origin + dir * (speed * t) -
                           Vec3 {0.0f,
                                 0.5f * Projectile::kGravity * t * t, 0.0f};
        const Vec3 point = target + vel * t;
        best = std::min(best, std::sqrt(dot(arrow - point, arrow - point)));
    }
    return best;
}

} // namespace

TEST(ProjectileTest, AimDirectionStationaryMatchesArcOnly) {
    // Phase 38 regression anchor: a stationary target (zero velocity) must
    // reproduce the Phase 27 arc-only aim — the leading is a no-op, so
    // standing players are hit exactly as before the change.
    const Vec3 origin {0.0f, 5.0f, 0.0f};
    const Vec3 target {10.0f, 5.0f, 0.0f};
    const Vec3 aim = computeAimDirection(origin, target, Vec3::zero(), 15.0f);
    const Vec3 expected = p27Aim(origin, target, 15.0f);
    EXPECT_NEAR(aim.x, expected.x, 1e-6f);
    EXPECT_NEAR(aim.y, expected.y, 1e-6f);
    EXPECT_NEAR(aim.z, expected.z, 1e-6f);
}

TEST(ProjectileTest, AimDirectionLeadsAMovingTarget) {
    const Vec3 origin {0.0f, 5.0f, 0.0f};
    const Vec3 target {10.0f, 5.0f, 0.0f};
    const float speed = 15.0f;

    // Target running away in +x: the lead must aim past the target's
    // current position and land strictly closer to where the target WILL
    // be than the Phase 27 no-lead aim does.
    const Vec3 away {3.0f, 0.0f, 0.0f};
    const Vec3 lead = computeAimDirection(origin, target, away, speed);
    const Vec3 noLead = p27Aim(origin, target, speed);
    EXPECT_GT(aimMiss(origin, lead, speed, target, away), 0.0f);
    EXPECT_LT(aimMiss(origin, lead, speed, target, away),
              aimMiss(origin, noLead, speed, target, away));

    // Target running toward the shooter: shorter flight, flatter aim
    // (more of the unit vector along +x). (No miss-superiority claim here:
    // a target running straight down the arrow's own flight line is
    // already well covered by the no-lead arc — the lead's win is against
    // sideways/away motion, below.)
    const Vec3 toward {-3.0f, 0.0f, 0.0f};
    const Vec3 leadIn = computeAimDirection(origin, target, toward, speed);
    const Vec3 still = computeAimDirection(origin, target, Vec3::zero(),
                                           speed);
    EXPECT_GT(leadIn.x, still.x);

    // Target moving sideways (+z): the aim gains a +z component — it
    // points where the target will be, not where it is — and the closest
    // approach is strictly better than the no-lead arc (the classic
    // "moving targets can dodge" case the lead exists to fix).
    const Vec3 side {0.0f, 0.0f, 4.0f};
    const Vec3 leadSide = computeAimDirection(origin, target, side, speed);
    EXPECT_GT(leadSide.z, 0.0f);
    EXPECT_LT(aimMiss(origin, leadSide, speed, target, side),
              aimMiss(origin, noLead, speed, target, side));
}

// -- dropped-item geometry ------------------------------------------------------

namespace {

ItemMeshGeometry makeGeometry() {
    BlockRegistry reg;
    auto r = reg.registerBlock(parseJson(
        R"({"id":"multi","model":{"textures":{"top":"top","side":"side","bottom":"bottom"}}})"));
    if (!r.ok()) {
        ADD_FAILURE() << r.error().message;
    }
    const std::vector<std::string> names {"bottom", "side", "top"};
    return makeItemCubeGeometry(reg.at(reg.idForName("multi")),
                                makeLayout(names));
}

} // namespace

TEST(ItemMeshTest, CubeIsCenteredAndSizedToItemEntity) {
    const ItemMeshGeometry g = makeGeometry();
    EXPECT_EQ(g.vertices.size(), 24u);
    EXPECT_EQ(g.indices.size(), 36u);
    const float half = ItemEntity::kSize * 0.5f;
    bool plus = false, minus = false;
    for (const MeshVertex& v : g.vertices) {
        EXPECT_LE(std::abs(v.position.x), half);
        EXPECT_LE(std::abs(v.position.y), half);
        EXPECT_LE(std::abs(v.position.z), half);
        // Full sky light baked in (day/night is the shader's job).
        EXPECT_FLOAT_EQ(v.skyLight, 1.0f);
        EXPECT_FLOAT_EQ(v.blockLight, 0.0f);
        if (v.position.x == half) {
            plus = true;
        }
        if (v.position.x == -half) {
            minus = true;
        }
    }
    // Every axis actually spans the full size (corners exist at +/- half).
    EXPECT_TRUE(plus);
    EXPECT_TRUE(minus);
}

TEST(ItemMeshTest, FaceWindingIsCounterClockwiseFromOutside) {
    // Same invariant as the chunk mesher's face table: for every face,
    // cross(corner1 - corner0, corner2 - corner0) must point along the face
    // normal (4 vertices per face, 6 faces).
    const ItemMeshGeometry g = makeGeometry();
    for (std::size_t face = 0; face < 6; ++face) {
        const std::size_t base = face * 4;
        const Vec3 c0 = g.vertices[base].position;
        const Vec3 c1 = g.vertices[base + 1].position;
        const Vec3 c2 = g.vertices[base + 2].position;
        const Vec3 crossDir = cross(c1 - c0, c2 - c0);
        EXPECT_GT(dot(crossDir, g.vertices[base].normal), 0.0f);
    }
}

TEST(ItemMeshTest, UvCornersStayInsideTheFaceTile) {
    // Three distinct tiles so a slot/UV mix-up is detectable.
    const ItemMeshGeometry g = makeGeometry();
    for (std::size_t face = 0; face < 6; ++face) {
        const Vec3 n = g.vertices[face * 4].normal;
        const std::string tile =
            (n == Vec3::unitY())     ? "top"
            : (n == -Vec3::unitY()) ? "bottom"
                                    : "side";
        // The layout is recomputed identically inside makeGeometry.
        const AtlasLayout layout = makeLayout({"bottom", "side", "top"});
        const Vec2 u0 = AtlasLayout::uvMin(layout, tile);
        const Vec2 u1 = AtlasLayout::uvMax(layout, tile);
        for (int i = 0; i < 4; ++i) {
            const Vec2 uv = g.vertices[face * 4 + i].uv;
            EXPECT_GE(uv.x, u0.x - 1e-6f);
            EXPECT_LE(uv.x, u1.x + 1e-6f);
            EXPECT_GE(uv.y, u0.y - 1e-6f);
            EXPECT_LE(uv.y, u1.y + 1e-6f);
        }
    }
}

TEST(ItemMeshTest, FenceAndPaneDropsUseTheSideTextureOnEveryFace) {
    // Phase 98/99: fence / glass pane blocks carry exactly ONE texture
    // ("side"; top/bottom are rejected at parse). The drop-icon cube must
    // paint every face with the side tile — cubeFaceTexture would return
    // the empty top name and the atlas lookup would throw (the Phase 65
    // cross-particle crash, thin-shape edition). Two distinct rows keep a
    // top/side mix-up detectable.
    for (const char* shape : {"fence", "glass_pane"}) {
        BlockRegistry reg;
        // The glass pane also enforces its light properties (Phase 99:
        // forced non-opaque / transparent); the fence defaults are fine.
        std::string json = std::string(R"({"id":")") + shape +
                           R"(","model":{"shape":")" + shape +
                           R"(","textures":{"side":"side"}})";
        if (std::string(shape) == "glass_pane") {
            json += R"(,"properties":{"opaque":false,"transparent":true})";
        }
        json += "}";
        auto r = reg.registerBlock(parseJson(json));
        ASSERT_TRUE(r.ok()) << r.error().message;
        const AtlasLayout layout = makeLayout({"top", "side"});
        ItemMeshGeometry g;
        ASSERT_NO_THROW(g = makeItemCubeGeometry(
            reg.at(reg.idForName(shape)), layout));
        EXPECT_EQ(g.vertices.size(), 24u);
        const Vec2 s0 = AtlasLayout::uvMin(layout, "side");
        const Vec2 s1 = AtlasLayout::uvMax(layout, "side");
        for (const MeshVertex& v : g.vertices) {
            EXPECT_GE(v.uv.x, s0.x - 1e-6f);
            EXPECT_LE(v.uv.x, s1.x + 1e-6f);
            EXPECT_GE(v.uv.y, s0.y - 1e-6f);
            EXPECT_LE(v.uv.y, s1.y + 1e-6f);
        }
    }
}

// -- entity-vs-entity separation (Phase 19) ----------------------------------

namespace {

// A non-pushable stub body: the "future mob" placeholder for the immovable
// case beyond the player.
class StubBody : public Entity {
public:
    StubBody(const World& world, const BlockRegistry& blocks, Vec3 pos)
        : Entity(world, blocks, pos, 0.6f, 1.8f) {}
    void update(double) override {}
    void integrate(float) override {}
    bool pushableByEntities() const override { return false; }
};

// Strict 3D AABB overlap (the separation convention: touching faces are
// not an overlap).
bool aabbOverlap(const Vec3& a0, const Vec3& a1, const Vec3& b0,
                 const Vec3& b1) {
    return std::min(a1.x, b1.x) > std::max(a0.x, b0.x) &&
           std::min(a1.y, b1.y) > std::max(a0.y, b0.y) &&
           std::min(a1.z, b1.z) > std::max(a0.z, b0.z);
}

// The bottleneck per-axis overlap extent: 3D overlap requires ALL axes to
// overlap, so this is > 0 iff the AABBs overlap (and the smallest positive
// value is how far apart the pair is on the separating axis).
float minAabbExtent(const Vec3& a0, const Vec3& a1, const Vec3& b0,
                    const Vec3& b1) {
    const float ox = std::min(a1.x, b1.x) - std::max(a0.x, b0.x);
    const float oy = std::min(a1.y, b1.y) - std::max(a0.y, b0.y);
    const float oz = std::min(a1.z, b1.z) - std::max(a0.z, b0.z);
    return std::min({ox, oy, oz});
}

Vec3 aabbMinOf(const Vec3& bottomCenter, float width) {
    return {bottomCenter.x - width * 0.5f, bottomCenter.y,
            bottomCenter.z - width * 0.5f};
}

Vec3 aabbMaxOf(const Vec3& bottomCenter, float width, float height) {
    return {bottomCenter.x + width * 0.5f, bottomCenter.y + height,
            bottomCenter.z + width * 0.5f};
}

} // namespace

TEST(SeparationTest, PairSplitsTwoPushableBodiesHalfEach) {
    Fixture f;
    ItemEntity a = f.makeItem({8.5f, 1.0f, 8.5f}, "stone");
    ItemEntity b = f.makeItem({8.625f, 1.0f, 8.5f}, "dirt");
    // a [8.375, 8.625], b [8.5, 8.75]: the x overlap (0.125) is shallower
    // than y/z (0.25). a is left of b: each moves half the face span
    // (0.0625) apart, landing face-to-face at 8.5625.
    EXPECT_TRUE(separateEntityPair(a, b, true));
    EXPECT_FLOAT_EQ(a.position().x, 8.5f - 0.0625f);
    EXPECT_FLOAT_EQ(b.position().x, 8.625f + 0.0625f);
    EXPECT_FLOAT_EQ(a.position().y, 1.0f);
    EXPECT_FLOAT_EQ(b.position().y, 1.0f);
    EXPECT_EQ(b.position().z, 8.5f);
    // Touching, not overlapping.
    EXPECT_LE(a.aabbMax().x, b.aabbMin().x);
    // The push does not touch the velocity.
    EXPECT_EQ(a.velocity().x, 0.0f);
    EXPECT_EQ(b.velocity().x, 0.0f);
}

TEST(SeparationTest, PairKeepsPlayerImmovableAndPushesItemOut) {
    Fixture f;
    Player player(f.world, f.blocks, {8.75f, 1.0f, 8.5f});
    ItemEntity item = f.makeItem({8.625f, 1.125f, 8.5f});
    // All three overlaps are 0.25 (tied -> x). The item center (8.625)
    // sits left of the player center (8.75): the item takes the full face
    // span (the player is immovable, the Phase 16 rule) and lands flush on
    // the player's left face.
    const Vec3 playerBefore = player.position();
    const Vec3 itemBefore = item.position();
    EXPECT_TRUE(separateEntityPair(player, item, true));
    EXPECT_EQ(player.position(), playerBefore); // immovable
    EXPECT_FLOAT_EQ(item.position().y, itemBefore.y);
    EXPECT_FLOAT_EQ(item.position().z, itemBefore.z);
    EXPECT_LT(item.position().x, itemBefore.x); // pushed away (-x)
    EXPECT_FLOAT_EQ(item.position().x,
                    8.625f - (8.75f - player.aabbMin().x));
    // No residual overlap.
    EXPECT_FALSE(aabbOverlap(player.aabbMin(), player.aabbMax(),
                             item.aabbMin(), item.aabbMax()));
}

TEST(SeparationTest, PairTieLaterBodyGoesNegative) {
    Fixture f;
    ItemEntity a = f.makeItem({8.5f, 1.0f, 8.5f}, "stone");
    ItemEntity b = f.makeItem({8.5f, 1.0f, 8.5f}, "dirt");
    // Exact center tie on all axes (tied extents -> x): both bodies are
    // pushable, so the LATER body (b) goes negative and the earlier one
    // (a) positive; each moves half the full overlap (0.125).
    EXPECT_TRUE(separateEntityPair(a, b, true));
    EXPECT_FLOAT_EQ(a.position().x, 8.5f + 0.125f);
    EXPECT_FLOAT_EQ(b.position().x, 8.5f - 0.125f);
}

TEST(SeparationTest, PairTiePlayerItemKeepsPhase16Direction) {
    Fixture f;
    Player player(f.world, f.blocks, {8.75f, 1.0f, 8.5f});
    ItemEntity item = f.makeItem({8.75f, 1.125f, 8.5f});
    // Exact center tie on x (the shallowest axis, tied extents): the
    // player is immovable, so the pushable item is pushed to the -axis
    // side by the full face span — the Phase 16 direction (an item
    // dropped exactly at the player's feet goes to the player's left).
    EXPECT_TRUE(separateEntityPair(player, item, true));
    EXPECT_EQ(player.position(), (Vec3{8.75f, 1.0f, 8.5f})); // immovable
    // The item's far face lands flush on the player's -x face.
    EXPECT_FLOAT_EQ(item.aabbMax().x, player.aabbMin().x);
    EXPECT_FLOAT_EQ(item.position().x,
                    8.75f - (player.aabbMax().x - 8.625f));
    EXPECT_FALSE(aabbOverlap(player.aabbMin(), player.aabbMax(),
                             item.aabbMin(), item.aabbMax()));
}

TEST(SeparationTest, PairMinimumAxisYResolvesVertically) {
    Fixture f;
    ItemEntity a = f.makeItem({8.5f, 1.0f, 8.5f});
    ItemEntity b = f.makeItem({8.5f, 1.2f, 8.5f});
    // y overlap 0.05 < x/z 0.25 -> the y axis. a (below) would be pushed
    // into the stone floor, so the world-aware fallback hands the full
    // separation to b: a stays on the floor, b lands on top of a.
    EXPECT_TRUE(separateEntityPair(a, b, true));
    EXPECT_EQ(a.position(), (Vec3{8.5f, 1.0f, 8.5f}));
    EXPECT_FLOAT_EQ(b.position().y, 1.2f + (1.25f - 1.2f));
    EXPECT_LE(b.aabbMin().y, a.aabbMax().y);
}

TEST(SeparationTest, PairTouchingFacesDoNotSeparate) {
    Fixture f;
    ItemEntity a = f.makeItem({8.5f, 1.0f, 8.5f}, "stone");
    ItemEntity b = f.makeItem({8.75f, 1.0f, 8.5f}, "dirt");
    // a [8.375, 8.625], b [8.625, 8.875]: touching at 8.625 (the strict
    // overlap test) -> nothing moves.
    EXPECT_FALSE(separateEntityPair(a, b, true));
    EXPECT_EQ(a.position(), (Vec3{8.5f, 1.0f, 8.5f}));
    EXPECT_EQ(b.position(), (Vec3{8.75f, 1.0f, 8.5f}));
}

TEST(SeparationTest, PairBothImmovableBodiesStayPut) {
    Fixture f;
    StubBody a(f.world, f.blocks, {8.5f, 1.0f, 8.5f});
    StubBody b(f.world, f.blocks, {8.6f, 1.0f, 8.5f});
    EXPECT_FALSE(separateEntityPair(a, b, true));
    EXPECT_EQ(a.position(), (Vec3{8.5f, 1.0f, 8.5f}));
    EXPECT_EQ(b.position(), (Vec3{8.6f, 1.0f, 8.5f}));
}

TEST(SeparationTest, ListQDropClusterConvergesDeterministically) {
    // The common Q-drop case: two different items spawned at the player's
    // feet (same position, fully overlapping). The player pairs are
    // resolved first (the player is immovable, the items' centers tie ->
    // -x, the Phase 16 direction), the item pair then splits them; the
    // sweeps converge to the items stacked face-to-face flush against the
    // player's LEFT face (player x [8.45, 9.05]: i0 flush at 8.325,
    // i1 at 8.075) within the sub-visual convergence threshold.
    auto runOnce = []() {
        Fixture f;
        Player player(f.world, f.blocks, {8.75f, 1.0f, 8.5f});
        ItemEntity i0 = f.makeItem({8.75f, 1.0f, 8.5f}, "stone");
        ItemEntity i1 = f.makeItem({8.75f, 1.0f, 8.5f}, "dirt");
        std::vector<Entity*> bodies {&player, &i0, &i1};
        separateEntityList(bodies);
        return std::make_tuple(player.position(), i0.position(),
                               i1.position());
    };
    const auto [p1, a1, b1] = runOnce();
    const auto [p2, a2, b2] = runOnce();
    // Deterministic: two fresh runs land on the exact same floats.
    EXPECT_EQ(p1, p2);
    EXPECT_EQ(a1, a2);
    EXPECT_EQ(b1, b2);
    // The player never moved; the items ended up on its -x side, ordered
    // i0 (the earlier body) between the player and i1.
    EXPECT_EQ(p1, (Vec3{8.75f, 1.0f, 8.5f}));
    EXPECT_LT(a1.x, p1.x);
    EXPECT_GT(a1.x, b1.x);
    // Converged: i0 against the player's left face, i1 against i0, and
    // every pair within the sub-visual convergence threshold (the
    // competing constraints halve the residual per sweep; the pass stops
    // at kSeparationConvergence).
    EXPECT_NEAR(a1.x, 8.45f - 0.125f, 1e-4f);
    EXPECT_NEAR(b1.x, 8.2f - 0.125f, 1e-4f);
    const Vec3 pMin = aabbMinOf(p1, 0.6f);
    const Vec3 pMax = aabbMaxOf(p1, 0.6f, 1.8f);
    const Vec3 aMin = aabbMinOf(a1, 0.25f);
    const Vec3 aMax = aabbMaxOf(a1, 0.25f, 0.25f);
    const Vec3 bMin = aabbMinOf(b1, 0.25f);
    const Vec3 bMax = aabbMaxOf(b1, 0.25f, 0.25f);
    EXPECT_LE(minAabbExtent(pMin, pMax, aMin, aMax), 1e-5f);
    EXPECT_LE(minAabbExtent(pMin, pMax, bMin, bMax), 1e-5f);
    EXPECT_LE(minAabbExtent(aMin, aMax, bMin, bMax), 1e-5f);
    // (Re-)running the pass on the settled cluster moves nothing up to the
    // convergence threshold.
    Fixture f;
    Player player2(f.world, f.blocks, p1);
    ItemEntity i0 = f.makeItem(a1, "stone");
    ItemEntity i1 = f.makeItem(b1, "dirt");
    std::vector<Entity*> bodies {&player2, &i0, &i1};
    separateEntityList(bodies);
    EXPECT_NEAR(i0.position().x, a1.x, 1e-5f);
    EXPECT_NEAR(i1.position().x, b1.x, 1e-5f);
}

TEST(SeparationTest, ListSkipsNonCollidingBodies) {
    Fixture f;
    ItemEntity item = f.makeItem({8.5f, 1.0f, 8.5f});
    Projectile arrow(f.world, f.blocks, {8.5f, 1.0f, 8.5f},
                     {0.0f, 1.0f, 0.0f});
    // The projectile flies through the item (Minecraft arrows): no
    // separation, no moved sweeps.
    EXPECT_FALSE(arrow.collidesWithEntities());
    std::vector<Entity*> bodies {&item, &arrow};
    EXPECT_EQ(separateEntityList(bodies), 0);
    EXPECT_EQ(item.position(), (Vec3{8.5f, 1.0f, 8.5f}));
}

TEST(SeparationTest, ParticipationFlags) {
    Fixture f;
    Player player(f.world, f.blocks, {8.5f, 1.0f, 8.5f});
    ItemEntity item = f.makeItem({8.5f, 1.0f, 8.5f});
    Projectile arrow(f.world, f.blocks, {8.5f, 1.0f, 8.5f},
                     {1.0f, 0.0f, 0.0f});
    EXPECT_TRUE(player.collidesWithEntities());
    EXPECT_FALSE(player.pushableByEntities()); // immovable (Phase 16 rule)
    EXPECT_TRUE(item.collidesWithEntities());
    EXPECT_TRUE(item.pushableByEntities());
    EXPECT_FALSE(arrow.collidesWithEntities()); // arrows pass through
    EXPECT_TRUE(arrow.pushableByEntities());
}

// -- fluid overlap (Phase 58: shared by the player's swim/lava burn and
//     the mob fire system) ----------------------------------------------------

TEST(EntityTest, OverlapsFluidStrictly) {
    Fixture f;
    ItemEntity e = f.makeItem({5.5f, 1.0f, 5.5f}); // AABB [5.375,5.625]x[1,1.25]
    EXPECT_FALSE(e.overlapsFluid(FluidKind::Water));
    EXPECT_FALSE(e.overlapsFluid(FluidKind::Lava));
    // A fluid cell BELOW the item (the face rests exactly on the boundary).
    f.world.setFluid(5, 0, 5, FluidKind::Water, kMaxFluidLevel);
    EXPECT_FALSE(e.overlapsFluid(FluidKind::Water));
    // A fluid cell at the item's own level: strict overlap.
    f.world.setFluid(5, 1, 5, FluidKind::Water, kMaxFluidLevel);
    EXPECT_TRUE(e.overlapsFluid(FluidKind::Water));
    EXPECT_FALSE(e.overlapsFluid(FluidKind::Lava)); // kind-specific
    // setFluid replaces the other kind: now lava, no water.
    f.world.setFluid(5, 1, 5, FluidKind::Lava, kMaxFluidLevel);
    EXPECT_TRUE(e.overlapsFluid(FluidKind::Lava));
    EXPECT_FALSE(e.overlapsFluid(FluidKind::Water));
}

TEST(EntityTest, OverlapsFluidStrictOnCellFaces) {
    Fixture f;
    // x-max exactly on the x=6 boundary: cell x=6 is NOT overlapped (the
    // strict convention), cell x=5 is.
    ItemEntity e = f.makeItem({5.875f, 1.0f, 5.5f}); // AABB x [5.75, 6.0]
    f.world.setFluid(6, 1, 5, FluidKind::Lava, kMaxFluidLevel);
    EXPECT_FALSE(e.overlapsFluid(FluidKind::Lava));
    f.world.setFluid(5, 1, 5, FluidKind::Lava, kMaxFluidLevel);
    EXPECT_TRUE(e.overlapsFluid(FluidKind::Lava));
}

// -- Phase 85: the slab (bottom half-block) height-aware collision ---------

TEST(EntityTest, Phase85ItemRestsOnSlabTop) {
    Fixture f;
    const BlockID slab = f.blocks.idForName("slab");
    // A slab at y=2 (solid region 2..2.5) above the fixture's stone floor
    // (top y=1, with air at y=1 between them). An item dropped above the
    // slab rests on the SLAB TOP (feet at y=2.5) — not the cell top (3.0,
    // which would be the full-block collision) and not inside the slab.
    f.world.setBlock(8, 2, 8, slab);
    ItemEntity item = f.makeItem({8.5f, 4.0f, 8.5f});
    f.run(item, 300);
    EXPECT_FLOAT_EQ(item.position().y, 2.5f);
}

TEST(EntityTest, Phase87StairSlopeRestsByZ) {
    Fixture f;
    const BlockID stairs = f.blocks.idForName("stairs");
    f.world.setBlock(8, 2, 8, stairs); // north-facing: -Z high, +Z low
    // An item dropped at the BACK (-Z, z~8.1) settles HIGH on the slope; one
    // at the FRONT (z~8.6) settles LOWER (the solid height varies with z).
    ItemEntity back = f.makeItem({8.5f, 4.0f, 8.1f});
    f.run(back, 300);
    ItemEntity front = f.makeItem({8.5f, 4.0f, 8.6f});
    f.run(front, 300);
    EXPECT_GT(back.position().y, front.position().y)
        << "the back of the stair is higher than the front (the slope)";
    // Phase 89: the blocking height is the HIGHEST slope point under the
    // AABB (the high-end edge), so the back item (AABB touching the -Z high
    // end) rests at ~3.0 and the front item (AABB at localZ ~0.5) at ~2.5.
    EXPECT_GE(back.position().y, 2.9f); // near the full-height (-Z) end
    EXPECT_LE(front.position().y, 2.6f); // lower, toward the +Z end
}

TEST(EntityTest, Phase89SouthStairSlopeIsOpposite) {
    Fixture f;
    const BlockID south = f.blocks.idForName("stairs_south");
    f.world.setBlock(8, 2, 8, south); // south: full at the +Z end, low at -Z
    // The OPPOSITE of the north stair: the +Z end is now HIGH, the -Z end
    // LOW. An item at the +Z end (z~8.9) settles higher than one at the -Z
    // end (z~8.1).
    ItemEntity plusZ = f.makeItem({8.5f, 4.0f, 8.9f});
    f.run(plusZ, 300);
    ItemEntity minusZ = f.makeItem({8.5f, 4.0f, 8.1f});
    f.run(minusZ, 300);
    EXPECT_GT(plusZ.position().y, minusZ.position().y)
        << "a south stair is high at +Z (opposite the north stair)";
}

// Phase 94: the PER-INSTANCE facing (the World's per-cell byte — the
// project's first per-block state) OVERRIDES the block's stair_facing
// (the default). A north stair (stair_facing 0) with the per-instance facing
// set to south (2) behaves like a south stair (high at +Z).
TEST(EntityTest, Phase94PerInstanceFacingOverridesBlockDefault) {
    Fixture f;
    const BlockID north = f.blocks.idForName("stairs"); // stair_facing 0
    f.world.setBlock(8, 2, 8, north);
    // Set the per-instance state (facing) to SOUTH (2) — overriding the
    // block's north default. Now the +Z end is HIGH (south), not the -Z
    // end.
    f.world.setBlockState(8, 2, 8, 2);
    // An item at the +Z end (z~8.9) settles HIGHER than one at the -Z end
    // (z~8.1) — the OPPOSITE of a north stair (the per-instance south facing
    // overrides the block's north default).
    ItemEntity plusZ = f.makeItem({8.5f, 4.0f, 8.9f});
    f.run(plusZ, 300);
    ItemEntity minusZ = f.makeItem({8.5f, 4.0f, 8.1f});
    f.run(minusZ, 300);
    EXPECT_GT(plusZ.position().y, minusZ.position().y)
        << "the per-instance south facing overrides the block's north "
           "default (high at +Z, opposite a north stair)";
}

// Phase 94: when the per-instance facing is UNSET (the sentinel), the
// collision falls back to the block's stair_facing (the default) — the
// pre-Phase-94 behavior (the unit tests without a game are unchanged).
TEST(EntityTest, Phase94UnsetFacingFallsBackToBlockDefault) {
    Fixture f;
    const BlockID north = f.blocks.idForName("stairs"); // stair_facing 0
    f.world.setBlock(8, 2, 8, north);
    // The per-instance facing is UNSET (the sentinel) — the collision falls
    // back to the block's north default. The -Z end is HIGH (north).
    ItemEntity minusZ = f.makeItem({8.5f, 4.0f, 8.1f});
    f.run(minusZ, 300);
    ItemEntity plusZ = f.makeItem({8.5f, 4.0f, 8.6f});
    f.run(plusZ, 300);
    EXPECT_GT(minusZ.position().y, plusZ.position().y)
        << "an unset per-instance facing falls back to the block's north "
           "default (high at -Z)";
}

TEST(EntityTest, Phase85SlabHasRoom) {
    Fixture f;
    const BlockID slab = f.blocks.idForName("slab");
    f.world.setBlock(8, 2, 8, slab); // solid region y 2..2.5
    f.world.setBlock(9, 2, 8, f.stone); // solid region y 2..3
    const ItemEntity item = f.makeItem({8.5f, 10.0f, 8.5f});
    // On the slab's TOP (AABB bottom at y=2.5 = the solid top): the AABB
    // [2.5, 2.5+size] does not overlap the solid region [2, 2.5] -> has room
    // (a face resting exactly on the slab top is not an overlap).
    EXPECT_TRUE(item.hasRoomAt(Vec3{8.5f, 2.5f, 8.5f}));
    // INSIDE the slab's solid region (AABB bottom at y=2.0): overlap -> no
    // room (you cannot occupy the slab's body).
    EXPECT_FALSE(item.hasRoomAt(Vec3{8.5f, 2.0f, 8.5f}));
    // The full stone block: the solid region is the whole cell [2, 3].
    EXPECT_TRUE(item.hasRoomAt(Vec3{9.5f, 3.0f, 8.5f})); // on its top
    EXPECT_FALSE(item.hasRoomAt(Vec3{9.5f, 2.5f, 8.5f})); // inside it
}

// Phase 95: the wall is a 1.5-block-tall barrier. The solid region is
// [y, y+1.5] (the cell + a half cap above). An item can rest on the wall's
// top (AABB bottom at y+1.5) but NOT occupy the wall's body (AABB bottom in
// [y, y+1.5]) — a barrier the player cannot pass through nor jump over.
TEST(EntityTest, Phase95WallIsABarrier) {
    Fixture f;
    const BlockID wall = f.blocks.idForName("wall");
    f.world.setBlock(8, 2, 8, wall); // solid region y 2..3.5
    const ItemEntity item = f.makeItem({8.5f, 10.0f, 8.5f});
    // On the wall's TOP (AABB bottom at y=3.5 = the solid top): the AABB
    // [3.5, 3.5+size] does not overlap the solid region [2, 3.5] -> has room
    // (a face resting exactly on the wall top is not an overlap).
    EXPECT_TRUE(item.hasRoomAt(Vec3{8.5f, 3.5f, 8.5f}));
    // INSIDE the wall's body (AABB bottom at y=3.0, in [2, 3.5]): overlap ->
    // no room (you cannot occupy the wall's body — the barrier).
    EXPECT_FALSE(item.hasRoomAt(Vec3{8.5f, 3.0f, 8.5f}));
    // In the CAP (AABB bottom at y=3.4, in [2, 3.5] but above the cell top
    // y=3): still inside the wall's solid region -> no room (the cap is solid).
    EXPECT_FALSE(item.hasRoomAt(Vec3{8.5f, 3.4f, 8.5f}));
}

// Phase 99: the glass pane is a JUMPABLE solid (y 0..1, the full cell — the
// 1/16 sheet is mesh-only). Unlike the wall/fence 1.5 barrier, the player
// can jump over it (the jump is ~1.125 > 1.0): the solid region tops out at
// exactly 1.0, so an item resting on top sits at y+1.0 (not y+1.5).
TEST(EntityTest, Phase99GlassPaneIsJumpableSolid) {
    Fixture f;
    const BlockID pane = f.blocks.idForName("glass_pane");
    f.world.setBlock(8, 2, 8, pane); // solid region y 2..3
    const ItemEntity item = f.makeItem({8.5f, 10.0f, 8.5f});
    // On the pane's TOP (AABB bottom at y=3.0): no overlap -> has room.
    EXPECT_TRUE(item.hasRoomAt(Vec3{8.5f, 3.0f, 8.5f}));
    // INSIDE the pane's body (AABB bottom at y=2.9, in [2, 3]): no room.
    EXPECT_FALSE(item.hasRoomAt(Vec3{8.5f, 2.9f, 8.5f}));
    // Halfway up (y=2.5): no room (the sheet is mesh-only, the collision is
    // the full cell). The resting point is 3.0 — 0.5 LOWER than the
    // wall/fence's 3.5: the jumpable height (the jump ~1.125 clears 1.0).
    EXPECT_FALSE(item.hasRoomAt(Vec3{8.5f, 2.5f, 8.5f}));
}

// Phase 98: the fence is a barrier with the SAME solid region as the wall
// (y 0..1.5) — the thin crossed posts are mesh-only; the collision does not
// care (Minecraft's fence blocks exactly like the wall).
TEST(EntityTest, Phase98FenceIsABarrier) {
    Fixture f;
    const BlockID fence = f.blocks.idForName("fence");
    f.world.setBlock(8, 2, 8, fence); // solid region y 2..3.5
    const ItemEntity item = f.makeItem({8.5f, 10.0f, 8.5f});
    // On the fence's TOP (AABB bottom at y=3.5): no overlap -> has room.
    EXPECT_TRUE(item.hasRoomAt(Vec3{8.5f, 3.5f, 8.5f}));
    // INSIDE the fence body (AABB bottom at y=3.0): no room (the barrier).
    EXPECT_FALSE(item.hasRoomAt(Vec3{8.5f, 3.0f, 8.5f}));
    // In the CAP (AABB bottom at y=3.4, above the cell top y=3): no room.
    EXPECT_FALSE(item.hasRoomAt(Vec3{8.5f, 3.4f, 8.5f}));
}

// -- ENT-4: the item fluid buoyancy (the symmetric float) -------------------
//
// The same Entity::refreshFluidState / integrateFluid the mobs use (the
// input-free twin of the player's FLUID-2 swim): an item in water OR lava
// RISES (the buoyancy 12, capped 1.5) and comes to rest AT W' — the
// stationary float (the friction zero's the horizontal residual — no
// surface drift, the documented MC simplification); an item with entry
// momentum (the vy >= 0 rise gate excluded) SINKS to the pool floor and
// STANDS (the fresh resting gate). The on-land trajectory is bit-identical
// to the pre-ENT-4 model (FallsAndRestsExactlyOnFloor + the rest of this
// file pin it).
//
// Pool conventions: the fixture floor tops out at y=1; an n-cell L8 pool
// (y=1..n) has W' = 1+n.

namespace {

// An n-cell full (L8) pool on the fixture floor. `kind` defaults to water;
// the symmetric lava float (the documented ENT-4 extension) is a test.
void buildItemPool(Fixture& f, int x0, int x1, int z0, int z1, int n,
                   FluidKind kind = FluidKind::Water) {
    for (int x = x0; x <= x1; ++x) {
        for (int z = z0; z <= z1; ++z) {
            for (int y = 1; y <= n; ++y) {
                f.world.setFluid(x, y, z, kind, kMaxFluidLevel);
            }
        }
    }
}

} // namespace

TEST(ItemEntityTest, FloatsAtWaterSurface) {
    // A submerged item (vy 0) RISES and FLOATS at W' = 3 (the 2-cell
    // pool) — the geometric terminal (exact, fps-independent; the item's
    // real-dt step runs on the fixed 1/60 in the test, the game's dt
    // history is irrelevant to the terminal — it is a fixed point). The
    // float is STATIONARY: the residual horizontal velocity decays to
    // zero (the friction, in fluid too — no surface drift).
    Fixture f;
    buildItemPool(f, 6, 10, 6, 10, 2); // W' = 3
    ItemEntity item = f.makeItem({8.5f, 2.0f, 8.5f});
    f.run(item, 300); // 5 s: the rise (~1.2 s) + the settle
    EXPECT_FLOAT_EQ(item.position().y, 3.0f);
    EXPECT_EQ(item.velocity().x, 0.0f);
    EXPECT_EQ(item.velocity().y, 0.0f);
    EXPECT_EQ(item.velocity().z, 0.0f);
    EXPECT_FALSE(item.onGround());
    // The fixed point: more frames keep it.
    f.run(item, 120);
    EXPECT_FLOAT_EQ(item.position().y, 3.0f);
    EXPECT_EQ(item.velocity().y, 0.0f);

    // A sliding item (an entry horizontal velocity) still floats — and
    // comes to REST at the surface (no drift, the documented
    // simplification): x stops where the friction zero's it, y = W'.
    ItemEntity sliding = f.makeItem({7.5f, 2.0f, 7.5f});
    sliding.setVelocity({1.5f, 0.0f, 0.0f});
    f.run(sliding, 300);
    EXPECT_FLOAT_EQ(sliding.position().y, 3.0f);
    EXPECT_EQ(sliding.velocity().x, 0.0f);
    EXPECT_EQ(sliding.velocity().y, 0.0f);
    // The slide is bounded (the total drift 1.5/2.5 = 0.6 blocks from the
    // preset — it never leaves the pool column x 6..10).
    EXPECT_GT(sliding.position().x, 7.5f);
    EXPECT_LT(sliding.position().x, 8.5f);
}

TEST(ItemEntityTest, SinksToPoolFloorWithEntryMomentum) {
    // A falling item (the entry momentum, the vy >= 0 rise gate excluded)
    // sinks at the water terminal to the pool floor and STANDS there (the
    // fresh resting gate — the player's "stand on the bottom").
    Fixture f;
    buildItemPool(f, 0, 12, 4, 10, 2); // W' = 3, the floor y=1
    ItemEntity item = f.makeItem({8.5f, 6.0f, 8.5f});
    item.setVelocity({0.0f, -30.0f, 0.0f}); // a fast entry
    f.run(item, 600); // 10 s: the fall + the sink + the settle
    EXPECT_FLOAT_EQ(item.position().y, 1.0f); // the pool floor
    EXPECT_TRUE(item.onGround());
    EXPECT_EQ(item.velocity().y, 0.0f);
    // It sank, it did not rise (mutually exclusive with the terminal of
    // FloatsAtWaterSurface on the same pool geometry).
    EXPECT_NE(item.position().y, 3.0f);
}

TEST(ItemEntityTest, FloatsInLavaSymmetric) {
    // The symmetric lava float (the documented ENT-4 extension: the
    // player's lava has the weaker pace, items — like mobs — take the
    // water model in both kinds). A 2-cell lava pool: W' = 3.
    Fixture f;
    buildItemPool(f, 6, 10, 6, 10, 2, FluidKind::Lava);
    ItemEntity item = f.makeItem({8.5f, 2.0f, 8.5f});
    f.run(item, 300);
    EXPECT_FLOAT_EQ(item.position().y, 3.0f);
    EXPECT_EQ(item.velocity().y, 0.0f);
    EXPECT_FALSE(item.onGround());
}

TEST(ItemEntityTest, OnLandIsFluidInert) {
    // The on-land regression anchor: a DRY run never enters the fluid
    // model — the state stays inert (inFluid false) and the fall is the
    // plain gravity + friction model (bit-identical to the pre-ENT-4
    // code — FallsAndRestsExactlyOnFloor pins the terminal).
    Fixture f;
    ItemEntity item = f.makeItem({8.5f, 10.0f, 8.5f});
    for (int i = 0; i < 90; ++i) { // the 9-block fall lands at ~frame 45
        item.update(kDt);
        EXPECT_FALSE(item.inFluid()) << "frame " << i;
    }
    EXPECT_TRUE(item.onGround());
    EXPECT_FLOAT_EQ(item.position().y, 1.0f);
    EXPECT_EQ(item.velocity().y, 0.0f);
}
