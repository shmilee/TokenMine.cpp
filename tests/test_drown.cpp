// SURV-2: the oxygen / drowning system — the pure frame-locked step
// (player/drown, the campfire pattern: the unit tests own the logic, the
// Game is the thin adapter) + the head-under-fluid predicate + the
// Player::damage armor bypass (the wiki damage table: Drowning =
// "Armor: No effect / Resistance: Applies") + the format-17 save
// discipline (oxygen always written; version-lie / out-of-range
// rejection; the 1-16 migration loads full air).
#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "block/block_registry.hpp"
#include "core/json.hpp"
#include "core/save.hpp"
#include "core/status_effect.hpp"
#include "item/inventory.hpp"
#include "item/item_registry.hpp"
#include "player/drown.hpp"
#include "player/player.hpp"
#include "world/world.hpp"

using namespace mc;

namespace {

Json parseJson(const std::string& text) {
    auto r = Json::parse(text);
    if (!r.ok()) {
        ADD_FAILURE() << "parse failed: " << r.error().message;
        return Json::object();
    }
    return *r;
}

// The diamond set (the full 20 points): one piece per slot.
const char* const kDiamondDefs[] = {
    R"({"id":"diamond_helmet","type":"material","max_stack":1,)"
    R"("armor_slot":"head","armor_points":3})",
    R"({"id":"diamond_chestplate","type":"material","max_stack":1,)"
    R"("armor_slot":"chest","armor_points":8})",
    R"({"id":"diamond_leggings","type":"material","max_stack":1,)"
    R"("armor_slot":"legs","armor_points":6})",
    R"({"id":"diamond_boots","type":"material","max_stack":1,)"
    R"("armor_slot":"feet","armor_points":3})",
};

// A solid stone floor covering x,z in [-16, 16) at y=0 (top surface
// y=1) — the same fixture shape the player tests use. The fluid cells
// for the headInFluid cases are set per test.
struct Fixture {
    ItemRegistry items;
    BlockRegistry blocks;
    EffectRegistry effects;
    World world;

