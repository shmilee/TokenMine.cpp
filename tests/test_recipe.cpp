// Crafting recipe tests (Phase 12): the recipe registry (strict validation
// cross-checked against the ItemRegistry, directory loading) and the pure
// craft logic (deterministic consumption, all-or-nothing, output fitting).
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <unistd.h>

#include "block/block_registry.hpp"
#include "core/json.hpp"
#include "item/inventory.hpp"
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

// stone + dirt block items + an "ingot" material + a "pick" tool (maxStack
// 1, for output-count bounds).
struct Fixture {
    BlockRegistry blocks;
    ItemRegistry items;
    RecipeRegistry recipes;
    ItemID stone;
    ItemID dirt;
    ItemID ingot;
    ItemID pick;

    Fixture() {
        auto b = blocks.registerBlock(parseJson(
            R"({"id":"stone","model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
        if (!b.ok()) {
            ADD_FAILURE() << b.error().message;
        }
        b = blocks.registerBlock(parseJson(
            R"({"id":"dirt","model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
        if (!b.ok()) {
            ADD_FAILURE() << b.error().message;
        }
        auto r = items.buildBlockItems(blocks);
        if (!r.ok()) {
            ADD_FAILURE() << r.error().message;
        }
        auto ing = items.registerItem(
            parseJson(R"({"id":"ingot","type":"material"})"));
        if (!ing.ok()) {
            ADD_FAILURE() << ing.error().message;
        }
        auto pickR = items.registerItem(
            parseJson(R"({"id":"pick","type":"tool","max_stack":1})"));
        if (!pickR.ok()) {
            ADD_FAILURE() << pickR.error().message;
        }
        stone = items.idForName("stone");
        dirt = items.idForName("dirt");
        ingot = items.idForName("ingot");
        pick = items.idForName("pick");
    }

    Recipe recipe(const std::string& id, ItemID out, int outCount,
                  std::vector<std::pair<ItemID, int>> ingredients) {
        Recipe r;
        r.id = id;
        r.output = out;
        r.outputCount = outCount;
        r.ingredients = std::move(ingredients);
        return r;
    }

    std::vector<ItemStack> snapshot(const Inventory& inv) {
        std::vector<ItemStack> s;
        s.reserve(inv.size());
        for (std::size_t i = 0; i < inv.size(); ++i) {
            s.push_back(inv.at(i));
        }
        return s;
    }
};

} // namespace

// -- RecipeRegistry ------------------------------------------------------------

TEST(RecipeRegistryTest, RegisterValidRecipe) {
    Fixture f;
    auto r = f.recipes.registerRecipe(
        parseJson(R"({"id":"ingot_plate","output":{"item":"ingot","count":4},
                     "ingredients":[{"item":"stone","count":2}]})"),
        f.items);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(f.recipes.count(), 1u);
    const Recipe* rec = f.recipes.find("ingot_plate");
    ASSERT_NE(rec, nullptr);
    EXPECT_EQ(rec->output, f.ingot);
    EXPECT_EQ(rec->outputCount, 4);
    ASSERT_EQ(rec->ingredients.size(), 1u);
    EXPECT_EQ(rec->ingredients[0].first, f.stone);
    EXPECT_EQ(rec->ingredients[0].second, 2);
}

TEST(RecipeRegistryTest, RejectsDuplicateRecipeId) {
    Fixture f;
    const std::string def =
        R"({"id":"x","output":{"item":"ingot","count":1},
           "ingredients":[{"item":"stone","count":1}]})";
    ASSERT_TRUE(f.recipes.registerRecipe(parseJson(def), f.items).ok());
    EXPECT_FALSE(f.recipes.registerRecipe(parseJson(def), f.items).ok());
}

TEST(RecipeRegistryTest, RejectsMissingId) {
    Fixture f;
    EXPECT_FALSE(f.recipes
                     .registerRecipe(
                         parseJson(
                             R"({"output":{"item":"ingot","count":1},
                                "ingredients":[{"item":"stone","count":1}]})"),
                         f.items)
                     .ok());
}

