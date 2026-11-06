#include <gtest/gtest.h>

#include <SDL3/SDL.h>

#include <cmath>
#include <cstdint>
#include <string>

#include "block/block_registry.hpp"
#include "core/input.hpp"
#include "entity/mob_swing.hpp"
#include "player/player.hpp"
#include "world/world.hpp"

using namespace mc;

namespace {

constexpr float kPi = 3.14159265358979f;
constexpr double kDt = 1.0 / 60.0;

Json parseJson(const std::string& text) {
    auto r = Json::parse(text);
    if (!r.ok()) {
        ADD_FAILURE() << "parse failed: " << r.error().message;
        return Json::object();
    }
    return *r;
}

BlockRegistry makeBlocks() {
    BlockRegistry blocks;
    auto r = blocks.registerBlock(parseJson(
        R"({"id":"stone","model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    if (!r.ok()) {
        ADD_FAILURE() << r.error().message;
    }
    r = blocks.registerBlock(parseJson(
        R"({"id":"water","properties":{"solid":false,"opaque":false,)"
        R"("transparent":true},"model":{"textures":{"top":"w","side":"w","bottom":"w"}}})"));
    if (!r.ok()) {
        ADD_FAILURE() << r.error().message;
    }
    // The fire block (Phase 70): non-solid, like the real content (the
    // player's AABB can overlap it — that IS the contact).
    r = blocks.registerBlock(parseJson(
        R"({"id":"fire","properties":{"solid":false,"opaque":false,)"
        R"("transparent":true},"model":{"textures":{"top":"f","side":"f","bottom":"f"}}})"));
    if (!r.ok()) {
        ADD_FAILURE() << r.error().message;
    }
    // The campfire block (Phase 71): non-solid like the real content — the
    // sitting test needs the player's AABB to overlap it (the campfire id
    // the player resolves by name in its constructor).
    r = blocks.registerBlock(parseJson(
        R"({"id":"campfire","properties":{"solid":false,"opaque":false,)"
        R"("transparent":true},"model":{"textures":{"top":"c","side":"c","bottom":"c"}}})"));
    if (!r.ok()) {
        ADD_FAILURE() << r.error().message;
    }
    // Phase 75: the extinguished twin (the seat works on it too).
    r = blocks.registerBlock(parseJson(
        R"({"id":"campfire_extinguished","properties":{"solid":false,)"
        R"("opaque":false,"transparent":true},"model":{"textures":{"top":"c","side":"c","bottom":"c"}}})"));
    if (!r.ok()) {
        ADD_FAILURE() << r.error().message;
    }
    // V2-9: the slab (solid-region top 0.5, the non-integer foot-height
    // surface of the step-up coincidence test) — the mob fixture's twin.
    r = blocks.registerBlock(parseJson(
        R"({"id":"slab","model":{"shape":"slab",)"
        R"("textures":{"top":"t","side":"t","bottom":"t"}}})"));
    if (!r.ok()) {
        ADD_FAILURE() << r.error().message;
    }
    return blocks;
}

// A solid stone floor covering x,z in [-16, 16) at y=0 (top surface y=1),
// plus the block set the player needs (solid stone, non-solid water).
struct Fixture {
    BlockRegistry blocks = makeBlocks();
    World world;
    BlockID stone;
    BlockID slab;
    BlockID water;
    BlockID fire;
    BlockID campfire;
    BlockID campfireExt;

    Fixture() {
        stone = blocks.idForName("stone");
        slab = blocks.idForName("slab");
        water = blocks.idForName("water");
        fire = blocks.idForName("fire");
        campfire = blocks.idForName("campfire");
        campfireExt = blocks.idForName("campfire_extinguished");
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

    Player makePlayer(Vec3 pos,
                      const Player::Params& params = Player::Params {}) const {
        return Player(world, blocks, pos, params);
    }
};

// SDL virtual key code for a GameKey (Input's test entry point).
std::uint32_t sdlCode(GameKey key) {
    switch (key) {
        case GameKey::W: return SDLK_W;
        case GameKey::A: return SDLK_A;
        case GameKey::S: return SDLK_S;
        case GameKey::D: return SDLK_D;
        case GameKey::Space: return SDLK_SPACE;
        case GameKey::LeftShift: return SDLK_LSHIFT;
        case GameKey::LeftControl: return SDLK_LCTRL;
        default: return 0;
    }
}

// Fresh input frame with the given keys held down. isDown() persists across
// frameBegin, so the same Input can drive many update() calls.
Input heldInput(std::initializer_list<GameKey> keys) {
    Input input;
    input.frameBegin();
    for (GameKey key : keys) {
        input.handleKey(sdlCode(key), true);
    }
    return input;
}

// Advance the player n ticks with the given held keys. Look is disabled by
// default (tests drive yaw/pitch directly); pass true to exercise mouse look.
void run(Player& p, int n, Input& input, bool look = false) {
    for (int i = 0; i < n; ++i) {
        input.frameBegin();
        p.update(kDt, input, look);
    }
}

} // namespace

// -- resting / gravity -------------------------------------------------------

TEST(PlayerTest, RestsExactlyOnFloor) {
    Fixture f;
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f}); // bottom on the floor surface
    Input idle = heldInput({});
    run(p, 120, idle);
    // The Y resolve clamps to the exact integer boundary: no jitter.
    EXPECT_FLOAT_EQ(p.position().y, 1.0f);
    EXPECT_EQ(p.velocity().y, 0.0f);
    EXPECT_TRUE(p.onGround());
}

TEST(PlayerTest, FallsThenRests) {
    Fixture f;
    Player p = f.makePlayer({8.5f, 10.0f, 8.5f});
    Input idle = heldInput({});
    run(p, 120, idle); // 9-block fall takes < 1 s
    EXPECT_FLOAT_EQ(p.position().y, 1.0f);
    EXPECT_TRUE(p.onGround());
    EXPECT_EQ(p.velocity().y, 0.0f);
}

TEST(PlayerTest, FallsOffLedgeStopsBeingOnGround) {
    Fixture f;
    // Remove half the floor (x >= 0) so the player can walk off an edge.
    for (int x = 0; x < 16; ++x) {
        for (int z = -16; z < 16; ++z) {
            f.world.setBlock(x, 0, z, BlockID::air());
        }
    }
    // Just behind the edge (AABB still over the last solid column, x=-1).
    Player p = f.makePlayer({-0.5f, 1.0f, 8.5f});
    Input idle = heldInput({});
    run(p, 5, idle);
    EXPECT_TRUE(p.onGround());

    // Walk east (+X) off the edge.
    p.yaw = -kPi / 2.0f; // facing +X
    Input walk = heldInput({GameKey::W});
    run(p, 30, walk); // ~2.1 blocks: well past the edge
    EXPECT_GT(p.position().x, 0.3f); // AABB fully clear of the floor
    EXPECT_FALSE(p.onGround());      // mid-fall
    EXPECT_LT(p.velocity().y, 0.0f);
}

// -- walls / sliding ---------------------------------------------------------

TEST(PlayerTest, WallBlocksMovement) {
    Fixture f;
    f.wallX(12, 1, 6, -16, 16); // wall in the plane x = 12 (cells [12, 13))
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    p.yaw = -kPi / 2.0f; // facing +X (into the wall)
    Input walk = heldInput({GameKey::W});
    run(p, 300, walk);
    // Clamped flush: player max.x (x + width/2) == wall min.x (12).
    EXPECT_FLOAT_EQ(p.position().x, 12.0f - 0.3f);
    EXPECT_EQ(p.velocity().x, 0.0f);
    EXPECT_LT(p.position().x, 12.0f); // never passed through
}

TEST(PlayerTest, SlidesAlongWall) {
    Fixture f;
    f.wallX(12, 1, 6, -16, 16);
    Player p = f.makePlayer({11.7f, 1.0f, 10.0f});
    p.yaw = -kPi / 2.0f; // W = +X (blocked by the wall), A = -right = -Z
    Input walk = heldInput({GameKey::W, GameKey::A});
    run(p, 120, walk); // 2 s: z should move along the wall
    // The blocked axis stays clamped, the other keeps moving: sliding.
    // ENT-7 re-pin: the diagonal (4.3/sqrt(2)) covers 2 s less the
    // start-up ramp distance (the kGroundAccelRate 8 approach).
    EXPECT_FLOAT_EQ(p.position().x, 12.0f - 0.3f);
    EXPECT_NEAR(p.position().z, 4.3115f, 0.001f);
    EXPECT_TRUE(p.onGround());
}

// -- jumping -----------------------------------------------------------------

TEST(PlayerTest, JumpReachesExpectedHeight) {
    Fixture f;
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    Input idle = heldInput({});
    run(p, 10, idle); // settle
    ASSERT_TRUE(p.onGround());

    Input jump = heldInput({GameKey::Space});
    p.update(kDt, jump, false);
    EXPECT_GT(p.velocity().y, 0.0f);

    float peak = p.position().y;
    for (int i = 0; i < 120; ++i) {
        jump.frameBegin();
        p.update(kDt, jump, false);
        peak = std::max(peak, p.position().y);
    }
    // Theoretical (continuous) peak: 1 + v^2/(2g) = 1 + 8.5^2/64 ~= 2.13;
    // the semi-implicit Euler discretization lands on 1 + 72/60 = 2.2.
    EXPECT_GT(peak, 2.0f);
    EXPECT_LT(peak, 2.25f);
}

TEST(PlayerTest, ClimbsOneBlockButNotTwo) {
    // One-block step at x in [10, 20), z in [8, 10) (wide enough that the
    // player cannot walk off the far end within the test window).
    // Phase 16: no jump needed — walking into the step auto-steps up one
    // block (flush face + solid step cell + two free cells above it).
    {
        Fixture f;
        for (int x = 10; x < 20; ++x) {
            for (int z = 8; z < 10; ++z) {
                f.world.setBlock(x, 1, z, f.stone);
            }
        }
        Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
        p.yaw = -kPi / 2.0f; // facing +X (into the step)
        Input walk = heldInput({GameKey::W});
        run(p, 60, walk); // walk only: the step is climbed, not jumped
        EXPECT_GT(p.position().x, 10.3f); // crossed the step
        EXPECT_FLOAT_EQ(p.position().y, 2.0f); // resting on the step top
        EXPECT_TRUE(p.onGround());
    }
    // Two-block step: no step-up (the cell above the step is solid) and
    // the jump peak (2.13) is below its top (3).
    {
        Fixture f;
        for (int x = 10; x < 20; ++x) {
            for (int z = 8; z < 10; ++z) {
                f.world.setBlock(x, 1, z, f.stone);
                f.world.setBlock(x, 2, z, f.stone);
            }
        }
        Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
        p.yaw = -kPi / 2.0f;
        Input jumpWalk = heldInput({GameKey::W, GameKey::Space});
        float maxY = p.position().y;
        for (int i = 0; i < 180; ++i) {
            jumpWalk.frameBegin();
            p.update(kDt, jumpWalk, false);
            maxY = std::max(maxY, p.position().y);
        }
        // 3 s of jump+walk: the bottom never clears the step top (y=3),
        // so the player never crosses.
        EXPECT_FLOAT_EQ(p.position().x, 10.0f - 0.3f);
        EXPECT_LT(maxY, 3.0f);
    }
}

TEST(PlayerTest, ClimbsOneBlockAlongZ) {
    // Regression: the Z-axis step-up never fired (the front-cell lookup was
    // indexed with moveAxis's numbering, axis==2, so cz stayed at the
    // player's own cell — air — and the step check always failed). Walk
    // -Z into a one-block step (the X-axis twin above covers +X).
    Fixture f;
    for (int x = 8; x < 10; ++x) {
        for (int z = -16; z < -8; ++z) { // wide: cannot walk off in 1 s
            f.world.setBlock(x, 1, z, f.stone);
        }
    }
    Player p = f.makePlayer({8.5f, 1.0f, -7.5f});
    p.yaw = 0.0f; // facing -Z (into the step)
    Input walk = heldInput({GameKey::W});
    run(p, 60, walk); // walk only: the step is climbed, not jumped
    EXPECT_LT(p.position().z, -8.3f); // crossed the step
    EXPECT_FLOAT_EQ(p.position().y, 2.0f); // resting on the step top
    EXPECT_TRUE(p.onGround());
}

TEST(PlayerTest, StepsUpSingleCellStepPlusX) {
    // V2-9 quirk ①: the +X front check now looks at the cell the body is
    // flush against (snapped, not snapped+1), so a step EXACTLY one cell
    // wide is crossable in +X (pre-V2-9 the player stayed clamped at the
    // face — the asymmetry the -X twin already passed). The single cell
    // is not wide enough to stand on for the whole window: the pin is the
    // crossing itself (the player walks past the far end, x > 11.3, and
    // settles back on the floor).
    Fixture f;
    for (int z = 8; z < 10; ++z) {
        f.world.setBlock(10, 1, z, f.stone); // one cell only (x = 10)
    }
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    p.yaw = -kPi / 2.0f; // facing +X (into the step)
    Input walk = heldInput({GameKey::W});
    run(p, 90, walk);
    EXPECT_GT(p.position().x, 11.3f); // crossed the single cell, walked off
    EXPECT_FLOAT_EQ(p.position().y, 1.0f); // back on the floor
    EXPECT_TRUE(p.onGround());
}

TEST(PlayerTest, StepsUpSingleCellStepPlusZ) {
    // V2-9 quirk ①, the Z axis: the +Z front check now looks at the cell
    // the body is flush against (snapped, not snapped+1), so a step
    // exactly one cell wide is crossable in +Z (ClimbsOneBlockAlongZ
    // above covers -Z; the X-axis twin is StepsUpSingleCellStepPlusX).
    Fixture f;
    for (int x = 8; x < 10; ++x) {
        f.world.setBlock(x, 1, 10, f.stone); // one cell only (z = 10)
    }
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    p.yaw = kPi; // facing +Z (into the step)
    Input walk = heldInput({GameKey::W});
    run(p, 90, walk);
    EXPECT_GT(p.position().z, 11.3f); // crossed the single cell, walked off
    EXPECT_FLOAT_EQ(p.position().y, 1.0f); // back on the floor
    EXPECT_TRUE(p.onGround());
}

TEST(PlayerTest, SlabTopWalkNoCoincidentalLift) {
    // V2-9 quirk ②: the flush test is positional, not causal — at the
    // NON-integer foot height (the slab top, 1.5) the face drifts over an
    // integer within kFaceEps while walking, with the front foot cell the
    // SAME slab (solid-region top 1.5 = the feet: no overlap, no real
    // clamp). Pre-V2-9 the body lifted 1.0 (feet 2.5) and fell back; the
    // front-column real-block check (a solid region strictly above the
    // feet, hard rule 6) rejects the coincidence — the feet stay at 1.5
    // for the whole walk (the mob slab-chase frame-606 pin, shared).
    // The start (8.17, not 8.5) phase-pins the walk: the frame-locked
    // trajectory's face crosses an integer within kFaceEps at frame 56
    // (probed on the pre-fix tree, where it pulses to 2.5 there); the
    // 1.0 offset twin (9.17) fires identically.
    Fixture f;
    for (int x = 8; x < 36; ++x) {
        for (int z = 8; z < 10; ++z) {
            f.world.setBlock(x, 1, z, f.slab); // the slab strip, top 1.5
        }
    }
    Player p = f.makePlayer({8.17f, 1.5f, 8.5f});
    p.yaw = -kPi / 2.0f; // facing +X, along the strip
    Input walk = heldInput({GameKey::W});
    float maxY = p.position().y;
    for (int i = 0; i < 300; ++i) {
        walk.frameBegin();
        p.update(kDt, walk, false);
        maxY = std::max(maxY, p.position().y);
    }
    EXPECT_FLOAT_EQ(maxY, 1.5f); // no coincidental 1.0 pulse
    EXPECT_FLOAT_EQ(p.position().y, 1.5f); // feet flush on the slab top
    EXPECT_GT(p.position().x, 28.0f); // walked most of the strip
    EXPECT_TRUE(p.onGround());
}

TEST(PlayerTest, CeilingStopsRise) {
    Fixture f;
    for (int x = 8; x < 10; ++x) {
        for (int z = 8; z < 10; ++z) {
            f.world.setBlock(x, 3, z, f.stone); // ceiling: cell [3, 4)
        }
    }
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    Input idle = heldInput({});
    run(p, 5, idle);

    Input jump = heldInput({GameKey::Space});
    float maxY = p.position().y;
    for (int i = 0; i < 80; ++i) {
        jump.frameBegin();
        p.update(kDt, jump, false);
        maxY = std::max(maxY, p.position().y);
    }
    // The head (y + 1.8) never passes the ceiling (y=3): y <= 1.2.
    EXPECT_LE(maxY, 1.2f + 1e-3f);
    // ...and the player eventually rests back on the floor.
    run(p, 60, idle);
    EXPECT_FLOAT_EQ(p.position().y, 1.0f);
}

// -- non-solid blocks --------------------------------------------------------

TEST(PlayerTest, WalksThroughWater) {
    Fixture f;
    for (int x = 10; x < 12; ++x) {
        for (int y = 1; y <= 4; ++y) {
            for (int z = 8; z < 10; ++z) {
                f.world.setBlock(x, y, z, f.water);
            }
        }
    }
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    p.yaw = -kPi / 2.0f; // facing +X (into the water)
    Input walk = heldInput({GameKey::W});
    run(p, 60, walk); // 1 s at 4.3 blocks/s
    EXPECT_GT(p.position().x, 11.9f); // passed straight through
    EXPECT_TRUE(p.onGround());
}

// -- swimming (Phase 9: fluid cells) ------------------------------------------

namespace {

// A 2-deep pool of full fluid (y=1..2) on the fixture's floor.
void buildPool(Fixture& f, int x0, int x1, int z0, int z1,
               FluidKind kind = FluidKind::Water) {
    for (int x = x0; x <= x1; ++x) {
        for (int z = z0; z <= z1; ++z) {
            f.world.setFluid(x, 1, z, kind, kMaxFluidLevel);
            f.world.setFluid(x, 2, z, kind, kMaxFluidLevel);
        }
    }
}

} // namespace

TEST(PlayerTest, SwimsUpWithSpace) {
    Fixture f;
    buildPool(f, 6, 10, 6, 10);
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f}); // feet on the pool floor
    Input idle = heldInput({});
    run(p, 10, idle);
    ASSERT_TRUE(p.onGround());
    Input swim = heldInput({GameKey::Space});
    const float y0 = p.position().y;
    // 0.5 s: rises at 4.5 (the swim-up); at the surface Space becomes the
    // jump (FLUID-2, Q3) — the player ends up above the pool top either
    // way.
    run(p, 30, swim);
    EXPECT_GT(p.position().y, y0 + 0.5f); // surfaced out of the pool
}

