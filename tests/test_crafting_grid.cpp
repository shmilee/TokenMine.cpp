// Crafting grid tests (Phase 17): the 9-cell storage, shaped/shapeless
// matching (offsets, extra counts, foreign cells), match precedence and
// the exact consumption rules.
#include <gtest/gtest.h>

#include <string>

#include "core/json.hpp"
#include "item/crafting_grid.hpp"
#include "item/item_registry.hpp"
#include "item/recipe.hpp"

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

struct GridFixture {
    ItemRegistry items;
    RecipeRegistry recipes;
    ItemID planks;
    ItemID stick;
    ItemID ingot;
    ItemID stone;

    GridFixture() {
        auto r = items.registerItem(parseJson(
            R"({"id":"planks","type":"material"})"));
        if (!r.ok()) {
            ADD_FAILURE() << r.error().message;
        }
        r = items.registerItem(parseJson(
            R"({"id":"stick","type":"material"})"));
        if (!r.ok()) {
            ADD_FAILURE() << r.error().message;
        }
        r = items.registerItem(parseJson(
            R"({"id":"ingot","type":"material"})"));
        if (!r.ok()) {
            ADD_FAILURE() << r.error().message;
        }
        r = items.registerItem(parseJson(
            R"({"id":"stone","type":"material"})"));
        if (!r.ok()) {
            ADD_FAILURE() << r.error().message;
        }
        planks = items.idForName("planks");
        stick = items.idForName("stick");
        ingot = items.idForName("ingot");
        stone = items.idForName("stone");
        // 2x2 planks -> 1 ingot (shaped); 2 planks -> 4 sticks (shapeless);
        // 1 stone -> 1 ingot (shapeless, for precedence ties).
        auto rr = recipes.registerRecipe(
            parseJson(R"({"id":"table","type":"shaped",
                         "output":{"item":"ingot","count":1},
                         "pattern":["PP","PP"],"key":{"P":"planks"}})"),
            items);
        if (!rr.ok()) {
            ADD_FAILURE() << rr.error().message;
        }
        rr = recipes.registerRecipe(
            parseJson(R"({"id":"sticks","output":{"item":"stick","count":4},
                         "ingredients":[{"item":"planks","count":2}]})"),
            items);
        if (!rr.ok()) {
            ADD_FAILURE() << rr.error().message;
        }
        rr = recipes.registerRecipe(
            parseJson(R"({"id":"stone_ingot","output":{"item":"ingot","count":1},
                         "ingredients":[{"item":"stone","count":1}]})"),
            items);
        if (!rr.ok()) {
            ADD_FAILURE() << rr.error().message;
        }
    }
};

} // namespace

// -- CraftingGrid storage -------------------------------------------------------

TEST(CraftingGridTest, StorageBasics) {
    CraftingGrid grid;
    EXPECT_TRUE(grid.empty());
    const ItemID id = GridFixture {}.planks;
    grid.setSlot(4, (ItemStack {id, 3}));
    EXPECT_FALSE(grid.empty());
    EXPECT_FALSE(grid.isEmpty(4));
    EXPECT_TRUE(grid.isEmpty(0));
    EXPECT_EQ(grid.take(4, 1), 1);
    EXPECT_EQ(grid.at(4), (ItemStack {id, 2}));
    EXPECT_EQ(grid.take(4, 99), 2); // clamps to what is there
    EXPECT_TRUE(grid.isEmpty(4));
    EXPECT_EQ(grid.take(4, 1), 0);
    grid.clear();
    EXPECT_TRUE(grid.empty());
}

// -- shaped matching --------------------------------------------------------------

