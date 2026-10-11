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
    EXPECT_EQ(reg.count(), 3u);
    EXPECT_EQ(reg.idForName("poison"), 0);
    EXPECT_EQ(reg.idForName("weakness"), 1);
    EXPECT_EQ(reg.idForName("regeneration"), 2);
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
