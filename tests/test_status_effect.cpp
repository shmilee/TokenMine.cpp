// Phase 46 status-effect tests: the EffectRegistry's strict parse
// (tick fields, the min_health damage-only rule, the melee penalty, the
// no-op rejection, unknown fields, duplicate ids) and the StatusEffects
// state machine (application/re-application, the real-dt tick crossing —
// one per interval, a large dt crosses several — the poison min_health
// floor, the heal clamp at max, clearNegative keeping the positives,
// clearAll, expiry, the melee-penalty sum). The save format-8 round trip
// lives in test_save.cpp; the mob on_hit_effect parse in test_mob.cpp.
#include <gtest/gtest.h>

#include <cstdio>
#include <string>
#include <vector>

#include "core/json.hpp"
#include "core/status_effect.hpp"

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

// The shipped effect set (data/effects/*.json, one file per effect).
EffectRegistry makeRegistry() {
    EffectRegistry reg;
    const struct {
        const char* def;
    } defs[] = {
        {R"({"id":"poison","tick_type":"damage","tick_amount":1.0,
            "tick_interval":2.5,"min_health":1.0,"negative":true,"icon":0})"},
        {R"({"id":"weakness","melee_penalty":4.0,"negative":true,"icon":1})"},
        {R"({"id":"regeneration","tick_type":"heal","tick_amount":1.0,
            "tick_interval":4.5,"negative":false,"icon":2})"},
        {R"({"id":"absorption","absorb_hp":4,"negative":false,"icon":3})"},
        {R"({"id":"fire","tick_type":"damage","tick_amount":1.0,
            "tick_interval":0.2,"min_health":0.0,"negative":false,
            "extinguished_by_water":true,"ignites_on_lava":true,
            "icon":4})"},
    };
    for (const auto& d : defs) {
        auto r = reg.registerEffect(parseJson(d.def));
        if (!r.ok()) {
            ADD_FAILURE() << d.def << ": " << r.error().message;
        }
    }
    return reg;
}

} // namespace

// -- EffectRegistry::parseEffect ------------------------------------------------

TEST(EffectParse, ValidPoison) {
    Effect e;
    auto r = EffectRegistry::parseEffect(
        parseJson(R"({"id":"poison","tick_type":"damage","tick_amount":1.0,
                     "tick_interval":2.5,"min_health":1.0,"negative":true,
                     "icon":0})"),
        e);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(e.name, "poison");
    EXPECT_EQ(e.tick, Effect::Tick::Damage);
    EXPECT_FLOAT_EQ(e.tickAmount, 1.0f);
    EXPECT_FLOAT_EQ(e.tickInterval, 2.5f);
    EXPECT_FLOAT_EQ(e.minHealth, 1.0f);
    EXPECT_FLOAT_EQ(e.meleePenalty, 0.0f);
    EXPECT_TRUE(e.negative);
    EXPECT_EQ(e.icon, 0);
}

TEST(EffectParse, ValidWeaknessNoTicks) {
    Effect e;
    auto r = EffectRegistry::parseEffect(
        parseJson(R"({"id":"weakness","melee_penalty":4.0,"negative":true,
                     "icon":1})"),
        e);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(e.tick, Effect::Tick::None);
    EXPECT_FLOAT_EQ(e.meleePenalty, 4.0f);
    EXPECT_TRUE(e.negative);
}

TEST(EffectParse, ValidRegeneration) {
    Effect e;
    auto r = EffectRegistry::parseEffect(
        parseJson(R"({"id":"regeneration","tick_type":"heal",
                     "tick_amount":1.0,"tick_interval":4.5,"icon":2})"),
        e);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(e.tick, Effect::Tick::Heal);
    EXPECT_FALSE(e.negative);  // absent = false (survives the milk)
}

TEST(EffectParse, Rejections) {
    const std::vector<std::string> bad = {
        R"({"id":""})",                                            // empty id
        R"({"id":"x","icon":0})",                                  // no-op
        R"({"id":"x","tick_type":"damage","tick_interval":2.5,"icon":0})", //
        // missing tick_amount
        R"({"id":"x","tick_type":"damage","tick_amount":1.0,"icon":0})", //
        // missing tick_interval
        R"({"id":"x","tick_type":"damage","tick_amount":0.0,
           "tick_interval":2.5,"icon":0})",                        // amount 0
        R"({"id":"x","tick_type":"damage","tick_amount":1.0,
           "tick_interval":0.0,"icon":0})",                        // interval 0
        R"({"id":"x","tick_type":"damage","tick_amount":10.1,
           "tick_interval":2.5,"icon":0})",                        // amount > 10
        R"({"id":"x","tick_type":"heal","tick_amount":1.0,
           "tick_interval":4.5,"min_health":1.0,"icon":0})",       //
        // min_health on a heal
        R"({"id":"x","tick_type":"damage","tick_amount":1.0,
           "tick_interval":2.5,"min_health":1.1,"icon":0})",       // floor > 1
        R"({"id":"x","tick_type":"wat","tick_amount":1.0,
           "tick_interval":2.5,"icon":0})",                        // bad type
        R"({"id":"x","tick_amount":1.0,"tick_interval":2.5,"icon":0})", //
        // tick fields without tick_type
        R"({"id":"x","melee_penalty":-1.0,"icon":0})",             // penalty < 0
        R"({"id":"x","melee_penalty":4.0,"icon":-1})",             // icon < 0
        R"({"id":"x","melee_penalty":4.0,"icon":1.5})",            // icon not int
        R"({"id":"x","melee_penalty":4.0,"icon":1,"extra":true})", // unknown
        R"([1, 2, 3])",                                            // not an object
    };
    for (const std::string& text : bad) {
        Effect e;
        auto r = EffectRegistry::parseEffect(parseJson(text), e);
        EXPECT_FALSE(r.ok()) << "accepted: " << text;
    }
}

// -- Phase 56: the absorption layer (absorb_hp) --------------------------------

TEST(EffectParse, ValidAbsorption) {
    Effect e;
    auto r = EffectRegistry::parseEffect(
        parseJson(R"({"id":"absorption","absorb_hp":4,"negative":false,
                     "icon":3})"),
        e);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(e.tick, Effect::Tick::None);   // a modifier (no ticks)
    EXPECT_FLOAT_EQ(e.absorbHp, 4.0f);
    EXPECT_FALSE(e.negative);                // survives the milk
    EXPECT_EQ(e.icon, 3);
}

TEST(EffectParse, AbsorbHpRange) {
    const std::vector<std::string> bad = {
        R"({"id":"x","absorb_hp":0,"icon":3})",        // below 1
        R"({"id":"x","absorb_hp":65,"icon":3})",       // above 64
        R"({"id":"x","absorb_hp":1.5,"icon":3})",      // not an integer
        R"({"id":"x","absorb_hp":"4","icon":3})",      // not a number
    };
    for (const std::string& def : bad) {
        Effect e;
        auto r = EffectRegistry::parseEffect(parseJson(def), e);
        EXPECT_FALSE(r.ok()) << def;
        EXPECT_NE(r.error().message.find("[1, 64]"), std::string::npos)
            << def << ": " << r.error().message;
    }
    // The boundaries themselves are legal.
    Effect lo, hi;
    EXPECT_TRUE(EffectRegistry::parseEffect(
        parseJson(R"({"id":"x","absorb_hp":1,"icon":3})"), lo).ok());
    EXPECT_TRUE(EffectRegistry::parseEffect(
        parseJson(R"({"id":"x","absorb_hp":64,"icon":3})"), hi).ok());
}