TEST(MatchShapedTest, Matches2x2AtOrigin) {
    GridFixture f;
    const Recipe* table = f.recipes.find("table");
    ASSERT_NE(table, nullptr);
    CraftingGrid grid;
    grid.setSlot(0, (ItemStack {f.planks, 1}));
    grid.setSlot(1, (ItemStack {f.planks, 1}));
    grid.setSlot(3, (ItemStack {f.planks, 1}));
    grid.setSlot(4, (ItemStack {f.planks, 1}));
    int dx = -1, dy = -1;
    EXPECT_TRUE(matchesShaped(grid, *table, &dx, &dy));
    EXPECT_EQ(dx, 0);
    EXPECT_EQ(dy, 0);
}

TEST(MatchShapedTest, MatchesAtOffset) {
    GridFixture f;
    const Recipe* table = f.recipes.find("table");
    ASSERT_NE(table, nullptr);
    // The 2x2 pattern in the bottom-right corner of the 3x3 model:
    // cells (1,1),(1,2),(2,1),(2,2) = slots 4,5,7,8 -> offset (1,1).
    CraftingGrid grid;
    grid.setSlot(4, (ItemStack {f.planks, 1}));
    grid.setSlot(5, (ItemStack {f.planks, 1}));
    grid.setSlot(7, (ItemStack {f.planks, 1}));
    grid.setSlot(8, (ItemStack {f.planks, 1}));
    int dx = -1, dy = -1;
    EXPECT_TRUE(matchesShaped(grid, *table, &dx, &dy));
    EXPECT_EQ(dx, 1);
    EXPECT_EQ(dy, 1);
}

TEST(MatchShapedTest, FailsOnForeignCell) {
    GridFixture f;
    const Recipe* table = f.recipes.find("table");
    ASSERT_NE(table, nullptr);
    CraftingGrid grid;
    grid.setSlot(0, (ItemStack {f.planks, 1}));
    grid.setSlot(1, (ItemStack {f.planks, 1}));
    grid.setSlot(3, (ItemStack {f.planks, 1}));
    grid.setSlot(4, (ItemStack {f.planks, 1}));
    grid.setSlot(8, (ItemStack {f.stone, 1})); // a cell the pattern misses
    int dx = -1, dy = -1;
    EXPECT_FALSE(matchesShaped(grid, *table, &dx, &dy));
}

TEST(MatchShapedTest, FailsOnWrongItemAndMissingCell) {
    GridFixture f;
    const Recipe* table = f.recipes.find("table");
    ASSERT_NE(table, nullptr);
    CraftingGrid wrong;
    wrong.setSlot(0, (ItemStack {f.stone, 1})); // wrong item in a cell
    wrong.setSlot(1, (ItemStack {f.planks, 1}));
    wrong.setSlot(3, (ItemStack {f.planks, 1}));
    wrong.setSlot(4, (ItemStack {f.planks, 1}));
    int dx = -1, dy = -1;
    EXPECT_FALSE(matchesShaped(wrong, *table, &dx, &dy));
    // ...and an empty pattern cell.
    CraftingGrid missing;
    missing.setSlot(0, (ItemStack {f.planks, 1}));
    missing.setSlot(1, (ItemStack {f.planks, 1}));
    missing.setSlot(4, (ItemStack {f.planks, 1}));
    EXPECT_FALSE(matchesShaped(missing, *table, &dx, &dy));
}

TEST(MatchShapedTest, ExtraCountsInCoveredCellsAreAllowed) {
    GridFixture f;
    const Recipe* table = f.recipes.find("table");
    ASSERT_NE(table, nullptr);
    CraftingGrid grid;
    grid.setSlot(0, (ItemStack {f.planks, 9})); // more than the 1 consumed
    grid.setSlot(1, (ItemStack {f.planks, 1}));
    grid.setSlot(3, (ItemStack {f.planks, 1}));
    grid.setSlot(4, (ItemStack {f.planks, 1}));
    int dx = 0, dy = 0;
    EXPECT_TRUE(matchesShaped(grid, *table, &dx, &dy));
}

// -- shapeless matching -------------------------------------------------------------

