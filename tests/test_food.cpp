// Eating channel tests (Phase 28, core/food.hpp): the 1.6 s eat animation
// state machine — hold to complete (consume + restore), release early to
// cancel (nothing consumed), plus the progress indicator mapping. SURV-4
// adds the drink-complete MECHANISM tests (the pure building blocks the
// Game's updateEatChannel branch composes: the apply-grant vs clear decision
// driven by the item data, the reset semantics, and the instant HP settlement
// on the Player). The Game-level composition + the tryFluidBucket world
// mutation are pinned by the E2E gate (no Game unit harness in the project).
#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <string>

#include "block/block_registry.hpp"
#include "core/food.hpp"
#include "core/json.hpp"
#include "core/status_effect.hpp"
#include "item/inventory.hpp"
#include "item/item_registry.hpp"
#include "player/player.hpp"
#include "world/world.hpp"

using namespace mc;

namespace {

// One update at a fixed real-dt step (the game's 60 fps frame).
constexpr double kDt = 1.0 / 60.0;

// 1.6 s of 1/60 s frames is 96 frames mathematically, but the IEEE-double
// sum of 96 steps of (1/60) lands a hair SHORT of the double 1.6 (the
// constant rounds up), so the boundary crosses on the 97th update. The
// tests pin the exact frame — the arithmetic is deterministic IEEE double.
constexpr int kFullEatFrames = 97;

} // namespace

TEST(EatChannelTest, StartsInactiveAndIdle) {
    EatChannel c;
    EXPECT_FALSE(c.active);
    EXPECT_FLOAT_EQ(c.progress(), 0.0f);
    EXPECT_EQ(c.update(kDt, true), EatEvent::None); // not started: no event
}

TEST(EatChannelTest, CompletesWhenHeldToTheEnd) {
    EatChannel c;
    c.start();
    ASSERT_TRUE(c.active);
    // One frame short of the full 1.6 s: still running, no event.
    for (int i = 1; i < kFullEatFrames; ++i) {
        EXPECT_EQ(c.update(kDt, true), EatEvent::None) << "frame " << i;
    }
    EXPECT_TRUE(c.active);
    // The final frame crosses the boundary: exactly one completion.
    EXPECT_EQ(c.update(kDt, true), EatEvent::Completed);
    EXPECT_FALSE(c.active);
    EXPECT_EQ(c.update(kDt, true), EatEvent::None); // terminal, fired once
}

TEST(EatChannelTest, EarlyReleaseCancels) {
    EatChannel c;
    c.start();
    // Halfway through the eat: releasing aborts — no completion event.
    for (int i = 1; i < kFullEatFrames / 2; ++i) {
        EXPECT_EQ(c.update(kDt, true), EatEvent::None);
    }
    EXPECT_EQ(c.update(kDt, false), EatEvent::Cancelled);
    EXPECT_FALSE(c.active);
    EXPECT_EQ(c.update(kDt, false), EatEvent::None); // terminal, fired once
}

TEST(EatChannelTest, ReReleaseWithoutHoldCancels) {
    // The button is already up on the first update after start (a stale
    // event or the lock was lost): the channel cancels immediately rather
    // than hanging.
    EatChannel c;
    c.start();
    EXPECT_EQ(c.update(kDt, false), EatEvent::Cancelled);
    EXPECT_FALSE(c.active);
}

TEST(EatChannelTest, ProgressRunsZeroToOne) {
    EatChannel c;
    c.start();
    EXPECT_LT(c.progress(), 0.01f); // just started
    for (int i = 0; i < kFullEatFrames / 2; ++i) { // 48 frames
        c.update(kDt, true);
    }
    // Halfway (48/97 frames ~ 0.5 of the clock): progress ~0.5, clamped
    // inside [0, 1].
    EXPECT_GT(c.progress(), 0.48f);
    EXPECT_LT(c.progress(), 0.52f);
    for (int i = 48; i < kFullEatFrames + 3; ++i) {
        if (c.update(kDt, true) == EatEvent::Completed) {
            break;
        }
    }
    EXPECT_FALSE(c.active);
    EXPECT_FLOAT_EQ(c.progress(), 0.0f); // completed: the indicator resets
}

TEST(EatChannelTest, ConstantsMatchMinecraft) {
    // The two data-free constants: the eat time (Minecraft's 1.6 s) and
    // the saturation factor (the cooked-food 2x rule of thumb).
    EXPECT_DOUBLE_EQ(kEatDurationSeconds, 1.6);
    EXPECT_FLOAT_EQ(kSaturationFactor, 2.0f);
}

// -- drinkable channel (Phase 43: the data-driven duration) ------------------

