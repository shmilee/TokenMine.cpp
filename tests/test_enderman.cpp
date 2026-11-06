// V3-10c (the enderman): the End's only mob — the data flags' strict
// parse (eye_contact_range / the teleport_interval pair / carries_blocks
// + the block-side carriable + its stateless gate), the teleport roll's
// pure function (the stream contract's fixedness + the box bounds + the
// calm/enraged anchors), the eye-contact frame counter (the provocation's
// 5-frame discipline + the blink reset), the carry session state, and
// the shipped data pins (the enderman's census + the carriable set).
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "block/block_registry.hpp"
#include "core/asset_manager.hpp"
#include "core/json.hpp"
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

// A minimal block registry (the mob fixture's world content — stone, the
// generic floor).
BlockRegistry makeBlocks() {
    BlockRegistry blocks;
    auto r = blocks.registerBlock(
        parseDef(R"({"id":"stone","properties":{},"model":{"textures":{"top":"s","side":"s","bottom":"s"}}})"));
    if (!r.ok()) {
        throw std::runtime_error(r.error().message);
    }
    return blocks;
}

struct Fixture {
    World world;
    BlockRegistry blocks = makeBlocks();

    Fixture() {
        // The floor at y 0 (the teleport probe's landing ground).
        const BlockID stone = blocks.idForName("stone");
        for (int x = -8; x < 8; ++x) {
            for (int z = -8; z < 8; ++z) {
                world.setBlock(x, 0, z, stone);
            }
        }
    }
};

// The enderman's test definition (the shipped data's behavior core — a
// one-box model keeps the mesh out of the unit tests, the bat's shape).
// The 10c flag group is a REPLACEABLE parameter (the negative tests swap
// in a single mutated field — a JSON duplicate key would keep the first
// value and the mutation would be invisible to the parse).
std::string endermanDef(
    const std::string& tenCProps =
        R"("eye_contact_range": 48, "teleport_interval_min": 2, "teleport_interval_max": 6, "carries_blocks": true)") {
    return std::string(R"({
  "id": "enderman",
  "properties": {
    "health": 40,
    "speed": 0.8,
    "detect_radius": 16,
    "attack_range": 1.6,
    "attack_damage": 6,
    "attack_cooldown": 1.5,
    "wander_interval": 2.0,
    "spawn_max_light": 0,
    "neutral": true,
    )" +
        tenCProps +
        R"(
  },
  "model": {
    "width": 0.6,
    "height": 2.9,
    "boxes": [
      {"pos": [0.0, 2.65, 0.0], "size": [0.5, 0.5, 0.5], "texture": "a"}
    ]
  }
})");
}

// A non-10c mob (the zombie's shape — the bit-root witness: no eye /
// teleport / carry flags at all).
std::string plainMobDef(const char* id) {
    return std::string(R"({"id":")") + id + R"(","properties":{"health":20,"speed":0.8,"detect_radius":16,"attack_range":1.6,"attack_damage":3,"attack_cooldown":1.0,"wander_interval":2.5},"model":{"width":0.6,"height":1.8,"boxes":[{"pos":[0.0,1.25,0.0],"size":[0.5,0.5,0.5],"texture":"a"}]}})";
}

MobRegistry registerEnderman() {
    MobRegistry mobs;
    auto r = mobs.registerMob(parseDef(endermanDef()));
    if (!r.ok()) {
        throw std::runtime_error(r.error().message);
    }
    return mobs;
}

// The per-mob stream's step (the PITS 4 gamma — a draw advances the
// state by exactly one step; the contract's count is a step count).
constexpr std::uint64_t kStreamGamma = 0x9E3779B97F4A7C15ULL;

} // namespace

// ---------------------------------------------------------------------------
// The data flags' strict parse (the S6's negative faces).
// ---------------------------------------------------------------------------

TEST(EndermanParse, ShippedShapeRegisters) {
    MobRegistry mobs = registerEnderman();
    const Mob& m = mobs.at(mobs.idForName("enderman"));
    const MobProperties& p = m.properties;
    EXPECT_TRUE(p.neutral);
    EXPECT_EQ(p.eyeContactRange, 48.0f);
    EXPECT_DOUBLE_EQ(p.teleportIntervalMin, 2.0);
    EXPECT_DOUBLE_EQ(p.teleportIntervalMax, 6.0);
    EXPECT_TRUE(p.carriesBlocks);
    EXPECT_EQ(p.spawnMaxLight, 0);
}