TEST(PlayerTest, SinksSlowlyInWater) {
    Fixture f;
    buildPool(f, 6, 10, 6, 10);
    Player p = f.makePlayer({8.5f, 4.0f, 8.5f}); // 1 block above the pool top
    Input idle = heldInput({});
    float worst = 0.0f;
    int cross = -1; // first frame with feet below the pool top (y=3)
    for (int i = 0; i < 120; ++i) {
        p.update(kDt, idle, false);
        if (cross < 0 && p.position().y < 3.0f) {
            cross = i;
        }
        // From the frame AFTER the crossing one, the water physics (with
        // the 4 blocks/s sink cap) is active; the crossing frame itself was
        // still air physics at ~8 blocks/s.
        if (cross > 0 && i > cross && p.velocity().y < -worst) {
            worst = -p.velocity().y;
        }
        if (p.onGround()) {
            break;
        }
    }
    // The sink speed never exceeds the water terminal (4 blocks/s).
    EXPECT_LE(worst, 4.0f + 1e-3f);
    EXPECT_TRUE(p.onGround());
    EXPECT_FLOAT_EQ(p.position().y, 1.0f); // back on the pool floor
}

TEST(PlayerTest, SwimSpeedIsReduced) {
    Fixture f;
    buildPool(f, 6, 20, 6, 10); // a long pool
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    Input idle = heldInput({});
    run(p, 5, idle);
    p.yaw = -kPi / 2.0f; // facing +X
    Input walk = heldInput({GameKey::W});
    const float x0 = p.position().x;
    run(p, 30, walk); // 0.5 s
    // swimSpeed (3.1) x 0.5 s — clearly below walk speed (4.3 x 0.5 = 2.15).
    EXPECT_NEAR(p.position().x - x0, 1.55f, 0.1f);
    EXPECT_TRUE(p.onGround());
}

// -- FLUID-2 swim: passive rise, surface float, crouch-dive ---------------------
//
// Pool conventions: the fixture floor tops out at y=1; an n-cell L8 pool
// (buildPoolDepth) has W' (the float plane) = 1 + n. The float terminal is
// the geometric fixed point (y = W', vy = 0, onGround false) — an exact,
// fps-independent assertion target.

namespace {

// An n-cell full (L8) water pool on the fixture floor (y=1..n): W' = 1+n.
void buildPoolDepth(Fixture& f, int x0, int x1, int z0, int z1, int n) {
    for (int x = x0; x <= x1; ++x) {
        for (int z = z0; z <= z1; ++z) {
            for (int y = 1; y <= n; ++y) {
                f.world.setFluid(x, y, z, FluidKind::Water, kMaxFluidLevel);
            }
        }
    }
}

// A player preset AT the surface of an n-cell pool (W' = 1+n), settled a
// few idle ticks into the float fixed point.
Player makeFloater(Fixture& f, int n) {
    buildPoolDepth(f, 6, 10, 6, 10, n);
    Player p = f.makePlayer({8.5f, 1.0f + static_cast<float>(n), 8.5f});
    Input idle = heldInput({});
    run(p, 5, idle);
    return p;
}

} // namespace

TEST(PlayerTest, RisesToSurfaceWithNoInput) {
    // A submerged body with no input RISES and FLOATS at the surface
    // (Minecraft — the Phase-9 "sink to the bottom" is retired).
    Fixture f;
    buildPoolDepth(f, 6, 10, 6, 10, 2); // W' = 3
    Player p = f.makePlayer({8.5f, 1.5f, 8.5f}); // mid-pool, 1.5 below W'
    Input idle = heldInput({});
    run(p, 600, idle); // 10 s: the rise (~1.2 s) + the settle
    // The geometric terminal: exact.
    EXPECT_FLOAT_EQ(p.position().y, 3.0f);
    EXPECT_EQ(p.velocity().y, 0.0f);
    EXPECT_FALSE(p.onGround());
}

TEST(PlayerTest, FloatIsDepthIndependent) {
    // The same pool, two preset depths (1.5 / 0.5 below W' = 3): the
    // terminal is the same surface plane (the float is geometry, not a
    // force balance).
    for (float preset : {1.5f, 2.5f}) {
        Fixture f;
        buildPoolDepth(f, 6, 10, 6, 10, 2);
        Player p = f.makePlayer({8.5f, preset, 8.5f});
        Input idle = heldInput({});
        run(p, 600, idle);
        EXPECT_FLOAT_EQ(p.position().y, 3.0f);
        EXPECT_EQ(p.velocity().y, 0.0f);
        EXPECT_FALSE(p.onGround());
    }
}

TEST(PlayerTest, StaysOnPoolFloorWhenNoInput) {
    // Grounded in water (the pool floor): the body does NOT rise
    // (Minecraft "stand on the bottom" — the rise is gated on
    // !onGround).
    Fixture f;
    buildPoolDepth(f, 6, 10, 6, 10, 2); // W' = 3
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f}); // feet on the pool floor
    Input idle = heldInput({});
    run(p, 10, idle);
    ASSERT_TRUE(p.onGround());
    bool left = false;
    for (int i = 0; i < 600; ++i) {
        p.update(kDt, idle, false);
        if (std::abs(p.position().y - 1.0f) > 1e-4f) {
            left = true;
        }
    }
    EXPECT_FALSE(left); // never left the floor (no auto-rise)
    EXPECT_FLOAT_EQ(p.position().y, 1.0f);
    EXPECT_EQ(p.velocity().y, 0.0f);
    EXPECT_TRUE(p.onGround());
}

TEST(PlayerTest, ShallowFlowFilmHasNoSurface) {
    // A pure flow film (level 3, no level-8 support): no float plane
    // (the L8-only W' contract — a Phase-47 source settles to L8 per
    // FLUID-1, a flowing film does not). The player sinks to the bottom
    // and stands (the Phase-9 behavior), and never rises to the film top.
    Fixture f;
    for (int x = 6; x <= 10; ++x) {
        for (int z = 6; z <= 10; ++z) {
            f.world.setFluid(x, 1, z, FluidKind::Water, 3); // an L3 film
        }
    }
    Player p = f.makePlayer({8.5f, 4.0f, 8.5f}); // 2 above the film top
    Input idle = heldInput({});
    bool roseAtFilmTop = false;
    for (int i = 0; i < 300; ++i) {
        p.update(kDt, idle, false);
        // A rise out of the film (unconditional buoyancy, the rejected
        // variant) shows up as positive vy at/just below the film top.
        if (p.position().y >= 1.9f && p.velocity().y > 0.0f) {
            roseAtFilmTop = true;
        }
    }
    EXPECT_FALSE(roseAtFilmTop);
    EXPECT_FLOAT_EQ(p.position().y, 1.0f); // on the bottom, standing
    EXPECT_TRUE(p.onGround());
    EXPECT_EQ(p.velocity().y, 0.0f);
}

TEST(PlayerTest, CrouchSinksInWater) {
    // Crouch in water = dive: vy converges to -waterSinkSpeed (1.5), y
    // descends monotonically, and the float never engages.
    Fixture f;
    buildPoolDepth(f, 6, 10, 6, 10, 2); // W' = 3
    Player p = f.makePlayer({8.5f, 2.5f, 8.5f}); // 0.5 below the surface
    Input crouch = heldInput({GameKey::LeftControl});
    float prevY = p.position().y;
    bool monotone = true;
    for (int i = 0; i < 40; ++i) {
        p.update(kDt, crouch, false);
        if (p.position().y > prevY + 1e-4f) {
            monotone = false;
        }
        prevY = p.position().y;
    }
    EXPECT_TRUE(monotone);
    EXPECT_EQ(p.velocity().y, -p.params().waterSinkSpeed); // converged
    EXPECT_LT(p.position().y, 2.5f); // descended
    EXPECT_GT(p.position().y, 1.0f); // still mid-pool (not the floor yet)
}

TEST(PlayerTest, CrouchDivesOutOfSurface) {
    // Crouch from the float: the dive breaks the float (y < W' - 0.05)
    // and the player is submerged.
    Fixture f;
    Player p = makeFloater(f, 2); // W' = 3, resting at the float
    ASSERT_FLOAT_EQ(p.position().y, 3.0f);
    Input crouch = heldInput({GameKey::LeftControl});
    run(p, 12, crouch); // 0.2 s
    EXPECT_LT(p.position().y, 3.0f - 0.05f); // left the surface
    EXPECT_TRUE(p.inWater()); // under the plane
}

TEST(PlayerTest, SpaceFromSurfaceJumps) {
    // Space at the surface (no water overlap) = a normal jump at
    // jumpSpeed (Q3: the breach is a jump, not a swim-up): the exact
    // impulse, a peak that clears W' + 1.0, and the return terminal =
    // standing on the pool floor (deep water — mutually exclusive with
    // the float terminal of RisesToSurfaceWithNoInput).
    Fixture f;
    Player p = makeFloater(f, 2); // W' = 3
    Input jump = heldInput({GameKey::Space});
    p.update(kDt, jump, false);
    EXPECT_EQ(p.velocity().y, p.params().jumpSpeed); // 8.5, not 4.5
    // The peak (Space released): clears W' + 1.0.
    Input idle = heldInput({});
    float peak = p.position().y;
    for (int i = 0; i < 120; ++i) {
        p.update(kDt, idle, false);
        peak = std::max(peak, p.position().y);
    }
    EXPECT_GE(peak, 3.0f + 1.0f);
    // The return: sinks to the pool floor and stands.
    run(p, 300, idle);
    EXPECT_FLOAT_EQ(p.position().y, 1.0f);
    EXPECT_TRUE(p.onGround());
    EXPECT_EQ(p.velocity().y, 0.0f);
}

TEST(PlayerTest, SpaceFromSubmergedUsesSwimUpSpeed) {
    // Space while submerged (feet 1 cell below W') = the swim-up at
    // swimUpSpeed — the branch choice (in water vs at the surface).
    Fixture f;
    buildPoolDepth(f, 6, 10, 6, 10, 2); // W' = 3
    Player p = f.makePlayer({8.5f, 2.0f, 8.5f}); // 1 below the surface
    Input swim = heldInput({GameKey::Space});
    p.update(kDt, swim, false);
    EXPECT_EQ(p.velocity().y, p.params().swimUpSpeed); // 4.5, not 8.5
}

TEST(PlayerTest, SurfaceWalkUsesSwimSpeed) {
    // Walking at the float: the surface pace is the swim pace
    // (3.1 x 0.5 = 1.55, not the ground pace 4.3 x 0.5 = 2.15).
    Fixture f;
    Player p = makeFloater(f, 2);
    p.yaw = -kPi / 2.0f; // facing +X
    Input walk = heldInput({GameKey::W});
    const float x0 = p.position().x;
    run(p, 30, walk); // 0.5 s
    EXPECT_NEAR(p.position().x - x0, 1.55f, 0.1f);
    // The walk keeps the float (the plane is maintained).
    EXPECT_FLOAT_EQ(p.position().y, 3.0f);
    EXPECT_FALSE(p.onGround());
}

TEST(PlayerTest, NoSprintAtSurface) {
    // Sprint (Shift) is disabled at the surface (the any-fluid rule,
    // extended by FLUID-2): the pace is the swim pace (1.55), not the
    // sprint pace (2.8) — the mutual pair of SurfaceWalkUsesSwimSpeed.
    Fixture f;
    Player p = makeFloater(f, 2);
    p.yaw = -kPi / 2.0f;
    Input sprint = heldInput({GameKey::W, GameKey::LeftShift});
    const float x0 = p.position().x;
    run(p, 30, sprint); // 0.5 s
    EXPECT_NEAR(p.position().x - x0, 1.55f, 0.1f);
    EXPECT_EQ(p.velocity().x, p.params().swimSpeed);
}

TEST(PlayerTest, FallingIntoPoolSinksToFloor) {
    // A fall THROUGH the surface (vy < 0): no float (the direction
    // gate) — the entry-momentum clamp to -waterTerminalVelocity is
    // kept (P9), and the terminal = standing on the pool floor (mutually
    // exclusive with the float y = 3.0 of RisesToSurfaceWithNoInput).
    Fixture f;
    buildPoolDepth(f, 6, 10, 6, 10, 2); // W' = 3
    Player p = f.makePlayer({8.5f, 5.0f, 8.5f}); // W' + 2
    Input idle = heldInput({});
    bool settled = false;
    float minVy = 0.0f;
    for (int i = 0; i < 240; ++i) {
        p.update(kDt, idle, false);
        if (p.inWater() && p.velocity().y < minVy) {
            minVy = p.velocity().y;
        }
        if (p.inWater() && p.onGround()) {
            settled = true;
            break;
        }
    }
    ASSERT_TRUE(settled);
    EXPECT_GE(minVy, -p.params().waterTerminalVelocity - 1e-3f); // the clamp
    EXPECT_FLOAT_EQ(p.position().y, 1.0f); // the pool floor, not the float
    EXPECT_EQ(p.velocity().y, 0.0f);
}

TEST(PlayerTest, LavaPoolNeverFloats) {
    // W' counts only WATER: a lava column gives no float plane — the
    // Phase-54 lava semantics (sink, burn) is unchanged. 59f < the first
    // burn tick (the 1/60 accumulation crosses 1.0 at frame 60 — see
    // LavaBurnsAtTheConfiguredRate): the health is intact.
    Fixture f;
    buildPool(f, 6, 10, 6, 10, FluidKind::Lava); // lava y=1..2, top y=3
    Player p = f.makePlayer({8.5f, 2.0f, 8.5f}); // airborne in the lava
    Input idle = heldInput({});
    bool roseInLava = false;
    for (int i = 0; i < 59; ++i) {
        p.update(kDt, idle, false);
        if (p.inLava() && p.velocity().y > 0.0f) {
            roseInLava = true;
        }
    }
    EXPECT_FALSE(roseInLava); // never a rise in the lava
    EXPECT_LT(p.position().y, 3.0f - 0.5f); // not at the lava top (a float)
    EXPECT_TRUE(p.onGround()); // on the pool floor
    EXPECT_FLOAT_EQ(p.health(), 20.0f); // 59f < the first burn tick
}