TEST(EatChannelTest, CustomDurationRunsAtItsOwnClock) {
    // A drinkable container arms the shared channel with its DATA-DRIVEN
    // drink_time (here 0.5 s): it completes at its own duration, not the
    // eat's 1.6 s. 0.5 s is 30 frames mathematically, but the IEEE-double
    // sum of 30 steps of (1/60) lands a hair SHORT of the double 0.5, so
    // the boundary crosses on the 31st update (the 97th-frame phenomenon
    // of the eat test, pinned the same way).
    EatChannel c;
    c.start(0.5);
    ASSERT_TRUE(c.active);
    for (int i = 1; i < 31; ++i) {
        EXPECT_EQ(c.update(kDt, true), EatEvent::None) << "frame " << i;
    }
    EXPECT_EQ(c.update(kDt, true), EatEvent::Completed);
    EXPECT_FALSE(c.active);
}

TEST(EatChannelTest, CustomDurationProgressNormalizesByOwnTotal) {
    // progress() divides by the channel's OWN duration (`total`): halfway
    // through a 0.5 s drink is ~0.5, not ~0.15 of the eat clock.
    EatChannel c;
    c.start(0.5);
    for (int i = 0; i < 15; ++i) { // 15 frames = 0.25 s
        c.update(kDt, true);
    }
    EXPECT_GT(c.progress(), 0.48f);
    EXPECT_LT(c.progress(), 0.52f);
}

TEST(EatChannelTest, CustomDurationEarlyReleaseStillCancels) {
    EatChannel c;
    c.start(0.5);
    for (int i = 1; i < 15; ++i) {
        EXPECT_EQ(c.update(kDt, true), EatEvent::None);
    }
    EXPECT_EQ(c.update(kDt, false), EatEvent::Cancelled);
    EXPECT_FALSE(c.active);
    EXPECT_EQ(c.update(kDt, false), EatEvent::None); // terminal, fired once
}

TEST(EatChannelTest, DefaultStartIsStillTheEatDuration) {
    // Backward compatibility: start() with no argument is the 1.6 s eat
    // (the existing tests pin the 97th-frame boundary).
    EatChannel c;
    c.start();
    EXPECT_DOUBLE_EQ(c.total, kEatDurationSeconds);
}

// ---------------------------------------------------------------------------
// SURV-4 (potions): the drink-complete MECHANISM — the pure building blocks
// the Game's updateEatChannel branch composes. The branch reads the item's
// DATA to decide: a drinkable that GRANTS effects applies them on a completed
// drink (the grant entries IN ORDER, the RESET semantics, no clear); a
// drinkable WITHOUT grants (milk) keeps the original clear-negative. The
// instant HP settlement (heal / damage-bypass) runs on the Player at apply
// time (Game::applyPlayerEffect). The Game-level composition + the
// tryFluidBucket world mutation are pinned by the E2E gate.
// ---------------------------------------------------------------------------

namespace {

Json parseJson(const std::string& text) {
    auto r = Json::parse(text);
    if (!r.ok()) {
        ADD_FAILURE() << "parse failed: " << r.error().message;
        return Json::object();
    }
    return *r;
}

// A drinkable that GRANTS effects (a potion) vs one that does not (milk) —
// the two shapes the branch decides between, read straight off the data.
bool itemGrantsEffects(const Item& it) {
    return !it.grantsEffect.empty() || !it.grantsEffects.empty();
}

// The drink-complete branch's EFFECT side (the Game's updateEatChannel else
// side, minus the container return which is a plain Inventory add): a
// grant applies the entries IN ORDER; no grant clears the negatives.
void drinkEffectSide(const Item& item, StatusEffects& se,
                     const EffectRegistry& effects) {
    if (itemGrantsEffects(item)) {
        if (!item.grantsEffect.empty()) {
            se.apply(effects.idForName(item.grantsEffect),
                     item.grantsEffectDuration);
        }
        for (const Item::GrantEffect& g : item.grantsEffects) {
            se.apply(effects.idForName(g.name), g.duration);
        }
    } else {
        se.clearNegative(effects); // the milk / water original clear
    }
}

// The effect set a potion drink needs (weakness + a positive control + a
// negative control), registered once for the branch tests.
EffectRegistry makeDrinkEffects() {
    EffectRegistry reg;
    const struct {
        const char* def;
    } defs[] = {
        {R"({"id":"weakness","melee_penalty":4.0,"negative":true,"icon":1})"},
        {R"({"id":"regeneration","tick_type":"heal","tick_amount":1.0,
             "tick_interval":4.5,"negative":false,"icon":2})"},
        {R"({"id":"poison","tick_type":"damage","tick_amount":1.0,
             "tick_interval":2.5,"min_health":1.0,"negative":true,"icon":0})"},
    };
    for (const auto& d : defs) {
        auto r = reg.registerEffect(parseJson(d.def));
        if (!r.ok()) {
            ADD_FAILURE() << d.def << ": " << r.error().message;
        }
    }
    return reg;
}