TEST(EffectParse, AbsorbHpAloneIsNotNoOp) {
    // A layer-only effect (no ticks, no penalty) is a valid effect — the
    // no-op rejection must not fire.
    Effect e;
    auto r = EffectRegistry::parseEffect(
        parseJson(R"({"id":"x","absorb_hp":4,"icon":3})"), e);
    ASSERT_TRUE(r.ok()) << r.error().message;
    // ...but an effect with NONE of the three actions still is a no-op.
    Effect noop;
    auto rNoop = EffectRegistry::parseEffect(
        parseJson(R"({"id":"x","icon":3})"), noop);
    ASSERT_FALSE(rNoop.ok());
    EXPECT_NE(rNoop.error().message.find("no-op"), std::string::npos);
    EXPECT_NE(rNoop.error().message.find("absorb_hp"), std::string::npos);
}

TEST(StatusEffects, TotalAbsorption) {
    const EffectRegistry reg = makeRegistry();
    StatusEffects se;
    EXPECT_FLOAT_EQ(se.totalAbsorption(reg), 0.0f);
    se.apply(reg.idForName("absorption"), 5.0);
    EXPECT_FLOAT_EQ(se.totalAbsorption(reg), 4.0f);
    // Poison/regeneration contribute 0 to the layer.
    se.apply(reg.idForName("poison"), 15.0);
    se.apply(reg.idForName("regeneration"), 10.0);
    EXPECT_FLOAT_EQ(se.totalAbsorption(reg), 4.0f);
    // Re-application resets the duration, not the layer.
    se.apply(reg.idForName("absorption"), 8.0);
    EXPECT_FLOAT_EQ(se.totalAbsorption(reg), 4.0f);
    se.clearAll();
    EXPECT_FLOAT_EQ(se.totalAbsorption(reg), 0.0f);
}

// -- Phase 57: the fire system (lava ignition + water extinguish) --------

TEST(EffectParse, ValidFire) {
    // The shipped fire effect (data/effects/fire.json): the burn tick
    // (Minecraft's 1 HP / 0.2 s, may kill), extinguished by water,
    // ignites the player from the lava.
    Effect e;
    auto r = EffectRegistry::parseEffect(
        parseJson(R"({"id":"fire","tick_type":"damage","tick_amount":1.0,
                     "tick_interval":0.2,"min_health":0.0,"negative":false,
                     "extinguished_by_water":true,"ignites_on_lava":true,
                     "icon":4})"),
        e);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(e.tick, Effect::Tick::Damage);
    EXPECT_FLOAT_EQ(e.tickAmount, 1.0f);
    EXPECT_FLOAT_EQ(e.tickInterval, 0.2f);
    EXPECT_FLOAT_EQ(e.minHealth, 0.0f); // no floor: the burn may kill
    EXPECT_FALSE(e.negative);           // milk does NOT put out a fire
    EXPECT_TRUE(e.ignitesOnLava);
    EXPECT_TRUE(e.extinguishedByWater);
    EXPECT_EQ(e.icon, 4);
}

TEST(EffectParse, FireFlagsDefaultFalse) {
    // Pre-Phase-57 effect files carry no flags: both default to false
    // (poison does not ignite from lava, nothing is water-extinguished).
    const EffectRegistry reg = makeRegistry();
    const Effect& poison = *reg.findByName("poison");
    EXPECT_FALSE(poison.ignitesOnLava);
    EXPECT_FALSE(poison.extinguishedByWater);
    const Effect& fire = *reg.findByName("fire");
    EXPECT_TRUE(fire.ignitesOnLava);
    EXPECT_TRUE(fire.extinguishedByWater);
}

TEST(EffectParse, FireFlagsNonBoolRejected) {
    const std::vector<std::string> bad = {
        R"({"id":"x","tick_type":"damage","tick_amount":1,
            "tick_interval":1,"ignites_on_lava":"yes","icon":0})",
        R"({"id":"x","tick_type":"damage","tick_amount":1,
            "tick_interval":1,"extinguished_by_water":1,"icon":0})",
    };
    for (const std::string& def : bad) {
        Effect e;
        auto r = EffectRegistry::parseEffect(parseJson(def), e);
        EXPECT_FALSE(r.ok()) << def;
        EXPECT_NE(r.error().message.find("must be a boolean"),
                  std::string::npos)
            << def << ": " << r.error().message;
    }
}

TEST(StatusEffects, ClearExtinguishable) {
    const EffectRegistry reg = makeRegistry();
    StatusEffects se;
    se.apply(reg.idForName("poison"), 15.0);
    se.apply(reg.idForName("fire"), 8.0);
    se.apply(reg.idForName("regeneration"), 10.0);
    EXPECT_EQ(se.count(), 3u);
    // Water puts out the fire, keeps the poison and the regeneration.
    se.clearExtinguishable(reg);
    ASSERT_EQ(se.count(), 2u);
    const int fireId = reg.idForName("fire");
    for (const ActiveEffect& a : se.list()) {
        EXPECT_NE(a.effect, fireId)
            << "the fire must be gone after clearExtinguishable";
    }
    // A second pass is a no-op.
    se.clearExtinguishable(reg);
    EXPECT_EQ(se.count(), 2u);
}

TEST(EffectRegistry, DuplicateIdRejected) {
    EffectRegistry reg;
    auto r1 = reg.registerEffect(
        parseJson(R"({"id":"poison","melee_penalty":1.0,"icon":0})"));
    ASSERT_TRUE(r1.ok());
    auto r2 = reg.registerEffect(
        parseJson(R"({"id":"poison","melee_penalty":2.0,"icon":1})"));
    EXPECT_FALSE(r2.ok());
    EXPECT_EQ(reg.count(), 1u);
}

TEST(EffectRegistry, Lookups) {
    const EffectRegistry reg = makeRegistry();
    EXPECT_EQ(reg.count(), 5u);
    EXPECT_EQ(reg.idForName("poison"), 0);
    EXPECT_EQ(reg.idForName("weakness"), 1);
    EXPECT_EQ(reg.idForName("regeneration"), 2);
    EXPECT_EQ(reg.idForName("absorption"), 3);
    EXPECT_EQ(reg.idForName("fire"), 4);
    EXPECT_EQ(reg.idForName("nope"), -1);
    EXPECT_NE(reg.findByName("poison"), nullptr);
    EXPECT_EQ(reg.find(-1), nullptr);
    EXPECT_EQ(reg.find(99), nullptr);
}

// -- StatusEffects ------------------------------------------------------------

TEST(StatusEffects, ApplyAndReapplyResetsDuration) {
    const EffectRegistry reg = makeRegistry();
    StatusEffects se;
    se.apply(reg.idForName("poison"), 15.0);
    EXPECT_EQ(se.count(), 1u);
    se.apply(reg.idForName("regeneration"), 30.0);
    EXPECT_EQ(se.count(), 2u);
    // Re-application RESETS the duration (the tick phase is kept —
    // advance 1.0 s first, then re-apply: the next poison tick must land
    // 1.5 s later, not 2.5 s).
    se.update(1.0, reg, [](const Effect&) {});
    se.apply(reg.idForName("poison"), 15.0);
    int ticks = 0;
    se.update(1.5, reg, [&](const Effect& e) {
        if (e.name == "poison") {
            ++ticks;
        }
    });
    EXPECT_EQ(ticks, 1);
    se.update(0.1, reg, [&](const Effect& e) {
        if (e.name == "poison") {
            ++ticks;
        }
    });
    EXPECT_EQ(ticks, 1);  // 1.5 s after the re-apply, not 2.5
}