TEST(RecipeRegistryTest, RejectsUnknownOutputItem) {
    Fixture f;
    EXPECT_FALSE(f.recipes
                     .registerRecipe(
                         parseJson(
                             R"({"id":"x","output":{"item":"nope","count":1},
                                "ingredients":[{"item":"stone","count":1}]})"),
                         f.items)
                     .ok());
}

TEST(RecipeRegistryTest, RejectsUnknownIngredientItem) {
    Fixture f;
    EXPECT_FALSE(f.recipes
                     .registerRecipe(
                         parseJson(
                             R"({"id":"x","output":{"item":"ingot","count":1},
                                "ingredients":[{"item":"nope","count":1}]})"),
                         f.items)
                     .ok());
}

TEST(RecipeRegistryTest, RejectsEmptyIngredients) {
    Fixture f;
    EXPECT_FALSE(f.recipes
                     .registerRecipe(
                         parseJson(R"({"id":"x","output":{"item":"ingot","count":1},
                                    "ingredients":[]})"),
                         f.items)
                     .ok());
    // Missing ingredients entirely is rejected too.
    EXPECT_FALSE(f.recipes
                     .registerRecipe(
                         parseJson(R"({"id":"x","output":{"item":"ingot","count":1}})"),
                         f.items)
                     .ok());
}

TEST(RecipeRegistryTest, RejectsBadOutputCount) {
    Fixture f;
    // 0 is below range; 65 exceeds ingot's max stack (64); 2 exceeds the
    // tool's max stack (1).
    EXPECT_FALSE(f.recipes
                     .registerRecipe(
                         parseJson(
                             R"({"id":"x","output":{"item":"ingot","count":0},
                                "ingredients":[{"item":"stone","count":1}]})"),
                         f.items)
                     .ok());
    EXPECT_FALSE(f.recipes
                     .registerRecipe(
                         parseJson(
                             R"({"id":"x","output":{"item":"ingot","count":65},
                                "ingredients":[{"item":"stone","count":1}]})"),
                         f.items)
                     .ok());
    EXPECT_FALSE(f.recipes
                     .registerRecipe(
                         parseJson(
                             R"({"id":"x","output":{"item":"pick","count":2},
                                "ingredients":[{"item":"stone","count":1}]})"),
                         f.items)
                     .ok());
}

TEST(RecipeRegistryTest, RejectsBadIngredientCount) {
    Fixture f;
    EXPECT_FALSE(f.recipes
                     .registerRecipe(
                         parseJson(
                             R"({"id":"x","output":{"item":"ingot","count":1},
                                "ingredients":[{"item":"stone","count":0}]})"),
                         f.items)
                     .ok());
    EXPECT_FALSE(f.recipes
                     .registerRecipe(
                         parseJson(
                             R"({"id":"x","output":{"item":"ingot","count":1},
                                "ingredients":[{"item":"stone","count":"one"}]})"),
                         f.items)
                     .ok());
}

TEST(RecipeRegistryTest, RejectsDuplicateIngredientItem) {
    Fixture f;
    EXPECT_FALSE(f.recipes
                     .registerRecipe(
                         parseJson(
                             R"({"id":"x","output":{"item":"ingot","count":1},
                                "ingredients":[{"item":"stone","count":1},
                                               {"item":"stone","count":2}]})"),
                         f.items)
                     .ok());
}