// The potion + milk drinkable items (the branch's two data shapes).
ItemRegistry makeDrinkItems() {
    ItemRegistry items;
    const struct {
        const char* def;
    } defs[] = {
        {R"({"id":"glass_bottle","type":"material","max_stack":16})"},
        {R"({"id":"bucket","type":"material","max_stack":16})"},
        {R"({"id":"potion_weakness","type":"material","max_stack":1,
             "drink_time":1.6,"returns_item":"glass_bottle",
             "grants_effects":[{"name":"weakness","duration":90}]})"},
        {R"({"id":"milk_bucket","type":"material","max_stack":1,
             "drink_time":1.6,"returns_item":"bucket"})"},
    };
    for (const auto& d : defs) {
        auto r = items.registerItem(parseJson(d.def));
        if (!r.ok()) {
            ADD_FAILURE() << d.def << ": " << r.error().message;
        }
    }
    return items;
}

} // namespace

TEST(DrinkCompleteTest, PotionDrinkAppliesGrantAndKeepsPositives) {
    // A potion (a drinkable WITH grants) applies its effect on a completed
    // drink — and does NOT clear the existing effects (unlike milk). The
    // positive regeneration survives; the pre-existing negative poison is
    // also NOT cleared (the grant branch replaces the clear entirely).
    const EffectRegistry effects = makeDrinkEffects();
    const ItemRegistry items = makeDrinkItems();
    const Item& potion = *items.findByName("potion_weakness");
    ASSERT_TRUE(itemGrantsEffects(potion));

    StatusEffects se;
    se.apply(effects.idForName("regeneration"), 30.0); // positive control
    se.apply(effects.idForName("poison"), 15.0);        // negative control
    ASSERT_EQ(se.count(), 2u);

    drinkEffectSide(potion, se, effects);

    // The weakness grant is applied...
    bool hasWeakness = false;
    for (const ActiveEffect& a : se.list()) {
        hasWeakness = (a.effect == effects.idForName("weakness"));
    }
    EXPECT_TRUE(hasWeakness);
    // ...and NEITHER pre-existing effect was cleared (the grant branch does
    // not call clearNegative — 3 active now).
    EXPECT_EQ(se.count(), 3u);
    bool hasRegen = false, hasPoison = false;
    for (const ActiveEffect& a : se.list()) {
        hasRegen = hasRegen || (a.effect == effects.idForName("regeneration"));
        hasPoison = hasPoison || (a.effect == effects.idForName("poison"));
    }
    EXPECT_TRUE(hasRegen);
    EXPECT_TRUE(hasPoison);
}

TEST(DrinkCompleteTest, NoGrantDrinkClearsNegativeRegression) {
    // The milk (a drinkable WITHOUT grants) keeps the ORIGINAL behavior: it
    // clears the NEGATIVE effects and keeps the positive ones.
    const EffectRegistry effects = makeDrinkEffects();
    const ItemRegistry items = makeDrinkItems();
    const Item& milk = *items.findByName("milk_bucket");
    ASSERT_FALSE(itemGrantsEffects(milk));

    StatusEffects se;
    se.apply(effects.idForName("regeneration"), 30.0); // positive: survives
    se.apply(effects.idForName("poison"), 15.0);        // negative: cleared
    se.apply(effects.idForName("weakness"), 15.0);      // negative: cleared
    ASSERT_EQ(se.count(), 3u);

    drinkEffectSide(milk, se, effects);

    ASSERT_EQ(se.count(), 1u);
    EXPECT_EQ(se.list().at(0).effect, effects.idForName("regeneration"));
}

TEST(DrinkCompleteTest, PotionDrinkReturnsBottle) {
    // The container return: a completed potion drink adds one glass_bottle
    // to the inventory (the Game's Inventory add — the start-time capacity
    // check makes it infallible, so the branch just calls add).
    const ItemRegistry items = makeDrinkItems();
    const Item& potion = *items.findByName("potion_weakness");
    ASSERT_EQ(potion.returnsItem, "glass_bottle");
    Inventory inv;
    inv.add(items.idForName(potion.returnsItem), 1, items);
    ASSERT_FALSE(inv.at(0).empty());
    EXPECT_EQ(inv.at(0).item, items.idForName("glass_bottle"));
    EXPECT_EQ(inv.at(0).count, 1);
}