TEST(StatusEffects, DamageTickCrossing) {
    const EffectRegistry reg = makeRegistry();
    StatusEffects se;
    se.apply(reg.idForName("poison"), 10.0);
    // One dt crossing two intervals fires two ticks (a large dt).
    int ticks = 0;
    se.update(5.1, reg, [&](const Effect&) { ++ticks; });
    EXPECT_EQ(ticks, 2);
    // The remaining counts down in real dt.
    EXPECT_NEAR(se.list().at(0).remaining, 4.9, 1e-9);
    // Expiry: no tick on the frame the effect dies.
    se.update(4.9, reg, [&](const Effect&) { ++ticks; });
    EXPECT_EQ(se.count(), 0u);
    EXPECT_EQ(ticks, 2);
}

TEST(StatusEffects, PoisonFloorIsNotAppliedHere) {
    // The min_health floor is the GAME's check (it needs the player's
    // current health) — StatusEffects fires the tick and the callback
    // decides. Verify the tick fires at the player health the callback
    // sees (the callback is where the floor lives; see Game::updateEffects).
    const EffectRegistry reg = makeRegistry();
    StatusEffects se;
    se.apply(reg.idForName("poison"), 10.0);
    int fired = 0;
    float amount = -1.0f;
    se.update(2.5, reg, [&](const Effect& e) {
        ++fired;
        amount = e.tickAmount;
        EXPECT_FLOAT_EQ(e.minHealth, 1.0f);
    });
    EXPECT_EQ(fired, 1);
    EXPECT_FLOAT_EQ(amount, 1.0f);
}

TEST(StatusEffects, HealTick) {
    const EffectRegistry reg = makeRegistry();
    StatusEffects se;
    se.apply(reg.idForName("regeneration"), 10.0);
    int healed = 0;
    se.update(4.5, reg, [&](const Effect& e) {
        if (e.tick == Effect::Tick::Heal) {
            ++healed;
        }
    });
    EXPECT_EQ(healed, 1);
}

TEST(StatusEffects, ClearNegativeKeepsPositives) {
    const EffectRegistry reg = makeRegistry();
    StatusEffects se;
    se.apply(reg.idForName("poison"), 15.0);
    se.apply(reg.idForName("weakness"), 15.0);
    se.apply(reg.idForName("regeneration"), 30.0);
    se.clearNegative(reg);
    ASSERT_EQ(se.count(), 1u);
    EXPECT_EQ(se.list().at(0).effect, reg.idForName("regeneration"));
    se.clearAll();
    EXPECT_EQ(se.count(), 0u);
}

TEST(StatusEffects, TotalMeleePenalty) {
    const EffectRegistry reg = makeRegistry();
    StatusEffects se;
    EXPECT_FLOAT_EQ(se.totalMeleePenalty(reg), 0.0f);
    se.apply(reg.idForName("weakness"), 15.0);
    EXPECT_FLOAT_EQ(se.totalMeleePenalty(reg), 4.0f);
    // Poison/regeneration contribute 0.
    se.apply(reg.idForName("poison"), 15.0);
    EXPECT_FLOAT_EQ(se.totalMeleePenalty(reg), 4.0f);
}

// ---------------------------------------------------------------------------
// Phase 84: the beacon's other primary effects (speed / strength /
// resistance) — the modifier fields on the effect data.
// ---------------------------------------------------------------------------

TEST(StatusEffectParse, Phase84ModifierFields) {
    Effect e;
    // speed_multiplier in (1.0, 3.0]; melee_bonus in [0, 100];
    // damage_reduction in [0.0, 1.0).
    ASSERT_TRUE(EffectRegistry::parseEffect(
        parseJson(R"({"id":"s","speed_multiplier":1.2,"icon":0})"), e).ok());
    EXPECT_FLOAT_EQ(e.speedMultiplier, 1.2f);
    ASSERT_TRUE(EffectRegistry::parseEffect(
        parseJson(R"({"id":"s","melee_bonus":2.0,"icon":0})"), e).ok());
    EXPECT_FLOAT_EQ(e.meleeBonus, 2.0f);
    ASSERT_TRUE(EffectRegistry::parseEffect(
        parseJson(R"({"id":"s","damage_reduction":0.2,"icon":0})"), e).ok());
    EXPECT_FLOAT_EQ(e.damageReduction, 0.2f);
    // Out-of-range values are rejected and named.
    EXPECT_FALSE(EffectRegistry::parseEffect(
        parseJson(R"({"id":"s","speed_multiplier":1.0,"icon":0})"), e).ok());
    EXPECT_FALSE(EffectRegistry::parseEffect(
        parseJson(R"({"id":"s","speed_multiplier":3.5,"icon":0})"), e).ok());
    EXPECT_FALSE(EffectRegistry::parseEffect(
        parseJson(R"({"id":"s","melee_bonus":-1.0,"icon":0})"), e).ok());
    EXPECT_FALSE(EffectRegistry::parseEffect(
        parseJson(R"({"id":"s","damage_reduction":1.0,"icon":0})"), e).ok());
    EXPECT_FALSE(EffectRegistry::parseEffect(
        parseJson(R"({"id":"s","damage_reduction":-0.1,"icon":0})"), e).ok());
}

TEST(StatusEffectParse, Phase84NoOpRule) {
    // A speed-only effect (no tick/penalty/absorption) is valid (it does
    // something: the speed multiplier).
    Effect e;
    EXPECT_TRUE(EffectRegistry::parseEffect(
        parseJson(R"({"id":"s","speed_multiplier":1.5,"icon":0})"), e).ok());
    // But a field at its no-effect value is still a no-op (speed 1.0 = no
    // change; the old rule still fires).
    EXPECT_FALSE(EffectRegistry::parseEffect(
        parseJson(R"({"id":"s","speed_multiplier":1.0,"icon":0})"), e).ok());
}

TEST(StatusEffects, Phase84Totals) {
    EffectRegistry reg;
    const struct {
        const char* def;
    } defs[] = {
        {R"({"id":"spd","speed_multiplier":1.2,"icon":0})"},
        {R"({"id":"str","melee_bonus":2.0,"icon":1})"},
        {R"({"id":"res","damage_reduction":0.2,"icon":2})"},
    };
    for (const auto& d : defs) {
        ASSERT_TRUE(reg.registerEffect(parseJson(d.def)).ok()) << d.def;
    }
    StatusEffects se;
    // Identity / zero when no such effect is active.
    EXPECT_FLOAT_EQ(se.totalSpeedMultiplier(reg), 1.0f);
    EXPECT_FLOAT_EQ(se.totalMeleeBonus(reg), 0.0f);
    EXPECT_FLOAT_EQ(se.totalDamageReduction(reg), 0.0f);
    // speed multiplies (product), strength adds (sum), resistance is the
    // highest (max).
    se.apply(reg.idForName("spd"), 15.0);
    EXPECT_FLOAT_EQ(se.totalSpeedMultiplier(reg), 1.2f);
    se.apply(reg.idForName("str"), 15.0);
    EXPECT_FLOAT_EQ(se.totalMeleeBonus(reg), 2.0f);
    se.apply(reg.idForName("res"), 15.0);
    EXPECT_FLOAT_EQ(se.totalDamageReduction(reg), 0.2f);
}