TEST(PlayerTest, FloatTerminalIsDtIndependent) {
    // The float terminal is a geometric clamp (the snap to W'), not an
    // integral equilibrium: driving the same rise with 1/60 frames and
    // 1/30 double frames lands on the same EXACT y (the fps-independent
    // paradigm — a force-balance float would not).
    for (int frames : {600, 300}) { // 10 s of physics either way
        const double dt = 10.0 / frames; // 1/60 or 1/30
        Fixture f;
        buildPoolDepth(f, 6, 10, 6, 10, 2); // W' = 3
        Player p = f.makePlayer({8.5f, 1.5f, 8.5f});
        Input idle = heldInput({});
        for (int i = 0; i < frames; ++i) {
            p.update(dt, idle, false);
        }
        EXPECT_FLOAT_EQ(p.position().y, 3.0f);
        EXPECT_EQ(p.velocity().y, 0.0f);
        EXPECT_FALSE(p.onGround());
    }
}

TEST(PlayerTest, EdgeBlockNoFloat) {
    // Standing on a pool-edge block whose top = the pool surface W'
    // (grounded at the surface): the float does not engage — the ground
    // pace (4.3 x 0.5 = 2.15), not the swim pace (the !onGround gate of
    // the surface state).
    Fixture f;
    buildPoolDepth(f, 6, 8, 6, 10, 2); // water x 6..8, W' = 3
    for (int x = 9; x <= 12; ++x) {
        for (int y = 1; y <= 2; ++y) {
            for (int z = 6; z <= 10; ++z) {
                f.world.setBlock(x, y, z, f.stone); // edge tops out at y=3
            }
        }
    }
    Player p = f.makePlayer({9.5f, 3.0f, 8.5f}); // on the edge top = W'
    p.yaw = -kPi / 2.0f; // facing +X (away from the pool)
    Input walk = heldInput({GameKey::W});
    run(p, 30, walk); // 0.5 s
    EXPECT_TRUE(p.onGround());
    // ENT-7 re-pin: the ground pace (4.3) over 0.5 s less the start-up
    // ramp distance (the kGroundAccelRate 8 exponential approach).
    EXPECT_NEAR(p.position().x - 9.5f, 1.6049f, 0.001f);
}

TEST(PlayerTest, OneDeepSourcePoolFloats) {
    // A 1-deep L8 pool (a bucket-poured source, settled per FLUID-1): a
    // submerged body rises to the film top and floats — the accepted Q2
    // deviation (pinned here; if a depth gate is reconsidered, this
    // test changes).
    Fixture f;
    buildPoolDepth(f, 6, 10, 6, 10, 1); // W' = 2
    Player p = f.makePlayer({8.5f, 1.5f, 8.5f}); // feet G + 0.5 (G = 1)
    Input idle = heldInput({});
    run(p, 600, idle);
    EXPECT_FLOAT_EQ(p.position().y, 2.0f);
    EXPECT_EQ(p.velocity().y, 0.0f);
    EXPECT_FALSE(p.onGround());
}

// -- swimming in lava (Phase 54): the burn + the slow --------------------------

TEST(PlayerTest, LavaBurnsAtTheConfiguredRate) {
    Fixture f;
    buildPool(f, 6, 10, 6, 10, FluidKind::Lava);
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f}); // feet in the lava
    Input idle = heldInput({});
    // 1.5 s: exactly one 1.0 s burn tick (the 1/60 accumulation crosses
    // 1.0 at frame 61, the 2.0 crossing is frame ~121 — 90 frames is in
    // between). 20 - 4 = 16.
    run(p, 90, idle);
    EXPECT_FLOAT_EQ(p.health(), 16.0f);
    EXPECT_FALSE(p.isDead());
}

TEST(PlayerTest, LavaBurnKills) {
    Fixture f;
    buildPool(f, 6, 10, 6, 10, FluidKind::Lava);
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    Input idle = heldInput({});
    // 20 HP / 4 per tick = 5 ticks; the fifth crossing is frame ~301.
    run(p, 320, idle);
    EXPECT_TRUE(p.isDead());
    EXPECT_FLOAT_EQ(p.health(), 0.0f);
}

TEST(PlayerTest, LavaBurnResetsOnExit) {
    Fixture f;
    buildPool(f, 6, 10, 6, 10, FluidKind::Lava);
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    Input idle = heldInput({});
    run(p, 90, idle); // tick 1 (frame 61)
    p.restorePosition({2.5f, 1.0f, 8.5f}); // dry land: the pending tick resets
    run(p, 60, idle);
    EXPECT_FLOAT_EQ(p.health(), 16.0f); // no tick while out of the lava
    p.restorePosition({8.5f, 1.0f, 8.5f}); // back in: a fresh 1.0 s starts
    run(p, 59, idle); // 0.98 s: not yet
    EXPECT_FLOAT_EQ(p.health(), 16.0f);
    run(p, 5, idle); // crosses 1.0 s of fresh submersion
    EXPECT_FLOAT_EQ(p.health(), 12.0f);
}

TEST(PlayerTest, MovesSlowerInLavaThanWater) {
    Fixture f;
    buildPool(f, 6, 20, 6, 10, FluidKind::Lava);
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    Input idle = heldInput({});
    run(p, 5, idle);
    p.yaw = -kPi / 2.0f; // facing +X
    Input walk = heldInput({GameKey::W});
    const float x0 = p.position().x;
    run(p, 30, walk); // 0.5 s
    // lavaSwimSpeed (1.9) x 0.5 s — clearly below water (3.1 x 0.5 = 1.55).
    EXPECT_NEAR(p.position().x - x0, 0.95f, 0.1f);
    // One-frame exact pace: the velocity is the lava swim speed.
    p.restorePosition({8.5f, 1.0f, 8.5f});
    run(p, 1, walk);
    EXPECT_FLOAT_EQ(p.velocity().x, p.params().lavaSwimSpeed);
}

TEST(PlayerTest, SwimUpInLavaUsesTheLavaPace) {
    Fixture f;
    buildPool(f, 6, 10, 6, 10, FluidKind::Lava);
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    Input idle = heldInput({});
    run(p, 5, idle);
    Input up = heldInput({GameKey::Space});
    run(p, 1, up);
    EXPECT_FLOAT_EQ(p.velocity().y, p.params().lavaSwimUpSpeed);
}

TEST(PlayerTest, SprintSuppressedInLava) {
    Fixture f;
    buildPool(f, 6, 20, 6, 10, FluidKind::Lava);
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    Input idle = heldInput({});
    run(p, 5, idle);
    p.yaw = -kPi / 2.0f; // facing +X
    Input sprint = heldInput({GameKey::W, GameKey::LeftShift});
    const float x0 = p.position().x;
    run(p, 30, sprint); // 0.5 s: the sprint pace (5.6) must NOT apply
    EXPECT_NEAR(p.position().x - x0, 0.95f, 0.1f);
    EXPECT_FLOAT_EQ(p.velocity().x, p.params().lavaSwimSpeed);
}

TEST(PlayerTest, FallDamageCancelledInLava) {
    Fixture f;
    // A SHALLOW (1-deep) lava pool: the landing-to-run-end submersion stays
    // under one 1.0 s burn tick, so this isolates the fall cancellation
    // from the burn (a 2-deep pool's 1 s fall-through would add a tick).
    for (int x = 6; x <= 10; ++x) {
        for (int z = 6; z <= 10; ++z) {
            f.world.setFluid(x, 1, z, FluidKind::Lava, kMaxFluidLevel);
        }
    }
    Player p = f.makePlayer({8.5f, 12.0f, 8.5f}); // 10 blocks above the floor
    Input idle = heldInput({});
    run(p, 90, idle); // the fall settles on the pool floor (~frame 77)
    // A 10-block fall would deal 10 - 3 = 7 HP in the air; the lava
    // landing cancels it (like water).
    EXPECT_FLOAT_EQ(p.health(), 20.0f);
    EXPECT_TRUE(p.onGround());
    EXPECT_FLOAT_EQ(p.position().y, 1.0f);
}

// -- movement direction / speed ----------------------------------------------

TEST(PlayerTest, MovementDirectionFollowsYaw) {
    Fixture f;
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    Input idle = heldInput({});
    run(p, 5, idle);

    // yaw = 0 faces -Z.
    p.yaw = 0.0f;
    const float z0 = p.position().z;
    const float x0 = p.position().x;
    Input walk = heldInput({GameKey::W});
    run(p, 10, walk);
    EXPECT_LT(p.position().z, z0);
    EXPECT_FLOAT_EQ(p.position().x, x0);

    // yaw = +90 deg faces -X.
    p.yaw = kPi / 2.0f;
    const float x1 = p.position().x;
    run(p, 10, walk);
    EXPECT_LT(p.position().x, x1);
}

TEST(PlayerTest, SprintIsFasterThanWalk) {
    Fixture f;
    Player a = f.makePlayer({0.5f, 1.0f, 0.5f});
    Player b = f.makePlayer({0.5f, 1.0f, 0.5f});
    Input idle = heldInput({});
    run(a, 5, idle);
    run(b, 5, idle);
    Input walk = heldInput({GameKey::W});
    Input sprint = heldInput({GameKey::W, GameKey::LeftShift});
    run(a, 60, walk);
    run(b, 60, sprint);
    // Both start at z = 0.5 and walk -Z for 1 s.
    const float aMove = 0.5f - a.position().z;
    const float bMove = 0.5f - b.position().z;
    EXPECT_GT(bMove, aMove);
    // ENT-7 re-pin: the 1 s distances (walk 4.3 / sprint 5.6 targets)
    // less the start-up ramp (the kGroundAccelRate 8 approach — the
    // sprint keeps its advantage over the full second).
    EXPECT_NEAR(aMove, 3.7977f, 0.001f); // walk speed
    EXPECT_NEAR(bMove, 4.9458f, 0.001f); // sprint speed
}

TEST(PlayerTest, MouseLookTurnsAndClampsPitch) {
    Fixture f;
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    // lookEnabled = true: the pointer is captured.
    Input input = heldInput({});
    input.frameBegin();
    // 100 px right, 50 px up, at default sensitivity 0.0025 rad/px.
    input.handleMouseMotion(100.0f, -50.0f, 0.0f, 0.0f);
    p.update(kDt, input, true);
    EXPECT_NEAR(p.yaw, -0.25f, 1e-4f);
    EXPECT_NEAR(p.pitch, 0.125f, 1e-4f);

    // Far more than 90 deg up: the pitch must clamp to ~89 deg.
    input.frameBegin();
    input.handleMouseMotion(0.0f, -10000.0f, 0.0f, 0.0f);
    p.update(kDt, input, true);
    EXPECT_GT(p.pitch, 1.5f);
    EXPECT_LT(p.pitch, 1.6f);

    // lookEnabled = false: the same motion must NOT rotate the view.
    const float yawBefore = p.yaw;
    const float pitchBefore = p.pitch;
    input.frameBegin();
    input.handleMouseMotion(100.0f, -50.0f, 0.0f, 0.0f);
    p.update(kDt, input, false);
    EXPECT_FLOAT_EQ(p.yaw, yawBefore);
    EXPECT_FLOAT_EQ(p.pitch, pitchBefore);
}

// -- world bounds ------------------------------------------------------------

TEST(PlayerTest, VoidFallKillsPlayer) {
    // No floor at all: the player falls through the world's bottom. Phase
    // 16: the kill plane is DEATH (the old respawn-teleport is gone). One
    // full fall (10 -> kill plane at -32) takes ~102 ticks.
    BlockRegistry blocks = makeBlocks();
    World world;
    Player p(world, blocks, {8.5f, 10.0f, 8.5f});
    Input idle = heldInput({});
    int deathTick = -1;
    float minY = 1e30f;
    for (int i = 0; i < 200; ++i) {
        idle.frameBegin();
        p.update(kDt, idle, false);
        minY = std::min(minY, p.position().y);
        if (deathTick < 0 && p.isDead()) {
            deathTick = i;
        }
    }
    EXPECT_GE(deathTick, 0); // died below the kill plane
    // The kill plane triggers within one tick of terminal-velocity travel.
    EXPECT_GT(minY, -33.0f);
    EXPECT_FLOAT_EQ(p.health(), 0.0f);
    // A dead player is frozen: update() no-ops (the game re-enters via
    // respawn() on the R key).
    const Vec3 deadPos = p.position();
    run(p, 10, idle);
    EXPECT_EQ(p.position(), deadPos);
    // Respawn: back at the spawn point with full health, falling again.
    p.respawn();
    EXPECT_FALSE(p.isDead());
    EXPECT_FLOAT_EQ(p.health(), p.maxHealth());
    EXPECT_FLOAT_EQ(p.position().y, 10.0f);
    run(p, 10, idle);
    EXPECT_LT(p.position().y, 10.0f);
    EXPECT_LT(p.velocity().y, 0.0f);
}

// -- survival (Phase 16): fall damage, death, regen, crouch, step-up --------

TEST(PlayerTest, FallDamageOneHpPerBlockAboveThree) {
    Fixture f;
    // A 4-block platform at x,z in [8, 10): top at y=5, the floor below at
    // y=1 -> a 4-block fall. Minecraft's rule: the first 3 blocks are safe,
    // 1 HP per block above. (The net drop is ~3.99 because the leave-ground
    // frame already includes one tick of gravity.)
    for (int x = 8; x < 10; ++x) {
        for (int z = 8; z < 10; ++z) {
            for (int y = 1; y < 5; ++y) {
                f.world.setBlock(x, y, z, f.stone);
            }
        }
    }
    Player p = f.makePlayer({8.5f, 5.0f, 8.5f});
    p.yaw = -kPi / 2.0f; // facing +X (off the platform edge)
    Input walk = heldInput({GameKey::W});
    run(p, 90, walk); // ~25 ticks to the edge, then the 4-block fall
    EXPECT_FLOAT_EQ(p.position().y, 1.0f); // landed on the floor
    EXPECT_TRUE(p.onGround());
    EXPECT_LT(p.health(), p.maxHealth());  // took damage
    EXPECT_GT(p.health(), p.maxHealth() - 1.1f); // ~1 HP (4-block fall)
    EXPECT_FALSE(p.isDead());
}

TEST(PlayerTest, FallDamageNoneWithinThreeBlocks) {
    Fixture f;
    // A 3-block platform: top at y=4 -> a 3-block fall (net ~2.99), which
    // is within the safe threshold: no damage at all.
    for (int x = 8; x < 10; ++x) {
        for (int z = 8; z < 10; ++z) {
            for (int y = 1; y < 4; ++y) {
                f.world.setBlock(x, y, z, f.stone);
            }
        }
    }
    Player p = f.makePlayer({8.5f, 4.0f, 8.5f});
    p.yaw = -kPi / 2.0f;
    Input walk = heldInput({GameKey::W});
    run(p, 90, walk);
    EXPECT_FLOAT_EQ(p.position().y, 1.0f);
    EXPECT_FLOAT_EQ(p.health(), p.maxHealth()); // untouched
}

// -- save restore (Phase 36): fall tracking is pinned on load --------------

TEST(PlayerTest, RestorePositionMidAirTakesFallDamage) {
    Fixture f;
    // A save taken mid-air: the restore puts the player 10 blocks above the
    // floor. Without the fall-tracking pin (Phase 36), the missing
    // leave-the-ground transition on load leaves m_fallStartY at its
    // initial 0 and the 10-block fall lands damage-free.
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    p.restorePosition({8.5f, 11.0f, 8.5f});
    Input idle = heldInput({});
    run(p, 120, idle); // a 10-block fall takes < 1 s
    EXPECT_FLOAT_EQ(p.position().y, 1.0f); // landed on the floor
    EXPECT_TRUE(p.onGround());
    // Net drop exactly 10.0 (restore and landing both on integer planes)
    // -> 10 - 3 (fallDamageOffset) = 7 HP.
    EXPECT_FLOAT_EQ(p.health(), p.maxHealth() - 7.0f);
}

TEST(PlayerTest, RestorePositionAtRestTakesNoDamage) {
    Fixture f;
    // The save was taken standing on the floor: the landing tick after the
    // restore sees a zero net drop (the pin equals the landing plane), so
    // the re-pin must not invent damage.
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    p.restorePosition({8.5f, 1.0f, 8.5f});
    Input idle = heldInput({});
    run(p, 120, idle);
    EXPECT_FLOAT_EQ(p.position().y, 1.0f);
    EXPECT_TRUE(p.onGround());
    EXPECT_FLOAT_EQ(p.health(), p.maxHealth()); // untouched
}