TEST(RecipeRegistryTest, LoadDirectory) {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        ("mc_recipe_test_" + std::to_string(::getpid()));
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "data" / "recipes");
    {
        std::ofstream f(root / "data/recipes/alpha.json");
        f << R"({"id":"alpha","output":{"item":"ingot","count":2},
                "ingredients":[{"item":"stone","count":1}]})";
    }
    {
        std::ofstream f(root / "data/recipes/beta.json");
        f << R"({"id":"beta","output":{"item":"dirt","count":1},
                "ingredients":[{"item":"ingot","count":3},{"item":"stone","count":1}]})";
    }
    {
        std::ofstream f(root / "data/recipes/ignore.txt");
        f << "not a recipe";
    }

    AssetManager assets;
    ASSERT_TRUE(assets.setRoot(root).ok());
    Fixture f;
    auto r = f.recipes.loadDirectory(assets, f.items, "data/recipes");
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(f.recipes.count(), 2u); // non-JSON files are skipped
    // Files load in sorted order: alpha first.
    EXPECT_EQ(f.recipes.all()[0].id, "alpha");
    EXPECT_EQ(f.recipes.all()[1].id, "beta");
    const Recipe* beta = f.recipes.find("beta");
    ASSERT_NE(beta, nullptr);
    ASSERT_EQ(beta->ingredients.size(), 2u);
    EXPECT_EQ(beta->ingredients[0].first, f.ingot);
    std::filesystem::remove_all(root);
}

TEST(RecipeRegistryTest, LoadDirectoryMissingDirIsEmpty) {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        ("mc_recipe_empty_" + std::to_string(::getpid()));
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    AssetManager assets;
    ASSERT_TRUE(assets.setRoot(root).ok());
    Fixture f;
    auto r = f.recipes.loadDirectory(assets, f.items, "data/recipes");
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(f.recipes.count(), 0u);
    std::filesystem::remove_all(root);
}

TEST(RecipeRegistryTest, LoadDirectoryRejectsInvalidJson) {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        ("mc_recipe_bad_" + std::to_string(::getpid()));
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "data" / "recipes");
    {
        std::ofstream f(root / "data/recipes/bad.json");
        f << "{ this is not json";
    }

    AssetManager assets;
    ASSERT_TRUE(assets.setRoot(root).ok());
    Fixture f;
    auto r = f.recipes.loadDirectory(assets, f.items, "data/recipes");
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("bad.json"), std::string::npos);
    std::filesystem::remove_all(root);
}

// -- craft logic ---------------------------------------------------------------

TEST(CraftTest, CanCraftFalseWhenIngredientMissing) {
    Fixture f;
    Inventory inv;
    inv.setSlot(0, (ItemStack {f.dirt, 3})); // need 4
    const Recipe r = f.recipe("x", f.ingot, 1, {{f.dirt, 4}});
    EXPECT_FALSE(canCraft(inv, r, f.items));
}

TEST(CraftTest, CanCraftTrueWithExactAmount) {
    Fixture f;
    Inventory inv;
    inv.setSlot(0, (ItemStack {f.dirt, 2}));
    inv.setSlot(5, (ItemStack {f.dirt, 2})); // spread across slots
    const Recipe r = f.recipe("x", f.ingot, 1, {{f.dirt, 4}});
    EXPECT_TRUE(canCraft(inv, r, f.items));
}

TEST(CraftTest, CraftConsumesLowestSlotsFirst) {
    Fixture f;
    Inventory inv;
    inv.setSlot(0, (ItemStack {f.stone, 5}));
    inv.setSlot(1, (ItemStack {f.dirt, 2}));
    inv.setSlot(3, (ItemStack {f.dirt, 3}));
    const Recipe r = f.recipe("x", f.ingot, 1, {{f.dirt, 4}});
    auto c = craft(inv, r, f.items);
    ASSERT_TRUE(c.ok()) << c.error().message;
    // Dirt consumed lowest-slot first: slot 1 emptied, slot 3 keeps 1.
    // The output lands in the first empty slot — slot 1, which the
    // consumption of this very craft freed.
    EXPECT_EQ(inv.at(1), (ItemStack {f.ingot, 1}));
    EXPECT_EQ(inv.at(3), (ItemStack {f.dirt, 1}));
    // Untouched slot; slot 2 stays empty (the earlier freed slot won).
    EXPECT_EQ(inv.at(0), (ItemStack {f.stone, 5}));
    EXPECT_TRUE(inv.isEmpty(2));
}