TEST(MatchShapelessTest, BagMatchAnywhere) {
    GridFixture f;
    const Recipe* sticks = f.recipes.find("sticks");
    ASSERT_NE(sticks, nullptr);
    // 2 planks split across two cells match (the bag ignores position).
    CraftingGrid grid;
    grid.setSlot(2, (ItemStack {f.planks, 1}));
    grid.setSlot(7, (ItemStack {f.planks, 1}));
    EXPECT_TRUE(matchesShapeless(grid, *sticks));
}

TEST(MatchShapelessTest, ExtraCountsOfAnIngredientSurvive) {
    GridFixture f;
    const Recipe* sticks = f.recipes.find("sticks");
    ASSERT_NE(sticks, nullptr);
    // 3 planks: the recipe needs 2 — the third is allowed to stay.
    CraftingGrid grid;
    grid.setSlot(0, (ItemStack {f.planks, 3}));
    EXPECT_TRUE(matchesShapeless(grid, *sticks));
}

TEST(MatchShapelessTest, FailsOnForeignItemOrShortage) {
    GridFixture f;
    const Recipe* sticks = f.recipes.find("sticks");
    ASSERT_NE(sticks, nullptr);
    CraftingGrid foreign;
    foreign.setSlot(0, (ItemStack {f.planks, 1}));
    foreign.setSlot(1, (ItemStack {f.stone, 1}));
    EXPECT_FALSE(matchesShapeless(foreign, *sticks));
    CraftingGrid shortage;
    shortage.setSlot(0, (ItemStack {f.planks, 1}));
    EXPECT_FALSE(matchesShapeless(shortage, *sticks));
}

// -- matchGridRecipe (precedence) ---------------------------------------------------

TEST(MatchGridRecipeTest, EmptyGridMatchesNothing) {
    GridFixture f;
    CraftingGrid grid;
    EXPECT_FALSE(matchGridRecipe(grid, f.recipes).matched());
}

TEST(MatchGridRecipeTest, ShapedBeatsShapeless) {
    GridFixture f;
    // 4 planks in a 2x2: BOTH the shaped table and the shapeless sticks
    // (2 of the 4) are satisfiable — the shaped one wins.
    CraftingGrid grid;
    grid.setSlot(0, (ItemStack {f.planks, 1}));
    grid.setSlot(1, (ItemStack {f.planks, 1}));
    grid.setSlot(3, (ItemStack {f.planks, 1}));
    grid.setSlot(4, (ItemStack {f.planks, 1}));
    const GridMatch m = matchGridRecipe(grid, f.recipes);
    ASSERT_TRUE(m.matched());
    EXPECT_EQ(m.recipe->id, "table");
}

TEST(MatchGridRecipeTest, RegistryOrderWithinAKind) {
    GridFixture f;
    // One stone: only the shapeless stone_ingot matches (sticks needs
    // planks). Also proves shapeless matching runs at all.
    CraftingGrid grid;
    grid.setSlot(5, (ItemStack {f.stone, 1}));
    const GridMatch m = matchGridRecipe(grid, f.recipes);
    ASSERT_TRUE(m.matched());
    EXPECT_EQ(m.recipe->id, "stone_ingot");
}

TEST(MatchGridRecipeTest, FirstShapedInRegistryOrderWins) {
    GridFixture f;
    // Two shaped recipes could fit the same grid only if they share a
    // pattern; here a second shaped recipe with a different pattern is
    // registered, and the FIRST matching one (registry order) wins.
    auto r = f.recipes.registerRecipe(
        parseJson(R"({"id":"table2","type":"shaped",
                     "output":{"item":"stone","count":1},
                     "pattern":["PP","PP"],"key":{"P":"planks"}})"),
        f.items);
    ASSERT_TRUE(r.ok()) << r.error().message;
    CraftingGrid grid;
    grid.setSlot(0, (ItemStack {f.planks, 1}));
    grid.setSlot(1, (ItemStack {f.planks, 1}));
    grid.setSlot(3, (ItemStack {f.planks, 1}));
    grid.setSlot(4, (ItemStack {f.planks, 1}));
    const GridMatch m = matchGridRecipe(grid, f.recipes);
    ASSERT_TRUE(m.matched());
    EXPECT_EQ(m.recipe->id, "table"); // "table" sorts before "table2"
}

