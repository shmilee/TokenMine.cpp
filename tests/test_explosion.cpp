// V3-14a explosion tests: the damage falloff (the pure curve) and the
// blast's cell scan (the pure read over a fixture world — the
// fire_spread test model: a registry with a blastable stone, a
// resistant obsidian-class block, a non-destroyable flammable (ignites,
// never breaks), and a destroyable flammable (breaks — the branch's
// precedence over the flame)).
#include <gtest/gtest.h>

#include <cmath>
#include <string>
#include <vector>

#include "block/block_registry.hpp"
#include "core/json.hpp"
#include "world/explosion.hpp"
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

// The fixture registry: stone (blastable), obsidian (blast_resistance
// 3 — beats the radius-3 blast everywhere), log (destroyable +
// flamable — the break's precedence over the flame), moss (NOT
// destroyable + flamable — ignites, never breaks).
struct ExplosionFixture {
    BlockRegistry reg;
    BlockID stone;
    BlockID obsidian;
    BlockID log;
    BlockID moss;

    ExplosionFixture() {
        const std::string stoneDef =
            "{\"id\":\"stone\",\"properties\":{\"hardness\":1.5,"
            "\"drops\":\"cobblestone\"},"
            "\"model\":{\"textures\":{\"top\":\"stone\",\"side\":\"stone\","
            "\"bottom\":\"stone\"}}}";
        const std::string obsidianDef =
            "{\"id\":\"obsidian\",\"properties\":{\"hardness\":10,"
            "\"blast_resistance\":3,\"drops\":\"obsidian\"},"
            "\"model\":{\"textures\":{\"top\":\"obsidian\","
            "\"side\":\"obsidian\",\"bottom\":\"obsidian\"}}}";
        const std::string logDef =
            "{\"id\":\"log\",\"properties\":{\"hardness\":2.0,"
            "\"flamable\":true,\"drops\":\"log\"},"
            "\"model\":{\"textures\":{\"top\":\"log\",\"side\":\"log\","
            "\"bottom\":\"log\"}}}";
        const std::string mossDef =
            "{\"id\":\"moss\",\"properties\":{\"hardness\":5.0,"
            "\"destroyable\":false,\"flamable\":true},"
            "\"model\":{\"textures\":{\"top\":\"moss\",\"side\":\"moss\","
            "\"bottom\":\"moss\"}}}";
        for (const std::string& def : {stoneDef, obsidianDef, logDef,
                                       mossDef}) {
            auto r = reg.registerBlock(parseDef(def));
            if (!r.ok()) {
                throw std::runtime_error(r.error().message);
            }
        }
        stone = reg.idForName("stone");
        obsidian = reg.idForName("obsidian");
        log = reg.idForName("log");
        moss = reg.idForName("moss");
    }
};

} // namespace

// -- the damage falloff -----------------------------------------------------

TEST(ExplosionTest, DamageFalloffCurve) {
    // Center = the base damage (12); the radius's bound = zero; past
    // the radius = zero (the blast's hard bound).
    EXPECT_EQ(explosionDamage(0.0, 3.0, 12.0), 12);
    EXPECT_EQ(explosionDamage(3.0, 3.0, 12.0), 0);
    EXPECT_EQ(explosionDamage(4.5, 3.0, 12.0), 0);
    // The linear falloff's integer read (ceil): half radius = 6,
    // quarter = 9 (12 * (1 - 0.75/3) = 9 exactly; 12 * (1 - 0.375/3)
    // = 10.5 -> 11).
    EXPECT_EQ(explosionDamage(1.5, 3.0, 12.0), 6);
    EXPECT_EQ(explosionDamage(0.75, 3.0, 12.0), 9);
    EXPECT_EQ(explosionDamage(0.375, 3.0, 12.0), 11);
    // A zero-radius / zero-damage blast is the no-op.
    EXPECT_EQ(explosionDamage(0.0, 0.0, 12.0), 0);
    EXPECT_EQ(explosionDamage(0.0, 3.0, 0.0), 0);
}

// -- the cell scan ------------------------------------------------------------

