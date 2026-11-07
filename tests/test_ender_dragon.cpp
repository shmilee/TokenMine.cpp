// V3-10d1 (the ender dragon): the boss's data-flag strict parse (the
// dragon_flight pairing group / the contact + exemption families / the
// breath-fireball-crystal number domains), the lap-decision pure
// function (the fixed 2-draw stream contract + the census-driven perch
// probability), the AI's behavior (the crystal-delta strafe / the strafe
// duration / the perch takeoff / the orbit angle re-anchor), the
// immunities (the knockback / the effect drop / the heal cap / the
// contact cooldown), the BreathCloud's lifecycle, and the shipped data
// pins.
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "block/block_registry.hpp"
#include "core/asset_manager.hpp"
#include "core/end_portal.hpp"
#include "core/game_local.hpp"
#include "core/json.hpp"
#include "core/status_effect.hpp"
#include "entity/breath_cloud.hpp"
#include "entity/mob_entity.hpp"
#include "entity/mob_registry.hpp"
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

BlockRegistry makeBlocks() {
    BlockRegistry blocks;
    auto r = blocks.registerBlock(
        parseDef(R"({"id":"stone","properties":{},"model":{"textures":{"top":"s","side":"s","bottom":"s"}}})"));
    if (!r.ok()) {
        throw std::runtime_error(r.error().message);
    }
    return blocks;
}

// The dragon's test definition (the shipped data's behavior core — a
// one-box model keeps the mesh out of the unit tests, the bat's shape).
std::string dragonDef() {
    return R"({
  "id": "ender_dragon",
  "properties": {
    "health": 200,
    "speed": 0.7,
    "detect_radius": 16,
    "attack_range": 0,
    "attack_damage": 0,
    "attack_cooldown": 4.0,
    "wander_interval": 2.0,
    "spawn_max_light": 15,
    "hostile": true,
    "flying": true,
    "spawns_in_air": true,
    "dragon_flight": true,
    "boss": true,
    "contact_damage": 5,
    "breath_cooldown": 8,
    "breath_duration": 3,
    "breath_radius": 2,
    "fireball_cooldown": 2.5,
    "fireball_speed": 12,
    "crystal_break_damage": 10,
    "detect_vertical": 8,
    "fire_exempt": true,
    "effect_exempt": true,
    "knockback_exempt": true,
    "xp": 0
  },
  "model": {
    "width": 8,
    "height": 4,
    "boxes": [
      {"pos": [0.0, 3.0, 0.0], "size": [1.0, 1.0, 1.0], "texture": "a"}
    ]
  }
})";
}

// A mob definition over the dragon's flight core with a MUTABLE
// properties tail (the negative-parse tests swap the tail — the JSON
// duplicate key would keep the first value and the mutation would be
// invisible). Single-line assembly (the multi-line raw string's
// terminator discipline is a parse hazard).
std::string mobJson(const std::string& props) {
    return std::string("{\"id\":\"d\",\"properties\":{") + props +
           "},\"model\":{\"width\":8,\"height\":4,\"boxes\":[{\"pos\":["
           "0,3,0],\"size\":[1,1,1],\"texture\":\"a\"}]}}";
}

// The dragon's full flight-core properties (the pairing group's shape).
const std::string kDragonCoreProps =
    "\"health\":200,\"speed\":0.7,\"detect_radius\":16,\"attack_range\":"
    "0,\"attack_damage\":0,\"attack_cooldown\":4,\"wander_interval\":2,"
    "\"spawn_max_light\":15,\"hostile\":true,\"flying\":true,"
    "\"spawns_in_air\":true,\"dragon_flight\":true,\"boss\":true,"
    "\"contact_damage\":5,\"breath_cooldown\":8,\"breath_duration\":3,"
    "\"breath_radius\":2,\"fireball_cooldown\":2.5,\"fireball_speed\":12,"
    "\"crystal_break_damage\":10,\"detect_vertical\":8,"
    "\"fire_exempt\":true,\"effect_exempt\":true,"
    "\"knockback_exempt\":true,\"xp\":0";


// The fixture OWNS the registry (the MobEntity's m_def points into it —
// the registry's lifetime must outlive every mob built from it, the
// enderman test's dangling-temporary shape improved on).
struct Fixture {
    World world;
    BlockRegistry blocks = makeBlocks();
    MobRegistry mobs;
    const Mob* dragon = nullptr;

    Fixture() {
        const BlockID stone = blocks.idForName("stone");
        for (int x = -8; x < 8; ++x) {
            for (int z = -8; z < 8; ++z) {
                world.setBlock(x, 0, z, stone);
            }
        }
        auto r = mobs.registerMob(parseDef(dragonDef()));
        if (!r.ok()) {
            throw std::runtime_error(r.error().message);
        }
        dragon = &mobs.at(mobs.idForName("ender_dragon"));
    }
};

// The per-mob stream's step (the PITS 4 gamma — a draw advances the
// state by exactly one step; the contract's count is a step count).
constexpr std::uint64_t kStreamGamma = 0x9E3779B97F4A7C15ULL;