    Fixture() {
        auto r = blocks.registerBlock(parseJson(
            R"({"id":"stone","model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
        if (!r.ok()) {
            ADD_FAILURE() << r.error().message;
        }
        for (const char* def : kDiamondDefs) {
            auto ir = items.registerItem(parseJson(def));
            if (!ir.ok()) {
                ADD_FAILURE() << def << ": " << ir.error().message;
            }
        }
        for (int x = -16; x < 16; ++x) {
            for (int z = -16; z < 16; ++z) {
                world.setBlock(x, 0, z, blocks.idForName("stone"));
            }
        }
    }

    void wearFullDiamond(Player& p) const {
        p.setArmorSlot(0,
                       ItemStack {items.idForName("diamond_helmet"), 1}, 3);
        p.setArmorSlot(1,
                       ItemStack {items.idForName("diamond_chestplate"), 1}, 8);
        p.setArmorSlot(2,
                       ItemStack {items.idForName("diamond_leggings"), 1}, 6);
        p.setArmorSlot(3, ItemStack {items.idForName("diamond_boots"), 1}, 3);
    }
};

// Copies a JSON object minus one key (Json has no erase).
Json withoutKey(const Json& root, const std::string& key) {
    Json out = Json::object();
    for (const auto& [k, v] : root.members()) {
        if (k != key) {
            out[k] = v;
        }
    }
    return out;
}

// CRAFT-1: the empty legacy "table_grid" array (9 slot entries) — a
// pre-format-19 file carries it (required in format 3-18); a downgrade
// of a current save restores it for the legacy parse.
Json emptyTableGrid() {
    Json arr = Json::array();
    for (int i = 0; i < CraftingGrid::kSize; ++i) {
        Json e = Json::object();
        e["item"] = Json(std::string(""));
        e["count"] = Json(0);
        arr.push_back(std::move(e));
    }
    return arr;
}

// A complete, minimal save (empty inventory / no world edits): the
// format-17 parse requires every section, serializeSave writes them all.
SaveGame makeSave() {
    SaveGame s;
    s.seed = 42;
    s.time = 0.5;
    s.playerPos = {1.5f, 2.0f, 1.5f};
    s.playerAbsorption = 0.0f; // format 12+ requires the field
    s.slots.resize(Inventory::kTotalSize);
    return s;
}

} // namespace

// -- drownStep: the frame-locked air rhythm ----------------------------------

TEST(DrownStepTest, BreatheRefillsFull) {
    // The head out of the fluid: the air refills (Minecraft: the surface
    // is instant) and the return is 0.
    DrownState s;
    s.oxygen = 37;
    s.drownTicks = 55;
    EXPECT_FLOAT_EQ(drownStep(s, false, 1234u), 0.0f);
    EXPECT_EQ(s.oxygen, kOxygenMax);
}

TEST(DrownStepTest, DecayOnTheSixFramePhase) {
    // From full air: one unit per 6 frames on the GLOBAL phase
    // (frame % 6) — frames 1..5 between decays change nothing.
    DrownState s;
    EXPECT_FLOAT_EQ(drownStep(s, true, 0u), 0.0f);
    EXPECT_EQ(s.oxygen, kOxygenMax - 1);
    for (unsigned f = 1u; f <= 5u; ++f) {
        EXPECT_FLOAT_EQ(drownStep(s, true, f), 0.0f) << "frame " << f;
        EXPECT_EQ(s.oxygen, kOxygenMax - 1) << "frame " << f;
    }
    EXPECT_FLOAT_EQ(drownStep(s, true, 6u), 0.0f);
    EXPECT_EQ(s.oxygen, kOxygenMax - 2);
    for (unsigned f = 7u; f <= 11u; ++f) {
        EXPECT_EQ(s.oxygen, kOxygenMax - 2) << "frame " << f;
        drownStep(s, true, f);
    }
    EXPECT_EQ(s.oxygen, kOxygenMax - 2);
    drownStep(s, true, 12u);
    EXPECT_EQ(s.oxygen, kOxygenMax - 3);
}

TEST(DrownStepTest, ExhaustsAtFrame894) {
    // The design timeline: head under fluid from frame 0, oxygen 150 ->
    // one unit per 6 frames -> 149 decays land on frames 0..888 (the
    // air is 1), the 150th on frame 894 (the air is 0). No damage until
    // the accumulator runs (it starts at 895).
    DrownState s;
    for (unsigned f = 0u; f <= 893u; ++f) {
        EXPECT_EQ(drownStep(s, true, f), 0.0f) << "frame " << f;
    }
    EXPECT_EQ(s.oxygen, 1);
    EXPECT_EQ(s.drownTicks, 0);
    EXPECT_FLOAT_EQ(drownStep(s, true, 894u), 0.0f);
    EXPECT_EQ(s.oxygen, 0);
    // And the accumulator is still counting (no damage yet): the first
    // 150 frames of exhaustion (895..1044) deal nothing before frame
    // 1044 (pinned by FirstDamageAtFrame1044).
    for (unsigned f = 895u; f <= 1043u; ++f) {
        EXPECT_EQ(drownStep(s, true, f), 0.0f) << "frame " << f;
    }
    EXPECT_EQ(s.drownTicks, 149);
}

TEST(DrownStepTest, FirstDamageAtFrame1044) {
    // From full air: the first 2.0 damage lands on frame 1044
    // (894 + 150), and the accumulator resets with it.
    DrownState s;
    for (unsigned f = 0u; f < 1044u; ++f) {
        EXPECT_EQ(drownStep(s, true, f), 0.0f) << "frame " << f;
    }
    EXPECT_FLOAT_EQ(drownStep(s, true, 1044u), kDrownDamage);
    EXPECT_EQ(s.oxygen, 0);
    EXPECT_EQ(s.drownTicks, 0);
}

TEST(DrownStepTest, DamageEvery150Frames) {
    // The sustained rhythm over a full 1500-frame dive: the damages
    // land EXACTLY on 1044 / 1194 / 1344 / 1494 (4 hits = 8.0, the
    // design's oxygen-150 timeline: hp 12.0 at frame 1500).
    DrownState s;
    std::vector<unsigned> hits;
    for (unsigned f = 0u; f < 1500u; ++f) {
        if (drownStep(s, true, f) > 0.0f) {
            hits.push_back(f);
        }
    }
    EXPECT_EQ(hits,
              (std::vector<unsigned> {1044u, 1194u, 1344u, 1494u}));
}

TEST(DrownStepTest, PresetZeroDamagesFromFrame149) {
    // A save preset to oxygen 0 (the E2E S2 scene): the accumulator
    // starts at 0, so the damages land on 149 / 299 / ... / 1499 —
    // 10 hits of 2.0 = 20.0 = the death frame 1499.
    DrownState s;
    s.oxygen = 0;
    std::vector<unsigned> hits;
    for (unsigned f = 0u; f < 1500u; ++f) {
        if (drownStep(s, true, f) > 0.0f) {
            hits.push_back(f);
        }
    }
    EXPECT_EQ(hits, (std::vector<unsigned> {149u, 299u, 449u, 599u,
                                            749u, 899u, 1049u, 1199u,
                                            1349u, 1499u}));
}

TEST(DrownStepTest, SurfacingRefillsAndResetsTicks) {
    // Out of the fluid mid-accumulation: the air refills AND the
    // drownTicks reset (a re-dive starts the 150-frame count fresh).
    DrownState s;
    s.oxygen = 0;
    s.drownTicks = 88;
    EXPECT_FLOAT_EQ(drownStep(s, false, 1000u), 0.0f);
    EXPECT_EQ(s.oxygen, kOxygenMax);
    EXPECT_EQ(s.drownTicks, 0);
}

TEST(DrownStepTest, DeadPlayerIsInert) {
    // The adapter contract (the Game's pass): after a death the step
    // keeps ticking but the damage no-ops (Player::damage on the
    // frozen body returns false) — health stays 0, the dead state
    // holds.
    Fixture f;
    Player p(f.world, f.blocks, Vec3 {8.5f, 1.0f, 8.5f});
    ASSERT_TRUE(p.damage(20.0f));
    ASSERT_TRUE(p.isDead());
    DrownState s;
    s.oxygen = 0;
    unsigned hits = 0;
    for (unsigned fr = 0u; fr < 300u; ++fr) {
        const float dmg = drownStep(s, true, fr);
        if (dmg > 0.0f) {
            ++hits;
            EXPECT_FALSE(p.damage(dmg, /*bypassArmor=*/true)) << "frame "
                                                              << fr;
        }
    }
    EXPECT_TRUE(p.isDead());
    EXPECT_FLOAT_EQ(p.health(), 0.0f);
    // The state itself kept running (harmlessly): 300 frames from a
    // zeroed accumulator = exactly two full damage cycles (frames 149
    // and 299), the accumulator reset with the last one.
    EXPECT_EQ(hits, 2u);
    EXPECT_EQ(s.drownTicks, 0);
}

// -- headInFluid: the eyes-in-the-fluid predicate -----------------------------

TEST(HeadInFluidTest, EyesInWaterDrowns) {
    Fixture f;
    // The player at y=1 (feet on the floor top): the eyes are at
    // 1 + 1.62 = 2.62 -> cell (8, 2, 8). Water there = under the eyes.
    ASSERT_TRUE(f.world.setFluid(8, 2, 8, FluidKind::Water, kMaxFluidLevel));
    Player p(f.world, f.blocks, Vec3 {8.5f, 1.0f, 8.5f});
    EXPECT_TRUE(headInFluid(p, f.world));
}

TEST(HeadInFluidTest, EyesInLavaDrown) {
    // Minecraft: lava drowns the same way (any fluid kind).
    Fixture f;
    ASSERT_TRUE(f.world.setFluid(8, 2, 8, FluidKind::Lava, kMaxFluidLevel));
    Player p(f.world, f.blocks, Vec3 {8.5f, 1.0f, 8.5f});
    EXPECT_TRUE(headInFluid(p, f.world));
}

TEST(HeadInFluidTest, EyesAboveTheSurfaceBreathe) {
    // The 1-deep pool: the water fills cell y=1 (the surface plane is
    // y=2). The standing eyes (2.62) are ABOVE the surface -> no
    // decay (the wading / surface-float case).
    Fixture f;
    ASSERT_TRUE(f.world.setFluid(8, 1, 8, FluidKind::Water, kMaxFluidLevel));
    Player p(f.world, f.blocks, Vec3 {8.5f, 1.0f, 8.5f});
    EXPECT_FALSE(headInFluid(p, f.world));
    // ...and one cell deeper (a 2-deep pool, the eyes IN the water)
    // drowns: the predicate is the eye cell, not the feet.
    ASSERT_TRUE(f.world.setFluid(8, 2, 8, FluidKind::Water, kMaxFluidLevel));
    EXPECT_TRUE(headInFluid(p, f.world));
}

TEST(HeadInFluidTest, OutOfRangeAndUncreatedAreAir) {
    Fixture f;
    // The eye cell above the world (y >= kWorldHeight) reads as air.
    Player high(f.world, f.blocks, Vec3 {8.5f, 126.5f, 8.5f});
    EXPECT_FALSE(headInFluid(high, f.world));
    // A column in a never-created chunk reads as air (the World
    // convention — no fluid was ever set there).
    Player far(f.world, f.blocks, Vec3 {80.5f, 1.0f, 80.5f});
    EXPECT_FALSE(headInFluid(far, f.world));
}

// -- Player::damage: the armor bypass (the drowning exemption) ----------------

TEST(DrownDamageTest, FullDiamondArmorBypassIsExact) {
    // 20 points (the full diamond set): damage(2.0, true) applies the
    // FULL 2.0 (the wiki table: Drowning = "Armor: No effect"). The
    // default path on the same body is the contrast — the floor rule
    // lands floor(2.0 x (100 - 4 x 20)) / 100 = 0.4, so 18.0 vs 19.6
    // are mutually exclusive.
    Fixture f;
    Player p(f.world, f.blocks, Vec3 {8.5f, 1.0f, 8.5f});
    f.wearFullDiamond(p);
    ASSERT_EQ(p.armorPoints(), 20);
    ASSERT_TRUE(p.damage(kDrownDamage, /*bypassArmor=*/true));
    EXPECT_FLOAT_EQ(p.health(), 18.0f); // 2.0 exact

    Player q(f.world, f.blocks, Vec3 {8.5f, 1.0f, 8.5f});
    f.wearFullDiamond(q);
    ASSERT_TRUE(q.damage(kDrownDamage)); // the default false: armor applies
    EXPECT_FLOAT_EQ(q.health(), 19.6f); // floor(40)/100 = 0.4
}

TEST(DrownDamageTest, ResistanceAndPoolStillApply) {
    // The bypass skips the armor step ONLY (the wiki table:
    // "Resistance: Applies"): the 20% resistance lands on the 2.0
    // (-> 1.6), and the absorption pool drains it FIRST (health
    // untouched while the pool covers it). The expected values are
    // written as the SAME float expressions the pipeline computes
    // (the existing damage tests' convention — 0.2f is not
    // binary-exact).
    Fixture f;
    Player p(f.world, f.blocks, Vec3 {8.5f, 1.0f, 8.5f});
    p.setDamageReduction(0.2f);
    p.setAbsorption(4.0f);
    const float hit = kDrownDamage * (1.0f - 0.2f);
    EXPECT_FLOAT_EQ(hit, 1.6f); // resistance 0.2 -> 1.6 (the design pin)
    ASSERT_TRUE(p.damage(kDrownDamage, /*bypassArmor=*/true));
    EXPECT_FLOAT_EQ(p.absorption(), 4.0f - hit);
    EXPECT_FLOAT_EQ(p.health(), 20.0f); // the pool covered it
    ASSERT_TRUE(p.damage(kDrownDamage, /*bypassArmor=*/true));
    EXPECT_FLOAT_EQ(p.absorption(), 4.0f - hit - hit);
    EXPECT_FLOAT_EQ(p.health(), 20.0f);
    // The third hit empties the pool and the remainder touches health.
    ASSERT_TRUE(p.damage(kDrownDamage, /*bypassArmor=*/true));
    EXPECT_FLOAT_EQ(p.absorption(), 0.0f);
    EXPECT_FLOAT_EQ(p.health(), 20.0f - (hit - (4.0f - hit - hit)));
}

// -- save format 17: the player's oxygen --------------------------------------

TEST(DrownSaveTest, OxygenRoundTrips) {
    // A mid-dive value survives the serialize/parse round trip, and the
    // key is ALWAYS written (the armor-key discipline).
    Fixture f;
    SaveGame s = makeSave();
    s.playerOxygen = 123;
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok()) << j.error().message;
    const Json* oxygen = j->find("player")->find("oxygen");
    ASSERT_TRUE(oxygen != nullptr && oxygen->isNumber());
    EXPECT_EQ(oxygen->asInt(), 123);
    auto out = parseSave(*j, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    EXPECT_EQ(out->playerOxygen, 123);
    // A format-17 file WITHOUT the key is rejected (named).
    {
        Json root = *j;
        root["player"] = withoutKey(root["player"], "oxygen");
        auto bad = parseSave(root, f.items, f.blocks, f.effects, 42);
        ASSERT_FALSE(bad.ok());
        EXPECT_NE(bad.error().message.find("player.oxygen"), std::string::npos)
            << bad.error().message;
        EXPECT_NE(bad.error().message.find("format 17"), std::string::npos)
            << bad.error().message;
    }
}

TEST(DrownSaveTest, Format16RejectsTheOxygenKey) {
    // The version-lie discipline (the armor twin): a format-16 file
    // carrying the oxygen key is rejected (named); the same file
    // WITHOUT the key loads and the sentinel stays -1 (the migration:
    // the player had full air — the system did not exist).
    Fixture f;
    SaveGame s = makeSave();
    s.playerOxygen = 87;
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok()) << j.error().message;
    {
        Json root = *j;
        root["format"] = Json(16); // the "oxygen" key stays -> rejected
        root["player"] = withoutKey(root["player"], "stats"); // SURV-7: the oxygen lie is the rejection under test
        auto bad = parseSave(root, f.items, f.blocks, f.effects, 42);
        ASSERT_FALSE(bad.ok());
        EXPECT_NE(bad.error().message.find("player.oxygen"),
                  std::string::npos)
            << bad.error().message;
        EXPECT_NE(bad.error().message.find("format 17 or newer"),
                  std::string::npos)
            << bad.error().message;
    }
    {
        Json root = *j;
        root["format"] = Json(16);
        root["player"] = withoutKey(root["player"], "stats"); // SURV-7: the stats key is format 21
        root = withoutKey(root, "tables"); // CRAFT-1: format 16 predates the per-block table grids
        root["table_grid"] = emptyTableGrid(); // CRAFT-1: format 16 still carried the global table grid
        root["player"] = withoutKey(root["player"], "oxygen");
        auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
        ASSERT_TRUE(out.ok()) << out.error().message;
        EXPECT_EQ(out->playerOxygen, -1);
    }
}

TEST(DrownSaveTest, Format15MigratesFullAir) {
    // A format-15 load (armor + oxygen stripped — both postdate it)
    // parses with the -1 sentinel: Game::applySave migrates it to the
    // full air (kOxygenMax).
    Fixture f;
    SaveGame s = makeSave();
    s.playerOxygen = 12;
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok()) << j.error().message;
    Json root = *j;
    root["format"] = Json(15);
    root["player"] = withoutKey(root["player"], "stats"); // SURV-7: the stats key is format 21
    root = withoutKey(root, "tables"); // CRAFT-1: format 15 predates the per-block table grids
    root["table_grid"] = emptyTableGrid(); // CRAFT-1: format 15 still carried the global table grid
    root["player"] = withoutKey(root["player"], "oxygen");
    root["player"] = withoutKey(root["player"], "armor");
    auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    EXPECT_EQ(out->playerOxygen, -1);
}

