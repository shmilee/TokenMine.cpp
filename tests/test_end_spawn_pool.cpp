// V3-10c (the End's mob spawn pool): the init-side pure resolution
// (loadEndSpawnPool — the loadNetherSpawnPool's third consumption): the
// file load + the shared strict schema + the vocabulary cross-check
// against the SHARED mob type table. Headless (a temp asset root), no
// game. The shipped 10c pool is the SINGLE-SPECIES enderman (the End's
// only mob — the 10d dragon's era re-discusses the overworld / nether
// pools' enderman entries, the recon's R11 boundary).
#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "core/asset_manager.hpp"
#include "core/json.hpp"
#include "entity/end_spawn_pool.hpp"
#include "entity/mob_registry.hpp"

using namespace mc;

namespace {

constexpr const char* kPoolDir = "/tmp/opencode/v310c_pool_test";

// A minimal valid mob def (the registry's strict parse — a one-box
// model, the zombie's head shape).
std::string mobDef(const char* id) {
    std::ostringstream ss;
    ss << R"({"id":")" << id << R"(","properties":{"health":40,"speed":0.8,"detect_radius":16,"attack_range":1.6,"attack_damage":6,"attack_cooldown":1.5,"wander_interval":2.0,"neutral":true},"model":{"width":0.6,"height":2.9,"boxes":[{"pos":[0.0,2.65,0.0],"size":[0.5,0.5,0.5],"texture":"a"}]}})";
    return ss.str();
}

void writePool(const std::string& text) {
    std::filesystem::remove_all(kPoolDir);
    std::filesystem::create_directories(std::string(kPoolDir) + "/data");
    std::ofstream out(std::string(kPoolDir) + "/data/mob_spawning_end.json");
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
  {"id": "enderman", "weight": 10}
 ]
})";

} // namespace

TEST(EndSpawnPool, ResolvesTheValidPoolAgainstTheSharedTable) {
    MobRegistry types;
    {
        auto parsed = Json::parse(mobDef("enderman"));
        ASSERT_TRUE(parsed.ok());
        auto r = types.registerMob(*parsed);
        ASSERT_TRUE(r.ok()) << r.error().message;
    }
    writePool(kValidPool);
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot(kPoolDir).ok());
    auto cfg = loadEndSpawnPool(assets, types);
    ASSERT_TRUE(cfg.ok()) << cfg.error().message;
    ASSERT_EQ(cfg->mobs.size(), 1u);
    EXPECT_EQ(cfg->mobs[0].id, "enderman");
    EXPECT_EQ(cfg->mobs[0].weight, 10);
    // The shared cadence (the nether file's twin).
    EXPECT_EQ(cfg->checkEveryFrames, 30);
    EXPECT_EQ(cfg->firstSpawnFrame, 60);
    EXPECT_EQ(cfg->maxMobs, 4);
    EXPECT_EQ(cfg->totalWeight(), 10);
    // The single-species pick's domain: every value lands on the
    // enderman (the weighted walk's degenerate shape).
    for (std::uint64_t pick : {0u, 1u, 9u, 12345u}) {
        EXPECT_EQ(cfg->pickIndex(pick % static_cast<std::uint64_t>(
                                       cfg->totalWeight())), 0u);
    }
    std::filesystem::remove_all(kPoolDir);
}

TEST(EndSpawnPool, MissingFileIsNamed) {
    MobRegistry types;
    {
        auto parsed = Json::parse(mobDef("enderman"));
        ASSERT_TRUE(parsed.ok());
        ASSERT_TRUE(types.registerMob(*parsed).ok());
    }
    // A root WITHOUT the pool file: the loadText failure names the path
    // (the S6 "end pool missing" face — the init's "cannot read asset
    // file" line, rc=1).
    std::filesystem::remove_all(kPoolDir);
    std::filesystem::create_directories(kPoolDir);
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot(kPoolDir).ok());
    auto cfg = loadEndSpawnPool(assets, types);
    ASSERT_FALSE(cfg.ok());
    EXPECT_NE(cfg.error().message.find("mob_spawning_end.json"),
              std::string::npos);
    std::filesystem::remove_all(kPoolDir);
}