// -- consumption ---------------------------------------------------------------------

TEST(ConsumeGridTest, ShapedConsumesOnePerCoveredCell) {
    GridFixture f;
    CraftingGrid grid;
    grid.setSlot(0, (ItemStack {f.planks, 5}));
    grid.setSlot(1, (ItemStack {f.planks, 1}));
    grid.setSlot(3, (ItemStack {f.planks, 1}));
    grid.setSlot(4, (ItemStack {f.planks, 1}));
    const GridMatch m = matchGridRecipe(grid, f.recipes);
    ASSERT_TRUE(m.matched());
    consumeGrid(grid, m);
    // Exactly one from each of the 4 covered cells; the 4 extras stay.
    EXPECT_EQ(grid.at(0), (ItemStack {f.planks, 4}));
    EXPECT_TRUE(grid.isEmpty(1));
    EXPECT_TRUE(grid.isEmpty(3));
    EXPECT_TRUE(grid.isEmpty(4));
}

TEST(ConsumeGridTest, ShapedConsumesAtTheMatchedOffset) {
    GridFixture f;
    // Pattern in the bottom-right corner (offset 1,1): the uncovered
    // top-left cells are empty, which is what pins the offset. The
    // shapeless sticks recipe is also satisfiable — the shaped one must
    // win, and consumption must hit the offset cells, not (0, 0).
    CraftingGrid grid;
    grid.setSlot(4, (ItemStack {f.planks, 1}));
    grid.setSlot(5, (ItemStack {f.planks, 1}));
    grid.setSlot(7, (ItemStack {f.planks, 1}));
    grid.setSlot(8, (ItemStack {f.planks, 1}));
    const GridMatch m = matchGridRecipe(grid, f.recipes);
    ASSERT_TRUE(m.matched());
    EXPECT_EQ(m.recipe->id, "table");
    ASSERT_EQ(m.dx, 1);
    ASSERT_EQ(m.dy, 1);
    consumeGrid(grid, m);
    EXPECT_TRUE(grid.empty()); // all four offset cells consumed
}

TEST(ConsumeGridTest, ShapelessConsumesLowestSlotFirst) {
    GridFixture f;
    const Recipe* sticks = f.recipes.find("sticks");
    ASSERT_NE(sticks, nullptr);
    CraftingGrid grid;
    grid.setSlot(2, (ItemStack {f.planks, 1}));
    grid.setSlot(7, (ItemStack {f.planks, 3}));
    GridMatch m;
    m.recipe = sticks;
    consumeGrid(grid, m);
    // 2 needed: slot 2 (lowest) emptied, slot 7 keeps 2.
    EXPECT_TRUE(grid.isEmpty(2));
    EXPECT_EQ(grid.at(7), (ItemStack {f.planks, 2}));
}

// -- Phase 48: bread (3 wheat -> 1 bread, shapeless) --------------------------------

namespace {

struct BreadFixture {
    ItemRegistry items;
    RecipeRegistry recipes;
    ItemID wheat;
    ItemID bread;

    BreadFixture() {
        auto r = items.registerItem(parseJson(
            R"({"id":"wheat","type":"material"})"));
        if (!r.ok()) {
            ADD_FAILURE() << r.error().message;
        }
        r = items.registerItem(parseJson(
            R"({"id":"bread","type":"material","restore_hunger":5})"));
        if (!r.ok()) {
            ADD_FAILURE() << r.error().message;
        }
        wheat = items.idForName("wheat");
        bread = items.idForName("bread");
        auto rr = recipes.registerRecipe(
            parseJson(R"({"id":"bread","output":{"item":"bread","count":1},
                          "ingredients":[{"item":"wheat","count":3}]})"),
            items);
        if (!rr.ok()) {
            ADD_FAILURE() << rr.error().message;
        }
    }
};

} // namespace

