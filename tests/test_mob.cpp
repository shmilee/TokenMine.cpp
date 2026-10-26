// Mob system tests (Phase 20): the mob registry (strict validation),
// the spawning config + pure spawn-cell scan, the box-model mesh
// geometry, the mob AI state machine (wander/chase/attack, knockback,
// determinism), combat (damage/death) and the AABB raycast used for
// player targeting.
#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <numbers>
#include <stdexcept>
#include <string>
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

// Registers the zombie def; ADD_FAILURE on error (tests expect success).
MobRegistry makeMobRegistry() {
    MobRegistry mobs;
    auto r = mobs.registerMob(parseJson(kZombieDef));
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

MobEntity makeCarrierMob(const Fixture& f, const char* def) {
    MobRegistry mobs;
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
    const EffectRegistry effects = makeCarrierEffects();
    MobEntity cow = makeCarrierMob(f, kCowDef);
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
    const EffectRegistry effects = makeCarrierEffects();
    std::string zombieDef = kZombieDef;
    const std::string key = "\"wander_interval\": 2.5";
    zombieDef.replace(zombieDef.find(key), key.size(),
                      std::string("\"wander_interval\": 2.5, \"undead\": true"));
    MobEntity zombie = makeCarrierMob(f, zombieDef.c_str());
    MobEntity cow = makeCarrierMob(f, kCowDef);
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
    const EffectRegistry effects = makeCarrierEffects();
    MobEntity cow = makeCarrierMob(f, kCowDef);
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
    const EffectRegistry effects = makeCarrierEffects();
    MobEntity cow = makeCarrierMob(f, kCowDef);
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
    const EffectRegistry effects = makeCarrierEffects();
    MobEntity cow = makeCarrierMob(f, kCowDef);
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
    const EffectRegistry effects = makeCarrierEffects();
    MobEntity cow = makeCarrierMob(f, kCowDef);
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
    // Night (dayFactor 0): the open surface's sky contribution scales to
    // 0, so the dark-only mob may spawn at the first candidate.
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

TEST(MobSpawnCellTest, NightScalesSkyLightToZero) {
    Fixture f;
    // The SAME bright cell is spawnable at night (dayFactor 0 -> the sky
    // contribution is 0, the renderer-darkening convention).
    f.world.getOrCreateChunk({0, 0, -1}).setSkyLightAt(0, 1, 10, 15);
    auto cell = findMobSpawnCell(f.world, f.blocks, 4.0f, 6.0f, 0, 0, 0,
                                 0.6f, 1.8f, 0.0f, 0, kPlayerMin, kPlayerMax,
                                 {});
    ASSERT_TRUE(cell.has_value());
    EXPECT_EQ(*cell, (BlockPos {0, 0, -6}));
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
    // One-block ledge at x in [10, 12), z in [8, 10) (TWO cells wide in
    // +X: the shared judgment's +X front check looks at snapped+1 — the
    // player's exact latent quirk, pinned by
    // DoesNotStepSingleCellStepPlusX below — so a one-cell step in +X is
    // not step-up-able for either actor).
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
    // chase frame of 16.8); the range also covers the player-shared
    // mid-air hop (a face flush by coincidence while the feet are at the
    // NON-integer slab-top level 1.5 lifts 1.0 and falls back — it can
    // shift the stop frame through the attack vertical band, see the
    // KNOWN_ISSUES 生存 step-up 条).
    EXPECT_GE(mob.position().x, 16.8f);
    EXPECT_LE(mob.position().x, 16.9f);
    EXPECT_FLOAT_EQ(mob.position().y, 1.5f); // feet flush on the slab top
    EXPECT_TRUE(mob.onGround());
}

TEST(MobStepUpTest, DoesNotStepUpWall) {
    // The Phase-99 judgment is shared verbatim: a wall (solid-region top
    // 1.5, the barrier) is NOT a 1-block step — the mob stays clamped at
    // its face, forever (the player's E2E S3b wall-contrast twin).
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
    runMob(mob, 300);
    EXPECT_EQ(mob.state(), MobState::Chase);
    EXPECT_FLOAT_EQ(mob.position().x, 10.0f - 0.3f); // clamped at the wall
    EXPECT_FLOAT_EQ(mob.position().y, 1.0f);         // never stepped up
    EXPECT_TRUE(mob.onGround());
}

TEST(MobStepUpTest, DoesNotStepUpFence) {
    // The fence is the wall's barrier twin (the same 1.5 solid region):
    // no step-up either.
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
    runMob(mob, 300);
    EXPECT_EQ(mob.state(), MobState::Chase);
    EXPECT_FLOAT_EQ(mob.position().x, 10.0f - 0.3f);
    EXPECT_FLOAT_EQ(mob.position().y, 1.0f);
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
    runMob(mob, 300);
    EXPECT_EQ(mob.state(), MobState::Chase);
    EXPECT_FLOAT_EQ(mob.position().x, 10.0f - 0.3f);
    EXPECT_FLOAT_EQ(mob.position().y, 1.0f);
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

TEST(MobStepUpTest, DoesNotStepSingleCellStepPlusX) {
    // The SHARED judgment's +X front check (snapped+1) looks at the cell
    // BEYOND the one the body is flush against — a single-cell step in +X
    // is not step-up-able. This pins the EXACT player behavior (the
    // player walks +X into a one-cell step and stays clamped at 9.7 —
    // probe-verified, the latent quirk documented in KNOWN_ISSUES 生存
    // step-up 条). Fixing it changes the player's step-up and is its own
    // phase — the mob must keep matching the player either way.
    Fixture f;
    MobRegistry mobs = makeMobRegistry();
    const Mob& def = mobs.at(MobID{0});
    for (int z = 8; z < 10; ++z) {
        f.world.setBlock(10, 1, z, f.stone); // one cell only
    }
    MobEntity mob = makeMob(f, def, {8.5f, 1.0f, 8.5f});
    mob.setPlayer({13.9f, 1.0f, 8.5f}, true);
    runMob(mob, 300);
    EXPECT_EQ(mob.state(), MobState::Chase); // stuck: distH 4.2 > 1.6
    EXPECT_FLOAT_EQ(mob.position().x, 10.0f - 0.3f);
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
                        "t", "leg"};
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
    // the same 1-block step (two cells wide in +X — the shared judgment's
    // +X front-check quirk, the DoesNotStepSingleCellStepPlusX twin) is
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