TEST(CraftTest, CraftAllOrNothingLeavesInventoryUnchanged) {
    Fixture f;
    Inventory inv;
    inv.setSlot(0, (ItemStack {f.stone, 5}));
    inv.setSlot(1, (ItemStack {f.dirt, 1}));
    const Recipe r = f.recipe("x", f.ingot, 1,
                              {{f.dirt, 2}, {f.stone, 3}});
    const auto before = f.snapshot(inv);
    EXPECT_FALSE(canCraft(inv, r, f.items));
    auto c = craft(inv, r, f.items);
    ASSERT_FALSE(c.ok());
    EXPECT_EQ(f.snapshot(inv), before); // bit-for-bit unchanged
}

TEST(CraftTest, CraftErrorNamesTheRecipe) {
    Fixture f;
    Inventory inv;
    const Recipe r = f.recipe("fancy_thing", f.ingot, 1, {{f.dirt, 2}});
    auto c = craft(inv, r, f.items);
    ASSERT_FALSE(c.ok());
    EXPECT_NE(c.error().message.find("fancy_thing"), std::string::npos);
}

TEST(CraftTest, CraftOutputMergesIntoPartialStack) {
    Fixture f;
    Inventory inv;
    inv.setSlot(0, (ItemStack {f.stone, 1}));
    inv.setSlot(2, (ItemStack {f.dirt, 60})); // 60 + 4 = 64 = max stack
    const Recipe r = f.recipe("x", f.dirt, 4, {{f.stone, 1}});
    auto c = craft(inv, r, f.items);
    ASSERT_TRUE(c.ok()) << c.error().message;
    EXPECT_EQ(inv.at(2), (ItemStack {f.dirt, 64}));
    EXPECT_TRUE(inv.isEmpty(0));
}

TEST(CraftTest, CraftOutputUsesFirstEmptySlot) {
    Fixture f;
    Inventory inv;
    inv.setSlot(3, (ItemStack {f.stone, 1})); // ingredient NOT in slot 0
    const Recipe r = f.recipe("x", f.ingot, 2, {{f.stone, 1}});
    auto c = craft(inv, r, f.items);
    ASSERT_TRUE(c.ok()) << c.error().message;
    // The first empty slot is 0 (not slot 3, which the craft freed).
    EXPECT_EQ(inv.at(0), (ItemStack {f.ingot, 2}));
    EXPECT_TRUE(inv.isEmpty(3));
}

TEST(CraftTest, CraftFailsWhenNoRoomForOutput) {
    // Every slot full of stone: the ingredient is available, but the output
    // (dirt) has neither an empty slot nor a dirt stack to merge into.
    Fixture f;
    Inventory inv;
    for (std::size_t i = 0; i < inv.size(); ++i) {
        inv.setSlot(i, (ItemStack {f.stone, 64}));
    }
    const Recipe r = f.recipe("x", f.dirt, 1, {{f.stone, 1}});
    const auto before = f.snapshot(inv);
    EXPECT_FALSE(canCraft(inv, r, f.items));
    auto c = craft(inv, r, f.items);
    ASSERT_FALSE(c.ok());
    EXPECT_EQ(f.snapshot(inv), before);
}

TEST(CraftTest, OutputItemCanBeAnIngredient) {
    // 2 dirt -> 1 dirt: legal; the post-consumption fit check makes this
    // exact. Net effect: -1 dirt.
    Fixture f;
    Inventory inv;
    inv.setSlot(0, (ItemStack {f.dirt, 3}));
    const Recipe r = f.recipe("x", f.dirt, 1, {{f.dirt, 2}});
    EXPECT_TRUE(canCraft(inv, r, f.items));
    auto c = craft(inv, r, f.items);
    ASSERT_TRUE(c.ok()) << c.error().message;
    EXPECT_EQ(inv.at(0), (ItemStack {f.dirt, 2}));
}

