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
                        int maxStack = 64) const {
        return ItemEntity(world, blocks, item, maxStack, pos);
    }

    void run(Entity& e, int n, double dt = kDt) {
        for (int i = 0; i < n; ++i) {
            e.update(dt);
        }
    }
};

AtlasLayout makeLayout(const std::vector<std::string>& names) {
    auto r = AtlasLayout::compute(names);
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
    ItemEntity item(world, blocks, "stone", 64, {8.5f, 10.0f, 8.5f});
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

// NOTE: the 30 s lifetime shares the age path verified by
// ItemEntityTest.DespawnAfterAge; it cannot be exercised in isolation
// because a live projectile is always either in flight (gravity eventually
// puts it into a solid or the kill plane) or already dead.

TEST(ProjectileTest, LaunchArcsAlongDirectionAndDiesOnFloor) {
    Fixture f;
    Projectile arrow(f.world, f.blocks, {0.5f, 5.0f, 0.5f}, {1.0f, 0.0f, 0.0f});
    f.run(arrow, 30); // 0.5 s: mid-flight
    EXPECT_FALSE(arrow.dead());
    EXPECT_GT(arrow.position().x, 6.0f); // ~0.5 + 15 * 0.5
    EXPECT_NEAR(arrow.position().y, 5.0f - 0.5f * 20.0f * 0.25f, 0.3f);
    f.run(arrow, 90); // 2 s total: long past the floor impact (~0.6 s)
    EXPECT_TRUE(arrow.dead());
    EXPECT_FLOAT_EQ(arrow.position().y, 1.0f); // stuck where it hit the floor
    EXPECT_LT(arrow.position().x, 12.0f);      // hit before running on
}

TEST(ProjectileTest, DespawnOnWallHit) {
    Fixture f;
    f.wallX(12, 0, 20, -16, 16);
    Projectile arrow(f.world, f.blocks, {8.5f, 5.0f, 8.5f}, {1.0f, 0.0f, 0.0f});
    f.run(arrow, 30); // the wall is ~0.23 s away
    EXPECT_TRUE(arrow.dead());
    // Stuck flush against the wall face (x = 12 - size/2).
    EXPECT_FLOAT_EQ(arrow.position().x, 12.0f - Projectile::kSize * 0.5f);
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
