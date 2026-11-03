// ExpOrbEntity tests (V2-11): the XP orb's physics (the ItemEntity's
// rest-class discipline: gravity / ground friction / sub-step collision),
// the value discipline (1..9 clamp — no merge, no split), the two timers
// (the 0.5 s real-dt pickup delay + the 120 s lifetime, the save's
// restore contract), and the structural no-merge guarantee (the orb is
// NOT an ItemEntity — the game's merge pass dynamic_casts ItemEntity and
// can never see one). The game-level consumers (the magnet branch, the
// pickup branch, the three grant points) are data- and format-tested in
// the registry / save suites; the E2E gate (182) covers them on the
// scene.
#include <gtest/gtest.h>

#include <cmath>
#include <string>
#include <vector>

#include "block/block_registry.hpp"
#include "core/json.hpp"
#include "entity/entity.hpp"
#include "entity/exp_orb.hpp"
#include "entity/item_entity.hpp"
#include "entity/pickup_magnet.hpp"
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

// stone: the floor block (one solid block is enough — the orb's physics
// never reads a property, the Entity base does).
BlockRegistry makeBlocks() {
    BlockRegistry blocks;
    auto r = blocks.registerBlock(parseJson(
        R"({"id":"stone","model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
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

    Fixture() {
        stone = blocks.idForName("stone");
        for (int x = -16; x < 16; ++x) {
            for (int z = -16; z < 16; ++z) {
                world.setBlock(x, 0, z, stone);
            }
        }
    }

    ExpOrbEntity makeOrb(Vec3 pos, int value = 5) const {
        return ExpOrbEntity(world, blocks, value, pos);
    }

    void run(Entity& e, int n, double dt = kDt) {
        for (int i = 0; i < n; ++i) {
            e.update(dt);
        }
    }
};

} // namespace

// -- physics (the ItemEntity's rest-class twin) -----------------------------

TEST(ExpOrbEntityTest, FallsAndRestsExactlyOnFloor) {
    Fixture f;
    ExpOrbEntity orb = f.makeOrb({8.5f, 10.0f, 8.5f});
    f.run(orb, 120); // 2 s: a 9-block fall takes < 1 s
    EXPECT_FLOAT_EQ(orb.position().y, 1.0f);
    EXPECT_EQ(orb.velocity().y, 0.0f);
    EXPECT_TRUE(orb.onGround());
}

TEST(ExpOrbEntityTest, RestsWithoutJitter) {
    Fixture f;
    ExpOrbEntity orb = f.makeOrb({8.5f, 1.0f, 8.5f}); // bottom on the surface
    f.run(orb, 240); // 4 s
    // The Y resolve clamps to the exact integer boundary (the item twin):
    // no drift.
    EXPECT_FLOAT_EQ(orb.position().y, 1.0f);
    EXPECT_FLOAT_EQ(orb.position().x, 8.5f);
    EXPECT_FLOAT_EQ(orb.position().z, 8.5f);
    EXPECT_TRUE(orb.onGround());
}

TEST(ExpOrbEntityTest, FrictionStopsHorizontalDrift) {
    Fixture f;
    ExpOrbEntity orb = f.makeOrb({0.5f, 1.0f, 0.5f});
    orb.setVelocity({4.0f, 0.0f, 0.0f});
    f.run(orb, 300); // 5 s: long past the decay (total drift 4/2.5 = 1.6)
    EXPECT_EQ(orb.velocity().x, 0.0f);
    EXPECT_TRUE(orb.onGround());
    EXPECT_GT(orb.position().x, 0.5f); // it did drift before stopping
    const float xAfter = orb.position().x;
    f.run(orb, 60);
    // Truly at rest: no sub-pixel creep (the kStopSpeed zero, the item's).
    EXPECT_FLOAT_EQ(orb.position().x, xAfter);
}

TEST(ExpOrbEntityTest, HighSpeedFallDoesNotTunnelThroughFloor) {
    Fixture f;
    ExpOrbEntity orb = f.makeOrb({8.5f, 10.0f, 8.5f});
    f.run(orb, 44); // build up fall speed (~23.5 blocks/s)
    ASSERT_LT(orb.position().y, 1.5f);
    ASSERT_GT(orb.position().y, 1.0f); // still above the floor
    orb.update(0.25); // one large tick: ~7.7 blocks of motion
    EXPECT_FLOAT_EQ(orb.position().y, 1.0f); // landed, not tunneled
    EXPECT_TRUE(orb.onGround());
}

// -- the value discipline (1..9, no merge / no split) -----------------------

TEST(ExpOrbEntityTest, ConstructorClampsValueIntoRange) {
    Fixture f;
    // In range: untouched.
    ExpOrbEntity ok = f.makeOrb({8.5f, 1.0f, 8.5f}, 5);
    EXPECT_EQ(ok.value(), 5);
    // Below the cap: clamped up to 1 (a 0 grant never spawns — the belt).
    ExpOrbEntity low = f.makeOrb({8.5f, 1.0f, 8.5f}, 0);
    EXPECT_EQ(low.value(), ExpOrbEntity::kMinValue);
    // Above the cap: clamped down to 9 (the split point — the documented
    // simplification: one orb max, no 9+ split).
    ExpOrbEntity high = f.makeOrb({8.5f, 1.0f, 8.5f}, 99);
    EXPECT_EQ(high.value(), ExpOrbEntity::kMaxValue);
    // The bounds themselves.
    EXPECT_EQ(f.makeOrb({8.5f, 1.0f, 8.5f}, 1).value(), 1);
    EXPECT_EQ(f.makeOrb({8.5f, 1.0f, 8.5f}, 9).value(), 9);
}

TEST(ExpOrbEntityTest, OrbsNeverMergeIntoEachOther) {
    // The structural no-merge guarantee: the game's merge pass
    // dynamic_casts ItemEntity — an orb is NOT an ItemEntity, so the pass
    // can never see one (two co-located orbs both survive, each keeping
    // its own value — no canMergeInto / absorb API exists on the type).
    Fixture f;
    ExpOrbEntity a = f.makeOrb({8.5f, 1.0f, 8.5f}, 3);
    ExpOrbEntity b = f.makeOrb({8.5f, 1.0f, 8.5f}, 5); // same spot
    // Through the polymorphic base (the game's merge pass casts its
    // Entity* list the same way): an orb can never downcast to the item.
    EXPECT_EQ(dynamic_cast<ItemEntity*>(static_cast<Entity*>(&a)), nullptr);
    EXPECT_EQ(dynamic_cast<ItemEntity*>(static_cast<Entity*>(&b)), nullptr);
    f.run(a, 30);
    f.run(b, 30);
    EXPECT_EQ(a.value(), 3); // untouched by any "merge"
    EXPECT_EQ(b.value(), 5); // both orbs coexist at the same rest spot
}

// -- the timers (the save's restore contract) --------------------------------

TEST(ExpOrbEntityTest, PickupDelayCountsDown) {
    Fixture f;
    ExpOrbEntity orb = f.makeOrb({8.5f, 1.0f, 8.5f});
    EXPECT_FALSE(orb.pickupReady()); // fresh spawn: the delay is active
    // 0.25 is exact in binary, so the two updates subtract the 0.5 s delay
    // with no floating-point residue (the item test's twin, verbatim).
    orb.update(0.25); // half the delay
    EXPECT_FALSE(orb.pickupReady());
    orb.update(0.25); // total 0.5 s: the delay is exactly exhausted
    EXPECT_TRUE(orb.pickupReady());
    f.run(orb, 60);
    EXPECT_TRUE(orb.pickupReady()); // stays ready
}

TEST(ExpOrbEntityTest, DespawnAfterAgeAndBelowKillPlane) {
    Fixture f;
    ExpOrbEntity orb = f.makeOrb({8.5f, 1.0f, 8.5f});
    f.run(orb, 119, 1.0); // 119 s (the item's lifetime, 120 s)
    EXPECT_FALSE(orb.shouldDespawn());
    orb.update(1.0); // 120 s
    EXPECT_TRUE(orb.shouldDespawn());
    // The kill plane (the item twin): a void world, the orb despawns on
    // the way down.
    BlockRegistry blocks = makeBlocks();
    World voidWorld; // no floor at all
    ExpOrbEntity falling(voidWorld, blocks, 5, {8.5f, 10.0f, 8.5f});
    int ticks = 0;
    while (!falling.shouldDespawn() && ticks < 500) {
        falling.update(kDt);
        ++ticks;
    }
    EXPECT_LT(ticks, 500);
    EXPECT_TRUE(falling.belowKillPlane());
}

TEST(ExpOrbEntityTest, TimerRestoreRoundTrip) {
    // The save/load contract (format 26 restores age + pickup_delay
    // through setAge / setPickupDelay, the item's twin): a restored orb
    // continues both timers exactly — the ready state and the despawn
    // horizon are pure functions of the restored values.
    Fixture f;
    ExpOrbEntity orb = f.makeOrb({8.5f, 1.0f, 8.5f});
    orb.setAge(60.0);
    orb.setPickupDelay(0.25);
    EXPECT_DOUBLE_EQ(orb.age(), 60.0);
    EXPECT_DOUBLE_EQ(orb.pickupDelayRemaining(), 0.25);
    EXPECT_FALSE(orb.pickupReady());
    orb.update(0.25); // the restored delay runs out on exactly 0.25 s
    EXPECT_TRUE(orb.pickupReady());
    EXPECT_DOUBLE_EQ(orb.age(), 60.25);
    orb.setAge(119.9);
    EXPECT_FALSE(orb.shouldDespawn());
    orb.update(0.1); // 120.0: the restored lifetime expires
    EXPECT_TRUE(orb.shouldDespawn());
}

// -- the render geometry (the solid cube convention) -------------------------

TEST(ExpOrbEntityTest, OrbGeometryIsTheSolidCube) {
    // makeExpOrbGeometry: the shared six-face table at kSize — 24
    // vertices / 36 indices (the item cube's count), every corner inside
    // the kSize cube centered on the origin (the draw translates by
    // bottom-center + kSize/2, the item cube's convention).
    const ItemMeshGeometry geom = makeExpOrbGeometry();
    EXPECT_EQ(geom.vertices.size(), 24u);
    EXPECT_EQ(geom.indices.size(), 36u);
    const float half = ExpOrbEntity::kSize * 0.5f;
    for (const MeshVertex& v : geom.vertices) {
        EXPECT_GE(v.position.x, -half);
        EXPECT_LE(v.position.x, half);
        EXPECT_GE(v.position.y, -half);
        EXPECT_LE(v.position.y, half);
        EXPECT_GE(v.position.z, -half);
        EXPECT_LE(v.position.z, half);
        // Full sky light baked (the per-draw override replaces it at
        // render — the item cube / arrow convention).
        EXPECT_FLOAT_EQ(v.skyLight, 1.0f);
        EXPECT_FLOAT_EQ(v.blockLight, 0.0f);
    }
    // The face table's winding: every face's corner cross product points
    // along the face normal (the FaceWindingIsCounterClockwiseFromOutside
    // convention — the orb reuses kCubeFaces, so the guard inherits).
    for (int face = 0; face < 6; ++face) {
        const MeshVertex& c0 =
            geom.vertices[static_cast<std::size_t>(face * 4)];
        const MeshVertex& c1 =
            geom.vertices[static_cast<std::size_t>(face * 4 + 1)];
        const MeshVertex& c2 =
            geom.vertices[static_cast<std::size_t>(face * 4 + 2)];
        const Vec3 crs = cross(c1.position - c0.position,
                               c2.position - c0.position);
        EXPECT_GT(dot(crs, c0.normal), 0.0f) << "face " << face;
    }
}

// -- the magnet's input contract (the game's branch consumes these) ---------

TEST(ExpOrbEntityTest, MagnetDisplacementAppliesToOrbState) {
    // The game's magnet branch (dynamic_cast isolated) pulls a READY orb
    // by the V2-6 frame-locked displacement — the same pure function the
    // items take. This pins the orb's side of the contract: the position
    // + pickupReady state the branch reads, and that the displacement is
    // nonzero inside the radius / zero beyond (a non-ready orb is never
    // pulled — the branch gates on pickupReady first).
    Fixture f;
    const Vec3 player {8.5f, 1.0f, 8.5f};
    ExpOrbEntity orb = f.makeOrb({10.5f, 1.0f, 8.5f}, 5); // 2.0 away
    ASSERT_FALSE(orb.pickupReady()); // fresh: the magnet branch skips it
    orb.update(0.5); // the delay runs out
    ASSERT_TRUE(orb.pickupReady());
    const Vec3 disp =
        pickupMagnetDisplacement(orb.position(), player, 0, 1.0 / 60.0);
    // 2.0 < kMagnetRadius (2.5): the radial pull is toward the player.
    EXPECT_EQ(disp.y, 0.0f); // horizontal only (the magnet convention)
    EXPECT_LT(disp.x, 0.0f); // toward the player at x = 8.5
    // Beyond the radius the pull is zero (the fixed-radius gate).
    ExpOrbEntity far = f.makeOrb({12.0f, 1.0f, 8.5f}, 5); // 3.5 away
    far.update(0.5);
    EXPECT_EQ(pickupMagnetDisplacement(far.position(), player, 0, 1.0 / 60.0),
              Vec3::zero());
}