// -- shaped recipes (Phase 17) ---------------------------------------------------

TEST(ShapedRecipeTest, RegistersValidShapedRecipe) {
    Fixture f;
    auto r = f.recipes.registerRecipe(
        parseJson(R"({"id":"ingot_table","type":"shaped",
                     "output":{"item":"ingot","count":1},
                     "pattern":["II","II"],
                     "key":{"I":"ingot"}})"),
        f.items);
    ASSERT_TRUE(r.ok()) << r.error().message;
    const Recipe* rec = f.recipes.find("ingot_table");
    ASSERT_NE(rec, nullptr);
    EXPECT_EQ(rec->type, RecipeType::Shaped);
    ASSERT_EQ(rec->pattern.size(), 2u);
    EXPECT_EQ(rec->pattern[0], "II");
    EXPECT_EQ(rec->pattern[1], "II");
    ASSERT_EQ(rec->key.size(), 1u);
    EXPECT_EQ(rec->key.at('I'), f.ingot);
    EXPECT_TRUE(rec->ingredients.empty());
}

TEST(ShapedRecipeTest, DefaultTypeIsShapeless) {
    Fixture f;
    auto r = f.recipes.registerRecipe(
        parseJson(R"({"id":"x","output":{"item":"ingot","count":1},
                     "ingredients":[{"item":"stone","count":1}]})"),
        f.items);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(f.recipes.find("x")->type, RecipeType::Shapeless);
    // An explicit "shapeless" is accepted too.
    auto r2 = f.recipes.registerRecipe(
        parseJson(R"({"id":"y","type":"shapeless",
                     "output":{"item":"ingot","count":1},
                     "ingredients":[{"item":"stone","count":1}]})"),
        f.items);
    ASSERT_TRUE(r2.ok()) << r2.error().message;
    EXPECT_EQ(f.recipes.find("y")->type, RecipeType::Shapeless);
}

TEST(ShapedRecipeTest, RejectsUnknownType) {
    Fixture f;
    EXPECT_FALSE(f.recipes
                     .registerRecipe(
                         parseJson(R"({"id":"x","type":"fancy",
                                      "output":{"item":"ingot","count":1},
                                      "ingredients":[{"item":"stone","count":1}]})"),
                         f.items)
                     .ok());
}

TEST(ShapedRecipeTest, RejectsShapedWithoutPatternOrKey) {
    Fixture f;
    EXPECT_FALSE(f.recipes
                     .registerRecipe(
                         parseJson(R"({"id":"x","type":"shaped",
                                      "output":{"item":"ingot","count":1},
                                      "key":{"I":"ingot"}})"),
                         f.items)
                     .ok()); // pattern missing
    EXPECT_FALSE(f.recipes
                     .registerRecipe(
                         parseJson(R"({"id":"y","type":"shaped",
                                      "output":{"item":"ingot","count":1},
                                      "pattern":["II"]})"),
                         f.items)
                     .ok()); // key missing
}

TEST(ShapedRecipeTest, RejectsPatternTooLarge) {
    Fixture f;
    EXPECT_FALSE(f.recipes
                     .registerRecipe(
                         parseJson(R"({"id":"x","type":"shaped",
                                      "output":{"item":"ingot","count":1},
                                      "pattern":["II","II","II","II"],
                                      "key":{"I":"ingot"}})"),
                         f.items)
                     .ok()); // 4 rows
    EXPECT_FALSE(f.recipes
                     .registerRecipe(
                         parseJson(R"({"id":"y","type":"shaped",
                                      "output":{"item":"ingot","count":1},
                                      "pattern":["III I"],
                                      "key":{"I":"ingot"}})"),
                         f.items)
                     .ok()); // 5 chars in a row
}