// A dragon mob at the orbit's top point (the anchors' shape: the orbit
// center + the perch the game would feed at spawn).
MobEntity makeDragon(const Fixture& f, std::uint64_t seed = 7) {
    MobEntity mob(f.world, f.blocks, *f.dragon, Vec3 {0.5f, 58.0f, 0.5f},
                  seed);
    mob.setDragonAnchors(Vec3 {0.5f, 56.5f, 0.5f}, Vec3 {0.5f, 58.0f, 0.5f});
    mob.setCrystalsAlive(6, false);
    return mob;
}

} // namespace

// ---------------------------------------------------------------------------
// The data flags' strict parse (the S6's negative faces).
// ---------------------------------------------------------------------------

// A core-properties string with one FRAGMENT replaced (the mutation's
// shape — kDragonCoreProps is the baseline, each negative swaps a
// field group's presence).
std::string coreWith(const std::string& fragment) {
    // kDragonCoreProps minus its flight core + the fragment's tail.
    // (The core's prefix is the shared numeric block up to "hostile".)
    const std::string prefix =
        "\"health\":200,\"speed\":0.7,\"detect_radius\":16,"
        "\"attack_range\":0,\"attack_damage\":0,\"attack_cooldown\":4,"
        "\"wander_interval\":2,\"spawn_max_light\":15,\"hostile\":true,";
    return prefix + fragment + ",\"xp\":0";
}

TEST(DragonParse, DragonFlightRequiresFlying) {
    // The parse's pairing gate: a dragon_flight mob without flying is a
    // data contradiction (the branch runs in the flying dispatch).
    auto r = MobRegistry().registerMob(
        parseDef(mobJson(coreWith(
            "\"spawns_in_air\":true,\"dragon_flight\":true,"
            "\"breath_cooldown\":8,\"breath_duration\":3,\"breath_radius\":2,"
            "\"fireball_cooldown\":2.5,\"fireball_speed\":12,"
            "\"crystal_break_damage\":10"))));
    ASSERT_FALSE(r.ok()) << "a non-flying dragon_flight must be named";
    EXPECT_NE(r.error().message.find("flying"), std::string::npos);
}

TEST(DragonParse, DragonFlightRequiresSpawnsInAir) {
    auto r = MobRegistry().registerMob(
        parseDef(mobJson(coreWith(
            "\"flying\":true,\"dragon_flight\":true,"
            "\"breath_cooldown\":8,\"breath_duration\":3,\"breath_radius\":2,"
            "\"fireball_cooldown\":2.5,\"fireball_speed\":12,"
            "\"crystal_break_damage\":10"))));
    ASSERT_FALSE(r.ok()) << "a non-in-air dragon_flight must be named";
    EXPECT_NE(r.error().message.find("spawns_in_air"), std::string::npos);
}

TEST(DragonParse, DragonFlightRequiresTheBreathPair) {
    auto r = MobRegistry().registerMob(
        parseDef(mobJson(coreWith(
            "\"flying\":true,\"spawns_in_air\":true,\"dragon_flight\":true,"
            "\"fireball_cooldown\":2.5,\"fireball_speed\":12,"
            "\"crystal_break_damage\":10"))));
    ASSERT_FALSE(r.ok()) << "a dragon_flight without the breath pair";
    EXPECT_NE(r.error().message.find("breath"), std::string::npos);
}

TEST(DragonParse, DragonFlightRequiresTheFireballPair) {
    auto r = MobRegistry().registerMob(
        parseDef(mobJson(coreWith(
            "\"flying\":true,\"spawns_in_air\":true,\"dragon_flight\":true,"
            "\"breath_cooldown\":8,\"breath_duration\":3,\"breath_radius\":2,"
            "\"crystal_break_damage\":10"))));
    ASSERT_FALSE(r.ok()) << "a dragon_flight without the fireball pair";
    EXPECT_NE(r.error().message.find("fireball"), std::string::npos);
}

TEST(DragonParse, BreathPairWithoutDragonFlightIsNamed) {
    // The dead-field discipline: the parameters without the capability
    // have no consumer — named (the milk pairing's reverse direction).
    auto r = MobRegistry().registerMob(
        parseDef(mobJson(coreWith(
            "\"flying\":true,\"spawns_in_air\":true,\"breath_cooldown\":8"))));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("dragon_flight"), std::string::npos);
}

TEST(DragonParse, FireballPairWithoutDragonFlightIsNamed) {
    auto r = MobRegistry().registerMob(
        parseDef(mobJson(coreWith(
            "\"flying\":true,\"spawns_in_air\":true,\"fireball_speed\":12"))));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("dragon_flight"), std::string::npos);
}

