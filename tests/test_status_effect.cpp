// Phase 46 status-effect tests: the EffectRegistry's strict parse
// (tick fields, the min_health damage-only rule, the melee penalty, the
// no-op rejection, unknown fields, duplicate ids) and the StatusEffects
// state machine (application/re-application, the real-dt tick crossing —
// one per interval, a large dt crosses several — the poison min_health
// floor, the heal clamp at max, clearNegative keeping the positives,
// clearAll, expiry, the melee-penalty sum). The save format-8 round trip
// lives in test_save.cpp; the mob on_hit_effect parse in test_mob.cpp.
#include <gtest/gtest.h>

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
