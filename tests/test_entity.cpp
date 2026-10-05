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
    // A stuck arrow is static (no physics) but keeps aging: it despawns
    // when the age reaches kMaxAge, like any other arrow.
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
    f.run(arrow, static_cast<int>(Projectile::kMaxAge / kDt) + 60);
    EXPECT_FALSE(arrow.dead());            // it aged out, it was not killed
    EXPECT_TRUE(arrow.shouldDespawn());
    EXPECT_GE(arrow.age(), ageAtStuck + Projectile::kMaxAge - 0.1);
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