TEST(BreadRecipeTest, ThreeWheatMatchesAndCraftsOneBread) {
    BreadFixture f;
    // 3 wheat split across cells -> the shapeless bag matches.
    CraftingGrid grid;
    grid.setSlot(0, (ItemStack {f.wheat, 1}));
    grid.setSlot(4, (ItemStack {f.wheat, 2}));
    const GridMatch m = matchGridRecipe(grid, f.recipes);
    ASSERT_TRUE(m.matched());
    EXPECT_EQ(m.recipe->id, "bread");
    EXPECT_EQ(m.recipe->output, f.bread);
    EXPECT_EQ(m.recipe->outputCount, 1);
    consumeGrid(grid, m);
    // Exactly 3 wheat consumed (all of them); nothing left.
    EXPECT_TRUE(grid.empty());
}

TEST(BreadRecipeTest, ShortageDoesNotMatch) {
    BreadFixture f;
    CraftingGrid grid;
    grid.setSlot(0, (ItemStack {f.wheat, 2})); // only 2 of the 3 needed
    const GridMatch m = matchGridRecipe(grid, f.recipes);
    EXPECT_FALSE(m.matched());
}

TEST(BreadRecipeTest, ExtraWheatSurvives) {
    BreadFixture f;
    // 5 wheat: the recipe needs 3 — the 2 extras stay.
    CraftingGrid grid;
    grid.setSlot(0, (ItemStack {f.wheat, 5}));
    const GridMatch m = matchGridRecipe(grid, f.recipes);
    ASSERT_TRUE(m.matched());
    consumeGrid(grid, m);
    EXPECT_EQ(grid.at(0), (ItemStack {f.wheat, 2}));
}

// -- Phase 49: golden apple (8 gold ingots around 1 apple, shaped) -----------

namespace {

struct GoldenAppleFixture {
    ItemRegistry items;
    RecipeRegistry recipes;
    ItemID apple;
    ItemID gold;
    ItemID goldenApple;

    GoldenAppleFixture() {
        auto r = items.registerItem(
            parseJson(R"({"id":"apple","type":"material","restore_hunger":4})"));
        if (!r.ok()) {
            ADD_FAILURE() << r.error().message;
        }
        r = items.registerItem(
            parseJson(R"({"id":"gold_ingot","type":"material"})"));
        if (!r.ok()) {
            ADD_FAILURE() << r.error().message;
        }
        r = items.registerItem(parseJson(
            R"({"id":"golden_apple","type":"material","restore_hunger":4,
                "grants_effect":{"name":"regeneration","duration":10.0}})"));
        if (!r.ok()) {
            ADD_FAILURE() << r.error().message;
        }
        apple = items.idForName("apple");
        gold = items.idForName("gold_ingot");
        goldenApple = items.idForName("golden_apple");
        auto rr = recipes.registerRecipe(
            parseJson(R"({"id":"golden_apple","type":"shaped",
                          "output":{"item":"golden_apple","count":1},
                          "pattern":["GGG","GAG","GGG"],
                          "key":{"G":"gold_ingot","A":"apple"}})"),
            items);
        if (!rr.ok()) {
            ADD_FAILURE() << rr.error().message;
        }
    }
};

} // namespace

TEST(GoldenAppleRecipeTest, EightGoldAroundAppleMatches) {
    GoldenAppleFixture f;
    CraftingGrid grid;
    grid.setSlot(4, (ItemStack {f.apple, 1})); // the center
    for (int i = 0; i < 9; ++i) {
        if (i != 4) {
            grid.setSlot(i, (ItemStack {f.gold, 1}));
        }
    }
    const GridMatch m = matchGridRecipe(grid, f.recipes);
    ASSERT_TRUE(m.matched());
    EXPECT_EQ(m.recipe->id, "golden_apple");
    EXPECT_EQ(m.recipe->output, f.goldenApple);
    consumeGrid(grid, m);
    EXPECT_TRUE(grid.empty()); // all 9 inputs consumed
}

