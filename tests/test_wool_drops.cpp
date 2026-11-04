// V2-15 (sheep): the wool_drops init cross-check (entity/wool_drops.cpp —
// the pure function the unit tests own; the game's init is a thin adapter,
// the checkRepairMaterials pattern) + the wool instance-state helpers'
// PURE side (rollWoolColor's cumulative walk, the woolDropEntry lookup).
// The MobEntity's state machine (the spawn roll's stream draw, the shear
// / dye / regrow frames) is pinned in test_mob.cpp's MobWool suite.
#include <gtest/gtest.h>

#include <string>

#include "block/block_registry.hpp"
#include "core/asset_manager.hpp"
#include "core/json.hpp"
#include "entity/mob_registry.hpp"
#include "entity/wool_drops.hpp"
#include "item/item_registry.hpp"
#include "renderer/texture_atlas.hpp"

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

// A minimal wool-drops mob (the sheep's data shape, one-box model — the
// registry does not check atlas existence at parse, only the geometry
// rules). `woolEntry` / `weights` are spliced (the rejection tests'
// levers).
std::string sheepDef(bool woolEntry, const std::string& weights) {
    const std::string woolDrop = woolEntry
        ? R"({"item": "wool", "count_min": 1, "count_max": 3}, )"
        : std::string();
    const std::string def =
        R"({
  "id": "sheep",
  "properties": {
    "health": 8,
    "speed": 0.5,
    "detect_radius": 0,
    "attack_range": 0,
    "attack_damage": 0,
    "attack_cooldown": 1.0,
    "wander_interval": 3.0,
    "drops": [)" +
        woolDrop + R"({"item": "mutton", "count": 1}],
    "wool_drops": true,
    "wool_weights": )" + weights + R"(
  },
  "model": {
    "width": 1.2,
    "height": 1.5,
    "boxes": [
      { "name": "wool", "pos": [0.0, 1.025, 0.0],
        "size": [0.625, 0.55, 0.95], "texture": "a" }
    ]
  }
})";
    return def;
}

std::string dyeDef(const std::string& name, const std::string& color) {
    return std::string(R"({"id":")") + name +
           R"(","type":"material","dye_color":")" + color + R"("})";
}

// The two dyes the sheepDef weights name (white / red) — the vocabulary
// for the check.
ItemRegistry makeDyes() {
    ItemRegistry items;
    auto r1 = items.registerItem(parseJson(dyeDef("dye_white", "white")));
    auto r2 = items.registerItem(parseJson(dyeDef("dye_red", "red")));
    if (!r1.ok() || !r2.ok()) {
        ADD_FAILURE() << r1.error().message << " / " << r2.error().message;
    }
    return items;
}

// A layout with the bare tile + BOTH dye colors' wool variant tiles
// (the complete set for makeDyes' two-color vocabulary).
AtlasLayout makeCompleteLayout() {
    auto r = AtlasLayout::parse(parseJson(
        R"({"columns":2,"rows":2,"tiles":{
          "a":[0,0], "wool":[1,0], "wool_red":[0,1], "sheep_skin":[1,1]}})"));
    if (!r.ok()) {
        ADD_FAILURE() << r.error().message;
    }
    return *r;
}

} // namespace

// ---------------------------------------------------------------------------
// The init cross-check (checkWoolDrops — the pure function).
// ---------------------------------------------------------------------------

TEST(WoolDropsCheck, PassesOnShippedContent) {
    // The REAL assets (the acceptance): every shipped mob, the full dye
    // vocabulary + the full atlas (15 wool variant tiles + sheep_skin)
    // pass the check (the sheep is the only wool_drops carrier).
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    MobRegistry mobs;
    ASSERT_TRUE(mobs.loadDirectory(assets, "data/mobs").ok());
    ItemRegistry items;
    BlockRegistry blocks;
    ASSERT_TRUE(blocks.loadDirectory(assets, "data/blocks").ok());
    ASSERT_TRUE(items.buildBlockItems(blocks).ok());
    ASSERT_TRUE(items.loadDirectory(assets, "data/items").ok());
    auto text = assets.loadText("data/textures/block_atlas.json");
    ASSERT_TRUE(text.ok()) << text.error().message;
    auto parsed = Json::parse(*text);
    ASSERT_TRUE(parsed.ok()) << parsed.error().message;
    auto layout = AtlasLayout::parse(*parsed);
    ASSERT_TRUE(layout.ok()) << layout.error().message;
    auto r = checkWoolDrops(mobs, items, *layout);
    ASSERT_TRUE(r.ok()) << r.error().message;
    // The sheep is the only wool_drops carrier (the data promise).
    int carriers = 0;
    for (const MobID id : mobs.allIds()) {
        if (mobs.at(id).properties.woolDrops) {
            ++carriers;
            EXPECT_EQ(mobs.at(id).name, "sheep");
        }
    }
    EXPECT_EQ(carriers, 1);
}