TEST(EndSpawnPool, UnknownMobIdIsNamed) {
    // The vocabulary cross-check (the nether's "unknown mob"
    // discipline): the pool names a type the shared table lacks — the
    // error names the pool file + the id (the S6 "pool unknown mob"
    // face).
    MobRegistry types;
    {
        auto parsed = Json::parse(mobDef("enderman"));
        ASSERT_TRUE(parsed.ok());
        ASSERT_TRUE(types.registerMob(*parsed).ok());
    }
    writePool(R"({
 "check_every_frames": 30,
 "first_spawn_frame": 60,
 "max_mobs": 4,
 "min_radius": 4,
 "max_radius": 6,
 "despawn_distance": 40,
 "mobs": [
  {"id": "enderman", "weight": 10},
  {"id": "ghast", "weight": 1}
 ]
})");
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot(kPoolDir).ok());
    auto cfg = loadEndSpawnPool(assets, types);
    ASSERT_FALSE(cfg.ok());
    EXPECT_NE(cfg.error().message.find("mob_spawning_end.json"),
              std::string::npos);
    EXPECT_NE(cfg.error().message.find("ghast"), std::string::npos);
    std::filesystem::remove_all(kPoolDir);
}

TEST(EndSpawnPool, SchemaRejectionsFlowThroughTheSharedParse) {
    // The shared strict schema (the nether file's twin — the weight 0 /
    // the empty list / the min>=max ring all reject, named, before the
    // vocabulary check).
    MobRegistry types;
    {
        auto parsed = Json::parse(mobDef("enderman"));
        ASSERT_TRUE(parsed.ok());
        ASSERT_TRUE(types.registerMob(*parsed).ok());
    }
    const char* bad[] = {
        // the weight 0
        R"({"check_every_frames":30,"first_spawn_frame":60,"max_mobs":4,"min_radius":4,"max_radius":6,"despawn_distance":40,"mobs":[{"id":"enderman","weight":0}]})",
        // the empty list
        R"({"check_every_frames":30,"first_spawn_frame":60,"max_mobs":4,"min_radius":4,"max_radius":6,"despawn_distance":40,"mobs":[]})",
        // the min >= max ring
        R"({"check_every_frames":30,"first_spawn_frame":60,"max_mobs":4,"min_radius":6,"max_radius":4,"despawn_distance":40,"mobs":[{"id":"enderman","weight":10}]})",
    };
    for (const char* text : bad) {
        writePool(text);
        AssetManager assets;
        ASSERT_TRUE(assets.setRoot(kPoolDir).ok());
        auto cfg = loadEndSpawnPool(assets, types);
        EXPECT_FALSE(cfg.ok()) << text;
    }
    std::filesystem::remove_all(kPoolDir);
}

TEST(EndSpawnPool, ShippedEndPoolPinsTheDesign) {
    // The REAL asset (data/mob_spawning_end.json): the design's
    // single-species pool — the enderman's weight 10, the shared
    // cadence (30 / 60 / 4 / ring 4-6 / despawn 40), and the
    // vocabulary resolves in the REAL shared type table (the shipped
    // mobs — the 10c enderman among them).
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    MobRegistry types;
    ASSERT_TRUE(types.loadDirectory(assets, "data/mobs").ok());
    auto cfg = loadEndSpawnPool(assets, types);
    ASSERT_TRUE(cfg.ok()) << cfg.error().message;
    ASSERT_EQ(cfg->mobs.size(), 1u);
    EXPECT_EQ(cfg->mobs[0].id, "enderman");
    EXPECT_EQ(cfg->mobs[0].weight, 10);
    EXPECT_EQ(cfg->checkEveryFrames, 30);
    EXPECT_EQ(cfg->firstSpawnFrame, 60);
    EXPECT_EQ(cfg->maxMobs, 4);
    EXPECT_FLOAT_EQ(cfg->minRadius, 4.0f);
    EXPECT_FLOAT_EQ(cfg->maxRadius, 6.0f);
    EXPECT_FLOAT_EQ(cfg->despawnDistance, 40.0f);
    EXPECT_EQ(cfg->totalWeight(), 10);
}