TEST(DragonParse, ContactDamageDomainIsNamed) {
    // (0, 100] — the 0 / the >100 / the string are all named (the
    // numeric gate's family).
    // The required numeric fields' shape (the plainMobDef's tail — the
    // parse's required-set is checked before the domain's).
    const std::string base =
        "{\"id\":\"d\",\"properties\":{\"health\":20,\"speed\":1,"
        "\"detect_radius\":8,\"attack_range\":1,\"attack_damage\":2,"
        "\"attack_cooldown\":2,\"wander_interval\":2,\"spawn_max_light\":15,"
        "\"hostile\":true,\"contact_damage\":";
    const std::string tail =
        "},\"model\":{\"width\":1,\"height\":1,\"boxes\":[{\"pos\":["
        "0,0.5,0],\"size\":[1,1,1],\"texture\":\"a\"}]}}";
    for (const char* bad : {"0", "101", "\"5\""}) {
        const std::string def = base + std::string(bad) + tail;
        auto r = MobRegistry().registerMob(parseDef(def));
        EXPECT_FALSE(r.ok()) << "contact_damage " << bad << " must be named";
    }
    // The 5.0 (the shipped value) registers.
    EXPECT_TRUE(
        MobRegistry().registerMob(parseDef(base + "5" + tail)).ok());
}

TEST(DragonParse, BreathRadiusDomainIsNamed) {
    for (const char* bad : {"0", "33"}) {
        const std::string def =
            std::string("{\"id\":\"d\",\"properties\":{\"health\":20,") +
            "\"speed\":1,\"flying\":true,\"spawns_in_air\":true,"
            "\"dragon_flight\":true,\"breath_cooldown\":8,"
            "\"breath_duration\":3,\"breath_radius\":" +
            bad +
            ",\"fireball_cooldown\":2.5,\"fireball_speed\":12,"
            "\"crystal_break_damage\":10},"
            "\"model\":{\"width\":8,\"height\":4,\"boxes\":[{\"pos\":["
            "0,3,0],\"size\":[1,1,1],\"texture\":\"a\"}]}}";
        auto r = MobRegistry().registerMob(parseDef(def));
        EXPECT_FALSE(r.ok()) << "breath_radius " << bad << " must be named";
    }
}

TEST(DragonParse, BossIsStrictBool) {
    const std::string def =
        "{\"id\":\"d\",\"properties\":{\"health\":20,\"speed\":1,"
        "\"detect_radius\":8,\"attack_range\":1,\"attack_damage\":2,"
        "\"attack_cooldown\":2,\"wander_interval\":2,\"spawn_max_light\":15,"
        "\"hostile\":true,\"boss\":\"yes\"},"
        "\"model\":{\"width\":1,\"height\":1,\"boxes\":[{\"pos\":["
        "0,0.5,0],\"size\":[1,1,1],\"texture\":\"a\"}]}}";
    auto r = MobRegistry().registerMob(parseDef(def));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("boss"), std::string::npos);
}

TEST(DragonParse, PlainMobKeepsTheBitRoot) {
    // A mob without any 10d flag keeps every dragon field at its
    // default (the structural-immunity root's parse side).
    MobRegistry mobs;
    auto r = mobs.registerMob(
        parseDef(R"({"id":"zombie","properties":{"health":20,"speed":0.8,"detect_radius":16,"attack_range":1.6,"attack_damage":3,"attack_cooldown":1,"wander_interval":2.5},"model":{"width":0.6,"height":1.8,"boxes":[{"pos":[0,1.25,0],"size":[0.5,0.5,0.5],"texture":"a"}]}})"));
    ASSERT_TRUE(r.ok());
    const MobProperties& p = mobs.at(mobs.idForName("zombie")).properties;
    EXPECT_FALSE(p.boss);
    EXPECT_FALSE(p.dragonFlight);
    EXPECT_FLOAT_EQ(p.contactDamage, 0.0f);
    EXPECT_FALSE(p.fireExempt);
    EXPECT_FALSE(p.effectExempt);
    EXPECT_FALSE(p.knockbackExempt);
    EXPECT_DOUBLE_EQ(p.breathCooldown, 0.0);
    EXPECT_DOUBLE_EQ(p.breathDuration, 0.0);
    EXPECT_FLOAT_EQ(p.breathRadius, 0.0f);
    EXPECT_DOUBLE_EQ(p.fireballCooldown, 0.0);
    EXPECT_FLOAT_EQ(p.fireballSpeed, 0.0f);
    EXPECT_FLOAT_EQ(p.crystalBreakDamage, 0.0f);
    EXPECT_FLOAT_EQ(p.detectVertical, 0.0f);
}

// ---------------------------------------------------------------------------
// The lap-decision pure function (the stream contract's apex).
// ---------------------------------------------------------------------------

TEST(DragonLapDecision, ConsumesExactlyTwoDrawsOnEveryOutcome) {
    // The contract's fixedness (the 10c's twin): the decision ALWAYS
    // advances the stream by exactly 2 steps — the outcome never
    // changes the consumption (the swallowed-roll discipline).
    for (int crystals : {0, 1, 6, 7}) {
        std::uint64_t state = 0x9E3779B97F4A7C15ULL;
        const std::uint64_t before = state;
        const DragonLapDecision d = rollDragonLapDecision(state, crystals);
        EXPECT_EQ(state, before + 2 * kStreamGamma) << "crystals=" << crystals
                                                    << " decision=" << (int)d;
    }
}

