// V3-3 (the nether's mob spawn pool): the init-side pure resolution
// (loadNetherSpawnPool — the checkWoolDrops pattern): the file load +
// the shared strict schema + the vocabulary cross-check against the
// SHARED mob type table. Headless (a temp asset root), no game.
#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "core/asset_manager.hpp"
#include "core/json.hpp"
#include "entity/mob_registry.hpp"
#include "entity/nether_spawn_pool.hpp"

using namespace mc;

namespace {

constexpr const char* kPoolDir = "/tmp/opencode/v380_pool_test";

// A minimal valid mob def (the registry's strict parse — a one-box
// model, the zombie's head shape).
std::string mobDef(const char* id) {
    std::ostringstream ss;
    ss << R"({"id":")" << id << R"(","properties":{"health":20,"speed":0.8,"detect_radius":16,"attack_range":1.6,"attack_damage":3,"attack_cooldown":1.0,"wander_interval":2.5},"model":{"width":0.6,"height":1.8,"boxes":[{"pos":[0.0,1.25,0.0],"size":[0.5,0.5,0.5],"texture":"a"}]}})";
    return ss.str();
}

// A temp asset root with the nether pool file (the given text) + the
// real mobs directory symlinked in is NOT used (the type table is the
// test's own registry) — only the pool file matters to the loader.
void writePool(const std::string& text) {
    std::filesystem::remove_all(kPoolDir);
    std::filesystem::create_directories(std::string(kPoolDir) + "/data");
    std::ofstream out(std::string(kPoolDir) + "/data/mob_spawning_nether.json");
    out << text;
}

const char* kValidPool = R"({
 "check_every_frames": 30,
 "first_spawn_frame": 60,
 "max_mobs": 4,
 "min_radius": 4,
 "max_radius": 6,
 "despawn_distance": 40,
 "mobs": [
  {"id": "zombified_piglin", "weight": 10},
  {"id": "piglin", "weight": 8}
 ]
})";

} // namespace

TEST(NetherSpawnPool, ResolvesTheValidPoolAgainstTheSharedTable) {
    MobRegistry types;
    for (const char* id : {"zombified_piglin", "piglin"}) {
        auto parsed = Json::parse(mobDef(id));
        ASSERT_TRUE(parsed.ok());
        auto r = types.registerMob(*parsed);
        ASSERT_TRUE(r.ok()) << r.error().message;
    }
    writePool(kValidPool);
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot(kPoolDir).ok());
    auto cfg = loadNetherSpawnPool(assets, types);
    ASSERT_TRUE(cfg.ok()) << cfg.error().message;
    EXPECT_EQ(cfg->mobs.size(), 2u);
    EXPECT_EQ(cfg->mobs[0].id, "zombified_piglin");
    EXPECT_EQ(cfg->mobs[0].weight, 10);
    EXPECT_EQ(cfg->mobs[1].id, "piglin");
    EXPECT_EQ(cfg->mobs[1].weight, 8);
    // The shared cadence (the overworld's twin).
    EXPECT_EQ(cfg->checkEveryFrames, 30);
    EXPECT_EQ(cfg->firstSpawnFrame, 60);
    EXPECT_EQ(cfg->maxMobs, 4);
    EXPECT_EQ(cfg->totalWeight(), 18);
    std::filesystem::remove_all(kPoolDir);
}

TEST(NetherSpawnPool, MissingFileIsNamed) {
    MobRegistry types;
    auto parsed = Json::parse(mobDef("zombified_piglin"));
    ASSERT_TRUE(parsed.ok());
    ASSERT_TRUE(types.registerMob(*parsed).ok());
    // A root WITHOUT the pool file: the loadText failure names the path
    // (the S6 "nether pool missing" face — the init's "cannot read asset
    // file" line, rc=1).
    std::filesystem::remove_all(kPoolDir);
    std::filesystem::create_directories(kPoolDir);
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot(kPoolDir).ok());
    auto cfg = loadNetherSpawnPool(assets, types);
    ASSERT_FALSE(cfg.ok());
    EXPECT_NE(cfg.error().message.find("mob_spawning_nether.json"),
              std::string::npos);
    std::filesystem::remove_all(kPoolDir);
}

TEST(NetherSpawnPool, UnknownMobIdIsNamed) {
    // The vocabulary cross-check (the overworld's "unknown mob"
    // discipline): the pool names a type the shared table lacks — the
    // error names the pool file + the id (the S6 "pool unknown mob"
    // face).
    MobRegistry types;
    auto parsed = Json::parse(mobDef("zombified_piglin"));
    ASSERT_TRUE(parsed.ok());
    ASSERT_TRUE(types.registerMob(*parsed).ok());
    writePool(R"({
 "check_every_frames": 30,
 "first_spawn_frame": 60,
 "max_mobs": 4,
 "min_radius": 4,
 "max_radius": 6,
 "despawn_distance": 40,
 "mobs": [
  {"id": "zombified_piglin", "weight": 10},
  {"id": "ghast", "weight": 1}
 ]
})");
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot(kPoolDir).ok());
    auto cfg = loadNetherSpawnPool(assets, types);
    ASSERT_FALSE(cfg.ok());
    EXPECT_NE(cfg.error().message.find("mob_spawning_nether.json"),
              std::string::npos);
    EXPECT_NE(cfg.error().message.find("ghast"), std::string::npos);
    std::filesystem::remove_all(kPoolDir);
}