TEST(ExplosionTest, ScanBreaksBlastableKeepsResistant) {
    ExplosionFixture f;
    World world;
    // A 5x1x5 stone slab at y=8, the blast centered on the slab's
    // center cell (the radius-3 blast's local intensity at every slab
    // cell beats stone's 0 resistance within the sphere).
    for (int x = -2; x <= 2; ++x) {
        for (int z = -2; z <= 2; ++z) {
            world.setBlock(x, 8, z, f.stone);
        }
    }
    // The obsidian pillar at the blast's EXACT center: local intensity
    // = the full radius (3) — obsidian's resistance 3 is NOT < 3, so it
    // survives at the center (the portal's bedrock twin by data).
    world.setBlock(0, 8, 0, f.obsidian);
    // A stone block OUTSIDE the sphere (radius 3, the corner at
    // distance ~5.2 — no blast): untouched.
    world.setBlock(6, 8, 6, f.stone);
    const Vec3 center {0.5f, 8.5f, 0.5f};
    const ExplosionScan scan = scanExplosion(world, f.reg, center, 3.0);
    // The 24 non-center slab cells break (the center is obsidian); the
    // far stone survives.
    EXPECT_EQ(scan.broken.size(), 24u);
    int centerBreaks = 0;
    for (const BlockPos& b : scan.broken) {
        EXPECT_EQ(world.getBlock(b.x, b.y, b.z), f.stone);
        if (b.x == 0 && b.z == 0) {
            ++centerBreaks; // the obsidian center must never break
        }
    }
    EXPECT_EQ(centerBreaks, 0);
    EXPECT_TRUE(scan.ignited.empty());
    EXPECT_EQ(world.getBlock(0, 8, 0), f.obsidian);
    EXPECT_EQ(world.getBlock(6, 8, 6), f.stone);
}

TEST(ExplosionTest, ScanIgnitesFlamableAndBreakWins) {
    ExplosionFixture f;
    World world;
    // A destroyable flammable (log) and a non-destroyable flammable
    // (moss) at the blast's center row: the log BREAKS (the break's
    // precedence — there is nothing left to burn), the moss IGNITES
    // (unbreakable, the flame takes it).
    world.setBlock(0, 8, 0, f.log);
    world.setBlock(1, 8, 0, f.moss);
    const Vec3 center {0.5f, 8.5f, 0.5f};
    const ExplosionScan scan = scanExplosion(world, f.reg, center, 3.0);
    ASSERT_EQ(scan.broken.size(), 1u);
    EXPECT_EQ(world.getBlock(scan.broken[0].x, scan.broken[0].y,
                             scan.broken[0].z),
              f.log);
    ASSERT_EQ(scan.ignited.size(), 1u);
    EXPECT_EQ(world.getBlock(scan.ignited[0].x, scan.ignited[0].y,
                             scan.ignited[0].z),
              f.moss);
}

TEST(ExplosionTest, ScanIsDeterministicAndOrderStable) {
    // An identical run scans identically (the bounding box's loop
    // order — the world journal's order; the apply's seed contract
    // rests on it).
    ExplosionFixture f;
    auto build = [&f]() {
        World world;
        for (int x = -2; x <= 2; ++x) {
            for (int z = -2; z <= 2; ++z) {
                world.setBlock(x, 8, z, (x + z) % 2 == 0 ? f.stone : f.log);
            }
        }
        return world;
    };
    const Vec3 center {0.5f, 8.5f, 0.5f};
    World w1 = build();
    World w2 = build();
    const ExplosionScan s1 = scanExplosion(w1, f.reg, center, 3.0);
    const ExplosionScan s2 = scanExplosion(w2, f.reg, center, 3.0);
    ASSERT_EQ(s1.broken.size(), s2.broken.size());
    for (std::size_t i = 0; i < s1.broken.size(); ++i) {
        EXPECT_EQ(s1.broken[i], s2.broken[i]) << i;
    }
    ASSERT_EQ(s1.ignited.size(), s2.ignited.size());
    for (std::size_t i = 0; i < s1.ignited.size(); ++i) {
        EXPECT_EQ(s1.ignited[i], s2.ignited[i]) << i;
    }
}

TEST(ExplosionTest, ScanNoOpOnEmptyBlast) {
    ExplosionFixture f;
    World world;
    world.setBlock(0, 8, 0, f.stone);
    // A zero-radius blast breaks / ignites nothing (the no-op's twin).
    const Vec3 center {0.5f, 8.5f, 0.5f};
    const ExplosionScan scan = scanExplosion(world, f.reg, center, 0.0);
    EXPECT_TRUE(scan.broken.empty());
    EXPECT_TRUE(scan.ignited.empty());
}