TEST(DragonLapDecision, IsDeterministicOverKnownStates) {
    for (std::uint64_t seed :
         {0x1111111111111111ULL, 0x2222222222222222ULL,
          0xABCDEF0123456789ULL, 0xFEDCBA9876543210ULL}) {
        std::uint64_t a = seed, b = seed;
        EXPECT_EQ(rollDragonLapDecision(a, 6), rollDragonLapDecision(b, 6))
            << "seed=" << seed;
    }
    // And a different state can give a different decision (the stream's
    // witness — not a constant function).
    bool anyDiffer = false;
    for (std::uint64_t s = 1; s < 1000 && !anyDiffer; ++s) {
        std::uint64_t a = s, b = s + 1;
        if (rollDragonLapDecision(a, 6) != rollDragonLapDecision(b, 6)) {
            anyDiffer = true;
        }
    }
    EXPECT_TRUE(anyDiffer);
}

TEST(DragonLapDecision, PerchProbabilityFollowsTheCensus) {
    // The MC's 1/(3 + live crystals) read over the 2nd draw: with 0
    // crystals the perch is common (the 1/3 class over the not-strafe
    // states), with 6 it is rarer (the 1/9 class) — the statistical
    // pins over a fixed state sequence (the bounds stay generous for
    // the 53-bit uniform, tight enough to catch a constant / an
    // inverted / a census-ignoring read).
    int perch0 = 0, perch6 = 0, total = 0;
    for (std::uint64_t s = 1; s <= 4000; ++s) {
        std::uint64_t a = s, b = s;
        if (rollDragonLapDecision(a, 0) == DragonLapDecision::Perch) {
            ++perch0;
        }
        if (rollDragonLapDecision(b, 6) == DragonLapDecision::Perch) {
            ++perch6;
        }
        ++total;
    }
    // Not-strafe's 3/4 x the 1/3 class ~ 25% at c=0; ~12.5% at c=6.
    EXPECT_GT(perch0, total / 10);  // well above the 1/9 class
    EXPECT_LT(perch0, total / 2);   // far below a coin flip
    EXPECT_LT(perch6, perch0);      // more crystals = rarer perch
    EXPECT_GT(perch6, total / 100); // ... but not dead
}

// ---------------------------------------------------------------------------
// The AI's behavior (the driven mob — the frame-locked 1/60 step).
// ---------------------------------------------------------------------------

TEST(DragonAi, CrystalDeltaEntersStrafeWhenSeeingThePlayer) {
    Fixture f;
    MobEntity mob = makeDragon(f);
    // The player in the detect context (the horizontal + the vertical
    // band — the dragon's detect_vertical 8 widens the 2.0 floor).
    mob.setPlayer(Vec3 {10.0f, 58.0f, 0.0f}, true);
    ASSERT_EQ(mob.state(), MobState::Wander); // the orbit
    // The break (the census 6 -> 5, the changed flag — the game's feed).
    mob.setCrystalsAlive(5, true);
    mob.update(1.0 / 60.0);
    EXPECT_EQ(mob.state(), MobState::RangedAttack); // the strafe
}

TEST(DragonAi, CrystalDeltaWithoutAPlayerInSightNeverStrafes) {
    Fixture f;
    MobEntity mob = makeDragon(f);
    mob.setPlayer(Vec3 {100.0f, 58.0f, 0.0f}, true); // outside the 16
    mob.setCrystalsAlive(5, true);
    mob.update(1.0 / 60.0);
    EXPECT_EQ(mob.state(), MobState::Wander);
    // The flag is consumed (the next frame without a fresh feed stays
    // the orbit — the single-consumption discipline).
    mob.update(1.0 / 60.0);
    EXPECT_EQ(mob.state(), MobState::Wander);
}

TEST(DragonAi, StrafeEndsAfterTheFixedDuration) {
    Fixture f;
    MobEntity mob = makeDragon(f);
    mob.setPlayer(Vec3 {10.0f, 58.0f, 0.0f}, true);
    const double dt = 1.0 / 60.0;
    mob.setCrystalsAlive(5, true);
    mob.update(dt);
    ASSERT_EQ(mob.state(), MobState::RangedAttack);
    // The strafe's expiry (the 3.5 s timer — the float accumulation's
    // ±2-frame tolerance over the 210/211 boundary, the tick-class
    // assertion's margin discipline). The re-anchor's continuity is
    // checked on the transition's frame: no teleport — the orbit's
    // angle is derived from the strafe's end position (the 10c's
    // swallow's contrast), so the mob moves at most one frame's arc
    // speed across it.
    int frames = 0;
    while (mob.state() == MobState::RangedAttack && frames < 240) {
        mob.setPlayer(Vec3 {10.0f, 58.0f, 0.0f}, true);
        const Vec3 before = mob.position();
        mob.update(dt);
        ++frames;
        if (mob.state() == MobState::Wander) {
            const float dx = before.x - mob.position().x;
            const float dz = before.z - mob.position().z;
            EXPECT_LE(std::sqrt(dx * dx + dz * dz), 0.3f);
        }
    }
    EXPECT_EQ(mob.state(), MobState::Wander); // the orbit's resume
    EXPECT_GE(frames, 208);
    EXPECT_LE(frames, 213);
}