TEST(PlayerTest, FallDamageClearedByWater) {
    Fixture f;
    // A 5-block platform (top y=6) over a water-filled pit: the landing is
    // in water, which cancels the fall damage (Minecraft's rule). The pool
    // is a FLUID level (isInWater reads the fluid layer, not the block).
    for (int x = 8; x < 20; ++x) {
        for (int z = 8; z < 10; ++z) {
            f.world.setFluid(x, 1, z, FluidKind::Water, kMaxFluidLevel); // pool: top at y=2
        }
    }
    for (int x = 8; x < 10; ++x) {
        for (int z = 8; z < 10; ++z) {
            for (int y = 2; y < 6; ++y) {
                f.world.setBlock(x, y, z, f.stone);
            }
        }
    }
    Player p = f.makePlayer({8.5f, 6.0f, 8.5f});
    p.yaw = -kPi / 2.0f;
    Input walk = heldInput({GameKey::W});
    run(p, 90, walk); // walk off the platform; the water slows the sink
    // Idle: the swim physics keeps the player on the pool floor (holding
    // W would swim it out toward the floor's edge).
    Input idle = heldInput({});
    run(p, 120, idle);
    EXPECT_FLOAT_EQ(p.position().y, 1.0f); // on the pool floor
    EXPECT_TRUE(p.onGround());
    EXPECT_FLOAT_EQ(p.health(), p.maxHealth()); // water cleared the fall
}

TEST(PlayerTest, LethalDamageKillsAndRespawnRestores) {
    Fixture f;
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    Input idle = heldInput({});
    EXPECT_TRUE(p.damage(19.0f));
    EXPECT_FLOAT_EQ(p.health(), 1.0f);
    EXPECT_FALSE(p.isDead());
    EXPECT_TRUE(p.damage(1.0f));
    EXPECT_FLOAT_EQ(p.health(), 0.0f);
    EXPECT_TRUE(p.isDead());
    // Dead: further damage is a no-op and the body is frozen.
    EXPECT_FALSE(p.damage(5.0f));
    const Vec3 deadPos = p.position();
    run(p, 10, idle);
    EXPECT_EQ(p.position(), deadPos);
    // Respawn: spawn point, full health, alive, and moving again.
    p.respawn();
    EXPECT_FALSE(p.isDead());
    EXPECT_FLOAT_EQ(p.health(), p.maxHealth());
    EXPECT_FLOAT_EQ(p.position().x, 8.5f);
    EXPECT_FLOAT_EQ(p.position().y, 1.0f);
    EXPECT_FLOAT_EQ(p.position().z, 8.5f);
    p.yaw = -kPi / 2.0f;
    Input walk = heldInput({GameKey::W});
    run(p, 10, walk);
    EXPECT_GT(p.position().x, 8.5f);
}

TEST(PlayerTest, RegenAfterDelay) {
    Fixture f;
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    Input idle = heldInput({});
    p.damage(1.0f); // health 19, the damage clock restarts
    // Before the 10 s delay: no regen.
    run(p, 5 * 60, idle);
    EXPECT_FLOAT_EQ(p.health(), 19.0f);
    // After the delay: +1 HP every 4 s, capped at max. 14 s covers two
    // intervals: the first restores 19 -> 20 (SURV-9: it COSTS one
    // hunger point — the buffer is empty here, a fresh player, so the
    // bar pays 20 -> 19); the second finds full health and elapses
    // free (no regen, no cost).
    run(p, 14 * 60, idle);
    EXPECT_FLOAT_EQ(p.health(), p.maxHealth());
    EXPECT_FLOAT_EQ(p.hunger(), 19.0f);
    EXPECT_FLOAT_EQ(p.saturation(), 0.0f);
}

// SURV-9: the regen cost goes through the standard drain — the hidden
// saturation buffer FIRST, the bar only when the buffer is empty.
TEST(PlayerTest, RegenCostsSaturationBeforeTheBar) {
    Fixture f;
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    Input idle = heldInput({});
    p.damage(2.0f); // health 18
    p.setSaturation(6.0f);
    // 10 s delay + one 4 s interval: +1 HP, the buffer pays 6 -> 5.
    // 15 s = the 14 s boundary plus 1 s of margin (the 1/60 double
    // accumulation sits just under 4.0 at exactly 840 frames).
    run(p, 15 * 60, idle);
    EXPECT_FLOAT_EQ(p.health(), 19.0f);
    EXPECT_FLOAT_EQ(p.saturation(), 5.0f);
    EXPECT_FLOAT_EQ(p.hunger(), 20.0f); // the bar untouched
}

TEST(PlayerTest, RegenAtFullHealthCostsNothing) {
    Fixture f;
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    Input idle = heldInput({});
    p.setSaturation(6.0f); // full health, never damaged
    // 20 s: the regen delay passes and five intervals elapse, but at
    // full health no half heart is regenerated -> no cost (20 s < the
    // 36 s decay tick, so the bar is untouched as well).
    run(p, 20 * 60, idle);
    EXPECT_FLOAT_EQ(p.health(), p.maxHealth());
    EXPECT_FLOAT_EQ(p.saturation(), 6.0f);
    EXPECT_FLOAT_EQ(p.hunger(), 20.0f);
}

TEST(PlayerTest, RegenGateClosesWhenTheCostDrainsTheBar) {
    Fixture f;
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    Input idle = heldInput({});
    p.damage(1.0f); // health 19
    p.setHunger(6.0f); // exactly at the regen threshold (6)
    // One interval: +1 HP (capped at 20), the bar pays 6 -> 5 = below
    // the gate -> no further regen (the Minecraft rule: eating is what
    // re-opens it). 15 s = the 14 s boundary + 1 s of margin (the 1/60
    // double accumulation).
    run(p, 15 * 60, idle);
    EXPECT_FLOAT_EQ(p.health(), p.maxHealth());
    EXPECT_FLOAT_EQ(p.hunger(), 5.0f);
    // Plenty more time: no HP left to regain; one 36 s decay tick
    // (5 -> 4) and nothing else.
    run(p, 40 * 60, idle);
    EXPECT_FLOAT_EQ(p.health(), p.maxHealth());
    EXPECT_FLOAT_EQ(p.hunger(), 4.0f);
}

TEST(PlayerTest, HealAddsAndClampsAtMax) {
    // Phase 46: the regeneration effect's heal entry point (the natural
    // hunger-based regen is separate — see RegenAfterDelay).
    Fixture f;
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    p.damage(5.0f); // health 15
    p.heal(2.0f);
    EXPECT_FLOAT_EQ(p.health(), 17.0f);
    // Clamped at max health (a heal cannot exceed it).
    p.heal(100.0f);
    EXPECT_FLOAT_EQ(p.health(), p.maxHealth());
}

// -- hunger (Phase 23) -------------------------------------------------------

// Note on the timings below: the accumulators add the double 1/60 per
// frame, so the first 36 s boundary lands on frame 2161 (the 2160-frame
// sum is a hair under 36.0) and every later one follows 2160 frames
// after; the 20 s starvation boundary lands on frame 1200 (the 1200-frame
// sum a hair over 20.0). The asserts pick windows with many seconds of
// slack on each side — one frame is trivial at this pace.

TEST(PlayerTest, HungerDecaysEvery36Seconds) {
    Fixture f;
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    Input idle = heldInput({});
    ASSERT_FLOAT_EQ(p.hunger(), p.maxHunger()); // fresh player: full
    // 95 s: two 36 s decay ticks have passed (the 3rd is ~13 s away): 18.
    run(p, 95 * 60, idle);
    EXPECT_FLOAT_EQ(p.hunger(), 18.0f);
    // 25 s more: the 3rd tick has passed (the 4th is ~24 s away): 17.
    run(p, 25 * 60, idle);
    EXPECT_FLOAT_EQ(p.hunger(), 17.0f);
}

TEST(PlayerTest, StarvationDamagesAtZeroHunger) {
    Fixture f;
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    Input idle = heldInput({});
    p.setHunger(0.0f);
    // At zero hunger: 1 HP per 20 s through the normal damage path.
    run(p, 25 * 60, idle); // the first tick (20 s) has passed, the second 15 s away
    EXPECT_FLOAT_EQ(p.health(), 19.0f);
    EXPECT_FALSE(p.isDead());
    run(p, 20 * 60, idle); // the second tick (40 s) has passed
    EXPECT_FLOAT_EQ(p.health(), 18.0f);
}

TEST(PlayerTest, StarvationKillsAtZeroHunger) {
    Fixture f;
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    Input idle = heldInput({});
    p.setHunger(0.0f);
    // 20 points of starvation: the 20th tick lands at 400 s (frame 24000).
    run(p, 405 * 60, idle);
    EXPECT_TRUE(p.isDead());
    EXPECT_FLOAT_EQ(p.health(), 0.0f);
}

TEST(PlayerTest, LowHungerBlocksRegen) {
    Fixture f;
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    Input idle = heldInput({});
    p.damage(1.0f); // health 19, the damage clock restarts
    p.setHunger(5.0f); // below the regen threshold (6)
    // 50 s: the regen delay (10 s) passes, but the hunger gate never does —
    // no HP comes back (one 36 s decay tick drops hunger 5 -> 4, never starving).
    run(p, 50 * 60, idle);
    EXPECT_FLOAT_EQ(p.health(), 19.0f);
    EXPECT_FLOAT_EQ(p.hunger(), 4.0f);
}

TEST(PlayerTest, RestoreHungerClampsToMax) {
    Fixture f;
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    p.setHunger(18.0f);
    p.restoreHunger(5.0f); // over the top: clamps to the max
    EXPECT_FLOAT_EQ(p.hunger(), p.maxHunger());
    p.setHunger(0.0f);
    p.restoreHunger(4.0f);
    EXPECT_FLOAT_EQ(p.hunger(), 4.0f);
}

TEST(PlayerTest, RespawnRestoresHunger) {
    Fixture f;
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    Input idle = heldInput({});
    p.setHunger(3.0f);
    p.damage(20.0f); // lethal
    ASSERT_TRUE(p.isDead());
    p.respawn();
    EXPECT_FLOAT_EQ(p.hunger(), p.maxHunger());
    // The decay clock restarted: no instant drop.
    run(p, 3 * 60, idle);
    EXPECT_FLOAT_EQ(p.hunger(), p.maxHunger());
}

// -- saturation (Phase 28) ------------------------------------------------------

// Note on the timings: same 1/60-frame accumulator convention as the
// hunger tests above (first 36 s boundary at frame 2161); the asserts
// pick windows with many seconds of slack.

TEST(PlayerTest, SaturationAbsorbsDecayBeforeHunger) {
    Fixture f;
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    Input idle = heldInput({});
    p.setHunger(20.0f);
    p.setSaturation(5.0f);
    // 190 s: five 36 s decay ticks — each one drains 1 of the saturation
    // buffer instead of the hunger bar: saturation 5 -> 0, hunger stays
    // at the max (the 6th tick is ~26 s away).
    run(p, 190 * 60, idle);
    EXPECT_FLOAT_EQ(p.saturation(), 0.0f);
    EXPECT_FLOAT_EQ(p.hunger(), 20.0f);
    // 40 s more: the 6th tick has passed and now hits the hunger bar.
    run(p, 40 * 60, idle);
    EXPECT_FLOAT_EQ(p.hunger(), 19.0f);
    EXPECT_FLOAT_EQ(p.saturation(), 0.0f);
}

TEST(PlayerTest, SprintDoublesTheHungerDecay) {
    // The sprint state is the input condition (Shift + moving wish, out
    // of water) — the same condition that selects sprintSpeed — so a
    // player sprinting INTO a wall still counts as sprinting. A wall in
    // front keeps the body on the fixture floor for the whole 75 s.
    Fixture f;
    f.wallX(9, 1, 4, 8, 10); // right in front of the player, facing +X
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    p.yaw = -kPi / 2.0f; // +X, into the wall
    Input sprint = heldInput({GameKey::W, GameKey::LeftShift});
    p.setHunger(20.0f);
    p.setSaturation(0.0f);
    // Sprinting runs the decay at 2x pace (a tick every 18 s): 75 s ->
    // four ticks -> 16 (the 5th is ~15 s away).
    run(p, 75 * 60, sprint);
    EXPECT_FLOAT_EQ(p.hunger(), 16.0f);
    // Control: the same 75 s standing still is only two 36 s ticks -> 18.
    Player q = f.makePlayer({8.5f, 1.0f, 8.5f});
    Input idle = heldInput({});
    q.setHunger(20.0f);
    run(q, 75 * 60, idle);
    EXPECT_FLOAT_EQ(q.hunger(), 18.0f);
}

TEST(PlayerTest, SaturationRestoresAndClampsToMax) {
    Fixture f;
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    p.setSaturation(19.0f);
    p.restoreSaturation(5.0f); // over the top: clamps to the max (20)
    EXPECT_FLOAT_EQ(p.saturation(), p.maxHunger());
    p.setSaturation(0.0f);
    p.restoreSaturation(3.5f);
    EXPECT_FLOAT_EQ(p.saturation(), 3.5f);
}

TEST(PlayerTest, RespawnRestoresSaturation) {
    // A respawn is a full meal: the full bar plus the full hidden buffer.
    Fixture f;
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    p.setSaturation(0.0f);
    p.damage(20.0f); // lethal
    ASSERT_TRUE(p.isDead());
    p.respawn();
    EXPECT_FLOAT_EQ(p.saturation(), p.maxHunger());
}

TEST(PlayerTest, CrouchShrinksHeightAndScalesSpeed) {
    Fixture f;
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    Input idle = heldInput({});
    run(p, 5, idle); // settle
    ASSERT_FLOAT_EQ(p.aabbMax().y - p.position().y, 1.8f); // standing
    p.yaw = -kPi / 2.0f;
    Input crouch = heldInput({GameKey::W, GameKey::LeftControl});
    run(p, 30, crouch); // 0.5 s of crouch-walking
    EXPECT_TRUE(p.isCrouching());
    EXPECT_FLOAT_EQ(p.aabbMax().y - p.position().y, 1.6f); // crouched
    EXPECT_FLOAT_EQ(p.position().y, 1.0f); // the feet never move
    // Crouch-walk: 0.3 x 4.3 x 0.5 s ~= 0.65 blocks (walking: 2.15).
    EXPECT_GT(p.position().x, 8.5f);
    EXPECT_LT(p.position().x, 9.2f);
    // Release the crouch (headroom is fine): back to standing height.
    run(p, 5, idle);
    EXPECT_FALSE(p.isCrouching());
    EXPECT_FLOAT_EQ(p.aabbMax().y - p.position().y, 1.8f);
}

TEST(PlayerTest, CrouchStaysOnLedge) {
    Fixture f;
    // Remove the floor east of x=0 so the player can reach an edge.
    for (int x = 0; x < 16; ++x) {
        for (int z = -16; z < 16; ++z) {
            f.world.setBlock(x, 0, z, BlockID::air());
        }
    }
    Player p = f.makePlayer({-0.5f, 1.0f, 8.5f});
    Input idle = heldInput({});
    run(p, 5, idle);
    p.yaw = -kPi / 2.0f; // +X, toward the edge at x=0
    Input crouchWalk = heldInput({GameKey::W, GameKey::LeftControl});
    run(p, 180, crouchWalk); // 3 s of crouch-walking at the edge
    // Held at the edge: the AABB face never crosses x=0 and the player
    // stays on the ground (a non-crouching walk would have gone over).
    EXPECT_LE(p.position().x + 0.3f, 1e-3f);
    EXPECT_GE(p.position().x, -0.5f);
    EXPECT_TRUE(p.onGround());
}

TEST(PlayerTest, CrouchDoesNotStepUp) {
    Fixture f;
    // One-block step at x in [10, 20): crouching disables the auto step.
    for (int x = 10; x < 20; ++x) {
        for (int z = 8; z < 10; ++z) {
            f.world.setBlock(x, 1, z, f.stone);
        }
    }
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    p.yaw = -kPi / 2.0f;
    Input crouchWalk = heldInput({GameKey::W, GameKey::LeftControl});
    run(p, 180, crouchWalk);
    EXPECT_FLOAT_EQ(p.position().x, 10.0f - 0.3f); // clamped at the step
    EXPECT_FLOAT_EQ(p.position().y, 1.0f);        // never stepped up
}

TEST(PlayerTest, StepUpBlockedByLowHeadroom) {
    Fixture f;
    // One-block step with a ceiling two cells up: the lifted player would
    // hit it, so no step (the player stays clamped at the step face).
    for (int x = 10; x < 20; ++x) {
        for (int z = 8; z < 10; ++z) {
            f.world.setBlock(x, 1, z, f.stone); // the step
            f.world.setBlock(x, 3, z, f.stone); // the ceiling
        }
    }
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    p.yaw = -kPi / 2.0f;
    Input walk = heldInput({GameKey::W});
    run(p, 120, walk);
    EXPECT_FLOAT_EQ(p.position().x, 10.0f - 0.3f);
    EXPECT_FLOAT_EQ(p.position().y, 1.0f);
}