TEST(StatusEffectContent, Phase83BeaconEffectsPinned) {
    // The shipped beacon effects (data/effects/*.json): the exact values
    // are the contract (speed 1.2 = MC speed I, strength +2 melee,
    // resistance 0.2 = MC resistance I).
    FILE* fp = std::fopen("assets/data/effects/speed.json", "rb");
    ASSERT_NE(fp, nullptr) << "run from the repo root";
    std::string text;
    char buf[4096];
    std::size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), fp)) > 0) {
        text.append(buf, n);
    }
    std::fclose(fp);
    Effect e;
    ASSERT_TRUE(EffectRegistry::parseEffect(parseJson(text), e).ok());
    EXPECT_EQ(e.name, "speed");
    EXPECT_FLOAT_EQ(e.speedMultiplier, 1.2f);
    EXPECT_FALSE(e.negative);
    EXPECT_EQ(e.icon, 5);
}

// ---------------------------------------------------------------------------
// SURV-4 (potions): the instant_hp field — a whole-HP change settled the
// moment the effect is applied (positive = heal, negative = damage), no
// lingering duration. Mutually exclusive with tick_type.
// ---------------------------------------------------------------------------

TEST(EffectParse, InstantHpValidHealing) {
    // The shipped healing potion effect (data/effects/healing.json): +4 HP,
    // icon 8 (the effects.png 128->160 extension tile).
    Effect e;
    auto r = EffectRegistry::parseEffect(
        parseJson(R"({"id":"healing","instant_hp":4,"negative":false,
                      "icon":8})"),
        e);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(e.name, "healing");
    EXPECT_EQ(e.tick, Effect::Tick::None);  // instant, not a tick
    EXPECT_FLOAT_EQ(e.instantHp, 4.0f);
    EXPECT_FALSE(e.negative);
    EXPECT_EQ(e.icon, 8);
}

TEST(EffectParse, InstantHpValidHarming) {
    // The shipped harming potion effect: -6 HP (a NEGATIVE instant — damage
    // with the armor exempted), icon 9.
    Effect e;
    auto r = EffectRegistry::parseEffect(
        parseJson(R"({"id":"harming","instant_hp":-6,"negative":true,
                      "icon":9})"),
        e);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_FLOAT_EQ(e.instantHp, -6.0f);
    EXPECT_TRUE(e.negative);
    EXPECT_EQ(e.icon, 9);
}

TEST(EffectParse, InstantHpAbsentByDefault) {
    // A pre-SURV-4 effect (weakness) has no instant action: instantHp 0.
    const EffectRegistry reg = makeRegistry();
    const Effect& w = *reg.findByName("weakness");
    EXPECT_FLOAT_EQ(w.instantHp, 0.0f);
    const Effect& p = *reg.findByName("poison");
    EXPECT_FLOAT_EQ(p.instantHp, 0.0f);
}

TEST(EffectParse, InstantHpRejectsExplicitZero) {
    // The absent field is the "no instant action" default, so an explicit 0
    // is a dead-field typo (the ammo / restore_hunger discipline).
    Effect e;
    auto r = EffectRegistry::parseEffect(
        parseJson(R"({"id":"x","instant_hp":0,"icon":8})"), e);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("non-zero"), std::string::npos);
}

TEST(EffectParse, InstantHpRejectsOutOfRange) {
    // Whole HP in [-32, 32]; the boundaries are legal, beyond is named.
    const std::vector<std::string> bad = {
        R"({"id":"x","instant_hp":33,"icon":8})",
        R"({"id":"x","instant_hp":-33,"icon":8})",
    };
    for (const std::string& def : bad) {
        Effect e;
        auto r = EffectRegistry::parseEffect(parseJson(def), e);
        EXPECT_FALSE(r.ok()) << def;
        EXPECT_NE(r.error().message.find("[-32, 32]"), std::string::npos)
            << def << ": " << r.error().message;
    }
    Effect hi, lo;
    EXPECT_TRUE(EffectRegistry::parseEffect(
        parseJson(R"({"id":"x","instant_hp":32,"icon":8})"), hi).ok());
    EXPECT_TRUE(EffectRegistry::parseEffect(
        parseJson(R"({"id":"x","instant_hp":-32,"icon":8})"), lo).ok());
    EXPECT_FLOAT_EQ(hi.instantHp, 32.0f);
    EXPECT_FLOAT_EQ(lo.instantHp, -32.0f);
}

TEST(EffectParse, InstantHpRejectsNonInteger) {
    // A fractional instant HP is a typo (the layer is whole HP, the
    // absorb_hp discipline).
    const std::vector<std::string> bad = {
        R"({"id":"x","instant_hp":4.5,"icon":8})",
        R"({"id":"x","instant_hp":-1.25,"icon":9})",
        R"({"id":"x","instant_hp":"4","icon":8})",
    };
    for (const std::string& def : bad) {
        Effect e;
        auto r = EffectRegistry::parseEffect(parseJson(def), e);
        EXPECT_FALSE(r.ok()) << def;
        EXPECT_NE(r.error().message.find("integer"), std::string::npos)
            << def << ": " << r.error().message;
    }
}

TEST(EffectParse, InstantHpMutuallyExclusiveWithTick) {
    // An effect is either a tick action or an instant one, not both (one
    // effect, one action form).
    Effect e;
    auto r = EffectRegistry::parseEffect(
        parseJson(R"({"id":"x","instant_hp":4,"tick_type":"heal",
                      "tick_amount":1.0,"tick_interval":4.5,"icon":8})"),
        e);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("either tick or instant"),
              std::string::npos) << r.error().message;
}

TEST(EffectParse, InstantHpAloneIsNotNoOp) {
    // An instant-only effect (no tick / penalty / layer) is a valid effect —
    // the no-op rejection must not fire. ...but an effect with NONE of the
    // actions still is a no-op.
    Effect e;
    auto r = EffectRegistry::parseEffect(
        parseJson(R"({"id":"x","instant_hp":4,"icon":8})"), e);
    ASSERT_TRUE(r.ok()) << r.error().message;
    Effect noop;
    auto rNoop = EffectRegistry::parseEffect(
        parseJson(R"({"id":"x","icon":8})"), noop);
    ASSERT_FALSE(rNoop.ok());
    EXPECT_NE(rNoop.error().message.find("no-op"), std::string::npos);
    EXPECT_NE(rNoop.error().message.find("instant_hp"), std::string::npos);
}

// -- StatusEffects: the instant grant (duration 0) ---------------------------

TEST(StatusEffects, ApplyZeroDurationRemovesOnNextUpdate) {
    // A grant of duration 0 (an instant effect) is applied then removed on
    // the NEXT update — it never lingers, and (having no tick) it never
    // fires a tick. The instant HP change itself is settled by the caller
    // (Game::applyPlayerEffect), not here.
    EffectRegistry reg;
    auto rid = reg.registerEffect(
        parseJson(R"({"id":"healing","instant_hp":4,"icon":8})"));
    ASSERT_TRUE(rid.ok()) << rid.error().message;
    const int healingId = *rid;
    StatusEffects se;
    se.apply(healingId, 0.0);
    ASSERT_EQ(se.count(), 1u);  // present the frame it is applied
    EXPECT_NEAR(se.list().at(0).remaining, 0.0, 1e-9);
    int ticks = 0;
    se.update(1.0 / 60.0, reg, [&](const Effect&) { ++ticks; });
    EXPECT_EQ(se.count(), 0u);   // gone on the next update (no lingering)
    EXPECT_EQ(ticks, 0);         // no tick (instant has none)
    // A second update is a no-op (nothing to remove, nothing to tick).
    se.update(1.0 / 60.0, reg, [&](const Effect&) { ++ticks; });
    EXPECT_EQ(se.count(), 0u);
    EXPECT_EQ(ticks, 0);
}