TEST(DrinkCompleteTest, ReapplyResetsDurationOnDrink) {
    // The drink RESET semantics (the channel the E2E pins): drinking the
    // same potion again re-times the effect to the FULL grant duration, not
    // the remaining (Minecraft's re-application).
    const EffectRegistry effects = makeDrinkEffects();
    const ItemRegistry items = makeDrinkItems();
    const Item& potion = *items.findByName("potion_weakness");

    StatusEffects se;
    drinkEffectSide(potion, se, effects); // first drink: weakness 90
    se.update(3.0, effects, [](const Effect&) {});
    ASSERT_EQ(se.count(), 1u);
    EXPECT_NEAR(se.list().at(0).remaining, 87.0, 1e-9);
    drinkEffectSide(potion, se, effects); // second drink: reset to 90
    ASSERT_EQ(se.count(), 1u); // not stacked
    EXPECT_NEAR(se.list().at(0).remaining, 90.0, 1e-9);
}

// -- the instant HP settlement (Game::applyPlayerEffect's Player side) ------

namespace {

// A Player on a stone floor (the damage/heal tests need a world + blocks).
struct PlayerFixture {
    BlockRegistry blocks;
    World world;
    ItemRegistry items;
    std::unique_ptr<Player> player;

    PlayerFixture() {
        auto r = blocks.registerBlock(parseJson(
            R"({"id":"stone","model":{"textures":{"top":"t","side":"t",
               "bottom":"t"}}})"));
        if (!r.ok()) {
            ADD_FAILURE() << r.error().message;
        }
        // The full diamond chain (head 3 + chest 8 + legs 6 + feet 3 = 20,
        // the armor cap) for the bypass proof.
        const struct {
            const char* def;
        } defs[] = {
            {R"({"id":"diamond_helmet","type":"material","max_stack":1,
                 "armor_slot":"head","armor_points":3})"},
            {R"({"id":"diamond_chestplate","type":"material","max_stack":1,
                 "armor_slot":"chest","armor_points":8})"},
            {R"({"id":"diamond_leggings","type":"material","max_stack":1,
                 "armor_slot":"legs","armor_points":6})"},
            {R"({"id":"diamond_boots","type":"material","max_stack":1,
                 "armor_slot":"feet","armor_points":3})"},
        };
        for (const auto& d : defs) {
            auto ir = items.registerItem(parseJson(d.def));
            if (!ir.ok()) {
                ADD_FAILURE() << d.def << ": " << ir.error().message;
            }
        }
        player = std::make_unique<Player>(world, blocks, Vec3 {8.5f, 2.0f,
                                                               8.5f});
    }

    void wearFullDiamond() {
        const int pts[4] = {3, 8, 6, 3};
        const char* names[4] = {"diamond_helmet", "diamond_chestplate",
                                "diamond_leggings", "diamond_boots"};
        for (int slot = 0; slot < 4; ++slot) {
            player->setArmorSlot(slot, ItemStack {items.idForName(names[slot]),
                                                  1},
                                 pts[slot], 0.0f);
        }
    }
};

} // namespace

TEST(InstantSettlementTest, HealingClampsAtMaxHealth) {
    // The healing potion: a +4 instant heal settled on apply (Game::
    // applyPlayerEffect's heal side). From 16 HP it lands at 20 (the base
    // max — heal clamps, it never overfills).
    PlayerFixture f;
    f.player->setHealth(16.0f);
    f.player->heal(4.0f); // def.instantHp > 0 -> heal(instantHp)
    EXPECT_FLOAT_EQ(f.player->health(), 20.0f);
    // Already at max: a heal clamps at 20 (no overfill).
    f.player->heal(4.0f);
    EXPECT_FLOAT_EQ(f.player->health(), 20.0f);
}

TEST(InstantSettlementTest, HarmingBypassesArmor) {
    // The harming potion: a -6 instant damage settled on apply with the
    // armor EXEMPTED (bypassArmor=true — Minecraft's magic/potion damage,
    // the SURV-2 drowning twin). From 16 HP in FULL diamond (20 pts) it
    // lands at 10 (NOT 16 - floor(6*20)/100 = 14.8 — the armor step is
    // skipped). The resistance effect would still apply (the damage
    // pipeline), but there is none here.
    PlayerFixture f;
    f.wearFullDiamond();
    ASSERT_EQ(f.player->armorPoints(), 20);
    f.player->setHealth(16.0f);
    f.player->damage(6.0f, true); // def.instantHp < 0 -> damage(-instantHp,
                                   // true)
    EXPECT_FLOAT_EQ(f.player->health(), 10.0f);
    // Control: the SAME damage WITHOUT the bypass is armor-reduced (14.8)
    // — the 10 vs 14.8 gap is the bypass proof.
    PlayerFixture g;
    g.wearFullDiamond();
    g.player->setHealth(16.0f);
    g.player->damage(6.0f, false);
    EXPECT_FLOAT_EQ(g.player->health(), 16.0f -
                                               std::floor(6.0f * 20.0f) / 100.0f);
    EXPECT_GT(g.player->health(), 10.0f);
}