TEST(PlayerTest, HighSpeedFallDoesNotTunnelThroughFloor) {
    Fixture f;
    Player p = f.makePlayer({8.5f, 10.0f, 8.5f});
    Input idle = heldInput({});
    // Build up fall speed (~0.73 s: y ~= 1.2, v ~= -23.5), then take one
    // large 0.25 s tick: ~7.7 blocks of motion, enough to tunnel straight
    // through the 1-block floor in a single naive step.
    run(p, 44, idle);
    ASSERT_LT(p.position().y, 1.5f);
    ASSERT_GT(p.position().y, 1.0f); // still above the floor
    p.update(0.25, idle, false);
    EXPECT_FLOAT_EQ(p.position().y, 1.0f); // landed, not tunneled through
    EXPECT_TRUE(p.onGround());
}

// -- spawn room ----------------------------------------------------------------

TEST(PlayerTest, HasRoomAtChecksSolidBlocks) {
    Fixture f;
    const float w = Player::Params {}.width;
    const float h = Player::Params {}.height;
    EXPECT_TRUE(Player::hasRoomAt(f.world, f.blocks, {8.5f, 1.0f, 8.5f}, w, h));
    // A solid block inside the spawn box (y=2 is within [1, 1+1.8)) blocks it.
    f.world.setBlock(8, 2, 8, f.stone);
    EXPECT_FALSE(Player::hasRoomAt(f.world, f.blocks, {8.5f, 1.0f, 8.5f}, w, h));
    // A water block does not (non-solid).
    f.world.setBlock(8, 2, 8, f.water);
    EXPECT_TRUE(Player::hasRoomAt(f.world, f.blocks, {8.5f, 1.0f, 8.5f}, w, h));
}

// -- config params (Phase 32) ---------------------------------------------------

// The Config getters silently fall back to the default on a type mismatch,
// so fromConfig must read the raw values: every present key below is either
// a JSON number (accepted, range-checked) or a string (rejected).

TEST(PlayerParamsConfigTest, DefaultsWhenNoPlayerSection) {
    Config cfg;
    auto r = Player::Params::fromConfig(cfg);
    ASSERT_TRUE(r.ok()) << r.error().describe();
    EXPECT_EQ(*r, Player::Params {});
}

TEST(PlayerParamsConfigTest, ExplicitValuesOverride) {
    Config cfg;
    cfg.set("player.width", Json(0.5));
    cfg.set("player.height", Json(1.9));
    cfg.set("player.eye_height", Json(1.7));
    cfg.set("player.walk_speed", Json(4.0));
    cfg.set("player.sprint_speed", Json(6.0));
    cfg.set("player.jump_speed", Json(9.0));
    cfg.set("player.gravity", Json(30.0));
    cfg.set("player.terminal_velocity", Json(40.0));
    cfg.set("player.swim_speed", Json(3.0));
    cfg.set("player.swim_up_speed", Json(4.0));
    cfg.set("player.water_buoyancy", Json(10.0));
    cfg.set("player.water_rise_speed", Json(1.2));
    cfg.set("player.water_sink_speed", Json(1.3));
    cfg.set("player.water_terminal_velocity", Json(3.0));
    cfg.set("player.lava_swim_speed", Json(1.5));
    cfg.set("player.lava_swim_up_speed", Json(1.0));
    cfg.set("player.lava_gravity", Json(3.0));
    cfg.set("player.lava_terminal_velocity", Json(1.5));
    cfg.set("player.mouse_sensitivity", Json(0.002));
    cfg.set("player.max_health", Json(30.0));
    cfg.set("player.fall_damage_offset", Json(2.0));
    cfg.set("player.crouch_height", Json(1.5));
    cfg.set("player.crouch_speed_scale", Json(0.4));
    cfg.set("player.regen_delay_seconds", Json(8.0));
    cfg.set("player.regen_interval_seconds", Json(3.0));
    cfg.set("player.max_hunger", Json(24.0));
    cfg.set("player.hunger_decay_seconds", Json(12.0));
    cfg.set("player.starve_damage_interval_seconds", Json(10.0));
    cfg.set("player.regen_hunger_threshold", Json(8.0));
    cfg.set("player.sprint_hunger_decay_factor", Json(3.0));
    cfg.set("player.lava_damage_interval_seconds", Json(2.0));
    cfg.set("player.lava_damage", Json(6.0));
    auto r = Player::Params::fromConfig(cfg);
    ASSERT_TRUE(r.ok()) << r.error().describe();
    const Player::Params& p = *r;
    EXPECT_FLOAT_EQ(p.width, 0.5f);
    EXPECT_FLOAT_EQ(p.height, 1.9f);
    EXPECT_FLOAT_EQ(p.eyeHeight, 1.7f);
    EXPECT_FLOAT_EQ(p.walkSpeed, 4.0f);
    EXPECT_FLOAT_EQ(p.sprintSpeed, 6.0f);
    EXPECT_FLOAT_EQ(p.jumpSpeed, 9.0f);
    EXPECT_FLOAT_EQ(p.gravity, 30.0f);
    EXPECT_FLOAT_EQ(p.terminalVelocity, 40.0f);
    EXPECT_FLOAT_EQ(p.swimSpeed, 3.0f);
    EXPECT_FLOAT_EQ(p.swimUpSpeed, 4.0f);
    EXPECT_FLOAT_EQ(p.waterBuoyancy, 10.0f);
    EXPECT_FLOAT_EQ(p.waterRiseSpeed, 1.2f);
    EXPECT_FLOAT_EQ(p.waterSinkSpeed, 1.3f);
    EXPECT_FLOAT_EQ(p.waterTerminalVelocity, 3.0f);
    EXPECT_FLOAT_EQ(p.lavaSwimSpeed, 1.5f);
    EXPECT_FLOAT_EQ(p.lavaSwimUpSpeed, 1.0f);
    EXPECT_FLOAT_EQ(p.lavaGravity, 3.0f);
    EXPECT_FLOAT_EQ(p.lavaTerminalVelocity, 1.5f);
    EXPECT_FLOAT_EQ(p.mouseSensitivity, 0.002f);
    EXPECT_FLOAT_EQ(p.maxHealth, 30.0f);
    EXPECT_FLOAT_EQ(p.fallDamageOffset, 2.0f);
    EXPECT_FLOAT_EQ(p.crouchHeight, 1.5f);
    EXPECT_FLOAT_EQ(p.crouchSpeedScale, 0.4f);
    EXPECT_DOUBLE_EQ(p.regenDelaySeconds, 8.0);
    EXPECT_DOUBLE_EQ(p.regenIntervalSeconds, 3.0);
    EXPECT_FLOAT_EQ(p.maxHunger, 24.0f);
    EXPECT_DOUBLE_EQ(p.hungerDecaySeconds, 12.0);
    EXPECT_DOUBLE_EQ(p.starveDamageIntervalSeconds, 10.0);
    EXPECT_FLOAT_EQ(p.regenHungerThreshold, 8.0f);
    EXPECT_DOUBLE_EQ(p.sprintHungerDecayFactor, 3.0);
    EXPECT_DOUBLE_EQ(p.lavaDamageIntervalSeconds, 2.0);
    EXPECT_FLOAT_EQ(p.lavaDamage, 6.0f);
}

TEST(PlayerParamsConfigTest, MissingKeysKeepPerKeyDefaults) {
    Config cfg;
    cfg.set("player.hunger_decay_seconds", Json(12.0));
    cfg.set("player.max_health", Json(24.0));
    auto r = Player::Params::fromConfig(cfg);
    ASSERT_TRUE(r.ok()) << r.error().describe();
    const Player::Params d; // the struct defaults
    EXPECT_DOUBLE_EQ(r->hungerDecaySeconds, 12.0);
    EXPECT_FLOAT_EQ(r->maxHealth, 24.0f);
    // Everything else is untouched.
    EXPECT_DOUBLE_EQ(r->starveDamageIntervalSeconds, d.starveDamageIntervalSeconds);
    EXPECT_FLOAT_EQ(r->maxHunger, d.maxHunger);
    EXPECT_FLOAT_EQ(r->walkSpeed, d.walkSpeed);
    EXPECT_DOUBLE_EQ(r->sprintHungerDecayFactor, d.sprintHungerDecayFactor);
}

TEST(PlayerParamsConfigTest, RejectsNonNumericValues) {
    Config cfg;
    cfg.set("player.max_health", Json("20")); // a string, not a number
    auto r = Player::Params::fromConfig(cfg);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("player.max_health"), std::string::npos);
}

TEST(PlayerParamsConfigTest, RejectsZeroAndNegative) {
    Config zero;
    zero.set("player.hunger_decay_seconds", Json(0));
    auto r0 = Player::Params::fromConfig(zero);
    ASSERT_FALSE(r0.ok());
    EXPECT_NE(r0.error().message.find("player.hunger_decay_seconds"),
              std::string::npos);

    Config negative;
    negative.set("player.walk_speed", Json(-4.0));
    auto rn = Player::Params::fromConfig(negative);
    ASSERT_FALSE(rn.ok());
    EXPECT_NE(rn.error().message.find("player.walk_speed"), std::string::npos);
}

TEST(PlayerParamsConfigTest, AllowsZeroFallDamageOffsetAndThreshold) {
    Config cfg;
    cfg.set("player.fall_damage_offset", Json(0));
    cfg.set("player.regen_hunger_threshold", Json(0));
    auto r = Player::Params::fromConfig(cfg);
    ASSERT_TRUE(r.ok()) << r.error().describe();
    EXPECT_FLOAT_EQ(r->fallDamageOffset, 0.0f);
    EXPECT_FLOAT_EQ(r->regenHungerThreshold, 0.0f);
}

TEST(PlayerParamsConfigTest, RejectsEyeHeightAtOrAboveHeight) {
    Config equal;
    equal.set("player.eye_height", Json(1.8)); // == the default height
    EXPECT_FALSE(Player::Params::fromConfig(equal).ok());
    Config above;
    above.set("player.eye_height", Json(2.0));
    auto r = Player::Params::fromConfig(above);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("eye_height"), std::string::npos);
}

TEST(PlayerParamsConfigTest, RejectsCrouchHeightAtOrAboveHeight) {
    Config cfg;
    cfg.set("player.crouch_height", Json(1.8)); // == the default height
    auto r = Player::Params::fromConfig(cfg);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("crouch_height"), std::string::npos);
}

TEST(PlayerParamsConfigTest, RejectsThresholdAboveMaxHunger) {
    Config above;
    above.set("player.regen_hunger_threshold", Json(21.0)); // > the default max
    auto r = Player::Params::fromConfig(above);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("regen_hunger_threshold"),
              std::string::npos);
    // Equal to the max is allowed (regen never fires, but is not broken).
    Config equal;
    equal.set("player.regen_hunger_threshold", Json(20.0));
    EXPECT_TRUE(Player::Params::fromConfig(equal).ok());
}

TEST(PlayerParamsConfigTest, RejectsZeroLavaDamageInterval) {
    Config cfg;
    cfg.set("player.lava_damage_interval_seconds", Json(0.0));
    auto r = Player::Params::fromConfig(cfg);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("lava_damage_interval_seconds"),
              std::string::npos);
    // A negative lava swim speed is rejected too (the generic positive
    // check on the movement keys).
    Config neg;
    neg.set("player.lava_swim_speed", Json(-1.0));
    auto rn = Player::Params::fromConfig(neg);
    ASSERT_FALSE(rn.ok());
    EXPECT_NE(rn.error().message.find("lava_swim_speed"),
              std::string::npos);
}

TEST(PlayerParamsConfigTest, RejectsLavaDamageAboveMaxHealth) {
    Config above;
    above.set("player.lava_damage", Json(21.0)); // > the default max_health
    auto r = Player::Params::fromConfig(above);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("lava_damage"), std::string::npos);
    // Equal to the max is allowed (one tick is lethal, but is not broken).
    Config equal;
    equal.set("player.lava_damage", Json(20.0));
    EXPECT_TRUE(Player::Params::fromConfig(equal).ok());
}

// -- Phase 57: the fire duration -------------------------------------------

TEST(PlayerParamsConfigTest, FireDurationDefaultsToEight) {
    // Minecraft's burn duration on Normal (8 s).
    Config cfg;
    auto r = Player::Params::fromConfig(cfg);
    ASSERT_TRUE(r.ok()) << r.error().describe();
    EXPECT_DOUBLE_EQ(r->fireDurationSeconds, 8.0);
}

TEST(PlayerParamsConfigTest, FireDurationRange) {
    // (0, 100]: 0 is rejected (the generic positive check), >100 by the
    // cross-field check.
    Config zero;
    zero.set("player.fire_duration_seconds", Json(0.0));
    EXPECT_FALSE(Player::Params::fromConfig(zero).ok());

    Config above;
    above.set("player.fire_duration_seconds", Json(100.5));
    auto r = Player::Params::fromConfig(above);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("fire_duration_seconds"),
              std::string::npos);

    Config atMax;
    atMax.set("player.fire_duration_seconds", Json(100.0));
    auto ok = Player::Params::fromConfig(atMax);
    ASSERT_TRUE(ok.ok()) << ok.error().describe();
    EXPECT_DOUBLE_EQ(ok->fireDurationSeconds, 100.0);
}

TEST(PlayerParamsConfigTest, UnknownPlayerKeysAreTolerated) {
    Config cfg;
    cfg.set("player.floating_pointy_hat", Json(1.0));
    auto r = Player::Params::fromConfig(cfg);
    EXPECT_TRUE(r.ok());
}

TEST(PlayerParamsConfigTest, ConfiguredParamsReachThePlayer) {
    // End to end: a 1 s decay tick from the config is what the player runs
    // with — hunger drops by exactly one point after 1.5 s (the 1/60
    // boundary lands on frame 61; the 2nd tick is ~2 s away).
    Config cfg;
    cfg.set("player.hunger_decay_seconds", Json(1.0));
    auto r = Player::Params::fromConfig(cfg);
    ASSERT_TRUE(r.ok()) << r.error().describe();
    Fixture f;
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f}, *r);
    Input idle = heldInput({});
    EXPECT_FLOAT_EQ(p.params().hungerDecaySeconds, 1.0);
    run(p, 90, idle); // 1.5 s
    EXPECT_FLOAT_EQ(p.hunger(), 19.0f);
}

// -- absorption pool (Phase 72, the two-component model) ----------------------
// The pool is SEPARATE from the health: damage drains it FIRST (the pool
// alone can never kill), healing NEVER fills it (heal clamps at the base
// max), and the game sets it from the active absorb_hp effects (a refresh
// restores the full sum — Minecraft's attribute recalculation).

TEST(PlayerTest, AbsorptionPoolIsIndependent) {
    Fixture f;
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    ASSERT_FLOAT_EQ(p.maxHealth(), p.params().maxHealth); // base only
    ASSERT_FLOAT_EQ(p.absorption(), 0.0f);
    p.setAbsorption(4.0f);
    EXPECT_FLOAT_EQ(p.absorption(), 4.0f);
    EXPECT_FLOAT_EQ(p.health(), 20.0f); // the pool never touches the health
    EXPECT_FLOAT_EQ(p.maxHealth(), 20.0f); // and never raises the max
    p.setAbsorption(0.0f);
    EXPECT_FLOAT_EQ(p.absorption(), 0.0f);
}

TEST(PlayerTest, AbsorptionClampsAtTheCap) {
    Fixture f;
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    p.setAbsorption(100.0f);
    EXPECT_FLOAT_EQ(p.absorption(), Player::kAbsorptionMax); // 64
    p.setAbsorption(-3.0f);
    EXPECT_FLOAT_EQ(p.absorption(), 0.0f);
}

TEST(PlayerTest, DamageDrainsAbsorptionFirst) {
    Fixture f;
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    p.setAbsorption(4.0f);
    // A hit smaller than the pool: the health is untouched.
    ASSERT_TRUE(p.damage(3.0f));
    EXPECT_FLOAT_EQ(p.absorption(), 1.0f);
    EXPECT_FLOAT_EQ(p.health(), 20.0f);
    // A hit larger than the pool: the remainder reaches the health.
    ASSERT_TRUE(p.damage(5.0f)); // pool 1 -> 0, health 20 -> 16
    EXPECT_FLOAT_EQ(p.absorption(), 0.0f);
    EXPECT_FLOAT_EQ(p.health(), 16.0f);
    EXPECT_FALSE(p.isDead());
}