TEST(ShapedRecipeTest, RejectsEmptyPattern) {
    Fixture f;
    EXPECT_FALSE(f.recipes
                     .registerRecipe(
                         parseJson(R"({"id":"x","type":"shaped",
                                      "output":{"item":"ingot","count":1},
                                      "pattern":["   "],
                                      "key":{}})"),
                         f.items)
                     .ok());
}

TEST(ShapedRecipeTest, RejectsPatternCharWithoutKey) {
    Fixture f;
    EXPECT_FALSE(f.recipes
                     .registerRecipe(
                         parseJson(R"({"id":"x","type":"shaped",
                                      "output":{"item":"ingot","count":1},
                                      "pattern":["IS"],
                                      "key":{"I":"ingot"}})"),
                         f.items)
                     .ok());
}

TEST(ShapedRecipeTest, RejectsUnusedKeyEntry) {
    Fixture f;
    EXPECT_FALSE(f.recipes
                     .registerRecipe(
                         parseJson(R"({"id":"x","type":"shaped",
                                      "output":{"item":"ingot","count":1},
                                      "pattern":["II"],
                                      "key":{"I":"ingot","S":"stone"}})"),
                         f.items)
                     .ok());
}

TEST(ShapedRecipeTest, RejectsUnknownKeyItem) {
    Fixture f;
    EXPECT_FALSE(f.recipes
                     .registerRecipe(
                         parseJson(R"({"id":"x","type":"shaped",
                                      "output":{"item":"ingot","count":1},
                                      "pattern":["NN"],
                                      "key":{"N":"nope"}})"),
                         f.items)
                     .ok());
}

TEST(ShapedRecipeTest, RejectsBadKeyEntry) {
    Fixture f;
    // A two-character key entry is not a single pattern char.
    EXPECT_FALSE(f.recipes
                     .registerRecipe(
                         parseJson(R"({"id":"x","type":"shaped",
                                      "output":{"item":"ingot","count":1},
                                      "pattern":["II"],
                                      "key":{"IN":"ingot"}})"),
                         f.items)
                     .ok());
}

TEST(ShapedRecipeTest, ShapedRejectsIngredients) {
    Fixture f;
    EXPECT_FALSE(f.recipes
                     .registerRecipe(
                         parseJson(R"({"id":"x","type":"shaped",
                                      "output":{"item":"ingot","count":1},
                                      "pattern":["II"],
                                      "key":{"I":"ingot"},
                                      "ingredients":[{"item":"stone","count":1}]})"),
                         f.items)
                     .ok());
}

TEST(ShapedRecipeTest, ShapelessRejectsPatternAndKey) {
    Fixture f;
    EXPECT_FALSE(f.recipes
                     .registerRecipe(
                         parseJson(R"({"id":"x",
                                      "output":{"item":"ingot","count":1},
                                      "ingredients":[{"item":"stone","count":1}],
                                      "pattern":["SS"]})"),
                         f.items)
                     .ok());
    EXPECT_FALSE(f.recipes
                     .registerRecipe(
                         parseJson(R"({"id":"y",
                                      "output":{"item":"ingot","count":1},
                                      "ingredients":[{"item":"stone","count":1}],
                                      "key":{"S":"stone"}})"),
                         f.items)
                     .ok());
}

TEST(ShapedRecipeTest, ShapedPatternWithHole) {
    Fixture f;
    // A T-shape with an interior space: legal, and the space is not a key
    // entry.
    auto r = f.recipes.registerRecipe(
        parseJson(R"({"id":"t","type":"shaped",
                     "output":{"item":"ingot","count":2},
                     "pattern":["III"," I "],
                     "key":{"I":"ingot"}})"),
        f.items);
    ASSERT_TRUE(r.ok()) << r.error().message;
    const Recipe* rec = f.recipes.find("t");
    ASSERT_NE(rec, nullptr);
    EXPECT_EQ(rec->pattern[1], " I ");
}