TEST(StatusEffects, ReapplyResetsToFullDuration) {
    // The drink RESET semantics (Minecraft's re-application): drinking the
    // same potion again re-times the effect to the FULL duration, not the
    // remaining. weakness 90 s, advance 3 s, re-apply 90 -> remaining 90
    // (not 87).
    const EffectRegistry reg = makeRegistry();
    StatusEffects se;
    const int weaknessId = reg.idForName("weakness");
    se.apply(weaknessId, 90.0);
    se.update(3.0, reg, [](const Effect&) {});
    EXPECT_NEAR(se.list().at(0).remaining, 87.0, 1e-9);
    // A second drink resets to the full 90.
    se.apply(weaknessId, 90.0);
    ASSERT_EQ(se.count(), 1u);  // still one instance (not stacked)
    EXPECT_NEAR(se.list().at(0).remaining, 90.0, 1e-9);
}

// ---------------------------------------------------------------------------
// SURV-5 (mob effects): the mob-carrier fields — `affects_mobs` (the
// acceptance gate), `min_health_mob` (the mob-side floor) and
// `undead_exempt` (the poison-does-not-touch-undead gate).
// ---------------------------------------------------------------------------

TEST(EffectParse, AffectsMobsValid) {
    // The shipped wither shape: a damage tick that accepts mobs.
    Effect e;
    auto r = EffectRegistry::parseEffect(
        parseJson(R"({"id":"wither","tick_type":"damage","tick_amount":1.0,
                      "tick_interval":4.0,"negative":true,"affects_mobs":true,
                      "icon":10})"),
        e);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_TRUE(e.affectsMobs);
    EXPECT_FALSE(e.undeadExempt);  // the wither kills undead (MC uniqueness)
}

TEST(EffectParse, AffectsMobsDefaultsFalse) {
    // Every pre-SURV-5 effect is player-only: the flag absent = false
    // (the makeRegistry fixture poison carries no affects_mobs) and an
    // explicit false is accepted.
    const EffectRegistry reg = makeRegistry();
    EXPECT_FALSE(reg.findByName("poison")->affectsMobs);
    Effect e;
    auto r = EffectRegistry::parseEffect(
        parseJson(R"({"id":"x","tick_type":"damage","tick_amount":1.0,
                      "tick_interval":2.5,"affects_mobs":false,"icon":0})"),
        e);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_FALSE(e.affectsMobs);
}

TEST(EffectParse, AffectsMobsRejectsNonBool) {
    for (const char* bad : {R"("yes")", "1", R"("true")", "[true]"}) {
        Effect e;
        auto r = EffectRegistry::parseEffect(
            parseJson(R"({"id":"x","tick_type":"damage","tick_amount":1.0,)"
                      R"("tick_interval":2.5,"affects_mobs":)" +
                std::string(bad) + R"(,"icon":0})"),
            e);
        EXPECT_FALSE(r.ok()) << "accepted affects_mobs " << bad;
        EXPECT_NE(r.error().message.find("'affects_mobs' must be a boolean"),
                  std::string::npos) << r.error().message;
    }
}

TEST(EffectParse, MinHealthMobExplicitValues) {
    // The shipped poison shape: the player floor 1.0 KEPT while the mob
    // floor is 0 (Minecraft's dual-target rule — poison cannot kill the
    // player, but it can kill a cow).
    Effect e;
    auto r = EffectRegistry::parseEffect(
        parseJson(R"({"id":"poison","tick_type":"damage","tick_amount":1.0,
                      "tick_interval":2.5,"min_health":1.0,
                      "min_health_mob":0,"icon":0})"),
        e);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_FLOAT_EQ(e.minHealth, 1.0f);   // the player floor is untouched
    EXPECT_EQ(e.minHealthMob, 0);         // the mob may be killed
    // The other boundary value (1 = the mob is kept at 1, never killed).
    Effect one;
    ASSERT_TRUE(EffectRegistry::parseEffect(
        parseJson(R"({"id":"x","tick_type":"damage","tick_amount":1.0,
                      "tick_interval":2.5,"min_health_mob":1,"icon":0})"),
        one).ok());
    EXPECT_EQ(one.minHealthMob, 1);
}

TEST(EffectParse, MinHealthMobAbsentFollowsMinHealth) {
    // ABSENT = the mob floor follows the player's min_health (the default-
    // follow rule): a pre-SURV-5 damage effect (the fire: floor 0) gives
    // the mob floor 0; the fixture poison (floor 1, no min_health_mob)
    // would give the mob floor 1. The sentinel -1 marks "not set".
    const EffectRegistry reg = makeRegistry();
    EXPECT_EQ(reg.findByName("fire")->minHealthMob, -1);
    EXPECT_EQ(reg.findByName("poison")->minHealthMob, -1);
    Effect e;
    ASSERT_TRUE(EffectRegistry::parseEffect(
        parseJson(R"({"id":"x","tick_type":"heal","tick_amount":1.0,
                      "tick_interval":4.5,"icon":2})"),
        e).ok());
    EXPECT_EQ(e.minHealthMob, -1);
}

TEST(EffectParse, MinHealthMobRejectsBadValues) {
    const std::vector<std::string> bad = {
        R"({"id":"x","tick_type":"damage","tick_amount":1.0,
           "tick_interval":2.5,"min_health_mob":0.5,"icon":0})",  //
        // non-integer
        R"({"id":"x","tick_type":"damage","tick_amount":1.0,
           "tick_interval":2.5,"min_health_mob":2,"icon":0})",    // > 1
        R"({"id":"x","tick_type":"damage","tick_amount":1.0,
           "tick_interval":2.5,"min_health_mob":-1,"icon":0})",   // < 0
        R"({"id":"x","tick_type":"heal","tick_amount":1.0,
           "tick_interval":4.5,"min_health_mob":0,"icon":0})",    // heal
        // floor
        R"({"id":"x","tick_type":"damage","tick_amount":1.0,
           "tick_interval":2.5,"min_health_mob":"0","icon":0})",  //
        // non-number
    };
    for (const std::string& def : bad) {
        Effect e;
        auto r = EffectRegistry::parseEffect(parseJson(def), e);
        EXPECT_FALSE(r.ok()) << def;
        EXPECT_NE(r.error().message.find("min_health_mob"), std::string::npos)
            << def << ": " << r.error().message;
    }
}

TEST(EffectParse, UndeadExemptValidAndStrict) {
    // The shipped poison flag: strict bool, absent = false.
    Effect e;
    auto r = EffectRegistry::parseEffect(
        parseJson(R"({"id":"poison","tick_type":"damage","tick_amount":1.0,
                      "tick_interval":2.5,"min_health":1.0,
                      "undead_exempt":true,"icon":0})"),
        e);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_TRUE(e.undeadExempt);
    const EffectRegistry reg = makeRegistry();
    EXPECT_FALSE(reg.findByName("fire")->undeadExempt);
    Effect notBool;
    auto rBad = EffectRegistry::parseEffect(
        parseJson(R"({"id":"x","tick_type":"damage","tick_amount":1.0,
                      "tick_interval":2.5,"undead_exempt":"yes","icon":0})"),
        notBool);
    ASSERT_FALSE(rBad.ok());
    EXPECT_NE(rBad.error().message.find("'undead_exempt' must be a boolean"),
              std::string::npos) << rBad.error().message;
}

// -- SURV-5: the shipped content pins ----------------------------------------