TEST(PlayerTest, AbsorptionAloneCannotKill) {
    Fixture f;
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    p.setAbsorption(4.0f);
    // A hit of exactly the pool size: the health is full, the player
    // lives (the P56 shared model would have shown 20 of 24 here).
    ASSERT_TRUE(p.damage(4.0f));
    EXPECT_FLOAT_EQ(p.absorption(), 0.0f);
    EXPECT_FLOAT_EQ(p.health(), 20.0f);
    EXPECT_FALSE(p.isDead());
    // The death comes only from the health.
    ASSERT_TRUE(p.damage(20.0f));
    EXPECT_TRUE(p.isDead());
    EXPECT_FLOAT_EQ(p.health(), 0.0f);
}

TEST(PlayerTest, HealNeverFillsThePool) {
    Fixture f;
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    p.damage(2.0f); // 18
    p.setAbsorption(4.0f);
    p.heal(100.0f);
    EXPECT_FLOAT_EQ(p.health(), 20.0f); // clamped at the BASE max (the P56
    EXPECT_FLOAT_EQ(p.absorption(), 4.0f); // model would have shown 24)
}

TEST(PlayerTest, SetHealthClampsAtTheBaseMax) {
    Fixture f;
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    p.setAbsorption(4.0f);
    // Format 12: the saved health is the base-pool value only — a value
    // above the base max is clamped on load (and rejected at the game
    // level for format-12 files).
    p.setHealth(24.0f);
    EXPECT_FLOAT_EQ(p.health(), 20.0f);
    p.setAbsorption(0.0f);
    p.setHealth(18.0f);
    EXPECT_FLOAT_EQ(p.health(), 18.0f);
    EXPECT_FALSE(p.isDead());
}

TEST(PlayerTest, RespawnClearsTheAbsorptionPool) {
    Fixture f;
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    p.setAbsorption(4.0f);
    p.damage(24.0f); // dead (pool 4 + health 20)
    ASSERT_TRUE(p.isDead());
    p.respawn();
    EXPECT_FALSE(p.isDead());
    EXPECT_FLOAT_EQ(p.absorption(), 0.0f);
    EXPECT_FLOAT_EQ(p.health(), p.params().maxHealth);
}

// -- armor (SURV-1): the Minecraft integer-floor damage reduction ----------
// The equipped points reduce the damage BEFORE the resistance effect and
// the absorption pool (the pipeline order armor -> resistance -> pool ->
// health, pinned below):
//     pts    = min(the equipped sum, kArmorMaxPoints)
//     amount = floor(amount * (100 - kArmorReductionPerPoint * pts)) / 100
// pts 0 skips the step = the value is bit-identical (every pre-SURV-1
// damage test is untouched — the zero-regression backing).

TEST(PlayerTest, ArmorDamageFloorExactValues) {
    // 10 points: 10.0 -> floor(10.0*60)/100 = 6.0.
    {
        Fixture f;
        Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
        p.setArmorSlot(1, ItemStack {ItemID {3}, 1}, 10);
        ASSERT_TRUE(p.damage(10.0f));
        EXPECT_FLOAT_EQ(p.health(), 14.0f);
    }
    // 2 points: 1.0 -> floor(1.0*92)/100 = 0.92.
    {
        Fixture f;
        Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
        p.setArmorSlot(0, ItemStack {ItemID {3}, 1}, 2);
        ASSERT_TRUE(p.damage(1.0f));
        EXPECT_FLOAT_EQ(p.health(), 20.0f - 0.92f);
    }
    // 3 points: 1.0 -> floor(1.0*88)/100 = 0.88 — the floor lands on the
    // PERCENT-scaled damage (the written formula), so a small tick is
    // reduced but not zeroed (the spec's "3 points block the fire tick"
    // claim is the recorded deviation).
    {
        Fixture f;
        Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
        p.setArmorSlot(0, ItemStack {ItemID {3}, 1}, 3);
        ASSERT_TRUE(p.damage(1.0f));
        EXPECT_FLOAT_EQ(p.health(), 20.0f - 0.88f);
    }
}

TEST(PlayerTest, ArmorZeroPointsIsBitIdentical) {
    // No armor: the incoming values are the pre-SURV-1 ones — the four
    // existing damage classes (a direct 5.0, a fractional 3.7, the
    // starvation tick 1.0, the lava tick 4.0).
    Fixture f;
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    ASSERT_TRUE(p.damage(5.0f));
    EXPECT_FLOAT_EQ(p.health(), 15.0f);
    ASSERT_TRUE(p.damage(3.7f));
    EXPECT_FLOAT_EQ(p.health(), 15.0f - 3.7f);
    ASSERT_TRUE(p.damage(1.0f));
    EXPECT_FLOAT_EQ(p.health(), 15.0f - 3.7f - 1.0f);
    ASSERT_TRUE(p.damage(4.0f));
    EXPECT_FLOAT_EQ(p.health(), 15.0f - 3.7f - 1.0f - 4.0f);
}

TEST(PlayerTest, ArmorCapsAtTwentyPoints) {
    // A 32-point set (8 x 4 — unreachable from the content: the per-item
    // cap is 20 and the full diamond set is exactly 20; the cap logic is
    // pinned directly): damage uses 20.
    Fixture f;
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    for (int s = 0; s < kArmorSlotCount; ++s) {
        p.setArmorSlot(s, ItemStack {ItemID {3}, 1}, 8);
    }
    EXPECT_EQ(p.armorPoints(), kArmorMaxPoints); // 32 -> 20
    ASSERT_TRUE(p.damage(10.0f)); // floor(10.0*(100-80))/100 = 2.0
    EXPECT_FLOAT_EQ(p.health(), 18.0f);
}

TEST(PlayerTest, ArmorReducesBeforeResistanceAndPool) {
    // The pipeline order pinned: armor (10 pts) -> resistance (0.5) ->
    // the pool (4.0). 10.0 -> 6.0 -> 3.0 -> the pool absorbs the 3.0,
    // the health is untouched (armor first, then the P84 float multiply,
    // then the P72 pool-first drain).
    Fixture f;
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    p.setArmorSlot(1, ItemStack {ItemID {3}, 1}, 10);
    p.setDamageReduction(0.5f);
    p.setAbsorption(4.0f);
    ASSERT_TRUE(p.damage(10.0f));
    EXPECT_FLOAT_EQ(p.health(), 20.0f);
    EXPECT_FLOAT_EQ(p.absorption(), 1.0f);
}

TEST(PlayerTest, ArmorSlotApi) {
    // The slot storage + the points derivation (the setArmorSlot
    // single-setter sync: an empty piece forces its points to 0).
    Fixture f;
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    for (int s = 0; s < kArmorSlotCount; ++s) {
        EXPECT_TRUE(p.armorSlot(s).empty());
    }
    EXPECT_EQ(p.armorPoints(), 0);
    p.setArmorSlot(0, ItemStack {ItemID {3}, 1}, 1);
    p.setArmorSlot(1, ItemStack {ItemID {7}, 1}, 2);
    EXPECT_EQ(p.armorPoints(), 3);
    EXPECT_EQ(p.armorSlot(0).item, ItemID {3});
    EXPECT_EQ(p.armorSlot(1).count, 1);
    p.setArmorSlot(0, ItemStack {}, 0);
    EXPECT_TRUE(p.armorSlot(0).empty());
    EXPECT_EQ(p.armorPoints(), 2);
}

// -- Fire block (Phase 70; the ignition source since Phase 74) ------------
// The fire block does NOT deal direct contact damage anymore (the P70
// real-dt timer is gone): Game::updateEffects re-applies the Phase 57
// fire effect while Player::isInFire (the inLava || isInFire gate) — the
// burn rate, the 8 s lingering burn after leaving, and the water
// extinguish are the effect path's, pinned by the E2E (S2 burn-out / S2b
// the lingering-kill contrast). Pinned HERE: the AABB predicate (the
// strict face convention) and the retired-config tolerance.

TEST(PlayerTest, FireFaceTouchIsNotOverlap) {
    Fixture f;
    f.world.setBlock(8, 1, 8, f.fire); // the cell [8, 9) x [1, 2) x [8, 9)
    // The AABB max.x EXACTLY on the cell's x=8 face (7.7 + 0.3 = 8.0):
    // the strict per-axis convention says NOT in the fire.
    Player touch = f.makePlayer({7.7f, 1.0f, 8.5f});
    EXPECT_FALSE(touch.isInFire());
    // One float step inside: the AABB crosses the face -> in the fire.
    Player inside = f.makePlayer({7.8f, 1.0f, 8.5f});
    EXPECT_TRUE(inside.isInFire());
}

TEST(PlayerParamsConfigTest, RetiredFireDamageKeysAreTolerated) {
    // Phase 74 retired player.fire_damage / fire_damage_interval_seconds
    // (the fire block ignites the player — the effect path — instead of
    // the direct contact damage). A pre-Phase-74 config carrying them
    // still boots: unknown player.* keys are tolerated (config
    // forward-compat, the fromConfig contract).
    Config cfg;
    cfg.set("player.fire_damage", Json(1.0));
    cfg.set("player.fire_damage_interval_seconds", Json(0.2));
    auto r = Player::Params::fromConfig(cfg);
    ASSERT_TRUE(r.ok()) << r.error().describe();
}

TEST(PlayerParamsConfigTest, RetiredWaterGravityKeyTolerated) {
    // FLUID-2 retired player.water_gravity (the role flipped to
    // water_buoyancy — the default value 12.0 carries over unchanged). A
    // pre-FLUID-2 config carrying the key still boots: unknown player.*
    // keys are tolerated (the Phase-74 fire_damage precedent), and the
    // value is ignored (the buoyancy default stands).
    Config cfg;
    cfg.set("player.water_gravity", Json(12.0));
    auto r = Player::Params::fromConfig(cfg);
    ASSERT_TRUE(r.ok()) << r.error().describe();
    EXPECT_FLOAT_EQ(r->waterBuoyancy, Player::Params {}.waterBuoyancy);
    EXPECT_FLOAT_EQ(r->waterRiseSpeed, Player::Params {}.waterRiseSpeed);
    EXPECT_FLOAT_EQ(r->waterSinkSpeed, Player::Params {}.waterSinkSpeed);
}


// -- Sitting (Phase 71): crouch on the ground, overlapping a campfire -------

TEST(PlayerTest, SittingOnCampfireLocksMovement) {
    Fixture f;
    f.world.setBlock(8, 1, 8, f.campfire); // the cell at the player's feet
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f}); // the AABB overlaps it
    Input idle = heldInput({});
    run(p, 10, idle); // settle on the ground (m_onGround from last tick)
    ASSERT_FALSE(p.isSitting()); // standing (not crouching) is not sitting
    Input sit = heldInput({GameKey::W, GameKey::LeftControl});
    run(p, 1, sit); // crouch while overlapping: sitting engages
    ASSERT_TRUE(p.isSitting());
    const Vec3 at = p.position();
    run(p, 120, sit); // 2 s of holding W + crouch: no movement at all
    EXPECT_TRUE(p.isSitting());
    EXPECT_DOUBLE_EQ(at.x, p.position().x);
    EXPECT_DOUBLE_EQ(at.y, p.position().y);
    EXPECT_DOUBLE_EQ(at.z, p.position().z);
}

TEST(PlayerTest, SittingOnExtinguishedCampfireLocksMovement) {
    // Phase 75: the unlit campfire is still a seat (Minecraft — the logs
    // are unchanged; only the flame is out).
    Fixture f;
    f.world.setBlock(8, 1, 8, f.campfireExt); // the cell at the player's feet
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f}); // the AABB overlaps it
    Input idle = heldInput({});
    run(p, 10, idle); // settle on the ground (m_onGround from last tick)
    ASSERT_FALSE(p.isSitting());
    Input sit = heldInput({GameKey::W, GameKey::LeftControl});
    run(p, 1, sit); // crouch while overlapping: sitting engages
    ASSERT_TRUE(p.isSitting());
    const Vec3 at = p.position();
    run(p, 120, sit); // 2 s of holding W + crouch: no movement at all
    EXPECT_TRUE(p.isSitting());
    EXPECT_DOUBLE_EQ(at.x, p.position().x);
    EXPECT_DOUBLE_EQ(at.y, p.position().y);
    EXPECT_DOUBLE_EQ(at.z, p.position().z);
}

TEST(PlayerTest, CrouchWalkWithoutCampfireStillMoves) {
    // The control: the same crouch-walk input with NO campfire — the
    // player walks (sitting is the only thing that locks the movement).
    Fixture f;
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    Input idle = heldInput({});
    run(p, 10, idle);
    Input walk = heldInput({GameKey::W, GameKey::LeftControl});
    run(p, 60, walk); // 1 s of crouch-walking (yaw 0 = -Z)
    EXPECT_FALSE(p.isSitting());
    EXPECT_LT(p.position().z, 8.4);
}

TEST(PlayerTest, UncrouchStandsUp) {
    Fixture f;
    f.world.setBlock(8, 1, 8, f.campfire);
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    Input idle = heldInput({});
    run(p, 10, idle);
    Input sit = heldInput({GameKey::LeftControl});
    run(p, 30, sit);
    ASSERT_TRUE(p.isSitting());
    Input walk = heldInput({GameKey::W}); // the crouch is released
    run(p, 1, walk);
    EXPECT_FALSE(p.isSitting()); // standing up
    const Vec3 at = p.position();
    run(p, 60, walk);
    EXPECT_LT(p.position().z, at.z); // the movement is back
}

TEST(PlayerTest, SittingBlocksJump) {
    Fixture f;
    f.world.setBlock(8, 1, 8, f.campfire);
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    Input idle = heldInput({});
    run(p, 10, idle);
    Input sit = heldInput({GameKey::Space, GameKey::LeftControl});
    run(p, 60, sit); // holding Space while sitting: no jump
    ASSERT_TRUE(p.isSitting());
    EXPECT_DOUBLE_EQ(1.0, p.position().y); // never left the ground
}

TEST(PlayerTest, SittingNeedsGround) {
    // Mid-air over the campfire column (overlapping the cell while
    // falling): the on-ground condition keeps the player from sitting.
    Fixture f;
    f.world.setBlock(8, 5, 8, f.campfire); // a campfire at the spawn height
    Player p = f.makePlayer({8.5f, 5.0f, 8.5f}); // the AABB overlaps it
    Input sit = heldInput({GameKey::LeftControl});
    run(p, 1, sit); // the first tick: still airborne (m_onGround = false)
    EXPECT_FALSE(p.isSitting());
}


// ============================================================================
//   ENT-7 — ground/air movement inertia (the exponential friction model)
// ============================================================================

namespace {
// The ground slide distance from a full-speed release: the analytic
// integral of v0 e^{-k t} (the stop epsilon truncates it by < 0.001).
constexpr float kGroundSlideAnalytic = 4.3f / 12.0f; // ~0.358
} // namespace

TEST(PlayerInertiaTest, GroundReleaseSlidesThenRests) {
    Fixture f;
    Player p = f.makePlayer({0.5f, 1.0f, 0.5f});
    Input idle = heldInput({});
    run(p, 10, idle);
    // Build up to full walk speed (the ramp is done in ~0.4 s).
    Input walk = heldInput({GameKey::W});
    run(p, 60, walk);
    const float xFull = p.position().z; // -Z travel (facing -Z by default)
    // Release: the slide covers the analytic distance, then rests with
    // the velocity exactly zero (the stop epsilon).
    run(p, 300, idle);
    EXPECT_FLOAT_EQ(p.velocity().z, 0.0f); // deterministic rest
    const float slide = xFull - p.position().z;
    EXPECT_NEAR(slide, kGroundSlideAnalytic, 0.05f);
    EXPECT_GT(slide, 0.2f); // not the instant stop (the old behavior)
    EXPECT_TRUE(p.onGround());
}

TEST(PlayerInertiaTest, AirReleaseKeepsGliding) {
    Fixture f;
    Player p = f.makePlayer({0.5f, 1.0f, 0.5f});
    Input idle = heldInput({});
    run(p, 10, idle);
    Input walk = heldInput({GameKey::W});
    run(p, 60, walk); // full walk speed
    // Jump to leave the ground with the horizontal speed: the air tier
    // must keep gliding (the weak kAirDecelRate 1.9 decay), not the
    // ground's fast slide (k 12). Air time = 2*8.5/32 = 0.531 s, so the
    // assertion sits at 0.467 s (still airborne, deterministic).
    Input jump = heldInput({GameKey::Space});
    p.update(kDt, jump, false);
    run(p, 11, walk); // 0.2 s airborne, still rising
    run(p, 16, idle); // release 0.267 s in the air
    ASSERT_LT(p.velocity().y, 0.0f); // falling: definitely airborne
    // The ground tier would decay 4.3 to < 0.4 in the same 0.267 s;
    // the air tier keeps > 2.
    EXPECT_GT(std::abs(p.velocity().z), 2.0f);
}