TEST(DragonAi, OrbitAngleFromPositionRoundTrips) {
    Fixture f;
    MobEntity mob = makeDragon(f); // the orbit center (0.5, 58, 0.5)
    // Place the mob at a known orbit point (angle pi/2 = the +Z side).
    const float r = kDragonOrbitRadius;
    const double theta = std::numbers::pi / 2;
    mob.teleportTo(Vec3 {static_cast<float>(0.5 + std::cos(theta) * r),
                         58.0f, static_cast<float>(0.5 + std::sin(theta) * r)});
    EXPECT_NEAR(mob.orbitAngleFromPosition(), static_cast<float>(theta),
                1e-4f);
    // The angle is in [0, 2pi) (the -X point reads pi, the -Z point
    // wraps toward 2pi, not negatives).
    mob.teleportTo(Vec3 {0.5f - r, 58.0f, 0.5f}); // angle pi
    EXPECT_NEAR(mob.orbitAngleFromPosition(),
                static_cast<float>(std::numbers::pi), 1e-4f);
    mob.teleportTo(Vec3 {0.5f, 58.0f, 0.5f - r}); // angle 3pi/2
    EXPECT_NEAR(mob.orbitAngleFromPosition(),
                static_cast<float>(3.0 * std::numbers::pi / 2), 1e-4f);
}

TEST(DragonAi, PerchTakeoffOnTheCumulativeDamage) {
    // The seed-search's existence claim: SOME seed perches within 60 s
    // (the lap roll's 1/9's witness), and the perched dragon takeoffs on
    // the kDragonPerchDamageTakeoff cumulative damage (the MC's 50
    // read) — the growl telegraph fires while the player is in the
    // breath range (the perch's state machine's E2E, the 10c's
    // provoked-line twin).
    const double dt = 1.0 / 60.0;
    int growls = -1, perchSeed = -1;
    for (std::uint64_t seed = 1; seed <= 300 && perchSeed < 0; ++seed) {
        Fixture f;
        MobEntity mob = makeDragon(f, seed);
        // The player at the perch's side (13.5 from the anchor — inside
        // the detect 16 AND the breath 20; the vertical 0.5 inside the
        // 8 band).
        mob.setPlayer(Vec3 {14.0f, 56.0f, 0.0f}, true);
        int growlCount = 0, breathCount = 0;
        mob.setGrowlCallback([&](Vec3) { ++growlCount; });
        mob.setBreathCallback([&]() { ++breathCount; });
        bool perched = false;
        for (int i = 0; i < 60 * 60; ++i) {
            mob.update(dt);
            if (mob.state() == MobState::Attack) {
                perched = true;
                break;
            }
        }
        if (!perched) {
            continue;
        }
        // Arrive at the anchor (the approach's 2x-speed — a bounded
        // wait on the arrival's 0.5-box, the float-inequality's shape).
        int waited = 0;
        while (std::abs(mob.position().x - 0.5f) > 0.5f ||
               std::abs(mob.position().z - 0.5f) > 0.5f) {
            if (waited >= 30 * 60) {
                break;
            }
            mob.update(dt);
            ++waited;
        }
        ASSERT_TRUE(mob.state() == MobState::Attack) << "seed=" << seed;
        EXPECT_LT(waited, 30 * 60);
        // The growl's telegraph (the player's inside the breath range —
        // the state machine's growl lead-in; bounded wait).
        for (int i = 0; i < 3 * 60 && growlCount == 0; ++i) {
            mob.update(dt);
        }
        // The escape read: the cumulative damage passes the 50.
        mob.damage(kDragonPerchDamageTakeoff + 1.0f);
        mob.update(dt);
        EXPECT_EQ(mob.state(), MobState::Wander) << "seed=" << seed
                                                 << " the takeoff";
        perchSeed = static_cast<int>(seed);
        growls = growlCount;
        break;
    }
    ASSERT_GE(perchSeed, 1) << "no seed perched in 300 (the lap roll's "
                               "existence witness)";
    EXPECT_GT(growls, 0) << "the perched growl's telegraph never fired "
                            "(the player's inside the breath range)";
}

TEST(DragonAi, TheOrbitStaysTheOrbitWithoutLaps) {
    // The no-context witness: a far player (no strafe's context) keeps
    // the orbit across a full second (no lap crossing in one second —
    // the lap is ~27 s), and the altitude's approach holds the setpoint
    // (the integrate's dragon branch: y converges on the orbit center's
    // 58, the bob's bounded swing).
    Fixture f;
    MobEntity mob = makeDragon(f);
    mob.setPlayer(Vec3 {100.0f, 58.0f, 0.0f}, true);
    for (int i = 0; i < 60; ++i) {
        mob.update(1.0 / 60.0);
    }
    EXPECT_EQ(mob.state(), MobState::Wander);
    EXPECT_NEAR(mob.position().y, 58.0f, 1.0f);
}

