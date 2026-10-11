#include <gtest/gtest.h>

#include <SDL3/SDL.h>

#include <cmath>
#include <cstdint>
#include <string>

#include "block/block_registry.hpp"
#include "core/input.hpp"
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
    return blocks;
}

// A solid stone floor covering x,z in [-16, 16) at y=0 (top surface y=1),
// plus the block set the player needs (solid stone, non-solid water).
struct Fixture {
    BlockRegistry blocks = makeBlocks();
    World world;
    BlockID stone;
    BlockID water;

    Fixture() {
        stone = blocks.idForName("stone");
        water = blocks.idForName("water");
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
    run(p, 120, walk); // 2 s: z should move ~6 blocks along the wall
    // The blocked axis stays clamped, the other keeps moving: sliding.
    EXPECT_FLOAT_EQ(p.position().x, 12.0f - 0.3f);
    EXPECT_LT(p.position().z, 4.0f);
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
void buildPool(Fixture& f, int x0, int x1, int z0, int z1) {
    for (int x = x0; x <= x1; ++x) {
        for (int z = z0; z <= z1; ++z) {
            f.world.setFluid(x, 1, z, kMaxFluidLevel);
            f.world.setFluid(x, 2, z, kMaxFluidLevel);
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
    run(p, 30, swim); // 0.5 s: rises at 4.5, peaks at ~0.84, sinks slightly
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
    EXPECT_NEAR(aMove, 4.3f, 0.2f); // walk speed
    EXPECT_NEAR(bMove, 5.6f, 0.2f); // sprint speed
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
            f.world.setFluid(x, 1, z, kMaxFluidLevel); // pool: top at y=2
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
    // intervals -> 19 -> 20 (capped).
    run(p, 14 * 60, idle);
    EXPECT_FLOAT_EQ(p.health(), p.maxHealth());
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
    cfg.set("player.water_gravity", Json(10.0));
    cfg.set("player.water_terminal_velocity", Json(3.0));
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
    EXPECT_FLOAT_EQ(p.waterGravity, 10.0f);
    EXPECT_FLOAT_EQ(p.waterTerminalVelocity, 3.0f);
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