TEST(PlayerInertiaTest, FluidTierKeepsTheInstantSet) {
    // The FLUID-2 tiers (water / surface / lava) are the documented
    // deviation: the instant set is untouched by the inertia.
    Fixture f;
    for (int x = 8; x < 10; ++x) {
        for (int z = 8; z < 10; ++z) {
            ASSERT_TRUE(f.world.setFluid(x, 1, z, FluidKind::Water,
                                         kMaxFluidLevel));
        }
    }
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f}); // in the 1-deep water
    p.yaw = 0.0f;
    Input walk = heldInput({GameKey::W});
    run(p, 10, walk); // 0.16 s in the water
    // The fluid tier sets the exact target every frame (no ramp): the
    // speed is at the swim-tier value after the first frame.
    EXPECT_GT(std::abs(p.velocity().z), 0.5f);
}

TEST(PlayerInertiaTest, StopEpsilonRestsExactly) {
    Fixture f;
    Player p = f.makePlayer({0.5f, 1.0f, 0.5f});
    Input idle = heldInput({});
    run(p, 10, idle);
    Input walk = heldInput({GameKey::W});
    run(p, 30, walk);
    run(p, 600, idle); // the slide ends long before this
    EXPECT_FLOAT_EQ(p.velocity().x, 0.0f);
    EXPECT_FLOAT_EQ(p.velocity().z, 0.0f);
    const float x0 = p.position().x;
    const float z0 = p.position().z;
    run(p, 60, idle); // the rest state holds (no drift)
    EXPECT_FLOAT_EQ(p.position().x, x0);
    EXPECT_FLOAT_EQ(p.position().z, z0);
}

// -- UX-3: the player limb-swing phase (the ENT-1 MobEntity rule) --------
//
// The phase is one int, advanced per frame from the STRICT frame-boundary
// horizontal displacement (the position says, not the input intent — the
// ENT-7 slide keeps swinging, the rest decays to the identity pose).
// Render-only: never saved, no config gate (the render gate is the camera
// mode — in UX-3 constant First = the phase is computed but never drawn).

TEST(PlayerSwingPhaseTest, AdvancesWhileWalking) {
    // The phase advances kSwingStep per MOVING frame: a walking player
    // (strict horizontal displacement every frame) at 10 frames has
    // swung exactly 10 * kSwingStep.
    Fixture f;
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    p.yaw = 0.0f; // W walks -Z
    Input walk = heldInput({GameKey::W});
    run(p, 10, walk);
    EXPECT_LT(p.position().z, 8.5f); // the walk really moved (non-vacuous)
    EXPECT_EQ(p.swingPhase(), 10 * kSwingStep);
}

TEST(PlayerSwingPhaseTest, KeepsSwingingWhileSlidingThenDecays) {
    // Walk 5 frames (phase 10), then release: the ENT-7 slide (velocity
    // decay) still produces strict per-frame displacement, so the phase
    // KEEPS advancing +kSwingStep per slide frame; the frame the velocity
    // is clamped to 0 is the first resting frame (the phase decays
    // kSwingDecay toward 0 from there), and the rest holds at 0.
    Fixture f;
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    p.yaw = 0.0f; // W walks -Z
    Input walk = heldInput({GameKey::W});
    run(p, 5, walk);
    ASSERT_EQ(p.swingPhase(), 5 * kSwingStep);
    Input idle = heldInput({});
    int slide = 0;
    for (;;) {
        p.update(kDt, idle, false);
        if (p.velocity().z != 0.0f) {
            ++slide;
            EXPECT_EQ(p.swingPhase(),
                      (5 * kSwingStep + slide * kSwingStep) %
                          kSwingPhaseCount);
        } else {
            break; // the stop frame: no displacement, the decay ran
        }
    }
    ASSERT_GE(slide, 5)
        << "the ENT-7 slide must carry the phase (non-vacuous)";
    EXPECT_EQ(p.swingPhase(),
              advanceSwingPhase((5 + slide) * kSwingStep % kSwingPhaseCount,
                                false));
    // The decay schedule (kSwingDecay per frame, clamped at 0).
    int expected = p.swingPhase();
    for (int i = 0; i < 20 && expected > 0; ++i) {
        p.update(kDt, idle, false);
        expected = advanceSwingPhase(expected, false);
        EXPECT_EQ(p.swingPhase(), expected);
    }
    EXPECT_EQ(p.swingPhase(), 0); // the rest pose, exactly
}

TEST(PlayerSwingPhaseTest, InertWithoutInput) {
    // The structural-immunity root: no input = no displacement = the
    // phase stays 0 (the identity pose) forever — a no-input run never
    // leaves the render fast path (the static mesh), so the baseline is
    // byte-identical.
    Fixture f;
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    Input idle = heldInput({});
    for (int i = 0; i < 20; ++i) {
        p.update(kDt, idle, false);
        EXPECT_EQ(p.swingPhase(), 0) << "frame " << i;
    }
}

// -- V2-19: the fly (the double-tap Space creative-style toggle) --------------

namespace {

// Settle the player on the fixture's floor (the shared fly-test preamble:
// the toggle's window is the Game's input state, so the physics tests
// drive the fly state directly through setFlying — the session flag the
// Game writes).
Player settledFlyingPlayer(Fixture& f) {
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    Input idle = heldInput({});
    run(p, 10, idle);
    p.setFlying(true);
    return p;
}

} // namespace

TEST(PlayerTest, FlyAscendsAtFixedSpeed) {
    // The gravity skip: the fly branch SETS the vy baseline (4.0, the
    // decision value) instead of accumulating — Space ascend.
    Fixture f;
    Player p = settledFlyingPlayer(f);
    ASSERT_TRUE(p.onGround());
    Input up = heldInput({GameKey::Space});
    p.update(kDt, up, false);
    EXPECT_FLOAT_EQ(p.velocity().y, 4.0f); // kFlyVerticalSpeed
    // 60 frames = 1 s at 4.0 b/s = 4.0 blocks.
    const float y0 = p.position().y;
    run(p, 60, up);
    EXPECT_NEAR(p.position().y, y0 + 4.0f, 1e-3f);
    EXPECT_TRUE(p.isFlying());
}

TEST(PlayerTest, FlyDescendsWithShift) {
    // LeftShift descend at the same fixed pace (the crouch mechanism
    // runs as usual: the Shift's dual meaning is the creative's).
    Fixture f;
    Player p = f.makePlayer({8.5f, 5.0f, 8.5f}); // mid-air
    p.setFlying(true);
    Input down = heldInput({GameKey::LeftShift});
    p.update(kDt, down, false);
    EXPECT_FLOAT_EQ(p.velocity().y, -4.0f); // the direct set, no gravity
    const float y0 = p.position().y;
    run(p, 30, down); // 0.5 s -> 2.0 blocks
    EXPECT_NEAR(p.position().y, y0 - 2.0f, 1e-3f);
}

TEST(PlayerTest, FlyHoversWithNoVerticalKeys) {
    // Neither Space nor Shift: the hover (the vy baseline 0, no gravity
    // accumulation — zero drift, bit-exact).
    Fixture f;
    Player p = f.makePlayer({8.5f, 5.0f, 8.5f});
    p.setFlying(true);
    Input idle = heldInput({});
    const float y0 = p.position().y;
    run(p, 30, idle);
    EXPECT_FLOAT_EQ(p.velocity().y, 0.0f);
    EXPECT_FLOAT_EQ(p.position().y, y0);
}

TEST(PlayerTest, FlyHorizontalPaceIsWalkTimesMultiplier) {
    // The horizontal pace = walkSpeed x kFlySpeedMultiplier (4.3 x 1.6
    // = 6.88 b/s — the user's "faster than walking", above the sprint
    // 5.6). Mid-air, the ENT-7 air-tier approach converges in ~1 s.
    Fixture f;
    Player p = f.makePlayer({8.5f, 5.0f, 8.5f});
    p.setFlying(true);
    p.yaw = -kPi / 2.0f; // facing +X
    Input walk = heldInput({GameKey::W});
    run(p, 600, walk); // 10 s: converged to the terminal pace
    const float hSpeed =
        std::hypot(p.velocity().x, p.velocity().z);
    EXPECT_NEAR(hSpeed, 4.3f * 1.6f, 0.05f);
    // The distance over the same time is well above the walk pace's.
    EXPECT_GT(p.position().x - 8.5f, 55.0f);
}

TEST(PlayerTest, FlyLandingTakesNoFallDamage) {
    // A controlled fly descent (Shift) back to the floor is a fly
    // LANDING, not a fall: the per-frame fall re-pin + the landing
    // path's fly gate take no damage (the creative's no-damage landing).
    Fixture f;
    Player p = settledFlyingPlayer(f);
    Input up = heldInput({GameKey::Space});
    run(p, 150, up); // 2.5 s x 4.0 = 10 blocks
    EXPECT_GT(p.position().y, 10.0f);
    Input down = heldInput({GameKey::LeftShift});
    run(p, 300, down); // 5 s: enough for the 10-block descent + margin
    EXPECT_FLOAT_EQ(p.position().y, 1.0f);
    EXPECT_TRUE(p.onGround());
    EXPECT_FLOAT_EQ(p.health(), p.maxHealth()); // no fall damage
}

TEST(PlayerTest, FlySwitchOffMidAirCountsFallFromSwitchHeight) {
    // The switch-OFF frame's free fall counts from the switch height
    // (the last per-frame re-pin) — the normal fall damage applies on
    // the landing (the MC same: mid-air switch-off grants no immunity;
    // the pre-V2-19 bug would have counted from the pre-fly ground and
    // taken zero damage for a 10-block fall).
    Fixture f;
    Player p = settledFlyingPlayer(f);
    Input up = heldInput({GameKey::Space});
    run(p, 150, up); // y ~ 11
    Input idle = heldInput({});
    run(p, 1, idle); // hover: the vy baseline 0 before the switch
    const float switchY = p.position().y;
    EXPECT_GT(switchY - 1.0f, 9.0f);
    p.setFlying(false); // the double-tap's second press (Game side)
    run(p, 300, idle); // the free fall back to the floor
    EXPECT_FLOAT_EQ(p.position().y, 1.0f);
    EXPECT_TRUE(p.onGround());
    const float fall = switchY - 1.0f; // ~10 blocks
    EXPECT_NEAR(p.health(), p.maxHealth() - (fall - 3.0f), 1e-3f);
    EXPECT_FALSE(p.isFlying());
}

TEST(PlayerTest, FlyAutoSwitchesOffInWater) {
    // The documented deviation: in water / lava the fly auto-switches
    // OFF (the project's fluid buoyancy model does not stack the flight)
    // — the frame inside the fluid is already on the swim model (Space
    // = the swim-up 4.5, not the fly's 4.0).
    Fixture f;
    buildPool(f, 6, 10, 6, 10);
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f}); // feet in the pool
    Input idle = heldInput({});
    run(p, 10, idle);
    p.setFlying(true);
    p.update(kDt, idle, false);
    EXPECT_FALSE(p.isFlying()); // the auto switch-off
    Input up = heldInput({GameKey::Space});
    p.update(kDt, up, false);
    EXPECT_FLOAT_EQ(p.velocity().y, p.params().swimUpSpeed); // the swim
}

TEST(PlayerTest, FlyStateResetsOnRespawn) {
    // The death / respawn resets the fly (session state — the player
    // respawns on the ground, no creative carry-over).
    Fixture f;
    Player p = f.makePlayer({8.5f, 5.0f, 8.5f});
    p.setFlying(true);
    ASSERT_TRUE(p.isFlying());
    p.damage(p.maxHealth() * 1000.0f); // lethal (the kill-plane pattern)
    ASSERT_TRUE(p.isDead());
    p.respawn();
    EXPECT_FALSE(p.isDead());
    EXPECT_FALSE(p.isFlying());
}

TEST(PlayerTest, TogglePressSwallowsTheJumpGrant) {
    // The double-tap's second press swallows the jump: the toggle-OFF
    // press on the ground does NOT grant the jump this frame (the
    // creative's toggle consumes it), and the swallow is ONE frame — a
    // Space still held on the next frame jumps normally (the pre-V2-19
    // hold convention).
    Fixture f;
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    Input idle = heldInput({});
    run(p, 10, idle);
    ASSERT_TRUE(p.onGround());
    p.suppressJumpGrant();
    Input jump = heldInput({GameKey::Space});
    p.update(kDt, jump, false);
    EXPECT_FLOAT_EQ(p.velocity().y, 0.0f); // no 8.5 impulse
    EXPECT_TRUE(p.onGround());
    jump.frameBegin();
    p.update(kDt, jump, false);
    EXPECT_FLOAT_EQ(p.velocity().y, p.params().jumpSpeed); // it re-grants
}

TEST(PlayerTest, SingleSpacePressStillJumps) {
    // The red line: a single tap (the window only armed, never hit) is
    // EXACTLY the pre-V2-19 jump.
    Fixture f;
    Player p = f.makePlayer({8.5f, 1.0f, 8.5f});
    Input idle = heldInput({});
    run(p, 10, idle);
    Input jump = heldInput({GameKey::Space});
    p.update(kDt, jump, false);
    EXPECT_FLOAT_EQ(p.velocity().y, p.params().jumpSpeed);
    EXPECT_FALSE(p.onGround()); // left the ground
}

// ===========================================================================
// T5 (v3.7.0): the soul sand walk slowdown — the block's data-driven
// walk_speed_factor (the (0, 1] strict parse is unit-tested in
// test_block.cpp; the content pin in test_nether_gen.cpp): the GROUND
// walk/sprint pace is scaled by the min of the two feet cells' factors
// (the slowest surface — any feet cell on the slowing block slows the
// walk), flight / the fluid tiers / crouch are EXEMPT (the walking-only
// rule — the recorded deviation 4). A factor of 1.0 (the field-less
// overworld content) is a bit-identical no-op (the structural-immunity
// red line — the control runs below prove the factor-less path is
// untouched).
// ===========================================================================

namespace {

// The soul fixture: a stone floor (the factor-less control) with an
// optional 2-cell-wide soul strip (the walk_speed_factor 0.4 block —
// the shipped soul_sand's Minecraft exact value) and an optional
// 1-deep water pool over it (the fluid-tier exemption's face).
struct SoulFixture {
    BlockRegistry blocks;
    World world;
    BlockID stone;
    BlockID soul;

    SoulFixture() {
        auto r = blocks.registerBlock(parseJson(
            R"({"id":"stone","model":{"textures":{"top":"t","side":"t",)"
            R"("bottom":"t"}}})"));
        if (!r.ok()) {
            ADD_FAILURE() << r.error().message;
        }
        r = blocks.registerBlock(parseJson(
            R"({"id":"soul","properties":{"walk_speed_factor":0.4},)"
            R"("model":{"textures":{"top":"s","side":"s","bottom":"s"}}})"));
        if (!r.ok()) {
            ADD_FAILURE() << r.error().message;
        }
        stone = blocks.idForName("stone");
        soul = blocks.idForName("soul");
        for (int x = -16; x < 16; ++x) {
            for (int z = -16; z < 16; ++z) {
                world.setBlock(x, 0, z, stone); // floor top at y = 1
            }
        }
    }

    // The soul strip: cells x in [x0, x0 + 2), all Z, at the floor row.
    void soulStrip(int x0) {
        for (int x = x0; x < x0 + 2; ++x) {
            for (int z = -16; z < 16; ++z) {
                world.setBlock(x, 0, z, soul);
            }
        }
    }

    // A 1-deep pool (the water cell y = 1) over the strip's X span: a
    // wading player's feet rest on the strip floor at y = 1.0 (the feet
    // row = the strip — the exemption test's geometry).
    void pool(int x0) {
        for (int x = x0; x < x0 + 2; ++x) {
            for (int z = -16; z < 16; ++z) {
                world.setFluid(x, 1, z, FluidKind::Water, kMaxFluidLevel);
            }
        }
    }

    Player makePlayer(Vec3 pos) const {
        return Player(world, blocks, pos);
    }
};

} // namespace