TEST(WoolDropsCheck, RejectsMissingWoolDropEntry) {
    // A wool_drops mob whose drop list lacks the "wool" family entry is
    // dead data (the death pass's recolor / shear's count range would
    // have no target) — named.
    MobRegistry mobs;
    ASSERT_TRUE(
        mobs.registerMob(parseJson(sheepDef(/*woolEntry=*/false,
                                            R"([{"color":"white","weight":1}])")))
            .ok());
    const ItemRegistry items = makeDyes();
    const AtlasLayout layout = makeCompleteLayout();
    auto r = checkWoolDrops(mobs, items, layout);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("no 'wool' drop entry"),
              std::string::npos)
        << r.error().message;
}

TEST(WoolDropsCheck, RejectsUnknownWeightColor) {
    // A weight color no dye item names is outside the data-defined
    // vocabulary — named (the block dye family's cross-check twin).
    MobRegistry mobs;
    ASSERT_TRUE(mobs.registerMob(
                    parseJson(sheepDef(true,
                                       R"([{"color":"lime","weight":1}])")))
                    .ok());
    const ItemRegistry items = makeDyes(); // no lime dye item
    const AtlasLayout layout = makeCompleteLayout();
    auto r = checkWoolDrops(mobs, items, layout);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("not a dye item's color"),
              std::string::npos)
        << r.error().message;
    EXPECT_NE(r.error().message.find("'lime'"), std::string::npos)
        << r.error().message;
}

TEST(WoolDropsCheck, RejectsMissingBareTile) {
    // The atlas without the bare tile (sheep_skin) — named.
    MobRegistry mobs;
    ASSERT_TRUE(mobs.registerMob(
                    parseJson(sheepDef(true,
                                       R"([{"color":"white","weight":1}])")))
                    .ok());
    const ItemRegistry items = makeDyes();
    auto layout = AtlasLayout::parse(parseJson(
        R"({"columns":2,"rows":2,"tiles":{
          "a":[0,0], "wool":[1,0], "wool_red":[0,1]}})"));
    ASSERT_TRUE(layout.ok());
    auto r = checkWoolDrops(mobs, items, *layout);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("'sheep_skin'"), std::string::npos)
        << r.error().message;
}

TEST(WoolDropsCheck, RejectsMissingVariantTile) {
    // The atlas without a DYE COLOR'S wool variant (wool_red — the mob
    // can be dyed any dye, so the full 15-color set must exist) — named.
    MobRegistry mobs;
    ASSERT_TRUE(mobs.registerMob(
                    parseJson(sheepDef(true,
                                       R"([{"color":"white","weight":1}])")))
                    .ok());
    const ItemRegistry items = makeDyes();
    auto layout = AtlasLayout::parse(parseJson(
        R"({"columns":2,"rows":2,"tiles":{
          "a":[0,0], "wool":[1,0], "sheep_skin":[0,1]}})"));
    ASSERT_TRUE(layout.ok());
    auto r = checkWoolDrops(mobs, items, *layout);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("'wool_red'"), std::string::npos)
        << r.error().message;
}

TEST(WoolDropsCheck, InertWithoutWoolMobs) {
    // The structural red line: a registry WITHOUT any wool_drops mob
    // passes over an EMPTY layout (no tile requirement at all — the
    // pre-V2-15 init behavior, bit-identical).
    MobRegistry mobs;
    ASSERT_TRUE(mobs
                    .registerMob(parseJson(R"({
      "id": "zombie",
      "properties": {
        "health": 20, "speed": 0.8, "detect_radius": 16,
        "attack_range": 1.6, "attack_damage": 3,
        "attack_cooldown": 1.0, "wander_interval": 2.5
      },
      "model": {
        "width": 0.6, "height": 1.8,
        "boxes": [
          {"pos": [0.0, 1.25, 0.0], "size": [0.5, 0.5, 0.5], "texture": "a"}
        ]
      }
    })"))
                    .ok());
    const ItemRegistry items; // no dyes at all
    auto layout = AtlasLayout::parse(
        parseJson(R"({"columns":1,"rows":1,"tiles":{"a":[0,0]}})"));
    ASSERT_TRUE(layout.ok());
    auto r = checkWoolDrops(mobs, items, *layout);
    ASSERT_TRUE(r.ok()) << r.error().message;
}

// ---------------------------------------------------------------------------
// The pure helpers (rollWoolColor / woolDropEntry).
// ---------------------------------------------------------------------------

TEST(WoolWeight, RollWoolColorCumulativeWalk) {
    // The shipped sheep's weights (the MC temperate table's integer
    // approximation — deviation 2): total 1000. The cumulative walk's
    // boundaries are EXACT (u * total against the running sum — the
    // u == 0.85 boundary lands on BLACK, the next entry, the
    // attemptMobSpawn pick's order discipline).
    const std::vector<WoolWeight> weights = {
        {"white", 850}, {"black", 80}, {"gray", 30}, {"brown", 30},
        {"pink", 10},
    };
    EXPECT_EQ(rollWoolColor(weights, 0.0), "white");
    EXPECT_EQ(rollWoolColor(weights, 0.849999), "white");
    EXPECT_EQ(rollWoolColor(weights, 0.85), "black");
    EXPECT_EQ(rollWoolColor(weights, 0.929999), "black");
    EXPECT_EQ(rollWoolColor(weights, 0.93), "gray");
    EXPECT_EQ(rollWoolColor(weights, 0.959999), "gray");
    EXPECT_EQ(rollWoolColor(weights, 0.96), "brown");
    EXPECT_EQ(rollWoolColor(weights, 0.989999), "brown");
    EXPECT_EQ(rollWoolColor(weights, 0.99), "pink");
    EXPECT_EQ(rollWoolColor(weights, 0.999999), "pink");
}