TEST(DrownSaveTest, OxygenBoundsRejected) {
    // Out-of-range / non-integer values are rejected and named: the
    // cap is kOxygenMax (150), the floor 0, and the value must be an
    // integer (the RFC parser cannot produce non-finite numbers, so
    // "number" already implies finite).
    Fixture f;
    SaveGame s = makeSave();
    s.playerOxygen = 123;
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok()) << j.error().message;
    const std::vector<int> overCap {kOxygenMax + 1, 1000};
    for (const int bad : overCap) {
        Json root = *j;
        root["player"]["oxygen"] = Json(bad);
        auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
        ASSERT_FALSE(out.ok()) << bad;
        EXPECT_NE(out.error().message.find("player.oxygen"), std::string::npos)
            << out.error().message;
        EXPECT_NE(out.error().message.find(std::to_string(bad)),
                  std::string::npos)
            << out.error().message;
    }
    Json rootNeg = *j;
    rootNeg["player"]["oxygen"] = Json(-1);
    auto neg = parseSave(rootNeg, f.items, f.blocks, f.effects, 42);
    ASSERT_FALSE(neg.ok());
    EXPECT_NE(neg.error().message.find("player.oxygen"), std::string::npos)
        << neg.error().message;
    Json rootFrac = *j;
    rootFrac["player"]["oxygen"] = Json(1.5);
    auto frac = parseSave(rootFrac, f.items, f.blocks, f.effects, 42);
    ASSERT_FALSE(frac.ok());
    EXPECT_NE(frac.error().message.find("integer"), std::string::npos)
        << frac.error().message;
}