TEST(WalkSpeedFactorTest, PureFunctionsPinned) {
    // The two feet cells: the SLOWEST surface wins (the min) — one foot
    // on the factor-0.4 block slows the walk, the factor-1.0 cell (the
    // field-less stone) never out-weights it.
    EXPECT_FLOAT_EQ(feetWalkFactor(0.4f, 1.0f), 0.4f);
    EXPECT_FLOAT_EQ(feetWalkFactor(1.0f, 0.4f), 0.4f); // the min is
                                                       // commutative
    EXPECT_FLOAT_EQ(feetWalkFactor(0.5f, 0.4f), 0.4f); // the slower of
                                                       // two slow cells
    EXPECT_FLOAT_EQ(feetWalkFactor(1.0f, 1.0f), 1.0f); // the no-slowdown
                                                       // identity
    // The walk-only gate: the factor scales the walk pace when walking...
    EXPECT_FLOAT_EQ(applyWalkSpeedFactor(4.3f, 0.4f, true), 4.3f * 0.4f);
    // ...and is a bit-identical no-op for a factor of 1.0 (x * 1.0f ==
    // x, IEEE 754 — the overworld's structural-immunity root, the
    // field-less content's pace is EXACTLY the pre-T5 value).
    EXPECT_FLOAT_EQ(applyWalkSpeedFactor(4.3f, 1.0f, true), 4.3f);
    EXPECT_FLOAT_EQ(applyWalkSpeedFactor(5.6f, 1.0f, true), 5.6f);
    // ...and is untouched for the exempt tiers (flying / swimming /
    // crouching — the walking-only rule, the recorded deviation 4).
    EXPECT_FLOAT_EQ(applyWalkSpeedFactor(6.88f, 0.4f, false), 6.88f);
    EXPECT_FLOAT_EQ(applyWalkSpeedFactor(3.1f, 0.4f, false), 3.1f);
}

TEST(WalkSpeedFactorTest, WalksSlowerOnTheSoulBlock) {
    // The walk pace over the soul strip is 0.4x the pace over the stone
    // control (the same start-up ramp shape — the ENT-7 exponential
    // approach scales linearly in the target, so the 2 s distances'
    // ratio IS the factor, the real-dt tolerance band's unit form).
    SoulFixture f;
    f.soulStrip(/*x0=*/0);
    Player onSoul = f.makePlayer({0.5f, 1.0f, 8.5f}); // the strip's cell
    Player onStone = f.makePlayer({4.5f, 1.0f, 8.5f}); // the control
    Input idle = heldInput({});
    run(onSoul, 5, idle);
    run(onStone, 5, idle);
    Input walk = heldInput({GameKey::W}); // yaw 0 = -Z
    run(onSoul, 120, walk);
    run(onStone, 120, walk);
    const float soulMove = 8.5f - onSoul.position().z;
    const float stoneMove = 8.5f - onStone.position().z;
    EXPECT_GT(soulMove, 0.0f);
    EXPECT_NEAR(stoneMove, 8.06f, 0.05f); // the control = the pre-T5
                                           // walk distance (4.3 x 1.875)
    EXPECT_NEAR(soulMove / stoneMove, 0.4f, 0.005f); // the Minecraft exact
}

TEST(WalkSpeedFactorTest, StandingOnTheStripBoundarySlows) {
    // The ANY-feet-cell rule (the dual-cell scan): a player centered on
    // the strip's EDGE (x = 2.0: the AABB's X span [1.7, 2.3] straddles
    // the soul cell x = 1 and the stone cell x = 2) is slowed (the min
    // over the two feet cells = 0.4 — the slowest surface wins).
    SoulFixture f;
    f.soulStrip(/*x0=*/0);
    Player edge = f.makePlayer({2.0f, 1.0f, 8.5f});
    Player control = f.makePlayer({4.5f, 1.0f, 8.5f});
    Input idle = heldInput({});
    run(edge, 5, idle);
    run(control, 5, idle);
    Input walk = heldInput({GameKey::W});
    run(edge, 120, walk);
    run(control, 120, walk);
    const float edgeMove = 8.5f - edge.position().z;
    const float controlMove = 8.5f - control.position().z;
    EXPECT_NEAR(edgeMove / controlMove, 0.4f, 0.005f);
}

TEST(WalkSpeedFactorTest, SprintIsSlowedToo) {
    // Sprint is a WALK (the Minecraft rule — the exemption list is
    // flight / fluids / crouch only): the sprint pace over the strip is
    // 0.4x the sprint pace over the control.
    SoulFixture f;
    f.soulStrip(/*x0=*/0);
    Player onSoul = f.makePlayer({0.5f, 1.0f, 8.5f});
    Player onStone = f.makePlayer({4.5f, 1.0f, 8.5f});
    Input idle = heldInput({});
    run(onSoul, 5, idle);
    run(onStone, 5, idle);
    Input sprint = heldInput({GameKey::W, GameKey::LeftShift});
    run(onSoul, 120, sprint);
    run(onStone, 120, sprint);
    const float soulMove = 8.5f - onSoul.position().z;
    const float stoneMove = 8.5f - onStone.position().z;
    EXPECT_GT(stoneMove, soulMove);
    EXPECT_NEAR(soulMove / stoneMove, 0.4f, 0.005f);
}

TEST(WalkSpeedFactorTest, CrouchIsExempt) {
    // The walking-only rule (the recorded deviation 4): crouching over
    // the soul strip keeps the crouch pace (4.3 x 0.3 — the factor does
    // NOT stack): the crouch trajectories over the strip and the control
    // are IDENTICAL (the exemption's bit-level form).
    SoulFixture f;
    f.soulStrip(/*x0=*/0);
    Player onSoul = f.makePlayer({0.5f, 1.0f, 8.5f});
    Player onStone = f.makePlayer({4.5f, 1.0f, 8.5f});
    Input idle = heldInput({});
    run(onSoul, 5, idle);
    run(onStone, 5, idle);
    Input crouch = heldInput({GameKey::W, GameKey::LeftControl});
    run(onSoul, 120, crouch);
    run(onStone, 120, crouch);
    EXPECT_NEAR(onSoul.position().z, onStone.position().z, 1e-4f);
    // And the crouch pace is the FACTOR-LESS crouch (no 0.4 stacking):
    // 1.29 x 1.875 ~ 2.42 blocks over 2 s (the walk's 3.22 minus the
    // crouch's own scale).
    EXPECT_NEAR(8.5f - onSoul.position().z, 2.42f, 0.05f);
}

TEST(WalkSpeedFactorTest, FlightIsExempt) {
    // The walking-only rule: the fly's horizontal pace (walkSpeed x
    // kFlySpeedMultiplier) is FACTOR-FREE — the fly trajectories over
    // the strip and the control are IDENTICAL (the exemption's bit-level
    // form).
    SoulFixture f;
    f.soulStrip(/*x0=*/0);
    Player onSoul = f.makePlayer({0.5f, 1.0f, 8.5f});
    Player onStone = f.makePlayer({4.5f, 1.0f, 8.5f});
    Input idle = heldInput({});
    run(onSoul, 5, idle);
    run(onStone, 5, idle);
    onSoul.setFlying(true);
    onStone.setFlying(true);
    Input fly = heldInput({GameKey::W});
    run(onSoul, 120, fly);
    run(onStone, 120, fly);
    EXPECT_NEAR(onSoul.position().z, onStone.position().z, 1e-4f);
    // The fly pace is the factor-free 6.88 b/s class (~6.4 blocks over
    // 2 s minus the start-up ramp — FAR above the walk's 3.22).
    EXPECT_GT(8.5f - onSoul.position().z, 6.0f);
}

TEST(WalkSpeedFactorTest, WadingInAPoolIsExempt) {
    // The walking-only rule's fluid face: a WADING player (the feet on
    // the strip floor, the AABB in the water cell — the feet row IS the
    // soul strip) swims at the swim pace — the factor does NOT apply
    // (the m_inWater gate): the wade trajectories over the strip and the
    // control are IDENTICAL.
    SoulFixture f;
    f.soulStrip(/*x0=*/0);
    f.pool(/*x0=*/0); // the soul wade (cells x 0..1, y = 1)
    {
        // The control pool on the plain stone floor (cells x 4..5).
        for (int x = 4; x < 6; ++x) {
            for (int z = -16; z < 16; ++z) {
                f.world.setFluid(x, 1, z, FluidKind::Water,
                                 kMaxFluidLevel);
            }
        }
    }
    Player onSoul = f.makePlayer({0.5f, 1.0f, 8.5f});
    Player onStone = f.makePlayer({4.5f, 1.0f, 8.5f});
    Input idle = heldInput({});
    run(onSoul, 5, idle);
    run(onStone, 5, idle);
    ASSERT_TRUE(onSoul.inWater()); // the wade is in the swim state
    ASSERT_TRUE(onStone.inWater());
    Input walk = heldInput({GameKey::W});
    run(onSoul, 120, walk);
    run(onStone, 120, walk);
    EXPECT_NEAR(onSoul.position().z, onStone.position().z, 1e-4f);
    // The swim pace (3.1 b/s class — the factor-free control, the
    // ENT-7 instant set: ~3.1 x 2 minus the first step's approach).
    EXPECT_NEAR(8.5f - onSoul.position().z, 6.2f, 0.1f);
}

// -- V3-13: the death cause + the session stats (the death screen) -----------

TEST(PlayerTest, DeathCauseKillingBlowWins) {
    // The non-lethal hit never sets the cause (the KILLING blow owns it).
    BlockRegistry blocks = makeBlocks();
    World world;
    Player p(world, blocks, {8.5f, 10.0f, 8.5f});
    p.damage(5.0f, false, {Player::DamageSource::Fall, {}});
    EXPECT_FALSE(p.isDead());
    EXPECT_EQ(p.deathInfo().source, Player::DamageSource::None);
    p.damage(15.0f, false, {Player::DamageSource::Starve, {}});
    ASSERT_TRUE(p.isDead());
    EXPECT_EQ(p.deathInfo().source, Player::DamageSource::Starve);
    EXPECT_EQ(p.deathInfo().name, "");
}

TEST(PlayerTest, DeathCauseMobCarriesKillerName) {
    BlockRegistry blocks = makeBlocks();
    World world;
    Player p(world, blocks, {8.5f, 10.0f, 8.5f});
    p.damage(25.0f, false, {Player::DamageSource::Mob, "zombie"});
    ASSERT_TRUE(p.isDead());
    EXPECT_EQ(p.deathInfo().name, "zombie");
    EXPECT_EQ(p.deathCauseLine(p.deathInfo()), "被 zombie 杀死");
}

TEST(PlayerTest, DeathCauseLineFontSafe) {
    // Every cause's line pinned (the CJK glyphs are all in the font's
    // 418-char set — a font-set regression fails loudly here).
    EXPECT_EQ(Player::deathCauseLine({Player::DamageSource::Fall, {}}),
              "死于高处掉落");
    EXPECT_EQ(Player::deathCauseLine({Player::DamageSource::Void, {}}),
              "死于世界之外");
    EXPECT_EQ(Player::deathCauseLine({Player::DamageSource::Starve, {}}),
              "饿死");
    EXPECT_EQ(Player::deathCauseLine({Player::DamageSource::Lava, {}}),
              "死于熔岩");
    EXPECT_EQ(Player::deathCauseLine({Player::DamageSource::Drown, {}}),
              "死于落水");
    EXPECT_EQ(Player::deathCauseLine({Player::DamageSource::Arrow, {}}),
              "被箭射死");
    EXPECT_EQ(Player::deathCauseLine({Player::DamageSource::Effect, "fire"}),
              "死于燃烧");
    EXPECT_EQ(Player::deathCauseLine({Player::DamageSource::Effect, "poison"}),
              "死于毒");
    EXPECT_EQ(Player::deathCauseLine({Player::DamageSource::Effect, "wither"}),
              "死于药水");
    EXPECT_EQ(Player::deathCauseLine({Player::DamageSource::None, {}}),
              "死了");
}

TEST(PlayerTest, RespawnClearsDeathCause) {
    BlockRegistry blocks = makeBlocks();
    World world;
    Player p(world, blocks, {8.5f, 10.0f, 8.5f});
    p.damage(25.0f, false, {Player::DamageSource::Lava, {}});
    ASSERT_TRUE(p.isDead());
    p.respawn();
    EXPECT_FALSE(p.isDead());
    EXPECT_EQ(p.deathInfo().source, Player::DamageSource::None);
    EXPECT_FLOAT_EQ(p.health(), p.params().maxHealth);
}

TEST(PlayerTest, StatsDistanceWalkAndTeleportGuard) {
    Fixture f;
    Player p = f.makePlayer({8.5f, 10.0f, 8.5f});
    Input walk = heldInput({GameKey::W});
    run(p, 60, walk);
    const float walked = p.statsDistance();
    ASSERT_GT(walked, 1.0f); // a walking second covers > 1 block
    // The vertical fall adds nothing (the horizontal-only stat).
    const float afterFall = p.statsDistance();
    EXPECT_FLOAT_EQ(afterFall, walked);
    // The respawn's teleport back to the spawn (a >1-block jump) is
    // skipped by the threshold guard — the stat does not balloon.
    p.damage(25.0f, false, {Player::DamageSource::Fall, {}});
    ASSERT_TRUE(p.isDead());
    p.respawn();
    EXPECT_FLOAT_EQ(p.statsDistance(), walked);
}

// V3-15: the netherite's fire immunity (the lava's burn tick never
// lands while a fire-resistant piece is worn; unequip resumes it).
TEST(PlayerTest, NetheriteArmorSuppressesLavaBurn) {
    Fixture f;
    // The sealed lava pool (the FallDamageCancelledInLava's twin).
    for (int x = 6; x <= 10; ++x) {
        for (int z = 6; z <= 10; ++z) {
            f.world.setFluid(x, 1, z, FluidKind::Lava, kMaxFluidLevel);
        }
    }
    Player p = f.makePlayer({8.5f, 12.0f, 8.5f});
    Input idle = heldInput({});
    run(p, 60, idle); // the fall settles, the feet in the lava
    ASSERT_TRUE(p.inLava());
    // Equip the fire-resistant piece (the setArmorSlot's effects param).
    p.setArmorSlot(1, ItemStack {ItemID {3}, 1}, 8, 0.0f, true);
    ASSERT_TRUE(p.hasFireResistantArmor());
    run(p, 240, idle); // 4 s of soaking: the 1.0 s burn ticks would
                       // drain 16 HP — the immunity keeps the full bar
    EXPECT_FLOAT_EQ(p.health(), p.maxHealth());
    // Unequip: the burn resumes (the control arm).
    p.setArmorSlot(1, ItemStack {}, 0);
    ASSERT_FALSE(p.hasFireResistantArmor());
    run(p, 240, idle);
    EXPECT_LT(p.health(), p.maxHealth()); // the lava's tick landed
}

// -- V3-11a (creative): the mode's player-side gates -------------------------

TEST(PlayerTest, CreativeBlocksAllDamageSources) {
    // The immunity is the mode itself: every source funnels the gate.
    BlockRegistry blocks = makeBlocks();
    World world;
    Player p(world, blocks, {8.5f, 10.0f, 8.5f});
    p.setCreative(true);
    for (Player::DamageSource src :
         {Player::DamageSource::Fall, Player::DamageSource::Void,
          Player::DamageSource::Starve, Player::DamageSource::Lava,
          Player::DamageSource::Drown, Player::DamageSource::Arrow,
          Player::DamageSource::Mob, Player::DamageSource::Effect}) {
        p.damage(25.0f, false, {src, "x"});
        EXPECT_FALSE(p.isDead()) << static_cast<int>(src);
        EXPECT_FLOAT_EQ(p.health(), p.maxHealth()) << static_cast<int>(src);
    }
}

TEST(PlayerTest, CreativeHungerIsStatic) {
    // The decay AND the starvation tick never run in creative; the entry
    // fills the bar (the creative's full static read).
    Fixture f;
    Player p = f.makePlayer({8.5f, 10.0f, 8.5f});
    p.setCreative(true);
    EXPECT_FLOAT_EQ(p.hunger(), p.params().maxHunger);
    Input idle = heldInput({});
    run(p, 3600, idle); // a minute: the decay would drain the bar
    EXPECT_FLOAT_EQ(p.hunger(), p.params().maxHunger);
    EXPECT_FLOAT_EQ(p.health(), p.maxHealth());
    // Back to survival: the decay resumes (the control — 23 ticks'
    // window: the 20-point saturation buffer drains first, then the
    // bar; 36 s = the default decay interval; the +2 tick margin is
    // the 1/60 double-accumulation boundary's slack, the
    // HungerDecaysEvery36Seconds' window discipline).
    p.setCreative(false);
    run(p, 23 * 36 * 60, idle);
    EXPECT_LT(p.hunger(), p.params().maxHunger);
}

TEST(PlayerTest, CreativeEntryFillsAStarvingBar) {
    Fixture f;
    Player p = f.makePlayer({8.5f, 10.0f, 8.5f});
    p.setHealth(3.0f);
    p.setCreative(true); // the entry fills the bar (the mode's read)
    EXPECT_FLOAT_EQ(p.hunger(), p.params().maxHunger);
}