// ---------------------------------------------------------------------------
// The immunities (the three exemption flags + the heal).
// ---------------------------------------------------------------------------

TEST(DragonImmunity, KnockbackExemptIgnoresTheImpulse) {
    Fixture f;
    MobEntity dragon = makeDragon(f);
    dragon.setVelocity(Vec3::zero());
    dragon.addKnockback(Vec3 {3.0f, 0.0f, 0.0f});
    EXPECT_EQ(dragon.velocity(), Vec3::zero());

    // The control: a plain mob takes the impulse (the bit root — the
    // gate's own witness).
    MobRegistry mobs;
    auto r = mobs.registerMob(
        parseDef(R"({"id":"zombie","properties":{"health":20,"speed":0.8,"detect_radius":16,"attack_range":1.6,"attack_damage":3,"attack_cooldown":1,"wander_interval":2.5},"model":{"width":0.6,"height":1.8,"boxes":[{"pos":[0,1.25,0],"size":[0.5,0.5,0.5],"texture":"a"}]}})"));
    ASSERT_TRUE(r.ok());
    MobEntity zombie(f.world, f.blocks, mobs.at(mobs.idForName("zombie")),
                     Vec3 {5.5f, 1.0f, 5.5f}, 3);
    zombie.setVelocity(Vec3::zero());
    zombie.addKnockback(Vec3 {3.0f, 0.0f, 0.0f});
    EXPECT_GT(zombie.velocity().x, 0.0f);
}

TEST(DragonImmunity, EffectExemptDropsTheStatus) {
    Fixture f;
    EffectRegistry effects;
    auto er = effects.registerEffect(
        parseDef(R"({"id":"poison","tick_type":"damage","tick_amount":1,"tick_interval":2.5,"min_health":1,"affects_mobs":true,"icon":0})"));
    ASSERT_TRUE(er.ok());
    MobEntity dragon = makeDragon(f);
    ASSERT_GT(dragon.health(), 0.0f);
    dragon.applyEffect(er.value(), 5.0, effects, 0);
    // The exemption's witness: the poison never arms (the non-exempt
    // mob's ticks would land at 2.5 s / 5 s — the dragon's health stays
    // full across the 6 s).
    for (int i = 0; i < 60 * 6; ++i) {
        dragon.update(1.0 / 60.0);
    }
    EXPECT_FLOAT_EQ(dragon.health(), 200.0f);
}

TEST(DragonImmunity, HealClampsAtTheCap) {
    Fixture f;
    MobEntity dragon = makeDragon(f);
    ASSERT_FLOAT_EQ(dragon.health(), 200.0f);
    // Full: the heal is a no-op (the beam's render gate — the crystals
    // glow but don't beam).
    EXPECT_FALSE(dragon.heal(1.0f));
    // Damaged: the heal lands, capped at the data health.
    ASSERT_FALSE(dragon.damage(10.0f));
    EXPECT_FLOAT_EQ(dragon.health(), 190.0f);
    EXPECT_TRUE(dragon.heal(5.0f));
    EXPECT_FLOAT_EQ(dragon.health(), 195.0f);
    EXPECT_TRUE(dragon.heal(50.0f)); // the over-heal's clamp
    EXPECT_FLOAT_EQ(dragon.health(), 200.0f);
    // The dead corpse: the heal cannot revive (the damage's guard's
    // twin).
    dragon.damage(1000.0f);
    ASSERT_TRUE(dragon.dead());
    EXPECT_FALSE(dragon.heal(10.0f));
    EXPECT_TRUE(dragon.dead());
}

TEST(DragonImmunity, TheContactCooldownTicksDown) {
    Fixture f;
    MobEntity dragon = makeDragon(f);
    EXPECT_TRUE(dragon.contactDamageReady());
    dragon.latchContactDamage();
    EXPECT_FALSE(dragon.contactDamageReady());
    dragon.tickContactCooldown(0.9);
    EXPECT_FALSE(dragon.contactDamageReady());
    dragon.tickContactCooldown(0.2); // 1.1 s total > the 1.0
    EXPECT_TRUE(dragon.contactDamageReady());
}

// ---------------------------------------------------------------------------
// The BreathCloud's lifecycle (the zero-physics transient).
// ---------------------------------------------------------------------------

TEST(BreathCloud, TheRadiusExpandsWithTheAge) {
    Fixture f;
    BreathCloudEntity cloud(f.world, f.blocks, Vec3 {0.5f, 1.0f, 0.5f}, 3.0,
                            2.0f);
    EXPECT_FLOAT_EQ(cloud.radius(), 2.0f);
    cloud.update(1.0); // 1 s
    EXPECT_FLOAT_EQ(cloud.radius(),
                    2.0f + BreathCloudEntity::kExpansionRate);
    cloud.update(2.0); // 3 s total
    EXPECT_FLOAT_EQ(cloud.radius(),
                    2.0f + 3.0f * BreathCloudEntity::kExpansionRate);
}