TEST(EndermanParse, EyeContactRangeNegativeIsNamed) {
    MobRegistry mobs;
    auto r = mobs.registerMob(parseDef(
        endermanDef(R"("eye_contact_range": -4, "carries_blocks": true)")));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("eye_contact_range"),
              std::string::npos);
}

TEST(EndermanParse, EyeContactRangeNonNumberIsNamed) {
    MobRegistry mobs;
    auto r = mobs.registerMob(parseDef(
        endermanDef(R"("eye_contact_range": "far", "carries_blocks": true)")));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("eye_contact_range"),
              std::string::npos);
}

TEST(EndermanParse, TeleportPairMustBeTogether) {
    // A lone min (the milk pairing's violation — named).
    {
        MobRegistry mobs;
        auto r = mobs.registerMob(
            parseDef(endermanDef(R"("teleport_interval_min": 9)")));
        ASSERT_FALSE(r.ok());
        EXPECT_NE(r.error().message.find("teleport_interval_min"),
                  std::string::npos);
        EXPECT_NE(r.error().message.find("teleport_interval_max"),
                  std::string::npos);
    }
    // A lone max (the mirror).
    {
        MobRegistry mobs;
        auto r = mobs.registerMob(
            parseDef(endermanDef(R"("teleport_interval_max": 9)")));
        ASSERT_FALSE(r.ok());
        EXPECT_NE(r.error().message.find("teleport_interval_min"),
                  std::string::npos);
    }
}

TEST(EndermanParse, TeleportPairRangeAndOrderAreNamed) {
    // min > max (the order violation).
    {
        MobRegistry mobs;
        auto r = mobs.registerMob(parseDef(
            endermanDef(R"("teleport_interval_min": 7, "teleport_interval_max": 3)")));
        ASSERT_FALSE(r.ok());
        EXPECT_NE(r.error().message.find("teleport_interval_min"),
                  std::string::npos);
    }
    // min <= 0 (the domain's lower bound).
    {
        MobRegistry mobs;
        auto r = mobs.registerMob(parseDef(
            endermanDef(R"("teleport_interval_min": 0, "teleport_interval_max": 6)")));
        ASSERT_FALSE(r.ok());
        EXPECT_NE(r.error().message.find("teleport_interval_min"),
                  std::string::npos);
    }
    // max > 60 (the domain's cap).
    {
        MobRegistry mobs;
        auto r = mobs.registerMob(parseDef(
            endermanDef(R"("teleport_interval_min": 2, "teleport_interval_max": 61)")));
        ASSERT_FALSE(r.ok());
        EXPECT_NE(r.error().message.find("teleport_interval_max"),
                  std::string::npos);
    }
}

TEST(EndermanParse, CarriesBlocksStrictBool) {
    MobRegistry mobs;
    auto r = mobs.registerMob(parseDef(
        endermanDef(R"("eye_contact_range": 48, "carries_blocks": "yes")")));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("carries_blocks"),
              std::string::npos);
}

TEST(EndermanParse, PlainMobKeepsTheBitRoot) {
    // A pre-V3-10c mob (the zombie's shape): every 10c flag is at its
    // default — the capability gates (canTeleport / eyeContactRange /
    // carriesBlocks) all read false, the structural-immunity root.
    MobRegistry mobs;
    auto r = mobs.registerMob(parseDef(plainMobDef("zombie")));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const MobProperties& p = mobs.at(mobs.idForName("zombie")).properties;
    EXPECT_EQ(p.eyeContactRange, 0.0f);
    EXPECT_DOUBLE_EQ(p.teleportIntervalMin, 0.0);
    EXPECT_DOUBLE_EQ(p.teleportIntervalMax, 0.0);
    EXPECT_FALSE(p.carriesBlocks);
}

// ---------------------------------------------------------------------------
// The block-side carriable flag (the strict bool + the stateless gate).
// ---------------------------------------------------------------------------

