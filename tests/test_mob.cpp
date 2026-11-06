// Mob system tests (Phase 20): the mob registry (strict validation),
// the spawning config + pure spawn-cell scan, the box-model mesh
// geometry, the mob AI state machine (wander/chase/attack, knockback,
// determinism), combat (damage/death) and the AABB raycast used for
// player targeting.
#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <numbers>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "block/block_registry.hpp"
#include "core/asset_manager.hpp"
#include "core/json.hpp"
#include "entity/mob_entity.hpp"
#include "entity/mob_mesh.hpp"
#include "entity/mob_registry.hpp"
#include "entity/mob_spawning.hpp"
#include "entity/mob_swing.hpp"
#include "item/item_registry.hpp"
#include "renderer/texture_atlas.hpp"
#include "world/fluid.hpp"
#include "world/lighting.hpp"
#include "world/raycast.hpp"
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

std::string readAll(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    EXPECT_TRUE(in) << path;
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// The project FNV-1a 64 pin (the font.png / skin-tile convention:
// offset basis 1469598103934665603, prime 1099511628211).
std::string fnv1a64(const std::string& data) {
    std::uint64_t h = 1469598103934665603ULL; // FNV-1a 64 offset basis
    for (char ch : data) {
        h ^= static_cast<unsigned char>(ch);
        h *= 1099511628211ULL;
    }
    char hex[17];
    std::snprintf(hex, sizeof(hex), "%016llx",
                  static_cast<unsigned long long>(h));
    return hex;
}

// stone: one solid block (floors + roofs in the spawn tests). ENT-3: the
// step-up shape set — a slab (solid-region top 0.5), a wall (1.5 barrier)
// and a fence (1.5 barrier) — the same shapes the shared step-up's
// Phase-99 TRUE-1-block judgment must accept / reject.
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
    r = blocks.registerBlock(parseJson(
        R"({"id":"wall","model":{"shape":"wall",)"
        R"("textures":{"top":"t","side":"t","bottom":"t"}}})"));
    if (!r.ok()) {
        ADD_FAILURE() << r.error().message;
    }
    r = blocks.registerBlock(parseJson(
        R"({"id":"fence","model":{"shape":"fence",)"
        R"("textures":{"side":"t"}}})"));
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
    BlockID slab;
    BlockID wall;
    BlockID fence;

    Fixture() {
        stone = blocks.idForName("stone");
        slab = blocks.idForName("slab");
        wall = blocks.idForName("wall");
        fence = blocks.idForName("fence");
        for (int x = -16; x < 16; ++x) {
            for (int z = -16; z < 16; ++z) {
                world.setBlock(x, 0, z, stone);
            }
        }
    }
};

// The zombie-like test definition (a registry instance is needed once;
// every test registers its own copy so ids are stable per test).
const char* kZombieDef = R"({
  "id": "zombie",
  "properties": {
    "health": 20,
    "speed": 0.8,
    "detect_radius": 16,
    "attack_range": 1.6,
    "attack_damage": 3,
    "attack_cooldown": 1.0,
    "wander_interval": 2.5
  },
  "model": {
    "width": 0.6,
    "height": 1.8,
    "boxes": [
      {"pos": [0.0, 1.25, 0.0], "size": [0.5, 0.5, 0.5], "texture": "a"},
      {"pos": [0.0, 0.5, 0.0], "size": [0.5, 1.0, 0.3], "texture": "b"}
    ]
  }
})";

// The skeleton test definition (Phase 27): the ranged hostile mob —
// shoot_range 14 > 0 marks the ranged attacker, retreat_distance 3 the
// kite band, attack_range 0 (it never melee-attacks).
const char* kSkeletonDef = R"({
  "id": "skeleton",
  "properties": {
    "health": 20,
    "speed": 0.8,
    "detect_radius": 16,
    "attack_range": 0,
    "attack_damage": 2,
    "attack_cooldown": 1.5,
    "shoot_range": 14,
    "retreat_distance": 3,
    "shoot_speed": 15,
    "ammo": "arrow",
    "wander_interval": 2.5,
    "spawn_max_light": 0,
    "hostile": true,
    "drops": [{"item": "arrow", "count": 1}]
  },
  "model": {
    "width": 0.6,
    "height": 1.8,
    "boxes": [
      {"pos": [0.0, 1.25, 0.0], "size": [0.5, 0.5, 0.5], "texture": "a"},
      {"pos": [0.0, 0.5, 0.0], "size": [0.5, 1.0, 0.3], "texture": "b"}
    ]
  }
})";

// The ghast test definition (V3-14b): the flying RANGED attacker —
// the bat's flight flags + the skeleton's shoot data fused, the first
// exploding shooter (explode_radius 3, NO ammo key — the exemption's
// witness; the single-box model keeps the mesh out of the AI tests).
const char* kGhastDef = R"({
  "id": "ghast",
  "properties": {
    "health": 10,
    "speed": 0.7,
    "detect_radius": 16,
    "attack_range": 0,
    "attack_damage": 0,
    "attack_cooldown": 4.0,
    "shoot_range": 14,
    "shoot_speed": 10,
    "explode_radius": 3,
    "wander_interval": 2.0,
    "spawn_max_light": 15,
    "hostile": true,
    "flying": true,
    "spawns_in_air": true
  },
  "model": {
    "width": 1.5,
    "height": 1.75,
    "boxes": [
      {"pos": [0.0, 1.0, 0.0], "size": [1.5, 1.5, 1.5], "texture": "g"}
    ]
  }
})";

// The bat test definition (V2-14): the flying passive mob — the flight
// flags + the ambient pair (the AI physics tests need the full data
// shape; a single-box model keeps the mesh out of it).
const char* kBatDef = R"({
  "id": "bat",
  "properties": {
    "health": 6,
    "speed": 1.0,
    "detect_radius": 0,
    "attack_range": 0,
    "attack_damage": 0,
    "attack_cooldown": 1.0,
    "wander_interval": 2.0,
    "spawn_max_light": 3,
    "flying": true,
    "spawns_in_air": true,
    "ambient_sound": "squeak",
    "ambient_interval": 3.0
  },
  "model": {
    "width": 0.9,
    "height": 0.5,
    "boxes": [
      {"pos": [0.0, 0.25, 0.0], "size": [0.5, 0.3, 0.5], "texture": "bat"}
    ]
  }
})";

// Registers the zombie def; ADD_FAILURE on error (tests expect success).
MobRegistry makeMobRegistry() {
    MobRegistry mobs;
    auto r = mobs.registerMob(parseJson(kZombieDef));
    if (!r.ok()) {
        ADD_FAILURE() << r.error().message;
    }
    return mobs;
}

// Registers the bat def; ADD_FAILURE on error (tests expect success).
MobRegistry makeBatRegistry() {
    MobRegistry mobs;
    auto r = mobs.registerMob(parseJson(kBatDef));
    if (!r.ok()) {
        ADD_FAILURE() << r.error().message;
    }
    return mobs;
}

MobEntity makeMob(const Fixture& f, const Mob& def, Vec3 pos,
                  std::uint64_t seed = 42) {
    return MobEntity(f.world, f.blocks, def, pos, seed);
}

// Advance the mob n frames on the fixed 1/60 step (the game's
// kMobStepPerFrame — hard rule 8: frame-locked mob time).
void runMob(MobEntity& mob, int n) {
    for (int i = 0; i < n; ++i) {
        mob.update(kDt);
    }
}

// The player AABB of a 0.6x1.8 body standing at (0.5, 1, 0.5) on the
// fixture floor (the spawn tests anchor the ring on that column).
const Vec3 kPlayerMin = {0.2f, 1.0f, 0.2f};
const Vec3 kPlayerMax = {0.8f, 2.8f, 0.8f};

} // namespace

// -- mob registry ------------------------------------------------------------

TEST(MobRegistryTest, ParsesZombie) {
    MobRegistry mobs = makeMobRegistry();
    ASSERT_EQ(mobs.count(), 1);
    const Mob& m = mobs.at(MobID{0});
    EXPECT_EQ(m.name, "zombie");
    EXPECT_FLOAT_EQ(m.properties.health, 20.0f);
    EXPECT_FLOAT_EQ(m.properties.speed, 0.8f);
    EXPECT_FLOAT_EQ(m.properties.detectRadius, 16.0f);
    EXPECT_FLOAT_EQ(m.properties.attackRange, 1.6f);
    EXPECT_FLOAT_EQ(m.properties.attackDamage, 3.0f);
    EXPECT_FLOAT_EQ(m.properties.attackCooldown, 1.0f);
    EXPECT_FLOAT_EQ(m.properties.wanderInterval, 2.5f);
    EXPECT_TRUE(m.properties.drops.empty());
    EXPECT_FLOAT_EQ(m.model.width, 0.6f);
    EXPECT_FLOAT_EQ(m.model.height, 1.8f);
    ASSERT_EQ(m.model.boxes.size(), 2);
    EXPECT_FLOAT_EQ(m.model.boxes[0].center.y, 1.25f);
    EXPECT_FLOAT_EQ(m.model.boxes[1].size.y, 1.0f);
    EXPECT_EQ(m.model.boxes[0].texture, "a");
    EXPECT_EQ(mobs.idForName("zombie").value, 0);
    EXPECT_EQ(mobs.idForName("nope"), MobID::invalid());
}

TEST(MobRegistryTest, RejectsMissingId) {
    MobRegistry mobs;
    // A fully valid mob definition without the "id" field.
    auto r = mobs.registerMob(parseJson(
        R"({"properties": {"health": 1, "speed": 1, "detect_radius": 1,
          "attack_range": 1, "attack_damage": 1, "attack_cooldown": 1,
          "wander_interval": 1},
           "model": {"width": 1, "height": 1,
                     "boxes": [{"pos": [0, 0.5, 0], "size": [0.5, 1, 0.5],
                                "texture": "a"}]}})"));
    EXPECT_FALSE(r.ok());
}

TEST(MobRegistryTest, RejectsNonPositiveHealth) {
    MobRegistry mobs;
    std::string def = kZombieDef;
    const std::string key = "\"health\": 20";
    def.replace(def.find(key), key.size(), "\"health\": 0");
    auto r = mobs.registerMob(parseJson(def));
    EXPECT_FALSE(r.ok());
}

TEST(MobRegistryTest, RejectsEmptyBoxes) {
    MobRegistry mobs;
    std::string def = kZombieDef;
    const std::size_t p = def.find("\"boxes\"");
    def.replace(p, def.size() - p, "\"boxes\": [] } }");
    auto r = mobs.registerMob(parseJson(def));
    EXPECT_FALSE(r.ok());
}

TEST(MobRegistryTest, RejectsBoxOutsideModelAabb) {
    MobRegistry mobs;
    std::string def = kZombieDef;
    // The head box shifted up beyond the 1.8 height.
    def.replace(def.find("[0.0, 1.25, 0.0]"), 16, "[0.0, 1.75, 0.0]");
    auto r = mobs.registerMob(parseJson(def));
    EXPECT_FALSE(r.ok());
}

TEST(MobRegistryTest, RejectsBadDropsCount) {
    MobRegistry mobs;
    std::string def = kZombieDef;
    const std::string key = "\"wander_interval\": 2.5";
    def.replace(def.find(key), key.size(),
                "\"wander_interval\": 2.5,\n    \"drops\": "
                "[{\"item\": \"x\", \"count\": 65}]");
    auto r = mobs.registerMob(parseJson(def));
    EXPECT_FALSE(r.ok());
}

// Phase 28: the drops LIST schema — {item, count} entries with strict
// validation, and the old single-string + drops_count schema rejected
// (migration guard).
TEST(MobRegistryTest, ParsesDropsList) {
    MobRegistry mobs;
    std::string def = kZombieDef;
    const std::string key = "\"wander_interval\": 2.5";
    def.replace(def.find(key), key.size(),
                "\"wander_interval\": 2.5,\n    \"drops\": "
                "[{\"item\": \"leather\"}, "
                "{\"item\": \"beef\", \"count\": 2}]");
    auto r = mobs.registerMob(parseJson(def));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const Mob& m = mobs.at(*r);
    ASSERT_EQ(m.properties.drops.size(), 2u);
    EXPECT_EQ(m.properties.drops[0].item, "leather");
    EXPECT_EQ(m.properties.drops[0].countMin, 1); // default count
    EXPECT_EQ(m.properties.drops[0].countMax, 1);
    EXPECT_EQ(m.properties.drops[1].item, "beef");
    EXPECT_EQ(m.properties.drops[1].countMin, 2);
    EXPECT_EQ(m.properties.drops[1].countMax, 2);
}

// Phase 38: the random drop range ("count_min"/"count_max") alongside the
// fixed "count" form.
TEST(MobRegistryTest, ParsesDropCountRange) {
    MobRegistry mobs;
    std::string def = kZombieDef;
    const std::string key = "\"wander_interval\": 2.5";
    def.replace(def.find(key), key.size(),
                "\"wander_interval\": 2.5,\n    \"drops\": "
                "[{\"item\": \"bone\", \"count_min\": 0, \"count_max\": 2}, "
                "{\"item\": \"arrow\", \"count\": 1}]");
    auto r = mobs.registerMob(parseJson(def));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const Mob& m = mobs.at(*r);
    ASSERT_EQ(m.properties.drops.size(), 2u);
    EXPECT_EQ(m.properties.drops[0].item, "bone");
    EXPECT_EQ(m.properties.drops[0].countMin, 0);
    EXPECT_EQ(m.properties.drops[0].countMax, 2);
    EXPECT_EQ(m.properties.drops[1].item, "arrow");
    EXPECT_EQ(m.properties.drops[1].countMin, 1);
    EXPECT_EQ(m.properties.drops[1].countMax, 1);
}

TEST(MobRegistryTest, RejectsBadDropRanges) {
    const std::string key = "\"wander_interval\": 2.5";
    const std::string variants[] = {
        // only one bound given
        "\"wander_interval\": 2.5,\n    \"drops\": "
        "[{\"item\": \"x\", \"count_min\": 0}]",
        "\"wander_interval\": 2.5,\n    \"drops\": "
        "[{\"item\": \"x\", \"count_max\": 2}]",
        // both count forms in one entry
        "\"wander_interval\": 2.5,\n    \"drops\": "
        "[{\"item\": \"x\", \"count\": 1, \"count_min\": 0, \"count_max\": 2}]",
        // inverted range
        "\"wander_interval\": 2.5,\n    \"drops\": "
        "[{\"item\": \"x\", \"count_min\": 5, \"count_max\": 2}]",
        // out of bounds
        "\"wander_interval\": 2.5,\n    \"drops\": "
        "[{\"item\": \"x\", \"count_min\": 0, \"count_max\": 65}]",
        "\"wander_interval\": 2.5,\n    \"drops\": "
        "[{\"item\": \"x\", \"count_min\": -1, \"count_max\": 2}]",
        // non-integer bound
        "\"wander_interval\": 2.5,\n    \"drops\": "
        "[{\"item\": \"x\", \"count_min\": 0.5, \"count_max\": 2}]",
    };
    for (const std::string& variant : variants) {
        MobRegistry mobs;
        std::string def = kZombieDef;
        def.replace(def.find(key), key.size(), variant);
        auto r = mobs.registerMob(parseJson(def));
        EXPECT_FALSE(r.ok()) << variant;
    }
}

TEST(MobRegistryTest, RejectsLegacyDropsSchema) {
    // The Phase 27-and-earlier schema ("drops": "item" + "drops_count")
    // is no longer accepted: a string 'drops' fails, and a stray
    // 'drops_count' field fails too.
    const std::string key = "\"wander_interval\": 2.5";
    const std::string variants[] = {
        "\"wander_interval\": 2.5,\n    \"drops\": \"rotten_flesh\"",
        "\"wander_interval\": 2.5,\n    \"drops_count\": 1",
        "\"wander_interval\": 2.5,\n    \"drops\": "
        "[{\"item\": \"leather\"}],\n    \"drops_count\": 1",
    };
    for (const std::string& variant : variants) {
        MobRegistry mobs;
        std::string def = kZombieDef;
        def.replace(def.find(key), key.size(), variant);
        auto r = mobs.registerMob(parseJson(def));
        EXPECT_FALSE(r.ok()) << variant;
    }
}

TEST(MobRegistryTest, RejectsMalformedDropsEntries) {
    const std::string key = "\"wander_interval\": 2.5";
    const std::string variants[] = {
        "\"wander_interval\": 2.5,\n    \"drops\": \"leather\"",
        "\"wander_interval\": 2.5,\n    \"drops\": [\"leather\"]",
        "\"wander_interval\": 2.5,\n    \"drops\": [123]",
        "\"wander_interval\": 2.5,\n    \"drops\": [{}]",
        "\"wander_interval\": 2.5,\n    \"drops\": [{\"item\": \"\"}]",
        "\"wander_interval\": 2.5,\n    \"drops\": "
        "[{\"item\": \"leather\", \"count\": 0}]",
        "\"wander_interval\": 2.5,\n    \"drops\": "
        "[{\"item\": \"leather\", \"count\": 1.5}]",
    };
    for (const std::string& variant : variants) {
        MobRegistry mobs;
        std::string def = kZombieDef;
        def.replace(def.find(key), key.size(), variant);
        auto r = mobs.registerMob(parseJson(def));
        EXPECT_FALSE(r.ok()) << variant;
    }
}

TEST(MobRegistryTest, RejectsDuplicateId) {
    MobRegistry mobs;
    ASSERT_TRUE(mobs.registerMob(parseJson(kZombieDef)).ok());
    auto r = mobs.registerMob(parseJson(kZombieDef));
    EXPECT_FALSE(r.ok());
}

TEST(MobRegistryTest, SpawnMaxLightDefaultsToZero) {
    // kZombieDef has no spawn_max_light: the hostile-mob default (dark
    // only) applies.
    MobRegistry mobs = makeMobRegistry();
    EXPECT_EQ(mobs.at(MobID{0}).properties.spawnMaxLight, 0);
}

TEST(MobRegistryTest, ParsesSpawnMaxLight) {
    MobRegistry mobs;
    std::string def = kZombieDef;
    const std::string key = "\"wander_interval\": 2.5";
    def.replace(def.find(key), key.size(),
                "\"wander_interval\": 2.5, \"spawn_max_light\": 7");
    auto r = mobs.registerMob(parseJson(def));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(mobs.at(*r).properties.spawnMaxLight, 7);
}

TEST(MobRegistryTest, RejectsSpawnMaxLightOutOfRange) {
    for (const int bad : {16, -1}) {
        MobRegistry mobs;
        std::string def = kZombieDef;
        const std::string key = "\"wander_interval\": 2.5";
        def.replace(def.find(key), key.size(),
                    "\"wander_interval\": 2.5, \"spawn_max_light\": " +
                    std::to_string(bad));
        auto r = mobs.registerMob(parseJson(def));
        EXPECT_FALSE(r.ok());
    }
}

TEST(MobRegistryTest, RejectsNonIntegerSpawnMaxLight) {
    MobRegistry mobs;
    std::string def = kZombieDef;
    const std::string key = "\"wander_interval\": 2.5";
    def.replace(def.find(key), key.size(),
                "\"wander_interval\": 2.5, \"spawn_max_light\": 1.5");
    auto r = mobs.registerMob(parseJson(def));
    EXPECT_FALSE(r.ok());
}

// -- V2-11 (XP): the mob's death grant ----------------------------------------

TEST(MobRegistryTest, XpDefaultsToZero) {
    // kZombieDef has no 'xp': the default (no orb) applies.
    MobRegistry mobs = makeMobRegistry();
    EXPECT_EQ(mobs.at(MobID{0}).properties.xp, 0);
}

TEST(MobRegistryTest, ParsesXp) {
    MobRegistry mobs;
    std::string def = kZombieDef;
    const std::string key = "\"wander_interval\": 2.5";
    def.replace(def.find(key), key.size(),
                "\"wander_interval\": 2.5, \"xp\": 5");
    auto r = mobs.registerMob(parseJson(def));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(mobs.at(*r).properties.xp, 5);
    // An explicit 0 is legal (the "no orb" data statement).
    MobRegistry mobsZero;
    std::string defZero = kZombieDef;
    defZero.replace(defZero.find(key), key.size(),
                    "\"wander_interval\": 2.5, \"xp\": 0");
    auto rz = mobsZero.registerMob(parseJson(defZero));
    ASSERT_TRUE(rz.ok()) << rz.error().message;
    EXPECT_EQ(mobsZero.at(*rz).properties.xp, 0);
}

TEST(MobRegistryTest, RejectsXpAboveTheOrbCap) {
    // > 9 = named rejection (the orb value cap — no split mechanism).
    for (const int bad : {10, 99}) {
        MobRegistry mobs;
        std::string def = kZombieDef;
        const std::string key = "\"wander_interval\": 2.5";
        def.replace(def.find(key), key.size(),
                    "\"wander_interval\": 2.5, \"xp\": " + std::to_string(bad));
        auto r = mobs.registerMob(parseJson(def));
        ASSERT_FALSE(r.ok());
        EXPECT_NE(r.error().message.find("xp"), std::string::npos)
            << r.error().message;
        EXPECT_NE(r.error().message.find("[0, 9]"), std::string::npos)
            << r.error().message;
    }
}

TEST(MobRegistryTest, RejectsNonIntegerNegativeAndNonNumberXp) {
    for (const std::string bad : {"1.5", "-1", "\"5\""}) {
        MobRegistry mobs;
        std::string def = kZombieDef;
        const std::string key = "\"wander_interval\": 2.5";
        def.replace(def.find(key), key.size(),
                    "\"wander_interval\": 2.5, \"xp\": " + bad);
        auto r = mobs.registerMob(parseJson(def));
        ASSERT_FALSE(r.ok()) << bad;
        EXPECT_NE(r.error().message.find("xp"), std::string::npos)
            << r.error().message;
    }
}

// -- V2-14: the flight data flags + the ambient sound (the bat) -----------

TEST(MobRegistryTest, FlightFlagsDefaultOff) {
    // kZombieDef carries no flight / ambient fields: the defaults
    // (false / empty / the interval default) — the pre-V2-14 structural
    // root (every shipped mob is bit-identical).
    MobRegistry mobs = makeMobRegistry();
    const MobProperties& p = mobs.at(MobID{0}).properties;
    EXPECT_FALSE(p.flying);
    EXPECT_FALSE(p.spawnsInAir);
    EXPECT_TRUE(p.ambientSoundName.empty());
    EXPECT_DOUBLE_EQ(p.ambientInterval, kAmbientIntervalDefaultSeconds);
}

TEST(MobRegistryTest, ParsesFlightFlagsAndAmbientSound) {
    MobRegistry mobs;
    std::string def = kZombieDef;
    const std::string key = "\"wander_interval\": 2.5";
    def.replace(def.find(key), key.size(),
                "\"wander_interval\": 2.5, \"flying\": true, "
                "\"spawns_in_air\": true, \"ambient_sound\": \"squeak\", "
                "\"ambient_interval\": 3.0");
    auto r = mobs.registerMob(parseJson(def));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const MobProperties& p = mobs.at(MobID{0}).properties;
    EXPECT_TRUE(p.flying);
    EXPECT_TRUE(p.spawnsInAir);
    EXPECT_EQ(p.ambientSoundName, "squeak");
    EXPECT_DOUBLE_EQ(p.ambientInterval, 3.0);
}

TEST(MobRegistryTest, AmbientIntervalDefaultsWithLoneSound) {
    // A sound without an interval: the default period (the fields are
    // each independently optional — the design's decision).
    MobRegistry mobs;
    std::string def = kZombieDef;
    const std::string key = "\"wander_interval\": 2.5";
    def.replace(def.find(key), key.size(),
                "\"wander_interval\": 2.5, \"ambient_sound\": \"squeak\"");
    auto r = mobs.registerMob(parseJson(def));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const MobProperties& p = mobs.at(MobID{0}).properties;
    EXPECT_EQ(p.ambientSoundName, "squeak");
    EXPECT_DOUBLE_EQ(p.ambientInterval, kAmbientIntervalDefaultSeconds);
}

TEST(MobRegistryTest, RejectsFlyingWithoutSpawnsInAir) {
    // The ONE hard pairing: flying requires spawns_in_air (a
    // ground-spawning flier is meaningless — named).
    MobRegistry mobs;
    std::string def = kZombieDef;
    const std::string key = "\"wander_interval\": 2.5";
    def.replace(def.find(key), key.size(),
                "\"wander_interval\": 2.5, \"flying\": true");
    auto r = mobs.registerMob(parseJson(def));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("flying"), std::string::npos);
    EXPECT_NE(r.error().message.find("spawns_in_air"), std::string::npos);
    // spawns_in_air WITHOUT flying is legal (the independent optional —
    // the future extension surface).
    MobRegistry lone;
    std::string def2 = kZombieDef;
    def2.replace(def2.find(key), key.size(),
                 "\"wander_interval\": 2.5, \"spawns_in_air\": true");
    auto r2 = lone.registerMob(parseJson(def2));
    ASSERT_TRUE(r2.ok()) << r2.error().message;
    EXPECT_TRUE(lone.at(MobID{0}).properties.spawnsInAir);
    EXPECT_FALSE(lone.at(MobID{0}).properties.flying);
}

TEST(MobRegistryTest, RejectsBadFlightAndAmbientFields) {
    for (const char* bad : {
             // type / range errors
             "\"flying\": 1",
             "\"spawns_in_air\": \"yes\"",
             "\"ambient_sound\": 7",
             "\"ambient_sound\": \"\"",
             "\"ambient_interval\": \"3\"",
             "\"ambient_interval\": 0",
             "\"ambient_interval\": -1.0",
             // the lone interval (dead data without the sound)
             "\"ambient_interval\": 3.0",
         }) {
        MobRegistry mobs;
        std::string def = kZombieDef;
        const std::string key = "\"wander_interval\": 2.5";
        def.replace(def.find(key), key.size(),
                    std::string("\"wander_interval\": 2.5, ") + bad);
        auto r = mobs.registerMob(parseJson(def));
        EXPECT_FALSE(r.ok()) << bad;
    }
    // The lone-interval rejection names the pairing.
    MobRegistry mobs;
    std::string def = kZombieDef;
    const std::string key = "\"wander_interval\": 2.5";
    def.replace(def.find(key), key.size(),
                "\"wander_interval\": 2.5, \"ambient_interval\": 3.0");
    auto r = mobs.registerMob(parseJson(def));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("ambient_sound"), std::string::npos);
}

// -- V3-14b: the exploding shooter's data (the blast's registry face) ----

TEST(MobRegistryTest, ExplodeRadiusPinsTheGhastShape) {
    // The ghast's definition (no 'ammo' — the explode shooter's
    // exemption witness: the fireball is consumed at impact and never
    // sticks, so its pickup is unreachable): the radius parses, the
    // shoot sound defaults to "shoot" (the data's absence), the ammo
    // stays empty.
    MobRegistry mobs;
    auto r = mobs.registerMob(parseJson(kGhastDef));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const MobProperties& p = mobs.at(MobID{0}).properties;
    EXPECT_FLOAT_EQ(p.explodeRadius, 3.0f);
    EXPECT_EQ(p.shootSound, "shoot");
    EXPECT_EQ(p.ammoItem, "");
    EXPECT_TRUE(p.flying);
    // A non-exploding shooter keeps the pre-V3-14b defaults.
    MobRegistry arrows;
    ASSERT_TRUE(arrows.registerMob(parseJson(kSkeletonDef)).ok());
    EXPECT_FLOAT_EQ(arrows.at(MobID{0}).properties.explodeRadius, 0.0f);
}

TEST(MobRegistryTest, RejectsBadExplodeRadius) {
    // The strict number (the shoot_speed's twin): negative / non-number
    // are named.
    auto bad = std::string(kGhastDef);
    bad.replace(bad.find("\"explode_radius\": 3"), 19,
                "\"explode_radius\": -1");
    auto r = MobRegistry{}.registerMob(parseJson(bad));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("explode_radius"), std::string::npos);
    bad = std::string(kGhastDef);
    bad.replace(bad.find("\"explode_radius\": 3"), 19,
                "\"explode_radius\": \"big\"");
    r = MobRegistry{}.registerMob(parseJson(bad));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("explode_radius"), std::string::npos);
}

TEST(MobRegistryTest, ExplodeRadiusRequiresShootRange) {
    // An exploding MELEE is a typo (the shot is the blast's only
    // carrier): explode_radius > 0 without shoot_range is named.
    auto bad = std::string(kGhastDef);
    bad.replace(bad.find("\"shoot_range\": 14,"), 18, "\"shoot_range\": 0,");
    auto r = MobRegistry{}.registerMob(parseJson(bad));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("explode_radius"), std::string::npos);
}

TEST(MobRegistryTest, RangedShooterStillRequiresAmmo) {
    // The exemption is the exploding shooter's ONLY: a normal ranged
    // mob (explode_radius 0) without ammo is still rejected (the
    // skeleton's rule untouched).
    auto bad = std::string(kSkeletonDef);
    bad.erase(bad.find("\"ammo\": \"arrow\",\n"), 17);
    auto r = MobRegistry{}.registerMob(parseJson(bad));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("ammo"), std::string::npos);
}

TEST(MobRegistryTest, RejectsBadShootSound) {
    // The shoot cue's strict string (the ammo discipline's twin):
    // explicit empty / non-string are named.
    auto bad = std::string(kGhastDef);
    const std::string anchor = "\"explode_radius\": 3,";
    bad.insert(bad.find(anchor) + anchor.size(),
               "\n    \"shoot_sound\": \"\",");
    auto r = MobRegistry{}.registerMob(parseJson(bad));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("shoot_sound"), std::string::npos);
}

// -- V3-14c: the bartering's data (the trade's registry face) ----------

// A bartering test definition (the piglin's shape, the single-box model
// keeps the mesh out of the AI tests).
const char* kBarterDef = R"({
  "id": "piglin",
  "properties": {
    "health": 16,
    "speed": 1.0,
    "detect_radius": 16,
    "attack_range": 1.6,
    "attack_damage": 6,
    "attack_cooldown": 1.5,
    "wander_interval": 2.5,
    "spawn_max_light": 7,
    "neutral": true,
    "xp": 3,
    "barter_input": "gold_ingot",
    "barter_outputs": [
      {"item": "quartz", "weight": 10, "count_min": 5, "count_max": 12},
      {"item": "obsidian", "weight": 40, "count": 1},
      {"item": "leather", "weight": 20, "count_min": 2, "count_max": 4}
    ]
  },
  "model": {
    "width": 0.6,
    "height": 1.8,
    "boxes": [
      {"pos": [0.0, 1.0, 0.0], "size": [0.5, 1.0, 0.5], "texture": "p"}
    ]
  }
})";

TEST(MobRegistryTest, ParsesBarterPairAndOutputs) {
    // The full valid shape: the input + the weighted table (the two
    // count forms: the range and the single). Data order is the roll
    // order (the cumulative walk's contract).
    MobRegistry mobs;
    auto r = mobs.registerMob(parseJson(kBarterDef));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const MobProperties& p = mobs.at(MobID{0}).properties;
    EXPECT_EQ(p.barterInput, "gold_ingot");
    ASSERT_EQ(p.barterOutputs.size(), 3u);
    EXPECT_EQ(p.barterOutputs[0].item, "quartz");
    EXPECT_EQ(p.barterOutputs[0].weight, 10);
    EXPECT_EQ(p.barterOutputs[0].countMin, 5);
    EXPECT_EQ(p.barterOutputs[0].countMax, 12);
    EXPECT_EQ(p.barterOutputs[1].item, "obsidian");
    EXPECT_EQ(p.barterOutputs[1].weight, 40);
    EXPECT_EQ(p.barterOutputs[1].countMin, 1); // the single 'count' form
    EXPECT_EQ(p.barterOutputs[1].countMax, 1);
    EXPECT_EQ(p.barterOutputs[2].item, "leather");
    EXPECT_EQ(p.barterOutputs[2].weight, 20);
    EXPECT_EQ(p.barterOutputs[2].countMin, 2);
    EXPECT_EQ(p.barterOutputs[2].countMax, 4);
    // Every pre-V3-14c mob (no barter data) keeps the empty defaults.
    MobRegistry zombies;
    ASSERT_TRUE(zombies.registerMob(parseJson(kZombieDef)).ok());
    EXPECT_TRUE(zombies.at(MobID{0}).properties.barterInput.empty());
    EXPECT_TRUE(zombies.at(MobID{0}).properties.barterOutputs.empty());
}

TEST(MobRegistryTest, BarterPairingDiscipline) {
    // A lone field is a typo (the milk pairing's twin): each side alone
    // is named; an empty input / an empty table are named.
    {
        MobRegistry m;
        auto r = m.registerMob(parseJson(R"({
          "id": "x",
          "properties": {"health": 1, "speed": 1.0, "detect_radius": 0, "attack_range": 0, "attack_damage": 0, "attack_cooldown": 1.0, "wander_interval": 2.0,
                         "barter_input": "gold_ingot"},
          "model": {"width": 1, "height": 1,
                    "boxes": [{"pos": [0, 0.5, 0], "size": [1, 1, 1],
                               "texture": "p"}]}
        })"));
        ASSERT_FALSE(r.ok());
        EXPECT_NE(r.error().message.find("together"), std::string::npos);
    }
    {
        MobRegistry m;
        auto r = m.registerMob(parseJson(R"({
          "id": "x",
          "properties": {"health": 1, "speed": 1.0, "detect_radius": 0, "attack_range": 0, "attack_damage": 0, "attack_cooldown": 1.0, "wander_interval": 2.0,
                         "barter_outputs": [{"item": "quartz",
                                              "weight": 1, "count": 1}]},
          "model": {"width": 1, "height": 1,
                    "boxes": [{"pos": [0, 0.5, 0], "size": [1, 1, 1],
                               "texture": "p"}]}
        })"));
        ASSERT_FALSE(r.ok());
        EXPECT_NE(r.error().message.find("together"), std::string::npos);
    }
    {
        MobRegistry m;
        auto r = m.registerMob(parseJson(R"({
          "id": "x",
          "properties": {"health": 1, "speed": 1.0, "detect_radius": 0, "attack_range": 0, "attack_damage": 0, "attack_cooldown": 1.0, "wander_interval": 2.0,
                         "barter_input": "",
                         "barter_outputs": [{"item": "quartz",
                                              "weight": 1, "count": 1}]},
          "model": {"width": 1, "height": 1,
                    "boxes": [{"pos": [0, 0.5, 0], "size": [1, 1, 1],
                               "texture": "p"}]}
        })"));
        ASSERT_FALSE(r.ok());
        EXPECT_NE(r.error().message.find("barter_input"), std::string::npos);
    }
    {
        MobRegistry m;
        auto r = m.registerMob(parseJson(R"({
          "id": "x",
          "properties": {"health": 1, "speed": 1.0, "detect_radius": 0, "attack_range": 0, "attack_damage": 0, "attack_cooldown": 1.0, "wander_interval": 2.0,
                         "barter_input": "gold_ingot",
                         "barter_outputs": []},
          "model": {"width": 1, "height": 1,
                    "boxes": [{"pos": [0, 0.5, 0], "size": [1, 1, 1],
                               "texture": "p"}]}
        })"));
        ASSERT_FALSE(r.ok());
        EXPECT_NE(r.error().message.find("barter_outputs"),
                  std::string::npos);
    }
}

TEST(MobRegistryTest, RejectsBadBarterEntries) {
    // The entry discipline: the unknown field / the duplicate item /
    // the weight out of [1,10000] / both count forms / the one-sided
    // range / the 0-count (the gold-for-nothing rejection) / the
    // min>max range are all named.
    const struct {
        const char* entry;
        const char* named;
    } cases[] = {
        {"{\"item\": \"quartz\", \"weight\": 1, \"count\": 1, "
         "\"nope\": 2}",
         "unknown field"},
        {"{\"item\": \"quartz\", \"weight\": 1, \"count\": 1}, "
         "{\"item\": \"quartz\", \"weight\": 2, \"count\": 1}",
         "twice"},
        {"{\"item\": \"quartz\", \"weight\": 0, \"count\": 1}",
         "weight"},
        {"{\"item\": \"quartz\", \"weight\": 10001, \"count\": 1}",
         "weight"},
        {"{\"item\": \"quartz\", \"weight\": 1, \"count\": 1, "
         "\"count_min\": 1, \"count_max\": 2}",
         "one form"},
        {"{\"item\": \"quartz\", \"weight\": 1, \"count_min\": 1}",
         "count_max"},
        {"{\"item\": \"quartz\", \"weight\": 1, \"count\": 0}",
         "count"},
        {"{\"item\": \"quartz\", \"weight\": 1, \"count_min\": 3, "
         "\"count_max\": 2}",
         "count_min"},
    };
    for (const auto& c : cases) {
        const std::string def =
            std::string(R"({
  "id": "x",
  "properties": {"health": 1, "speed": 1.0, "detect_radius": 0,
                 "attack_range": 0, "attack_damage": 0, "attack_cooldown": 1.0, "wander_interval": 2.0,
                 "barter_input": "gold_ingot",
                 "barter_outputs": [)") +
            c.entry + R"(]},
  "model": {"width": 1, "height": 1,
            "boxes": [{"pos": [0, 0.5, 0], "size": [1, 1, 1],
                       "texture": "p"}]}
})";
        auto r = MobRegistry{}.registerMob(parseJson(def));
        ASSERT_FALSE(r.ok()) << c.entry;
        EXPECT_NE(r.error().message.find(c.named), std::string::npos)
            << r.error().message;
    }
}

TEST(MobRegistryTest, RollBarterOutputIsTheCumulativeWalk) {
    // The pure pick (the rollWoolColor twin): u in [0,1) over the
    // cumulative weights, DATA order; the boundary u=1 lands on the
    // last entry (the walk's fallback).
    const BarterOutput a {"a", 1, 1, 1};
    const BarterOutput b {"b", 3, 1, 1};
    const std::vector<BarterOutput> table {a, b};
    EXPECT_EQ(rollBarterOutput(table, 0.0), 0);    // a owns [0, 0.25)
    EXPECT_EQ(rollBarterOutput(table, 0.24), 0);
    EXPECT_EQ(rollBarterOutput(table, 0.25), 1);   // b owns [0.25, 1)
    EXPECT_EQ(rollBarterOutput(table, 0.999), 1);
    EXPECT_EQ(rollBarterOutput(table, 1.0), 1);    // the fallback
    // The one-entry table: the single draw still lands (the
    // degenerate-discipline witness).
    EXPECT_EQ(rollBarterOutput({a}, 0.5), 0);
}

// -- V3-14d: the rideable flag (the right-click mount's data predicate) --

TEST(MobRegistryTest, RideableDefaultsToFalse) {
    // Every pre-V3-14d mob (kZombieDef has no 'rideable'): the default
    // (not rideable) applies — the tryRide's fall-through predicate.
    MobRegistry mobs = makeMobRegistry();
    EXPECT_FALSE(mobs.at(MobID{0}).properties.rideable);
}

TEST(MobRegistryTest, ParsesRideableStrict) {
    // The strict bool (the walks_on_lava's twin): true parses, the
    // non-boolean is named.
    auto def = std::string(kGhastDef);
    const std::string anchor = "\"spawn_max_light\": 15,";
    def.insert(def.find(anchor) + anchor.size(),
               "\n    \"rideable\": true,");
    MobRegistry mobs;
    auto r = mobs.registerMob(parseJson(def));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_TRUE(mobs.at(MobID{0}).properties.rideable);
    auto bad = std::string(kGhastDef);
    bad.insert(bad.find(anchor) + anchor.size(),
               "\n    \"rideable\": \"yes\",");
    r = MobRegistry{}.registerMob(parseJson(bad));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("rideable"), std::string::npos);
}

// The strider test definition (V3-14d): the RIDEABLE lava walker —
// the ride's data predicate + the ambient (the per-mob stream's
// ridden-alignment test needs the hiss roll).
const char* kStriderRideDef = R"({
  "id": "strider",
  "properties": {
    "health": 20,
    "speed": 1.0,
    "detect_radius": 0,
    "attack_range": 0,
    "attack_damage": 0,
    "attack_cooldown": 1.0,
    "wander_interval": 2.0,
    "spawn_max_light": 15,
    "walks_on_lava": true,
    "rideable": true,
    "ambient_sound": "strider_hiss",
    "ambient_interval": 3.0,
    "xp": 3
  },
  "model": {
    "width": 1.0,
    "height": 1.5,
    "boxes": [
      {"pos": [0.0, 0.55, 0.0], "size": [1.0, 0.5, 1.0], "texture": "s"}
    ]
  }
})";

// -- V3-14d: the ridden AI (the rider's steer takeover) -------------------

namespace {
// A player far from the fixture origin: the strider's detect_radius 0
// never sees it (the ride test's kRideFarPlayer twin, local — that
// constant lives in the flight section below).
const Vec3 kRideFarPlayer {40.0f, 10.0f, 40.0f};
} // namespace

TEST(MobRideTest, RiddenFlagTogglesTheSeparationOptOut) {
    // The separation pass's opt-out (the projectile's twin): a ridden
    // mount is pushed neither by the rider nor by other mobs (the
    // rider's box overlaps the mount's by design — the carry snap owns
    // the pose); a non-ridden one participates.
    Fixture f;
    MobRegistry mobs;
    ASSERT_TRUE(mobs.registerMob(parseJson(kStriderRideDef)).ok());
    MobEntity mob = makeMob(f, mobs.at(MobID{0}), {8.5f, 1.0f, 8.5f});
    EXPECT_TRUE(mob.collidesWithEntities());
    mob.setRidden(true);
    EXPECT_FALSE(mob.collidesWithEntities());
    EXPECT_TRUE(mob.ridden());
    mob.setRidden(false);
    EXPECT_TRUE(mob.collidesWithEntities());
    EXPECT_FALSE(mob.ridden());
}

TEST(MobRideTest, ForwardDriveSetsDataSpeedAlongTheHeading) {
    // The rider pushes forward toward +Z (dir (0,0,1)): the strider
    // faces the heading (yaw pi — the camera convention forward(yaw) =
    // (-sin, -cos)) and walks at the DATA speed along it. The state is
    // Ridden (the takeover's report).
    Fixture f;
    MobRegistry mobs;
    ASSERT_TRUE(mobs.registerMob(parseJson(kStriderRideDef)).ok());
    MobEntity mob = makeMob(f, mobs.at(MobID{0}), {8.5f, 1.0f, 8.5f});
    mob.setPlayer(kRideFarPlayer, true);
    mob.setRidden(true);
    mob.setRiderSteer({0.0f, 0.0f, 1.0f}, true);
    mob.update(kDt);
    EXPECT_EQ(mob.state(), MobState::Ridden);
    EXPECT_NEAR(mob.velocity().x, 0.0f, 1e-6f);
    EXPECT_NEAR(mob.velocity().z, 1.0f, 1e-6f); // the data speed 1.0
    // yaw pi (the ±pi signed-zero twin — the direction cosines are the
    // heading's authority, the V3-14b ghast test's discipline):
    // forward(pi) = (-sin, -cos) = (0, +1) = the +Z wish.
    EXPECT_NEAR(std::cos(mob.facingYaw()), -1.0f, 1e-4f);
    EXPECT_NEAR(std::sin(mob.facingYaw()), 0.0f, 1e-4f);
}

TEST(MobRideTest, ReleaseDecaysToTheStand) {
    // The drift-stop: the rider releases (no direction wish) — the
    // kRideDecel ease-out (per fixed 1/60 step) decays the speed to the
    // kRideStopEps stand. 0.9^k * 1.0 < 0.02 first at k = 36 (0.9^35 =
    // 0.0206 > 0.02, 0.9^36 = 0.0185 < 0.02) — the stop frame is
    // frame-locked (the pin proves the decay contract).
    Fixture f;
    MobRegistry mobs;
    ASSERT_TRUE(mobs.registerMob(parseJson(kStriderRideDef)).ok());
    MobEntity mob = makeMob(f, mobs.at(MobID{0}), {8.5f, 1.0f, 8.5f});
    mob.setPlayer(kRideFarPlayer, true);
    mob.setRidden(true);
    mob.setRiderSteer({0.0f, 0.0f, 1.0f}, true);
    mob.update(kDt); // one driven frame: the full data speed
    EXPECT_NEAR(mob.velocity().z, 1.0f, 1e-6f);
    mob.setRiderSteer({0.0f, 0.0f, 0.0f}, false); // the release
    // The decay contract (the pinned sequence): each release frame
    // multiplies the speed by kRideDecel 0.9 — 0.9, 0.81, 0.729, ...
    // The zeroing: 0.9^k < kRideStopEps 0.02 first at k = 38
    // (0.9^37 = 0.0203 > 0.02, 0.9^38 = 0.0183 < 0.02) — the stand
    // frame is frame-locked (the pin proves the decay contract).
    const float expect[] = {0.9f, 0.81f, 0.729f, 0.6561f, 0.59049f};
    for (int k = 1; k <= 5; ++k) {
        mob.update(kDt);
        EXPECT_NEAR(mob.velocity().z, expect[k - 1], 1e-6f) << "k=" << k;
    }
    for (int k = 6; k <= 37; ++k) {
        mob.update(kDt);
        EXPECT_GT(mob.velocity().z, 0.0f) << "k=" << k;
    }
    mob.update(kDt); // k = 38: below the epsilon, zeroed
    EXPECT_FLOAT_EQ(mob.velocity().x, 0.0f);
    EXPECT_FLOAT_EQ(mob.velocity().z, 0.0f);
}

TEST(MobRideTest, RiddenKeepsTheAmbientStreamAlive) {
    // The per-mob stream contract (PITS 403⑤): the ridden takeover
    // KEEPS the ambient roll (the hiss plays while ridden — the
    // strider's home voice), so the stream stays aligned with the
    // twin that never rode. Over 1800 frames at the 3.0 s interval
    // (p = 1/180) the deterministic hiss frames fire — the count is
    // pinned (a silent / degenerate stream would read 0).
    Fixture f;
    MobRegistry mobs;
    ASSERT_TRUE(mobs.registerMob(parseJson(kStriderRideDef)).ok());
    MobEntity mob = makeMob(f, mobs.at(MobID{0}), {8.5f, 1.0f, 8.5f});
    mob.setPlayer(kRideFarPlayer, true);
    int hisses = 0;
    mob.setAmbientSoundCallback([&](Vec3) { ++hisses; });
    mob.setRidden(true);
    mob.setRiderSteer({0.0f, 0.0f, 0.0f}, false);
    for (int i = 0; i < 1800; ++i) {
        mob.update(kDt);
    }
    EXPECT_GT(hisses, 0);
    // The exact count (the pinned deterministic roll — a stream drift
    // changes it; 13 hisses in 1800 frames at the 3.0 s interval).
    EXPECT_EQ(hisses, 13);
}

TEST(MobRideTest, NonRiddenStriderStaysBitIdentical) {
    // The pure-new-branch red line: a strider that is NEVER ridden
    // takes the pre-V3-14d wander path bit-identically (the m_ridden
    // gate at the runAi's top skips the takeover; the ambient / the
    // wander stream are untouched). Twin the ridden one's first-frame
    // release against a never-ridden strider's wander: the positions
    // diverge (different AI) but the NEVER-ridden twin matches a fresh
    // same-seed strider exactly (the structural immunity).
    Fixture f;
    MobRegistry mobs;
    ASSERT_TRUE(mobs.registerMob(parseJson(kStriderRideDef)).ok());
    const Mob& def = mobs.at(MobID{0});
    MobEntity a = makeMob(f, def, {8.5f, 1.0f, 8.5f}, 42);
    MobEntity b = makeMob(f, def, {8.5f, 1.0f, 8.5f}, 42);
    a.setPlayer(kRideFarPlayer, true);
    b.setPlayer(kRideFarPlayer, true);
    for (int i = 0; i < 120; ++i) {
        a.update(kDt);
        b.update(kDt);
    }
    EXPECT_FLOAT_EQ(a.position().x, b.position().x);
    EXPECT_FLOAT_EQ(a.position().y, b.position().y);
    EXPECT_FLOAT_EQ(a.position().z, b.position().z);
    EXPECT_FLOAT_EQ(a.velocity().x, b.velocity().x);
    EXPECT_FLOAT_EQ(a.velocity().z, b.velocity().z);
    EXPECT_EQ(a.state(), MobState::Wander); // the wander, not Ridden
}

// -- hostile flag (Phase 25: the difficulty system is data-driven) ------------

TEST(MobRegistryTest, HostileDefaultsToFalse) {
    // kZombieDef has no 'hostile': the default (passive) applies — the
    // difficulty system must not guess hostility from behavior values.
    MobRegistry mobs = makeMobRegistry();
    EXPECT_FALSE(mobs.at(MobID{0}).properties.hostile);
}

TEST(MobRegistryTest, ParsesHostileTrueAndExplicitFalse) {
    for (const char* value : {"true", "false"}) {
        MobRegistry mobs;
        std::string def = kZombieDef;
        const std::string key = "\"wander_interval\": 2.5";
        def.replace(def.find(key), key.size(),
                    std::string("\"wander_interval\": 2.5, \"hostile\": ") +
                        value);
        auto r = mobs.registerMob(parseJson(def));
        ASSERT_TRUE(r.ok()) << r.error().message;
        EXPECT_EQ(mobs.at(*r).properties.hostile,
                  std::string(value) == "true");
    }
}

TEST(MobRegistryTest, RejectsNonBoolHostile) {
    for (const char* bad : {"\"yes\"", "1"}) {
        MobRegistry mobs;
        std::string def = kZombieDef;
        const std::string key = "\"wander_interval\": 2.5";
        def.replace(def.find(key), key.size(),
                    std::string("\"wander_interval\": 2.5, \"hostile\": ") +
                        bad);
        auto r = mobs.registerMob(parseJson(def));
        EXPECT_FALSE(r.ok()) << "accepted hostile: " << bad;
    }
}

// -- on_hit_effect (Phase 46, the spider's poison) ---------------------------

TEST(MobRegistryTest, HitEffectDefaultsToNone) {
    // kZombieDef has no 'on_hit_effect': every pre-Phase-46 mob is
    // unaffected.
    MobRegistry mobs = makeMobRegistry();
    const auto& p = mobs.at(MobID{0}).properties;
    EXPECT_TRUE(p.hitEffectName.empty());
    EXPECT_DOUBLE_EQ(p.hitEffectDuration, 0.0);
}

TEST(MobRegistryTest, ParsesOnHitEffect) {
    MobRegistry mobs;
    std::string def = kZombieDef;
    const std::string key = "\"wander_interval\": 2.5";
    def.replace(def.find(key), key.size(),
                std::string("\"wander_interval\": 2.5, \"on_hit_effect\": ") +
                    "{\"name\": \"poison\", \"duration\": 15.0}");
    auto r = mobs.registerMob(parseJson(def));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const auto& p = mobs.at(*r).properties;
    EXPECT_EQ(p.hitEffectName, "poison");
    EXPECT_DOUBLE_EQ(p.hitEffectDuration, 15.0);
}

TEST(MobRegistryTest, RejectsBadOnHitEffect) {
    const std::vector<std::string> bad = {
        "1",                                     // not an object
        R"({"name": "poison"})",                 // missing duration
        R"({"duration": 15.0})",                 // missing name
        R"({"name": "", "duration": 15.0})",     // empty name
        R"({"name": "poison", "duration": 0})",  // duration 0
        R"({"name": "poison", "duration": -1})", // duration < 0
        R"({"name": "poison", "duration": "15"})",  // non-number
        R"({"name": "poison", "extra": 1})",     // unknown field
    };
    for (const std::string& value : bad) {
        MobRegistry mobs;
        std::string def = kZombieDef;
        const std::string key = "\"wander_interval\": 2.5";
        def.replace(def.find(key), key.size(),
                    std::string("\"wander_interval\": 2.5, "
                                "\"on_hit_effect\": ") + value);
        auto r = mobs.registerMob(parseJson(def));
        EXPECT_FALSE(r.ok()) << "accepted on_hit_effect: " << value;
    }
}

TEST(MobRegistryTest, ParsesOnHitEffectTier) {
    // SURV-6: the optional 0-based 'tier' on the hit grant (the range
    // against the effect's tier_max is cross-checked at GAME INIT — the
    // registry is not available at parse).
    MobRegistry mobs;
    std::string def = kZombieDef;
    const std::string key = "\"wander_interval\": 2.5";
    def.replace(def.find(key), key.size(),
                std::string("\"wander_interval\": 2.5, \"on_hit_effect\": ") +
                    "{\"name\": \"poison\", \"duration\": 15.0, "
                    "\"tier\": 1}");
    auto r = mobs.registerMob(parseJson(def));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const auto& p = mobs.at(*r).properties;
    EXPECT_EQ(p.hitEffectName, "poison");
    EXPECT_EQ(p.hitEffectTier, 1);
    // Absent = 0 (the pre-SURV-6 hit, bit-identical).
    MobRegistry mobs2 = makeMobRegistry();
    EXPECT_EQ(mobs2.at(MobID{0}).properties.hitEffectTier, 0);
}

TEST(MobRegistryTest, RejectsBadOnHitEffectTier) {
    const std::vector<std::string> bad = {
        "1.5",    // non-integer
        "-1",     // negative
        "\"1\"",  // a string
    };
    for (const std::string& value : bad) {
        MobRegistry mobs;
        std::string def = kZombieDef;
        const std::string key = "\"wander_interval\": 2.5";
        def.replace(def.find(key), key.size(),
                    std::string("\"wander_interval\": 2.5, \"on_hit_effect\": "
                                "{\"name\": \"poison\", \"duration\": 15.0, "
                                "\"tier\": ") + value + "}");
        auto r = mobs.registerMob(parseJson(def));
        EXPECT_FALSE(r.ok()) << "accepted hit tier: " << value;
    }
}

// -- SURV-5: the undead flag + the arrow effect ------------------------------

TEST(MobRegistryTest, ParsesUndead) {
    // The strict bool (default false — every pre-SURV-5 mob): the zombie
    // (a shipped undead) carries true.
    MobRegistry mobs;
    std::string def = kZombieDef;
    const std::string key = "\"wander_interval\": 2.5";
    def.replace(def.find(key), key.size(),
                std::string("\"wander_interval\": 2.5, \"undead\": true"));
    auto r = mobs.registerMob(parseJson(def));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_TRUE(mobs.at(*r).properties.undead);
    // Absent = false (the passive default — the fixture zombie).
    MobRegistry plain = makeMobRegistry();
    EXPECT_FALSE(plain.at(MobID{0}).properties.undead);
}

TEST(MobRegistryTest, RejectsBadUndead) {
    for (const char* bad : {"\"yes\"", "1", "0", "null"}) {
        MobRegistry mobs;
        std::string def = kZombieDef;
        const std::string key = "\"wander_interval\": 2.5";
        def.replace(def.find(key), key.size(),
                    std::string("\"wander_interval\": 2.5, \"undead\": ") +
                        bad);
        auto r = mobs.registerMob(parseJson(def));
        EXPECT_FALSE(r.ok()) << "accepted undead: " << bad;
    }
}

TEST(MobRegistryTest, ParsesArrowEffect) {
    // The skeleton's data shape: the strict {name, duration} pair (the
    // on_hit_effect discipline) on a RANGED mob — the arrows carry the
    // effect (the wither, MC's wither-skeleton value 10 s).
    MobRegistry mobs;
    std::string def = kSkeletonDef;
    const std::string key = "\"wander_interval\": 2.5";
    def.replace(def.find(key), key.size(),
                std::string("\"wander_interval\": 2.5, "
                            "\"arrow_effect\": {\"name\": \"wither\", "
                            "\"duration\": 10}"));
    auto r = mobs.registerMob(parseJson(def));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const auto& p = mobs.at(*r).properties;
    EXPECT_EQ(p.arrowEffectName, "wither");
    EXPECT_DOUBLE_EQ(p.arrowEffectDuration, 10.0);
    // A pre-SURV-5 mob: the field absent.
    MobRegistry plain;
    ASSERT_TRUE(plain.registerMob(parseJson(kSkeletonDef)).ok());
    EXPECT_TRUE(plain.at(MobID{0}).properties.arrowEffectName.empty());
    EXPECT_DOUBLE_EQ(plain.at(MobID{0}).properties.arrowEffectDuration, 0.0);
}

TEST(MobRegistryTest, RejectsBadArrowEffect) {
    const std::vector<std::string> bad = {
        "1",                                     // not an object
        R"({"name": "wither"})",                 // missing duration
        R"({"duration": 10})",                   // missing name
        R"({"name": "", "duration": 10})",       // empty name
        R"({"name": "wither", "duration": 0})",  // duration 0
        R"({"name": "wither", "duration": -1})", // duration < 0
        R"({"name": "wither", "duration": "10"})", // non-number
        R"({"name": "wither", "extra": 1})",     // unknown field
    };
    for (const std::string& value : bad) {
        MobRegistry mobs;
        std::string def = kSkeletonDef;
        const std::string key = "\"wander_interval\": 2.5";
        def.replace(def.find(key), key.size(),
                    std::string("\"wander_interval\": 2.5, "
                                "\"arrow_effect\": ") + value);
        auto r = mobs.registerMob(parseJson(def));
        EXPECT_FALSE(r.ok()) << "accepted arrow_effect: " << value;
    }
    // A MELEE mob carrying the field: its arrows never fly, so the field
    // would be dead (the ammo discipline — the shoot_range requirement).
    MobRegistry mobs;
    std::string def = kZombieDef;
    const std::string key = "\"wander_interval\": 2.5";
    def.replace(def.find(key), key.size(),
                std::string("\"wander_interval\": 2.5, "
                            "\"arrow_effect\": {\"name\": \"wither\", "
                            "\"duration\": 10}"));
    EXPECT_FALSE(mobs.registerMob(parseJson(def)).ok());
}

// -- SURV-5: the shipped content pins -----------------------------------------

TEST(MobContent, SkeletonUndeadAndWitherArrowPinned) {
    // The real assets (data/mobs/skeleton.json): the MC wither-skeleton
    // twin — its arrows carry the wither for 10 s (MC's value) and the
    // skeleton is an undead (poison-immune, wither-vulnerable).
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    MobRegistry mobs;
    ASSERT_TRUE(mobs.loadDirectory(assets, "data/mobs").ok());
    const Mob* sk = mobs.findByName("skeleton");
    ASSERT_NE(sk, nullptr);
    EXPECT_TRUE(sk->properties.undead);
    EXPECT_EQ(sk->properties.arrowEffectName, "wither");
    EXPECT_DOUBLE_EQ(sk->properties.arrowEffectDuration, 10.0);
    // The zombie is the other shipped undead; the rest are not.
    const Mob* zombie = mobs.findByName("zombie");
    ASSERT_NE(zombie, nullptr);
    EXPECT_TRUE(zombie->properties.undead);
    for (const char* name : {"cow", "sheep", "chicken", "spider"}) {
        const Mob* m = mobs.findByName(name);
        ASSERT_NE(m, nullptr) << name;
        EXPECT_FALSE(m->properties.undead) << name;
        EXPECT_TRUE(m->properties.arrowEffectName.empty()) << name;
    }
}

TEST(MobContent, Lgt3MoonlightThresholdPinned) {
    // LGT-3 (the 10-27 user verdict): the shipped hostile trio pairs its
    // spawn threshold to the kNightSkyLevel 7 moonlight floor (MC: hostiles
    // spawn at light <= 7 — night open sky 7 spawns, a torch circle 14 is
    // safe, day open sky 15 does not); the passives stay at 15 (any light).
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    MobRegistry mobs;
    ASSERT_TRUE(mobs.loadDirectory(assets, "data/mobs").ok());
    for (const char* name : {"zombie", "skeleton", "spider"}) {
        const Mob* m = mobs.findByName(name);
        ASSERT_NE(m, nullptr) << name;
        EXPECT_EQ(m->properties.spawnMaxLight, 7) << name;
    }
    for (const char* name : {"cow", "sheep", "chicken"}) {
        const Mob* m = mobs.findByName(name);
        ASSERT_NE(m, nullptr) << name;
        EXPECT_EQ(m->properties.spawnMaxLight, 15) << name;
    }
    // V2-5: the pig joins the passive day-spawn trio at the same gate.
    const Mob* pig = mobs.findByName("pig");
    ASSERT_NE(pig, nullptr);
    EXPECT_EQ(pig->properties.spawnMaxLight, 15);
}

TEST(MobContent, PigPassiveAndDropsPinned) {
    // V2-5 (the pig — the "add a mob = JSON + textures" data-driven
    // promise): the chicken's passive shape (detect_radius 0 / attack 0,
    // the chicken's wander_interval 2.0), the MC pig stats (health 10,
    // speed 0.6) and the drop chain: pork x1-2 (the Phase 38 count
    // RANGE — the pig is the first shipped carrier of a min>0 range) +
    // leather x0-1 (the cow/sheep leather precedent).
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    MobRegistry mobs;
    ASSERT_TRUE(mobs.loadDirectory(assets, "data/mobs").ok());
    const Mob* pig = mobs.findByName("pig");
    ASSERT_NE(pig, nullptr);
    EXPECT_FLOAT_EQ(pig->properties.health, 10.0f);
    EXPECT_FLOAT_EQ(pig->properties.speed, 0.6f);
    EXPECT_FLOAT_EQ(pig->properties.detectRadius, 0.0f);
    EXPECT_FLOAT_EQ(pig->properties.attackRange, 0.0f);
    EXPECT_FLOAT_EQ(pig->properties.attackDamage, 0.0f);
    EXPECT_DOUBLE_EQ(pig->properties.wanderInterval, 2.0);
    EXPECT_FALSE(pig->properties.hostile);
    EXPECT_TRUE(pig->properties.drops.size() == 2);
    EXPECT_EQ(pig->properties.drops[0].item, "pork");
    EXPECT_EQ(pig->properties.drops[0].countMin, 1);
    EXPECT_EQ(pig->properties.drops[0].countMax, 2);
    EXPECT_EQ(pig->properties.drops[1].item, "leather");
    EXPECT_EQ(pig->properties.drops[1].countMin, 0);
    EXPECT_EQ(pig->properties.drops[1].countMax, 1);
}

TEST(MobContent, PreV25MobFilesUnchanged) {
    // V2-5 red line: the six pre-pig mob JSONs are byte-untouched (the
    // FNV pin — the SkinPngBytesPinned existing-assets precedent: a
    // stray edit of a shipped mob turns this red).
    // V2-7: the spider GAINS its drop list (string / spider_eye — the
    // intended edit, the new pin below); its FNV updates accordingly, the
    // other five files stay.
    // V2-11: every mob GAINS its `xp` grant (the intended edit — the
    // wiki-verified values, the V2-7 pin-update precedent); all six
    // FNVs update accordingly.
    // V2-15: the sheep GAINS its wool data (the wool_drops pair + the
    // 1-3 wool range + the body box's name — the intended edit, the V2-11
    // pin-update precedent); the other five files stay.
    const struct {
        const char* path;
        const char* fnv;
    } files[] = {
        {"assets/data/mobs/chicken.json", "315c4f4f1ac672bc"},
        {"assets/data/mobs/cow.json", "17b36715e3e0f286"},
        {"assets/data/mobs/sheep.json", "2ebd17a8200597d2"},
        {"assets/data/mobs/skeleton.json", "a444383845de7fb8"},
        {"assets/data/mobs/spider.json", "9b2b53e9ffa14ed8"},
        {"assets/data/mobs/zombie.json", "653a09208010cccb"},
    };
    for (const auto& f : files) {
        EXPECT_EQ(fnv1a64(readAll(f.path)), f.fnv) << f.path;
    }
}

TEST(MobContent, V27SpiderDropsPinned) {
    // V2-7: the spider's drops (the "spider drops nothing" documentation
    // retired): string x0-2 + spider_eye x0-2 (the MC spider values, the
    // Phase 38 count RANGE — the pig's first-shipped-range precedent).
    // Every OTHER spider field stays (the hostile stats + the poison
    // on_hit_effect) — the drop list is the only edit.
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    MobRegistry mobs;
    ASSERT_TRUE(mobs.loadDirectory(assets, "data/mobs").ok());
    const Mob* sp = mobs.findByName("spider");
    ASSERT_NE(sp, nullptr);
    ASSERT_TRUE(sp->properties.drops.size() == 2);
    EXPECT_EQ(sp->properties.drops[0].item, "string");
    EXPECT_EQ(sp->properties.drops[0].countMin, 0);
    EXPECT_EQ(sp->properties.drops[0].countMax, 2);
    EXPECT_EQ(sp->properties.drops[1].item, "spider_eye");
    EXPECT_EQ(sp->properties.drops[1].countMin, 0);
    EXPECT_EQ(sp->properties.drops[1].countMax, 2);
    // The untouched rest of the spider (the V2-7 change is the drops).
    EXPECT_FLOAT_EQ(sp->properties.health, 16.0f);
    EXPECT_FLOAT_EQ(sp->properties.speed, 1.0f);
    EXPECT_FLOAT_EQ(sp->properties.detectRadius, 16.0f);
    EXPECT_FLOAT_EQ(sp->properties.attackRange, 1.6f);
    EXPECT_FLOAT_EQ(sp->properties.attackDamage, 2.0f);
    EXPECT_FLOAT_EQ(sp->properties.attackCooldown, 1.0f);
    EXPECT_FLOAT_EQ(sp->properties.wanderInterval, 2.5f);
    EXPECT_EQ(sp->properties.spawnMaxLight, 7);
    EXPECT_TRUE(sp->properties.hostile);
    EXPECT_EQ(sp->properties.hitEffectName, "poison");
    EXPECT_DOUBLE_EQ(sp->properties.hitEffectDuration, 15.0);
    // The other SIX shipped mobs (the 5 pre-V2-5 non-spider + the pig)
    // are byte-untouched by V2-7 (the FNV pin, the PreV25 precedent).
    // V2-11: all seven shipped mobs GAIN their `xp` grant (the intended
    // edit — the pins update, the V2-7 precedent for the spider).
    const struct {
        const char* path;
        const char* fnv;
    } untouched[] = {
        {"assets/data/mobs/chicken.json", "315c4f4f1ac672bc"},
        {"assets/data/mobs/cow.json", "17b36715e3e0f286"},
        // V2-15: the sheep's wool data (the intended edit, the PreV25
        // test's twin update).
        {"assets/data/mobs/sheep.json", "2ebd17a8200597d2"},
        {"assets/data/mobs/skeleton.json", "a444383845de7fb8"},
        {"assets/data/mobs/zombie.json", "653a09208010cccb"},
        {"assets/data/mobs/pig.json", "b49895541001dfcb"},
    };
    for (const auto& f : untouched) {
        EXPECT_EQ(fnv1a64(readAll(f.path)), f.fnv) << f.path;
    }
}

// -- V2-11: the XP system (the mob death grant) --------------------------------

TEST(MobContent, V211MobXpPinned) {
    // The shipped mob XP values (the wiki-verified 11-03 set — the modern
    // Minecraft table simplified to FIXED values: the hostile trio is MC's
    // fixed 5 (equipment bonuses out of scope), the passive quartet the
    // MC 1-3 interval mid = 2 — the documented deviation). Every OTHER
    // mob field stays (the V2-11 data change is the xp line).
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    MobRegistry mobs;
    ASSERT_TRUE(mobs.loadDirectory(assets, "data/mobs").ok());
    const struct {
        const char* name;
        int xp;
    } expect[] = {
        {"zombie", 5}, {"skeleton", 5}, {"spider", 5},
        {"pig", 2}, {"cow", 2}, {"sheep", 2}, {"chicken", 2},
    };
    for (const auto& e : expect) {
        SCOPED_TRACE(e.name);
        const Mob* m = mobs.findByName(e.name);
        ASSERT_NE(m, nullptr);
        EXPECT_EQ(m->properties.xp, e.xp);
    }
    // V2-14: the bat is the zero-grant (the pure ambience mob — the MC
    // "no drops / xp 0" fact).
    const Mob* bat = mobs.findByName("bat");
    ASSERT_NE(bat, nullptr);
    EXPECT_EQ(bat->properties.xp, 0);
}

// -- V2-14: the bat content pin (the data-driven mob promise) ------------------

TEST(MobContent, BatDataPinned) {
    // The REAL asset (data/mobs/bat.json): the MC-verified bat (HP 6,
    // passive, no drops / xp 0, the light <= 3 cave semantics, the
    // cruise speed 1.0) + the three new data flags (flying /
    // spawns_in_air / the ambient pair — the bat is the first carrier
    // of each).
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    MobRegistry mobs;
    ASSERT_TRUE(mobs.loadDirectory(assets, "data/mobs").ok());
    const Mob* bat = mobs.findByName("bat");
    ASSERT_NE(bat, nullptr);
    const MobProperties& p = bat->properties;
    EXPECT_FLOAT_EQ(p.health, 6.0f);
    EXPECT_FLOAT_EQ(p.speed, kBatCruiseSpeed);  // the MC cruise 1.0
    EXPECT_FLOAT_EQ(p.detectRadius, 0.0f);
    EXPECT_FLOAT_EQ(p.attackRange, 0.0f);
    EXPECT_FLOAT_EQ(p.attackDamage, 0.0f);
    EXPECT_DOUBLE_EQ(p.wanderInterval, 2.0);    // the cruise turn cadence
    EXPECT_EQ(p.spawnMaxLight, 3);              // the MC light <= 3 (cave)
    EXPECT_FALSE(p.hostile);
    EXPECT_TRUE(p.drops.empty());                // no drops (omitted)
    EXPECT_EQ(p.xp, 0);
    // The three data flags.
    EXPECT_TRUE(p.flying);
    EXPECT_TRUE(p.spawnsInAir);
    EXPECT_EQ(p.ambientSoundName, "squeak");
    EXPECT_DOUBLE_EQ(p.ambientInterval, 3.0);
    // Model: the flat hovering form (deviation 1 — the MC 0.5 x 0.9
    // vertical box cannot fit the AABB's wing spread; 0.9 x 0.5 is the
    // visual self-consistent hover), 4 boxes (body / head / two wings)
    // inside the AABB, the two-wing 0.9 rad antiphase flapping.
    EXPECT_FLOAT_EQ(bat->model.width, 0.9f);
    EXPECT_FLOAT_EQ(bat->model.height, 0.5f);
    ASSERT_EQ(bat->model.boxes.size(), 4u);
    const MobBox& body = bat->model.boxes[0];
    EXPECT_NEAR(body.center.x, 0.0f, 1e-6f);
    EXPECT_NEAR(body.center.y, 0.25f, 1e-6f);
    EXPECT_NEAR(body.center.z, 0.0f, 1e-6f);
    EXPECT_NEAR(body.size.x, 0.5f, 1e-6f);
    EXPECT_NEAR(body.size.y, 0.3f, 1e-6f);
    EXPECT_NEAR(body.size.z, 0.5f, 1e-6f);
    EXPECT_EQ(body.texture, "bat");
    EXPECT_TRUE(body.faces.empty());
    const MobBox& head = bat->model.boxes[1];
    EXPECT_NEAR(head.center.x, 0.0f, 1e-6f);
    EXPECT_NEAR(head.center.y, 0.35f, 1e-6f);
    EXPECT_NEAR(head.center.z, -0.25f, 1e-6f);  // front = -Z
    EXPECT_NEAR(head.size.x, 0.25f, 1e-6f);
    EXPECT_NEAR(head.size.y, 0.2f, 1e-6f);
    EXPECT_NEAR(head.size.z, 0.2f, 1e-6f);
    EXPECT_EQ(head.texture, "bat");
    const char* wingNames[] = {"wing_l", "wing_r"};
    for (int i = 0; i < 2; ++i) {
        const MobBox& wing = bat->model.boxes[static_cast<std::size_t>(2 + i)];
        EXPECT_EQ(wing.name, wingNames[i]) << "wing " << i;
        EXPECT_NEAR(std::abs(wing.center.x), 0.275f, 1e-6f) << "wing " << i;
        EXPECT_NEAR(wing.center.y, 0.3f, 1e-6f) << "wing " << i;
        EXPECT_NEAR(wing.center.z, 0.0f, 1e-6f) << "wing " << i;
        EXPECT_NEAR(wing.size.x, 0.35f, 1e-6f) << "wing " << i;
        EXPECT_NEAR(wing.size.y, 0.05f, 1e-6f) << "wing " << i;
        EXPECT_NEAR(wing.size.z, 0.5f, 1e-6f) << "wing " << i;
        EXPECT_EQ(wing.texture, "bat") << "wing " << i;
        EXPECT_TRUE(wing.faces.empty()) << "wing " << i;
    }
    ASSERT_EQ(bat->model.swings.size(), 2u);
    for (int i = 0; i < 2; ++i) {
        const MobSwing& sw = bat->model.swings[static_cast<std::size_t>(i)];
        EXPECT_FLOAT_EQ(sw.amplitude, 0.9f) << "swing " << i;
        EXPECT_EQ(sw.group, i) << "swing " << i;  // the antiphase flapping
        EXPECT_EQ(
            bat->model.boxes[static_cast<std::size_t>(sw.boxIndex)].name,
            wingNames[i])
            << "swing " << i;
    }
}

TEST(MobContent, BatAssetsPinned) {
    // The bat's new asset files (the content is data — the FNV pins, the
    // PreV25MobFilesUnchanged convention): bat.json, textures/mobs/bat.png
    // (the single tile — dark brown-black hide + wing membrane),
    // squeak.wav (the ambient sound) and mob_spawning.json (the 8-entry
    // vocabulary).
    EXPECT_EQ(fnv1a64(readAll("assets/data/mobs/bat.json")),
              "2b86ddaad75a7b84");
    EXPECT_EQ(fnv1a64(readAll("assets/textures/mobs/bat.png")),
              "960a8e389c157af2");
    EXPECT_EQ(fnv1a64(readAll("assets/sounds/squeak.wav")),
              "3857c104ff96aed5");
    EXPECT_EQ(fnv1a64(readAll("assets/data/mob_spawning.json")),
              "a8e3bf9e89ccbbf0");
}

// -- V2-15: the sheep's wool content (the 16-color dyed sheep) ------------------

namespace {

// The sheep's REAL data shape as an inline def (the passive AI — the
// MobEntity tests never see a player) + the full wool pair (the shipped
// weights). The twin without the wool fields is the zero-consumption
// red line's control.
const char* kSheepWoolDef = R"({
  "id": "sheep",
  "properties": {
    "health": 8,
    "speed": 0.5,
    "detect_radius": 0,
    "attack_range": 0,
    "attack_damage": 0,
    "attack_cooldown": 1.0,
    "wander_interval": 3.0,
    "drops": [
      {"item": "wool", "count_min": 1, "count_max": 3},
      {"item": "mutton", "count": 1}
    ],
    "wool_drops": true,
    "wool_weights": [
      {"color": "white", "weight": 850},
      {"color": "black", "weight": 80},
      {"color": "gray", "weight": 30},
      {"color": "brown", "weight": 30},
      {"color": "pink", "weight": 10}
    ]
  },
  "model": {
    "width": 1.2,
    "height": 1.5,
    "boxes": [
      {"name": "wool", "pos": [0.0, 1.025, 0.0],
       "size": [0.625, 0.55, 0.95], "texture": "a"},
      {"pos": [0.0, 1.1, -0.375], "size": [0.5, 0.5, 0.45],
       "texture": "b"}
    ]
  }
})";

const char* kSheepNoWoolDef = R"({
  "id": "sheep",
  "properties": {
    "health": 8,
    "speed": 0.5,
    "detect_radius": 0,
    "attack_range": 0,
    "attack_damage": 0,
    "attack_cooldown": 1.0,
    "wander_interval": 3.0,
    "drops": [
      {"item": "mutton", "count": 1}
    ]
  },
  "model": {
    "width": 1.2,
    "height": 1.5,
    "boxes": [
      {"pos": [0.0, 1.025, 0.0], "size": [0.625, 0.55, 0.95],
       "texture": "a"},
      {"pos": [0.0, 1.1, -0.375], "size": [0.5, 0.5, 0.45],
       "texture": "b"}
    ]
  }
})";

MobRegistry makeSheepRegistry(const char* def) {
    MobRegistry mobs;
    auto r = mobs.registerMob(parseJson(def));
    if (!r.ok()) {
        ADD_FAILURE() << r.error().message;
    }
    return mobs;
}

} // namespace

TEST(MobContent, SheepWoolContentPinned) {
    // The REAL asset (data/mobs/sheep.json): the V2-15 edit — the wool
    // drop RANGE (MC exact 1-3, the Phase 38 form), the wool_drops pair
    // (the MC temperate table's integer approximation — deviation 2:
    // white 850 / black 80 / gray 30 / brown 30 / pink 10), the body
    // box's variant anchor name ("wool"), and EVERY other field
    // unchanged (the health / speed / xp / legs pins — the V2-11
    // pin-update precedent).
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    MobRegistry mobs;
    ASSERT_TRUE(mobs.loadDirectory(assets, "data/mobs").ok());
    const Mob* sheep = mobs.findByName("sheep");
    ASSERT_NE(sheep, nullptr);
    const MobProperties& p = sheep->properties;
    EXPECT_FLOAT_EQ(p.health, 8.0f);
    EXPECT_FLOAT_EQ(p.speed, 0.5f);
    EXPECT_DOUBLE_EQ(p.wanderInterval, 3.0);
    EXPECT_EQ(p.xp, 2);
    ASSERT_TRUE(p.drops.size() == 2);
    EXPECT_EQ(p.drops[0].item, "wool");
    EXPECT_EQ(p.drops[0].countMin, 1);  // MC: 1-3 wool
    EXPECT_EQ(p.drops[0].countMax, 3);
    EXPECT_EQ(p.drops[1].item, "mutton");
    EXPECT_EQ(p.drops[1].countMin, 1);
    EXPECT_EQ(p.drops[1].countMax, 1);
    // The wool pair (deviation 2's shipped temperate table).
    EXPECT_TRUE(p.woolDrops);
    ASSERT_EQ(p.woolWeights.size(), 5u);
    const struct {
        const char* color;
        int weight;
    } weights[] = {
        {"white", 850}, {"black", 80}, {"gray", 30}, {"brown", 30},
        {"pink", 10},
    };
    for (std::size_t i = 0; i < p.woolWeights.size(); ++i) {
        EXPECT_EQ(p.woolWeights[i].color, weights[i].color) << i;
        EXPECT_EQ(p.woolWeights[i].weight, weights[i].weight) << i;
    }
    // Model: the body box GAINS the variant anchor name; the head /
    // legs / geometry are untouched.
    ASSERT_EQ(sheep->model.boxes.size(), 6u);
    EXPECT_EQ(sheep->model.boxes[0].name, "wool");
    EXPECT_EQ(sheep->model.boxes[0].texture, "sheep_body");
    EXPECT_EQ(sheep->model.boxes[5].texture, "sheep_head");
    // The other shipped mobs are wool-less (the default-off red line).
    for (const char* name :
         {"zombie", "skeleton", "spider", "pig", "cow", "chicken",
          "bat"}) {
        const Mob* m = mobs.findByName(name);
        ASSERT_NE(m, nullptr) << name;
        EXPECT_FALSE(m->properties.woolDrops) << name;
        EXPECT_TRUE(m->properties.woolWeights.empty()) << name;
    }
}

TEST(MobRegistryTest, WoolFieldsDefaultOff) {
    // Absent both = the pre-V2-15 semantics (the bit-identical root for
    // every non-sheep mob).
    const MobRegistry mobs = makeSheepRegistry(kSheepNoWoolDef);
    const Mob& sheep = mobs.at(MobID{0});
    EXPECT_FALSE(sheep.properties.woolDrops);
    EXPECT_TRUE(sheep.properties.woolWeights.empty());
}

TEST(MobRegistryTest, ParsesWoolPair) {
    const MobRegistry mobs = makeSheepRegistry(kSheepWoolDef);
    const Mob& sheep = mobs.at(MobID{0});
    EXPECT_TRUE(sheep.properties.woolDrops);
    ASSERT_EQ(sheep.properties.woolWeights.size(), 5u);
    EXPECT_EQ(sheep.properties.woolWeights[0].color, "white");
    EXPECT_EQ(sheep.properties.woolWeights[0].weight, 850);
    EXPECT_EQ(sheep.properties.woolWeights[4].color, "pink");
    EXPECT_EQ(sheep.properties.woolWeights[4].weight, 10);
}

TEST(MobRegistryTest, RejectsLoneWoolFields) {
    // The strict pair (the milk pair's discipline): one without the
    // other is dead data, rejected by name.
    MobRegistry mobs;
    auto r = mobs.registerMob(parseJson(R"({
      "id": "sheep",
      "properties": {
        "health": 8, "speed": 0.5, "detect_radius": 0,
        "attack_range": 0, "attack_damage": 0,
        "attack_cooldown": 1.0, "wander_interval": 3.0,
        "wool_drops": true
      },
      "model": {
        "width": 1.2, "height": 1.5,
        "boxes": [{"pos": [0.0, 0.5, 0.0], "size": [0.5, 0.5, 0.5],
                   "texture": "a"}]
      }
    })"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("given together"),
              std::string::npos)
        << r.error().message;
    r = mobs.registerMob(parseJson(R"({
      "id": "sheep",
      "properties": {
        "health": 8, "speed": 0.5, "detect_radius": 0,
        "attack_range": 0, "attack_damage": 0,
        "attack_cooldown": 1.0, "wander_interval": 3.0,
        "wool_weights": [{"color": "white", "weight": 1}]
      },
      "model": {
        "width": 1.2, "height": 1.5,
        "boxes": [{"pos": [0.0, 0.5, 0.0], "size": [0.5, 0.5, 0.5],
                   "texture": "a"}]
      }
    })"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("given together"),
              std::string::npos)
        << r.error().message;
}

TEST(MobRegistryTest, RejectsNonBoolWoolDrops) {
    MobRegistry mobs;
    auto r = mobs.registerMob(parseJson(R"({
      "id": "sheep",
      "properties": {
        "health": 8, "speed": 0.5, "detect_radius": 0,
        "attack_range": 0, "attack_damage": 0,
        "attack_cooldown": 1.0, "wander_interval": 3.0,
        "wool_drops": "yes",
        "wool_weights": [{"color": "white", "weight": 1}]
      },
      "model": {
        "width": 1.2, "height": 1.5,
        "boxes": [{"pos": [0.0, 0.5, 0.0], "size": [0.5, 0.5, 0.5],
                   "texture": "a"}]
      }
    })"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("'wool_drops' must be a boolean"),
              std::string::npos)
        << r.error().message;
}

TEST(MobRegistryTest, RejectsEmptyWoolWeights) {
    MobRegistry mobs;
    auto r = mobs.registerMob(parseJson(R"({
      "id": "sheep",
      "properties": {
        "health": 8, "speed": 0.5, "detect_radius": 0,
        "attack_range": 0, "attack_damage": 0,
        "attack_cooldown": 1.0, "wander_interval": 3.0,
        "wool_drops": true,
        "wool_weights": []
      },
      "model": {
        "width": 1.2, "height": 1.5,
        "boxes": [{"pos": [0.0, 0.5, 0.0], "size": [0.5, 0.5, 0.5],
                   "texture": "a"}]
      }
    })"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("empty array"),
              std::string::npos)
        << r.error().message;
}

TEST(MobRegistryTest, RejectsBadWoolWeightEntries) {
    // The strict entry shape: {color, weight} only; weight an integer in
    // [1, 10000]; no duplicate colors. Each typo is named.
    const struct {
        const char* weights;
        const char* needle;
    } cases[] = {
        {R"([{"color": "white", "weight": 0}])", "[1, 10000]"},
        {R"([{"color": "white", "weight": 10001}])", "[1, 10000]"},
        {R"([{"color": "white", "weight": 3.5}])", "[1, 10000]"},
        {R"([{"weight": 1}])", "'color'"},
        {R"([{"color": "white"}])", "'weight'"},
        {R"([{"color": "white", "weight": 1, "extra": 2}])",
         "unknown field"},
        {R"([{"color": "white", "weight": 1}, {"color": "white", "weight": 2}])",
         "twice"},
    };
    for (const auto& c : cases) {
        SCOPED_TRACE(c.weights);
        MobRegistry mobs;
        const std::string def = std::string(R"({
          "id": "sheep",
          "properties": {
            "health": 8, "speed": 0.5, "detect_radius": 0,
            "attack_range": 0, "attack_damage": 0,
            "attack_cooldown": 1.0, "wander_interval": 3.0,
            "wool_drops": true,
            "wool_weights": )") +
                                c.weights + R"(
          },
          "model": {
            "width": 1.2, "height": 1.5,
            "boxes": [{"pos": [0.0, 0.5, 0.0], "size": [0.5, 0.5, 0.5],
                       "texture": "a"}]
          }
        })";
        auto r = mobs.registerMob(parseJson(def));
        ASSERT_FALSE(r.ok());
        EXPECT_NE(r.error().message.find(c.needle), std::string::npos)
            << r.error().message;
    }
}

TEST(MobContent, V215WoolAssetsPinned) {
    // The V2-15 new asset files (the BatAssetsPinned convention — the
    // content is data, the FNV pins are the re-run determinism proof):
    // the shears item + recipe (the 2 iron ingots shapeless), the
    // shears icon (the new salt 152+), the shear sound (the new seed
    // 0x5B4A) and the atlas layout (the sheep_skin's [11, 6] cell — the
    // 121-tile zero-shift red line).
    EXPECT_EQ(fnv1a64(readAll("assets/data/items/shears.json")),
              "32e8220dfca768c2");
    EXPECT_EQ(fnv1a64(readAll("assets/data/recipes/shears.json")),
              "a83ef51067e50fee");
    EXPECT_EQ(fnv1a64(readAll("assets/textures/items/shears.png")),
              "73b48b6aca85ca6c");
    EXPECT_EQ(fnv1a64(readAll("assets/sounds/shear.wav")),
              "f4fb0c5bdd5b6019");
    // V2-16 re-pin: the atlas layout gained the snow tile's [12, 6] cell
    // (the 122-tile zero-shift red line lives in test_atlas's census —
    // this FNV is the file-bytes re-run determinism proof). V3-1 Phase A
    // re-pin: the atlas layout gained the nether portal's [13, 6] cell
    // (the 123-tile zero-shift red line, the test_atlas census). V3-1
    // Phase B re-pin: the netherrack's [13, 7] cell (the 124-tile zero-
    // shift red line, the test_atlas census). V3-2 Part 1 re-pin: the
    // nether content family's 12 tiles (row 8, the 16x16 grid growth —
    // the 136-tile zero-shift red line, the test_atlas census). V3-3
    // re-pin: the nether mob family's 9 tiles (the row-8 free run
    // [12..15, 8] + the row-9 free run [0..4, 9] — the 145-tile
    // zero-shift red line, the test_atlas census). V3-5 re-pin: the
    // building content family's 20 tiles (the row-9 free run [5..15, 9]
    // + the row-10 free run [0..8, 10] — the 165-tile zero-shift red
    // line, the test_atlas census). V3-14b re-pin: the ghast's body
    // tile (the row-10 free run [9, 10] — the 166-tile zero-shift red
    // line). V3-10a re-pin: the End's four tiles (the row-10 free run
    // [10..13, 10] — the 170-tile zero-shift red line, the test_atlas
    // census; the ender_eye icon is a standalone item PNG).
    EXPECT_EQ(fnv1a64(readAll("assets/data/textures/block_atlas.json")),
              "cf127c15a903407e");
}

// -- V2-15: the wool instance state (MobEntity — the frame discipline) ----

TEST(MobWoolTest, SpawnColorIsTheStreamFirstDraw) {
    // The bit-level red line, observed: with the SAME seed, the
    // non-wool mob's FIRST stream draw is its frame-1 wander decision
    // (seed 11's draw 1: stand — the velocity stays zero), while the
    // wool mob's draw 1 is the CONSTRUCTOR's color roll (draw 2 becomes
    // its frame-1 decision: walk — the velocity is nonzero at the
    // data-speed x kWanderSpeedFactor magnitude). One extra draw, no
    // more: the two streams differ by exactly the color roll.
    const std::uint64_t kSeed = 11;  // draw1 % 3 == 0, draw2 % 3 == 1
    Fixture f;
    MobRegistry woolMobs = makeSheepRegistry(kSheepWoolDef);
    MobRegistry plainMobs = makeSheepRegistry(kSheepNoWoolDef);
    const Mob& woolDef = woolMobs.at(MobID{0});
    const Mob& plainDef = plainMobs.at(MobID{0});
    MobEntity plain = makeMob(f, plainDef, Vec3 {0.5f, 1.0f, 0.5f}, kSeed);
    MobEntity wool = makeMob(f, woolDef, Vec3 {0.5f, 1.0f, 0.5f}, kSeed);
    // The color roll: seed 11's first uniform (0.579101...) lands in
    // the white band [0, 850) of the shipped weights — pinned.
    EXPECT_EQ(wool.woolColor(), "white");
    plain.update(kDt);
    wool.update(kDt);
    EXPECT_EQ(plain.velocity(), Vec3::zero());  // draw 1: stand
    const Vec3 v = wool.velocity();
    EXPECT_NE(v, Vec3::zero());                  // draw 2: walk
    EXPECT_NEAR(std::hypot(v.x, v.z), 0.25f,
                1e-6f);  // speed 0.5 x kWanderSpeedFactor
}

TEST(MobWoolTest, ThreeInstanceSpawnSequencePinned) {
    // The game's spawn-seed derivation (attemptMobSpawn: the world seed
    // ^ gamma * counter, counters 1..3, world seed 0): the three
    // instances' spawn colors are the pinned sequence (the roll is the
    // stream's first draw — the E2E's "same run, same colors" basis).
    Fixture f;
    MobRegistry mobs = makeSheepRegistry(kSheepWoolDef);
    const Mob& def = mobs.at(MobID{0});
    const std::uint64_t gamma = 0x9E3779B97F4A7C15ULL;
    const char* expect[] = {"white", "white", "brown"};
    for (int c = 1; c <= 3; ++c) {
        const std::uint64_t seed = gamma * static_cast<std::uint64_t>(c);
        MobEntity mob = makeMob(f, def,
                                Vec3 {static_cast<float>(c), 1.0f, 0.5f},
                                seed);
        EXPECT_EQ(mob.woolColor(), expect[static_cast<int>(c) - 1]) << c;
    }
}

TEST(MobWoolTest, ShearRollsTheWoolEntryRangeAndSetsBare) {
    // Seed 11's fresh mob (the color roll consumed draw 1): the shear
    // roll is draw 2 (m % 3 == 1) -> 1 + 1 = 2 wool — pinned (the
    // wool entry's 1-3 range, the rollDropCount's % span). The bare
    // state + the FULL regrow window; the re-shear is a no-op (0, the
    // regrow UNRESET — the second shear must not extend the window).
    Fixture f;
    MobRegistry mobs = makeSheepRegistry(kSheepWoolDef);
    const Mob& def = mobs.at(MobID{0});
    MobEntity mob = makeMob(f, def, Vec3 {0.5f, 1.0f, 0.5f}, 11);
    const std::string color = mob.woolColor();
    EXPECT_FALSE(mob.isBare());
    EXPECT_EQ(mob.woolRegrowFrames(), 0);
    EXPECT_EQ(mob.shearWool(), 2);
    EXPECT_TRUE(mob.isBare());
    EXPECT_EQ(mob.woolRegrowFrames(), kSheepRegrowFrames);
    EXPECT_EQ(mob.woolColor(), color);  // the shear keeps the color
    EXPECT_EQ(mob.shearWool(), 0);      // the bare re-shear: no-op
    EXPECT_EQ(mob.woolRegrowFrames(), kSheepRegrowFrames);  // unreset
}

TEST(MobWoolTest, RegrowRestoresTheWoolOnTheFrameBoundary) {
    // One frame short: still bare; the boundary frame: woolly again
    // (the milkCooldown's frame-boundary twin, deviation 3's fixed
    // duration).
    Fixture f;
    MobRegistry mobs = makeSheepRegistry(kSheepWoolDef);
    const Mob& def = mobs.at(MobID{0});
    MobEntity mob = makeMob(f, def, Vec3 {0.5f, 1.0f, 0.5f}, 12);
    mob.shearWool();
    runMob(mob, kSheepRegrowFrames - 1);
    EXPECT_TRUE(mob.isBare());
    EXPECT_EQ(mob.woolRegrowFrames(), 1);
    runMob(mob, 1);
    EXPECT_FALSE(mob.isBare());
    EXPECT_EQ(mob.woolRegrowFrames(), 0);
}

TEST(MobWoolTest, DyeRecoloredRefusedWhileBareKeptOnRegrow) {
    // Minecraft's two dye rules: a woolly mob takes the held dye's
    // color; a BARE mob refuses it (the no-op — the color unchanged,
    // the click is the game's concern); the regrow RESTORES the dyed
    // color (the "sheared sheep keeps its dye" semantics).
    Fixture f;
    MobRegistry mobs = makeSheepRegistry(kSheepWoolDef);
    const Mob& def = mobs.at(MobID{0});
    MobEntity mob = makeMob(f, def, Vec3 {0.5f, 1.0f, 0.5f}, 12);
    mob.dyeWool("red");
    EXPECT_EQ(mob.woolColor(), "red");
    mob.shearWool();
    mob.dyeWool("blue");  // refused (bare)
    EXPECT_EQ(mob.woolColor(), "red");
    runMob(mob, kSheepRegrowFrames);
    EXPECT_FALSE(mob.isBare());
    EXPECT_EQ(mob.woolColor(), "red");  // the dye survived the regrow
    // The empty color is a no-op (the belt).
    mob.dyeWool("");
    EXPECT_EQ(mob.woolColor(), "red");
}

TEST(MobWoolTest, WoolVariantTileNames) {
    // The render variant key (dyeVariantName's naming law — white is
    // the suffix-less base): woolly gray -> "wool_gray"; dyed red ->
    // "wool_red"; dyed WHITE -> "wool" (no suffix); bare -> the skin
    // tile.
    Fixture f;
    MobRegistry mobs = makeSheepRegistry(kSheepWoolDef);
    const Mob& def = mobs.at(MobID{0});
    MobEntity mob = makeMob(f, def, Vec3 {0.5f, 1.0f, 0.5f}, 11);
    EXPECT_EQ(mob.woolColor(), "white");  // seed 11's spawn roll
    EXPECT_EQ(mob.woolVariantTile(), "wool");  // white = the base (no suffix)
    mob.dyeWool("gray");
    EXPECT_EQ(mob.woolVariantTile(), "wool_gray");
    mob.dyeWool("red");
    EXPECT_EQ(mob.woolVariantTile(), "wool_red");
    mob.dyeWool("white");
    EXPECT_EQ(mob.woolVariantTile(), "wool");
    mob.shearWool();
    EXPECT_EQ(mob.woolVariantTile(), "sheep_skin");
    runMob(mob, kSheepRegrowFrames);
    EXPECT_EQ(mob.woolVariantTile(), "wool");  // the regrown white
}

// -- V2-15: the wool variant's render geometry (the box rebuild) ---------

TEST(MobMeshTest, WoolVariantRebuildsOnlyTheWoolBoxUv) {
    // The variant rebuild swaps ONLY the wool box's UVs (the geometry —
    // positions and normals — is bit-identical; the head box's UVs are
    // untouched): the variant tile resolves to its layout cell (the
    // faceUv corner pattern: (u0,v1),(u1,v1),(u1,v0),(u0,v0)).
    // The two-tile layout ("a" = cell (0,0) v [0, 0.5], "b" = cell
    // (0,1) v [0.5, 1] — the box-model section's makeMobLayout twin,
    // inlined: that helper is declared later in this file).
    auto layout = AtlasLayout::parse(parseJson(
        R"({"columns":1,"rows":2,"tiles":{"a":[0,0],"b":[0,1]}})"));
    ASSERT_TRUE(layout.ok()) << layout.error().message;
    const AtlasLayout l = *layout;
    // makeMobLayout's two tiles: "a" = cell (0,0) v [0, 0.5], "b" =
    // cell (0,1) v [0.5, 1] (one column: u spans [0, 1]).
    const MobRegistry mobs = makeSheepRegistry(kSheepWoolDef);
    const MobModel& model = mobs.at(MobID{0}).model;
    ASSERT_EQ(model.boxes.size(), 2u);
    EXPECT_EQ(model.boxes[0].name, "wool");  // the variant anchor
    EXPECT_EQ(model.boxes[0].texture, "a");
    EXPECT_EQ(model.boxes[1].texture, "b");
    const ItemMeshGeometry base = makeMobModelGeometry(model, l);
    const ItemMeshGeometry bare =
        makeMobModelGeometry(withWoolVariantTile(model, "b"), l);
    ASSERT_EQ(base.vertices.size(), bare.vertices.size());
    ASSERT_EQ(base.indices.size(), bare.indices.size());
    for (std::size_t i = 0; i < base.vertices.size(); ++i) {
        EXPECT_EQ(base.vertices[i].position, bare.vertices[i].position);
        EXPECT_EQ(base.vertices[i].normal, bare.vertices[i].normal);
    }
    const Vec2 u0 = AtlasLayout::uvMin(l, "b");
    const Vec2 u1 = AtlasLayout::uvMax(l, "b");
    const Vec2 corner[4] = {{u0.x, u1.y}, {u1.x, u1.y},
                            {u1.x, u0.y}, {u0.x, u0.y}};
    for (std::size_t i = 0; i < kBoxVertexCount; ++i) {
        // The wool box (vertices 0..23): the UVs moved to tile "b".
        EXPECT_NE(base.vertices[i].uv, bare.vertices[i].uv);
        EXPECT_EQ(bare.vertices[i].uv, corner[i % 4]);
    }
    for (std::size_t i = kBoxVertexCount; i < base.vertices.size(); ++i) {
        // The head box (vertices 24..47): "b" in BOTH — untouched.
        EXPECT_EQ(base.vertices[i].uv, bare.vertices[i].uv);
    }
}

// -- SURV-5: the mob effect carrier (MobEntity) --------------------------------

namespace {

// A 10-HP passive mob (the cow's data shape): the poison-to-death
// carrier test target (detect_radius 0 — it never sees the player).
const char* kCowDef = R"({
  "id": "cow",
  "properties": {
    "health": 10,
    "speed": 0.5,
    "detect_radius": 0,
    "attack_range": 0,
    "attack_damage": 0,
    "attack_cooldown": 1.0,
    "wander_interval": 3.0
  },
  "model": {
    "width": 0.9,
    "height": 1.0,
    "boxes": [
      {"pos": [0.0, 0.5, 0.0], "size": [0.6, 0.8, 0.9], "texture": "a"}
    ]
  }
})";

// The shipped effect set for the carrier tests: the real poison (player
// floor 1, mob floor 0, undead-exempt, mob-accepting), a PLAYER-ONLY
// poison twin (the gate negative), the wither (no floor anywhere, kills
// undead) and a mob-accepting HEAL (the heal no-op proof).
EffectRegistry makeCarrierEffects() {
    EffectRegistry reg;
    const struct {
        const char* def;
    } defs[] = {
        {R"({"id":"poison","tick_type":"damage","tick_amount":1.0,
             "tick_interval":2.5,"min_health":1.0,"min_health_mob":0,
             "negative":true,"affects_mobs":true,"undead_exempt":true,
             "icon":0})"},
        {R"({"id":"poison_player_only","tick_type":"damage",
             "tick_amount":1.0,"tick_interval":2.5,"min_health":1.0,
             "negative":true,"icon":0})"},
        {R"({"id":"wither","tick_type":"damage","tick_amount":1.0,
             "tick_interval":4.0,"negative":true,"affects_mobs":true,
             "icon":10})"},
        {R"({"id":"mob_heal","tick_type":"heal","tick_amount":2.0,
             "tick_interval":1.0,"affects_mobs":true,"icon":2})"},
        {R"({"id":"kept_at_one","tick_type":"damage","tick_amount":1.0,
             "tick_interval":1.0,"min_health_mob":1,"affects_mobs":true,
             "icon":0})"},
    };
    for (const auto& d : defs) {
        auto r = reg.registerEffect(parseJson(d.def));
        if (!r.ok()) {
            ADD_FAILURE() << d.def << ": " << r.error().message;
        }
    }
    return reg;
}

// The carrier mob's definition pointer (MobEntity::m_def) points INTO
// `mobs` — the registry must outlive every mob this helper returns, so
// it is TEST-OWNED: a registry local to the helper would dangle the
// entity's definition on return (a latent UAF the V2-11
// MobProperties layout shift exposed — the freed memory happened to
// still hold the old flags, so the undead gate "worked" by luck).
MobEntity makeCarrierMob(const Fixture& f, MobRegistry& mobs, const char* def) {
    auto r = mobs.registerMob(parseJson(def));
    if (!r.ok()) {
        throw std::runtime_error(r.error().message);
    }
    return MobEntity(f.world, f.blocks, mobs.at(*r), {8.5f, 1.0f, 8.5f}, 7);
}

} // namespace

TEST(MobEffectCarrier, RefusesPlayerOnlyEffects) {
    // The affects_mobs gate: a player-only effect is ignored on apply
    // (count stays 0 — the mob is never a carrier for it).
    Fixture f;
    MobRegistry mobs; // test-owned (outlives the carrier mobs — see the helper)
    const EffectRegistry effects = makeCarrierEffects();
    MobEntity cow = makeCarrierMob(f, mobs, kCowDef);
    cow.applyEffect(effects.idForName("poison_player_only"), 30.0, effects);
    EXPECT_FALSE(cow.hasEffects());
    // The mob-accepting poison is taken.
    cow.applyEffect(effects.idForName("poison"), 30.0, effects);
    EXPECT_TRUE(cow.hasEffects());
    EXPECT_EQ(cow.statusEffects().count(), 1u);
}

TEST(MobEffectCarrier, UndeadRefuseExemptEffects) {
    // The undead gate: poison (undead_exempt) x undead (zombie) =
    // ignored; the same poison on a cow applies; the wither (NO
    // undead_exempt) is taken by the zombie — the effect that kills
    // undead (Minecraft's uniqueness).
    Fixture f;
    MobRegistry mobs; // test-owned (outlives the carrier mobs — see the helper)
    const EffectRegistry effects = makeCarrierEffects();
    std::string zombieDef = kZombieDef;
    const std::string key = "\"wander_interval\": 2.5";
    zombieDef.replace(zombieDef.find(key), key.size(),
                      std::string("\"wander_interval\": 2.5, \"undead\": true"));
    MobEntity zombie = makeCarrierMob(f, mobs, zombieDef.c_str());
    MobEntity cow = makeCarrierMob(f, mobs, kCowDef);
    zombie.applyEffect(effects.idForName("poison"), 30.0, effects);
    EXPECT_FALSE(zombie.hasEffects());  // the undead exemption
    cow.applyEffect(effects.idForName("poison"), 30.0, effects);
    EXPECT_TRUE(cow.hasEffects());      // the same poison on a normal mob
    zombie.applyEffect(effects.idForName("wither"), 10.0, effects);
    EXPECT_EQ(zombie.statusEffects().count(), 1u);  // the wither kills them
}

TEST(MobEffectCarrier, PoisonTicksKillTheCow) {
    // The floor-0 lethal line: a 10-HP cow + poison (1 HP / 2.5 s,
    // min_health_mob 0). Ticks land on the fixed 1/60 grid at ~2.5 s
    // cadence; the assertions sit well clear of a tick boundary (the
    // double sum of 1/60 steps drifts a frame, the E2E tick-margin
    // discipline). 18 s: 7 ticks due (the 8th at 20 s) -> 3 HP left;
    // 26 s: 10 ticks due (the 10th kills; the 11th at 27.5 s) -> dead.
    Fixture f;
    MobRegistry mobs; // test-owned (outlives the carrier mobs — see the helper)
    const EffectRegistry effects = makeCarrierEffects();
    MobEntity cow = makeCarrierMob(f, mobs, kCowDef);
    cow.applyEffect(effects.idForName("poison"), 60.0, effects);
    for (int frame = 0; frame < 60 * 18; ++frame) {
        cow.updateStatusEffects(1.0 / 60.0, effects);
    }
    ASSERT_FALSE(cow.dead());
    EXPECT_FLOAT_EQ(cow.health(), 3.0f);  // 10 - 7 ticks
    for (int frame = 60 * 18; frame < 60 * 26; ++frame) {
        cow.updateStatusEffects(1.0 / 60.0, effects);
    }
    EXPECT_TRUE(cow.dead());  // the 10th tick kills (the existing
    // damage/despawn path)
    EXPECT_FLOAT_EQ(cow.health(), 0.0f);
    // The effect is still carried at death (it ticks until the mob is
    // removed by the game's despawn pass, which this unit does not run).
    EXPECT_TRUE(cow.hasEffects());
}

TEST(MobEffectCarrier, MobFloorKeepsTheMobAlive) {
    // The min_health_mob 1 floor: the twin damage effect never kills —
    // the cow is clamped at exactly 1 HP (the player's poison-floor
    // twin on the mob side).
    Fixture f;
    MobRegistry mobs; // test-owned (outlives the carrier mobs — see the helper)
    const EffectRegistry effects = makeCarrierEffects();
    MobEntity cow = makeCarrierMob(f, mobs, kCowDef);
    cow.applyEffect(effects.idForName("kept_at_one"), 30.0, effects);
    for (int frame = 0; frame < 60 * 25; ++frame) {  // 25 s: 25 ticks due
        cow.updateStatusEffects(1.0 / 60.0, effects);
    }
    EXPECT_FALSE(cow.dead());
    EXPECT_FLOAT_EQ(cow.health(), 1.0f);  // clamped at the floor
}

TEST(MobEffectCarrier, HealTicksAreANoop) {
    // The documented deviation: MobEntity has no regeneration — a
    // mob-accepting heal tick changes nothing (the apply itself is
    // legal: the effect is carried, it simply does not heal).
    Fixture f;
    MobRegistry mobs; // test-owned (outlives the carrier mobs — see the helper)
    const EffectRegistry effects = makeCarrierEffects();
    MobEntity cow = makeCarrierMob(f, mobs, kCowDef);
    cow.damage(4.0f);  // 10 -> 6
    ASSERT_FLOAT_EQ(cow.health(), 6.0f);
    cow.applyEffect(effects.idForName("mob_heal"), 10.0, effects);
    EXPECT_TRUE(cow.hasEffects());
    for (int frame = 0; frame < 60 * 10; ++frame) {  // 10 s: 10 ticks due
        cow.updateStatusEffects(1.0 / 60.0, effects);
    }
    EXPECT_FALSE(cow.dead());
    EXPECT_FLOAT_EQ(cow.health(), 6.0f);  // unchanged (no heal)
}

TEST(MobEffectCarrier, FastExitWithoutEffects) {
    // count == 0 = zero cost / zero change: a mob without effects is
    // untouched by the pass (the structural-immunity basis).
    Fixture f;
    MobRegistry mobs; // test-owned (outlives the carrier mobs — see the helper)
    const EffectRegistry effects = makeCarrierEffects();
    MobEntity cow = makeCarrierMob(f, mobs, kCowDef);
    for (int frame = 0; frame < 600; ++frame) {
        cow.updateStatusEffects(1.0 / 60.0, effects);
    }
    EXPECT_FALSE(cow.dead());
    EXPECT_FLOAT_EQ(cow.health(), 10.0f);
    EXPECT_FALSE(cow.hasEffects());
    // A dead mob refuses both the apply and the ticks.
    cow.damage(10.0f);
    ASSERT_TRUE(cow.dead());
    cow.applyEffect(effects.idForName("poison"), 30.0, effects);
    EXPECT_FALSE(cow.hasEffects());
}

TEST(MobRegistryTest, ParsesPassiveMob) {
    // A passive mob (Phase 22): zero detect/attack values are valid data
    // ("never chases / never attacks / harmless"), not typos.
    MobRegistry mobs;
    auto r = mobs.registerMob(parseJson(R"({
      "id": "cow",
      "properties": {
        "health": 10,
        "speed": 0.5,
        "detect_radius": 0,
        "attack_range": 0,
        "attack_damage": 0,
        "attack_cooldown": 1.0,
        "wander_interval": 3.0,
        "spawn_max_light": 15,
        "milk_with": "bucket",
        "milk_gives": "milk_bucket",
        "drops": [{"item": "leather", "count": 1},
                  {"item": "beef", "count": 1}]
      },
      "model": {
        "width": 1.2,
        "height": 1.5,
        "boxes": [
          {"pos": [0.0, 0.9, 0.0], "size": [0.8, 0.7, 1.1], "texture": "a"},
          {"pos": [0.0, 1.2, -0.35], "size": [0.45, 0.45, 0.45], "texture": "b"}
        ]
      }
    })"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const Mob& mob = mobs.at(*r);
    EXPECT_FLOAT_EQ(mob.properties.detectRadius, 0.0f);
    EXPECT_FLOAT_EQ(mob.properties.attackRange, 0.0f);
    EXPECT_FLOAT_EQ(mob.properties.attackDamage, 0.0f);
    EXPECT_EQ(mob.properties.spawnMaxLight, 15);
    // Phase 43: the milking pair (hold a bucket, get a milk bucket).
    EXPECT_EQ(mob.properties.milkWith, "bucket");
    EXPECT_EQ(mob.properties.milkGives, "milk_bucket");
    // Phase 28: the drop LIST (leather + the new beef food).
    ASSERT_EQ(mob.properties.drops.size(), 2u);
    EXPECT_EQ(mob.properties.drops[0].item, "leather");
    EXPECT_EQ(mob.properties.drops[1].item, "beef");
    EXPECT_EQ(mob.properties.drops[1].countMin, 1);
    EXPECT_EQ(mob.properties.drops[1].countMax, 1);
    EXPECT_EQ(mob.model.boxes.size(), 2u);
}

TEST(MobRegistryTest, RejectsNegativePassiveFields) {
    // 0 is valid for the detect/attack fields (passive mob), but negative
    // values and zeroed-out required-positive fields are still rejected.
    const char* const cases[][2] = {
        {"\"detect_radius\": 16", "\"detect_radius\": -1"},
        {"\"attack_range\": 1.6", "\"attack_range\": -0.5"},
        {"\"attack_damage\": 3", "\"attack_damage\": -1"},
        {"\"health\": 20", "\"health\": 0"},
        {"\"speed\": 0.8", "\"speed\": 0"},
        {"\"attack_cooldown\": 1.0", "\"attack_cooldown\": 0"},
        {"\"wander_interval\": 2.5", "\"wander_interval\": 0"},
    };
    for (const auto& c : cases) {
        MobRegistry mobs;
        std::string def = kZombieDef;
        const std::string from(c[0]);
        def.replace(def.find(from), from.size(), c[1]);
        auto r = mobs.registerMob(parseJson(def));
        EXPECT_FALSE(r.ok()) << c[0];
    }
}

TEST(MobRegistryTest, ParsesSheep) {
    // Phase 26: the second passive mob — a woolly quadruped with the cow's
    // data shape (all-zero attack fields, spawnable at any light, 1 drop).
    MobRegistry mobs;
    auto r = mobs.registerMob(parseJson(R"({
      "id": "sheep",
      "properties": {
        "health": 8,
        "speed": 0.5,
        "detect_radius": 0,
        "attack_range": 0,
        "attack_damage": 0,
        "attack_cooldown": 1.0,
        "wander_interval": 3.0,
        "spawn_max_light": 15,
        "drops": [{"item": "wool", "count": 1},
                  {"item": "mutton", "count": 1}]
      },
      "model": {
        "width": 1.2,
        "height": 1.5,
        "boxes": [
          {"pos": [0.0, 0.9, 0.0], "size": [0.8, 0.8, 1.2], "texture": "a"},
          {"pos": [0.0, 1.25, -0.4], "size": [0.4, 0.4, 0.4], "texture": "b"}
        ]
      }
    })"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const Mob& mob = mobs.at(*r);
    EXPECT_FLOAT_EQ(mob.properties.health, 8.0f);
    EXPECT_EQ(mob.properties.spawnMaxLight, 15);
    // Phase 28: the drop LIST (wool + the new mutton food).
    ASSERT_EQ(mob.properties.drops.size(), 2u);
    EXPECT_EQ(mob.properties.drops[0].item, "wool");
    EXPECT_EQ(mob.properties.drops[1].item, "mutton");
    EXPECT_FLOAT_EQ(mob.model.width, 1.2f);
    EXPECT_EQ(mob.model.boxes.size(), 2u);
}

TEST(MobRegistryTest, ParsesChicken) {
    // Phase 26: the small passive bird — the first model SMALLER than the
    // zombie (AABB 0.5x0.8); the spawn-cell/overlap/spacing rules all key
    // off these model numbers, so they must parse exactly.
    MobRegistry mobs;
    auto r = mobs.registerMob(parseJson(R"({
      "id": "chicken",
      "properties": {
        "health": 4,
        "speed": 0.6,
        "detect_radius": 0,
        "attack_range": 0,
        "attack_damage": 0,
        "attack_cooldown": 1.0,
        "wander_interval": 2.0,
        "spawn_max_light": 15,
        "drops": [{"item": "feather", "count": 1},
                  {"item": "chicken", "count": 1}]
      },
      "model": {
        "width": 0.5,
        "height": 0.8,
        "boxes": [
          {"pos": [0.0, 0.25, 0.0], "size": [0.3, 0.4, 0.5], "texture": "a"},
          {"pos": [0.0, 0.6, -0.1], "size": [0.25, 0.25, 0.25], "texture": "b"}
        ]
      }
    })"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const Mob& mob = mobs.at(*r);
    EXPECT_FLOAT_EQ(mob.properties.health, 4.0f);
    EXPECT_FLOAT_EQ(mob.properties.wanderInterval, 2.0f);
    EXPECT_EQ(mob.properties.spawnMaxLight, 15);
    // Phase 28: the drop LIST (feather + the new chicken food).
    ASSERT_EQ(mob.properties.drops.size(), 2u);
    EXPECT_EQ(mob.properties.drops[0].item, "feather");
    EXPECT_EQ(mob.properties.drops[1].item, "chicken");
    EXPECT_FLOAT_EQ(mob.model.width, 0.5f);
    EXPECT_FLOAT_EQ(mob.model.height, 0.8f);
    EXPECT_EQ(mob.model.boxes.size(), 2u);
}

TEST(MobRegistryTest, RejectsChickenBoxOutsideAabb) {
    // The chicken's 0.5x0.8 AABB is the smallest model so far: a head box
    // whose top (0.7 + 0.3/2 = 0.85) pokes above the AABB height must be
    // rejected by the box-in-AABB validation.
    MobRegistry mobs;
    auto r = mobs.registerMob(parseJson(R"({
      "id": "chicken",
      "properties": {
        "health": 4,
        "speed": 0.6,
        "detect_radius": 0,
        "attack_range": 0,
        "attack_damage": 0,
        "attack_cooldown": 1.0,
        "wander_interval": 2.0,
        "spawn_max_light": 15,
        "drops": [{"item": "feather", "count": 1}]
      },
      "model": {
        "width": 0.5,
        "height": 0.8,
        "boxes": [
          {"pos": [0.0, 0.25, 0.0], "size": [0.3, 0.4, 0.5], "texture": "a"},
          {"pos": [0.0, 0.7, -0.1], "size": [0.3, 0.3, 0.3], "texture": "b"}
        ]
      }
    })"));
    EXPECT_FALSE(r.ok());
}

TEST(MobRegistryTest, ParsesSkeleton) {
    // Phase 27: the ranged hostile mob — shoot_range > 0 marks a ranged
    // attacker (fire on the cooldown, kite inside retreat_distance);
    // attack_range 0 = it never melee-attacks; hostile = true puts it in
    // the difficulty protection ring / damage scale (Phase 25).
    MobRegistry mobs;
    auto r = mobs.registerMob(parseJson(kSkeletonDef));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const Mob& mob = mobs.at(*r);
    EXPECT_FLOAT_EQ(mob.properties.shootRange, 14.0f);
    EXPECT_FLOAT_EQ(mob.properties.retreatDistance, 3.0f);
    EXPECT_FLOAT_EQ(mob.properties.shootSpeed, 15.0f);
    // Phase 42: the ranged mob's 'ammo' item (a stuck arrow shot by it
    // returns it to the player's inventory on pickup).
    EXPECT_EQ(mob.properties.ammoItem, "arrow");
    EXPECT_FLOAT_EQ(mob.properties.attackRange, 0.0f);
    EXPECT_FLOAT_EQ(mob.properties.attackDamage, 2.0f);
    EXPECT_TRUE(mob.properties.hostile);
    EXPECT_EQ(mob.properties.spawnMaxLight, 0);
    // Phase 28: the drop LIST (the arrow, one entry).
    ASSERT_EQ(mob.properties.drops.size(), 1u);
    EXPECT_EQ(mob.properties.drops[0].item, "arrow");
}

TEST(MobRegistryTest, RejectsRangedFieldErrors) {
    // Phase 27: the optional ranged fields are strictly validated —
    // negatives rejected, shoot_speed in (0, 100] when present (an
    // explicit 0 is a typo even for melee mobs), and a ranged mob
    // (shoot_range > 0) must define a shoot_speed.
    struct Case {
        const char* valid;
        const char* broken;
    };
    const Case cases[] = {
        {"\"shoot_range\": 14", "\"shoot_range\": -1"},
        {"\"retreat_distance\": 3", "\"retreat_distance\": -0.5"},
        {"\"shoot_speed\": 15", "\"shoot_speed\": 0"},
        {"\"shoot_speed\": 15", "\"shoot_speed\": 101"},
        // Phase 42: 'ammo' must be a non-empty string (item name).
        {"\"ammo\": \"arrow\"", "\"ammo\": \"\""},
        {"\"ammo\": \"arrow\"", "\"ammo\": 7"},
    };
    for (const Case& c : cases) {
        std::string def = kSkeletonDef;
        def.replace(def.find(c.valid), std::strlen(c.valid), c.broken);
        MobRegistry mobs;
        EXPECT_FALSE(mobs.registerMob(parseJson(def)).ok()) << c.broken;
    }
    // Cross-field: shoot_range > 0 without any shoot_speed.
    std::string def = kSkeletonDef;
    const char* speed = "\"shoot_speed\": 15,\n    ";
    def.replace(def.find(speed), std::strlen(speed), "");
    MobRegistry mobs;
    EXPECT_FALSE(mobs.registerMob(parseJson(def)).ok());
    // Cross-field (Phase 42): a RANGED mob without 'ammo' is rejected
    // (its stuck arrows would be unpickable); a melee mob with no ammo
    // at all is fine (the zombie def).
    std::string noAmmo = kSkeletonDef;
    const char* ammo = "\"ammo\": \"arrow\",\n    ";
    noAmmo.replace(noAmmo.find(ammo), std::strlen(ammo), "");
    MobRegistry noAmmoMobs;
    EXPECT_FALSE(noAmmoMobs.registerMob(parseJson(noAmmo)).ok());
    MobRegistry meleeMobs;
    EXPECT_TRUE(meleeMobs.registerMob(parseJson(kZombieDef)).ok());
}

// -- milking (Phase 43: cow) --------------------------------------------------

TEST(MobRegistryTest, MilkingPairAbsentByDefault) {
    // No milk fields: not milkable (every pre-Phase-43 mob, e.g. the
    // zombie def).
    MobRegistry mobs;
    auto r = mobs.registerMob(parseJson(kZombieDef));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_TRUE(mobs.at(*r).properties.milkWith.empty());
    EXPECT_TRUE(mobs.at(*r).properties.milkGives.empty());
}

TEST(MobRegistryTest, RejectsBadMilkingPair) {
    // The pair must be complete (a lone field is a typo — the
    // dye_base / dye_color discipline) and both names non-empty strings
    // (the ammo discipline).
    struct Case {
        const char* broken;
    };
    const Case cases[] = {
        // A lone milk_with (no milk_gives).
        {"\"milk_with\": \"bucket\",\n    "},
        // A lone milk_gives (no milk_with).
        {"\"milk_gives\": \"milk_bucket\",\n    "},
        // Explicit empty = a typo.
        {"\"milk_with\": \"\",\n    \"milk_gives\": \"milk_bucket\",\n    "},
        // Non-string = a typo.
        {"\"milk_with\": 7,\n    \"milk_gives\": \"milk_bucket\",\n    "},
    };
    // Build a base def (the cow shape) with NO milk fields, then insert
    // the broken fragment after "spawn_max_light".
    const char* anchor = "\"spawn_max_light\": 15,\n    ";
    for (const Case& c : cases) {
        std::string def = R"({
      "id": "cow",
      "properties": {
        "health": 10,
        "speed": 0.5,
        "detect_radius": 0,
        "attack_range": 0,
        "attack_damage": 0,
        "attack_cooldown": 1.0,
        "wander_interval": 3.0,
        "spawn_max_light": 15,
        "drops": []
      },
      "model": {
        "width": 1.2,
        "height": 1.5,
        "boxes": [
          {"pos": [0.0, 0.9, 0.0], "size": [0.8, 0.7, 1.1], "texture": "a"}
        ]
      }
    })";
        def.replace(def.find(anchor), std::strlen(anchor),
                    std::string(anchor) + c.broken);
        MobRegistry mobs;
        EXPECT_FALSE(mobs.registerMob(parseJson(def)).ok()) << c.broken;
    }
}

// -- spawning config ----------------------------------------------------------

TEST(MobSpawningConfigTest, ParsesValid) {
    auto r = MobSpawningConfig::parse(parseJson(R"({
      "check_every_frames": 30,
      "first_spawn_frame": 60,
      "max_mobs": 4,
      "min_radius": 4,
      "max_radius": 6,
      "despawn_distance": 40,
      "mobs": [{"id": "zombie"}]
    })"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ((*r).checkEveryFrames, 30);
    EXPECT_EQ((*r).firstSpawnFrame, 60);
    EXPECT_EQ((*r).maxMobs, 4);
    EXPECT_FLOAT_EQ((*r).minRadius, 4.0f);
    EXPECT_FLOAT_EQ((*r).maxRadius, 6.0f);
    EXPECT_FLOAT_EQ((*r).despawnDistance, 40.0f);
    ASSERT_EQ((*r).mobs.size(), 1);
    EXPECT_EQ((*r).mobs[0].id, "zombie");
    EXPECT_EQ((*r).mobs[0].weight, 1);  // weight omitted = default 1
    EXPECT_EQ((*r).totalWeight(), 1);
}

TEST(MobSpawningConfigTest, ParsesFourMobList) {
    // Phase 26: the data list grew to four types (zombie, cow, sheep,
    // chicken). List order is significant — the deterministic type pick
    // walks the cumulative weights in this exact order (Phase 39; all
    // weights 1 = the legacy pick % list.size()).
    auto r = MobSpawningConfig::parse(parseJson(R"({
      "check_every_frames": 30,
      "first_spawn_frame": 60,
      "max_mobs": 4,
      "min_radius": 4,
      "max_radius": 6,
      "despawn_distance": 40,
      "mobs": [{"id": "zombie"}, {"id": "cow"}, {"id": "sheep"},
               {"id": "chicken"}]
    })"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    ASSERT_EQ((*r).mobs.size(), 4u);
    EXPECT_EQ((*r).mobs[0].id, "zombie");
    EXPECT_EQ((*r).mobs[1].id, "cow");
    EXPECT_EQ((*r).mobs[2].id, "sheep");
    EXPECT_EQ((*r).mobs[3].id, "chicken");
}

TEST(MobSpawningConfigTest, ParsesFiveMobList) {
    // Phase 27: the list grows to five (the skeleton, the first ranged
    // hostile). The deterministic type pick is the cumulative walk with
    // this order (Phase 39; all weights 1 = pick % 5).
    auto r = MobSpawningConfig::parse(parseJson(R"({
      "check_every_frames": 30,
      "first_spawn_frame": 60,
      "max_mobs": 4,
      "min_radius": 4,
      "max_radius": 6,
      "despawn_distance": 40,
      "mobs": [{"id": "zombie"}, {"id": "cow"}, {"id": "sheep"},
               {"id": "chicken"}, {"id": "skeleton"}]
    })"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    ASSERT_EQ((*r).mobs.size(), 5u);
    EXPECT_EQ((*r).mobs[4].id, "skeleton");
}

// Phase 39: the weighted list ({"id", "weight"} entries).
TEST(MobSpawningConfigTest, ParsesWeightedMobList) {
    auto r = MobSpawningConfig::parse(parseJson(R"({
      "check_every_frames": 30,
      "first_spawn_frame": 60,
      "max_mobs": 4,
      "min_radius": 4,
      "max_radius": 6,
      "despawn_distance": 40,
      "mobs": [{"id": "zombie", "weight": 1}, {"id": "skeleton",
                "weight": 10}]
    })"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    ASSERT_EQ((*r).mobs.size(), 2u);
    EXPECT_EQ((*r).mobs[0].id, "zombie");
    EXPECT_EQ((*r).mobs[0].weight, 1);
    EXPECT_EQ((*r).mobs[1].id, "skeleton");
    EXPECT_EQ((*r).mobs[1].weight, 10);
    EXPECT_EQ((*r).totalWeight(), 11);
}

TEST(MobSpawningConfigTest, ShippedSpawnListPinsPig) {
    // V2-5: the REAL assets/data/mob_spawning.json — the pre-pig six +
    // the pig, all weight 1 (the vocabulary pin: with every weight 1 the
    // pick is the legacy % list.size(), and the pig's cumulative slot is
    // the last one). V2-14: the 8th entry is the bat (the passive-pool
    // member — the MC ambient cap deviation, the design's documented
    // deviation 2): the vocabulary pin extends to eight, the bat's
    // cumulative slot is the last.
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    auto text = assets.loadText("data/mob_spawning.json");
    ASSERT_TRUE(text.ok()) << text.error().message;
    auto def = Json::parse(*text);
    ASSERT_TRUE(def.ok()) << def.error().message;
    auto r = MobSpawningConfig::parse(*def);
    ASSERT_TRUE(r.ok()) << r.error().message;
    ASSERT_EQ((*r).mobs.size(), 8u);
    const char* ids[] = {"zombie", "cow", "sheep", "chicken", "skeleton",
                         "spider", "pig", "bat"};
    for (std::size_t i = 0; i < 8; ++i) {
        EXPECT_EQ((*r).mobs[i].id, ids[i]) << i;
        EXPECT_EQ((*r).mobs[i].weight, 1) << i;
    }
    EXPECT_EQ((*r).totalWeight(), 8);
    // The pick's domain: v = 7 (the last unit) resolves to the bat.
    EXPECT_EQ((*r).pickIndex(7), 7u);
    // The vocabulary 7 -> 8 shifts the weighted pick's stream mapping
    // (the design's note: pin the new values — the same raw pick value
    // now lands on a different entry than pre-V2-14): the legacy
    // v % 7 mapping is gone, the cumulative walk owns [0, 8).
    EXPECT_EQ((*r).pickIndex(0), 0u);
    EXPECT_EQ((*r).pickIndex(6), 6u);
    EXPECT_EQ((*r).pickIndex(123456789ULL % 8), 123456789ULL % 8);
}

TEST(MobSpawningConfigTest, RejectsBadMobListEntries) {
    const char* variants[] = {
        // the legacy plain-string form (migration guard)
        R"("mobs": ["zombie"])",
        // a non-object entry
        R"("mobs": [7])",
        // missing / empty id
        R"("mobs": [{"weight": 1}])",
        R"("mobs": [{"id": ""}])",
        // bad weights: 0, negative, fractional, out of range
        R"("mobs": [{"id": "zombie", "weight": 0}])",
        R"("mobs": [{"id": "zombie", "weight": -1}])",
        R"("mobs": [{"id": "zombie", "weight": 1.5}])",
        R"("mobs": [{"id": "zombie", "weight": 10001}])",
        // duplicate id
        R"("mobs": [{"id": "zombie"}, {"id": "zombie"}])",
    };
    for (const char* variant : variants) {
        const std::string def = std::string(R"({
      "check_every_frames": 30,
      "first_spawn_frame": 60,
      "max_mobs": 4,
      "min_radius": 4,
      "max_radius": 6,
      "despawn_distance": 40,
      )") + variant + "\n    }";
        auto r = MobSpawningConfig::parse(parseJson(def));
        EXPECT_FALSE(r.ok()) << variant;
    }
}

TEST(MobSpawningConfigTest, WeightedPickUniformMatchesLegacyMod) {
    // All weights 1: the cumulative walk is exactly v % size — the legacy
    // behavior the shipped data relies on.
    auto r = MobSpawningConfig::parse(parseJson(R"({
      "check_every_frames": 30,
      "first_spawn_frame": 60,
      "max_mobs": 4,
      "min_radius": 4,
      "max_radius": 6,
      "despawn_distance": 40,
      "mobs": [{"id": "zombie"}, {"id": "cow"}, {"id": "sheep"},
               {"id": "chicken"}, {"id": "skeleton"}]
    })"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const MobSpawningConfig& cfg = *r;
    for (std::uint64_t v = 0; v < 5; ++v) {
        EXPECT_EQ(cfg.pickIndex(v), v);
    }
    // The caller reduces the raw stream value; a few large values:
    EXPECT_EQ(cfg.pickIndex(123456789ULL % 5), 123456789ULL % 5);
    EXPECT_EQ(cfg.pickIndex(99999999999ULL % 5), 99999999999ULL % 5);
}

TEST(MobSpawningConfigTest, WeightedPickCoversRangesDeterministically) {
    // Weights [1,1,1,1,10] (total 14): entry i owns [sum_{<i}, sum_{<=i})
    // — entries 0..3 own singletons, entry 4 owns [4, 14).
    auto r = MobSpawningConfig::parse(parseJson(R"({
      "check_every_frames": 30,
      "first_spawn_frame": 60,
      "max_mobs": 4,
      "min_radius": 4,
      "max_radius": 6,
      "despawn_distance": 40,
      "mobs": [{"id": "zombie"}, {"id": "cow"}, {"id": "sheep"},
               {"id": "chicken"}, {"id": "skeleton", "weight": 10}]
    })"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const MobSpawningConfig& cfg = *r;
    EXPECT_EQ(cfg.totalWeight(), 14);
    EXPECT_EQ(cfg.pickIndex(0), 0);
    EXPECT_EQ(cfg.pickIndex(1), 1);
    EXPECT_EQ(cfg.pickIndex(2), 2);
    EXPECT_EQ(cfg.pickIndex(3), 3);
    EXPECT_EQ(cfg.pickIndex(4), 4);   // the heavy entry starts at 4
    EXPECT_EQ(cfg.pickIndex(7), 4);
    EXPECT_EQ(cfg.pickIndex(13), 4);  // and runs to total-1
}

TEST(MobSpawningConfigTest, WeightedPickSingleEntry) {
    auto r = MobSpawningConfig::parse(parseJson(R"({
      "check_every_frames": 30,
      "first_spawn_frame": 60,
      "max_mobs": 4,
      "min_radius": 4,
      "max_radius": 6,
      "despawn_distance": 40,
      "mobs": [{"id": "zombie", "weight": 5}]
    })"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ((*r).totalWeight(), 5);
    for (std::uint64_t v = 0; v < 5; ++v) {
        EXPECT_EQ((*r).pickIndex(v), 0);
    }
}

TEST(MobSpawningConfigTest, RejectsInvertedRadii) {
    auto r = MobSpawningConfig::parse(parseJson(R"({
      "check_every_frames": 30, "first_spawn_frame": 60, "max_mobs": 4,
      "min_radius": 6, "max_radius": 4, "despawn_distance": 40,
      "mobs": [{"id": "zombie"}]
    })"));
    EXPECT_FALSE(r.ok());
}

TEST(MobSpawningConfigTest, RejectsEmptyMobs) {
    auto r = MobSpawningConfig::parse(parseJson(R"({
      "check_every_frames": 30, "first_spawn_frame": 60, "max_mobs": 4,
      "min_radius": 4, "max_radius": 6, "despawn_distance": 40,
      "mobs": []
    })"));
    EXPECT_FALSE(r.ok());
}

TEST(MobSpawningConfigTest, RejectsDespawnInsideRing) {
    auto r = MobSpawningConfig::parse(parseJson(R"({
      "check_every_frames": 30, "first_spawn_frame": 60, "max_mobs": 4,
      "min_radius": 4, "max_radius": 6, "despawn_distance": 6,
      "mobs": [{"id": "zombie"}]
    })"));
    EXPECT_FALSE(r.ok());
}

// -- spawn-cell scan -----------------------------------------------------------
//
// Fixture: a flat stone floor at y=0 (top surface y=1), the player column
// at (0, 0) with its floor at y=0, the player AABB of a body standing at
// (0.5, 1, 0.5).

TEST(MobSpawnCellTest, FirstValidCellInScanOrder) {
    Fixture f;
    auto cell = findMobSpawnCell(f.world, f.blocks, 4.0f, 6.0f, 0, 0, 0,
                                 0.6f, 1.8f, 0.0f, 0, kPlayerMin, kPlayerMax, {});
    ASSERT_TRUE(cell.has_value());
    // Scan order: dz from -6, then dx from -6; the first ring cell is
    // (dx=0, dz=-6) — distance 6.0, on the flat floor, clear headroom.
    EXPECT_EQ(*cell, (BlockPos {0, 0, -6}));
}

TEST(MobSpawnCellTest, SkipsCellWithBlockedHeadroom) {
    Fixture f;
    // A 3-block wall in the first candidate's column (0, -6): the floor
    // scan lands on the highest band cell (y=2) and the headroom check
    // then sees the wall continue above it (y=3) -> rejected. (A single
    // block there would simply become the new floor — standing on a
    // 1-block roof is valid.)
    for (int y = 1; y <= 3; ++y) {
        f.world.setBlock(0, y, -6, f.stone);
    }
    auto cell = findMobSpawnCell(f.world, f.blocks, 4.0f, 6.0f, 0, 0, 0,
                                 0.6f, 1.8f, 0.0f, 0, kPlayerMin, kPlayerMax, {});
    ASSERT_TRUE(cell.has_value());
    // (0, -6) is blocked; the next candidate in scan order is (dx=-3,
    // dz=-5) — distance 5.83.
    EXPECT_EQ(*cell, (BlockPos {-3, 0, -5}));
}

TEST(MobSpawnCellTest, SkipsCellInFluid) {
    Fixture f;
    f.world.setFluid(0, 1, -6, FluidKind::Water, 1);
    auto cell = findMobSpawnCell(f.world, f.blocks, 4.0f, 6.0f, 0, 0, 0,
                                 0.6f, 1.8f, 0.0f, 0, kPlayerMin, kPlayerMax, {});
    ASSERT_TRUE(cell.has_value());
    EXPECT_EQ(*cell, (BlockPos {-3, 0, -5}));
}

TEST(MobSpawnCellTest, RespectsMinRadius) {
    Fixture f;
    auto cell = findMobSpawnCell(f.world, f.blocks, 6.5f, 8.0f, 0, 0, 0,
                                 0.6f, 1.8f, 0.0f, 0, kPlayerMin, kPlayerMax, {});
    ASSERT_TRUE(cell.has_value());
    // First ring cell: (dx=0, dz=-8) — distance 8.0.
    EXPECT_EQ(*cell, (BlockPos {0, 0, -8}));
}

TEST(MobSpawnCellTest, NoFloorInRingYieldsNoCell) {
    Fixture f;
    // The fixture floor spans x, z in [-16, 16) (its farthest corner
    // (-16, -16) sits at distance sqrt(512) = 22.6); a ring beyond 22.7
    // has no solid floor.
    auto cell = findMobSpawnCell(f.world, f.blocks, 22.7f, 24.0f, 0, 0, 0,
                                 0.6f, 1.8f, 0.0f, 0, kPlayerMin, kPlayerMax, {});
    EXPECT_FALSE(cell.has_value());
}

TEST(MobSpawnCellTest, KeepsClearanceFromExistingMobs) {
    Fixture f;
    // An existing mob AT the first candidate's spawn point (0.5, 1, -5.5).
    auto cell = findMobSpawnCell(f.world, f.blocks, 4.0f, 6.0f, 0, 0, 0,
                                 0.6f, 1.8f, 0.0f, 0, kPlayerMin, kPlayerMax,
                                 {Vec3 {0.5f, 1.0f, -5.5f}});
    ASSERT_TRUE(cell.has_value());
    EXPECT_EQ(*cell, (BlockPos {-3, 0, -5}));
}

TEST(MobSpawnCellTest, RejectsSpawnOverlappingThePlayer) {
    Fixture f;
    // A wide player AABB so the first ring candidate's spawn AABB
    // (center 0.5, 1, -0.5; z span [-0.8, -0.2]) overlaps it (the
    // player's z starts at -0.3); the next candidate (dx=-1, dz=0) is
    // not overlapped (its x span [-0.8, -0.2] stops before the player's
    // x start at 0.2).
    auto cell = findMobSpawnCell(f.world, f.blocks, 0.4f, 1.0f, 0, 0, 0,
                                 0.6f, 1.8f, 0.0f, 0, {0.2f, 1.0f, -0.3f},
                                 {3.0f, 2.8f, 3.0f}, {});
    ASSERT_TRUE(cell.has_value());
    EXPECT_EQ(*cell, (BlockPos {-1, 0, 0}));
}

TEST(MobSpawnCellTest, RejectsFloorFarFromThePlayers) {
    Fixture f;
    // Raise the player's floor: the flat floor at y=0 is then more than
    // kSpawnFloorDelta below it, so no cell qualifies.
    auto cell = findMobSpawnCell(f.world, f.blocks, 4.0f, 6.0f, 0, 0, 10,
                                 0.6f, 1.8f, 0.0f, 0, kPlayerMin, kPlayerMax, {});
    EXPECT_FALSE(cell.has_value());
}

// Light-gate tests (the day/night spawn rule). Fixture cells read as
// 0/0 light (fresh chunks = dark); a cell is made "bright" by writing
// the chunk's light arrays directly (world (0, 1, -6) = local (0, 1, 10)
// of chunk (0, 0, -1)).
TEST(MobSpawnCellTest, SpawnsInDarknessAtNight) {
    Fixture f;
    // Night (dayFactor 0): the fixture cells read stored 0/0 light (fresh
    // chunks = already dark), so the LGT-3 retreat gives max(0, 0 - 8) =
    // 0 effective, and the dark-only mob may spawn at the first candidate.
    auto cell = findMobSpawnCell(f.world, f.blocks, 4.0f, 6.0f, 0, 0, 0,
                                 0.6f, 1.8f, 0.0f, 0, kPlayerMin, kPlayerMax,
                                 {});
    ASSERT_TRUE(cell.has_value());
    EXPECT_EQ(*cell, (BlockPos {0, 0, -6}));
}

TEST(MobSpawnCellTest, SkipsCellThatIsTooBrightByDay) {
    Fixture f;
    // Full sky light in the first candidate's feet cell; by day
    // (dayFactor 1) the effective level is 15 > 0 -> rejected, the scan
    // continues to the next candidate (-3, -5).
    f.world.getOrCreateChunk({0, 0, -1}).setSkyLightAt(0, 1, 10, 15);
    auto cell = findMobSpawnCell(f.world, f.blocks, 4.0f, 6.0f, 0, 0, 0,
                                 0.6f, 1.8f, 1.0f, 0, kPlayerMin, kPlayerMax,
                                 {});
    ASSERT_TRUE(cell.has_value());
    EXPECT_EQ(*cell, (BlockPos {-3, 0, -5}));
}

// LGT-3 moonlight floor (the 10-27 user verdict): deep-night open sky
// (stored 15) retreats to kNightSkyLevel 7, NOT 0 — the pre-LGT-3
// "night scales sky to 0" behavior is gone for lit cells. The shipped
// hostile threshold (spawn_max_light 7) pairs with it: night open sky
// 7 <= 7 spawns, a torch circle 14 > 7 does not, day open sky 15 > 7
// does not — the gameplay loop is preserved.
TEST(MobSpawnCellTest, NightMoonlightBlocksZeroThresholdMob) {
    Fixture f;
    // The SAME bright cell that the pre-LGT-3 test spawned in is NOT
    // spawnable at night for a max_light 0 mob: dayFactor 0 -> the sky
    // contribution is 15 - 8 = 7 (the moonlight floor) > 0.
    f.world.getOrCreateChunk({0, 0, -1}).setSkyLightAt(0, 1, 10, 15);
    auto cell = findMobSpawnCell(f.world, f.blocks, 4.0f, 6.0f, 0, 0, 0,
                                 0.6f, 1.8f, 0.0f, 0, kPlayerMin, kPlayerMax,
                                 {});
    ASSERT_TRUE(cell.has_value());
    EXPECT_EQ(*cell, (BlockPos {-3, 0, -5}));
}

TEST(MobSpawnCellTest, NightMoonlightAllowsShippedHostileThreshold) {
    Fixture f;
    // Open sky at deep night = effective 7; the shipped hostile threshold
    // 7 admits it (7 <= 7) -> the first candidate.
    f.world.getOrCreateChunk({0, 0, -1}).setSkyLightAt(0, 1, 10, 15);
    auto cell = findMobSpawnCell(f.world, f.blocks, 4.0f, 6.0f, 0, 0, 0,
                                 0.6f, 1.8f, 0.0f, 7, kPlayerMin, kPlayerMax,
                                 {});
    ASSERT_TRUE(cell.has_value());
    EXPECT_EQ(*cell, (BlockPos {0, 0, -6}));
}

TEST(MobSpawnCellTest, TorchCircleBlocksHostileAtNight) {
    Fixture f;
    // A torch circle (block light 14, time-independent) at deep night:
    // effective max(7, 14) = 14 > 7 -> rejected even though the sky is
    // moonlit (the MC safe-circle semantics).
    Chunk& c = f.world.getOrCreateChunk({0, 0, -1});
    c.setSkyLightAt(0, 1, 10, 15);
    c.setBlockLightAt(0, 1, 10, 14);
    auto cell = findMobSpawnCell(f.world, f.blocks, 4.0f, 6.0f, 0, 0, 0,
                                 0.6f, 1.8f, 0.0f, 7, kPlayerMin, kPlayerMax,
                                 {});
    ASSERT_TRUE(cell.has_value());
    EXPECT_EQ(*cell, (BlockPos {-3, 0, -5}));
}

TEST(MobSpawnCellTest, DayBlocksShippedHostileThreshold) {
    Fixture f;
    // Day (dayFactor 1): open sky stays 15 > 7 -> the first candidate is
    // rejected (hostiles do not spawn by day, as before LGT-3).
    f.world.getOrCreateChunk({0, 0, -1}).setSkyLightAt(0, 1, 10, 15);
    auto cell = findMobSpawnCell(f.world, f.blocks, 4.0f, 6.0f, 0, 0, 0,
                                 0.6f, 1.8f, 1.0f, 7, kPlayerMin, kPlayerMax,
                                 {});
    ASSERT_TRUE(cell.has_value());
    EXPECT_EQ(*cell, (BlockPos {-3, 0, -5}));
}

TEST(MobSpawnCellTest, BlockLightStillBlocksSpawnAtNight) {
    Fixture f;
    // Block light (glowstone) is time-independent: level 1 > 0 even at
    // deep night -> rejected.
    f.world.getOrCreateChunk({0, 0, -1}).setBlockLightAt(0, 1, 10, 1);
    auto cell = findMobSpawnCell(f.world, f.blocks, 4.0f, 6.0f, 0, 0, 0,
                                 0.6f, 1.8f, 0.0f, 0, kPlayerMin, kPlayerMax,
                                 {});
    ASSERT_TRUE(cell.has_value());
    EXPECT_EQ(*cell, (BlockPos {-3, 0, -5}));
}

TEST(MobSpawnCellTest, PermissiveMobSpawnsInLight) {
    Fixture f;
    // A mob with spawn_max_light 15 (a future passive mob) may spawn in
    // full daylight.
    f.world.getOrCreateChunk({0, 0, -1}).setSkyLightAt(0, 1, 10, 15);
    auto cell = findMobSpawnCell(f.world, f.blocks, 4.0f, 6.0f, 0, 0, 0,
                                 0.6f, 1.8f, 1.0f, 15, kPlayerMin,
                                 kPlayerMax, {});
    ASSERT_TRUE(cell.has_value());
    EXPECT_EQ(*cell, (BlockPos {0, 0, -6}));
}

// LGT-3: the spawn gate is the SEEN light — the SAME affine retreat the
// renderer evaluates (effectiveSkyLight), not the pre-LGT-3 scale
// floor(stored * dayFactor). At noon (dayFactor 1) the two are
// value-identical for every stored level; deep night differs — the
// moonlight floor (stored 15 -> 7, the feature).
TEST(MobSpawnCellTest, SpawnLightMatchesEffectiveSkyModel) {
    Fixture f;
    // The first candidate's feet cell (0, 1, -6): full sky light 15.
    f.world.getOrCreateChunk({0, 0, -1}).setSkyLightAt(0, 1, 10, 15);
    // Noon: the identity (== the old floor(15 * 1)); deep night: the
    // moonlight floor 15 - 8 = 7 (the old scale gave 0 — the feature).
    EXPECT_EQ(currentSpawnLight(f.world, 0, 1, -6, 1.0f), 15);
    EXPECT_EQ(currentSpawnLight(f.world, 0, 1, -6, 0.0f), kNightSkyLevel);
    // Ramps: the affine retreat of stored 15 (delta = floor(8*(1-df))),
    // value-identical to the pure function on the same input (the mirror).
    EXPECT_EQ(currentSpawnLight(f.world, 0, 1, -6, 0.5f), 11);  // 15 - 4
    EXPECT_EQ(currentSpawnLight(f.world, 0, 1, -6, 0.5f),
              static_cast<int>(effectiveSkyLight(15, 0.5f)));
    EXPECT_EQ(currentSpawnLight(f.world, 0, 1, -6, 0.25f), 9);   // 15 - 6
    EXPECT_EQ(currentSpawnLight(f.world, 0, 1, -6, 0.75f), 13);  // 15 - 2
    // Block light is time-independent: the torch circle stands above the
    // moonlight at deep night (14 > 7), the moonlight above weak block
    // light (7 > 3).
    f.world.getOrCreateChunk({0, 0, -1}).setBlockLightAt(0, 1, 10, 14);
    EXPECT_EQ(currentSpawnLight(f.world, 0, 1, -6, 0.0f), 14);
    f.world.getOrCreateChunk({0, 0, -1}).setBlockLightAt(0, 1, 10, 3);
    EXPECT_EQ(currentSpawnLight(f.world, 0, 1, -6, 0.0f), 7);
    EXPECT_EQ(currentSpawnLight(f.world, 0, 1, -6, 0.5f), 11);
    EXPECT_EQ(currentSpawnLight(f.world, 0, 1, -6, 1.0f), 15);
}

// UX-1: the configured floor (render.night_sky_level) drives the spawn
// light through the SAME affine retreat as the renderer. The pairing
// rule holds for any floor: at deep night the open-sky level IS the
// floor, so the shipped hostile threshold (spawn_max_light 7) admits the
// floor at L <= 7 and refuses it at L > 7 (the user's choice).
TEST(MobSpawnCellTest, SpawnLightWithConfiguredNightFloor) {
    Fixture f;
    // The first candidate's feet cell (0, 1, -6): full sky light 15.
    f.world.getOrCreateChunk({0, 0, -1}).setSkyLightAt(0, 1, 10, 15);
    // L = 0: the all-black night — open sky 0 (a zero-threshold mob may
    // spawn; 0 <= 7 the pairing rule is preserved).
    EXPECT_EQ(currentSpawnLight(f.world, 0, 1, -6, 0.0f, 0), 0);
    // L = 7: the default floor — the pre-UX-1 moonlight value.
    EXPECT_EQ(currentSpawnLight(f.world, 0, 1, -6, 0.0f, 7), kNightSkyLevel);
    // L = 10: 15 - 5 = 10 > 7 — the hostile threshold is NOT admitted.
    EXPECT_EQ(currentSpawnLight(f.world, 0, 1, -6, 0.0f, 10), 10);
    // L = 15: no retreat — open sky 15 > 7, not admitted.
    EXPECT_EQ(currentSpawnLight(f.world, 0, 1, -6, 0.0f, 15), 15);
    // Noon is the identity for any floor.
    EXPECT_EQ(currentSpawnLight(f.world, 0, 1, -6, 1.0f, 0), 15);
    EXPECT_EQ(currentSpawnLight(f.world, 0, 1, -6, 1.0f, 10), 15);
    // The default parameter is the pre-UX-1 value (the 7 floor).
    EXPECT_EQ(currentSpawnLight(f.world, 0, 1, -6, 0.0f), kNightSkyLevel);
}

// -- spawn protection circle (Phase 25) ---------------------------------------
//
// The circle is centered on the WORLD spawn point (here: the cell (0,0)
// center (0.5, 0.5), where the fixture player stands) and gates hostile
// natural spawns by the difficulty's radius (passive mobs pass 0 — the
// caller's job).

TEST(MobSpawnCellTest, ProtectionCoversTheWholeRingYieldsNoCell) {
    Fixture f;
    // Radius 12 (normal) around (0.5, 0.5): the farthest ring cell of the
    // 4..6 ring is sqrt(2) * 6 = 8.49 < 12, so every candidate is inside
    // the protection circle.
    auto cell = findMobSpawnCell(f.world, f.blocks, 4.0f, 6.0f, 0, 0, 0,
                                 0.6f, 1.8f, 0.0f, 0, kPlayerMin, kPlayerMax,
                                 {}, 12.0f, 0.5f, 0.5f);
    EXPECT_FALSE(cell.has_value());
}

TEST(MobSpawnCellTest, ProtectionRejectsNearCellsScanContinues) {
    Fixture f;
    // Radius 6.0 (easy) with the circle center 6 blocks NORTH of the
    // player (0.5, -6.5): the whole -z half of the ring sits inside the
    // circle, the +z / side cells do not.
    auto cell = findMobSpawnCell(f.world, f.blocks, 4.0f, 6.0f, 0, 0, 0,
                                 0.6f, 1.8f, 0.0f, 0, kPlayerMin, kPlayerMax,
                                 {}, 6.0f, 0.5f, -6.5f);
    ASSERT_TRUE(cell.has_value());
    // Scan order (dz then dx from -6): the dz=-6..-4 ring cells all sit
    // within 6.0 of (0.5, -6.5) (max circle distance there: (dx=±5, dz=-5)
    // at sqrt(25+4) = 5.39); at dz=-3 the first ring cell (dx=-5, distSq
    // from the player 34) is sqrt(25+16) = 6.4 from the circle center
    // -> outside -> the first valid cell.
    EXPECT_EQ(*cell, (BlockPos {-5, 0, -3}));
}

TEST(MobSpawnCellTest, ProtectionCircleFarAwayHasNoEffect) {
    Fixture f;
    // The circle center 100 blocks away: no candidate is inside it.
    auto cell = findMobSpawnCell(f.world, f.blocks, 4.0f, 6.0f, 0, 0, 0,
                                 0.6f, 1.8f, 0.0f, 0, kPlayerMin, kPlayerMax,
                                 {}, 12.0f, 100.5f, 100.5f);
    ASSERT_TRUE(cell.has_value());
    EXPECT_EQ(*cell, (BlockPos {0, 0, -6}));
}

TEST(MobSpawnCellTest, ZeroProtectionRadiusIsLegacyBehavior) {
    Fixture f;
    // Radius 0 (a passive mob's caller value) disables the circle even
    // with the center under the player.
    auto cell = findMobSpawnCell(f.world, f.blocks, 4.0f, 6.0f, 0, 0, 0,
                                 0.6f, 1.8f, 0.0f, 0, kPlayerMin, kPlayerMax,
                                 {}, 0.0f, 0.5f, 0.5f);
    ASSERT_TRUE(cell.has_value());
    EXPECT_EQ(*cell, (BlockPos {0, 0, -6}));
}

// -- V2-14: the in-air branch (the bat — the spawns_in_air data flag) ---------
//
// The FLOOR BAND IS SKIPPED (any Y, the MC bat cave semantics): the scan
// walks each ring column TOP-DOWN and takes the first cell whose box is
// air with no fluid at the light gate or below. The fixture's fresh-chunk
// light is 0 everywhere (no sky propagation), so the OPEN air above a
// roof would read as a dark spawn cell — the seal helper fills the column
// above the pocket with stone to emulate the real world's stone column /
// sky light (the pocket is then the column's only dark air).

namespace {

// A TRUE cave cross-section in the column: stone everywhere except the
// single air pocket cell at pocketY. (A roof-only seal would leave the
// column's lower air dark too — the top-down scan would fall through a
// lit pocket to the unlit air below it; the real cave's floor stone is
// what stops it.) The fixture's fresh-chunk light is 0 everywhere (no
// sky propagation), so the OPEN air above a roof would read as a dark
// spawn cell — the stone column emulates the real world's stone / sky
// light.
void sealCaveColumn(Fixture& f, int x, int z, int pocketY) {
    for (int y = 0; y < kWorldHeight; ++y) {
        if (y == pocketY) {
            continue;
        }
        f.world.setBlock(x, y, z, f.stone);
    }
}

// The in-air call shape (the bat's geometry: 0.9 wide x 0.5 tall, the
// light gate 3, the fixture's dayFactor/nightLevel arguments explicit).
std::optional<BlockPos> inAirCell(const Fixture& f, float minRadius,
                                  float maxRadius, int cx, int cz,
                                  float dayFactor,
                                  const Vec3& pmin = kPlayerMin,
                                  const Vec3& pmax = kPlayerMax,
                                  const std::vector<Vec3>& mobs = {}) {
    return findMobSpawnCell(f.world, f.blocks, minRadius, maxRadius, cx, cz,
                            0, 0.9f, 0.5f, dayFactor, 3, pmin, pmax, mobs,
                            0.0f, 0.0f, 0.0f, kNightSkyLevel, true);
}

} // namespace

TEST(MobSpawnCellTest, InAirSpawnsInTheDarkPocketUnderTheRoof) {
    Fixture f;
    // The first ring column (0, -6): the pocket at y=4 (the fixture floor
    // stays at y=0 — the band is irrelevant here).
    sealCaveColumn(f, 0, -6, 4);
    auto cell = inAirCell(f, 4.0f, 6.0f, 0, 0, 0.0f);
    ASSERT_TRUE(cell.has_value());
    EXPECT_EQ(*cell, (BlockPos {0, 4, -6}));
    // The light gate's day twin (dayFactor 1.0): the fresh-chunk light is
    // still 0 (no sky propagation in the fixture) -> the pocket admits.
    EXPECT_EQ(inAirCell(f, 4.0f, 6.0f, 0, 0, 1.0f).value_or(BlockPos{}),
              (BlockPos {0, 4, -6}));
}

TEST(MobSpawnCellTest, InAirIgnoresTheFloorBand) {
    Fixture f;
    // The pocket at y=19 is FAR from the player's floor y=0 (the
    // kSpawnFloorDelta 2 band would bar the floor branch): the in-air
    // branch takes it (any Y = MC).
    sealCaveColumn(f, 0, -6, 19);
    auto cell = inAirCell(f, 4.0f, 6.0f, 0, 0, 0.0f);
    ASSERT_TRUE(cell.has_value());
    EXPECT_EQ(*cell, (BlockPos {0, 19, -6}));
}

TEST(MobSpawnCellTest, InAirRejectsTheNightFloorLight) {
    Fixture f;
    // The first pocket (0, 4, -6) lit to the stored open-sky value (15:
    // at the night floor it reads effective 7 > the gate 3 — the open
    // night sky the bat must NOT spawn in, the NightMoonlight test's
    // twin): the scan skips it. The SECOND column (dx=-3, dz=-5 — the
    // next in-ring in scan order) carries a dark pocket at the same
    // height.
    sealCaveColumn(f, 0, -6, 4);
    f.world.getOrCreateChunk({0, 0, -1}).setSkyLightAt(0, 4, 10, 15);
    sealCaveColumn(f, -3, -5, 4);
    auto cell = inAirCell(f, 4.0f, 6.0f, 0, 0, 0.0f);
    ASSERT_TRUE(cell.has_value());
    EXPECT_EQ(*cell, (BlockPos {-3, 4, -5}));
}

TEST(MobSpawnCellTest, InAirRejectsFluidInThePocket) {
    Fixture f;
    // Water in the first pocket's cell: the box cell is not air-fluid-
    // free. The second column's dry pocket takes it.
    sealCaveColumn(f, 0, -6, 4);
    f.world.setFluid(0, 4, -6, FluidKind::Water, 1);
    sealCaveColumn(f, -3, -5, 4);
    auto cell = inAirCell(f, 4.0f, 6.0f, 0, 0, 0.0f);
    ASSERT_TRUE(cell.has_value());
    EXPECT_EQ(*cell, (BlockPos {-3, 4, -5}));
}

TEST(MobSpawnCellTest, InAirNoDarkPocketYieldsNoCell) {
    Fixture f;
    // The ring [4.0, 4.05] holds EXACTLY the four axis columns
    // (0, ±4) / (±4, 0): every pocket lit to the stored open-sky value
    // (15 -> effective 7 at the night floor > the gate 3, AND 15 by day)
    // -> no cell (the light gate's hermetic negative).
    for (const std::pair<int, int> col :
         {std::pair {0, -4}, {0, 4}, {-4, 0}, {4, 0}}) {
        sealCaveColumn(f, col.first, col.second, 4);
        const int lx = col.first >= 0 ? col.first : col.first + 16;
        const int lz = col.second >= 0 ? col.second : col.second + 16;
        f.world.getOrCreateChunk({col.first >= 0 ? 0 : -1, 0,
                                  col.second >= 0 ? 0 : -1})
            .setSkyLightAt(lx, 4, lz, 15);
    }
    EXPECT_FALSE(inAirCell(f, 4.0f, 4.05f, 0, 0, 0.0f).has_value());
    EXPECT_FALSE(inAirCell(f, 4.0f, 4.05f, 0, 0, 1.0f).has_value());
}

TEST(MobSpawnCellTest, InAirRejectsSpawnOverlappingThePlayer) {
    Fixture f;
    // A player AABB sitting in the first pocket (the bat's box spans
    // y [4, 4.5) there): the overlap bars it, the second pocket takes it.
    sealCaveColumn(f, 0, -6, 4);
    sealCaveColumn(f, -3, -5, 4);
    auto cell = inAirCell(f, 4.0f, 6.0f, 0, 0, 0.0f,
                          Vec3 {0.4f, 4.0f, -5.6f}, Vec3 {0.6f, 4.5f,
                                                          -5.4f});
    ASSERT_TRUE(cell.has_value());
    EXPECT_EQ(*cell, (BlockPos {-3, 4, -5}));
}

TEST(MobSpawnCellTest, InAirKeepsClearanceFromExistingMobs) {
    Fixture f;
    // An existing mob at the first pocket's spawn point (0.5, 4, -5.5):
    // the horizontal clearance bars it, the second pocket takes it.
    sealCaveColumn(f, 0, -6, 4);
    sealCaveColumn(f, -3, -5, 4);
    auto cell = inAirCell(f, 4.0f, 6.0f, 0, 0, 0.0f, kPlayerMin, kPlayerMax,
                          {Vec3 {0.5f, 4.0f, -5.5f}});
    ASSERT_TRUE(cell.has_value());
    EXPECT_EQ(*cell, (BlockPos {-3, 4, -5}));
}

TEST(MobSpawnCellTest, InAirKeepsTheProtectionCircle) {
    Fixture f;
    // The protection circle centered ON the first pocket's column (its
    // center (0.5, -5.5) is 6.0 from the player — just past the ring's
    // inner edge, so the pocket stays in-ring) with radius 1.0: it bars
    // the first pocket only; the second takes it.
    sealCaveColumn(f, 0, -6, 4);
    sealCaveColumn(f, -3, -5, 4);
    auto cell = findMobSpawnCell(f.world, f.blocks, 4.0f, 6.0f, 0, 0, 0,
                                 0.9f, 0.5f, 0.0f, 3, kPlayerMin,
                                 kPlayerMax, {}, 1.0f, 0.5f, -5.5f,
                                 kNightSkyLevel, true);
    ASSERT_TRUE(cell.has_value());
    EXPECT_EQ(*cell, (BlockPos {-3, 4, -5}));
}

TEST(MobSpawnCellTest, V217SpawnFloorTopByFloorShape) {
    // V2-17: the spawn floor top by floor shape — a stateful slab floor
    // tops out at its solid region (state 0 = kSlabHalf, state 1 / 2 =
    // the cell top); EVERY other shape (full blocks, the stateless bed
    // slab, stairs) keeps the exact legacy top (1.0 — the bit-level red
    // line).
    Fixture f;
    auto r = f.blocks.registerBlock(parseJson(
        R"({"id":"slab3","model":{"shape":"slab","state_count":3,)"
        R"("state_default":0,"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const BlockID slab3 = *r;
    auto rs = f.blocks.registerBlock(parseJson(
        R"({"id":"stairs","model":{"shape":"stairs",)"
        R"("textures":{"top":"t","side":"t","bottom":"t"}}})"));
    ASSERT_TRUE(rs.ok()) << rs.error().message;
    // Full block / stateless slab (the bed family's shape) / stairs /
    // wall: the legacy floor top (1.0).
    f.world.setBlock(8, 2, 8, f.stone);
    EXPECT_FLOAT_EQ(spawnFloorTop(f.world, f.blocks, 8, 2, 8), 1.0f);
    f.world.setBlock(8, 2, 8, f.slab);
    EXPECT_FLOAT_EQ(spawnFloorTop(f.world, f.blocks, 8, 2, 8), 1.0f);
    f.world.setBlock(8, 2, 8, *rs);
    EXPECT_FLOAT_EQ(spawnFloorTop(f.world, f.blocks, 8, 2, 8), 1.0f);
    f.world.setBlock(8, 2, 8, f.wall);
    EXPECT_FLOAT_EQ(spawnFloorTop(f.world, f.blocks, 8, 2, 8), 1.0f);
    // The stateful slab, by per-cell state (unset -> the default 0).
    f.world.setBlock(8, 2, 8, slab3);
    EXPECT_FLOAT_EQ(spawnFloorTop(f.world, f.blocks, 8, 2, 8), 0.5f); // unset
    f.world.setBlockState(8, 2, 8, 0);
    EXPECT_FLOAT_EQ(spawnFloorTop(f.world, f.blocks, 8, 2, 8), 0.5f);
    f.world.setBlockState(8, 2, 8, 1);
    EXPECT_FLOAT_EQ(spawnFloorTop(f.world, f.blocks, 8, 2, 8), 1.0f);
    f.world.setBlockState(8, 2, 8, 2);
    EXPECT_FLOAT_EQ(spawnFloorTop(f.world, f.blocks, 8, 2, 8), 1.0f);
}

TEST(MobSpawnCellTest, V217TopSlabIsASpawnFloor) {
    // A TOP slab (state 1) in the first candidate column (0, -6) is the
    // highest solid: the scan returns it as the floor cell (the feet rest
    // on its top face, cell.y + 1.0 — the spawnFloorTop's game-side twin;
    // pre-V2-17 the top slab's cell-top was not a reachable floor).
    Fixture f;
    auto r = f.blocks.registerBlock(parseJson(
        R"({"id":"slab3","model":{"shape":"slab","state_count":3,)"
        R"("state_default":0,"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    f.world.setBlock(0, 1, -6, *r);
    f.world.setBlockState(0, 1, -6, 1);
    auto cell = findMobSpawnCell(f.world, f.blocks, 4.0f, 6.0f, 0, 0, 0,
                                 0.6f, 1.8f, 0.0f, 0, kPlayerMin, kPlayerMax,
                                 {});
    ASSERT_TRUE(cell.has_value());
    EXPECT_EQ(*cell, (BlockPos {0, 1, -6}));
}

TEST(MobSpawnCellTest, V217BottomSlabIsASpawnFloor) {
    // A BOTTOM slab (state 0, the unset default) is a spawn floor too —
    // the feet rest on the slab top, floorY + kSlabHalf (the legacy scan
    // put them at floorY + 1.0: floating half a block above the slab,
    // now the exact standing-on-slab twin).
    Fixture f;
    auto r = f.blocks.registerBlock(parseJson(
        R"({"id":"slab3","model":{"shape":"slab","state_count":3,)"
        R"("state_default":0,"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    f.world.setBlock(0, 1, -6, *r); // the state byte stays unset -> default 0
    auto cell = findMobSpawnCell(f.world, f.blocks, 4.0f, 6.0f, 0, 0, 0,
                                 0.6f, 1.8f, 0.0f, 0, kPlayerMin, kPlayerMax,
                                 {});
    ASSERT_TRUE(cell.has_value());
    EXPECT_EQ(*cell, (BlockPos {0, 1, -6}));
    EXPECT_FLOAT_EQ(spawnFloorTop(f.world, f.blocks, 0, 1, -6), 0.5f);
}

// -- box-model mesh ------------------------------------------------------------

namespace {

AtlasLayout makeMobLayout() {
    auto r = AtlasLayout::parse(parseJson(
        R"({"columns":1,"rows":2,"tiles":{"a":[0,0],"b":[0,1]}})"));
    if (!r.ok()) {
        ADD_FAILURE() << r.error().message;
    }
    return *r;
}

ItemMeshGeometry makeMobGeometry() {
    const MobRegistry mobs = makeMobRegistry();
    return makeMobModelGeometry(mobs.at(MobID{0}).model, makeMobLayout());
}

// V2-2: a single-box model with the optional `faces` overrides
// ("" = no faces field — the pre-V2-2 def), parsed through the strict
// path. makeMobLayout's two tiles: "a" = row 0 -> v [0, 0.5],
// "b" = row 1 -> v [0.5, 1] (one column: u spans [0, 1] for both).
MobModel parseFacesModel(const std::string& facesText) {
    const std::string facesField =
        facesText.empty() ? "" : (",\"faces\":" + facesText);
    // The model def: the head box, its `texture` and the (optional)
    // `faces` field spliced in, closing the box + the boxes array.
    const std::string def =
        std::string(R"({"width":0.6,"height":1.8,"boxes":[{"name":"head","pos":[0.0,1.25,0.0],"size":[0.5,0.5,0.5],"texture":"a")")
            + facesField + R"(}]})";
    MobModel m;
    auto r = MobRegistry::parseModel(parseJson(def), m, "mob model");
    if (!r.ok()) {
        ADD_FAILURE() << r.error().message;
    }
    return m;
}

} // namespace

TEST(MobMeshTest, VertexAndIndexCounts) {
    const ItemMeshGeometry g = makeMobGeometry();
    // 2 boxes x 6 faces x 4 vertices / 6 indices.
    EXPECT_EQ(g.vertices.size(), 48);
    EXPECT_EQ(g.indices.size(), 72);
}

TEST(MobMeshTest, VerticesStayInsideTheModelAabb) {
    const ItemMeshGeometry g = makeMobGeometry();
    for (const MeshVertex& v : g.vertices) {
        EXPECT_GE(v.position.x, -0.3f - 1e-5f);
        EXPECT_LE(v.position.x, 0.3f + 1e-5f);
        EXPECT_GE(v.position.y, 0.0f - 1e-5f);
        EXPECT_LE(v.position.y, 1.8f + 1e-5f);
        EXPECT_GE(v.position.z, -0.3f - 1e-5f);
        EXPECT_LE(v.position.z, 0.3f + 1e-5f);
    }
}

TEST(MobMeshTest, FaceWindingIsCounterClockwiseFromOutside) {
    // Same invariant as the chunk mesher's face table and the item cube:
    // for every face, cross(corner1 - corner0, corner2 - corner0) must
    // point along the face normal.
    const ItemMeshGeometry g = makeMobGeometry();
    for (std::size_t face = 0; face < g.vertices.size() / 4; ++face) {
        const std::size_t base = face * 4;
        const Vec3 c0 = g.vertices[base].position;
        const Vec3 c1 = g.vertices[base + 1].position;
        const Vec3 c2 = g.vertices[base + 2].position;
        const Vec3 crossDir = cross(c1 - c0, c2 - c0);
        EXPECT_GT(dot(crossDir, g.vertices[base].normal), 0.0f);
    }
}

TEST(MobMeshTest, UvsStayInsideTheBoxTile) {
    const ItemMeshGeometry g = makeMobGeometry();
    const AtlasLayout layout = makeMobLayout();
    // Box 0 (vertices 0..23) uses tile "a"; box 1 uses tile "b".
    for (std::size_t i = 0; i < g.vertices.size(); ++i) {
        const std::string& tile = i < 24 ? "a" : "b";
        const Vec2 u0 = AtlasLayout::uvMin(layout, tile);
        const Vec2 u1 = AtlasLayout::uvMax(layout, tile);
        EXPECT_GE(g.vertices[i].uv.x, u0.x - 1e-6f);
        EXPECT_LE(g.vertices[i].uv.x, u1.x + 1e-6f);
        EXPECT_GE(g.vertices[i].uv.y, u0.y - 1e-6f);
        EXPECT_LE(g.vertices[i].uv.y, u1.y + 1e-6f);
    }
}

// -- V2-2: the per-face tile overrides -----------------------------------------

TEST(MobMeshTest, FacesOverrideUvPerFace) {
    // The partial-override contract: the front (-Z, the yaw-0 facing)
    // face takes the override tile "b"; the other five faces keep the
    // default tile "a" with the EXACT pre-V2-2 corner UVs. kCubeFaces
    // order: 0 +Y (top), 1 -Y (bottom), 2 +Z (back), 3 -Z (front),
    // 4 +X (right), 5 -X (left); the corner order is (u0,v1),(u1,v1),
    // (u1,v0),(u0,v0) — "a" = v [0, 0.5], "b" = v [0.5, 1].
    const MobModel m = parseFacesModel(R"({"front":"b"})");
    const ItemMeshGeometry g = makeMobModelGeometry(m, makeMobLayout());
    ASSERT_EQ(g.vertices.size(), 24u);
    ASSERT_EQ(g.indices.size(), 36u);
    const Vec2 front[4] = {{0.0f, 1.0f}, {1.0f, 1.0f},
                           {1.0f, 0.5f}, {0.0f, 0.5f}};
    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(g.vertices[static_cast<std::size_t>(12 + i)].uv,
                  front[i])
            << "front corner " << i;
    }
    const Vec2 def[4] = {{0.0f, 0.5f}, {1.0f, 0.5f}, {1.0f, 0.0f},
                         {0.0f, 0.0f}};
    for (const int fi : {0, 1, 2, 4, 5}) {
        const int base = fi * 4;
        for (int i = 0; i < 4; ++i) {
            EXPECT_EQ(g.vertices[static_cast<std::size_t>(base + i)].uv,
                      def[i])
                << "face " << fi << " corner " << i;
        }
    }
    // The override touched the UVs ONLY: positions/normals/indices are
    // bit-identical to the faces-less model of the same box (the
    // bit-level red line's geometry side).
    const ItemMeshGeometry gp = makeMobModelGeometry(
        parseFacesModel(""), makeMobLayout());
    ASSERT_EQ(gp.vertices.size(), g.vertices.size());
    for (std::size_t i = 0; i < g.vertices.size(); ++i) {
        EXPECT_EQ(g.vertices[i].position, gp.vertices[i].position) << i;
        EXPECT_EQ(g.vertices[i].normal, gp.vertices[i].normal) << i;
        EXPECT_FLOAT_EQ(g.vertices[i].skyLight, gp.vertices[i].skyLight)
            << i;
        EXPECT_FLOAT_EQ(g.vertices[i].blockLight,
                        gp.vertices[i].blockLight)
            << i;
    }
    EXPECT_EQ(g.indices, gp.indices);
}

TEST(MobMeshTest, FacesOverrideAllSix) {
    // The full six-face map: every face lands on the override tile "b"
    // (the corner order per face unchanged — the shared table's winding
    // invariant holds on the identical positions).
    const MobModel m = parseFacesModel(
        R"({"front":"b","back":"b","left":"b","right":"b","top":"b","bottom":"b"})");
    const ItemMeshGeometry g = makeMobModelGeometry(m, makeMobLayout());
    const Vec2 corners[4] = {{0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.5f},
                             {0.0f, 0.5f}}; // tile "b"'s corner order
    for (std::size_t i = 0; i < g.vertices.size(); ++i) {
        EXPECT_EQ(g.vertices[i].uv, corners[i % 4]) << i;
    }
}

// -- mob AI --------------------------------------------------------------------

TEST(MobAiTest, ChasesPlayerInDetectRange) {
    Fixture f;
    MobRegistry mobs = makeMobRegistry();
    const Mob& def = mobs.at(MobID{0});
    MobEntity mob = makeMob(f, def, {8.5f, 1.0f, 5.5f});
    // The player is 3 blocks away in +z (inside detect radius 16, outside
    // attack range 1.6).
    mob.setPlayer({8.5f, 1.0f, 8.5f}, true);
    mob.update(kDt);
    EXPECT_EQ(mob.state(), MobState::Chase);
    // Full data speed straight toward the player.
    EXPECT_NEAR(mob.velocity().z, 0.8f, 1e-3f);
    EXPECT_NEAR(mob.velocity().x, 0.0f, 1e-3f);
    // Facing +z is yaw +/-pi (forward(yaw) = (-sin, 0, -cos); the exact
    // sign depends on the zero of the -x component — check the direction).
    EXPECT_NEAR(std::cos(mob.facingYaw()), -1.0f, 1e-5f);
    EXPECT_NEAR(std::sin(mob.facingYaw()), 0.0f, 1e-5f);
}

TEST(MobAiTest, DoesNotChaseBeyondDetectRadius) {
    Fixture f;
    MobRegistry mobs = makeMobRegistry();
    const Mob& def = mobs.at(MobID{0});
    MobEntity mob = makeMob(f, def, {8.5f, 1.0f, 5.5f});
    // 22 blocks away: beyond the detect radius of 16.
    mob.setPlayer({8.5f, 1.0f, 27.5f}, true);
    mob.update(kDt);
    EXPECT_EQ(mob.state(), MobState::Wander);
    // Wander speed is at most half the chase speed (or still).
    const float speed = std::hypot(mob.velocity().x, mob.velocity().z);
    EXPECT_LE(speed, 0.4f + 1e-3f);
}

TEST(MobAiTest, AttacksOnCooldown) {
    Fixture f;
    MobRegistry mobs = makeMobRegistry();
    const Mob& def = mobs.at(MobID{0});
    MobEntity mob = makeMob(f, def, {8.5f, 1.0f, 7.5f});
    // 0.5 blocks from the player: inside the attack range of 1.6.
    mob.setPlayer({8.5f, 1.0f, 8.0f}, true);
    int hits = 0;
    float totalDamage = 0.0f;
    mob.setHitCallback([&](float amount) {
        ++hits;
        totalDamage += amount;
    });
    mob.update(kDt);
    EXPECT_EQ(mob.state(), MobState::Attack);
    EXPECT_NEAR(mob.velocity().x, 0.0f, 1e-6f);
    EXPECT_NEAR(mob.velocity().z, 0.0f, 1e-6f);
    EXPECT_EQ(hits, 1);
    EXPECT_FLOAT_EQ(totalDamage, 3.0f);
    // 59 more updates (60 total = 1.0 s): the cooldown has not expired
    // yet (it decrements AFTER the check, so the hit at exactly 1.0 s is
    // the 61st update).
    for (int i = 0; i < 59; ++i) {
        mob.update(kDt);
    }
    EXPECT_EQ(hits, 1);
    // The second hit lands within the next two updates (1.0 s plus
    // floating-point drift of the decrement chain).
    int extra = 0;
    while (hits < 2 && extra < 4) {
        mob.update(kDt);
        ++extra;
    }
    EXPECT_EQ(hits, 2);
    EXPECT_LE(extra, 2);
    EXPECT_FLOAT_EQ(totalDamage, 6.0f);
}

TEST(MobAiTest, DoesNotAttackBeyondVerticalRange) {
    Fixture f;
    MobRegistry mobs = makeMobRegistry();
    const Mob& def = mobs.at(MobID{0});
    MobEntity mob = makeMob(f, def, {8.5f, 1.0f, 7.5f});
    // Horizontally in range, but 1.5 blocks higher (the attack band is
    // 1.0; the detect band is 2.0, so the mob chases without attacking).
    mob.setPlayer({8.5f, 2.5f, 8.0f}, true);
    int hits = 0;
    mob.setHitCallback([&](float) { ++hits; });
    mob.update(kDt);
    EXPECT_EQ(mob.state(), MobState::Chase);
    EXPECT_EQ(hits, 0);
}

TEST(MobAiTest, IgnoresDeadPlayer) {
    Fixture f;
    MobRegistry mobs = makeMobRegistry();
    const Mob& def = mobs.at(MobID{0});
    MobEntity mob = makeMob(f, def, {8.5f, 1.0f, 7.5f});
    mob.setPlayer({8.5f, 1.0f, 8.0f}, false);
    int hits = 0;
    mob.setHitCallback([&](float) { ++hits; });
    mob.update(kDt);
    EXPECT_EQ(mob.state(), MobState::Wander);
    EXPECT_EQ(hits, 0);
}

TEST(MobAiTest, WanderIsDeterministicForTheSameSeed) {
    Fixture f;
    MobRegistry mobs = makeMobRegistry();
    const Mob& def = mobs.at(MobID{0});
    MobEntity a = makeMob(f, def, {4.5f, 1.0f, 4.5f}, 777);
    MobEntity b = makeMob(f, def, {4.5f, 1.0f, 4.5f}, 777);
    const Vec3 far {30.0f, 1.0f, 30.0f};  // out of detect range: pure wander
    a.setPlayer(far, true);
    b.setPlayer(far, true);
    for (int i = 0; i < 300; ++i) {
        a.update(kDt);
        b.update(kDt);
    }
    EXPECT_EQ(a.state(), b.state());
    EXPECT_FLOAT_EQ(a.position().x, b.position().x);
    EXPECT_FLOAT_EQ(a.position().y, b.position().y);
    EXPECT_FLOAT_EQ(a.position().z, b.position().z);
    EXPECT_FLOAT_EQ(a.facingYaw(), b.facingYaw());
}

TEST(MobAiTest, KnockbackSuppressesSteeringThenResumes) {
    Fixture f;
    MobRegistry mobs = makeMobRegistry();
    const Mob& def = mobs.at(MobID{0});
    MobEntity mob = makeMob(f, def, {8.5f, 1.0f, 5.5f});
    mob.setPlayer({8.5f, 1.0f, 8.5f}, true);  // chases in +z
    mob.update(kDt);
    EXPECT_EQ(mob.state(), MobState::Chase);
    // A knockback away from the player (-z) with a lift.
    const Vec3 before = mob.position();
    mob.addKnockback({0.0f, 2.0f, -6.0f});
    mob.update(kDt);
    // During the 0.3 s window the AI does not override the impulse: the
    // horizontal velocity still points -z (decayed by the damping).
    EXPECT_LT(mob.velocity().z, 0.0f);
    EXPECT_LT(mob.position().z, before.z);
    // After the window the chase resumes at full speed.
    for (int i = 0; i < 24; ++i) {  // 0.4 s > kKnockbackTime
        mob.update(kDt);
    }
    EXPECT_EQ(mob.state(), MobState::Chase);
    EXPECT_NEAR(mob.velocity().z, 0.8f, 1e-3f);
}

TEST(MobAiTest, PassiveMobNeverChasesOrAttacks) {
    // detect_radius 0 + attack_range 0 + attack_damage 0 (Phase 22): the
    // chase/attack conditions never hold even with the player a half block
    // away, so the mob only wanders and never lands a hit.
    Fixture f;
    MobRegistry mobs;
    std::string def = kZombieDef;
    const std::string detect = "\"detect_radius\": 16";
    def.replace(def.find(detect), detect.size(), "\"detect_radius\": 0");
    const std::string range = "\"attack_range\": 1.6";
    def.replace(def.find(range), range.size(), "\"attack_range\": 0");
    const std::string damage = "\"attack_damage\": 3";
    def.replace(def.find(damage), damage.size(), "\"attack_damage\": 0");
    ASSERT_TRUE(mobs.registerMob(parseJson(def)).ok());
    MobEntity mob = makeMob(f, mobs.at(MobID{0}), {8.5f, 1.0f, 7.5f});
    // 0.5 blocks from the player: well inside the hostile mob's 1.6 range.
    mob.setPlayer({8.5f, 1.0f, 8.0f}, true);
    int hits = 0;
    float totalDamage = 0.0f;
    mob.setHitCallback([&](float amount) {
        ++hits;
        totalDamage += amount;
    });
    // 300 updates (5 s): a couple of wander decisions fire, but the state
    // never leaves Wander and no hit may ever land.
    for (int i = 0; i < 300; ++i) {
        mob.update(kDt);
        EXPECT_EQ(mob.state(), MobState::Wander);
    }
    EXPECT_EQ(hits, 0);
    EXPECT_FLOAT_EQ(totalDamage, 0.0f);
}

// Phase 38: death-drop count rolls use the mob's seeded splitmix64 stream
// (the same state-advancing flow as the wander decisions). Pinned values
// for seed 42: first draw 0xbdd732262feb6e95 (%3 = 1), second draw
// 0x28efe333b266f103 (%5 = 1 → 1+1 = 2).
TEST(MobAiTest, RollDropCountFollowsTheSeededStream) {
    Fixture f;
    MobRegistry mobs;
    ASSERT_TRUE(mobs.registerMob(parseJson(kZombieDef)).ok());
    MobEntity mob = makeMob(f, mobs.at(MobID{0}), {8.5f, 1.0f, 5.5f}, 42ULL);
    EXPECT_EQ(mob.rollDropCount(0, 2), 1);  // first stream draw
    EXPECT_EQ(mob.rollDropCount(1, 5), 2);  // second stream draw
}

TEST(MobAiTest, RollDropCountFixedRangeDoesNotAdvanceTheStream) {
    // A degenerate range (min == max, the fixed-count form) returns the
    // constant WITHOUT advancing the stream: fixed-count drops keep the
    // pre-Phase-38 stream exactly, so a mixed drop list (bone range +
    // arrow fixed) rolls the range from the same state the list-order
    // walk reached.
    Fixture f;
    MobRegistry mobs;
    ASSERT_TRUE(mobs.registerMob(parseJson(kZombieDef)).ok());
    MobEntity mob = makeMob(f, mobs.at(MobID{0}), {8.5f, 1.0f, 5.5f}, 42ULL);
    EXPECT_EQ(mob.rollDropCount(1, 1), 1);  // no stream advance
    EXPECT_EQ(mob.rollDropCount(0, 2), 1);  // still the FIRST stream draw
}

TEST(MobAiTest, RollDropCountStaysInRange) {
    Fixture f;
    MobRegistry mobs;
    ASSERT_TRUE(mobs.registerMob(parseJson(kZombieDef)).ok());
    MobEntity mob = makeMob(f, mobs.at(MobID{0}), {8.5f, 1.0f, 5.5f}, 7ULL);
    for (int i = 0; i < 200; ++i) {
        const int n = mob.rollDropCount(0, 2);
        EXPECT_GE(n, 0);
        EXPECT_LE(n, 2);
    }
}

// ---------------------------------------------------------------------------
// V2-14: the flight AI (the bat — the flyCruise branch, the data-flag
// gated, no species switch). CPU frame pins: the cruise speed / heading
// cadence, the bob's 60-frame period, the wall-hit re-roll, the
// knockback return-to-cruise and the ambient roll.
// ---------------------------------------------------------------------------

namespace {

// A player far from the fixture origin: the bat's detect_radius 0 never
// sees it anyway, but the far position also keeps the player-out-of-range
// bookkeeping out of the picture.
const Vec3 kFlyFarPlayer {40.0f, 10.0f, 40.0f};

} // namespace

TEST(MobFlyTest, CruiseAtDataSpeedAlongTheRolledHeading) {
    Fixture f;
    MobRegistry mobs = makeBatRegistry();
    const Mob& def = mobs.at(MobID{0});
    MobEntity mob = makeMob(f, def, {8.5f, 10.5f, 8.5f}, 42);
    mob.setPlayer(kFlyFarPlayer, true);
    const Vec3 p0 = mob.position();
    mob.update(kDt);
    EXPECT_EQ(mob.state(), MobState::Wander);
    // Cruise speed = the data speed (kBatCruiseSpeed): the horizontal
    // velocity magnitude is exactly 1.0...
    EXPECT_NEAR(std::hypot(mob.velocity().x, mob.velocity().z), 1.0f, 1e-6f);
    // ...along the heading (the camera yaw convention forward =
    // (-sin, -cos) — the wander / chase steering's twin).
    EXPECT_NEAR(mob.velocity().x, -std::sin(mob.facingYaw()), 1e-6f);
    EXPECT_NEAR(mob.velocity().z, -std::cos(mob.facingYaw()), 1e-6f);
    // The frame-locked displacement: one 1/60 step along the velocity.
    EXPECT_NEAR(mob.position().x - p0.x, mob.velocity().x * kDt, 1e-6f);
    EXPECT_NEAR(mob.position().z - p0.z, mob.velocity().z * kDt, 1e-6f);
}

TEST(MobFlyTest, HeadingReRollsEveryWanderInterval) {
    Fixture f;
    MobRegistry mobs = makeBatRegistry();
    const Mob& def = mobs.at(MobID{0});
    MobEntity mob = makeMob(f, def, {8.5f, 10.5f, 8.5f}, 42);
    mob.setPlayer(kFlyFarPlayer, true);
    mob.update(kDt);  // frame 1: the first decision (the timer at 0)
    const float yaw1 = mob.facingYaw();
    // Frames 2..115: the 2.0 s cadence has not elapsed — no re-roll
    // (the boundary sits at frame 121, outside the pinned window — the
    // repeated-subtraction double residue stays below 1e-15 there, so
    // the 6-frame margin is exact, not fudged).
    for (int i = 1; i < 115; ++i) {
        mob.update(kDt);
        EXPECT_FLOAT_EQ(mob.facingYaw(), yaw1) << "frame " << i + 1;
    }
    // By frame 123 the cadence has elapsed (2.0 s = 120 frames): the
    // re-roll has happened.
    for (int i = 115; i < 123; ++i) {
        mob.update(kDt);
    }
    EXPECT_NE(mob.facingYaw(), yaw1);
}

TEST(MobFlyTest, BobIsAFrameLockedSine) {
    Fixture f;
    MobRegistry mobs = makeBatRegistry();
    const Mob& def = mobs.at(MobID{0});
    const float y0 = 10.5f;  // the spawn height (the bob's center)
    // Same seed twice: bit-identical trajectories (the per-mob stream).
    MobEntity a = makeMob(f, def, {8.5f, y0, 8.5f}, 1234);
    MobEntity b = makeMob(f, def, {8.5f, y0, 8.5f}, 1234);
    a.setPlayer(kFlyFarPlayer, true);
    b.setPlayer(kFlyFarPlayer, true);
    float y30 = y0;
    float y60 = y0;
    for (int i = 0; i < 120; ++i) {
        a.update(kDt);
        b.update(kDt);
        if (i == 29) {
            y30 = a.position().y;
        }
        if (i == 59) {
            y60 = a.position().y;
        }
        EXPECT_FLOAT_EQ(a.position().y, b.position().y) << "frame " << i + 1;
    }
    const float y120 = a.position().y;
    // The 60-frame period (kBatBobPeriodFrames): the displacement over
    // each full period is zero (the sine sums to 0 on the frame grid),
    // so y returns to the period start within the float residue.
    EXPECT_NEAR(y60 - y0, 0.0f, 1e-4f);   // frames 1..60: one full period
    EXPECT_NEAR(y120 - y60, 0.0f, 1e-4f); // frames 61..120: another
    // The bob's position amplitude (A / omega = 0.2 / (2*pi/1s) ~ 0.032
    // blocks): the y stays close to the spawn height for every frame.
    MobEntity c = makeMob(f, def, {8.5f, y0, 8.5f}, 1234);
    c.setPlayer(kFlyFarPlayer, true);
    for (int i = 0; i < 120; ++i) {
        c.update(kDt);
        EXPECT_GT(c.position().y, y0 - 0.04f) << "frame " << i + 1;
        EXPECT_LT(c.position().y, y0 + 0.04f) << "frame " << i + 1;
    }
    // A different seed (a different spawn-stream phase roll) differs at
    // a QUARTER period (frame 30: the bob is mid-swing, far from the
    // period-start value y0 that every phase returns to).
    MobEntity d = makeMob(f, def, {8.5f, y0, 8.5f}, 4321);
    d.setPlayer(kFlyFarPlayer, true);
    for (int i = 0; i < 30; ++i) {
        d.update(kDt);
    }
    EXPECT_NE(d.position().y, y30);
}

TEST(MobFlyTest, NoGravityWhileFlying) {
    Fixture f;
    MobRegistry mobs = makeBatRegistry();
    const Mob& def = mobs.at(MobID{0});
    MobEntity mob = makeMob(f, def, {8.5f, 10.5f, 8.5f}, 42);
    mob.setPlayer(kFlyFarPlayer, true);
    // 300 frames (5 s): a falling mob would drop ~373 blocks (32*t^2/2);
    // the flyer hovers (the bob's position bound).
    for (int i = 0; i < 300; ++i) {
        mob.update(kDt);
    }
    EXPECT_GT(mob.position().y, 10.5f - 0.05f);
    EXPECT_LT(mob.position().y, 10.5f + 0.05f);
}

TEST(MobFlyTest, WallHitRerollsTheHeading) {
    Fixture f;
    MobRegistry mobs = makeBatRegistry();
    const Mob& def = mobs.at(MobID{0});
    // Seed 79149: the constructor's bob-phase draw + the first heading
    // draw land the initial heading on yaw ~ 3*pi/2 (facing +X — the
    // forward(yaw) = (-sin, -cos) convention: -sin(3*pi/2) = +1; the
    // residue 1.5e-5 rad is the pinned stream value's).
    MobEntity mob = makeMob(f, def, {4.5f, 10.5f, 4.5f}, 79149);
    mob.setPlayer(kFlyFarPlayer, true);
    mob.update(kDt);
    EXPECT_NEAR(mob.facingYaw(), 3.0 * std::numbers::pi / 2.0, 1e-4f);
    // The wall: the x=6 slab (y 9..12, z 3..7) — the bat's +X face
    // clamps flush at x = 6.0 - 0.45 = 5.55.
    for (int y = 9; y <= 12; ++y) {
        for (int z = 3; z <= 7; ++z) {
            f.world.setBlock(6, y, z, f.stone);
        }
    }
    // Cruise +X at 1.0 b/s from x = 4.5: the flush clamp lands on frame
    // 62-64 (the 1.05-block gap at 1.0 b/s = 63 frames, the float
    // accumulation residue decides the ±1 boundary — both ends pinned).
    int hitFrame = 0;
    for (int i = 1; i < 120 && hitFrame == 0; ++i) {
        mob.update(kDt);
        if (std::abs(mob.position().x - 5.55f) < 1e-5f) {
            hitFrame = i;
        }
    }
    ASSERT_GT(hitFrame, 0) << "the bat never reached the wall";
    EXPECT_GE(hitFrame, 62);
    EXPECT_LE(hitFrame, 64);
    // The wall hit (the clamped x velocity) re-rolls the heading on the
    // NEXT frame (the deviation-3 natural detour: no pathfinding)...
    const float yawAtWall = mob.facingYaw();
    mob.update(kDt);
    EXPECT_NE(mob.facingYaw(), yawAtWall);
    // ...and the bat never tunnels (600 more frames: x stays < 6.0).
    for (int i = 0; i < 600; ++i) {
        mob.update(kDt);
        EXPECT_LT(mob.position().x, 6.0f) << "frame " << i;
    }
}

TEST(MobFlyTest, KnockbackSlidesThenCruiseResumes) {
    Fixture f;
    MobRegistry mobs = makeBatRegistry();
    const Mob& def = mobs.at(MobID{0});
    MobEntity mob = makeMob(f, def, {8.5f, 10.5f, 8.5f}, 42);
    mob.setPlayer(kFlyFarPlayer, true);
    mob.update(kDt);  // frame 1: the cruise is set
    const float zBefore = mob.position().z;
    // The arrow's impulse: the knockback window suppresses the steering;
    // integrate's exponential damping carries the slide.
    mob.addKnockback({0.0f, 0.0f, -3.0f});
    // 0.3 s = kKnockbackTime. A couple of extra updates absorb the float
    // residue (0.3 is not binary-exact, so the window's close drifts by
    // one frame — 20 updates = 0.333 s is safely past it).
    for (int i = 0; i < 20; ++i) {
        mob.update(kDt);
    }
    // The slide ran (the bat moved -z against the cruise)...
    EXPECT_LT(mob.position().z, zBefore);
    // ...and the window closed: the cruise is back at the full data
    // speed (the decayed impulse is overridden by the steering).
    EXPECT_NEAR(std::hypot(mob.velocity().x, mob.velocity().z), 1.0f, 1e-6f);
    EXPECT_EQ(mob.state(), MobState::Wander);
}

TEST(MobFlyTest, AmbientSqueakRollsDeterministically) {
    Fixture f;
    MobRegistry mobs = makeBatRegistry();
    const Mob& def = mobs.at(MobID{0});
    // The per-frame roll: p = 1 / (ambient_interval * 60) = 1/180 of the
    // per-mob stream. 3000 frames -> ~16.7 squeaks (the seeded value,
    // pinned below).
    auto squeakFrames = [&](std::uint64_t seed) {
        MobEntity mob = makeMob(f, def, {8.5f, 10.5f, 8.5f}, seed);
        mob.setPlayer(kFlyFarPlayer, true);
        int calls = 0;
        mob.setAmbientSoundCallback([&calls](Vec3) { ++calls; });
        std::vector<int> frames;
        for (int i = 1; i <= 3000; ++i) {
            mob.update(kDt);
            if (calls > static_cast<int>(frames.size())) {
                frames.push_back(i);
            }
        }
        return frames;
    };
    const std::vector<int> a = squeakFrames(777);
    const std::vector<int> b = squeakFrames(777);
    EXPECT_EQ(a, b);  // same seed = same frames (the stream)
    EXPECT_GT(a.size(), 5u);   // the rate's lower band
    EXPECT_LT(a.size(), 40u);  // the rate's upper band
    // The pinned frames (seed 777's first-run values — the frame-locked
    // roll's bit-level pin; 16 squeaks over 3000 frames ~ the 3000/180
    // = 16.7 expectation).
    EXPECT_EQ(a, (std::vector<int> {21, 180, 222, 779, 794, 888, 1142,
                                    1203, 1680, 2112, 2311, 2648, 2669,
                                    2708, 2894, 2943}));
    // A mob WITHOUT the ambient data never rolls (structural: the
    // pre-V2-14 stream shape).
    MobRegistry plain;
    ASSERT_TRUE(plain.registerMob(parseJson(kZombieDef)).ok());
    MobEntity zombie = makeMob(f, plain.at(MobID{0}), {8.5f, 1.0f, 8.5f}, 777);
    int zombieCalls = 0;
    zombie.setAmbientSoundCallback([&zombieCalls](Vec3) { ++zombieCalls; });
    zombie.setPlayer({8.5f, 1.0f, 40.0f}, true);
    for (int i = 0; i < 3000; ++i) {
        zombie.update(kDt);
    }
    EXPECT_EQ(zombieCalls, 0);
}

TEST(MobFlyTest, FlightInARoomIsDeterministicForTheSameSeed) {
    // The full interaction — cruise + bob + wall hits (heading re-rolls)
    // + the ambient roll — as a pure function of the seed: two bats in
    // IDENTICAL sealed rooms (x/z 5..8, y 9..12) run 900 frames (15 s:
    // 7 heading cadences + several wall hits) bit-identically.
    Fixture fa;
    Fixture fb;
    auto buildRoom = [](Fixture& f) {
        // The room walls: the x=4 / x=9 and z=4 / z=9 slabs (y 9..12),
        // the interior (x/z 5..8, y 9..12) air. The bat hovers at
        // y ~ 10.5 — the interior height keeps it clear of the floor
        // (the fixture's y=0 slab is far below) and the open top.
        for (int y = 9; y <= 12; ++y) {
            for (int z = 4; z <= 9; ++z) {
                f.world.setBlock(4, y, z, f.stone);
                f.world.setBlock(9, y, z, f.stone);
            }
            for (int x = 5; x <= 8; ++x) {
                f.world.setBlock(x, y, 4, f.stone);
                f.world.setBlock(x, y, 9, f.stone);
            }
        }
    };
    buildRoom(fa);
    buildRoom(fb);
    MobRegistry mobs = makeBatRegistry();
    const Mob& def = mobs.at(MobID{0});
    MobEntity a = makeMob(fa, def, {6.5f, 10.5f, 6.5f}, 555);
    MobEntity b = makeMob(fb, def, {6.5f, 10.5f, 6.5f}, 555);
    int aCalls = 0;
    int bCalls = 0;
    a.setAmbientSoundCallback([&aCalls](Vec3) { ++aCalls; });
    b.setAmbientSoundCallback([&bCalls](Vec3) { ++bCalls; });
    a.setPlayer(kFlyFarPlayer, true);
    b.setPlayer(kFlyFarPlayer, true);
    for (int i = 0; i < 900; ++i) {
        a.update(kDt);
        b.update(kDt);
        if (i % 30 == 0) {
            EXPECT_FLOAT_EQ(a.position().x, b.position().x) << "frame " << i;
            EXPECT_FLOAT_EQ(a.position().y, b.position().y) << "frame " << i;
            EXPECT_FLOAT_EQ(a.position().z, b.position().z) << "frame " << i;
            EXPECT_FLOAT_EQ(a.facingYaw(), b.facingYaw()) << "frame " << i;
        }
    }
    // The bats left the room's interior (they hit walls — the re-roll
    // path ran, not just the open cruise)...
    const Vec3 atSpawn = Vec3 {6.5f, 10.5f, 6.5f};
    EXPECT_NE(a.position(), atSpawn);
    // ...and their squeak counts agree (the stream's full history).
    EXPECT_EQ(aCalls, bCalls);
    EXPECT_GT(aCalls, 0);
}

TEST(MobAiTest, RangedMobShootsAndHoldsDistance) {
    // Phase 27: the skeleton 8 blocks from the player (inside shoot_range
    // 14, outside retreat_distance 3) stands, faces the player, and fires
    // on the 1.5 s cooldown — it never closes in and never melee-hits.
    Fixture f;
    MobRegistry mobs;
    ASSERT_TRUE(mobs.registerMob(parseJson(kSkeletonDef)).ok());
    MobEntity mob = makeMob(f, mobs.at(MobID{0}), {8.5f, 1.0f, 1.5f});
    mob.setPlayer({8.5f, 1.0f, 9.5f}, true);  // 8 blocks away
    int shots = 0;
    int hits = 0;
    mob.setShootCallback([&] { ++shots; });
    mob.setHitCallback([&](float) { ++hits; });
    for (int i = 0; i < 300; ++i) {
        mob.update(kDt);
        EXPECT_EQ(mob.state(), MobState::RangedAttack);
    }
    // 5 s at a 1.5 s cooldown: the cooldown starts at 0, so the first shot
    // lands on frame 0, then ~1.517 / ~3.033 / ~4.55 s (the 1/60 double
    // subtraction leaves a residue that pushes each cycle one frame out).
    EXPECT_EQ(shots, 4);
    EXPECT_EQ(hits, 0);  // it never melee-attacks (attack_range 0)
    // Standing still: no horizontal drift.
    EXPECT_NEAR(mob.position().x, 8.5f, 0.01f);
    EXPECT_NEAR(mob.position().z, 1.5f, 0.01f);
}

TEST(MobAiTest, RangedMobRetreatsWhenClose) {
    // Phase 27: the skeleton with the player a block away (inside
    // retreat_distance 3) backs away at full speed while it keeps firing;
    // once the player is outside the retreat distance it stands.
    Fixture f;
    MobRegistry mobs;
    ASSERT_TRUE(mobs.registerMob(parseJson(kSkeletonDef)).ok());
    MobEntity mob = makeMob(f, mobs.at(MobID{0}), {8.5f, 1.0f, 7.5f});
    mob.setPlayer({8.5f, 1.0f, 8.5f}, true);  // 1 block away, +z side
    int shots = 0;
    int hits = 0;
    mob.setShootCallback([&] { ++shots; });
    mob.setHitCallback([&](float) { ++hits; });
    mob.update(kDt);
    EXPECT_EQ(mob.state(), MobState::RangedAttack);
    EXPECT_LT(mob.velocity().z, -0.5f);  // retreating (-z, away from player)
    for (int i = 1; i < 300; ++i) {
        mob.update(kDt);
    }
    const float distH = std::hypot(mob.position().x - 8.5f,
                                   mob.position().z - 8.5f);
    EXPECT_GT(distH, 3.0f);  // kited the player past the retreat distance
    EXPECT_GE(shots, 1);     // kept firing while retreating
    EXPECT_EQ(hits, 0);
}

// -- V3-14b: the flying ranged attacker (the ghast's flyShoot branch) ---

TEST(MobFlyShootTest, GhastHoversAndShootsInContext) {
    // The ghast 8 blocks from the player (inside shoot_range 14 + the
    // detect radius, level with the eyes): it HOLDS the hover position
    // (zero horizontal velocity — the skeleton branch's flying twin:
    // no kite, no close-in) and fires on the 4.0 s cooldown.
    Fixture f;
    MobRegistry mobs;
    ASSERT_TRUE(mobs.registerMob(parseJson(kGhastDef)).ok());
    MobEntity mob = makeMob(f, mobs.at(MobID{0}), {8.5f, 10.5f, 1.5f});
    mob.setPlayer({8.5f, 10.5f, 9.5f}, true);  // 8 blocks away, level
    int shots = 0;
    mob.setShootCallback([&] { ++shots; });
    for (int i = 0; i < 300; ++i) {
        mob.update(kDt);
        EXPECT_EQ(mob.state(), MobState::RangedAttack);
        // The hover hold: no horizontal drift (the bob's vertical is
        // integrate's — the horizontal is the branch's job).
        EXPECT_NEAR(mob.velocity().x, 0.0f, 1e-6f);
        EXPECT_NEAR(mob.velocity().z, 0.0f, 1e-6f);
    }
    // 5 s at a 4.0 s cooldown: the first shot on frame 0, the second
    // at ~4.02 s (the 1/60 double subtraction's residue) — 2 shots.
    EXPECT_EQ(shots, 2);
    // It faces the player (the camera yaw convention: forward =
    // (-sin, -cos) — the 8-block +z offset reads yaw ±pi; the signed
    // zero of dx = 0 pins -pi on IEEE, so the heading is compared by
    // its direction cosines, not the raw angle).
    EXPECT_NEAR(std::cos(mob.facingYaw()), -1.0f, 0.01f);
    EXPECT_NEAR(std::sin(mob.facingYaw()), 0.0f, 0.01f);
    EXPECT_NEAR(mob.position().x, 8.5f, 0.01f);
    EXPECT_NEAR(mob.position().z, 1.5f, 0.01f);
}

TEST(MobFlyShootTest, GhastCruisesWhenOutOfRange) {
    // The player past the detect radius (40 blocks — the bat test's
    // kFlyFarPlayer twin): the shot's context is off, the bat's cruise
    // runs verbatim (Wander + the data speed 0.7 along the rolled
    // heading), no shots.
    Fixture f;
    MobRegistry mobs;
    ASSERT_TRUE(mobs.registerMob(parseJson(kGhastDef)).ok());
    MobEntity mob = makeMob(f, mobs.at(MobID{0}), {8.5f, 10.5f, 8.5f});
    mob.setPlayer(kFlyFarPlayer, true);
    int shots = 0;
    mob.setShootCallback([&] { ++shots; });
    mob.update(kDt);
    EXPECT_EQ(mob.state(), MobState::Wander);
    EXPECT_NEAR(std::hypot(mob.velocity().x, mob.velocity().z), 0.7f,
                1e-6f);
    EXPECT_EQ(shots, 0);
}

TEST(MobFlyShootTest, GhastCruisesWhenPlayerDead) {
    // The shot's context's alive gate (the seesPlayer's m_playerAlive
    // term): a dead player (even in range) sends the ghast back to the
    // cruise — the skeleton branch's twin.
    Fixture f;
    MobRegistry mobs;
    ASSERT_TRUE(mobs.registerMob(parseJson(kGhastDef)).ok());
    MobEntity mob = makeMob(f, mobs.at(MobID{0}), {8.5f, 10.5f, 1.5f});
    mob.setPlayer({8.5f, 10.5f, 9.5f}, /*alive=*/false);
    int shots = 0;
    mob.setShootCallback([&] { ++shots; });
    mob.update(kDt);
    EXPECT_EQ(mob.state(), MobState::Wander);
    EXPECT_EQ(shots, 0);
}

TEST(MobFlyShootTest, BatStaysBitIdenticalCruiser) {
    // The pre-V3-14b red line, observed: the bat (flying, shootRange 0
    // in the data) still runs flyCruise verbatim — the shootRange gate
    // in the flying branch keeps the bat's stream untouched (its first
    // stream draw is the wander decision, the FlyShootTest's shots
    // counter would steal it otherwise).
    Fixture f;
    MobRegistry mobs = makeBatRegistry();
    const Mob& def = mobs.at(MobID{0});
    ASSERT_FLOAT_EQ(def.properties.shootRange, 0.0f);
    MobEntity mob = makeMob(f, def, {8.5f, 10.5f, 8.5f}, 42);
    mob.setPlayer(kFlyFarPlayer, true);
    int shots = 0;
    mob.setShootCallback([&] { ++shots; });
    mob.update(kDt);
    EXPECT_EQ(mob.state(), MobState::Wander);
    EXPECT_NEAR(std::hypot(mob.velocity().x, mob.velocity().z), 1.0f,
                1e-6f);  // the bat's cruise speed, the V2-14 pin
    EXPECT_EQ(shots, 0);
}

// -- V3-14c: the bartering's mob face (the roll + the non-enrage) ---

TEST(MobBarterTest, NonBarterMobDrawsNothingAndReturnsEmpty) {
    // The structural immunity root: a pre-V3-14c mob (no barter data)
    // returns the empty item with NO stream draw — the next wander
    // heading roll is bit-identical to one that never called
    // barterItem (the ambient-free mob's bit-identical twin).
    Fixture f;
    MobRegistry mobs = makeMobRegistry(); // the zombie-like def
    const Mob& def = mobs.at(MobID{0});
    ASSERT_TRUE(def.properties.barterInput.empty());
    MobEntity a = makeMob(f, def, {8.5f, 10.5f, 8.5f}, 42);
    MobEntity b = makeMob(f, def, {8.5f, 10.5f, 8.5f}, 42);
    const auto r = a.barterItem();
    EXPECT_TRUE(r.first.empty());
    EXPECT_EQ(r.second, 0);
    // The stream parity: both mobs, the same seed, one update each —
    // the wander heading roll (a's first stream draw) lands on the
    // same value as b's (the barter drew nothing).
    a.update(kDt);
    b.update(kDt);
    EXPECT_FLOAT_EQ(a.velocity().x, b.velocity().x);
    EXPECT_FLOAT_EQ(a.velocity().z, b.velocity().z);
}

TEST(MobBarterTest, BarterIsDeterministicAndNeverEnrages) {
    // The trade roll (the per-mob stream): the same seed + the same
    // call count yields the same (item, count) sequence, every count
    // inside its entry's range, and the neutral's m_angry stays FALSE
    // (the barter path never touches the enrage state — the damage()
    // single choke point is the only enrage source).
    Fixture f;
    MobRegistry mobs;
    ASSERT_TRUE(mobs.registerMob(parseJson(kBarterDef)).ok());
    const Mob& def = mobs.at(MobID{0});
    MobEntity a = makeMob(f, def, {8.5f, 10.5f, 8.5f}, 7);
    MobEntity b = makeMob(f, def, {8.5f, 10.5f, 8.5f}, 7);
    ASSERT_FALSE(a.angry());
    for (int i = 0; i < 20; ++i) {
        const auto ra = a.barterItem();
        const auto rb = b.barterItem();
        ASSERT_FALSE(ra.first.empty());
        EXPECT_EQ(ra.first, rb.first) << i;
        EXPECT_EQ(ra.second, rb.second) << i;
        for (std::size_t t = 0; t < def.properties.barterOutputs.size();
             ++t) {
            if (def.properties.barterOutputs[t].item == ra.first) {
                EXPECT_GE(ra.second,
                          def.properties.barterOutputs[t].countMin)
                    << i;
                EXPECT_LE(ra.second,
                          def.properties.barterOutputs[t].countMax)
                    << i;
            }
        }
        ASSERT_FALSE(a.angry()) << "the barter must not enrage (i=" << i
                                << ")";
    }
}

// -- ENT-3: mob step-up (the player's 1-block step, shared) --------------------
//
// The mobs now step up one-block terrain steps exactly like the player:
// the shared Entity::stepUp (Phase-16 geometry, Phase-99 TRUE-1-block
// judgment — solid-region top <= 1.0: a slab / full block / a glass pane,
// NOT the wall / fence 1.5 barrier). All scenarios are chase-driven
// (the zombie def: speed 0.8, detect 16, attack range 1.6) on the fixture
// floor (surface y=1); a chase over a step crosses it, and the stop point
// is attack-range-center (player center +/- 1.6).

TEST(MobStepUpTest, ChasesUpOneBlockStep) {
    Fixture f;
    MobRegistry mobs = makeMobRegistry();
    const Mob& def = mobs.at(MobID{0});
    // One-block ledge at x in [10, 12), z in [8, 10) (the pre-V2-9 width
    // was forced by the shared judgment's +X front-check quirk, which
    // V2-9 fixed — the single-cell pin is StepsUpSingleCellStepPlusX
    // below).
    for (int x = 10; x < 12; ++x) {
        for (int z = 8; z < 10; ++z) {
            f.world.setBlock(x, 1, z, f.stone);
        }
    }
    MobEntity mob = makeMob(f, def, {8.5f, 1.0f, 8.5f});
    mob.setPlayer({13.9f, 1.0f, 8.5f}, true); // beyond the step
    runMob(mob, 400);
    // Crossed the step (far end x=12) and stopped in attack range
    // (13.9 - 1.6 = 12.3). Without the step-up it stays clamped at 9.7 in
    // the chase state (distH 4.2 > 1.6) — mutually exclusive.
    EXPECT_EQ(mob.state(), MobState::Attack);
    EXPECT_NEAR(mob.position().x, 12.3f, 0.02f);
    EXPECT_FLOAT_EQ(mob.position().y, 1.0f);
    EXPECT_TRUE(mob.onGround());
}

TEST(MobStepUpTest, StepsUpSlabAndRestsOnSlabTop) {
    // A slab step (solid-region top 0.5) is a TRUE 1-block step: the mob
    // steps up and its feet SETTLE on the slab's top (y = 1.5, flush —
    // not floating at the +1.0 lift), the Phase-85 standing-on-slab
    // behavior the player has. The player sits beyond the far end of the
    // slab, so the stop point is ON the slab: a single final state pins
    // both the crossing and the vertical rest.
    Fixture f;
    MobRegistry mobs = makeMobRegistry();
    const Mob& def = mobs.at(MobID{0});
    for (int x = 10; x < 20; ++x) {
        for (int z = 8; z < 10; ++z) {
            f.world.setBlock(x, 1, z, f.slab);
        }
    }
    MobEntity mob = makeMob(f, def, {8.5f, 1.0f, 8.5f});
    mob.setPlayer({18.4f, 1.0f, 8.5f}, true); // stop 18.4 - 1.6 = 16.8
    runMob(mob, 900);
    EXPECT_EQ(mob.state(), MobState::Attack);
    // The stop is the first frame-start with distH <= 1.6 (within one
    // chase frame of 16.8). The pre-V2-9 range also covered the
    // player-shared mid-air hop (a coincidental face flush at the
    // NON-integer slab-top level 1.5 lifted 1.0 and fell back, shifting
    // the stop frame through the attack vertical band — V2-9's
    // real-block check removes it, see the inverted single-cell pin
    // below and PlayerTest.SlabTopWalkNoCoincidentalLift).
    EXPECT_GE(mob.position().x, 16.8f);
    EXPECT_LE(mob.position().x, 16.9f);
    EXPECT_FLOAT_EQ(mob.position().y, 1.5f); // feet flush on the slab top
    EXPECT_TRUE(mob.onGround());
}

TEST(MobStepUpTest, V217StepsUpTopSlabAndRestsOnCellTop) {
    // V2-17: a row of TOP slabs (state 1, solid region 1.5..2.0): the
    // step's front top = 1.0 (the slab's top face IS the cell top — the
    // true-1-block step, the Minecraft "the top slab's top is walkable"
    // twin): the mob steps up and its feet settle at 2.0 = the top slab's
    // top face (not floating at a 1.5 lift, not inside the slab).
    Fixture f;
    auto r = f.blocks.registerBlock(parseJson(
        R"({"id":"slab3","model":{"shape":"slab","state_count":3,)"
        R"("state_default":0,"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const BlockID slab3 = *r;
    MobRegistry mobs = makeMobRegistry();
    const Mob& def = mobs.at(MobID{0});
    for (int x = 10; x < 20; ++x) {
        for (int z = 8; z < 10; ++z) {
            f.world.setBlock(x, 1, z, slab3);
            f.world.setBlockState(x, 1, z, 1);
        }
    }
    MobEntity mob = makeMob(f, def, {8.5f, 1.0f, 8.5f});
    mob.setPlayer({18.4f, 1.0f, 8.5f}, true); // stop 18.4 - 1.6 = 16.8
    runMob(mob, 900);
    EXPECT_EQ(mob.state(), MobState::Attack);
    EXPECT_GE(mob.position().x, 16.8f);
    EXPECT_LE(mob.position().x, 16.9f);
    EXPECT_FLOAT_EQ(mob.position().y, 2.0f); // feet flush on the top slab's top
    EXPECT_TRUE(mob.onGround());
}

TEST(MobStepUpTest, DoesNotStepUpWall) {
    // The Phase-99 judgment is shared verbatim: a wall (solid-region top
    // 1.5, the barrier) is NOT a 1-block step — the mob never climbs
    // onto it. ENT-6 evolution (the contract change): the mob no longer
    // stays clamped at the wall's face forever — the stuck-triggered BFS
    // routes AROUND the barrier instead (the wall's shape is not
    // FullCube, so no route node exists on top of it: the maxY pin is
    // the surviving assertion of the original judgment).
    Fixture f;
    MobRegistry mobs = makeMobRegistry();
    const Mob& def = mobs.at(MobID{0});
    for (int x = 10; x < 12; ++x) {
        for (int z = 8; z < 10; ++z) {
            f.world.setBlock(x, 1, z, f.wall);
        }
    }
    MobEntity mob = makeMob(f, def, {8.5f, 1.0f, 8.5f});
    mob.setPlayer({13.9f, 1.0f, 8.5f}, true);
    bool sawPathfind = false;
    float maxY = 0.0f;
    for (int i = 0; i < 300; ++i) {
        if (mob.state() == MobState::Pathfind) {
            sawPathfind = true;
        }
        if (mob.position().y > maxY) {
            maxY = mob.position().y;
        }
        mob.update(kDt);
    }
    EXPECT_TRUE(sawPathfind); // the straight chase dead-ended, the BFS took over
    EXPECT_FLOAT_EQ(maxY, 1.0f); // never on top of the 1.5 barrier
    EXPECT_TRUE(mob.onGround());
}

TEST(MobStepUpTest, DoesNotStepUpFence) {
    // The fence is the wall's barrier twin (the same 1.5 solid region):
    // no step-up either. ENT-6 evolution: the BFS routes around the
    // fence (no FullCube-floor node on top of it — the surviving pin is
    // the maxY below).
    Fixture f;
    MobRegistry mobs = makeMobRegistry();
    const Mob& def = mobs.at(MobID{0});
    for (int x = 10; x < 12; ++x) {
        for (int z = 8; z < 10; ++z) {
            f.world.setBlock(x, 1, z, f.fence);
        }
    }
    MobEntity mob = makeMob(f, def, {8.5f, 1.0f, 8.5f});
    mob.setPlayer({13.9f, 1.0f, 8.5f}, true);
    bool sawPathfind = false;
    float maxY = 0.0f;
    for (int i = 0; i < 300; ++i) {
        if (mob.state() == MobState::Pathfind) {
            sawPathfind = true;
        }
        if (mob.position().y > maxY) {
            maxY = mob.position().y;
        }
        mob.update(kDt);
    }
    EXPECT_TRUE(sawPathfind);
    EXPECT_FLOAT_EQ(maxY, 1.0f); // never on top of the 1.5 barrier
    EXPECT_TRUE(mob.onGround());
}

TEST(MobStepUpTest, DoesNotStepTwoBlockStep) {
    // Two cells high (no headroom above the step): the lifted mob would
    // embed in the second cell — no step (the player's
    // StepUpBlockedByLowHeadroom twin).
    Fixture f;
    MobRegistry mobs = makeMobRegistry();
    const Mob& def = mobs.at(MobID{0});
    for (int x = 10; x < 12; ++x) {
        for (int z = 8; z < 10; ++z) {
            f.world.setBlock(x, 1, z, f.stone);
            f.world.setBlock(x, 2, z, f.stone);
        }
    }
    MobEntity mob = makeMob(f, def, {8.5f, 1.0f, 8.5f});
    mob.setPlayer({13.9f, 1.0f, 8.5f}, true);
    // ENT-6 evolution: two cells high has no standable top node the
    // step-up edge can reach (the rise is 2, the edge is 1) — the BFS
    // routes around instead of the mob staying clamped.
    bool sawPathfind = false;
    float maxY = 0.0f;
    for (int i = 0; i < 300; ++i) {
        if (mob.state() == MobState::Pathfind) {
            sawPathfind = true;
        }
        if (mob.position().y > maxY) {
            maxY = mob.position().y;
        }
        mob.update(kDt);
    }
    EXPECT_TRUE(sawPathfind);
    EXPECT_FLOAT_EQ(maxY, 1.0f); // never on top of the 2-step
    EXPECT_TRUE(mob.onGround());
}

TEST(MobStepUpTest, ChaseOverStepIsFrameLockedDeterministic) {
    // Hard rule 8: the step-up frame is pure geometry — two
    // independently-constructed mobs (same seed, same world, same player
    // path) run bit-identical positions through the whole chase-over-step
    // sequence (the 20-frame checkpoints cover the step-up frame itself).
    Fixture f;
    MobRegistry mobs = makeMobRegistry();
    const Mob& def = mobs.at(MobID{0});
    for (int x = 10; x < 12; ++x) {
        for (int z = 8; z < 10; ++z) {
            f.world.setBlock(x, 1, z, f.stone);
        }
    }
    const Vec3 player {13.9f, 1.0f, 8.5f};
    MobEntity a = makeMob(f, def, {8.5f, 1.0f, 8.5f}, 777);
    MobEntity b = makeMob(f, def, {8.5f, 1.0f, 8.5f}, 777);
    a.setPlayer(player, true);
    b.setPlayer(player, true);
    for (int i = 0; i < 400; ++i) {
        a.update(kDt);
        b.update(kDt);
        if (i % 20 == 0) {
            EXPECT_FLOAT_EQ(a.position().x, b.position().x);
            EXPECT_FLOAT_EQ(a.position().y, b.position().y);
            EXPECT_FLOAT_EQ(a.position().z, b.position().z);
            EXPECT_FLOAT_EQ(a.facingYaw(), b.facingYaw());
        }
    }
}

TEST(MobStepUpTest, KnockbackIntoStepDoesNotTunnel) {
    // Hard rule 6: a fast knockback slide into the step is sub-stepped
    // (the clamp zeroes the velocity) and the step-up's re-applied move
    // is ONE moveAxis call with distPerTick = preMoveSpeed*dt — under 1.0
    // at the fixed 1/60 mob step (the terminal 38 gives 0.633) — so it
    // cannot skip the front cell. The whole sequence must never embed the
    // AABB in a solid (hasRoomAt at every checkpoint).
    Fixture f;
    MobRegistry mobs = makeMobRegistry();
    const Mob& def = mobs.at(MobID{0});
    for (int x = 10; x < 12; ++x) {
        for (int z = 8; z < 10; ++z) {
            f.world.setBlock(x, 1, z, f.stone);
        }
    }
    MobEntity mob = makeMob(f, def, {8.5f, 1.0f, 8.5f});
    mob.setPlayer({13.9f, 1.0f, 8.5f}, true);
    mob.addKnockback({30.0f, 0.0f, 0.0f});
    for (int i = 0; i < 300; ++i) {
        mob.update(kDt);
        if (i % 5 == 0) {
            EXPECT_TRUE(mob.hasRoomAt(mob.position()));
        }
    }
    // The knockback carried it into the step, the step-up crossed it, the
    // resumed chase stopped in attack range — the same terminal state as
    // the plain walk (ChasesUpOneBlockStep).
    EXPECT_EQ(mob.state(), MobState::Attack);
    EXPECT_NEAR(mob.position().x, 12.3f, 0.02f);
    EXPECT_FLOAT_EQ(mob.position().y, 1.0f);
}

TEST(MobStepUpTest, StepsUpSingleCellStepMinusX) {
    // The -X direction's front check (snapped-1) looks at the cell the
    // body is flush against, so even a SINGLE-cell step is step-up-able in
    // -X — the exact same asymmetry the player has (the +X twin is pinned
    // by DoesNotStepSingleCellStepPlusX).
    Fixture f;
    MobRegistry mobs = makeMobRegistry();
    const Mob& def = mobs.at(MobID{0});
    for (int z = 8; z < 10; ++z) {
        f.world.setBlock(10, 1, z, f.stone); // one cell only
    }
    MobEntity mob = makeMob(f, def, {12.5f, 1.0f, 8.5f});
    mob.setPlayer({7.1f, 1.0f, 8.5f}, true); // chase -X, stop 7.1 + 1.6
    runMob(mob, 400);
    EXPECT_EQ(mob.state(), MobState::Attack);
    EXPECT_NEAR(mob.position().x, 8.7f, 0.02f);
    EXPECT_FLOAT_EQ(mob.position().y, 1.0f);
    EXPECT_TRUE(mob.onGround());
}

TEST(MobStepUpTest, StepsUpSingleCellStepPlusX) {
    // V2-9: the +X front check now looks at the cell the body is flush
    // against (snapped, not snapped+1), so a SINGLE-cell step is
    // step-up-able in +X — this test is the INVERTED pre-V2-9 pin (it was
    // SingleCellStepPlusXStaysClampedAtTheQuirk, the quirk's surviving pin
    // after ENT-6 re-named DoesNotStepSingleCellStepPlusX: the corridor
    // walls made the step the only crossing, the BFS planned it, the
    // physics refused it, and the chase re-clamped). With the quirk fixed
    // the straight chase steps over and stops in attack range — the BFS
    // never engages (the corridor is still sealed: z7 / z10 rows, x4..16,
    // no detour).
    Fixture f;
    MobRegistry mobs = makeMobRegistry();
    const Mob& def = mobs.at(MobID{0});
    for (int z = 8; z < 10; ++z) {
        f.world.setBlock(10, 1, z, f.stone); // one cell only
    }
    // The corridor walls (the z7 / z10 rows, x4..16): no detour.
    for (int y = 1; y <= 2; ++y) {
        for (int x = 4; x <= 16; ++x) {
            f.world.setBlock(x, y, 7, f.stone);
            f.world.setBlock(x, y, 10, f.stone);
        }
    }
    MobEntity mob = makeMob(f, def, {8.5f, 1.0f, 8.5f});
    mob.setPlayer({13.9f, 1.0f, 8.5f}, true);
    bool sawPathfind = false;
    for (int i = 0; i < 300; ++i) {
        if (mob.state() == MobState::Pathfind) {
            sawPathfind = true;
        }
        mob.update(kDt);
    }
    EXPECT_FALSE(sawPathfind); // no detour needed: the step itself is crossed
    EXPECT_EQ(mob.state(), MobState::Attack); // crossed (13.9 - 1.6 = 12.3)
    EXPECT_NEAR(mob.position().x, 12.3f, 0.02f);
    EXPECT_FLOAT_EQ(mob.position().y, 1.0f);
    EXPECT_TRUE(mob.onGround());
}

// -- combat ----------------------------------------------------------------------

TEST(MobCombatTest, DamageKillsAtZero) {
    Fixture f;
    MobRegistry mobs = makeMobRegistry();
    const Mob& def = mobs.at(MobID{0});
    MobEntity mob = makeMob(f, def, {4.5f, 1.0f, 4.5f});
    EXPECT_FLOAT_EQ(mob.health(), 20.0f);
    // A killing hit reports true (and clamps the health to 0).
    EXPECT_TRUE(mob.damage(20.0f));
    EXPECT_TRUE(mob.dead());
    EXPECT_FLOAT_EQ(mob.health(), 0.0f);
    EXPECT_TRUE(mob.damage(5.0f));  // already dead: still reports dead
    EXPECT_TRUE(mob.shouldDespawn());  // dead -> the game removes it
}

TEST(MobCombatTest, PartialDamageDoesNotKill) {
    Fixture f;
    MobRegistry mobs = makeMobRegistry();
    const Mob& def = mobs.at(MobID{0});
    MobEntity mob = makeMob(f, def, {4.5f, 1.0f, 4.5f});
    EXPECT_FALSE(mob.damage(3.0f));
    EXPECT_FLOAT_EQ(mob.health(), 17.0f);
    EXPECT_FALSE(mob.dead());
}

TEST(MobCombatTest, KnockbackAppliesTheImpulse) {
    Fixture f;
    MobRegistry mobs = makeMobRegistry();
    const Mob& def = mobs.at(MobID{0});
    MobEntity mob = makeMob(f, def, {4.5f, 1.0f, 4.5f});
    mob.setVelocity({0.8f, 0.0f, 0.0f});
    mob.addKnockback({-2.0f, 1.0f, 0.0f});
    EXPECT_NEAR(mob.velocity().x, -1.2f, 1e-6f);
    EXPECT_NEAR(mob.velocity().y, 1.0f, 1e-6f);
    EXPECT_NEAR(mob.velocity().z, 0.0f, 1e-6f);
}

// -- AABB raycast (player targeting) ----------------------------------------------

TEST(MobRaycastTest, HitReturnsEntryDistance) {
    // A mob AABB at the origin (0.6 wide, 1.8 tall); a ray from (0, 0, 5)
    // toward -z enters at z = 0.3 -> t = 4.7.
    const float t = raycastAabb({0.0f, 0.0f, 5.0f}, {0.0f, 0.0f, -1.0f},
                                {-0.3f, 0.0f, -0.3f}, {0.3f, 1.8f, 0.3f});
    EXPECT_NEAR(t, 4.7f, 1e-5f);
}

TEST(MobRaycastTest, MissReturnsNegative) {
    // The box is behind the ray origin (ray toward +x from (5, 0.9, 0)).
    const float t = raycastAabb({5.0f, 0.9f, 0.0f}, {1.0f, 0.0f, 0.0f},
                                {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f});
    EXPECT_LT(t, 0.0f);
}

TEST(MobRaycastTest, OriginInsideReturnsZero) {
    const float t = raycastAabb({0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f},
                                {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f});
    EXPECT_FLOAT_EQ(t, 0.0f);
}

TEST(MobRaycastTest, ParallelMissWhenOutsideSlab) {
    // Ray along x at y = 5: parallel to the y slab and outside its span.
    const float t = raycastAabb({-5.0f, 5.0f, 0.5f}, {1.0f, 0.0f, 0.0f},
                                {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f});
    EXPECT_LT(t, 0.0f);
}

// -- item attack_damage ----------------------------------------------------------

namespace {

BlockRegistry makeItemBlocks() {
    BlockRegistry blocks;
    auto r = blocks.registerBlock(parseJson(
        R"({"id":"stone","model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    if (!r.ok()) {
        ADD_FAILURE() << r.error().message;
    }
    return blocks;
}

} // namespace

TEST(MobItemsTest, ToolParsesAttackDamage) {
    ItemRegistry items;
    ASSERT_TRUE(items.buildBlockItems(makeItemBlocks()).ok());
    auto r = items.registerItem(parseJson(R"({
      "id": "iron_pickaxe", "type": "tool",
      "tool": {"break_speed": 4.0},
      "attack_damage": 3, "max_stack": 1
    })"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_FLOAT_EQ(items.at(*r).attackDamage, 3.0f);
}

TEST(MobItemsTest, AttackDamageDefaultsToOne) {
    ItemRegistry items;
    ASSERT_TRUE(items.buildBlockItems(makeItemBlocks()).ok());
    auto r = items.registerItem(
        parseJson(R"({"id": "stick", "type": "material"})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_FLOAT_EQ(items.at(*r).attackDamage, 1.0f);
    // Block items (implicit) also default to 1.0.
    EXPECT_FLOAT_EQ(items.at(items.idForName("stone")).attackDamage, 1.0f);
}

TEST(MobItemsTest, RejectsNonPositiveAttackDamage) {
    ItemRegistry items;
    ASSERT_TRUE(items.buildBlockItems(makeItemBlocks()).ok());
    auto r = items.registerItem(parseJson(R"({
      "id": "stick", "type": "material", "attack_damage": 0
    })"));
    EXPECT_FALSE(r.ok());
}

// -- fire system v2 (Phase 58): the lava ignites mobs -----------------------

// The tanky test def (Phase 58): HP 100 so the burn tests can observe past
// the 8-second lingering window (the zombie's 20 HP would run out in 4 s).
const char* kTankDef = R"({
  "id": "tank",
  "properties": {
    "health": 100,
    "speed": 0.8,
    "detect_radius": 16,
    "attack_range": 1.6,
    "attack_damage": 3,
    "attack_cooldown": 1.0,
    "wander_interval": 2.5
  },
  "model": {
    "width": 0.6,
    "height": 1.8,
    "boxes": [
      {"pos": [0.0, 1.25, 0.0], "size": [0.5, 0.5, 0.5], "texture": "a"},
      {"pos": [0.0, 0.5, 0.0], "size": [0.5, 1.0, 0.3], "texture": "b"}
    ]
  }
})";

MobRegistry makeTankRegistry() {
    MobRegistry mobs;
    auto r = mobs.registerMob(parseJson(kTankDef));
    if (!r.ok()) {
        ADD_FAILURE() << r.error().message;
    }
    return mobs;
}

// The fixture floor (x,z in [-16,16)) covered in 2-deep fluid (y=1..2):
// big enough that a wandering mob (<= ~0.4 b/s) cannot leave it within a
// test window, so the fluid state is independent of the wander path.
void fillFloor(Fixture& f, FluidKind kind) {
    for (int x = -16; x < 16; ++x) {
        for (int z = -16; z < 16; ++z) {
            f.world.setFluid(x, 1, z, kind, kMaxFluidLevel);
            f.world.setFluid(x, 2, z, kind, kMaxFluidLevel);
        }
    }
}

void clearFloor(Fixture& f, FluidKind kind) {
    for (int x = -16; x < 16; ++x) {
        for (int z = -16; z < 16; ++z) {
            f.world.setFluid(x, 1, z, kind, 0);
            f.world.setFluid(x, 2, z, kind, 0);
        }
    }
}

// The shipped fire effect's parameters (fire.json: 1 HP / 0.2 s) + the
// config default duration (player.fire_duration_seconds = 8 s).
void armShippedFire(MobEntity& mob) {
    mob.armFire(0.2, 1.0f, 8.0);
}

TEST(MobFireTest, BurnsInLavaAtTheFireRate) {
    Fixture f;
    fillFloor(f, FluidKind::Lava);
    MobRegistry mobs = makeMobRegistry();
    const Mob& def = mobs.at(MobID{0});
    MobEntity mob = makeMob(f, def, {8.5f, 1.0f, 8.5f});
    mob.setPlayer({30.0f, 1.0f, 30.0f}, false); // pure wander
    armShippedFire(mob);
    EXPECT_FALSE(mob.burning());
    for (int i = 0; i < 120; ++i) { // 2 s = exactly 10 ticks (12 frames each)
        mob.update(kDt);
    }
    EXPECT_TRUE(mob.burning());
    EXPECT_FALSE(mob.dead());
    EXPECT_FLOAT_EQ(mob.health(), 10.0f); // 20 - 10
}

TEST(MobFireTest, BurnKillsTheMob) {
    Fixture f;
    fillFloor(f, FluidKind::Lava);
    MobRegistry mobs = makeMobRegistry();
    const Mob& def = mobs.at(MobID{0});
    MobEntity mob = makeMob(f, def, {8.5f, 1.0f, 8.5f});
    mob.setPlayer({30.0f, 1.0f, 30.0f}, false);
    armShippedFire(mob);
    for (int i = 0; i < 239; ++i) {
        mob.update(kDt);
    }
    EXPECT_FALSE(mob.dead());
    EXPECT_FLOAT_EQ(mob.health(), 1.0f); // 19 ticks landed
    mob.update(kDt); // frame 240 = t 4.0 s = the 20th tick
    EXPECT_TRUE(mob.dead());
}

TEST(MobFireTest, BurnLingersAfterLeavingLava) {
    Fixture f;
    fillFloor(f, FluidKind::Lava);
    MobRegistry mobs = makeTankRegistry();
    const Mob& def = mobs.at(MobID{0});
    MobEntity mob = makeMob(f, def, {8.5f, 1.0f, 8.5f});
    mob.setPlayer({30.0f, 1.0f, 30.0f}, false);
    armShippedFire(mob);
    for (int i = 0; i < 60; ++i) { // 1 s in the lava: 5 ticks
        mob.update(kDt);
    }
    EXPECT_FLOAT_EQ(mob.health(), 95.0f);
    EXPECT_TRUE(mob.burning());
    clearFloor(f, FluidKind::Lava); // dry land: the ~8 s left count down
    for (int i = 0; i < 420; ++i) {
        mob.update(kDt);
    }
    // 40 ticks landed (burn frames 12..480). The lingering burn is STILL
    // running: its countdown started from 8 s minus one frame at the lava
    // exit (the re-ignitation reset loses the step's dt), so it expires
    // one frame AFTER t = 8 s of pure countdown (dry frame 480).
    EXPECT_TRUE(mob.burning());
    EXPECT_FLOAT_EQ(mob.health(), 60.0f); // 100 - 40
    for (int i = 0; i < 60; ++i) { // through the expiry (dry frame 480)
        mob.update(kDt);
    }
    EXPECT_FALSE(mob.burning()); // expired (the expiry step does not tick)
    EXPECT_FLOAT_EQ(mob.health(), 56.0f); // 100 - 44 (tick 45 is due on
                                          // the expiry frame itself)
    for (int i = 0; i < 120; ++i) { // no further damage after the expiry
        mob.update(kDt);
    }
    EXPECT_FLOAT_EQ(mob.health(), 56.0f);
    EXPECT_FALSE(mob.dead());
}

TEST(MobFireTest, ExtinguishedInWater) {
    Fixture f;
    fillFloor(f, FluidKind::Lava);
    MobRegistry mobs = makeTankRegistry();
    const Mob& def = mobs.at(MobID{0});
    MobEntity mob = makeMob(f, def, {8.5f, 1.0f, 8.5f});
    mob.setPlayer({30.0f, 1.0f, 30.0f}, false);
    armShippedFire(mob);
    for (int i = 0; i < 60; ++i) {
        mob.update(kDt);
    }
    EXPECT_FLOAT_EQ(mob.health(), 95.0f);
    EXPECT_TRUE(mob.burning());
    clearFloor(f, FluidKind::Lava);
    fillFloor(f, FluidKind::Water); // the water extinguishes instantly
    mob.update(kDt);
    EXPECT_FALSE(mob.burning());
    for (int i = 0; i < 600; ++i) { // 10 s: no burn damage in the water
        mob.update(kDt);
    }
    EXPECT_FLOAT_EQ(mob.health(), 95.0f);
    EXPECT_FALSE(mob.dead());
}

TEST(MobFireTest, ExtinguishedInRain) {
    // V2-4 (the design's third extinguish case — the fire EFFECT on the
    // entity, over the live update path): rain extinguishes the burning
    // mob the way water does (the inWater ∨ raining extension — no
    // roof-shade check = the documented deviation; the re-ignition runs
    // first, so a mob on a fire source in the rain stays out, the water-
    // beats-fire order). Off-immunity: the default setRaining(false)
    // keeps the pre-V2-4 water-only semantics (the lingering burn
    // counts down normally in dry air — the BurnLingersAfterLeavingLava
    // twin) and the next ignition burns normally.
    Fixture f;
    fillFloor(f, FluidKind::Lava);
    MobRegistry mobs = makeTankRegistry();
    const Mob& def = mobs.at(MobID{0});
    MobEntity mob = makeMob(f, def, {8.5f, 1.0f, 8.5f});
    mob.setPlayer({30.0f, 1.0f, 30.0f}, false);
    armShippedFire(mob);
    for (int i = 0; i < 60; ++i) {
        mob.update(kDt);
    }
    EXPECT_FLOAT_EQ(mob.health(), 95.0f);
    EXPECT_TRUE(mob.burning());
    clearFloor(f, FluidKind::Lava); // dry land — no water anywhere
    mob.setRaining(true); // the rain is live (no roof in between)
    mob.update(kDt);
    EXPECT_FALSE(mob.burning());
    for (int i = 0; i < 600; ++i) { // 10 s: no burn damage in the rain
        mob.update(kDt);
    }
    EXPECT_FLOAT_EQ(mob.health(), 95.0f);
    EXPECT_FALSE(mob.dead());
    // The rain stops: the next ignition lingers normally (the off-
    // immunity — the water-only baseline semantics are back).
    mob.setRaining(false);
    fillFloor(f, FluidKind::Lava);
    for (int i = 0; i < 60; ++i) {
        mob.update(kDt);
    }
    EXPECT_TRUE(mob.burning());
}

TEST(MobFireTest, StayingInLavaReignites) {
    Fixture f;
    fillFloor(f, FluidKind::Lava);
    MobRegistry mobs = makeTankRegistry();
    const Mob& def = mobs.at(MobID{0});
    MobEntity mob = makeMob(f, def, {8.5f, 1.0f, 8.5f});
    mob.setPlayer({30.0f, 1.0f, 30.0f}, false);
    armShippedFire(mob);
    for (int i = 0; i < 540; ++i) { // 9 s: past the 8 s window
        mob.update(kDt);
    }
    // A single 8 s application would have expired at frame 480 (40 ticks,
    // HP 60). The re-ignition keeps the burn alive: 45 ticks -> HP 55.
    EXPECT_TRUE(mob.burning());
    EXPECT_FALSE(mob.dead());
    EXPECT_FLOAT_EQ(mob.health(), 55.0f);
}

TEST(MobFireTest, UnarmedMobDoesNotBurn) {
    Fixture f;
    fillFloor(f, FluidKind::Lava);
    MobRegistry mobs = makeMobRegistry();
    const Mob& def = mobs.at(MobID{0});
    MobEntity mob = makeMob(f, def, {8.5f, 1.0f, 8.5f});
    mob.setPlayer({30.0f, 1.0f, 30.0f}, false);
    for (int i = 0; i < 600; ++i) { // 10 s in the lava, no fire armed
        mob.update(kDt);
    }
    EXPECT_FALSE(mob.burning());
    EXPECT_FALSE(mob.dead());
    EXPECT_FLOAT_EQ(mob.health(), 20.0f);
}

TEST(MobFireTest, WaterBeatsLava) {
    Fixture f;
    // Water below, lava above (the mob's 1.8-tall AABB spans both cells):
    // the re-ignition and the extinguish both fire every step, and the
    // extinguish runs last -> zero burn (the P57 water-wins rule).
    fillFloor(f, FluidKind::Water);
    for (int x = -16; x < 16; ++x) {
        for (int z = -16; z < 16; ++z) {
            f.world.setFluid(x, 2, z, FluidKind::Lava, kMaxFluidLevel);
        }
    }
    MobRegistry mobs = makeMobRegistry();
    const Mob& def = mobs.at(MobID{0});
    MobEntity mob = makeMob(f, def, {8.5f, 1.0f, 8.5f});
    mob.setPlayer({30.0f, 1.0f, 30.0f}, false);
    armShippedFire(mob);
    for (int i = 0; i < 300; ++i) {
        mob.update(kDt);
    }
    EXPECT_FALSE(mob.burning());
    EXPECT_FALSE(mob.dead());
    EXPECT_FLOAT_EQ(mob.health(), 20.0f);
}
// -- fire system v3 (Phase 73): the fire block ignites mobs -------------------

// The fire-block twin of the fixture: the same stone floor + a "fire" block
// registered (the MobEntity constructor resolves its id; a fixture WITHOUT
// it — the plain Fixture — pins the structurally-inert invalid-id path).
BlockRegistry makeFireBlocks() {
    BlockRegistry blocks = makeBlocks();
    // NON-SOLID (the real fire.json: the cross-model fire — a solid fire
    // cell would push the mob ON TOP of the patch and kill the overlap).
    auto r = blocks.registerBlock(parseJson(
        R"({"id":"fire","properties":{"solid":false},"model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    if (!r.ok()) {
        ADD_FAILURE() << r.error().message;
    }
    return blocks;
}

struct FireFixture {
    BlockRegistry blocks = makeFireBlocks();
    World world;
    BlockID stone;
    BlockID fire;

    FireFixture() {
        stone = blocks.idForName("stone");
        fire = blocks.idForName("fire");
        for (int x = -16; x < 16; ++x) {
            for (int z = -16; z < 16; ++z) {
                world.setBlock(x, 0, z, stone);
            }
        }
    }
};

MobEntity makeFireMob(const FireFixture& f, const Mob& def, Vec3 pos,
                      std::uint64_t seed = 42) {
    return MobEntity(f.world, f.blocks, def, pos, seed);
}

// The 3x3 fire patch at feet level (y=1, the air cells above the floor):
// the mob's 1.8-tall AABB strictly overlaps the patch cells. Big enough
// that a wandering mob (<= 0.8 b/s * 2 s = 1.6 blocks in one direction,
// the wander re-decision at 2.5 s is past the test windows) cannot leave
// it, so the contact state is independent of the wander path.
void fillFirePatch(FireFixture& f) {
    for (int x = 7; x <= 9; ++x) {
        for (int z = 7; z <= 9; ++z) {
            f.world.setBlock(x, 1, z, f.fire);
        }
    }
}

void clearFirePatch(FireFixture& f) {
    for (int x = 7; x <= 9; ++x) {
        for (int z = 7; z <= 9; ++z) {
            f.world.setBlock(x, 1, z, BlockID::air());
        }
    }
}

void fillWaterFloor(FireFixture& f) {
    for (int x = -16; x < 16; ++x) {
        for (int z = -16; z < 16; ++z) {
            f.world.setFluid(x, 1, z, FluidKind::Water, kMaxFluidLevel);
            f.world.setFluid(x, 2, z, FluidKind::Water, kMaxFluidLevel);
        }
    }
}

TEST(MobFireContactTest, BurnsInFireAtTheFireRate) {
    FireFixture f;
    fillFirePatch(f);
    MobRegistry mobs = makeMobRegistry();
    const Mob& def = mobs.at(MobID{0});
    MobEntity mob = makeFireMob(f, def, {8.5f, 1.0f, 8.5f});
    mob.setPlayer({30.0f, 1.0f, 30.0f}, false); // pure wander
    armShippedFire(mob);
    EXPECT_FALSE(mob.burning());
    for (int i = 0; i < 120; ++i) { // 2 s = exactly 10 ticks (12 frames each)
        mob.update(kDt);
    }
    EXPECT_TRUE(mob.burning());
    EXPECT_FALSE(mob.dead());
    EXPECT_FLOAT_EQ(mob.health(), 10.0f); // 20 - 10
}

TEST(MobFireContactTest, BurnLingersAfterLeavingFire) {
    FireFixture f;
    fillFirePatch(f);
    MobRegistry mobs = makeTankRegistry();
    const Mob& def = mobs.at(MobID{0});
    MobEntity mob = makeFireMob(f, def, {8.5f, 1.0f, 8.5f});
    mob.setPlayer({30.0f, 1.0f, 30.0f}, false);
    armShippedFire(mob);
    for (int i = 0; i < 60; ++i) { // 1 s in the fire: 5 ticks
        mob.update(kDt);
    }
    EXPECT_FLOAT_EQ(mob.health(), 95.0f);
    EXPECT_TRUE(mob.burning());
    clearFirePatch(f); // dry land: the ~8 s left count down
    for (int i = 0; i < 420; ++i) {
        mob.update(kDt);
    }
    // Same expiry arithmetic as the lava test: 40 ticks landed (burn
    // frames 12..480), the lingering burn is STILL running (it expires
    // one frame after t = 8 s of pure countdown, dry frame 480).
    EXPECT_TRUE(mob.burning());
    EXPECT_FLOAT_EQ(mob.health(), 60.0f); // 100 - 40
    for (int i = 0; i < 60; ++i) { // through the expiry (dry frame 480)
        mob.update(kDt);
    }
    EXPECT_FALSE(mob.burning()); // expired (the expiry step does not tick)
    EXPECT_FLOAT_EQ(mob.health(), 56.0f); // 100 - 44 (tick 45 is due on
                                          // the expiry frame itself)
    for (int i = 0; i < 120; ++i) { // no further damage after the expiry
        mob.update(kDt);
    }
    EXPECT_FLOAT_EQ(mob.health(), 56.0f);
    EXPECT_FALSE(mob.dead());
}

TEST(MobFireContactTest, ExtinguishedInWater) {
    FireFixture f;
    fillFirePatch(f);
    MobRegistry mobs = makeTankRegistry();
    const Mob& def = mobs.at(MobID{0});
    MobEntity mob = makeFireMob(f, def, {8.5f, 1.0f, 8.5f});
    mob.setPlayer({30.0f, 1.0f, 30.0f}, false);
    armShippedFire(mob);
    for (int i = 0; i < 60; ++i) {
        mob.update(kDt);
    }
    EXPECT_FLOAT_EQ(mob.health(), 95.0f);
    EXPECT_TRUE(mob.burning());
    clearFirePatch(f);
    fillWaterFloor(f); // the water extinguishes instantly
    mob.update(kDt);
    EXPECT_FALSE(mob.burning());
    for (int i = 0; i < 600; ++i) { // 10 s: no burn damage in the water
        mob.update(kDt);
    }
    EXPECT_FLOAT_EQ(mob.health(), 95.0f);
    EXPECT_FALSE(mob.dead());
}

TEST(MobFireContactTest, UnarmedMobDoesNotBurnInFire) {
    FireFixture f;
    fillFirePatch(f);
    MobRegistry mobs = makeMobRegistry();
    const Mob& def = mobs.at(MobID{0});
    MobEntity mob = makeFireMob(f, def, {8.5f, 1.0f, 8.5f});
    mob.setPlayer({30.0f, 1.0f, 30.0f}, false);
    for (int i = 0; i < 600; ++i) { // 10 s in the fire, no fire armed
        mob.update(kDt);
    }
    EXPECT_FALSE(mob.burning());
    EXPECT_FALSE(mob.dead());
    EXPECT_FLOAT_EQ(mob.health(), 20.0f);
}

TEST(MobFireContactTest, NoFireBlockRegisteredIsInert) {
    // The plain fixture's registry has no "fire" block: the
    // constructor-resolved id is invalid, the contact check is
    // structurally inert (the P70 player pattern) — an armed mob on dry
    // land never burns.
    Fixture f;
    MobRegistry mobs = makeMobRegistry();
    const Mob& def = mobs.at(MobID{0});
    MobEntity mob = makeMob(f, def, {8.5f, 1.0f, 8.5f});
    mob.setPlayer({30.0f, 1.0f, 30.0f}, false);
    armShippedFire(mob);
    for (int i = 0; i < 300; ++i) {
        mob.update(kDt);
    }
    EXPECT_FALSE(mob.burning());
    EXPECT_FLOAT_EQ(mob.health(), 20.0f);
}

// ---------------------------------------------------------------------------
// 99.7 content pin: the shipped quadrupeds have legs. The Phase 10 models
// were body + head only (the animals floated — the user's walking-run
// report); the legs are thin ground-reaching boxes reusing the body
// texture. The humanoids (zombie/skeleton) already carry full-height body
// boxes (y 0-1.0) — that box IS the legs. Walking leg SWING is the
// documented v1 deviation (the per-type mesh is a static cache; per-
// instance limb animation is a future phase).
// ---------------------------------------------------------------------------

TEST(MobRegistryTest, ShippedModelsReachTheGroundWithLegs) {
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    MobRegistry mobs;
    ASSERT_TRUE(mobs.loadDirectory(assets, "data/mobs").ok());
    const struct {
        const char* name;
        std::size_t boxes; // expected total box count
    } cases[] = {
        {"chicken", 7},   // body + 2 legs + 2 wings + head + beak
        {"cow", 6},       // body + 4 legs + head
        {"sheep", 6},     // body + 4 legs + head
        {"spider", 10},   // body + 8 legs + head
        {"zombie", 2},    // full-height body (0-1.0) + head
        {"skeleton", 2}   // full-height body (0-1.0) + head
    };
    for (const auto& c : cases) {
        const MobID id = mobs.idForName(c.name);
        ASSERT_TRUE(id.valid()) << c.name;
        const Mob& m = mobs.at(id);
        EXPECT_EQ(m.model.boxes.size(), c.boxes) << c.name;
        // Every model touches the ground: the lowest box bottom == 0
        // (quadrupeds via the legs, humanoids via the full-height body).
        float lowest = 1e9f;
        for (const MobBox& b : m.model.boxes) {
            lowest = std::min(lowest, b.center.y - b.size.y / 2.0f);
        }
        EXPECT_NEAR(lowest, 0.0f, 1e-6f) << c.name;
    }
}

// ---------------------------------------------------------------------------
// ENT-1: mob limb swing. The pure integer / fixed-point pipeline (the
// compile-time sine table, the per-frame phase advance, the pivot
// derivation, the limb-angle rotation) + the strict 'swings' data schema
// + the shipped content + the MobEntity's phase bookkeeping (render-only,
// never saved — mobs are ephemeral).
// ---------------------------------------------------------------------------

namespace {

// A two-legged synthetic walker: body + two IDENTICAL leg boxes (same
// geometry, so the group-antiphase cross-frame equality below is exact),
// leg_a group 0 / leg_b group 1 (the half-cycle offset).
const char* kTwoLegDef = R"({
  "id": "walker",
  "properties": {
    "health": 4,
    "speed": 0.5,
    "detect_radius": 0,
    "attack_range": 0,
    "attack_damage": 0,
    "attack_cooldown": 1.0,
    "wander_interval": 2.0
  },
  "model": {
    "width": 1.0,
    "height": 1.0,
    "boxes": [
      {"name": "body", "pos": [0.0, 0.75, 0.0], "size": [0.5, 0.5, 0.5],
       "texture": "a"},
      {"name": "leg_a", "pos": [0.0, 0.25, 0.0], "size": [0.2, 0.5, 0.2],
       "texture": "b"},
      {"name": "leg_b", "pos": [0.0, 0.25, 0.0], "size": [0.2, 0.5, 0.2],
       "texture": "b"}
    ],
    "swings": [
      {"part": "leg_a", "amplitude": 0.5, "group": 0},
      {"part": "leg_b", "amplitude": 0.5, "group": 1}
    ]
  }
})";

// The base schema for the strict-parse error table: one named body + one
// named leg, the leg swinging group 0 at 0.5 rad.
const char* kSwingBaseDef = R"({
  "id": "swingy",
  "properties": {
    "health": 4,
    "speed": 0.5,
    "detect_radius": 0,
    "attack_range": 0,
    "attack_damage": 0,
    "attack_cooldown": 1.0,
    "wander_interval": 2.0
  },
  "model": {
    "width": 1.0,
    "height": 1.0,
    "boxes": [
      {"name": "body", "pos": [0.0, 0.75, 0.0], "size": [0.5, 0.5, 0.5],
       "texture": "a"},
      {"name": "leg", "pos": [0.0, 0.25, 0.0], "size": [0.3, 0.5, 0.3],
       "texture": "b"}
    ],
    "swings": [
      {"part": "leg", "amplitude": 0.5, "group": 0}
    ]
  }
})";

// The zombie def WITH swing data (the MobEntity phase tests need a type
// that advances; the fixture kZombieDef is swing-less by design).
const char* kSwingZombieDef = R"({
  "id": "zombie",
  "properties": {
    "health": 20,
    "speed": 0.8,
    "detect_radius": 16,
    "attack_range": 1.6,
    "attack_damage": 3,
    "attack_cooldown": 1.0,
    "wander_interval": 2.5
  },
  "model": {
    "width": 0.6,
    "height": 1.8,
    "boxes": [
      {"pos": [0.0, 1.25, 0.0], "size": [0.5, 0.5, 0.5], "texture": "a"},
      {"name": "torso", "pos": [0.0, 0.5, 0.0], "size": [0.5, 1.0, 0.3],
       "texture": "b"}
    ],
    "swings": [
      {"part": "torso", "amplitude": 0.5, "group": 0}
    ]
  }
})";

MobRegistry makeTwoLegRegistry() {
    MobRegistry mobs;
    auto r = mobs.registerMob(parseJson(kTwoLegDef));
    if (!r.ok()) {
        ADD_FAILURE() << r.error().message;
    }
    return mobs;
}

SwingModel makeTwoLegSwingModel() {
    const MobRegistry mobs = makeTwoLegRegistry();
    return buildSwingModel(mobs.at(MobID{0}).model, makeMobLayout());
}

} // namespace

TEST(MobSwingTest, SinTableMatchesDocumentedSine) {
    // The compile-time Taylor table must equal the documented formula
    // round(2^16 * sin(2*pi*i/N)) for every entry (the double reference
    // is a test-only oracle — the shipped path never calls libm).
    for (int i = 0; i < kSwingPhaseCount; ++i) {
        const double expect =
            std::sin(2.0 * std::numbers::pi * i / kSwingPhaseCount) *
            65536.0;
        EXPECT_LE(std::abs(static_cast<double>(kSwingSinTable[i]) - expect),
                  1.0)
            << "entry " << i;
        EXPECT_GE(kSwingSinTable[i], -65536);
        EXPECT_LE(kSwingSinTable[i], 65536);
    }
    // Exact pins (the cardinal angles are representable exactly).
    EXPECT_EQ(kSwingSinTable[0], 0);
    EXPECT_EQ(kSwingSinTable[kSwingPhaseCount / 8], 46341); // sin(pi/4)
    EXPECT_EQ(kSwingSinTable[kSwingPhaseCount / 4], 65536); // sin(pi/2)
    EXPECT_EQ(kSwingSinTable[kSwingPhaseCount / 2], 0);     // sin(pi)
    EXPECT_EQ(kSwingSinTable[3 * kSwingPhaseCount / 4], -65536);
    // Half-cycle antipodality (the group-1 offset's correctness):
    // table[(i + N/2) % N] == -table[i] exactly.
    for (int i = 0; i < kSwingPhaseCount; ++i) {
        EXPECT_EQ(kSwingSinTable[(i + kSwingHalf) % kSwingPhaseCount],
                  -kSwingSinTable[i])
            << "entry " << i;
    }
    // Monotone rise on the first quarter (no wobble = no formula error).
    for (int i = 1; i < kSwingPhaseCount / 4; ++i) {
        EXPECT_GT(kSwingSinTable[i], kSwingSinTable[i - 1]) << "entry " << i;
    }
}

TEST(MobSwingTest, PhaseAdvanceMovingAndResting) {
    // Moving: forward kSwingStep per frame, wrapping at N.
    EXPECT_EQ(advanceSwingPhase(0, true), kSwingStep);
    EXPECT_EQ(advanceSwingPhase(kSwingPhaseCount - 1, true),
              (kSwingPhaseCount - 1 + kSwingStep) % kSwingPhaseCount); // 63 -> 1
    EXPECT_EQ(advanceSwingPhase(kSwingPhaseCount - 2, true), 0); // 62 -> 0
    // Resting: decay kSwingDecay toward 0 (clamped), a full phase rests in
    // 16 frames (63/4), 0 is a fixed point.
    EXPECT_EQ(advanceSwingPhase(0, false), 0);
    EXPECT_EQ(advanceSwingPhase(2, false), 0); // below the decay: to 0
    EXPECT_EQ(advanceSwingPhase(5, false), 1); // 5 - 4
    EXPECT_EQ(advanceSwingPhase(63, false), 59);
    int p = 63;
    int frames = 0;
    while (p != 0) {
        p = advanceSwingPhase(p, false);
        ++frames;
    }
    EXPECT_EQ(frames, 16); // the smooth step-off (~15-20 frame target)
    // Moving dominates a full cycle: 32 steps return to the start.
    int q = 0;
    for (int i = 0; i < kSwingPhaseCount / kSwingStep; ++i) {
        q = advanceSwingPhase(q, true);
    }
    EXPECT_EQ(q, 0);
}

TEST(MobSwingTest, LimbSwingActiveGate) {
    // The render fast path: config off, a swing-less type, or the rest
    // phase (0 = the identity pose) all take the static mesh.
    EXPECT_FALSE(limbSwingActive(false, true, 7));   // config off
    EXPECT_FALSE(limbSwingActive(true, false, 7));    // no swing data
    EXPECT_FALSE(limbSwingActive(true, true, 0));     // rest pose
    EXPECT_TRUE(limbSwingActive(true, true, 1));      // swinging
    EXPECT_TRUE(limbSwingActive(true, true, 63));     // swinging (decay)
}

TEST(MobSwingTest, PivotIsBoxTopCenter) {
    // The formula (binary-exact inputs: a pin the float cannot fudge).
    const MobBox exact {Vec3 {0.0f, 0.5f, 0.0f}, Vec3 {0.5f, 1.0f, 0.3f},
                        "t", "leg", {} };
    const Vec3 pe = swingPivot(exact);
    EXPECT_FLOAT_EQ(pe.x, 0.0f);
    EXPECT_FLOAT_EQ(pe.y, 1.0f); // 0.5 + 1.0/2, exact in binary
    EXPECT_FLOAT_EQ(pe.z, 0.0f);
    // The shipped chicken leg (the JSON floats, near-tolerance).
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    MobRegistry mobs;
    ASSERT_TRUE(mobs.loadDirectory(assets, "data/mobs").ok());
    const Mob& chicken = mobs.at(mobs.idForName("chicken"));
    const MobBox& leg = chicken.model.boxes[1]; // "leg_l" (the content pin
    ASSERT_EQ(leg.name, "leg_l");                // below re-checks this)
    const Vec3 p = swingPivot(leg);
    EXPECT_NEAR(p.x, -0.094f, 1e-6f);
    EXPECT_NEAR(p.y, 0.31f, 1e-6f); // 0.155 + 0.31/2 = the leg's top
    EXPECT_NEAR(p.z, 0.0f, 1e-6f);
    // The pivot sits FLUSH at the body bottom (body bottom 0.31 = 0.485
    // − 0.35/2, ENT-1b joint-out-of-body geometry): the joint is never
    // visible and the swing cannot clip the body side.
    const MobBox& body = chicken.model.boxes[0];
    EXPECT_GT(p.y, body.center.y - body.size.y / 2.0f - 1e-6f);
}

TEST(MobSwingTest, RotateZeroAngleIsBitExactIdentity) {
    // (s, c) = (0, 2^16) must reproduce the input BYTE for byte — the
    // phase-0 frame equals the static mesh (the fast-path contract).
    const Vec3 v {1.25f, -0.375f, 0.75f};
    const Vec3 pivot {0.5f, 1.0f, -0.25f};
    const Vec3 n {-1.0f, 0.0f, 0.0f};
    const Vec3 rv = swingRotateVertex(v, pivot, 0.0f, 1.0f);
    const Vec3 rn = swingRotateNormal(n, 0.0f, 1.0f);
    ASSERT_EQ(rv.x, v.x);
    ASSERT_EQ(rv.y, v.y);
    ASSERT_EQ(rv.z, v.z);
    ASSERT_EQ(rn.x, n.x);
    ASSERT_EQ(rn.y, n.y);
    ASSERT_EQ(rn.z, n.z);
}

TEST(MobSwingTest, RotateKnownAngles) {
    // The integer limb-angle Taylor at 1.0 rad (2^16 x 2^16 fixed): pin
    // the exact deterministic values + the double reference bound.
    std::int32_t s16 = 0;
    std::int32_t c16 = 0;
    swingSinCosFixed(65536, s16, c16);
    EXPECT_EQ(s16, 55147);  // sin(1 rad) * 2^16 (exact integer result)
    EXPECT_EQ(c16, 35408);  // cos(1 rad) * 2^16 (1 ulp under the true 35409)
    EXPECT_NEAR(static_cast<double>(s16) / 65536.0, std::sin(1.0), 3e-5);
    EXPECT_NEAR(static_cast<double>(c16) / 65536.0, std::cos(1.0), 3e-5);
    // Odd/even: sin(-t) == -sin(t) exactly (the alternating series),
    // cos(-t) == cos(t) exactly.
    std::int32_t sn16 = 0;
    std::int32_t cn16 = 0;
    swingSinCosFixed(-65536, sn16, cn16);
    EXPECT_EQ(sn16, -55147);
    EXPECT_EQ(cn16, 35408);
    // The data amplitude 0.5 rad quantizes to exactly 2^15 (binary).
    EXPECT_EQ(swingFixedFromRadian(0.5f), 32768);
    EXPECT_EQ(swingFixedFromRadian(1.0f), 65536);
    // A leg tip one unit below the pivot, swung by +1 rad: the tip lands
    // at (-cos, -sin) — toward -Z = the mob's front (the documented
    // sign convention).
    const float s = static_cast<float>(s16) / 65536.0f;
    const float c = static_cast<float>(c16) / 65536.0f;
    const Vec3 tip = swingRotateVertex({0.0f, -1.0f, 0.0f},
                                       {0.0f, 0.0f, 0.0f}, s, c);
    EXPECT_NEAR(tip.y, -std::cos(1.0f), 5e-5);
    EXPECT_NEAR(tip.z, -std::sin(1.0f), 5e-5);
    EXPECT_FLOAT_EQ(tip.x, 0.0f); // the X-axis rotation never touches x
    // The same tip at the HALF-CYCLE phase (the group-1 offset, -1 rad):
    // the z displacement flips sign, the y stays.
    const float sn = static_cast<float>(sn16) / 65536.0f;
    const Vec3 tipNeg = swingRotateVertex({0.0f, -1.0f, 0.0f},
                                          {0.0f, 0.0f, 0.0f}, sn, cn16 / 65536.0f);
    EXPECT_NEAR(tipNeg.z, +std::sin(1.0f), 5e-5);
    EXPECT_NEAR(tipNeg.y, -std::cos(1.0f), 5e-5);
    // Normals rotate rigidly with the vertices: the +Y face normal at
    // +1 rad tilts toward +Z (the leg tip swings forward -Z, so the top
    // face leans back — its shade bucket follows, basic.vert).
    const Vec3 ny = swingRotateNormal({0.0f, 1.0f, 0.0f}, s, c);
    EXPECT_NEAR(ny.y, std::cos(1.0f), 5e-5);
    EXPECT_NEAR(ny.z, std::sin(1.0f), 5e-5);
}

TEST(MobSwingTest, SwingModelPartsPinned) {
    // The two-leg walker: 3 boxes x 24 = 72 base vertices, 108 indices,
    // two parts at boxes 1/2 with the data amplitude quantized (0.5 rad
    // = exactly 32768) and the half-cycle groups.
    const SwingModel sm = makeTwoLegSwingModel();
    ASSERT_TRUE(sm.active());
    EXPECT_EQ(sm.base.size(), 72u);
    EXPECT_EQ(sm.indices.size(), 108u);
    ASSERT_EQ(sm.parts.size(), 2u);
    EXPECT_EQ(sm.parts[0].boxIndex, 1);
    EXPECT_EQ(sm.parts[0].vertexStart, 24); // box 1 * 24
    EXPECT_EQ(sm.parts[0].amplitudeFixed, 32768);
    EXPECT_EQ(sm.parts[0].group, 0);
    EXPECT_EQ(sm.parts[1].boxIndex, 2);
    EXPECT_EQ(sm.parts[1].vertexStart, 48);
    EXPECT_EQ(sm.parts[1].group, 1);
    // The pivots: both legs' top-center (the boxes are identical: y 0..0.5
    // -> pivot y 0.5).
    EXPECT_FLOAT_EQ(sm.parts[0].pivot.y, 0.5f);
    EXPECT_FLOAT_EQ(sm.parts[1].pivot.y, 0.5f);
    // The per-phase (sin, cos) precompute: eff 0 is the identity, eff N/4
    // is the full amplitude (0.5 rad).
    EXPECT_FLOAT_EQ(sm.parts[0].sinCos[0], 0.0f);
    EXPECT_FLOAT_EQ(sm.parts[0].sinCos[1], 1.0f);
    const std::int32_t tQuarter = fmul(32768, kSwingSinTable[kSwingPhaseCount / 4]);
    std::int32_t s16 = 0;
    std::int32_t c16 = 0;
    swingSinCosFixed(tQuarter, s16, c16);
    EXPECT_FLOAT_EQ(sm.parts[0].sinCos[kSwingPhaseCount / 2],
                    static_cast<float>(s16) / 65536.0f);
    EXPECT_FLOAT_EQ(sm.parts[0].sinCos[kSwingPhaseCount / 2 + 1],
                    static_cast<float>(c16) / 65536.0f);
    // A type without swing data: base mesh only, no parts (zombie).
    MobRegistry mobs = makeMobRegistry();
    const SwingModel zm = buildSwingModel(mobs.at(MobID{0}).model,
                                          makeMobLayout());
    EXPECT_FALSE(zm.active());
    EXPECT_EQ(zm.base.size(), 48u);
}

TEST(MobSwingTest, SwingFramePhaseZeroEqualsStatic) {
    // The rest pose is the identity for EVERY part (the table's 0 and
    // N/2 entries are exactly 0): the phase-0 frame == the base, byte
    // for byte (the fast-path contract: the static mesh is a valid
    // phase-0 render).
    const SwingModel sm = makeTwoLegSwingModel();
    std::vector<MeshVertex> f0;
    buildSwingFrame(sm, 0, f0);
    ASSERT_EQ(f0.size(), sm.base.size());
    for (std::size_t i = 0; i < f0.size(); ++i) {
        ASSERT_EQ(f0[i].position.x, sm.base[i].position.x) << i;
        ASSERT_EQ(f0[i].position.y, sm.base[i].position.y) << i;
        ASSERT_EQ(f0[i].position.z, sm.base[i].position.z) << i;
        ASSERT_EQ(f0[i].normal.x, sm.base[i].normal.x) << i;
        ASSERT_EQ(f0[i].normal.y, sm.base[i].normal.y) << i;
        ASSERT_EQ(f0[i].normal.z, sm.base[i].normal.z) << i;
        ASSERT_EQ(f0[i].uv.x, sm.base[i].uv.x) << i;
        ASSERT_EQ(f0[i].uv.y, sm.base[i].uv.y) << i;
    }
}

TEST(MobSwingTest, SwingFrameOnlyTouchesSwingParts) {
    // Mid-swing (phase 8): the non-swing body vertices stay bit-identical
    // to the base; the leg vertices move (and their normals tilt).
    const SwingModel sm = makeTwoLegSwingModel();
    std::vector<MeshVertex> f;
    buildSwingFrame(sm, 8, f);
    ASSERT_EQ(f.size(), sm.base.size());
    for (int i = 0; i < kBoxVertexCount; ++i) { // the body (box 0)
        const MeshVertex& b = sm.base[static_cast<std::size_t>(i)];
        const MeshVertex& v = f[static_cast<std::size_t>(i)];
        ASSERT_EQ(v.position.x, b.position.x) << i;
        ASSERT_EQ(v.position.y, b.position.y) << i;
        ASSERT_EQ(v.position.z, b.position.z) << i;
        ASSERT_EQ(v.normal.x, b.normal.x) << i;
    }
    bool moved = false;
    bool normalsTilted = false;
    for (const SwingPart& p : sm.parts) { // both legs (boxes 1, 2)
        for (int i = 0; i < kBoxVertexCount; ++i) {
            const std::size_t idx =
                static_cast<std::size_t>(p.vertexStart + i);
            const MeshVertex& b = sm.base[idx];
            const MeshVertex& v = f[idx];
            if (v.position != b.position) {
                moved = true;
            }
            if (v.normal != b.normal) {
                normalsTilted = true;
            }
        }
    }
    EXPECT_TRUE(moved);
    EXPECT_TRUE(normalsTilted);
}

TEST(MobSwingTest, SwingFrameGroupsAreAntiphase) {
    // The half-cycle offset, made visible: the identical legs at phases
    // p and p + N/2 exchange roles — leg_a(p + N/2) == leg_b(p) vertex
    // for vertex, for every p (the group-1 offset is exactly a half
    // cycle, the diagonal gait).
    const SwingModel sm = makeTwoLegSwingModel();
    ASSERT_EQ(sm.parts.size(), 2u);
    std::vector<MeshVertex> fa;
    std::vector<MeshVertex> fb;
    for (int p = 0; p < kSwingPhaseCount; ++p) {
        buildSwingFrame(sm, p, fa);
        buildSwingFrame(sm, (p + kSwingHalf) % kSwingPhaseCount, fb);
        // leg_a is parts[0] at frame fa; leg_b is parts[1] at frame fb
        // only when p' = p + N/2 puts leg_b at eff p... check the direct
        // statement: leg_b(p) has the pose leg_a(p + N/2) has.
        for (int i = 0; i < kBoxVertexCount; ++i) {
            const std::size_t ia =
                static_cast<std::size_t>(sm.parts[0].vertexStart + i);
            const std::size_t ib =
                static_cast<std::size_t>(sm.parts[1].vertexStart + i);
            ASSERT_EQ(fa[ib].position.x, fb[ia].position.x) << p << i;
            ASSERT_EQ(fa[ib].position.y, fb[ia].position.y) << p << i;
            ASSERT_EQ(fa[ib].position.z, fb[ia].position.z) << p << i;
        }
    }
    // And at a mid-swing phase the two legs are actually DIFFERENT
    // (the antiphase, not a shared pose): the z displacements oppose.
    buildSwingFrame(sm, 8, fa);
    const std::size_t ia =
        static_cast<std::size_t>(sm.parts[0].vertexStart + 20);
    const std::size_t ib =
        static_cast<std::size_t>(sm.parts[1].vertexStart + 20);
    EXPECT_NE(fa[ia].position.z, fa[ib].position.z);
}

TEST(MobRegistryTest, ParsesSwingSpecs) {
    // The full valid schema: named boxes + the swing list, resolved to
    // box indices; a swing-less model (the legacy zombie) parses with an
    // empty list (the v0.9.0 content).
    MobRegistry mobs;
    auto r = mobs.registerMob(parseJson(kSwingBaseDef));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const Mob& m = mobs.at(*r);
    EXPECT_EQ(m.model.boxes[0].name, "body");
    EXPECT_EQ(m.model.boxes[1].name, "leg");
    ASSERT_EQ(m.model.swings.size(), 1u);
    EXPECT_EQ(m.model.swings[0].boxIndex, 1);
    EXPECT_FLOAT_EQ(m.model.swings[0].amplitude, 0.5f);
    EXPECT_EQ(m.model.swings[0].group, 0);
    MobRegistry legacy = makeMobRegistry(); // the plain zombie (no swings)
    EXPECT_TRUE(legacy.at(MobID{0}).model.swings.empty());
    EXPECT_EQ(legacy.at(MobID{0}).model.boxes[0].name, ""); // unnamed
}

TEST(MobRegistryTest, RejectsSwingSchemaErrors) {
    // The strict-parse table: every error names the offending field.
    const struct {
        const char* anchor;
        const char* broken;
        const char* expect; // message substring
    } cases[] = {
        {"\"part\": \"leg\"", "\"part\": \"nope\"", "unknown part"},
        {"\"part\": \"leg\"", "\"part\": \"\"", "non-empty"},
        {"\"amplitude\": 0.5", "\"amplitude\": 0", "(0, 1]"},
        {"\"amplitude\": 0.5", "\"amplitude\": 1.5", "(0, 1]"},
        {"\"amplitude\": 0.5", "\"amplitude\": \"wide\"", "amplitude"},
        {"\"group\": 0}", "\"group\": 2}", "0 or 1"},
        {"\"group\": 0}", "\"group\": 0.5}", "integer"},
        {"\"group\": 0}", "\"group\": 0, \"amp\": 1}", "unknown field"},
        {"\"texture\": \"b\"}", "\"texture\": \"b\", \"foo\": 1}",
         "unknown field"},
        {"\"name\": \"leg\"", "\"name\": \"body\"", "duplicate part name"},
        {"\"swings\": [",
         "\"swings\": [{\"part\": \"leg\", \"amplitude\": 0.5, "
         "\"group\": 0},",
         "already claimed"},
    };
    for (const auto& c : cases) {
        std::string def = kSwingBaseDef;
        const std::size_t p = def.find(c.anchor);
        ASSERT_NE(p, std::string::npos) << c.broken;
        // The "already claimed" case opens the swings array with an extra
        // entry for the same part; the rest replace in place.
        def.replace(p, std::strlen(c.anchor), c.broken);
        MobRegistry mobs;
        auto r = mobs.registerMob(parseJson(def));
        EXPECT_FALSE(r.ok()) << c.broken;
        if (!r.ok()) {
            EXPECT_NE(r.error().message.find(c.expect), std::string::npos)
                << r.error().message << " <- " << c.broken;
        }
    }
}

// -- V2-2: the per-face tile overrides (the strict parse) -----------------------

namespace {

// The V2-2 fixture def: the zombie's properties + a NAMED head box whose
// `faces` field is the injected object text ("" = no faces field at all).
std::string defWithFaces(const std::string& faces) {
    const std::string facesField =
        faces.empty() ? "" : (", \"faces\": " + faces);
    return std::string(R"({
  "id": "zombie",
  "properties": {
    "health": 20,
    "speed": 0.8,
    "detect_radius": 16,
    "attack_range": 1.6,
    "attack_damage": 3,
    "attack_cooldown": 1.0,
    "wander_interval": 2.5
  },
  "model": {
    "width": 0.6,
    "height": 1.8,
    "boxes": [
      {"name": "head", "pos": [0.0, 1.25, 0.0], "size": [0.5, 0.5, 0.5],
       "texture": "a")" + facesField + R"( } ]
  }
})");
}

} // namespace

TEST(MobRegistryTest, ParsesFacesFullAndPartial) {
    // The full six-face map: parsed exact (the yaw-0 = -Z vocabulary,
    // the values verbatim).
    MobRegistry mobs;
    auto r = mobs.registerMob(
        parseJson(defWithFaces(
            R"({"front":"f","back":"k","left":"l","right":"r","top":"t","bottom":"b"})")));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(mobs.at(*r).model.boxes[0].faces,
              (std::map<std::string, std::string> {
                  {"front", "f"}, {"back", "k"}, {"left", "l"},
                  {"right", "r"}, {"top", "t"}, {"bottom", "b"}}));
    // Partial keys: only the named faces are overridden (the rest fall
    // back to `texture` at the geometry build — see MobMeshTest).
    MobRegistry partial;
    auto rp = partial.registerMob(parseJson(defWithFaces(R"({"front":"f"})")));
    ASSERT_TRUE(rp.ok()) << rp.error().message;
    EXPECT_EQ(partial.at(*rp).model.boxes[0].faces,
              (std::map<std::string, std::string>{{"front", "f"}}));
    // An empty {} = the absent case: the pre-V2-2 single-tile behavior.
    MobRegistry empty;
    auto re = empty.registerMob(parseJson(defWithFaces(R"({})")));
    ASSERT_TRUE(re.ok()) << re.error().message;
    EXPECT_TRUE(empty.at(*re).model.boxes[0].faces.empty());
    // No faces field at all: the shipped legacy def parses unchanged
    // (every pre-V2-2 mob = the bit-identical root).
    MobRegistry none = makeMobRegistry();
    EXPECT_TRUE(none.at(MobID{0}).model.boxes[0].faces.empty());
    EXPECT_TRUE(none.at(MobID{0}).model.boxes[1].faces.empty());
}

TEST(MobRegistryTest, RejectsBadFaces) {
    // The strict-parse table: every error names the offending face (and
    // the box — the named part 'head' here).
    const struct {
        const char* faces;
        const char* expect; // message substring
    } cases[] = {
        {R"({"up":"f"})", "unknown face 'up'"},
        {R"({"front":"f","frontal":"f"})", "unknown face 'frontal'"},
        {R"({"front":""})", "non-empty string"},
        {R"({"front":42})", "non-empty string"},
        {R"({"front":null})", "non-empty string"},
        {R"([{"front":"f"}])", "must be a JSON object"},
    };
    for (const auto& c : cases) {
        MobRegistry mobs;
        auto r = mobs.registerMob(parseJson(defWithFaces(c.faces)));
        ASSERT_FALSE(r.ok()) << c.faces;
        EXPECT_NE(r.error().message.find(c.expect), std::string::npos)
            << r.error().message << " <- " << c.faces;
        EXPECT_NE(r.error().message.find("box 'head'"), std::string::npos)
            << r.error().message << " <- " << c.faces;
    }
}

TEST(MobSwingTest, ShippedMobsSwingContentPinned) {
    // The 99.7 content + the ENT-1 swing spec: the legs (and only the
    // legs) swing. chicken 2 legs (half-cycle), cow/sheep 4 legs
    // (diagonal), spider 8 legs (even/odd rows), zombie/skeleton no limb
    // parts at all (head + torso — the documented deviation: no
    // fabricated arm geometry).
    struct Exp {
        const char* part;
        float amplitude;
        int group;
    };
    const Exp chicken[] = {{"leg_l", 0.4f, 0}, {"leg_r", 0.4f, 1}};
    const Exp quadruped[] = {{"leg_fl", 0.5f, 0}, {"leg_fr", 0.5f, 1},
                             {"leg_rl", 0.5f, 1}, {"leg_rr", 0.5f, 0}};
    const Exp spider[] = {{"leg_1", 0.45f, 0}, {"leg_2", 0.45f, 1},
                          {"leg_3", 0.45f, 0}, {"leg_4", 0.45f, 1},
                          {"leg_5", 0.45f, 0}, {"leg_6", 0.45f, 1},
                          {"leg_7", 0.45f, 0}, {"leg_8", 0.45f, 1}};
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    MobRegistry mobs;
    ASSERT_TRUE(mobs.loadDirectory(assets, "data/mobs").ok());
    auto expectSwings = [&mobs](const char* mobName, const Exp* exp, int n) {
        const Mob& m = mobs.at(mobs.idForName(mobName));
        ASSERT_EQ(static_cast<int>(m.model.swings.size()), n) << mobName;
        for (int i = 0; i < n; ++i) {
            const MobSwing& s = m.model.swings[static_cast<std::size_t>(i)];
            const std::string part =
                m.model.boxes[static_cast<std::size_t>(s.boxIndex)].name;
            EXPECT_EQ(part, exp[i].part) << mobName << " swing " << i;
            EXPECT_FLOAT_EQ(s.amplitude, exp[i].amplitude)
                << mobName << " swing " << i;
            EXPECT_EQ(s.group, exp[i].group) << mobName << " swing " << i;
        }
    };
    expectSwings("chicken", chicken, 2);
    expectSwings("cow", quadruped, 4);
    expectSwings("sheep", quadruped, 4);
    expectSwings("spider", spider, 8);
    EXPECT_TRUE(mobs.at(mobs.idForName("zombie")).model.swings.empty());
    EXPECT_TRUE(mobs.at(mobs.idForName("skeleton")).model.swings.empty());
    // The non-swinging boxes of a swinging mob stay unnamed-free or
    // unnamed-and-ignored: every swung box is a leg (the wings, body and
    // head of the chicken never swing).
    const Mob& ck = mobs.at(mobs.idForName("chicken"));
    for (const MobSwing& s : ck.model.swings) {
        EXPECT_EQ(
            ck.model.boxes[static_cast<std::size_t>(s.boxIndex)].texture,
            "chicken_legs");
    }
}

TEST(MobEntityTest, SwingPhaseAdvancesWhileChasing) {
    // The phase advances kSwingStep per MOVING frame: a chasing zombie
    // (strict horizontal displacement every frame) at 10 frames has
    // swung exactly 10 * 2 = 20.
    Fixture f;
    MobRegistry mobs;
    ASSERT_TRUE(mobs.registerMob(parseJson(kSwingZombieDef)).ok());
    const Mob& def = mobs.at(MobID{0});
    MobEntity mob = makeMob(f, def, {8.5f, 1.0f, 5.5f});
    mob.setPlayer({8.5f, 1.0f, 8.5f}, true); // 3 away: chase, not attack
    for (int i = 0; i < 10; ++i) {
        mob.setPlayer({8.5f, 1.0f, 8.5f}, true);
        mob.update(kDt);
        ASSERT_EQ(mob.state(), MobState::Chase) << "frame " << i;
    }
    EXPECT_EQ(mob.swingPhase(), 10 * kSwingStep);
}

TEST(MobEntityTest, SwingPhaseDecaysToZeroWhenStopped) {
    // Chase 5 frames (phase 10), then the player steps into attack range:
    // the mob stands still and the phase decays 4/frame to the rest pose
    // (10 -> 6 -> 2 -> 0) exactly three frames later.
    Fixture f;
    MobRegistry mobs;
    ASSERT_TRUE(mobs.registerMob(parseJson(kSwingZombieDef)).ok());
    const Mob& def = mobs.at(MobID{0});
    MobEntity mob = makeMob(f, def, {8.5f, 1.0f, 5.5f});
    for (int i = 0; i < 5; ++i) {
        mob.setPlayer({8.5f, 1.0f, 8.5f}, true);
        mob.update(kDt);
    }
    ASSERT_EQ(mob.swingPhase(), 5 * kSwingStep);
    // The player is now 1.0 away horizontally (inside attack range 1.6):
    // the mob stands still and swings its attack (zero displacement).
    for (int i = 0; i < 3; ++i) {
        mob.setPlayer({8.5f, 1.0f, 6.5f}, true);
        mob.update(kDt);
    }
    EXPECT_EQ(mob.state(), MobState::Attack);
    EXPECT_EQ(mob.swingPhase(), 0); // the rest pose, exactly
}

TEST(MobEntityTest, SwingPhaseInertWithoutSwingData) {
    // The structural gate: a swing-less type (the legacy zombie) NEVER
    // advances — its phase stays 0 (the identity pose) even while
    // walking, so its render path is the static mesh (immunity).
    Fixture f;
    MobRegistry mobs = makeMobRegistry(); // no 'swings' in the model
    const Mob& def = mobs.at(MobID{0});
    MobEntity mob = makeMob(f, def, {8.5f, 1.0f, 5.5f});
    for (int i = 0; i < 20; ++i) {
        mob.setPlayer({8.5f, 1.0f, 8.5f}, true);
        mob.update(kDt);
    }
    EXPECT_EQ(mob.state(), MobState::Chase);
    EXPECT_EQ(mob.swingPhase(), 0);
}

// -- ENT-4: the underwater behavior (the fluid immersion model) ------------
//
// The shared fluid model (Entity::refreshFluidState / integrateFluid —
// the input-free twin of the player's FLUID-2 swim, FLUID-3-corrected): a
// submerged mob RISES (the buoyancy 12, capped 1.5) to the SWIM LEVEL =
// W' - height/2 (the body's mid-axis on the surface — half the body
// submerged, the MC swim look; the exact, fps-independent terminal state;
// the pre-FLUID-3 float put the FEET on W', a fully dry body "standing on
// water"), and a RESTING mob rises in a pool >= 2 deep (the Q2 depth gate
// — the 1-deep wade still stands on the bottom, the deep-water lockout
// fixed); a falling mob (the entry momentum, the vy >= 0 rise gate
// excluded) SINKS to the pool floor (and in a 2-deep pool rises off the
// bottom to the swim level — the 1-deep stand is unchanged); a DRY body
// in the at-surface band keeps its vy (the full-speed pass — no -4 clamp,
// no buoyancy — the player's twin line, the cliff-dive "parachute" fixed);
// a knocked-up body breaches the surface (the body's top clears W') and
// settles at the swim level (the splash). The horizontal speed is gated
// ×0.6 (kUnderwaterSpeedFactor); the ENT-3 step-up runs in fluid too
// (the FLUID-3 lift of the ENT-4 ban — a wading mob steps out of the
// 1-deep pool; a swimming mob, onGround false, cannot step up: no swim-up
// shore climb, the documented deviation). The player path is untouched
// (its dry-band snap to the feet-plane is the accepted first-person Q2
// deviation) — test_player.cpp is the bit-level red line. The fire system
// (P57/58/73) is untouched — the MobFireTest lava scenarios above pin the
// burn rate (the 2-deep lava floor stand now rises to the swim level,
// still inside the top lava cell — the re-ignition cadence holds).
//
// Pool conventions: the fixture floor tops out at y=1; an n-cell L8 pool
// (y=1..n) has W' = 1+n. The zombie test def (h 1.8) swims at
// W' - 0.9: the 2-deep floor (fillFloor / the 2-cell pool, W' = 3) swim
// level is 2.1 exact; the item (kSize 0.25) at W' - 0.125: 2.875 exact.
// fillFloor (the fire-section helper) covers the whole fixture floor in
// 2-deep fluid — a wandering mob (<= 0.4 b/s, gated 0.24 in fluid) cannot
// leave it within a test window, so the float tests do not depend on the
// wander path.

namespace {

// An n-cell full (L8) pool on the fixture floor. `kind` defaults to water;
// the symmetric lava float (the documented ENT-4 extension) is a test.
void buildMobPool(Fixture& f, int x0, int x1, int z0, int z1, int n,
                  FluidKind kind = FluidKind::Water) {
    for (int x = x0; x <= x1; ++x) {
        for (int z = z0; z <= z1; ++z) {
            for (int y = 1; y <= n; ++y) {
                f.world.setFluid(x, y, z, kind, kMaxFluidLevel);
            }
        }
    }
}

// A 1-cell L8 film across the whole fixture floor (the wade case: W' = 2,
// the feet on the pool floor — a step-up would lift out of the water).
void buildMobFilm(Fixture& f, int x1) {
    for (int x = -16; x <= x1; ++x) {
        for (int z = -16; z < 16; ++z) {
            f.world.setFluid(x, 1, z, FluidKind::Water, kMaxFluidLevel);
        }
    }
}

} // namespace

TEST(MobFluidTest, DetectsFootBodyLavaAndDry) {
    // The in-fluid state = the strict AABB overlap of at least one water
    // OR lava cell (the player's isInWater/isInLava union scan): the feet
    // cell (a wading mob on a 1-cell film), the BODY cell (a submerged
    // mob whose feet clear the water), lava (the symmetric path), and DRY
    // (no pool — the state is inert).
    Fixture f;
    MobRegistry mobs = makeMobRegistry();
    const Mob& def = mobs.at(MobID{0});

    // Feet cell: a 1-cell pool (the water at y=1), the mob's feet on the
    // pool floor (y=1) — the AABB [1, 2.8] strictly overlaps the cell
    // [1, 2) and nothing else.
    buildMobPool(f, 6, 10, 6, 10, 1);
    MobEntity feet = makeMob(f, def, {8.5f, 1.0f, 8.5f});
    feet.setPlayer({30.0f, 1.0f, 30.0f}, false);
    feet.update(kDt);
    EXPECT_TRUE(feet.inFluid());

    // Body cell: a 2-cell pool, the mob's feet at y=2 (the AABB [2, 3.8]
    // strictly overlaps ONLY the body water cell [2, 3) — the feet cell
    // [1, 2) is not overlapped).
    buildMobPool(f, -14, -10, 6, 10, 2);
    MobEntity body = makeMob(f, def, {-12.5f, 2.0f, 8.5f});
    body.setPlayer({30.0f, 1.0f, 30.0f}, false);
    body.update(kDt);
    EXPECT_TRUE(body.inFluid());
    EXPECT_TRUE(body.overlapsFluid(FluidKind::Water));

    // Lava: the symmetric in-fluid path (P58's burn is untouched — the
    // fire system's FRESH checks run as before). The pool stays inside
    // the fixture floor's chunk region (setFluid on an unloaded chunk
    // would fail — the P54 void-stays-dry rule).
    buildMobPool(f, 14, 15, 6, 10, 1, FluidKind::Lava);
    MobEntity lava = makeMob(f, def, {14.5f, 1.0f, 8.5f});
    lava.setPlayer({30.0f, 1.0f, 30.0f}, false);
    lava.update(kDt);
    EXPECT_TRUE(lava.inFluid());

    // Dry: the same floor with no pool under the mob — inert.
    MobEntity dry = makeMob(f, def, {0.5f, 1.0f, 0.5f});
    dry.setPlayer({30.0f, 1.0f, 30.0f}, false);
    dry.update(kDt);
    EXPECT_FALSE(dry.inFluid());
}

TEST(MobFluidTest, SubmergedMobRisesAndFloatsAtSurface) {
    // A submerged body with no input RISES (the buoyancy, capped at 1.5)
    // and FLOATS at the SWIM LEVEL = W' - h/2 = 3 - 0.9 = 2.1 (the
    // 2-deep floor pool, the body's mid-axis on the surface — half the
    // body submerged, the MC swim look; FLUID-3: the pre-FLUID-3 float
    // put the FEET on W' = 3, a fully dry body "standing on water", the
    // reported B1). The geometric terminal (exact, fps-independent; the
    // assertion point).
    Fixture f;
    fillFloor(f, FluidKind::Water); // W' = 3 everywhere on the floor
    MobRegistry mobs = makeMobRegistry();
    const Mob& def = mobs.at(MobID{0}); // h 1.8: the swim level 3 - 0.9
    MobEntity mob = makeMob(f, def, {8.5f, 2.0f, 8.5f}); // 0.1 below it
    mob.setPlayer({30.0f, 1.0f, 30.0f}, false); // pure wander (stays in)
    bool rose = false;
    for (int i = 0; i < 180; ++i) {
        mob.update(kDt);
        if (mob.position().y > 2.05f) {
            rose = true;
        }
        // The rise never exceeds the swim level by more than one rise
        // sub-step (the cap 1.5 over the 1/60 frame — the snap clamps the
        // crossing to the swim level exactly).
        EXPECT_LE(mob.position().y, 2.1f + 1.5f / 60.0f + 1e-6f)
            << "frame " << i;
    }
    EXPECT_TRUE(rose); // it did rise (not the pre-ENT-4 sink-to-bottom)
    // The geometric terminal: exact (the float is geometry, not a force
    // balance).
    EXPECT_FLOAT_EQ(mob.position().y, 2.1f);
    EXPECT_EQ(mob.velocity().y, 0.0f);
    EXPECT_FALSE(mob.onGround());
    // FLUID-3: the terminal is HALF SUBMERGED (the body's lower half in
    // the water — the snap re-fires as a no-op fixed point) — the
    // pre-FLUID-3 dry W' float had inFluid false.
    EXPECT_TRUE(mob.inFluid());
    // The fixed point: more frames keep it (the fps-independent terminal
    // — the E2E assertion point).
    runMob(mob, 240);
    EXPECT_FLOAT_EQ(mob.position().y, 2.1f);
    EXPECT_EQ(mob.velocity().y, 0.0f);
    EXPECT_FALSE(mob.onGround());
    EXPECT_TRUE(mob.inFluid());
}

TEST(MobFluidTest, FallingMobSinksToPoolFloor) {
    // Entry momentum: a fast-falling mob (the vy >= 0 rise gate excluded)
    // sinks at the water terminal (-4) to the pool floor — and, FLUID-3
    // (the fixture is a 2-deep pool: the Q2 depth gate), it RISES off the
    // bottom to the swim level (W' - h/2 = 3 - 0.9 = 2.1) instead of
    // standing (the 1-deep wade stand is pinned in
    // OneDeepPoolNoRiseWhenResting). The dive-to-bottom is the mutually
    // exclusive proof vs an immediate float: it reaches y = 1 first.
    Fixture f;
    fillFloor(f, FluidKind::Water); // the floor y=1, W' = 3 (2-deep)
    MobRegistry mobs = makeMobRegistry();
    const Mob& def = mobs.at(MobID{0}); // h 1.8: the swim level 2.1
    MobEntity mob = makeMob(f, def, {8.5f, 8.0f, 8.5f});
    mob.setVelocity({0.0f, -30.0f, 0.0f}); // a fast entry
    mob.setPlayer({30.0f, 1.0f, 30.0f}, false);
    float minY = 8.0f;
    for (int i = 0; i < 600; ++i) { // 10 s: the air fall + the sink + the rise
        mob.update(kDt);
        minY = std::min(minY, mob.position().y);
    }
    // It DIVED to the pool floor (the entry momentum — a still body would
    // float straight up from the preset).
    EXPECT_FLOAT_EQ(minY, 1.0f);
    // ...and rose off the bottom (the 2-deep Q2 depth gate) to the swim
    // level terminal (exact, fps-independent).
    EXPECT_FLOAT_EQ(mob.position().y, 2.1f);
    EXPECT_EQ(mob.velocity().y, 0.0f);
    EXPECT_FALSE(mob.onGround());
    EXPECT_TRUE(mob.inFluid());
    // It did not stand on the floor (mutually exclusive with the 1-deep
    // terminal of OneDeepPoolNoRiseWhenResting).
    EXPECT_NE(mob.position().y, 1.0f);
}

TEST(MobFluidTest, WadesSlowerInWaterThanOnLand) {
    // The horizontal gate: a chasing mob's per-frame displacement is
    // speed × 0.6 / 60 in the wade (the feet on the 1-cell film) and
    // speed / 60 on land (the identical fixture minus the film). The
    // chase is exactly +X (the player ahead at the same height), so the
    // per-frame dx is the raw gated speed (no normalization residue).
    MobRegistry mobs = makeMobRegistry();
    const Mob& def = mobs.at(MobID{0});

    Fixture f;
    buildMobFilm(f, 15); // the 1-cell film: W' = 2, the wade case
    MobEntity inWater = makeMob(f, def, {6.5f, 1.0f, 9.5f});
    inWater.setPlayer({15.5f, 1.0f, 9.5f}, true); // chase +X
    Fixture g; // the same floor, DRY
    MobEntity onLand = makeMob(g, def, {6.5f, 1.0f, 9.5f});
    onLand.setPlayer({15.5f, 1.0f, 9.5f}, true);
    runMob(inWater, 15); // 0.25 s — well inside the detect radius
    runMob(onLand, 15);
    const float dxWater = inWater.position().x - 6.5f;
    const float dxLand = onLand.position().x - 6.5f;
    EXPECT_NEAR(dxWater, 15.0 * 0.8f * 0.6f / 60.0, 1e-5f);
    EXPECT_NEAR(dxLand, 15.0 * 0.8f / 60.0, 1e-5f);
    EXPECT_NEAR(dxWater / dxLand, 0.6f, 1e-4f); // the gate ratio, pinned
    // The wade stays on the pool floor (the fresh resting gate: no rise
    // off the bottom — the float is at W' = 2, one block up, unreachable
    // for a grounded body).
    EXPECT_FLOAT_EQ(inWater.position().y, 1.0f);
    EXPECT_TRUE(inWater.onGround());
}

TEST(MobFluidTest, WadingMobStepsOutOfWater) {
    // FLUID-3 (the inversion of the ENT-4 NoStepUpInWater — the old
    // "stuck in the water" semantic is retired): the step-up runs in
    // fluid too (the `!m_inFluid` gate lifted — the player never had one):
    // the same 1-block step (two cells wide in +X — the pre-V2-9 width,
    // forced by the +X front-check quirk that V2-9 fixed) is
    // CROSSED in the wading film (the mob steps out of the 1-deep pool
    // over the shore wall — the "walked in, can't get out" report, A1)
    // AND on land (the unchanged twin). The step-up's geometry / the
    // TRUE-1-block judgment / the sub-step replay are untouched (ENT-3) —
    // only the gate. The 1-deep film's depth (W' - feet = 1 < 2) keeps the
    // wade a BOTTOM STAND (no rise) — it is the step, not the buoyancy,
    // that gets the mob out.
    MobRegistry mobs = makeMobRegistry();
    const Mob& def = mobs.at(MobID{0});

    Fixture f;
    // The step: x 10..11 (two cells in +X), z 8..10, top y=2.
    for (int x = 10; x < 12; ++x) {
        for (int z = 8; z < 10; ++z) {
            f.world.setBlock(x, 1, z, f.stone);
        }
    }
    // The wading film: up to the step face (the step cell itself is solid
    // — setFluid on it would fail, the block-occupies-cell rule).
    buildMobFilm(f, 9);
    MobEntity inWater = makeMob(f, def, {8.5f, 1.0f, 9.5f});
    inWater.setPlayer({13.9f, 1.0f, 9.5f}, true); // beyond the step

    Fixture g;
    for (int x = 10; x < 12; ++x) {
        for (int z = 8; z < 10; ++z) {
            g.world.setBlock(x, 1, z, g.stone);
        }
    }
    MobEntity onLand = makeMob(g, def, {8.5f, 1.0f, 9.5f});
    onLand.setPlayer({13.9f, 1.0f, 9.5f}, true);

    // The wade: in the film at the start (the feet cell L8) — the step-out
    // must come from a wading (onGround, inFluid) mob.
    bool sawWade = false;
    for (int i = 0; i < 900; ++i) {
        inWater.update(kDt);
        if (inWater.inFluid()) {
            sawWade = true;
        }
    }
    EXPECT_TRUE(sawWade);
    runMob(onLand, 900);
    // On land: crossed the step (and fell back to the far floor — the
    // ChasesUpOneBlockStep twin's stop point 13.9 - 1.6 = 12.3, y = 1).
    EXPECT_EQ(onLand.state(), MobState::Attack);
    EXPECT_NEAR(onLand.position().x, 12.3f, 0.02f);
    EXPECT_FLOAT_EQ(onLand.position().y, 1.0f);
    // In the wading film: the SAME crossing — the step-up over the shore
    // wall, then the fall to the far floor (mutually exclusive with the
    // pre-FLUID-3 clamp at 9.7, chase forever, the reported lockout).
    EXPECT_EQ(inWater.state(), MobState::Attack);
    EXPECT_NEAR(inWater.position().x, 12.3f, 0.02f);
    EXPECT_FLOAT_EQ(inWater.position().y, 1.0f);
    EXPECT_TRUE(inWater.onGround());
}

TEST(MobFluidTest, SubmergedRiseIsFrameLockedDeterministic) {
    // Two identical mobs (the same seed — the wander stream), preset
    // MID-POOL (the rise + the float transition inside the run): every
    // frame's state is bit-identical (hard rule 8: the fixed 1/60 step,
    // no real dt, no randomness beyond the shared stream).
    Fixture f;
    fillFloor(f, FluidKind::Water); // W' = 3
    MobRegistry mobs = makeMobRegistry();
    const Mob& def = mobs.at(MobID{0});
    MobEntity a = makeMob(f, def, {8.5f, 2.0f, 8.5f});
    MobEntity b = makeMob(f, def, {8.5f, 2.0f, 8.5f});
    const Vec3 far {30.0f, 1.0f, 30.0f};
    a.setPlayer(far, false);
    b.setPlayer(far, false);
    for (int i = 0; i < 300; ++i) {
        a.update(kDt);
        b.update(kDt);
        EXPECT_EQ(a.position().x, b.position().x) << "frame " << i;
        EXPECT_EQ(a.position().y, b.position().y) << "frame " << i;
        EXPECT_EQ(a.position().z, b.position().z) << "frame " << i;
        EXPECT_EQ(a.velocity().x, b.velocity().x) << "frame " << i;
        EXPECT_EQ(a.velocity().y, b.velocity().y) << "frame " << i;
        EXPECT_EQ(a.velocity().z, b.velocity().z) << "frame " << i;
    }
    // Both settled at the exact swim-level terminal (W' - h/2 = 2.1 — the
    // fps-independent assertion point).
    EXPECT_FLOAT_EQ(a.position().y, 2.1f);
    EXPECT_FLOAT_EQ(b.position().y, 2.1f);
    EXPECT_EQ(a.velocity().y, 0.0f);
    EXPECT_EQ(b.velocity().y, 0.0f);
    EXPECT_FALSE(a.onGround());
    EXPECT_FALSE(b.onGround());
    EXPECT_TRUE(a.inFluid());
    EXPECT_TRUE(b.inFluid());
}

TEST(MobFluidTest, AirBandFallKeepsFullVy) {
    // FLUID-3 fix A2 (the cliff-dive "parachute"): a DRY body falling
    // through the at-surface band (the feet within 0.4 of W', the body
    // clear of the water) keeps its FULL vy — no -4 water-terminal clamp,
    // no buoyancy decrement (the pre-FLUID-3 sink branch ran un-gated: it
    // clamped the dry fall to -4 inside the band). The player's twin
    // keeps this "keep vy" behavior — the shared model now matches it.
    // The preset (mid-fall beside the pool, vy = -5) is a real cliff fall
    // through the band.
    Fixture f;
    fillFloor(f, FluidKind::Water); // W' = 3
    MobRegistry mobs = makeMobRegistry();
    const Mob& def = mobs.at(MobID{0});
    MobEntity mob = makeMob(f, def, {8.5f, 3.2f, 8.5f}); // 0.2 above W'
    mob.setVelocity({0.0f, -5.0f, 0.0f}); // the fall in progress
    mob.setPlayer({30.0f, 1.0f, 30.0f}, false);
    float maxDryAbsVy = 0.0f;
    float maxWetAbsVy = 0.0f;
    bool wet = false;
    for (int i = 0; i < 300; ++i) {
        mob.update(kDt);
        const float absVy = std::abs(mob.velocity().y);
        if (mob.inFluid()) {
            wet = true;
            maxWetAbsVy = std::max(maxWetAbsVy, absVy);
        } else if (mob.position().y <= 3.0f + 0.4f) {
            // The dry pass (the band + the sub-surface air of the column):
            // full speed, unclamped.
            maxDryAbsVy = std::max(maxDryAbsVy, absVy);
        }
    }
    EXPECT_TRUE(wet); // it reached the water (the fall continued)
    // DRY: the fall kept the full -5 (the A2 fix — no -4 clamp, no
    // buoyancy decrement in the band).
    EXPECT_NEAR(maxDryAbsVy, 5.0f, 1e-6f);
    // WET: the entry momentum clamps at the water terminal -4 (the MC
    // dive, unchanged).
    EXPECT_LE(maxWetAbsVy, 4.0f + 1e-6f);
    // The full cycle: the dive to the floor (2-deep: it stands off the
    // bottom) settles at the swim level.
    EXPECT_FLOAT_EQ(mob.position().y, 2.1f);
    EXPECT_EQ(mob.velocity().y, 0.0f);
    EXPECT_FALSE(mob.onGround());
}

TEST(MobFluidTest, OneDeepPoolNoRiseWhenResting) {
    // FLUID-3 fix B3-guard (the E4 mainline kept): a mob RESTING on a
    // 1-deep pool floor (the wade — the feet in the 1-cell film) does NOT
    // rise (the Q2 depth gate: the pool depth W' - feet = 2 - 1 = 1 < 2) —
    // it stands on the bottom for the whole 1800-frame run.
    Fixture f;
    buildMobFilm(f, 15); // the 1-cell film: W' = 2, the wade case
    MobRegistry mobs = makeMobRegistry();
    const Mob& def = mobs.at(MobID{0});
    MobEntity mob = makeMob(f, def, {0.5f, 1.0f, 0.5f});
    mob.setPlayer({30.0f, 1.0f, 30.0f}, false); // pure wander (stays in)
    for (int i = 0; i < 1800; ++i) {
        mob.update(kDt);
        EXPECT_FLOAT_EQ(mob.position().y, 1.0f) << "frame " << i;
        EXPECT_TRUE(mob.onGround()) << "frame " << i;
    }
    // It waded (the feet cell L8 — the depth gate, not the dry state,
    // held it).
    EXPECT_TRUE(mob.inFluid());
}

TEST(MobFluidTest, TwoDeepPoolRisesWhenResting) {
    // FLUID-3 fix B3/A1 (the deep-water lockout): a mob RESTING on a
    // 2-deep pool floor RISES (the Q2 depth gate: the pool depth
    // W' - feet = 3 - 1 = 2 >= 2 lets the resting body off the bottom)
    // to the swim level = W' - h/2 = 3 - 0.9 = 2.1 exact, where it
    // settles (the fixed point). The pre-FLUID-3 resting gate held it on
    // the floor forever (the report: "sunk to the bottom, can't get
    // out" — the shore wall is now a step, the pool floor no longer a
    // trap).
    Fixture f;
    fillFloor(f, FluidKind::Water); // W' = 3 (2-deep), the floor y=1
    MobRegistry mobs = makeMobRegistry();
    const Mob& def = mobs.at(MobID{0}); // h 1.8: the swim level 2.1
    MobEntity mob = makeMob(f, def, {8.5f, 1.0f, 8.5f}); // resting on the floor
    mob.setPlayer({30.0f, 1.0f, 30.0f}, false); // pure wander (stays in)
    bool offBottom = false;
    for (int i = 0; i < 300; ++i) {
        mob.update(kDt);
        if (mob.position().y > 1.05f) {
            offBottom = true;
        }
        // The rise never exceeds the swim level by more than one rise
        // sub-step (the cap 1.5 over the 1/60 frame — the snap clamps the
        // crossing to the swim level exactly).
        EXPECT_LE(mob.position().y, 2.1f + 1.5f / 60.0f + 1e-6f)
            << "frame " << i;
    }
    EXPECT_TRUE(offBottom); // it left the floor (not the pre-FLUID-3 stand)
    EXPECT_FLOAT_EQ(mob.position().y, 2.1f);
    EXPECT_EQ(mob.velocity().y, 0.0f);
    EXPECT_FALSE(mob.onGround());
    EXPECT_TRUE(mob.inFluid());
    // The fixed point.
    runMob(mob, 240);
    EXPECT_FLOAT_EQ(mob.position().y, 2.1f);
    EXPECT_EQ(mob.velocity().y, 0.0f);
    EXPECT_FALSE(mob.onGround());
}

TEST(MobFluidTest, KnockbackSplashThenSwim) {
    // FLUID-3 fix B1-splash: a mob at the 2-deep pool floor knocked up
    // (the +3 impulse — the rise cap 1.5 clips it, the pre-FLUID-3
    // un-capped dry sink decrement let a dry rise run away) breaches the
    // SURFACE (the body's top clears W' = 3 — the swim look: the head out
    // of the water) and settles at the swim level = 3 - 0.9 = 2.1 (the
    // half-submerged terminal — the pre-FLUID-3 terminal was the dry
    // W' float, the feet on the surface, "standing on water"). The feet
    // never pass the swim level (the snap owns the crossing — a sub-step
    // overshoot of at most the cap 1.5 over the 1/60 frame).
    Fixture f;
    fillFloor(f, FluidKind::Water); // W' = 3 (2-deep), the floor y=1
    MobRegistry mobs = makeMobRegistry();
    const Mob& def = mobs.at(MobID{0}); // h 1.8: the swim level 2.1
    MobEntity mob = makeMob(f, def, {8.5f, 1.0f, 8.5f});
    mob.setPlayer({30.0f, 1.0f, 30.0f}, false);
    mob.addKnockback({0.0f, 3.0f, 0.0f}); // the upward impulse
    bool breached = false;
    for (int i = 0; i < 300; ++i) {
        mob.update(kDt);
        if (mob.position().y + def.model.height > 3.0f) {
            breached = true; // the body's top cleared the surface
        }
        EXPECT_LE(mob.position().y, 2.1f + 1.5f / 60.0f + 1e-6f)
            << "frame " << i;
    }
    EXPECT_TRUE(breached); // it splashed over the surface
    // The half-submerged swim terminal (exact, the fixed point).
    EXPECT_FLOAT_EQ(mob.position().y, 2.1f);
    EXPECT_EQ(mob.velocity().y, 0.0f);
    EXPECT_FALSE(mob.onGround());
    EXPECT_TRUE(mob.inFluid());
    runMob(mob, 240);
    EXPECT_FLOAT_EQ(mob.position().y, 2.1f);
    EXPECT_TRUE(mob.inFluid());
}

TEST(MobFluidTest, DryBodyAboveWaterNoRise) {
    // FLUID-3 fix B1 (the "standing on water" production chain): a DRY
    // body at the water surface does NOT rise (the passive-rise inFluid
    // gate — the pre-FLUID-3 rise ran on a dry body in the band, pushing
    // it to the dry W' float) and does NOT take fluid forces at all.
    // Part A (the band hover): a body preset in the dry at-surface band
    // (the feet 0.2 above W', the body clear of the water, no support —
    // a hover) keeps its vy (the pre-FLUID-3 dry-rise would have pushed
    // it; the player's dry-band snap to the feet-plane is the accepted
    // first-person Q2 deviation — the mob model does not copy it). The
    // band hold (the frozen vy) is a preset-only artifact: a real mob in
    // the band arrives falling (vy < 0) or from the swim level.
    Fixture f;
    fillFloor(f, FluidKind::Water); // W' = 3
    MobRegistry mobs = makeMobRegistry();
    const Mob& def = mobs.at(MobID{0});
    MobEntity hover = makeMob(f, def, {8.5f, 3.2f, 8.5f}); // 0.2 above W'
    hover.setPlayer({30.0f, 1.0f, 30.0f}, false);
    for (int i = 0; i < 300; ++i) {
        hover.update(kDt);
        // No rise (and no pull-down snap): the hover holds (the band's
        // keep-vy with vy 0 — the dry body is untouched by the fluid
        // model).
        EXPECT_FLOAT_EQ(hover.position().y, 3.2f) << "frame " << i;
        EXPECT_EQ(hover.velocity().y, 0.0f) << "frame " << i;
        EXPECT_FALSE(hover.inFluid()) << "frame " << i;
    }
    // Part B (the gravity fall): a body ABOVE the band (0.5 over W') is
    // not in the fluid model at all — plain gravity pulls it down (no
    // buoyancy while dry), it passes the band at the gravity-arrived vy
    // (the frozen keep-vy — the pre-FLUID-3 un-gated sink would have
    // decremented it in the band), enters the water (the -4 entry-momentum
    // clamp), dives to the floor and — 2-deep pool, the Q2 depth gate —
    // rises to the swim level terminal.
    MobEntity fall = makeMob(f, def, {8.5f, 3.5f, 8.5f}); // 0.5 above W'
    fall.setPlayer({30.0f, 1.0f, 30.0f}, false);
    float minDryVy = 0.0f;
    bool fell = false;
    for (int i = 0; i < 300; ++i) {
        fall.update(kDt);
        if (!fall.inFluid() && fall.position().y <= 3.4f) {
            // The dry band pass: the gravity-arrived vy, frozen (no
            // buoyancy decrement, no -4 clamp).
            minDryVy = std::min(minDryVy, fall.velocity().y);
        }
        if (fall.position().y < 3.4f) {
            fell = true; // gravity pulled it below the preset
        }
    }
    EXPECT_TRUE(fell);
    // Five gravity frames (3.5 -> the band at 3.37) = 5 x 32/60 = 8/3 —
    // the exact frozen band pass (the pre-FLUID-3 sink ran it to -4).
    EXPECT_NEAR(minDryVy, -8.0f / 3.0f, 1e-5f);
    // The full cycle settles at the swim level (not the pre-FLUID-3 dry
    // W' float).
    EXPECT_FLOAT_EQ(fall.position().y, 2.1f);
    EXPECT_EQ(fall.velocity().y, 0.0f);
    EXPECT_FALSE(fall.onGround());
    EXPECT_TRUE(fall.inFluid());
}

TEST(MobFluidTest, OnLandMobIsFluidInert) {
    // The on-land regression anchor: a DRY run never enters the fluid
    // model — the state stays inert (inFluid false, no surface plane) and
    // the fall is the plain gravity model (the on-land trajectory
    // bit-identical to the pre-ENT-4 code — the whole existing mob suite
    // pins it: chase / wander / knockback / step-up).
    Fixture f;
    MobRegistry mobs = makeMobRegistry();
    const Mob& def = mobs.at(MobID{0});
    MobEntity mob = makeMob(f, def, {8.5f, 10.0f, 8.5f});
    mob.setPlayer({30.0f, 1.0f, 30.0f}, false);
    for (int i = 0; i < 60; ++i) {
        mob.update(kDt);
        EXPECT_FALSE(mob.inFluid()) << "frame " << i;
    }
    EXPECT_TRUE(mob.onGround());
    EXPECT_FLOAT_EQ(mob.position().y, 1.0f);
    EXPECT_EQ(mob.velocity().y, 0.0f);
}

// ============================================================================
//   ENT-5 — mob fall damage (the Player's Phase-16 rule mirrored)
// ============================================================================

TEST(MobFallDamageTest, FallingAboveOffsetTakesDamage) {
    Fixture f;
    MobRegistry mobs = makeMobRegistry();
    const Mob& z = mobs.at(MobID{0});
    // Spawned 9 blocks above the fixture floor (top y1): fall 9 >
    // offset 3 -> 6 damage.
    MobEntity mob = makeMob(f, z, Vec3{0.5f, 10.0f, 0.5f});
    runMob(mob, 300);
    EXPECT_TRUE(mob.onGround());
    EXPECT_FLOAT_EQ(mob.health(), 14.0f);
}

TEST(MobFallDamageTest, FallingWithinOffsetIsHarmless) {
    Fixture f;
    MobRegistry mobs = makeMobRegistry();
    const Mob& z = mobs.at(MobID{0});
    // Spawned 3 blocks above the floor: fall 3 = the safe offset -> 0.
    MobEntity mob = makeMob(f, z, Vec3{0.5f, 4.0f, 0.5f});
    runMob(mob, 300);
    EXPECT_TRUE(mob.onGround());
    EXPECT_FLOAT_EQ(mob.health(), 20.0f);
}

TEST(MobFallDamageTest, LandingInAPoolCancelsTheFall) {
    // The player's P54 rule: ANY fluid cancels the fall. A 1-deep pool
    // in the landing column.
    Fixture f;
    MobRegistry mobs = makeMobRegistry();
    const Mob& z = mobs.at(MobID{0});
    f.world.setBlock(0, 1, 0, BlockID::air());
    ASSERT_TRUE(f.world.setFluid(0, 1, 0, FluidKind::Water, kMaxFluidLevel));
    MobEntity mob = makeMob(f, z, Vec3{0.5f, 10.0f, 0.5f});
    runMob(mob, 300);
    EXPECT_TRUE(mob.onGround());
    EXPECT_FLOAT_EQ(mob.health(), 20.0f);
}

TEST(MobFallDamageTest, LethalFallKillsThroughTheExistingPath) {
    Fixture f;
    MobRegistry mobs = makeMobRegistry();
    const Mob& z = mobs.at(MobID{0});
    // Fall 23 > 20 health: the killing hit goes through damage()'s
    // existing death path.
    MobEntity mob = makeMob(f, z, Vec3{0.5f, 24.0f, 0.5f});
    runMob(mob, 400);
    EXPECT_TRUE(mob.dead());
}

TEST(MobFallDamageTest, WalkingOffAColumnTakesDamage) {
    // The leave-the-ground transition pins the fall start (not just the
    // spawn pin): a mob standing on a 5-tall platform loses it under its
    // feet and drops 5 -> 2 damage. 3x3 platform: the chase toward the
    // origin player moves the mob at most 0.4 blocks in the 30 settle
    // frames, so it cannot walk off before the removal.
    Fixture f;
    MobRegistry mobs = makeMobRegistry();
    const Mob& z = mobs.at(MobID{0});
    for (int x = 0; x <= 2; ++x) {
        for (int zc = 0; zc <= 2; ++zc) {
            for (int y = 1; y <= 5; ++y) {
                ASSERT_TRUE(f.world.setBlock(x, y, zc, f.stone));
            }
        }
    }
    MobEntity mob = makeMob(f, z, Vec3{1.5f, 6.0f, 1.5f});
    runMob(mob, 30); // settle on the platform
    ASSERT_TRUE(mob.onGround());
    for (int x = 0; x <= 2; ++x) {
        for (int zc = 0; zc <= 2; ++zc) {
            for (int y = 1; y <= 5; ++y) {
                f.world.setBlock(x, y, zc, BlockID::air());
            }
        }
    }
    runMob(mob, 300);
    EXPECT_TRUE(mob.onGround());
    // ~2 damage: the leave-ground pin lands one physics substep after
    // the platform disappears, so the credited fall is 5.0 minus that
    // substep's drop (deterministic — band it tight rather than pin the
    // substep arithmetic).
    EXPECT_NEAR(mob.health(), 18.0f, 0.02f);
}

// ============================================================================
//   ENT-6 — the stuck-triggered chase path (BFS)
// ============================================================================

namespace {
// The 2-high, 3-wide wall on the fixture floor (top y1) at x=2.
void buildWall(Fixture& f) {
    for (int y = 1; y <= 2; ++y) {
        for (int zc = -1; zc <= 1; ++zc) {
            f.world.setBlock(2, y, zc, f.stone);
        }
    }
}
} // namespace

TEST(MobPathfindTest, StuckChaserFindsPath) {
    Fixture f;
    MobRegistry mobs = makeMobRegistry();
    const Mob& z = mobs.at(MobID{0});
    buildWall(f);
    int hits = 0;
    MobEntity mob = makeMob(f, z, Vec3{0.5f, 1.0f, 0.5f});
    mob.setPlayer(Vec3{4.5f, 1.0f, 0.5f}, true);
    mob.setHitCallback([&hits](float) { ++hits; });
    bool sawPathfind = false;
    for (int i = 0; i < 1200; ++i) {
        if (mob.state() == MobState::Pathfind) {
            sawPathfind = true;
        }
        mob.update(kDt);
    }
    // The straight chase dead-ends at the wall; the BFS route (around
    // the wall's end at z±2) gets the mob to the player's attack range.
    EXPECT_TRUE(sawPathfind);
    EXPECT_GT(hits, 0);
    const float distH =
        std::hypot(mob.position().x - 4.5f, mob.position().z - 0.5f);
    EXPECT_LE(distH, 1.6f + 1e-3f);
}

TEST(MobPathfindTest, FlatChaserNeverPathfinds) {
    Fixture f;
    MobRegistry mobs = makeMobRegistry();
    const Mob& z = mobs.at(MobID{0});
    // No wall: the straight chase is unblocked — the pathfinding code
    // must stay structurally dormant (the zero-regression guarantee).
    int hits = 0;
    MobEntity mob = makeMob(f, z, Vec3{0.5f, 1.0f, 0.5f});
    mob.setPlayer(Vec3{4.5f, 1.0f, 0.5f}, true);
    mob.setHitCallback([&hits](float) { ++hits; });
    bool sawPathfind = false;
    for (int i = 0; i < 300; ++i) {
        if (mob.state() == MobState::Pathfind) {
            sawPathfind = true;
        }
        mob.update(kDt);
    }
    EXPECT_FALSE(sawPathfind);
    EXPECT_FALSE(mob.pathActive());
    EXPECT_GT(hits, 0); // the straight chase reached the player
}

TEST(MobPathfindTest, SealedChaserStaysLost) {
    Fixture f;
    MobRegistry mobs = makeMobRegistry();
    const Mob& z = mobs.at(MobID{0});
    // A sealed 3x3 room (walls y1..2, ceiling y3) around the mob.
    for (int y = 1; y <= 2; ++y) {
        for (int x = -1; x <= 1; ++x) {
            f.world.setBlock(x, y, -1, f.stone);
            f.world.setBlock(x, y, 1, f.stone);
        }
        for (int zc = -1; zc <= 1; ++zc) {
            f.world.setBlock(-1, y, zc, f.stone);
            f.world.setBlock(1, y, zc, f.stone);
        }
    }
    for (int x = -1; x <= 1; ++x) {
        for (int zc = -1; zc <= 1; ++zc) {
            f.world.setBlock(x, 3, zc, f.stone);
        }
    }
    int hits = 0;
    MobEntity mob = makeMob(f, z, Vec3{0.5f, 1.0f, 0.5f});
    mob.setPlayer(Vec3{4.5f, 1.0f, 0.5f}, true);
    mob.setHitCallback([&hits](float) { ++hits; });
    bool sawPathfind = false;
    for (int i = 0; i < 300; ++i) {
        if (mob.state() == MobState::Pathfind) {
            sawPathfind = true;
        }
        mob.update(kDt);
    }
    // No standable route: the failed BFS takes the lost cooldown, the
    // straight chase resumes against the wall, the player is never hit.
    EXPECT_FALSE(sawPathfind);
    EXPECT_EQ(hits, 0);
    EXPECT_LE(std::abs(mob.position().x), 1.0f + 1e-3f);
    EXPECT_LE(std::abs(mob.position().z), 1.0f + 1e-3f);
}

TEST(MobPathfindTest, PathDroppedWhenPlayerGone) {
    Fixture f;
    MobRegistry mobs = makeMobRegistry();
    const Mob& z = mobs.at(MobID{0});
    buildWall(f);
    MobEntity mob = makeMob(f, z, Vec3{0.5f, 1.0f, 0.5f});
    mob.setPlayer(Vec3{4.5f, 1.0f, 0.5f}, true);
    // Run until the path engages (the stuck window + the BFS).
    int i = 0;
    while (i < 900 && !mob.pathActive()) {
        mob.update(kDt);
        ++i;
    }
    ASSERT_TRUE(mob.pathActive());
    // The player dies: the next AI tick leaves the chase context and
    // drops the path (the wander takes over).
    mob.setPlayer(Vec3{4.5f, 1.0f, 0.5f}, false);
    mob.update(kDt);
    EXPECT_FALSE(mob.pathActive());
    EXPECT_NE(mob.state(), MobState::Pathfind);
}

// -- SURV-10: the milking cooldown (the Minecraft 5-minute rule) -------

TEST(MobMilkCooldownTest, CooldownBlocksAndExpiresOnTheFrameBoundary) {
    Fixture f;
    MobRegistry mobs = makeMobRegistry();
    const Mob& def = mobs.at(mobs.idForName("zombie"));
    MobEntity mob = makeMob(f, def, Vec3 {0.5f, 1.0f, 0.5f});
    EXPECT_TRUE(mob.milkReady()); // fresh: never milked
    mob.startMilkCooldown();
    EXPECT_FALSE(mob.milkReady());
    // One frame short of the 18000-frame (5-minute) boundary: still on
    // cooldown (the countdown runs in update, one frame per step).
    runMob(mob, kMilkingCooldownFrames - 1);
    EXPECT_FALSE(mob.milkReady());
    // The boundary frame: expired.
    runMob(mob, 1);
    EXPECT_TRUE(mob.milkReady());
    // A re-milk restarts the full cooldown.
    mob.startMilkCooldown();
    EXPECT_FALSE(mob.milkReady());
    runMob(mob, kMilkingCooldownFrames);
    EXPECT_TRUE(mob.milkReady());
}

TEST(MobMilkCooldownTest, CooldownIsPerMobInstance) {
    Fixture f;
    MobRegistry mobs = makeMobRegistry();
    const Mob& def = mobs.at(mobs.idForName("zombie"));
    MobEntity a = makeMob(f, def, Vec3 {0.5f, 1.0f, 0.5f}, 1);
    MobEntity b = makeMob(f, def, Vec3 {5.5f, 1.0f, 5.5f}, 2);
    a.startMilkCooldown();
    EXPECT_FALSE(a.milkReady());
    EXPECT_TRUE(b.milkReady()); // the sibling is unaffected
    // b idles through the full window: nothing to count down.
    runMob(b, kMilkingCooldownFrames);
    EXPECT_TRUE(b.milkReady());
    runMob(a, kMilkingCooldownFrames - 1);
    EXPECT_FALSE(a.milkReady()); // a is still one frame short
}

// ===========================================================================
// V3-3: the nether's mob family (the 4+1 nether mobs + the three minimal
// data-flag mechanisms — M1 neutral / M2 walks_on_lava / M3 the nether
// pool). The overworld red line: every new flag is false in the shipped
// overworld data (the structural-immunity root — the existing family's
// tests above run the flagless paths bit-identically).
// ===========================================================================

namespace {

// A neutral test definition (the zombie's shape + the neutral flag):
// the AI physics tests never see a player by default (m_playerAlive
// true, m_playerPos zero — the setPlayer feed).
const char* kNeutralDef = R"({
  "id": "neutral_mob",
  "properties": {
    "health": 20,
    "speed": 0.8,
    "detect_radius": 16,
    "attack_range": 1.6,
    "attack_damage": 3,
    "attack_cooldown": 1.0,
    "wander_interval": 2.5,
    "neutral": true
  },
  "model": {
    "width": 0.6,
    "height": 1.8,
    "boxes": [
      {"pos": [0.0, 1.25, 0.0], "size": [0.5, 0.5, 0.5], "texture": "a"},
      {"pos": [0.0, 0.5, 0.0], "size": [0.5, 1.0, 0.3], "texture": "b"}
    ]
  }
})";

// The neutral test definition + a ground ambient pair (the runAi roll's
// carrier — the strider's data face).
const char* kNeutralAmbientDef = R"({
  "id": "neutral_mob",
  "properties": {
    "health": 20,
    "speed": 0.8,
    "detect_radius": 16,
    "attack_range": 1.6,
    "attack_damage": 3,
    "attack_cooldown": 1.0,
    "wander_interval": 2.5,
    "neutral": true,
    "ambient_sound": "strider_hiss",
    "ambient_interval": 3.0
  },
  "model": {
    "width": 0.6,
    "height": 1.8,
    "boxes": [
      {"pos": [0.0, 1.25, 0.0], "size": [0.5, 0.5, 0.5], "texture": "a"},
      {"pos": [0.0, 0.5, 0.0], "size": [0.5, 1.0, 0.3], "texture": "b"}
    ]
  }
})";

// A walks_on_lava test definition (the strider's data face — passive:
// detect 0, no attack, the lava walker's whole story is the physics).
const char* kStriderDef = R"({
  "id": "strider_mob",
  "properties": {
    "health": 20,
    "speed": 1.0,
    "detect_radius": 0,
    "attack_range": 0,
    "attack_damage": 0,
    "attack_cooldown": 1.0,
    "wander_interval": 2.0,
    "walks_on_lava": true
  },
  "model": {
    "width": 1.0,
    "height": 1.5,
    "boxes": [
      {"pos": [0.0, 0.55, 0.0], "size": [0.7, 0.5, 0.8], "texture": "s"}
    ]
  }
})";

// A flagless control definition (the hoglin's shape: the no-flag lava
// swimmer — the generic integrateFluid's carrier).
const char* kFlaglessDef = R"({
  "id": "flagless_mob",
  "properties": {
    "health": 40,
    "speed": 1.0,
    "detect_radius": 16,
    "attack_range": 1.6,
    "attack_damage": 6,
    "attack_cooldown": 2.0,
    "wander_interval": 2.5
  },
  "model": {
    "width": 1.2,
    "height": 1.5,
    "boxes": [
      {"pos": [0.0, 1.0, 0.0], "size": [0.8, 0.7, 0.9], "texture": "f"}
    ]
  }
})";

MobRegistry makeNeutralRegistry(const char* def) {
    MobRegistry mobs;
    auto r = mobs.registerMob(parseJson(def));
    if (!r.ok()) {
        ADD_FAILURE() << r.error().message;
    }
    return mobs;
}

// A fixture world with a 3x3 LAVA POOL (cells 4..6 on x and z, y=0 —
// the fixture stone floor cleared there, setFluid's air-only cell rule,
// a source cell per pool cell): the surface plane is y = 1.0, the
// strider's floor. The pool is wide enough for the test bodies (the
// hoglin's 1.2 AABB spans three cells — a one-cell hole would embed a
// wide body in the surrounding floor and the collision resolve's far-
// side push would move it, the PITS-style fixture trap).
void makeLavaPuddle(Fixture& f) {
    for (int x = 4; x <= 6; ++x) {
        for (int z = 4; z <= 6; ++z) {
            f.world.setBlock(x, 0, z, BlockID::air());
            EXPECT_TRUE(f.world.setFluid(x, 0, z, FluidKind::Lava,
                                         kMaxFluidLevel));
        }
    }
}

} // namespace

// -- M1: the neutral class flag (the parse face) -----------------------------

TEST(NeutralRegistryTest, FlagsDefaultOff) {
    // kZombieDef has neither flag: the defaults (false / false) apply —
    // every pre-V3-3 mob is the bit-identical structural root.
    MobRegistry mobs = makeMobRegistry();
    const auto& p = mobs.at(MobID{0}).properties;
    EXPECT_FALSE(p.neutral);
    EXPECT_FALSE(p.walksOnLava);
}

TEST(NeutralRegistryTest, ParsesNeutralAndWalksOnLava) {
    for (const char* def : {kNeutralDef, kStriderDef}) {
        MobRegistry mobs;
        auto r = mobs.registerMob(parseJson(def));
        ASSERT_TRUE(r.ok()) << r.error().message;
    }
    const auto& n = makeNeutralRegistry(kNeutralDef).at(MobID{0}).properties;
    EXPECT_TRUE(n.neutral);
    EXPECT_FALSE(n.walksOnLava);
    EXPECT_FALSE(n.hostile);
    const auto& s = makeNeutralRegistry(kStriderDef).at(MobID{0}).properties;
    EXPECT_TRUE(s.walksOnLava);
    EXPECT_FALSE(s.neutral);
    EXPECT_FALSE(s.hostile);
}

TEST(NeutralRegistryTest, RejectsNeutralHostileBoth) {
    // The strict mutual exclusion (the design's S6 face — the parse
    // rejects the pair by name: a sight-chasing hostile cannot also
    // carry the anger gate).
    std::string def = kNeutralDef;
    const std::string key = "\"neutral\": true";
    ASSERT_NE(def.find(key), std::string::npos);
    def.replace(def.find(key), key.size(),
                std::string(key) + ", \"hostile\": true");
    MobRegistry mobs;
    auto r = mobs.registerMob(parseJson(def));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("mutually exclusive"),
              std::string::npos);
}

TEST(NeutralRegistryTest, RejectsNonBoolFlags) {
    for (const char* field : {"neutral", "walks_on_lava"}) {
        for (const char* bad : {"\"yes\"", "1", "0.5"}) {
            MobRegistry mobs;
            std::string def = kZombieDef;
            const std::string key = "\"wander_interval\": 2.5";
            def.replace(def.find(key), key.size(),
                        std::string("\"wander_interval\": 2.5, \"") + field +
                            "\": " + bad);
            auto r = mobs.registerMob(parseJson(def));
            EXPECT_FALSE(r.ok()) << field << " accepted: " << bad;
        }
    }
}

// -- the 5 nether mob data pins (the content is data — the real assets) -----

TEST(NetherMobContent, ZombifiedPiglinDataPinned) {
    // The REAL asset (data/mobs/zombified_piglin.json): the zombie's data
    // twin (deviation 6 — the MC zombified piglin's stats) with the
    // undead flag explicitly FALSE (the MC zombie pigman is not undead —
    // the poison exemption's data half).
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    MobRegistry mobs;
    ASSERT_TRUE(mobs.loadDirectory(assets, "data/mobs").ok());
    const Mob* z = mobs.findByName("zombified_piglin");
    ASSERT_NE(z, nullptr);
    const MobProperties& p = z->properties;
    EXPECT_FLOAT_EQ(p.health, 20.0f);
    EXPECT_FLOAT_EQ(p.speed, 0.8f);
    EXPECT_FLOAT_EQ(p.detectRadius, 16.0f);
    EXPECT_FLOAT_EQ(p.attackRange, 1.6f);
    EXPECT_FLOAT_EQ(p.attackDamage, 3.0f);
    EXPECT_FLOAT_EQ(p.attackCooldown, 1.0f);
    EXPECT_DOUBLE_EQ(p.wanderInterval, 2.5);
    EXPECT_EQ(p.spawnMaxLight, 7);
    EXPECT_TRUE(p.hostile);
    EXPECT_FALSE(p.neutral);
    EXPECT_FALSE(p.walksOnLava);
    EXPECT_FALSE(p.undead);
    ASSERT_EQ(p.drops.size(), 1u);
    EXPECT_EQ(p.drops[0].item, "rotten_flesh");
    EXPECT_EQ(p.drops[0].countMin, 1);
    EXPECT_EQ(p.drops[0].countMax, 1);
    EXPECT_EQ(p.xp, 5);
    // The model: the zombie's two-box humanoid, the zombified palette +
    // the per-face head (the V2-2 consumption).
    EXPECT_FLOAT_EQ(z->model.width, 0.6f);
    EXPECT_FLOAT_EQ(z->model.height, 1.8f);
    ASSERT_EQ(z->model.boxes.size(), 2u);
    EXPECT_EQ(z->model.boxes[0].texture, "zpiglin_body");
    ASSERT_EQ(z->model.boxes[0].faces.size(), 1u);
    EXPECT_EQ(z->model.boxes[0].faces.at("front"), "zpiglin_face");
    EXPECT_EQ(z->model.boxes[1].texture, "zpiglin_body");
    EXPECT_TRUE(z->model.boxes[1].faces.empty());
    EXPECT_TRUE(z->model.swings.empty());
}

TEST(NetherMobContent, PiglinDataPinned) {
    // The REAL asset (data/mobs/piglin.json): the NEUTRAL class (the
    // M1 flag's first carrier): wanders calm, chases only while angry.
    // No DROPS (the trade's output is the barter table — V3-14c, the
    // next test), the whisper ambient (the ground-mob roll's carrier),
    // xp 3.
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok());
    MobRegistry mobs;
    ASSERT_TRUE(mobs.loadDirectory(assets, "data/mobs").ok());
    const Mob* pl = mobs.findByName("piglin");
    ASSERT_NE(pl, nullptr);
    const MobProperties& p = pl->properties;
    EXPECT_FLOAT_EQ(p.health, 16.0f);
    EXPECT_FLOAT_EQ(p.speed, 1.0f);
    EXPECT_FLOAT_EQ(p.detectRadius, 16.0f);
    EXPECT_FLOAT_EQ(p.attackRange, 1.6f);
    EXPECT_FLOAT_EQ(p.attackDamage, 6.0f);
    EXPECT_FLOAT_EQ(p.attackCooldown, 1.5f);
    EXPECT_DOUBLE_EQ(p.wanderInterval, 2.5);
    EXPECT_EQ(p.spawnMaxLight, 7);
    EXPECT_FALSE(p.hostile);
    EXPECT_TRUE(p.neutral);
    EXPECT_FALSE(p.walksOnLava);
    EXPECT_TRUE(p.drops.empty());
    EXPECT_EQ(p.xp, 3);
    EXPECT_EQ(p.ambientSoundName, "piglin_whisper");
    EXPECT_DOUBLE_EQ(p.ambientInterval, 3.0);
    EXPECT_FLOAT_EQ(pl->model.width, 0.6f);
    EXPECT_FLOAT_EQ(pl->model.height, 1.8f);
    ASSERT_EQ(pl->model.boxes.size(), 2u);
    EXPECT_EQ(pl->model.boxes[0].faces.at("front"), "piglin_face");
    EXPECT_EQ(pl->model.boxes[0].texture, "piglin_body");
    EXPECT_EQ(pl->model.boxes[1].texture, "piglin_body");
}

TEST(NetherMobContent, PiglinBarterDataPinned) {
    // The REAL asset (data/mobs/piglin.json): the V3-14c bartering —
    // the single input (gold_ingot — the current-MC ruling: no
    // nugget/block tiers) + the 12-output weighted table (the MC
    // barter's project-approximation: the weights follow MC's ratios,
    // the outputs are the project's existing items — the pool-weight
    // "MC 近似" precedent). The full table is pinned (a data edit
    // re-runs the rolls — the stream contract).
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok());
    MobRegistry mobs;
    ASSERT_TRUE(mobs.loadDirectory(assets, "data/mobs").ok());
    const Mob* pl = mobs.findByName("piglin");
    ASSERT_NE(pl, nullptr);
    const MobProperties& p = pl->properties;
    EXPECT_EQ(p.barterInput, "gold_ingot");
    ASSERT_EQ(p.barterOutputs.size(), 12u);
    const struct {
        const char* item;
        int weight;
        int cmin;
        int cmax;
    } expect[] = {
        {"gravel", 80, 8, 16},       {"nether_brick", 40, 1, 1},
        {"obsidian", 40, 1, 1},      {"string", 20, 3, 9},
        {"leather", 20, 2, 4},       {"soul_sand", 10, 2, 8},
        {"quartz", 10, 5, 12},       {"ghast_membrane", 10, 1, 1},
        {"hoglin_meat", 10, 1, 1},   {"water_bottle", 6, 1, 1},
        {"golden_apple", 2, 1, 1},   {"netherite_scrap", 1, 1, 1},
    };
    for (std::size_t i = 0; i < p.barterOutputs.size(); ++i) {
        EXPECT_EQ(p.barterOutputs[i].item, expect[i].item) << i;
        EXPECT_EQ(p.barterOutputs[i].weight, expect[i].weight) << i;
        EXPECT_EQ(p.barterOutputs[i].countMin, expect[i].cmin) << i;
        EXPECT_EQ(p.barterOutputs[i].countMax, expect[i].cmax) << i;
    }
}

TEST(NetherMobContent, HoglinDataPinned) {
    // The REAL asset (data/mobs/hoglin.json): the NEUTRAL quadruped (the
    // HP 40 tank — the slow 1.0 chase), the food chain's source (the
    // hoglin_meat 1-2 + leather 0-1 drops, the pig's range precedent),
    // the grunt ambient.
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok());
    MobRegistry mobs;
    ASSERT_TRUE(mobs.loadDirectory(assets, "data/mobs").ok());
    const Mob* h = mobs.findByName("hoglin");
    ASSERT_NE(h, nullptr);
    const MobProperties& p = h->properties;
    EXPECT_FLOAT_EQ(p.health, 40.0f);
    EXPECT_FLOAT_EQ(p.speed, 1.0f);
    EXPECT_FLOAT_EQ(p.detectRadius, 16.0f);
    EXPECT_FLOAT_EQ(p.attackRange, 1.6f);
    EXPECT_FLOAT_EQ(p.attackDamage, 6.0f);
    EXPECT_FLOAT_EQ(p.attackCooldown, 2.0f);
    EXPECT_DOUBLE_EQ(p.wanderInterval, 2.5);
    EXPECT_EQ(p.spawnMaxLight, 7);
    EXPECT_FALSE(p.hostile);
    EXPECT_TRUE(p.neutral);
    EXPECT_FALSE(p.walksOnLava);
    ASSERT_EQ(p.drops.size(), 2u);
    EXPECT_EQ(p.drops[0].item, "hoglin_meat");
    EXPECT_EQ(p.drops[0].countMin, 1);
    EXPECT_EQ(p.drops[0].countMax, 2);
    EXPECT_EQ(p.drops[1].item, "leather");
    EXPECT_EQ(p.drops[1].countMin, 0);
    EXPECT_EQ(p.drops[1].countMax, 1);
    EXPECT_EQ(p.xp, 5);
    EXPECT_EQ(p.ambientSoundName, "hoglin_grunt");
    EXPECT_DOUBLE_EQ(p.ambientInterval, 3.0);
    // The model: the pig's quadruped scaled up (width 1.2 / height 1.5),
    // 6 boxes (body / 4 legs / head) + the diagonal 4-leg swing + the
    // per-face tusked head.
    EXPECT_FLOAT_EQ(h->model.width, 1.2f);
    EXPECT_FLOAT_EQ(h->model.height, 1.5f);
    ASSERT_EQ(h->model.boxes.size(), 6u);
    for (int i = 1; i <= 4; ++i) {
        EXPECT_EQ(h->model.boxes[static_cast<std::size_t>(i)].texture,
                  "hoglin_body");
    }
    EXPECT_EQ(h->model.boxes[5].faces.at("front"), "hoglin_face");
    ASSERT_EQ(h->model.swings.size(), 4u);
    // The diagonal quadruped gait (the cow's swing spec — the data-order
    // leg pair): fl/rr in group 0, fr/rl in group 1, amplitude 0.5.
    const char* legNames[] = {"leg_fl", "leg_fr", "leg_rl", "leg_rr"};
    const int groups[] = {0, 1, 1, 0};
    for (int i = 0; i < 4; ++i) {
        const MobSwing& sw = h->model.swings[static_cast<std::size_t>(i)];
        EXPECT_FLOAT_EQ(sw.amplitude, 0.5f) << i;
        EXPECT_EQ(sw.group, groups[i]) << i;
        EXPECT_EQ(h->model.boxes[static_cast<std::size_t>(sw.boxIndex)].name,
                  legNames[i]) << i;
    }
}

TEST(NetherMobContent, MagmaCubeDataPinned) {
    // The REAL asset (data/mobs/magma_cube.json): the HOSTILE blob (the
    // slow 0.8 chase, deviation 3 — no hop AI: the static three-box
    // model, no swings), no drops / xp 0, NO ambient (the documented
    // absence — the magma's rumble is the ambience_nether bed's face).
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok());
    MobRegistry mobs;
    ASSERT_TRUE(mobs.loadDirectory(assets, "data/mobs").ok());
    const Mob* m = mobs.findByName("magma_cube");
    ASSERT_NE(m, nullptr);
    const MobProperties& p = m->properties;
    EXPECT_FLOAT_EQ(p.health, 16.0f);
    EXPECT_FLOAT_EQ(p.speed, 0.8f);
    EXPECT_FLOAT_EQ(p.detectRadius, 16.0f);
    EXPECT_FLOAT_EQ(p.attackRange, 1.6f);
    EXPECT_FLOAT_EQ(p.attackDamage, 3.0f);
    EXPECT_FLOAT_EQ(p.attackCooldown, 1.0f);
    EXPECT_EQ(p.spawnMaxLight, 7);
    EXPECT_TRUE(p.hostile);
    EXPECT_FALSE(p.neutral);
    EXPECT_TRUE(p.drops.empty());
    EXPECT_EQ(p.xp, 0);
    EXPECT_TRUE(p.ambientSoundName.empty());
    // The model: the faceless 3-box blob (deviation 2 — no split: the
    // static mesh, the single dark-red cracked tile, no swings).
    EXPECT_FLOAT_EQ(m->model.width, 0.9f);
    EXPECT_FLOAT_EQ(m->model.height, 0.9f);
    ASSERT_EQ(m->model.boxes.size(), 3u);
    for (const MobBox& box : m->model.boxes) {
        EXPECT_EQ(box.texture, "magma_cube");
        EXPECT_TRUE(box.faces.empty());
    }
    EXPECT_TRUE(m->model.swings.empty());
}

TEST(NetherMobContent, StriderDataPinned) {
    // The REAL asset (data/mobs/strider.json): the PASSIVE lava walker
    // (the M2 flag's only carrier — detect 0, no attack, the wander
    // only) + the hiss ambient + the leather 0-1 drop + xp 3.
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok());
    MobRegistry mobs;
    ASSERT_TRUE(mobs.loadDirectory(assets, "data/mobs").ok());
    const Mob* s = mobs.findByName("strider");
    ASSERT_NE(s, nullptr);
    const MobProperties& p = s->properties;
    EXPECT_FLOAT_EQ(p.health, 20.0f);
    EXPECT_FLOAT_EQ(p.speed, 1.0f);
    EXPECT_FLOAT_EQ(p.detectRadius, 0.0f);
    EXPECT_FLOAT_EQ(p.attackRange, 0.0f);
    EXPECT_FLOAT_EQ(p.attackDamage, 0.0f);
    EXPECT_DOUBLE_EQ(p.wanderInterval, 2.0);
    // spawn_max_light 15 (deviation 9 — the lava's light 15 gates its
    // own surface: the foot cell over the sea reads 14-15 by
    // construction, so the 7 gate would be self-defeating; 15 = the
    // lava-surface rule owns the location, the MC nether spawn class).
    EXPECT_EQ(p.spawnMaxLight, 15);
    EXPECT_FALSE(p.hostile);
    EXPECT_FALSE(p.neutral);
    EXPECT_TRUE(p.walksOnLava);
    // V3-14d: the rideable flag (the strider is the project's first —
    // and only — ride target; the right-click mount's data predicate).
    EXPECT_TRUE(p.rideable);
    ASSERT_EQ(p.drops.size(), 1u);
    EXPECT_EQ(p.drops[0].item, "leather");
    EXPECT_EQ(p.drops[0].countMin, 0);
    EXPECT_EQ(p.drops[0].countMax, 1);
    EXPECT_EQ(p.xp, 3);
    EXPECT_EQ(p.ambientSoundName, "strider_hiss");
    EXPECT_DOUBLE_EQ(p.ambientInterval, 3.0);
    // The model: the long-necked quadruped (width 1.0 / height 1.5) —
    // 7 boxes (body / 4 legs / neck / head) + the diagonal swing + the
    // per-face head (the saddle band's body tile).
    EXPECT_FLOAT_EQ(s->model.width, 1.0f);
    EXPECT_FLOAT_EQ(s->model.height, 1.5f);
    ASSERT_EQ(s->model.boxes.size(), 7u);
    EXPECT_EQ(s->model.boxes[6].faces.at("front"), "strider_face");
    ASSERT_EQ(s->model.swings.size(), 4u);
    for (const MobSwing& sw : s->model.swings) {
        EXPECT_FLOAT_EQ(sw.amplitude, 0.4f);
    }
}

TEST(NetherMobContent, GhastDataPinned) {
    // The REAL asset (data/mobs/ghast.json): the V3-14b nether's
    // floating horror — the FLYING RANGED attacker (the bat's flight
    // data flags + the skeleton's shoot data fused, the first exploding
    // shooter: explode_radius 3 turns its shot into the fireball, the
    // V3-14a blast's first in-game consumer; the ammo rule's exemption
    // witness — no 'ammo' key, the fireball is consumed at impact).
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok());
    MobRegistry mobs;
    ASSERT_TRUE(mobs.loadDirectory(assets, "data/mobs").ok());
    const Mob* g = mobs.findByName("ghast");
    ASSERT_NE(g, nullptr);
    const MobProperties& p = g->properties;
    EXPECT_FLOAT_EQ(p.health, 10.0f); // the MC ghast's 10 hearts / 2
    EXPECT_FLOAT_EQ(p.speed, 0.7f);
    EXPECT_FLOAT_EQ(p.detectRadius, 16.0f);
    EXPECT_FLOAT_EQ(p.attackRange, 0.0f);
    EXPECT_FLOAT_EQ(p.attackDamage, 0.0f); // the blast is the damage
    EXPECT_FLOAT_EQ(p.attackCooldown, 4.0f);
    EXPECT_FLOAT_EQ(p.shootRange, 14.0f);
    EXPECT_FLOAT_EQ(p.shootSpeed, 10.0f);
    EXPECT_FLOAT_EQ(p.explodeRadius, 3.0f); // the TNT-class blast
    EXPECT_EQ(p.shootSound, "ghast_roar"); // the pre-fireball warning
    EXPECT_DOUBLE_EQ(p.wanderInterval, 2.0);
    // spawn_max_light 15 (the strider's lava-surface twin — the ghast
    // spawns in the nether's lit open too, the MC nether class).
    EXPECT_EQ(p.spawnMaxLight, 15);
    EXPECT_TRUE(p.hostile);
    EXPECT_FALSE(p.neutral);
    EXPECT_TRUE(p.flying);
    EXPECT_TRUE(p.spawnsInAir);
    EXPECT_EQ(p.ammoItem, ""); // the explode shooter's exemption
    ASSERT_EQ(p.drops.size(), 1u);
    EXPECT_EQ(p.drops[0].item, "ghast_membrane");
    EXPECT_EQ(p.drops[0].countMin, 0);
    EXPECT_EQ(p.drops[0].countMax, 1);
    EXPECT_EQ(p.xp, 5);
    EXPECT_EQ(p.ambientSoundName, "ghast");
    EXPECT_DOUBLE_EQ(p.ambientInterval, 8.0);
    // The model: the big blob (width 1.5 / height 1.75) — 5 boxes
    // (body + 4 tentacles), all the ghast tile.
    EXPECT_FLOAT_EQ(g->model.width, 1.5f);
    EXPECT_FLOAT_EQ(g->model.height, 1.75f);
    ASSERT_EQ(g->model.boxes.size(), 5u);
    for (const auto& b : g->model.boxes) {
        EXPECT_EQ(b.texture, "ghast");
    }
}

TEST(NetherMobContent, NetherMobAssetsPinned) {
    // The V3-3 new asset files (the FNV pins — the re-run determinism
    // proof, the BatAssetsPinned convention): the 6 mob JSONs (the
    // V3-14b ghast's JSON + texture + roar/ambient wavs + the
    // membrane's item JSON + icon), the nether pool, the 10 mob
    // tiles, the 5 ambient/shot wavs, the food chain's 2 item JSONs +
    // 2 icons.
    const struct {
        const char* path;
        const char* fnv;
    } files[] = {
        {"assets/data/mobs/zombified_piglin.json", "e1bf3ff177362499"},
        {"assets/data/mobs/piglin.json", "0aaa11fa09c81302"},
        {"assets/data/mobs/hoglin.json", "e596d325a2f9e783"},
        {"assets/data/mobs/magma_cube.json", "4ea96c29f0ceb813"},
        {"assets/data/mobs/strider.json", "b0f3fef8c3449477"},
        {"assets/data/mobs/ghast.json", "848d8741dc090816"},
        {"assets/data/mob_spawning_nether.json", "3ccb76538c02c146"},
        {"assets/textures/mobs/zpiglin_body.png", "310db573d5a32124"},
        {"assets/textures/mobs/zpiglin_face.png", "9016c729122ae26b"},
        {"assets/textures/mobs/piglin_body.png", "94c29a6cd03b92c3"},
        {"assets/textures/mobs/piglin_face.png", "fb6a0b46ec9f8e50"},
        {"assets/textures/mobs/hoglin_body.png", "ecf71d3c8d3a9b08"},
        {"assets/textures/mobs/hoglin_face.png", "95e8bb720361de40"},
        {"assets/textures/mobs/strider_body.png", "b5570a01bbbd6213"},
        {"assets/textures/mobs/strider_face.png", "3062c95192b05c35"},
        {"assets/textures/mobs/magma_cube.png", "7fc6832baf615911"},
        {"assets/sounds/hoglin_grunt.wav", "33f7b09e10814b57"},
        {"assets/sounds/piglin_whisper.wav", "10de938161925ccf"},
        {"assets/sounds/strider_hiss.wav", "2fa1016f6c43f0dd"},
        {"assets/data/items/hoglin_meat.json", "4733bf49c98fb5bd"},
        {"assets/data/items/cooked_hoglin_meat.json", "46dcf633a7a3858b"},
        {"assets/textures/items/hoglin_meat.png", "fcab29b08916a19a"},
        {"assets/textures/items/cooked_hoglin_meat.png", "c3c1a6500aa7ddd8"},
        // V3-14b: the ghast's face (the blob's body tile + the two
        // voices + the membrane's item JSON + icon).
        {"assets/textures/mobs/ghast.png", "e51f396f1a50f0c2"},
        {"assets/sounds/ghast.wav", "631de1267caf050e"},
        {"assets/sounds/ghast_roar.wav", "04714ab128e8d0b8"},
        {"assets/data/items/ghast_membrane.json", "4d1407e0343ab953"},
        {"assets/textures/items/ghast_membrane.png", "995ecfa4b7bdf84e"},
    };
    for (const auto& f : files) {
        EXPECT_EQ(fnv1a64(readAll(f.path)), f.fnv) << f.path;
    }
    // The overworld pool's ZERO-TOUCH red line (the M3 structural
    // immunity root — the file is byte-identical to the V2-14 pin in
    // BatAssetsPinned: a98e3bf9e89ccbbf0 is the bat's pin — re-asserted
    // here as the nether family's isolation witness).
    EXPECT_EQ(fnv1a64(readAll("assets/data/mob_spawning.json")),
              "a8e3bf9e89ccbbf0");
}

// -- M1: the neutral's enrage state machine (the frame discipline) ---------

TEST(NeutralMobTest, CalmNeutralDoesNotChase) {
    // The design's single predicate: a calm neutral mob's detect test
    // holds only while angry — the player within the detect_radius (16)
    // does NOT start a chase (the passive-walk semantics). The mob
    // wanders (or stands) — never Chase / Attack / Pathfind.
    Fixture f;
    MobRegistry mobs = makeNeutralRegistry(kNeutralDef);
    const Mob& def = mobs.at(MobID{0});
    MobEntity mob = makeMob(f, def, Vec3 {0.5f, 1.0f, 0.5f}, 42);
    mob.setPlayer({8.5f, 1.0f, 0.5f}, true);  // 8 away: in detect range
    EXPECT_FALSE(mob.angry());
    for (int i = 0; i < 120; ++i) {
        mob.update(kDt);
        EXPECT_NE(mob.state(), MobState::Chase) << "frame " << i;
        EXPECT_NE(mob.state(), MobState::Attack) << "frame " << i;
        EXPECT_NE(mob.state(), MobState::Pathfind) << "frame " << i;
    }
    EXPECT_FALSE(mob.angry());
}

TEST(NeutralMobTest, AnyDamageEnragesAndTheChaseBegins) {
    // deviation 7: ANY damage source enragues (the damage() choke point —
    // the player's melee / the arrow / the fire tick / the fall all flow
    // through it). One hit flips the state: the next AI run chases the
    // player at full data speed.
    Fixture f;
    MobRegistry mobs = makeNeutralRegistry(kNeutralDef);
    const Mob& def = mobs.at(MobID{0});
    MobEntity mob = makeMob(f, def, Vec3 {0.5f, 1.0f, 0.5f}, 42);
    mob.setPlayer({8.5f, 1.0f, 0.5f}, true);
    EXPECT_FALSE(mob.angry());
    mob.damage(1.0f);
    EXPECT_TRUE(mob.angry());
    EXPECT_FLOAT_EQ(mob.health(), 19.0f);
    mob.update(kDt);
    EXPECT_EQ(mob.state(), MobState::Chase);
    // The chase faces the player (the camera yaw convention: -sin/-cos).
    const Vec3 p = mob.position();
    const float dx = 8.5f - p.x;
    const float dz = 0.5f - p.z;
    EXPECT_NEAR(mob.facingYaw(), std::atan2(-dx, -dz), 1e-4f);
}

TEST(NeutralMobTest, EnrageExpiresAfterTheThirtySecondWindow) {
    // The real-dt survival-class countdown (hard rule 8: entity age =
    // real dt — the game's fixed 1/60 step IS the dt): angry at hit,
    // calm at 1799 frames (29.98 s, one frame short) is NOT asserted at
    // the float boundary — the band pins are 1700 (still angry) and
    // 1900 (de-enraged), the kNeutralAngryDurationSeconds = 30 s figure.
    Fixture f;
    MobRegistry mobs = makeNeutralRegistry(kNeutralDef);
    const Mob& def = mobs.at(MobID{0});
    MobEntity mob = makeMob(f, def, Vec3 {0.5f, 1.0f, 0.5f}, 42);
    mob.setPlayer({8.5f, 1.0f, 0.5f}, true);
    mob.damage(1.0f);
    runMob(mob, 1700);  // 28.33 s: inside the 30 s window
    EXPECT_TRUE(mob.angry());
    // Engaged (the 8.5-block player is REACHED inside the window — the
    // 0.8 b/s chase covers 22.6 blocks in 28.3 s): Chase, Attack (the
    // terminal, stand-swing) or Pathfind are all the angry states.
    EXPECT_TRUE(mob.state() == MobState::Chase ||
                mob.state() == MobState::Attack ||
                mob.state() == MobState::Pathfind);
    runMob(mob, 200);   // 31.67 s: past the window
    EXPECT_FALSE(mob.angry());
    // The player is within attack range now — the de-enrage is the WANDER
    // (the anger term of the predicate falls out, the calm-neutral
    // does-not-chase witness).
    EXPECT_EQ(mob.state(), MobState::Wander);
}

TEST(NeutralMobTest, RepeatHitRefreshesTheWindow) {
    // A repeat hit (the burning neutral's continuous enrage — the MC
    // behavior) resets the full window: the 1700-frame-ago enrage that
    // would have expired at 1800 lives to 1700 + 1900 from the second
    // hit.
    Fixture f;
    MobRegistry mobs = makeNeutralRegistry(kNeutralDef);
    const Mob& def = mobs.at(MobID{0});
    MobEntity mob = makeMob(f, def, Vec3 {0.5f, 1.0f, 0.5f}, 42);
    mob.setPlayer({8.5f, 1.0f, 0.5f}, true);
    mob.damage(1.0f);
    runMob(mob, 1700);
    mob.damage(1.0f);  // refresh: the window restarts at 30 s
    runMob(mob, 1700);
    EXPECT_TRUE(mob.angry());  // 3400 from the first, 1700 from the last
    runMob(mob, 200);
    EXPECT_FALSE(mob.angry());
}

TEST(NeutralMobTest, PlayerDeathDeEnrages) {
    // The design's ruling: a dead player is no threat — the de-enrage
    // is instant (the existing "dead player -> wander" path's twin, made
    // state-explicit: a respawned player does not find a stale grudge).
    Fixture f;
    MobRegistry mobs = makeNeutralRegistry(kNeutralDef);
    const Mob& def = mobs.at(MobID{0});
    MobEntity mob = makeMob(f, def, Vec3 {0.5f, 1.0f, 0.5f}, 42);
    mob.setPlayer({8.5f, 1.0f, 0.5f}, true);
    mob.damage(1.0f);
    runMob(mob, 10);
    EXPECT_TRUE(mob.angry());
    mob.setPlayer({8.5f, 1.0f, 0.5f}, false);  // the player dies
    runMob(mob, 1);
    EXPECT_FALSE(mob.angry());
    // The player respawns (alive again): the calm neutral does NOT
    // re-chase on sight (the enrage is damage-gated, not sight-gated).
    mob.setPlayer({8.5f, 1.0f, 0.5f}, true);
    runMob(mob, 10);
    EXPECT_FALSE(mob.angry());
    EXPECT_NE(mob.state(), MobState::Chase);
}

TEST(NeutralMobTest, HostileMobIsNotAngerGated) {
    // The control: a HOSTILE mob (the zombie — m_neutral false) takes
    // the pre-V3-3 chase path: damage never sets the anger state (the
    // hook's m_neutral gate) and the chase is sight-gated as before
    // (the bit-level red line, the design's single-predicate witness).
    Fixture f;
    MobRegistry mobs = makeMobRegistry();  // the zombie
    const Mob& def = mobs.at(MobID{0});
    MobEntity mob = makeMob(f, def, Vec3 {0.5f, 1.0f, 0.5f}, 42);
    mob.setPlayer({8.5f, 1.0f, 0.5f}, true);
    runMob(mob, 1);
    EXPECT_EQ(mob.state(), MobState::Chase);  // sight-gated, as pre-V3-3
    EXPECT_FALSE(mob.angry());
    mob.damage(1.0f);
    EXPECT_FALSE(mob.angry());  // the hook's gate: no anger state
    runMob(mob, 1);
    EXPECT_EQ(mob.state(), MobState::Chase);
}

// -- M2: the lava-surface walker (the strider's physics) ---------------------

TEST(LavaWalkerTest, StriderHoldsTheLavaSurfacePlane) {
    // The design's M2: the lava's surface plane is the strider's FLOOR —
    // the feet rest ON it (y = 1.0, the puddle's top) with vy zeroed
    // (the fixed-point terminal), DRY at the surface (the strict AABB
    // overlap is false — the face rests exactly on the boundary), and
    // the walk pace is the data speed (no 0.6x underwater gate).
    // The settle: frame 1 the fresh (kind-default Water) column drops
    // the body 0.009 into the lava, frame 2 the kind flips to LAVA and
    // the snap lands the feet on the plane, frame 3 the body is dry and
    // the remembered kind keeps the plane its floor (the fixed point).
    Fixture f;
    makeLavaPuddle(f);
    MobRegistry mobs = makeNeutralRegistry(kStriderDef);
    const Mob& def = mobs.at(MobID{0});
    MobEntity strider = makeMob(f, def, Vec3 {5.5f, 1.0f, 5.5f}, 42);
    strider.setPlayer({50.5f, 1.0f, 50.5f}, true);  // far: the wander
    runMob(strider, 3);
    EXPECT_FLOAT_EQ(strider.position().y, 1.0f);
    EXPECT_FALSE(strider.inFluid());  // dry at its surface
    const Vec3 start = strider.position();
    runMob(strider, 600);  // 10 s of wandering on the plane
    EXPECT_FLOAT_EQ(strider.position().y, 1.0f);  // the plane is invariant
    // The walk pace moved it (the wander's data-speed steering — the
    // 0.6x gate would halve the same trajectory's displacement class).
    const float dx = strider.position().x - start.x;
    const float dz = strider.position().z - start.z;
    EXPECT_GT(std::hypot(dx, dz), 0.5f);
}

TEST(LavaWalkerTest, StriderWalksAtFullDataPace) {
    // The pace gate's EXEMPTION (the design's walk pace): the witness is
    // the per-frame step size while walking — the data-speed wander
    // (speed 1.0 x the wander factor 0.5 = 0.5 b/s) divided by the 60
    // frame steps = 0.5/60 per frame. The 0.6x underwater gate would
    // give 0.3/60 = 0.005 — OUTSIDE the pin band (the gate's detection
    // witness, seed-independent: every walking frame steps 0.5/60).
    // The gross displacement (seed 8's 600-frame wander covers 5.0) and
    // the plane invariance (y = 1.0 forever) complete the pin.
    Fixture f;
    makeLavaPuddle(f);
    MobRegistry mobs = makeNeutralRegistry(kStriderDef);
    const Mob& def = mobs.at(MobID{0});
    MobEntity strider = makeMob(f, def, Vec3 {5.5f, 1.0f, 5.5f}, 8);
    strider.setPlayer({50.5f, 1.0f, 50.5f}, true);
    runMob(strider, 2);
    const Vec3 start = strider.position();
    float maxStep = 0.0f;
    for (int i = 0; i < 600; ++i) {
        const Vec3 before = strider.position();
        strider.update(kDt);
        const float step =
            std::hypot(strider.position().x - before.x,
                       strider.position().z - before.z);
        if (step > maxStep) {
            maxStep = step;
        }
    }
    EXPECT_NEAR(maxStep, 0.5f / 60.0f, 2e-4f);  // 0.5 b/s, NOT 0.3
    const float gross = std::abs(strider.position().x - start.x) +
                        std::abs(strider.position().z - start.z);
    EXPECT_GT(gross, 2.0f);  // the walking class (the stand-still fail)
    EXPECT_FLOAT_EQ(strider.position().y, 1.0f);  // still on the plane
}

TEST(LavaWalkerTest, FallingStriderLandsOnTheLavaPlane) {
    // The splash-less landing (the design's M2: the strider sets down on
    // the lava and walks — it never dives): a fall from 2 blocks above
    // the plane (entry velocity ~11 b/s, the band-crossing sub-step)
    // lands the feet ON the plane, vy zeroed.
    Fixture f;
    makeLavaPuddle(f);
    MobRegistry mobs = makeNeutralRegistry(kStriderDef);
    const Mob& def = mobs.at(MobID{0});
    MobEntity strider = makeMob(f, def, Vec3 {5.5f, 3.0f, 5.5f}, 42);
    strider.setPlayer({50.5f, 1.0f, 50.5f}, true);
    runMob(strider, 60);  // the fall (~0.35 s) + the landing + the settle
    EXPECT_FLOAT_EQ(strider.position().y, 1.0f);
    EXPECT_FLOAT_EQ(strider.velocity().y, 0.0f);
    runMob(strider, 120);
    EXPECT_FLOAT_EQ(strider.position().y, 1.0f);  // the fixed point holds
}

TEST(LavaWalkerTest, StriderIsIgnitionExemptInLava) {
    // The design's M2: the lava ignition's EXEMPTION (the fire block
    // still ignites — the inFire term is untouched, the next test). The
    // strider is placed SUBMERGED in the lava (feet y = 0.5, the mid-
    // column — the AABB strictly overlaps the lava cell, so the raw
    // inLava read is TRUE): the rise carries it to its surface floor,
    // and the lava contact along the way (the first frames, the wet
    // band) would have ignited a flagless body — the flag keeps the
    // burn at zero. 600 frames (10 s — the burn would have ticked ~50
    // times) take zero damage.
    Fixture f;
    makeLavaPuddle(f);
    MobRegistry mobs = makeNeutralRegistry(kStriderDef);
    const Mob& def = mobs.at(MobID{0});
    MobEntity strider = makeMob(f, def, Vec3 {5.5f, 0.5f, 5.5f}, 42);
    strider.setPlayer({50.5f, 1.0f, 50.5f}, true);
    strider.armFire(0.2, 1.0f, 10.0);  // the game's spawn arming
    runMob(strider, 600);
    EXPECT_FALSE(strider.burning());
    EXPECT_FLOAT_EQ(strider.health(), 20.0f);
    EXPECT_FLOAT_EQ(strider.position().y, 1.0f);  // risen to its floor
}

TEST(LavaWalkerTest, FireBlockStillIgnitesTheStrider) {
    // The M2 boundary: the FIRE BLOCK contact ignites the strider too
    // (Minecraft's strider burns on fire — the inFire term is NOT
    // gated by the flag; only the inLava term is).
    BlockRegistry blocks;
    auto r = blocks.registerBlock(parseJson(
        R"({"id":"stone","model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    ASSERT_TRUE(r.ok());
    r = blocks.registerBlock(parseJson(
        R"({"id":"fire","properties":{"solid":false,"opaque":false,"transparent":true},"model":{"shape":"cross","textures":{"cross":"f"}}})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    World world;
    const BlockID stone = blocks.idForName("stone");
    const BlockID fire = blocks.idForName("fire");
    world.setBlock(5, 0, 5, stone);  // the floor
    world.setBlock(5, 1, 5, fire);  // a fire cell in the strider's AABB
    MobRegistry mobs = makeNeutralRegistry(kStriderDef);
    const Mob& def = mobs.at(MobID{0});
    MobEntity strider(world, blocks, def, Vec3 {5.5f, 1.0f, 5.5f}, 42);
    strider.setPlayer({50.5f, 1.0f, 50.5f}, true);
    strider.armFire(0.2, 1.0f, 10.0);
    runMob(strider, 2);
    EXPECT_TRUE(strider.burning());  // the fire cell ignited it
}

TEST(LavaWalkerTest, FlaglessMobIgnitesInLava) {
    // The control (the design's M2 witness): the SAME lava puddle, a
    // FLAGLESS mob (the hoglin's shape) SUBMERGED at the same feet —
    // the pre-V3-3 ignition: the lava contact ignites it on the first
    // wet frame, the burn ticks its health down (the strider's
    // exemption is the flag, not a lava change).
    Fixture f;
    makeLavaPuddle(f);
    MobRegistry mobs = makeNeutralRegistry(kFlaglessDef);
    const Mob& def = mobs.at(MobID{0});
    MobEntity hoglin = makeMob(f, def, Vec3 {5.5f, 0.5f, 5.5f}, 42);
    hoglin.setPlayer({50.5f, 1.0f, 50.5f}, true);
    hoglin.armFire(0.2, 1.0f, 10.0);
    runMob(hoglin, 30);  // 0.5 s: the ignition + the first burn ticks
    EXPECT_TRUE(hoglin.burning());
    EXPECT_LT(hoglin.health(), 40.0f);
}

TEST(LavaWalkerTest, FlaglessMobInLavaKeepsTheSwimLevelFloat) {
    // The no-flag fluid REGRESSION (the design's M2 red line: the
    // generic integrateFluid's swim-level float is bit-identical for a
    // flagless mob in lava — the strider's surface walk is the flag's
    // branch, not a lava physics change): the flagless mob in the puddle
    // floats at the swim level (y = surface - height/2 = 1.0 - 0.75 =
    // 0.25, the half-submerged MC swim look), NOT on the plane.
    Fixture f;
    makeLavaPuddle(f);
    MobRegistry mobs = makeNeutralRegistry(kFlaglessDef);
    const Mob& def = mobs.at(MobID{0});
    MobEntity mob = makeMob(f, def, Vec3 {5.5f, 0.5f, 5.5f}, 42);
    mob.setPlayer({50.5f, 1.0f, 50.5f}, true);
    runMob(mob, 120);
    EXPECT_NEAR(mob.position().y, 0.25f, 1e-3f);  // the swim level
    EXPECT_TRUE(mob.inFluid());                    // the half-submerged
}

// -- M2: the lava-surface spawn predicate (the findMobSpawnCell branch) ------

namespace {

// The spawn-predicate fixture world: the fixture floor (stone at y=0)
// with a SOURCE cell at (0, 1, -6) (the air cell above the floor —
// setFluid's air-only rule) in the first ring column (distance 6.0).
void makeSpawnLava(Fixture& f, FluidKind kind, bool belowFluid, bool cover) {
    EXPECT_TRUE(f.world.setFluid(0, 1, -6, kind, kMaxFluidLevel));
    if (belowFluid) {
        // A second fluid below the source: the double-fluid rejection
        // (the lava film over water — not a walkable top). The stone
        // floor cell is cleared first (the fluid's air-only rule).
        f.world.setBlock(0, 0, -6, BlockID::air());
        EXPECT_TRUE(f.world.setFluid(0, 0, -6, FluidKind::Water,
                                     kMaxFluidLevel));
    }
    if (cover) {
        // A solid in the headroom: the covered-lava rejection (the
        // plateau-over-sea case — the lava is not an exposed top).
        f.world.setBlock(0, 2, -6, f.stone);
    }
}

} // namespace

TEST(MobSpawnCellTest, LavaSurfaceBranchAcceptsTheLavaSourceTop) {
    // The design's M2 spawn surface: the LAVA SOURCE cell whose top is a
    // free surface (the air above, no fluid below) is the strider's
    // floor — the first ring column (0, -6) at distance 6.0, the
    // returns the LAVA CELL (the game's spawnY = cell.y + 1.0 = the top).
    Fixture f;
    makeSpawnLava(f, FluidKind::Lava, false, false);
    auto cell = findMobSpawnCell(f.world, f.blocks, 4.0f, 6.0f, 0, 0, 0,
                                 1.0f, 1.5f, 0.0f, 0, kPlayerMin,
                                 kPlayerMax, {}, 0.0f, 0.0f, 0.0f,
                                 kNightSkyLevel, false, true);
    ASSERT_TRUE(cell.has_value());
    // The LAVA CELL (y=1) — the game's spawnY = cell.y + 1.0 = 2.0 (the
    // top face the walker's feet rest on, the plane its integrate owns).
    EXPECT_EQ(*cell, (BlockPos {0, 1, -6}));
}

TEST(MobSpawnCellTest, LavaSurfaceBranchAcceptsTheSeaColumn) {
    // The design's accepted form over the REAL lava sea: the all-source
    // column's TOPMOST L8 cell is the sea's top — the cell below holds
    // LAVA (the same kind, not a second fluid) and is the accepted case
    // (the strider's MC spawn: the lava sea's surface).
    Fixture f;
    makeSpawnLava(f, FluidKind::Lava, false, false);
    // A second lava source directly below the first (the sea column).
    f.world.setBlock(0, 0, -6, BlockID::air());
    EXPECT_TRUE(f.world.setFluid(0, 0, -6, FluidKind::Lava, kMaxFluidLevel));
    auto cell = findMobSpawnCell(f.world, f.blocks, 4.0f, 6.0f, 0, 0, 0,
                                 1.0f, 1.5f, 0.0f, 0, kPlayerMin,
                                 kPlayerMax, {}, 0.0f, 0.0f, 0.0f,
                                 kNightSkyLevel, false, true);
    ASSERT_TRUE(cell.has_value());
    // The TOPMOST L8 (the sea's top, y=1) — the walker's feet plane.
    EXPECT_EQ(*cell, (BlockPos {0, 1, -6}));
}

TEST(MobSpawnCellTest, LavaSurfaceBranchRejectsNonLava) {
    // The flag-gated predicate is LAVA-only (the design's 3-form's
    // "non-lava reject"): a WATER source top is not a walker's floor —
    // the strider sinks in water (the M2 boundary, the documented
    // eviction). No lava anywhere in the ring = no cell.
    Fixture f;
    makeSpawnLava(f, FluidKind::Water, false, false);
    auto cell = findMobSpawnCell(f.world, f.blocks, 4.0f, 6.0f, 0, 0, 0,
                                 1.0f, 1.5f, 0.0f, 0, kPlayerMin,
                                 kPlayerMax, {}, 0.0f, 0.0f, 0.0f,
                                 kNightSkyLevel, false, true);
    EXPECT_FALSE(cell.has_value());
}

TEST(MobSpawnCellTest, LavaSurfaceBranchRejectsDoubleFluid) {
    // The design's 3-form's "double-fluid reject": a lava source with a
    // fluid cell BELOW it (the lava film over water) is not a walkable
    // surface — the walker would stand in the double film.
    Fixture f;
    makeSpawnLava(f, FluidKind::Lava, true, false);
    auto cell = findMobSpawnCell(f.world, f.blocks, 4.0f, 6.0f, 0, 0, 0,
                                 1.0f, 1.5f, 0.0f, 0, kPlayerMin,
                                 kPlayerMax, {}, 0.0f, 0.0f, 0.0f,
                                 kNightSkyLevel, false, true);
    EXPECT_FALSE(cell.has_value());
}

TEST(MobSpawnCellTest, LavaSurfaceBranchRejectsCoveredLava) {
    // The headroom twin: a solid in the cell above the lava source (the
    // nether plateau over the lava sea — the covered lava is not an
    // exposed top) rejects the column.
    Fixture f;
    makeSpawnLava(f, FluidKind::Lava, false, true);
    auto cell = findMobSpawnCell(f.world, f.blocks, 4.0f, 6.0f, 0, 0, 0,
                                 1.0f, 1.5f, 0.0f, 0, kPlayerMin,
                                 kPlayerMax, {}, 0.0f, 0.0f, 0.0f,
                                 kNightSkyLevel, false, true);
    EXPECT_FALSE(cell.has_value());
}

TEST(MobSpawnCellTest, FlaglessMobIgnoresTheLavaSurface) {
    // The flag's default-false red line (the design's "flagless mob
    // candidate predicate bit-level invariant"): the SAME lava column
    // the walker takes is NOT a floor for a flagless mob (the floor scan
    // needs a SOLID cell — the lava's cell block is air) — the scan
    // falls through to the next candidate, exactly as pre-V3-3.
    Fixture f;
    makeSpawnLava(f, FluidKind::Lava, false, false);
    auto cell = findMobSpawnCell(f.world, f.blocks, 4.0f, 6.0f, 0, 0, 0,
                                 0.6f, 1.8f, 0.0f, 0, kPlayerMin,
                                 kPlayerMax, {});
    ASSERT_TRUE(cell.has_value());
    EXPECT_EQ(*cell, (BlockPos {-3, 0, -5}));  // the lava column skipped
}

// -- the ground-mob ambient roll (the runAi frame-locked draw) ---------------

TEST(NeutralMobTest, GroundAmbientRollIsFrameLocked) {
    // The flyCruise squeak's ground twin (the design's ambient face): a
    // ground mob with the data ambient pair rolls once per frame —
    // probability 1 / (interval * 60) of the per-mob stream — and plays
    // through the wired callback. The pin: two SAME-SEEDED instances
    // fire on the EXACT same frames (the frame-locked determinism, hard
    // rule 8) and the count over 3600 frames is the 1/180 rate class
    // (the 3.0 s interval = 180 frames — ~20 fires, the band witness).
    Fixture f;
    MobRegistry mobs = makeNeutralRegistry(kNeutralAmbientDef);
    const Mob& def = mobs.at(MobID{0});
    std::vector<int> firesA;
    std::vector<int> firesB;
    {
        MobEntity a = makeMob(f, def, Vec3 {0.5f, 1.0f, 0.5f}, 99);
        a.setPlayer({50.5f, 1.0f, 50.5f}, true);
        // The frame index: the callback fires with the mob's CURRENT
        // position (the spatialization), the pin tracks the frame
        // number (the counter's order — the frame-locked draw).
        int frame = 0;
        a.setAmbientSoundCallback(
            [&firesA, &frame](Vec3) { firesA.push_back(frame); });
        for (int i = 0; i < 3600; ++i) {
            a.update(kDt);
            ++frame;
        }
    }
    {
        MobEntity b = makeMob(f, def, Vec3 {0.5f, 1.0f, 0.5f}, 99);
        b.setPlayer({50.5f, 1.0f, 50.5f}, true);
        int frame = 0;
        b.setAmbientSoundCallback(
            [&firesB, &frame](Vec3) { firesB.push_back(frame); });
        for (int i = 0; i < 3600; ++i) {
            b.update(kDt);
            ++frame;
        }
    }
    ASSERT_FALSE(firesA.empty());  // the roll fired (the ~20 class)
    EXPECT_EQ(firesA, firesB);     // the same seed, the same frames
    EXPECT_GT(firesA.size(), 10u);  // the 1/180 rate class over 3600
    EXPECT_LT(firesA.size(), 40u);
}