TEST(BreathCloud, DespawnsAtTheDuration) {
    Fixture f;
    BreathCloudEntity cloud(f.world, f.blocks, Vec3 {0.5f, 1.0f, 0.5f}, 3.0,
                            2.0f);
    for (int i = 0; i < 60 * 2 + 1; ++i) {
        cloud.update(1.0 / 60.0);
    }
    EXPECT_FALSE(cloud.shouldDespawn()); // 2.0167 s < 3 s
    for (int i = 0; i < 60; ++i) {
        cloud.update(1.0 / 60.0);
    }
    EXPECT_TRUE(cloud.shouldDespawn()); // 3.0167 s
}

TEST(BreathCloud, HasNoPhysics) {
    Fixture f;
    BreathCloudEntity cloud(f.world, f.blocks, Vec3 {0.5f, 1.0f, 0.5f}, 5.0,
                            2.0f);
    const Vec3 start = cloud.position();
    for (int i = 0; i < 60 * 4; ++i) {
        cloud.update(1.0 / 60.0);
    }
    EXPECT_EQ(cloud.position(), start); // the hover (the gravity's bypass)
    EXPECT_EQ(cloud.velocity(), Vec3::zero());
}

// ---------------------------------------------------------------------------
// The shipped data pins (the real asset — the design's census).
// ---------------------------------------------------------------------------

TEST(ShippedDragon, DataPinned) {
    // The REAL assets (data/mobs/ender_dragon.json): the design's
    // census — the 200 HP (the MC exact, the zombie's precedent) / the
    // hostile class / the flight pair + the dragon_flight / the boss /
    // the contact 5 / the three exemptions / the breath 8s-3s-2r / the
    // fireball 2.5s-12b/s / the crystal-break 10 / the detect_vertical
    // 8 / the ambient / the xp 0 (the burst is the ritual's 200-orb,
    // not the drop) / the 8x4 model's 8 boxes + the 2 wing swings.
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    MobRegistry mobs;
    ASSERT_TRUE(mobs.loadDirectory(assets, "data/mobs").ok());
    const Mob* m = mobs.findByName("ender_dragon");
    ASSERT_TRUE(m != nullptr);
    const MobProperties& p = m->properties;
    EXPECT_FLOAT_EQ(p.health, 200.0f);
    EXPECT_FLOAT_EQ(p.speed, 0.7f);
    EXPECT_TRUE(p.hostile);
    EXPECT_TRUE(p.flying);
    EXPECT_TRUE(p.spawnsInAir);
    EXPECT_TRUE(p.dragonFlight);
    EXPECT_TRUE(p.boss);
    EXPECT_FLOAT_EQ(p.contactDamage, 5.0f);
    EXPECT_TRUE(p.fireExempt);
    EXPECT_TRUE(p.effectExempt);
    EXPECT_TRUE(p.knockbackExempt);
    EXPECT_DOUBLE_EQ(p.breathCooldown, 8.0);
    EXPECT_DOUBLE_EQ(p.breathDuration, 3.0);
    EXPECT_FLOAT_EQ(p.breathRadius, 2.0f);
    EXPECT_DOUBLE_EQ(p.fireballCooldown, 2.5);
    EXPECT_FLOAT_EQ(p.fireballSpeed, 12.0f);
    EXPECT_FLOAT_EQ(p.crystalBreakDamage, 10.0f);
    EXPECT_FLOAT_EQ(p.detectVertical, 8.0f);
    EXPECT_EQ(p.ambientSoundName, "dragon_ambient");
    EXPECT_EQ(p.xp, 0);
    // The model (the 8x4 — the boss's wing-span read; the 8 boxes,
    // the 2 swings — the wing's signature).
    EXPECT_FLOAT_EQ(m->model.width, 8.0f);
    EXPECT_FLOAT_EQ(m->model.height, 4.0f);
    ASSERT_EQ(m->model.boxes.size(), 8u);
    ASSERT_EQ(m->model.swings.size(), 2u);
    // NO other mob carries the dragon group (the exclusivity's
    // witness — the core never switches on the species, but the SHIPPED
    // data must stay the singleton's).
    for (const MobID id : mobs.allIds()) {
        const Mob& mob = mobs.at(id);
        if (mob.name != "ender_dragon") {
            EXPECT_FALSE(mob.properties.dragonFlight) << mob.name;
            EXPECT_FALSE(mob.properties.boss) << mob.name;
        }
    }
}

// ---------------------------------------------------------------------------
// V3-10d2 (the summon / the re-kill): the crystal-base pair's strict
// parse, the shipped base set, the face-slot vocabulary, the named
// constants.
// ---------------------------------------------------------------------------

namespace {

std::string blockDef(const std::string& props) {
    return std::string("{\"id\":\"b\",\"properties\":{") + props +
           "},\"model\":{\"textures\":{\"top\":\"t\",\"side\":\"t\","
           "\"bottom\":\"t\"}}}";
}

} // namespace

