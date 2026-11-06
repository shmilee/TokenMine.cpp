// V3-10b (v3.24.0): the ender pearl's data face — the throws_teleport
// channel (the item registry's strict-bool predicate + the two sound
// names + the one-item-owns-one-right-click exclusivity, the
// parseIgnites block's shape) + the Projectile's trailing-parameter
// teleport flag (the legacy call's bit-identical default).
#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "block/block_registry.hpp"
#include "core/json.hpp"
#include "entity/projectile.hpp"
#include "item/item_registry.hpp"
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

// The pearl's data shape (the flag + the two sound names + the MC-
// exact stack; `extra` splices a channel-conflict field for the
// exclusivity arms).
std::string pearlDef(const std::string& extra = "") {
    return std::string(R"({
  "id": "ender_pearl",
  "type": "material",
  "max_stack": 16,
  "icon": "ender_pearl",
  "throws_teleport": true,
  "throw_sound": "ender_pearl_throw",
  "teleport_sound": "ender_pearl_teleport")") + extra + "}";
}

ItemRegistry registerPearl(const std::string& def) {
    ItemRegistry items;
    auto r = items.registerItem(parseDef(def));
    if (!r.ok()) {
        throw std::runtime_error(r.error().message);
    }
    return items;
}

} // namespace

// ---------------------------------------------------------------------------
// The throws_teleport channel's parse (the strict bool + the sounds).
// ---------------------------------------------------------------------------

TEST(PearlItemTest, ParsesTheThrowChannel) {
    ItemRegistry items = registerPearl(pearlDef());
    const Item& p = items.at(items.idForName("ender_pearl"));
    EXPECT_TRUE(p.throwsTeleport);
    EXPECT_EQ(p.throwSound, "ender_pearl_throw");
    EXPECT_EQ(p.teleportSound, "ender_pearl_teleport");
    EXPECT_EQ(p.maxStack, 16);
}

TEST(PearlItemTest, StrictBoolAndSoundNames) {
    // The strict bool (the shears' twin): a non-boolean is named.
    {
        ItemRegistry items;
        auto bad = std::string(R"({
          "id": "ender_pearl",
          "type": "material",
          "throws_teleport": "yes",
          "throw_sound": "a",
          "teleport_sound": "b"
        })");
        auto r = items.registerItem(parseDef(bad));
        ASSERT_FALSE(r.ok());
        EXPECT_NE(r.error().message.find("throws_teleport"),
                  std::string::npos);
    }
    // The sound names' strict non-empty strings (the lone sound name's
    // dead-field rejection — the milk pairing's twin).
    {
        ItemRegistry items;
        auto r = items.registerItem(
            parseDef(R"({
          "id": "ender_pearl",
          "type": "material",
          "throw_sound": "ender_pearl_throw"
        })"));
        ASSERT_FALSE(r.ok());
        EXPECT_NE(r.error().message.find("throws_teleport"),
                  std::string::npos);
    }
}

TEST(PearlItemTest, MaterialOnlyTheToolIsNamed) {
    // A tool carrying the flag is a dead-channel typo (the shears'
    // material-rejection mirror, inverted): named.
    ItemRegistry items;
    auto r = items.registerItem(parseDef(R"({
      "id": "pearl_axe",
      "type": "tool",
      "durability": 10,
      "attack_damage": 3,
      "throws_teleport": true,
      "throw_sound": "a",
      "teleport_sound": "b"
    })"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("throws_teleport"), std::string::npos);
}