TEST(EffectContent, WitherEffectContentPinned) {
    // The shipped wither (data/effects/wither.json): Minecraft wither I —
    // 1 HP / 4 s, NO floor on either target (lethal on player and mob),
    // mob-accepting, undead-KILLING (no undead_exempt), icon 10 (the
    // effects.png 160->176 tail extension).
    FILE* fp = std::fopen("assets/data/effects/wither.json", "rb");
    ASSERT_NE(fp, nullptr) << "run from the repo root";
    std::string text;
    char buf[4096];
    std::size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), fp)) > 0) {
        text.append(buf, n);
    }
    std::fclose(fp);
    Effect e;
    ASSERT_TRUE(EffectRegistry::parseEffect(parseJson(text), e).ok());
    EXPECT_EQ(e.name, "wither");
    EXPECT_EQ(e.tick, Effect::Tick::Damage);
    EXPECT_FLOAT_EQ(e.tickAmount, 1.0f);
    EXPECT_FLOAT_EQ(e.tickInterval, 4.0f);
    EXPECT_FLOAT_EQ(e.minHealth, 0.0f);  // absent = no floor (player can
    // die)
    EXPECT_EQ(e.minHealthMob, -1);       // absent = follows min_health = 0
    // (mob can die)
    EXPECT_TRUE(e.negative);
    EXPECT_TRUE(e.affectsMobs);
    EXPECT_FALSE(e.undeadExempt);        // the wither kills the undead
    EXPECT_EQ(e.icon, 10);
}

TEST(EffectContent, PoisonMobCarrierFieldsPinned) {
    // The shipped poison (data/effects/poison.json) after SURV-5: the
    // player's 1-HP floor is KEPT (zero regression), the mob floor is 0
    // (a cow can be poisoned to death — the dual-target rule), it accepts
    // mobs, and the undead refuse it.
    FILE* fp = std::fopen("assets/data/effects/poison.json", "rb");
    ASSERT_NE(fp, nullptr) << "run from the repo root";
    std::string text;
    char buf[4096];
    std::size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), fp)) > 0) {
        text.append(buf, n);
    }
    std::fclose(fp);
    Effect e;
    ASSERT_TRUE(EffectRegistry::parseEffect(parseJson(text), e).ok());
    EXPECT_EQ(e.name, "poison");
    EXPECT_FLOAT_EQ(e.tickAmount, 1.0f);
    EXPECT_FLOAT_EQ(e.tickInterval, 2.5f);
    EXPECT_FLOAT_EQ(e.minHealth, 1.0f);   // the player floor (unchanged)
    EXPECT_EQ(e.minHealthMob, 0);         // the mob floor (lethal)
    EXPECT_TRUE(e.negative);
    EXPECT_TRUE(e.affectsMobs);
    EXPECT_TRUE(e.undeadExempt);
    EXPECT_EQ(e.icon, 0);
}

// -- SURV-6: the effect tiers (the amplifier levels) ---------------------------

namespace {

// A single-tierable fixture registry (the shipped shape, inlined so the
// values are test-owned): five tiered modifier effects + one single-tier
// control.
EffectRegistry makeTieredRegistry() {
    EffectRegistry reg;
    const struct {
        const char* def;
    } defs[] = {
        {R"({"id":"speed","speed_multiplier":1.2,"icon":0,
            "tier_max":3,"tier_steps":{"speed_multiplier":0.2}})"},
        {R"({"id":"resistance","damage_reduction":0.2,"icon":1,
            "tier_max":3,"tier_steps":{"damage_reduction":0.2}})"},
        {R"({"id":"absorption","absorb_hp":4,"icon":2,
            "tier_max":3,"tier_steps":{"absorb_hp":2}})"},
        {R"({"id":"strength","melee_bonus":2.0,"icon":3,
            "tier_max":3,"tier_steps":{"melee_bonus":2}})"},
        {R"({"id":"weakness","melee_penalty":4.0,"negative":true,"icon":4,
            "tier_max":3,"tier_steps":{"melee_penalty":4}})"},
        {R"({"id":"poison","tick_type":"damage","tick_amount":1.0,
            "tick_interval":2.5,"min_health":1.0,"negative":true,"icon":5})"},
    };
    for (const auto& d : defs) {
        auto r = reg.registerEffect(parseJson(d.def));
        if (!r.ok()) {
            ADD_FAILURE() << d.def << ": " << r.error().message;
        }
    }
    return reg;
}

} // namespace

TEST(EffectParse, TierMaxRange) {
    // int in [1, 3]; absent = 1 (the pre-SURV-6 single tier).
    auto reg = makeTieredRegistry();
    const Effect* speed = reg.findByName("speed");
    ASSERT_NE(speed, nullptr);
    EXPECT_EQ(speed->tierMax, 3);
    const Effect* poison = reg.findByName("poison");
    ASSERT_NE(poison, nullptr);
    EXPECT_EQ(poison->tierMax, 1);
    // The direct, exact rejections:
    Effect e;
    auto r0 = EffectRegistry::parseEffect(
        parseJson(R"({"id":"x","melee_bonus":2,"tier_max":0})"), e);
    EXPECT_FALSE(r0.ok());
    auto r4 = EffectRegistry::parseEffect(
        parseJson(R"({"id":"x","melee_bonus":2,"tier_max":4})"), e);
    EXPECT_FALSE(r4.ok());
    auto rF = EffectRegistry::parseEffect(
        parseJson(R"({"id":"x","melee_bonus":2,"tier_max":"3"})"), e);
    EXPECT_FALSE(rF.ok());
    auto r25 = EffectRegistry::parseEffect(
        parseJson(R"({"id":"x","melee_bonus":2,"tier_max":2.5})"), e);
    EXPECT_FALSE(r25.ok());
}

TEST(EffectParse, TierStepsRejections) {
    // A FRESH Effect per case: parseEffect does not reset `out`, so a
    // shared instance leaks the previous case's base fields into the
    // next case's pairing checks.
    // A non-tierable field named (the tick/instant forms are single-tier).
    {
        Effect e;
        auto rTick = EffectRegistry::parseEffect(
            parseJson(R"({"id":"x","icon":0,"tick_type":"damage",
                "tick_amount":1,"tick_interval":2,"tier_max":2,
                "tier_steps":{"tick_interval":0.5}})"), e);
        EXPECT_FALSE(rTick.ok());
        EXPECT_NE(rTick.error().message.find("non-tierable"),
                  std::string::npos);
    }
    // A negative step (the tiers are monotonically stronger).
    {
        Effect e;
        auto rNeg = EffectRegistry::parseEffect(
            parseJson(R"({"id":"x","icon":0,"melee_bonus":2,"tier_max":2,
                "tier_steps":{"melee_bonus":-1}})"), e);
        EXPECT_FALSE(rNeg.ok());
        EXPECT_NE(rNeg.error().message.find(">= 0"), std::string::npos);
    }
    // Pairing: a step without its base field (dead data).
    {
        Effect e;
        auto rPair = EffectRegistry::parseEffect(
            parseJson(R"({"id":"x","icon":0,"tier_max":2,
                "tier_steps":{"melee_bonus":2}})"), e);
        EXPECT_FALSE(rPair.ok());
        EXPECT_NE(rPair.error().message.find("requires"),
                  std::string::npos);
    }
    // Dead field: steps with tier_max 1 (a single-tier effect has no
    // tier to scale).
    {
        Effect e;
        auto rDead = EffectRegistry::parseEffect(
            parseJson(R"({"id":"x","icon":0,"melee_bonus":2,"tier_max":1,
                "tier_steps":{"melee_bonus":2}})"), e);
        EXPECT_FALSE(rDead.ok());
        EXPECT_NE(rDead.error().message.find("dead data"),
                  std::string::npos);
    }
    // No-op: tier_max 3 with every step 0 (a tier set that changes
    // nothing is a typo).
    {
        Effect e;
        auto rNoop = EffectRegistry::parseEffect(
            parseJson(R"({"id":"x","icon":0,"melee_bonus":2,"tier_max":3,
                "tier_steps":{"melee_bonus":0}})"), e);
        EXPECT_FALSE(rNoop.ok());
        EXPECT_NE(rNoop.error().message.find("no-op"), std::string::npos);
    }
    // No-op: tier_max 3 with no tier_steps object at all.
    {
        Effect e;
        auto rNone = EffectRegistry::parseEffect(
            parseJson(R"({"id":"x","icon":0,"melee_bonus":2,
                "tier_max":3})"), e);
        EXPECT_FALSE(rNone.ok());
        EXPECT_NE(rNone.error().message.find("no 'tier_steps'"),
                  std::string::npos);
    }
}