TEST(DragonSummon, CrystalBaseFlagsAreStrictBool) {
    // The pair's strict-bool family (the carriable's twin shape): the
    // string / the number are named, the true registers, the absent is
    // the false default (the bit root).
    for (const char* bad : {"\"yes\"", "1"}) {
        for (const char* key : {"crystal_base", "requires_crystal_base"}) {
            const std::string def = blockDef(
                std::string("\"") + key + "\": " + bad);
            auto r = BlockRegistry().registerBlock(parseDef(def));
            EXPECT_FALSE(r.ok()) << key << " = " << bad;
        }
    }
    auto ok1 = BlockRegistry().registerBlock(
        parseDef(blockDef("\"crystal_base\": true")));
    ASSERT_TRUE(ok1.ok());
    auto ok2 = BlockRegistry().registerBlock(
        parseDef(blockDef("\"requires_crystal_base\": true")));
    ASSERT_TRUE(ok2.ok());
    BlockRegistry reg;
    ASSERT_TRUE(reg.registerBlock(parseDef(blockDef(""))).ok());
    EXPECT_FALSE(reg.at(ok1.value()).properties.crystalBase)
        << "the other block's flag stays false (the registry's isolation)";
}

TEST(DragonSummon, ShippedBaseSetIsExact) {
    // The REAL assets: crystal_base is the MC's foundation set —
    // obsidian / bedrock / the end portal frame family, NOTHING else;
    // requires_crystal_base is the crystal alone (the placement gate's
    // data twin); the egg's blast_resistance 3 (the crystal blast's
    // survivor — the MC egg's near-indestructible read, the obsidian's
    // br-3 project twin).
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    BlockRegistry reg;
    ASSERT_TRUE(reg.loadDirectory(assets, "data/blocks").ok());
    std::vector<std::string> bases;
    std::vector<std::string> requirers;
    for (std::size_t i = 0; i < reg.count(); ++i) {
        const Block& b = reg.at(BlockID{static_cast<int>(i)});
        if (b.properties.crystalBase) {
            bases.push_back(b.name);
        }
        if (b.properties.requiresCrystalBase) {
            requirers.push_back(b.name);
        }
    }
    std::sort(bases.begin(), bases.end());
    std::sort(requirers.begin(), requirers.end());
    EXPECT_EQ(bases,
              (std::vector<std::string> {
                  "bedrock", "end_portal_frame", "end_portal_frame_eyed",
                  "obsidian"}));
    EXPECT_EQ(requirers, (std::vector<std::string> {"end_crystal"}));
    // The egg's blast resistance (the summon's face-crystal blast's
    // survivor — without it a face shatter eats the egg, the MC
    // deviation the br-3 closes).
    const Block& egg = reg.at(reg.idForName("dragon_egg"));
    EXPECT_FLOAT_EQ(egg.properties.blastResistance, 3.0f);
}

TEST(DragonSummon, FaceSlotsVocabularyPinned) {
    // The summon's 4 EXIT-FACE cells (the MC's "four sides" read): the
    // ±X pair ARE the arena's exit crystals (kEndCrystalSlots[4..5]'s
    // cells), the ±Z pair are worldgen-ABSENT (only a placement fills
    // them — the summon's player-action core). The single source's pin.
    const int want[4][3] = {{2, 54, 0}, {-2, 54, 0},
                            {0, 54, 2}, {0, 54, -2}};
    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(static_cast<int>(std::floor(kEndSummonFaceSlots[i].x)),
                  want[i][0]) << i;
        EXPECT_EQ(static_cast<int>(std::floor(kEndSummonFaceSlots[i].y)),
                  want[i][1]) << i;
        EXPECT_EQ(static_cast<int>(std::floor(kEndSummonFaceSlots[i].z)),
                  want[i][2]) << i;
    }
    // The ±X pair's cells == the arena exit slots' cells (the shared
    // vocabulary's witness — the arena crystal counts toward the
    // summon).
    EXPECT_EQ(static_cast<int>(std::floor(kEndSummonFaceSlots[0].x)),
              static_cast<int>(std::floor(kEndCrystalSlots[4].x)));
    EXPECT_EQ(static_cast<int>(std::floor(kEndSummonFaceSlots[1].x)),
              static_cast<int>(std::floor(kEndCrystalSlots[5].x)));
}

TEST(DragonSummon, NamedConstantsPinned) {
    // The session timers' + the re-kill XP's named values (the D5's
    // read: the MC 500/12000 ratio's 1/24 over the project's 200).
    EXPECT_EQ(kEggRespawnFrames, 3600); // the 60 s (the MC ~1-min read)
    EXPECT_EQ(kDragonSummonCountdownFrames, 60); // the 1 s countdown
    EXPECT_EQ(kDragonRekillXpValue, 8);
    // The first kill's 200 = the 23 orbs' sum (22 x 9 + the last 2)
    // vs the re-kill's 8 (the ratio's witness).
    EXPECT_EQ((kDragonXpOrbs - 1) * kDragonXpOrbValue +
                  kDragonXpLastOrbValue,
              200);
}