TEST(PearlItemTest, OneRightClickExclusivityNamed) {
    // Every other channel fight is named (the parseIgnites block's
    // shape — the one-item-owns-one-right-click discipline): food,
    // drink, launcher, igniter, dye, the bucket pair, shears, armor.
    const std::vector<std::pair<std::string, std::string>> conflicts = {
        {R"(, "restore_hunger": 4)", "restore_hunger"},
        {R"(, "drink_time": 1.5)", "drink_time"},
        {R"(, "shoot_speed": 15)", "shoot_speed"},
        {R"(, "ignites_flamable": true)", "ignites_flamable"},
        {R"(, "dye_color": "red")", "dye_color"},
        {R"(, "collects_fluid": ["water"])", "collects_fluid"},
        {R"(, "pours_fluid": "lava")", "pours_fluid"},
        {R"(, "shears": true)", "shears"},
    };
    for (const auto& [extra, named] : conflicts) {
        ItemRegistry items;
        auto r = items.registerItem(parseDef(pearlDef(extra)));
        ASSERT_FALSE(r.ok()) << extra;
        EXPECT_NE(r.error().message.find(named), std::string::npos)
            << r.error().message;
    }
    // The armor pair (the strict pair — the material-only gate).
    {
        ItemRegistry items;
        auto r = items.registerItem(
            parseDef(R"({
          "id": "ender_pearl",
          "type": "material",
          "throws_teleport": true,
          "throw_sound": "a",
          "teleport_sound": "b",
          "armor_slot": "head",
          "armor_points": 2
        })"));
        ASSERT_FALSE(r.ok());
        EXPECT_NE(r.error().message.find("armor_slot"), std::string::npos);
    }
}

TEST(PearlItemTest, LegacyItemsStayCold) {
    // The structural-immunity root: every pre-V3-10b item (the shipped
    // apple / shears shape) keeps the flag false — the tryThrowPearl's
    // predicate is cold on them, the bit-identical legacy path.
    ItemRegistry items;
    const std::vector<std::string> defs = {
        R"({"id":"apple","type":"material","restore_hunger":4})",
        R"({"id":"flint_and_steel","type":"material","durability":64,"ignites_flamable":true})"};
    for (const std::string& def : defs) {
        ASSERT_TRUE(items.registerItem(parseDef(def)).ok()) << def;
    }
    for (const ItemID iid : items.allIds()) {
        EXPECT_FALSE(items.at(iid).throwsTeleport);
        EXPECT_TRUE(items.at(iid).throwSound.empty());
        EXPECT_TRUE(items.at(iid).teleportSound.empty());
    }
}

// ---------------------------------------------------------------------------
// The Projectile's trailing-parameter teleport flag (the legacy call's
// bit-identical default).
// ---------------------------------------------------------------------------

TEST(PearlProjectileTest, TeleportFlagDefaultsFalse) {
    // The legacy shape (every pre-V3-10b call site — the arrow's full
    // parameter list, the ghast's explode tail): the flag defaults
    // false, the explodes() gate untouched — the bit-identical
    // red line's unit witness.
    World world;
    BlockRegistry blocks;
    Projectile legacy(world, blocks, {0.5f, 10.0f, 0.5f}, {0.0f, 0.0f, 1.0f});
    EXPECT_FALSE(legacy.teleports());
    EXPECT_FALSE(legacy.explodes());
    Projectile arrow(world, blocks, {0.5f, 10.0f, 0.5f}, {0.0f, 0.0f, 1.0f},
                     15.0f, 2.0f, false, 1.0f, "arrow");
    EXPECT_FALSE(arrow.teleports());
    // The pearl's shape: the trailing true (damage 0, hurtPlayer false
    // — the zero-damage channel).
    Projectile pearl(world, blocks, {0.5f, 10.0f, 0.5f}, {0.0f, 0.0f, 1.0f},
                     30.0f, 0.0f, false, 1.0f, std::string {}, -1, 0.0, 0,
                     0.0f, std::string {}, true);
    EXPECT_TRUE(pearl.teleports());
    EXPECT_FALSE(pearl.explodes());
    EXPECT_FALSE(pearl.hurtPlayer());
    EXPECT_FLOAT_EQ(pearl.velocity().z, 30.0f); // the launch's full speed
}