TEST(EffectTieredValue, ExactMinecraftTiers) {
    // The tier math: base + tier x step (tier 0 = the base bit-for-bit,
    // the pre-SURV-6 value).
    EXPECT_FLOAT_EQ(effectTieredValue(0, 1.2f, 0.2f), 1.2f);
    EXPECT_FLOAT_EQ(effectTieredValue(1, 1.2f, 0.2f), 1.4f);
    EXPECT_FLOAT_EQ(effectTieredValue(2, 1.2f, 0.2f), 1.6f);
    EXPECT_FLOAT_EQ(effectTieredValue(1, 4.0f, 2.0f), 6.0f);
    EXPECT_FLOAT_EQ(effectTieredValue(2, 0.2f, 0.2f), 0.6f);
    // A zero step keeps the base at EVERY tier (single-tier fields on a
    // tiered effect, e.g. resistance's absent melee_bonus).
    EXPECT_FLOAT_EQ(effectTieredValue(2, 2.0f, 0.0f), 2.0f);
}

TEST(StatusEffects, TierAdjustsTheTotals) {
    const EffectRegistry reg = makeTieredRegistry();
    const int speed = reg.idForName("speed");
    const int resist = reg.idForName("resistance");
    const int absorb = reg.idForName("absorption");
    const int strength = reg.idForName("strength");
    const int weakness = reg.idForName("weakness");
    StatusEffects s;
    // Tier 0 = the shipped base values (the pre-SURV-6 behavior).
    s.apply(speed, 100.0, 0);
    s.apply(resist, 100.0, 0);
    s.apply(absorb, 100.0, 0);
    s.apply(strength, 100.0, 0);
    s.apply(weakness, 100.0, 0);
    EXPECT_FLOAT_EQ(s.totalSpeedMultiplier(reg), 1.2f);
    EXPECT_FLOAT_EQ(s.totalDamageReduction(reg), 0.2f);
    EXPECT_FLOAT_EQ(s.totalAbsorption(reg), 4.0f);
    EXPECT_FLOAT_EQ(s.totalMeleeBonus(reg), 2.0f);
    EXPECT_FLOAT_EQ(s.totalMeleePenalty(reg), 4.0f);
    // Re-apply each at tier 1: the REPLACE semantics (no stacking) lift
    // the totals to the Minecraft II values.
    s.apply(speed, 100.0, 1);
    s.apply(resist, 100.0, 1);
    s.apply(absorb, 100.0, 1);
    s.apply(strength, 100.0, 1);
    s.apply(weakness, 100.0, 1);
    EXPECT_FLOAT_EQ(s.totalSpeedMultiplier(reg), 1.4f);
    EXPECT_FLOAT_EQ(s.totalDamageReduction(reg), 0.4f);
    EXPECT_FLOAT_EQ(s.totalAbsorption(reg), 6.0f);
    EXPECT_FLOAT_EQ(s.totalMeleeBonus(reg), 4.0f);
    EXPECT_FLOAT_EQ(s.totalMeleePenalty(reg), 8.0f);
    // Tier 2 = the III values.
    s.apply(speed, 100.0, 2);
    s.apply(resist, 100.0, 2);
    s.apply(absorb, 100.0, 2);
    s.apply(strength, 100.0, 2);
    s.apply(weakness, 100.0, 2);
    EXPECT_FLOAT_EQ(s.totalSpeedMultiplier(reg), 1.6f);
    EXPECT_FLOAT_EQ(s.totalDamageReduction(reg), 0.6f);
    EXPECT_FLOAT_EQ(s.totalAbsorption(reg), 8.0f);
    EXPECT_FLOAT_EQ(s.totalMeleeBonus(reg), 6.0f);
    EXPECT_FLOAT_EQ(s.totalMeleePenalty(reg), 12.0f);
}

TEST(StatusEffects, ReapplyReplacesTheTier) {
    const EffectRegistry reg = makeTieredRegistry();
    const int speed = reg.idForName("speed");
    StatusEffects s;
    s.apply(speed, 100.0, 2);
    EXPECT_FLOAT_EQ(s.totalSpeedMultiplier(reg), 1.6f);
    // A re-apply at tier 0 drops the instance back to the base (the
    // amplifier does not stack across re-applies).
    s.apply(speed, 100.0, 0);
    EXPECT_FLOAT_EQ(s.totalSpeedMultiplier(reg), 1.2f);
    EXPECT_EQ(s.count(), 1u);
}

TEST(ShippedContent, TieredEffectsMatchMinecraft) {
    // The five shipped tierable effects: tier_max 3 + the exact per-tier
    // steps (I/II/III = base/base+step/base+2step). The other six are
    // single-tier (no tier_max key, every step 0).
    struct Pin {
        const char* file;
        int tierMax;
        float speedStep, absorbStep, bonusStep, penaltyStep, resistStep;
    };
    const Pin pins[] = {
        {"speed", 3, 0.2f, 0, 0, 0, 0},
        {"absorption", 3, 0, 2.0f, 0, 0, 0},
        {"resistance", 3, 0, 0, 0, 0, 0.2f},
        {"strength", 3, 0, 0, 2.0f, 0, 0},
        {"weakness", 3, 0, 0, 0, 4.0f, 0},
        {"poison", 1, 0, 0, 0, 0, 0},
        {"wither", 1, 0, 0, 0, 0, 0},
        {"regeneration", 1, 0, 0, 0, 0, 0},
        {"fire", 1, 0, 0, 0, 0, 0},
        {"healing", 1, 0, 0, 0, 0, 0},
        {"harming", 1, 0, 0, 0, 0, 0},
    };
    for (const Pin& p : pins) {
        std::string text;
        FILE* fp =
            std::fopen((std::string("assets/data/effects/") + p.file +
                        ".json")
                           .c_str(),
                       "rb");
        ASSERT_NE(fp, nullptr) << "run from the repo root";
        char buf[4096];
        std::size_t n;
        while ((n = std::fread(buf, 1, sizeof(buf), fp)) > 0) {
            text.append(buf, n);
        }
        std::fclose(fp);
        Effect e;
        ASSERT_TRUE(EffectRegistry::parseEffect(parseJson(text), e).ok())
            << p.file << ": " << text;
        EXPECT_EQ(e.tierMax, p.tierMax) << p.file;
        EXPECT_FLOAT_EQ(e.stepSpeedMultiplier, p.speedStep) << p.file;
        EXPECT_FLOAT_EQ(e.stepAbsorbHp, p.absorbStep) << p.file;
        EXPECT_FLOAT_EQ(e.stepMeleeBonus, p.bonusStep) << p.file;
        EXPECT_FLOAT_EQ(e.stepMeleePenalty, p.penaltyStep) << p.file;
        EXPECT_FLOAT_EQ(e.stepDamageReduction, p.resistStep) << p.file;
    }
}