TEST(NetherSpawnPool, SchemaRejectionsFlowThroughTheSharedParse) {
    // The shared strict schema (the overworld file's twin — the weight
    // 0 / the empty list / the min>=max ring all reject, named, before
    // the vocabulary check).
    MobRegistry types;
    auto parsed = Json::parse(mobDef("zombified_piglin"));
    ASSERT_TRUE(parsed.ok());
    ASSERT_TRUE(types.registerMob(*parsed).ok());
    const char* bad[] = {
        // the weight 0
        R"({"check_every_frames":30,"first_spawn_frame":60,"max_mobs":4,"min_radius":4,"max_radius":6,"despawn_distance":40,"mobs":[{"id":"zombified_piglin","weight":0}]})",
        // the empty list
        R"({"check_every_frames":30,"first_spawn_frame":60,"max_mobs":4,"min_radius":4,"max_radius":6,"despawn_distance":40,"mobs":[]})",
        // the min >= max ring
        R"({"check_every_frames":30,"first_spawn_frame":60,"max_mobs":4,"min_radius":6,"max_radius":4,"despawn_distance":40,"mobs":[{"id":"zombified_piglin","weight":10}]})",
    };
    for (const char* text : bad) {
        writePool(text);
        AssetManager assets;
        ASSERT_TRUE(assets.setRoot(kPoolDir).ok());
        auto cfg = loadNetherSpawnPool(assets, types);
        EXPECT_FALSE(cfg.ok()) << text;
    }
    std::filesystem::remove_all(kPoolDir);
}

TEST(NetherSpawnPool, ShippedNetherPoolPinsTheDesign) {
    // The REAL asset (data/mob_spawning_nether.json): the design's 5-mob
    // nether pool — the weights (zombified_piglin 10 / piglin 8 / hoglin
    // 7 / magma_cube 5 / strider 1, the MC nether pool's approximation),
    // the shared cadence (30 / 60 / 4 / ring 4-6 / despawn 40), and the
    // vocabulary resolves in the REAL shared type table (the 13 shipped
    // mobs — the 8 overworld + the 6 nether (the V3-14b ghast, the
    // nether's second flyer, the pool's rare slot after the strider)).
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    MobRegistry types;
    ASSERT_TRUE(types.loadDirectory(assets, "data/mobs").ok());
    auto cfg = loadNetherSpawnPool(assets, types);
    ASSERT_TRUE(cfg.ok()) << cfg.error().message;
    ASSERT_EQ(cfg->mobs.size(), 6u);
    const struct {
        const char* id;
        int weight;
    } expect[] = {
        {"zombified_piglin", 10}, {"piglin", 8}, {"hoglin", 7},
        {"magma_cube", 5},        {"strider", 1}, {"ghast", 2},
    };
    for (std::size_t i = 0; i < 6; ++i) {
        EXPECT_EQ(cfg->mobs[i].id, expect[i].id) << i;
        EXPECT_EQ(cfg->mobs[i].weight, expect[i].weight) << i;
    }
    EXPECT_EQ(cfg->checkEveryFrames, 30);
    EXPECT_EQ(cfg->firstSpawnFrame, 60);
    EXPECT_EQ(cfg->maxMobs, 4);
    EXPECT_FLOAT_EQ(cfg->minRadius, 4.0f);
    EXPECT_FLOAT_EQ(cfg->maxRadius, 6.0f);
    EXPECT_FLOAT_EQ(cfg->despawnDistance, 40.0f);
    EXPECT_EQ(cfg->totalWeight(), 33); // V3-14b: + the ghast's weight 2
    // The weighted pick's domain (the design's census: 33 =
    // 10+8+7+5+1+2 — the V3-14b ghast appends the last slot).
    EXPECT_EQ(cfg->pickIndex(9u), 0u);   // zombified_piglin owns [0, 10)
    EXPECT_EQ(cfg->pickIndex(17u), 1u);  // piglin owns [10, 18)
    EXPECT_EQ(cfg->pickIndex(24u), 2u);  // hoglin owns [18, 25)
    EXPECT_EQ(cfg->pickIndex(29u), 3u);  // magma_cube owns [25, 30)
    EXPECT_EQ(cfg->pickIndex(30u), 4u);  // strider owns [30, 31)
    EXPECT_EQ(cfg->pickIndex(32u), 5u);  // ghast owns [31, 33)
}