// -- The death pass's wool resolution (woolDeathDropName) ---------------

namespace {

MobProperties woolProps(bool woolDrops) {
    MobProperties p;
    p.woolDrops = woolDrops;
    return p;
}

MobDrop entry(const std::string& item, int lo, int hi) {
    MobDrop d;
    d.item = item;
    d.countMin = lo;
    d.countMax = hi;
    return d;
}

} // namespace

TEST(WoolDeathDrop, WoollyDeathDropsTheInstanceColorVariant) {
    // The Minecraft rule: the woolly death drops 1-3 of the instance
    // COLOR's wool — white resolves to the suffix-less base (the
    // dyeVariantName naming law).
    const MobProperties p = woolProps(true);
    EXPECT_EQ(*woolDeathDropName(p, entry("wool", 1, 3), /*bare=*/false,
                                 "white"),
              "wool");
    EXPECT_EQ(*woolDeathDropName(p, entry("wool", 1, 3), /*bare=*/false,
                                 "red"),
              "wool_red");
    EXPECT_EQ(*woolDeathDropName(p, entry("wool", 1, 3), /*bare=*/false,
                                 "light_blue"),
              "wool_light_blue");
}

TEST(WoolDeathDrop, BareDeathDropsNoWool) {
    // The sheared death: the wool entry resolves to nullopt (the
    // despawn pass skips it BEFORE the count roll — no stream draw),
    // whatever the (kept) instance color is.
    const MobProperties p = woolProps(true);
    EXPECT_FALSE(
        woolDeathDropName(p, entry("wool", 1, 3), /*bare=*/true, "red")
            .has_value());
    EXPECT_FALSE(
        woolDeathDropName(p, entry("wool", 1, 3), /*bare=*/true, "white")
            .has_value());
}

TEST(WoolDeathDrop, OtherEntriesAndNonWoolMobsAreUntouched) {
    // The bit-identical root: a non-wool mob's "wool" entry keeps its
    // data name (no recolor, no skip — the pre-V2-15 death), and every
    // non-family entry (mutton) resolves to its data name on a wool
    // mob.
    const MobProperties wool = woolProps(true);
    const MobProperties plain = woolProps(false);
    EXPECT_EQ(*woolDeathDropName(wool, entry("mutton", 1, 1), false,
                                 "red"),
              "mutton");
    EXPECT_EQ(*woolDeathDropName(wool, entry("mutton", 1, 1), true,
                                 "red"),
              "mutton"); // even bare: the mutton is not wool
    EXPECT_EQ(*woolDeathDropName(plain, entry("wool", 1, 1), false, ""),
              "wool");   // a non-wool mob is never recolored
    EXPECT_EQ(*woolDeathDropName(plain, entry("wool", 1, 1), true, ""),
              "wool");   // ...and never skipped (bare is a wool-mob state)
}

TEST(WoolWeight, WoolDropEntryLookup) {
    // The family-entry lookup (the death pass's / shearing's target):
    // a wool_drops mob's "wool" entry, nullptr for a non-wool mob and
    // for a wool mob missing the entry (the init rejection's twin).
    MobRegistry mobs;
    ASSERT_TRUE(mobs
                    .registerMob(parseJson(sheepDef(
                        /*woolEntry=*/true,
                        R"([{"color":"white","weight":1}])")))
                    .ok());
    ASSERT_TRUE(mobs
                    .registerMob(parseJson(R"({
      "id": "cow",
      "properties": {
        "health": 10, "speed": 0.5, "detect_radius": 0,
        "attack_range": 0, "attack_damage": 0,
        "attack_cooldown": 1.0, "wander_interval": 3.0,
        "drops": [{"item": "beef", "count": 1}]
      },
      "model": {
        "width": 0.9, "height": 1.3,
        "boxes": [
          {"pos": [0.0, 0.7, 0.0], "size": [0.5, 0.8, 0.9], "texture": "a"}
        ]
      }
    })"))
                    .ok());
    const Mob& sheep = mobs.at(mobs.idForName("sheep"));
    const Mob& cow = mobs.at(mobs.idForName("cow"));
    const MobDrop* e = woolDropEntry(&sheep.properties);
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(e->item, "wool");
    EXPECT_EQ(e->countMin, 1);
    EXPECT_EQ(e->countMax, 3);
    EXPECT_EQ(woolDropEntry(&cow.properties), nullptr);
    // A null properties pointer = the defensive no-mob case.
    EXPECT_EQ(woolDropEntry(nullptr), nullptr);
}