// -- PRE-7: the full-screen effect tint (poison = the purple veil) --------

TEST(EffectParse, TintValid) {
    Effect e;
    auto r = EffectRegistry::parseEffect(
        parseJson(R"({"id":"tinted","melee_penalty":1.0,"icon":0,
                     "tint":[0.63, 0.24, 0.86, 0.35]})"),
        e);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_FLOAT_EQ(e.tintR, 0.63f);
    EXPECT_FLOAT_EQ(e.tintG, 0.24f);
    EXPECT_FLOAT_EQ(e.tintB, 0.86f);
    EXPECT_FLOAT_EQ(e.tintA, 0.35f);
    // The boundary values are legal (inclusive [0, 1]).
    r = EffectRegistry::parseEffect(
        parseJson(R"({"id":"tinted","melee_penalty":1.0,"icon":0,
                     "tint":[0, 1, 1, 0.0]})"),
        e);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_FLOAT_EQ(e.tintA, 0.0f); // alpha 0 = the absent-field default
}

TEST(EffectParse, TintAbsentIsTheZeroVeil) {
    // Every pre-PRE-7 definition (no "tint" key) parses to the zero
    // rgba = no veil (zero draws, the structural-immunity root).
    Effect e;
    auto r = EffectRegistry::parseEffect(
        parseJson(R"({"id":"poison","tick_type":"damage","tick_amount":1.0,
                     "tick_interval":2.5,"min_health":1.0,"negative":true,
                     "icon":0})"),
        e);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_FLOAT_EQ(e.tintR, 0.0f);
    EXPECT_FLOAT_EQ(e.tintG, 0.0f);
    EXPECT_FLOAT_EQ(e.tintB, 0.0f);
    EXPECT_FLOAT_EQ(e.tintA, 0.0f);
}

TEST(EffectParse, TintRejections) {
    struct Case {
        const char* tint;
        const char* needle;
    };
    const Case cases[] = {
        {R"([0.63, 0.24, 0.86])", "array of 4 numbers"},       // 3
        {R"([0.63, 0.24, 0.86, 0.35, 1])", "array of 4 numbers"}, // 5
        {R"([0.63, 0.24, "purple", 0.35])", "tint[2]' must be a number"},
        {R"([0.63, 0.24, 0.86, 1.5])", "tint[3]' must be in [0, 1]"},
        {R"([-0.1, 0.24, 0.86, 0.35])", "tint[0]' must be in [0, 1]"},
        {R"("purple")", "array of 4 numbers"}, // a bare string
    };
    for (const Case& c : cases) {
        Effect e;
        auto r = EffectRegistry::parseEffect(
            parseJson(std::string(R"({"id":"bad","melee_penalty":1.0,
                      "icon":0,"tint":)") + c.tint + "}"),
            e);
        EXPECT_FALSE(r.ok()) << c.tint;
        if (!r.ok()) {
            EXPECT_NE(r.error().message.find(c.needle), std::string::npos)
                << r.error().message;
        }
    }
}

TEST(EffectParse, TintAlphaAloneIsNotNoOp) {
    // PRE-7 evolves the no-op rule: a positive tint alpha is a
    // presentation-layer action (a pure decorative veil effect is legal).
    Effect e;
    auto r = EffectRegistry::parseEffect(
        parseJson(R"({"id":"veil","icon":0,"tint":[0.63, 0.24, 0.86, 0.35]})"),
        e);
    ASSERT_TRUE(r.ok()) << r.error().message;
    // But an alpha-0 tint does NOT satisfy the rule (it draws nothing).
    r = EffectRegistry::parseEffect(
        parseJson(R"({"id":"veil","icon":0,"tint":[0.63, 0.24, 0.86, 0.0]})"),
        e);
    EXPECT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("no-op"), std::string::npos);
    EXPECT_NE(r.error().message.find("tint"), std::string::npos);
}

TEST(StatusEffects, ActiveEffectTintsOnlyForTintedEffects) {
    // A registry with one tinted effect (poison-shaped) and one plain
    // effect (weakness-shaped).
    EffectRegistry reg;
    auto rp = reg.registerEffect(
        parseJson(R"({"id":"poison","tick_type":"damage","tick_amount":1.0,
                     "tick_interval":2.5,"min_health":1.0,"negative":true,
                     "icon":0,"tint":[0.63, 0.24, 0.86, 0.35]})"));
    auto rw = reg.registerEffect(
        parseJson(R"({"id":"weakness","melee_penalty":4.0,"negative":true,
                     "icon":1})"));
    ASSERT_TRUE(rp.ok()) << rp.error().message;
    ASSERT_TRUE(rw.ok()) << rw.error().message;
    const int poisonId = *rp;
    const int weaknessId = *rw;

    StatusEffects fx;
    // Nothing active = no tints (the structural immunity).
    EXPECT_TRUE(activeEffectTints(fx, reg).empty());
    // Only the plain effect active = still no tints.
    fx.apply(weaknessId, 10.0);
    EXPECT_TRUE(activeEffectTints(fx, reg).empty());
    // The tinted effect joins = exactly one veil, the poison purple.
    fx.apply(poisonId, 60.0);
    auto tints = activeEffectTints(fx, reg);
    ASSERT_EQ(tints.size(), 1u);
    EXPECT_FLOAT_EQ(tints[0].r, 0.63f);
    EXPECT_FLOAT_EQ(tints[0].g, 0.24f);
    EXPECT_FLOAT_EQ(tints[0].b, 0.86f);
    EXPECT_FLOAT_EQ(tints[0].a, 0.35f);
    // Expiry removes the veil.
    fx.update(61.0, reg, [](const Effect&) {});
    EXPECT_TRUE(activeEffectTints(fx, reg).empty());
}

TEST(EffectContent, PoisonTintPinned) {
    // The shipped poison (data/effects/poison.json) after PRE-7 carries
    // the Minecraft purple veil (RGB 160/60/220 at alpha 0.35); the
    // other ten shipped effects stay untinted (zero veils).
    struct Pin {
        const char* file;
        float r, g, b, a;
    };
    const Pin pins[] = {
        {"poison", 0.63f, 0.24f, 0.86f, 0.35f},
        {"wither", 0, 0, 0, 0}, {"regeneration", 0, 0, 0, 0},
        {"fire", 0, 0, 0, 0},   {"healing", 0, 0, 0, 0},
        {"harming", 0, 0, 0, 0}, {"speed", 0, 0, 0, 0},
        {"absorption", 0, 0, 0, 0}, {"resistance", 0, 0, 0, 0},
        {"strength", 0, 0, 0, 0}, {"weakness", 0, 0, 0, 0},
    };
    for (const Pin& p : pins) {
        std::string text;
        FILE* fp =
            std::fopen((std::string("assets/data/effects/") + p.file +
                        ".json")
                           .c_str(),
                       "rb");
        ASSERT_NE(fp, nullptr) << "run from the repo root";
        char buf[4096];
        std::size_t n;
        while ((n = std::fread(buf, 1, sizeof(buf), fp)) > 0) {
            text.append(buf, n);
        }
        std::fclose(fp);
        Effect e;
        ASSERT_TRUE(EffectRegistry::parseEffect(parseJson(text), e).ok())
            << p.file << ": " << text;
        EXPECT_FLOAT_EQ(e.tintR, p.r) << p.file;
        EXPECT_FLOAT_EQ(e.tintG, p.g) << p.file;
        EXPECT_FLOAT_EQ(e.tintB, p.b) << p.file;
        EXPECT_FLOAT_EQ(e.tintA, p.a) << p.file;
    }
}