TEST(GoldenAppleRecipeTest, GoldShortageDoesNotMatch) {
    GoldenAppleFixture f;
    // Only 7 of the 8 surrounding ingots (slots 0 and 8 missing).
    CraftingGrid grid;
    grid.setSlot(4, (ItemStack {f.apple, 1}));
    for (int i = 1; i < 8; ++i) {
        grid.setSlot(i, (ItemStack {f.gold, 1}));
    }
    const GridMatch m = matchGridRecipe(grid, f.recipes);
    EXPECT_FALSE(m.matched());
}

TEST(GoldenAppleRecipeTest, AppleNotInCenterDoesNotMatch) {
    GoldenAppleFixture f;
    // The apple in the corner, gold filling the rest: the shape requires
    // the apple at the pattern center (slot 4).
    CraftingGrid grid;
    grid.setSlot(0, (ItemStack {f.apple, 1}));
    for (int i = 1; i < 9; ++i) {
        grid.setSlot(i, (ItemStack {f.gold, 1}));
    }
    const GridMatch m = matchGridRecipe(grid, f.recipes);
    EXPECT_FALSE(m.matched());
}

// -- Phase 50: farmland (1 dirt -> 1 farmland, shapeless) ---------------------

namespace {

struct FarmlandFixture {
    ItemRegistry items;
    RecipeRegistry recipes;
    ItemID dirt;
    ItemID farmland;

    FarmlandFixture() {
        auto r = items.registerItem(
            parseJson(R"({"id":"dirt","type":"material"})"));
        if (!r.ok()) {
            ADD_FAILURE() << r.error().message;
        }
        r = items.registerItem(
            parseJson(R"({"id":"farmland","type":"material"})"));
        if (!r.ok()) {
            ADD_FAILURE() << r.error().message;
        }
        dirt = items.idForName("dirt");
        farmland = items.idForName("farmland");
        auto rr = recipes.registerRecipe(
            parseJson(R"({"id":"farmland","output":{"item":"farmland","count":1},
                          "ingredients":[{"item":"dirt","count":1}]})"),
            items);
        if (!rr.ok()) {
            ADD_FAILURE() << rr.error().message;
        }
    }
};

} // namespace

TEST(FarmlandRecipeTest, OneDirtMatchesAndCraftsOneFarmland) {
    FarmlandFixture f;
    CraftingGrid grid;
    grid.setSlot(0, (ItemStack {f.dirt, 1}));
    const GridMatch m = matchGridRecipe(grid, f.recipes);
    ASSERT_TRUE(m.matched());
    EXPECT_EQ(m.recipe->id, "farmland");
    EXPECT_EQ(m.recipe->output, f.farmland);
    consumeGrid(grid, m);
    EXPECT_TRUE(grid.empty()); // the single dirt is consumed
}

TEST(FarmlandRecipeTest, ExtraDirtSurvives) {
    FarmlandFixture f;
    // 3 dirt: the recipe needs 1 — the 2 extras stay.
    CraftingGrid grid;
    grid.setSlot(0, (ItemStack {f.dirt, 3}));
    const GridMatch m = matchGridRecipe(grid, f.recipes);
    ASSERT_TRUE(m.matched());
    consumeGrid(grid, m);
    EXPECT_EQ(grid.at(0), (ItemStack {f.dirt, 2}));
}

TEST(FarmlandRecipeTest, EmptyGridDoesNotMatch) {
    FarmlandFixture f;
    CraftingGrid grid;
    const GridMatch m = matchGridRecipe(grid, f.recipes);
    EXPECT_FALSE(m.matched());
}