TEST(EndermanCarriable, StrictBoolAndDefaultFalse) {
    {
        BlockRegistry blocks;
        auto r = blocks.registerBlock(
            parseDef(R"({"id":"rock","properties":{"carriable":"yes"},"model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
        ASSERT_FALSE(r.ok());
        EXPECT_NE(r.error().message.find("carriable"), std::string::npos);
    }
    {
        BlockRegistry blocks;
        auto r = blocks.registerBlock(
            parseDef(R"({"id":"rock","properties":{},"model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
        ASSERT_TRUE(r.ok());
        EXPECT_FALSE(blocks.at(blocks.idForName("rock")).properties.carriable);
    }
}

TEST(EndermanCarriable, StatefulCarriableIsNamed) {
    // The S6 face: a carriable STATEFUL block is a journal the carry
    // cannot write — the register-time ENT-2 twin names it.
    BlockRegistry blocks;
    auto r = blocks.registerBlock(parseDef(
        R"({"id":"weird","properties":{"carriable":true},"model":{"state_count":2,"state_default":0,"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("carriable"), std::string::npos);
    EXPECT_NE(r.error().message.find("state"), std::string::npos);
}

// ---------------------------------------------------------------------------
// The teleport roll's pure function (the stream contract + the bounds).
// ---------------------------------------------------------------------------

TEST(EndermanTeleportRoll, IsDeterministicOverTheStream) {
    std::uint64_t a = 0xDEADBEEFCAFEBABEULL;
    std::uint64_t b = 0xDEADBEEFCAFEBABEULL;
    const Vec3 pos {10.5f, 53.0f, -3.5f};
    const Vec3 player {14.0f, 53.5f, 2.0f};
    const auto ra = rollTeleportCandidates(a, pos, player, /*enraged=*/false);
    const auto rb = rollTeleportCandidates(b, pos, player, /*enraged=*/false);
    for (int i = 0; i < kTeleportAttempts; ++i) {
        EXPECT_EQ(ra[i].pos, rb[i].pos) << i;
    }
    // A different stream state gives a different roll (the seed's
    // witness — the per-mob stream's independence).
    std::uint64_t c = 0x0123456789ABCDEFULL;
    const auto rc = rollTeleportCandidates(c, pos, player, /*enraged=*/false);
    bool anyDiffer = false;
    for (int i = 0; i < kTeleportAttempts; ++i) {
        if (ra[i].pos != rc[i].pos) {
            anyDiffer = true;
        }
    }
    EXPECT_TRUE(anyDiffer);
}

TEST(EndermanTeleportRoll, ConsumesTheFixedStreamContract) {
    // The contract's fixedness (the recon's R2 / the PITS 389
    // discipline): an attempt ALWAYS advances the stream by exactly
    // 3 draws x kTeleportAttempts candidates — the probe's outcome
    // (the game side) never touches it. The step count is the gamma's
    // multiple (a draw = one advance).
    std::uint64_t before = 0x9E3779B97F4A7C15ULL;
    const auto out = rollTeleportCandidates(
        before, Vec3 {0.5f, 1.0f, 0.5f}, Vec3 {9.0f, 1.0f, 7.0f}, false);
    (void)out;
    const std::uint64_t want =
        0x9E3779B97F4A7C15ULL +
        3 * static_cast<std::uint64_t>(kTeleportAttempts) * kStreamGamma;
    EXPECT_EQ(before, want);
}

TEST(EndermanTeleportRoll, CalmBoxBounds) {
    // The calm candidate (the mob's own position's ±16 x/z, ±8 y box —
    // the 64³ read's project scale): every candidate sits in the box.
    std::uint64_t state = 0xC0FFEE0000000001ULL;
    const Vec3 pos {20.5f, 53.0f, 12.5f};
    const auto out =
        rollTeleportCandidates(state, pos, Vec3 {30.0f, 53.0f, 40.0f},
                               /*enraged=*/false);
    for (int i = 0; i < kTeleportAttempts; ++i) {
        EXPECT_LE(std::abs(out[i].pos.x - pos.x), kTeleportCalmH + 1e-4f)
            << i;
        EXPECT_LE(std::abs(out[i].pos.y - pos.y), kTeleportCalmV + 1e-4f)
            << i;
        EXPECT_LE(std::abs(out[i].pos.z - pos.z), kTeleportCalmH + 1e-4f)
            << i;
    }
}

TEST(EndermanTeleportRoll, EnragedAnchorsTowardThePlayer) {
    // The enraged candidate (the player-toward 12-block point's ±8 x/z,
    // ±4 y box — the MC 16h/17v read's project scale): the anchor is
    // the 12-point when the player is farther, the player itself when
    // nearer (the clamp), and every candidate sits in the anchor's box.
    {
        std::uint64_t state = 0x1111111111111111ULL;
        const Vec3 pos {0.5f, 53.0f, 0.5f};
        const Vec3 player {30.0f, 53.0f, 0.0f}; // 29.5 away > 12
        const auto out =
            rollTeleportCandidates(state, pos, player, /*enraged=*/true);
        const float dist = std::sqrt(
            (player.x - pos.x) * (player.x - pos.x) +
            (player.y - pos.y) * (player.y - pos.y) +
            (player.z - pos.z) * (player.z - pos.z));
        const float t = kTeleportAngryAnchor / dist;
        const Vec3 anchor {pos.x + (player.x - pos.x) * t,
                           pos.y + (player.y - pos.y) * t,
                           pos.z + (player.z - pos.z) * t};
        for (int i = 0; i < kTeleportAttempts; ++i) {
            EXPECT_LE(std::abs(out[i].pos.x - anchor.x),
                      kTeleportAngryH + 1e-4f)
                << i;
            EXPECT_LE(std::abs(out[i].pos.y - anchor.y),
                      kTeleportAngryV + 1e-4f)
                << i;
            EXPECT_LE(std::abs(out[i].pos.z - anchor.z),
                      kTeleportAngryH + 1e-4f)
                << i;
        }
    }
    {
        // The player NEARER than the anchor: the anchor IS the player
        // (the closest-approach clamp — the teleport lands on the
        // player's neighborhood).
        std::uint64_t state = 0x2222222222222222ULL;
        const Vec3 pos {0.5f, 53.0f, 0.5f};
        const Vec3 player {2.0f, 53.0f, 0.0f}; // ~1.6 away < 12
        const auto out =
            rollTeleportCandidates(state, pos, player, /*enraged=*/true);
        for (int i = 0; i < kTeleportAttempts; ++i) {
            EXPECT_LE(std::abs(out[i].pos.x - player.x),
                      kTeleportAngryH + 1e-4f)
                << i;
            EXPECT_LE(std::abs(out[i].pos.y - player.y),
                      kTeleportAngryV + 1e-4f)
                << i;
            EXPECT_LE(std::abs(out[i].pos.z - player.z),
                      kTeleportAngryH + 1e-4f)
                << i;
        }
    }
}

// ---------------------------------------------------------------------------
// The eye-contact feed (the provocation's frame discipline).
// ---------------------------------------------------------------------------

MobEntity makeEnderman(const Fixture& f, std::uint64_t seed = 7) {
    MobRegistry mobs = registerEnderman();
    return MobEntity(f.world, f.blocks,
                     mobs.at(mobs.idForName("enderman")),
                     Vec3 {5.5f, 1.0f, 5.5f}, seed);
}

TEST(EndermanEyeContact, FiveConsecutiveFramesProvoke) {
    Fixture f;
    MobEntity mob = makeEnderman(f);
    EXPECT_FALSE(mob.angry());
    // Four looking frames: no provocation yet (the counter's run-up).
    for (int i = 0; i < kEyeContactFrames - 1; ++i) {
        EXPECT_FALSE(mob.eyeContactFeed(true)) << i;
    }
    // The 5th: the PROVOKING frame (true + the neutral's anger state).
    EXPECT_TRUE(mob.eyeContactFeed(true));
    EXPECT_TRUE(mob.angry());
    // Already angry: further looking frames do not re-provoke (the
    // window's running — the counter stays spent).
    EXPECT_FALSE(mob.eyeContactFeed(true));
}

TEST(EndermanEyeContact, TheBlinkResetsTheCounter) {
    Fixture f;
    MobEntity mob = makeEnderman(f);
    // Four looking, one blink, four looking: the counter never reaches
    // 5 consecutive (the line-of-sight break's de-provocation — the MC
    // read: the provocation does not persist across a blink).
    for (int i = 0; i < kEyeContactFrames - 1; ++i) {
        EXPECT_FALSE(mob.eyeContactFeed(true));
    }
    EXPECT_FALSE(mob.eyeContactFeed(false)); // the blink
    for (int i = 0; i < kEyeContactFrames - 1; ++i) {
        EXPECT_FALSE(mob.eyeContactFeed(true));
    }
    EXPECT_FALSE(mob.angry());
}

TEST(EndermanEyeContact, ReProvokeAfterTheWindowDeEnrages) {
    Fixture f;
    MobEntity mob = makeEnderman(f);
    for (int i = 0; i < kEyeContactFrames; ++i) {
        mob.eyeContactFeed(true);
    }
    ASSERT_TRUE(mob.angry());
    // The 30 s window expires (the update's real-dt countdown — 31 s of
    // frames de-enrages it): the counter is spent, the anger is gone.
    const double dt = 1.0 / 60.0;
    for (int i = 0; i < 31 * 60; ++i) {
        mob.update(dt);
    }
    EXPECT_FALSE(mob.angry());
    // A fresh 5-frame looking run re-provokes (the provocation's
    // repeatability — the window is the de-enrage, not the lockout).
    for (int i = 0; i < kEyeContactFrames - 1; ++i) {
        EXPECT_FALSE(mob.eyeContactFeed(true));
    }
    EXPECT_TRUE(mob.eyeContactFeed(true));
    EXPECT_TRUE(mob.angry());
}

TEST(EndermanEyeContact, ARangeZeroMobNeverProvokes) {
    // The bit root: a mob without the data range (the plain zombie)
    // keeps the counter at 0 forever — the feed is a no-op.
    Fixture f;
    MobRegistry mobs;
    auto r = mobs.registerMob(parseDef(plainMobDef("zombie")));
    ASSERT_TRUE(r.ok());
    MobEntity mob(f.world, f.blocks, mobs.at(mobs.idForName("zombie")),
                  Vec3 {5.5f, 1.0f, 5.5f}, 7);
    for (int i = 0; i < 10; ++i) {
        EXPECT_FALSE(mob.eyeContactFeed(true));
    }
    EXPECT_FALSE(mob.angry());
}

// ---------------------------------------------------------------------------
// The hit-triggered teleport flag (damage sets it, the update's slot
// consumes it — the stream order's R2 witness).
// ---------------------------------------------------------------------------

TEST(EndermanTeleport, TheHitFlagArmsTheNextUpdate) {
    Fixture f;
    MobEntity mob = makeEnderman(f);
    int attempts = 0;
    mob.setTeleportCallback(
        [&](const std::array<TeleportCandidate, kTeleportAttempts>&) {
            ++attempts;
            return true; // the probe's success (the mob moves — the
                          // callback's contract's witness)
        });
    // No damage, no interval expiry yet (the constructor's first roll is
    // in (2, 6) — run well under 2 s): no attempt.
    const double dt = 1.0 / 60.0;
    for (int i = 0; i < 60; ++i) { // 1 s
        mob.update(dt);
    }
    EXPECT_EQ(attempts, 0);
    // The hit: damage() sets the want-flag (it does NOT touch the
    // stream — the call site's un-orderable frame point).
    ASSERT_FALSE(mob.damage(2.0f));
    // The next update's slot consumes it: exactly one attempt.
    mob.update(dt);
    EXPECT_EQ(attempts, 1);
    // A second hit re-arms (the 100%-on-hit read — every hit gets its
    // attempt, the cooldown's no-exemption).
    ASSERT_FALSE(mob.damage(2.0f));
    mob.update(dt);
    EXPECT_EQ(attempts, 2);
}

TEST(EndermanTeleport, TheIntervalExpiryAttemptsWithoutAHit) {
    Fixture f;
    MobEntity mob = makeEnderman(f);
    int attempts = 0;
    mob.setTeleportCallback(
        [&](const std::array<TeleportCandidate, kTeleportAttempts>&) {
            ++attempts;
            return false; // the swallow (the total failure — the stream
                           // is the roll's, the outcome's not)
        });
    // Run past the max interval (6 s + margin): the expiry attempts
    // (the random teleport — no damage involved).
    const double dt = 1.0 / 60.0;
    for (int i = 0; i < 8 * 60; ++i) {
        mob.update(dt);
    }
    EXPECT_GE(attempts, 1);
}

TEST(EndermanTeleport, APlainMobNeverAttempts) {
    // The capability gate: a non-teleport mob's update runs no attempt
    // (the callback is never wired by the game for it — the belt: even
    // a wired one is never invoked, the canTeleport gate's witness).
    Fixture f;
    MobRegistry mobs;
    auto r = mobs.registerMob(parseDef(plainMobDef("zombie")));
    ASSERT_TRUE(r.ok());
    MobEntity mob(f.world, f.blocks, mobs.at(mobs.idForName("zombie")),
                  Vec3 {5.5f, 1.0f, 5.5f}, 7);
    int attempts = 0;
    mob.setTeleportCallback(
        [&](const std::array<TeleportCandidate, kTeleportAttempts>&) {
            ++attempts;
            return true;
        });
    const double dt = 1.0 / 60.0;
    for (int i = 0; i < 60 * 30; ++i) { // 30 s — far past any interval
        mob.update(dt);
    }
    mob.damage(2.0f); // a hit (the neutral's enrage — the zombie's not
                      // neutral: the damage is plain, the flag is never
                      // read)
    mob.update(dt);
    EXPECT_EQ(attempts, 0);
}

// ---------------------------------------------------------------------------
// The carry session state (the setter's witness + the death-drop's
// predicate — the instance state, the wool color's class).
// ---------------------------------------------------------------------------

TEST(EndermanCarry, TheSessionStateRoundTrips) {
    Fixture f;
    MobEntity mob = makeEnderman(f);
    EXPECT_TRUE(mob.carriesBlocks());
    EXPECT_FALSE(mob.carried().valid());
    const BlockID stone = f.blocks.idForName("stone");
    mob.setCarried(stone);
    EXPECT_EQ(mob.carried(), stone);
    mob.setCarried(BlockID::invalid());
    EXPECT_FALSE(mob.carried().valid());
}

TEST(EndermanCarry, APlainMobCannotCarry) {
    Fixture f;
    MobRegistry mobs;
    auto r = mobs.registerMob(parseDef(plainMobDef("zombie")));
    ASSERT_TRUE(r.ok());
    MobEntity mob(f.world, f.blocks, mobs.at(mobs.idForName("zombie")),
                  Vec3 {5.5f, 1.0f, 5.5f}, 7);
    EXPECT_FALSE(mob.carriesBlocks());
    // The setter's belt: the game's carry pass gates on carriesBlocks()
    // before it ever calls setCarried — the plain mob's field stays
    // invalid (this pins the gate's predicate, not the setter's
    // freedom).
    EXPECT_FALSE(mob.carried().valid());
}

// ---------------------------------------------------------------------------
// The SHIPPED data (the real assets — the enderman's census + the
// carriable set + the End pool's vocabulary).
// ---------------------------------------------------------------------------

TEST(ShippedEnderman, DataPinned) {
    // The REAL assets (data/mobs/enderman.json): the design's census —
    // the 40 HP / the 0.8 speed / the neutral class / the 48 eye range
    // (the fog's kEndFogEnd — the "see it to enrage it" ruling) / the
    // 2-6 s interval pair / the carry / the drops (the pearl's 0-1 MC
    // 50% + the eye's 0-1 project addition — the 10a entry's survival
    // closure) / the xp 5 / the 0.6 x 2.9 model / the light-0 gate (the
    // skyless End's darkness read + the exit portal's light-11 ring
    // exclusion).
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    MobRegistry mobs;
    ASSERT_TRUE(mobs.loadDirectory(assets, "data/mobs").ok());
    const Mob* m = mobs.findByName("enderman");
    ASSERT_TRUE(m != nullptr);
    const MobProperties& p = m->properties;
    EXPECT_FLOAT_EQ(p.health, 40.0f);
    EXPECT_FLOAT_EQ(p.speed, 0.8f);
    EXPECT_FLOAT_EQ(p.detectRadius, 16.0f);
    EXPECT_FLOAT_EQ(p.attackDamage, 6.0f);
    EXPECT_TRUE(p.neutral);
    EXPECT_FALSE(p.hostile);
    EXPECT_FLOAT_EQ(p.eyeContactRange, 48.0f);
    EXPECT_DOUBLE_EQ(p.teleportIntervalMin, 2.0);
    EXPECT_DOUBLE_EQ(p.teleportIntervalMax, 6.0);
    EXPECT_TRUE(p.carriesBlocks);
    EXPECT_EQ(p.spawnMaxLight, 0);
    ASSERT_EQ(p.drops.size(), 2u);
    EXPECT_EQ(p.drops[0].item, "ender_pearl");
    EXPECT_EQ(p.drops[0].countMin, 0);
    EXPECT_EQ(p.drops[0].countMax, 1);
    EXPECT_EQ(p.drops[1].item, "ender_eye");
    EXPECT_EQ(p.drops[1].countMin, 0);
    EXPECT_EQ(p.drops[1].countMax, 1);
    EXPECT_EQ(p.xp, 5);
    // The model (the 0.6 x 2.9 — the MC exact read; the 6 boxes, the
    // 4 swings — the long-arm's signature).
    EXPECT_FLOAT_EQ(m->model.width, 0.6f);
    EXPECT_FLOAT_EQ(m->model.height, 2.9f);
    ASSERT_EQ(m->model.boxes.size(), 6u);
    ASSERT_EQ(m->model.swings.size(), 4u);
    // Every box sits in the model AABB (the parse's belt — the vanilla
    // arm overhang's project-scale inset: the boxes' x-extent <= 0.3).
    for (const MobBox& box : m->model.boxes) {
        EXPECT_LE(std::abs(box.center.x) + box.size.x / 2.0f, 0.3f + 1e-6f);
        EXPECT_LE(box.center.y + box.size.y / 2.0f, 2.9f + 1e-6f);
        EXPECT_GE(box.center.y - box.size.y / 2.0f, -1e-6f);
    }
}

TEST(ShippedEnderman, TheCarriableSetIsExactlyTheTwo) {
    // The REAL blocks (data/blocks/*.json): the carriable set is the
    // 10c-lite's two (endstone + chorus_plant — the End-related holdable
    // read; the overworld / nether set's no-consumer ruling) — a census
    // pin (a new carriable is a conscious update).
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    BlockRegistry blocks;
    ASSERT_TRUE(blocks.loadDirectory(assets, "data/blocks").ok());
    std::vector<std::string> carriables;
    for (const BlockID id : blocks.allIds()) {
        const Block& b = blocks.at(id);
        if (b.properties.carriable) {
            carriables.push_back(b.name);
            // The belt (the register-time gate's witness over the
            // shipped set): a carriable is stateless.
            EXPECT_FALSE(b.hasState()) << b.name;
        }
    }
    std::sort(carriables.begin(), carriables.end());
    EXPECT_EQ(carriables, (std::vector<std::string>{"chorus_plant",
                                                    "endstone"}));
}

TEST(ShippedEnderman, NoOtherMobCarriesTheTenCFlags) {
    // The capability census (the bit-root's shipped witness): the
    // enderman is the ONLY mob with the eye range / the teleport pair /
    // the carry flag — every other shipped mob is at the defaults (the
    // S1's structural-immunity root's data side).
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    MobRegistry mobs;
    ASSERT_TRUE(mobs.loadDirectory(assets, "data/mobs").ok());
    int eyeMobs = 0, teleportMobs = 0, carryMobs = 0;
    for (const MobID id : mobs.allIds()) {
        const MobProperties& p = mobs.at(id).properties;
        if (p.eyeContactRange > 0.0f) {
            ++eyeMobs;
        }
        if (p.teleportIntervalMin > 0.0) {
            ++teleportMobs;
        }
        if (p.carriesBlocks) {
            ++carryMobs;
        }
    }
    EXPECT_EQ(eyeMobs, 1);
    EXPECT_EQ(teleportMobs, 1);
    EXPECT_EQ(carryMobs, 1);
}

// ---------------------------------------------------------------------------
// The teleport slot's interval cadence (the random attempt over the
// data pair — the isolated witness: the first attempt lands in the
// (2, 6) s window, the re-roll keeps the cadence running).
// ---------------------------------------------------------------------------

TEST(EndermanTeleportSlot, TheIntervalCadenceFires) {
    Fixture f;
    MobEntity mob = makeEnderman(f);
    int attempts = 0;
    mob.setTeleportCallback(
        [&](const std::array<TeleportCandidate, kTeleportAttempts>&) {
            ++attempts;
            return true;
        });
    const double dt = 1.0 / 60.0;
    int firstAt = -1;
    for (int i = 0; i < 15 * 60; ++i) { // 15 s
        mob.update(dt);
        if (firstAt < 0 && attempts >= 1) {
            firstAt = i;
        }
    }
    // The constructor's first roll is in (2, 6) s: the first attempt
    // lands in frames 120..359 (the expiry's integer-frame window).
    EXPECT_GE(firstAt, 119);
    EXPECT_LE(firstAt, 359);
    // 15 s over the (2, 6) s re-rolls: at least two attempts total.
    EXPECT_GE(attempts, 2);
}
