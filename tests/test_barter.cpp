// V3-14c (piglin bartering): the barter init cross-check
// (entity/barter_check.cpp — the pure function the unit tests own; the
// game's init is a thin adapter, the checkWoolDrops pattern).
#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

#include "block/block_registry.hpp"
#include "core/asset_manager.hpp"
#include "core/json.hpp"
#include "entity/barter_check.hpp"
#include "entity/mob_registry.hpp"
#include "item/item_registry.hpp"

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

ItemRegistry makeItems(const std::vector<std::string>& names) {
    ItemRegistry items;
    for (const std::string& n : names) {
        const std::string def = R"({
  "id": ")" + n + R"(",
  "type": "material",
  "max_stack": 64,
  "icon": ")" + n + R"("
})";
        if (!items.registerItem(parseJson(def)).ok()) {
            ADD_FAILURE() << "fixture item " << n;
        }
    }
    return items;
}

// A bartering mob def with the input + a single output (the lever the
// rejection tests mutate). The token substitution keeps the JSON in one
// piece (the raw-string-concatenation trap — PITS 28's sibling).
std::string barterMob(const std::string& input, const std::string& output,
                      const std::string& count = "\"count\": 1",
                      const std::string& id = "piglin") {
    // The tokens are substituted in one pass each (the raw-string
    // concatenation trap — a lone `)"` in the JSON would close the
    // literal early; plain escaped strings keep the shape auditable).
    std::string def =
        "{\"id\": \"" + id + "\",\n"
        " \"properties\": {\n"
        "  \"health\": 16, \"speed\": 1.0, \"detect_radius\": 16,\n"
        "  \"attack_range\": 1.6, \"attack_damage\": 6,\n"
        "  \"attack_cooldown\": 1.5, \"wander_interval\": 2.5,\n"
        "  \"neutral\": true,\n"
        "  \"barter_input\": \"" + input + "\",\n"
        "  \"barter_outputs\": [{\"item\": \"" + output +
        "\", \"weight\": 1, " + count + "}]\n"
        " },\n"
        " \"model\": {\n"
        "  \"width\": 0.6, \"height\": 1.8,\n"
        "  \"boxes\": [{\"pos\": [0.0, 1.0, 0.0],\n"
        "                \"size\": [0.5, 1.0, 0.5], \"texture\": \"p\"}]\n"
        " }\n"
        "}";
    return def;
}

MobRegistry makeBarterMob(const std::string& def) {
    MobRegistry mobs;
    auto r = mobs.registerMob(parseJson(def));
    if (!r.ok()) {
        ADD_FAILURE() << "fixture mob: " << r.error().message;
    }
    return mobs;
}

} // namespace

TEST(BarterCheckTest, ShippedPiglinPasses) {
    // The REAL assets: the piglin's barter table is complete against
    // the item registry (the input + all 12 outputs exist, every
    // rolled top fits its stack). The other 13 mobs are non-barter
    // (the empty-input skip).
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    MobRegistry mobs;
    ASSERT_TRUE(mobs.loadDirectory(assets, "data/mobs").ok());
    ItemRegistry items;
    BlockRegistry blocks;
    ASSERT_TRUE(blocks.loadDirectory(assets, "data/blocks").ok());
    // The implicit block items FIRST (the game's init order — the
    // barter table's block-item outputs: gravel / nether_brick /
    // obsidian / soul_sand).
    ASSERT_TRUE(items.buildBlockItems(blocks).ok());
    ASSERT_TRUE(items.loadDirectory(assets, "data/items").ok());
    auto r = checkBarter(mobs, items);
    EXPECT_TRUE(r.ok()) << r.error().message;
}

TEST(BarterCheckTest, NonBarterRegistryPasses) {
    // The structural-immunity witness: a registry with NO barter data
    // (every pre-V3-14c mob) passes trivially.
    MobRegistry mobs;
    ASSERT_TRUE(
        mobs.registerMob(parseJson(R"({
      "id": "zombie",
      "properties": {"health": 20, "speed": 0.8, "detect_radius": 0,
                "attack_range": 0, "attack_damage": 0, "attack_cooldown": 1.0, "wander_interval": 2.0},
      "model": {"width": 0.6, "height": 1.8,
                "boxes": [{"pos": [0, 1, 0], "size": [0.5, 1, 0.5],
                           "texture": "z"}]}
    })")).ok());
    ItemRegistry items = makeItems({"rotten_flesh"});
    EXPECT_TRUE(checkBarter(mobs, items).ok());
}

TEST(BarterCheckTest, UnknownInputIsNamed) {
    MobRegistry mobs =
        makeBarterMob(barterMob("gold_nugget", "quartz"));
    ItemRegistry items = makeItems({"quartz"});
    auto r = checkBarter(mobs, items);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("gold_nugget"), std::string::npos);
    EXPECT_NE(r.error().message.find("piglin"), std::string::npos);
}

TEST(BarterCheckTest, UnknownOutputIsNamed) {
    MobRegistry mobs =
        makeBarterMob(barterMob("gold_ingot", "crying_obsidian"));
    ItemRegistry items = makeItems({"gold_ingot"});
    auto r = checkBarter(mobs, items);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("crying_obsidian"), std::string::npos);
}

TEST(BarterCheckTest, OutputAboveMaxStackIsNamed) {
    // The rolled top (countMax) exceeds the item's max stack: named
    // (the drops' Phase 38 twin). The parser's [1, 64] bound is the
    // DATA's ceiling; the stack check bites against a SMALL stack
    // (a max_stack 16 item, the ender_pearl class).
    MobRegistry mobs =
        makeBarterMob(barterMob("gold_ingot", "quartz",
                                R"("count_min": 1, "count_max": 64)"));
    ItemRegistry items;
    const std::vector<std::pair<std::string, int>> stacks = {
        {"gold_ingot", 64}, {"quartz", 16}};
    for (const auto& [name, stack] : stacks) {
        const std::string def =
            std::string("{\"id\": \"") + name +
            "\", \"type\": \"material\", \"max_stack\": " +
            std::to_string(stack) + ", \"icon\": \"" + name + "\"}";
        ASSERT_TRUE(items.registerItem(parseJson(def)).ok());
    }
    auto r = checkBarter(mobs, items);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("max stack"), std::string::npos);
}

TEST(BarterCheckTest, FirstFailingMobIsNamedInRegistryOrder) {
    // Two bartering mobs, the FIRST's output unknown: the naming is the
    // registry order's (the init cross-check convention).
    MobRegistry mobs;
    ASSERT_TRUE(
        mobs.registerMob(parseJson(barterMob("gold_ingot", "nope_a",
                                             R"("count": 1)", "piglin_a")))
            .ok());
    ASSERT_TRUE(
        mobs.registerMob(parseJson(barterMob("gold_ingot", "nope_b",
                                             R"("count": 1)", "piglin_b")))
            .ok());
    ItemRegistry items = makeItems({"gold_ingot"});
    auto r = checkBarter(mobs, items);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("nope_a"), std::string::npos);
    EXPECT_EQ(r.error().message.find("nope_b"), std::string::npos);
}
