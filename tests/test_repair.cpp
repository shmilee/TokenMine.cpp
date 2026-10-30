// UX-5: the material repair system (Minecraft's anvil, crafting-grid
// form) — the pure match/apply over the crafting grid (item/repair, the
// campfire / durability pattern: the unit tests own the logic, the UI's
// output click and the game's init are thin adapters) + the GridMatch
// repair fallback (the recipe match always wins) + the InventoryUI
// clickOutput round (the repaired tool leaves the grid into the carry,
// one material unit spent) + the 'repair_with' parse (strict name, the
// ammo discipline) + the init cross-check (pairing + existence) + the
// shipped content pin (every durable item's exact material; no
// non-durable item carries the field).
#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "core/json.hpp"
#include "item/crafting_grid.hpp"
#include "item/inventory.hpp"
#include "item/item_registry.hpp"
#include "item/repair.hpp"
#include "item/recipe.hpp"
#include "ui/inventory_ui.hpp"

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

// Reads one real asset file (the content-pin tests run from the repo
// root — the other asset tests' convention).
std::string readAsset(const std::string& rel) {
    std::ifstream in(rel);
    if (!in) {
        ADD_FAILURE() << "cannot open " << rel;
        return "";
    }
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// The fixture: a repairable tool (iron pickaxe, max 250 -> one material
// = 62.5), a repairable armor piece (leather helmet, max 55 -> 13.75),
// a STACKABLE repairable (the flint and steel, the registry default
// max stack 64 — the count-preservation case), a durable but NOT
// repairable tool (the wooden pickaxe carries no 'repair_with'), the
// repair materials, and the stone recipe (shapeless 4 cobblestone ->
// 1 stone — a control: no harness grid can match it, so the no-recipe
// repair fallback fires everywhere in the shared fixture; the
// recipe-priority probe gets its OWN fixture, see PriorityFixture).
struct Fixture {
    ItemRegistry items;
    RecipeRegistry recipes;
    Inventory inv;
    CraftingGrid grid;
    InventoryUI ui;

    Fixture() : ui(inv, items, recipes) {
        ui.bind(grid);
        const struct {
            const char* def;
        } defs[] = {
            {R"({"id":"iron_pickaxe","type":"tool",)"
             R"("tool":{"break_speed":4.0},"max_stack":1,)"
             R"("durability":250,"repair_with":"iron_ingot"})"},
            {R"({"id":"wooden_pickaxe","type":"tool",)"
             R"("tool":{"break_speed":2.0},"max_stack":1,)"
             R"("durability":59})"},
            {R"({"id":"leather_helmet","type":"material","max_stack":1,)"
             R"("armor_slot":"head","armor_points":1,)"
             R"("durability":55,"repair_with":"leather"})"},
            {R"({"id":"flint_and_steel","type":"material",)"
             R"("ignites_flamable":true,"durability":64,)"
             R"("repair_with":"flint"})"},
            {R"({"id":"iron_ingot","type":"material"})"},
            {R"({"id":"leather","type":"material"})"},
            {R"({"id":"flint","type":"material"})"},
            {R"({"id":"cobblestone","type":"material"})"},
            {R"({"id":"stone","type":"material"})"},
        };
        for (const auto& d : defs) {
            auto r = items.registerItem(parseJson(d.def));
            if (!r.ok()) {
                ADD_FAILURE() << d.def << ": " << r.error().message;
            }
        }
        auto rr = recipes.registerRecipe(
            parseJson(R"({"id":"stone","output":{"item":"stone","count":1},)"
                      R"("ingredients":[{"item":"cobblestone","count":4}]})"),
            items);
        if (!rr.ok()) {
            ADD_FAILURE() << rr.error().message;
        }
    }

    ItemID id(const char* name) const { return items.idForName(name); }
};

// The priority probe's OWN fixture: a recipe whose ingredients are
// EXACTLY the repair pair (damaged pickaxe + ingot) — a grid holding
// the pair matches BOTH the recipe and the repair, so the winner is
// the rule under test. Kept out of the shared Fixture: with that
// recipe registered, every harness grid (tool + material) would match
// the recipe and hide the repair fallback.
struct PriorityFixture {
    ItemRegistry items;
    RecipeRegistry recipes;
    Inventory inv;
    CraftingGrid grid;
    InventoryUI ui;

    PriorityFixture() : ui(inv, items, recipes) {
        ui.bind(grid);
        const struct {
            const char* def;
        } defs[] = {
            {R"({"id":"iron_pickaxe","type":"tool",)"
             R"("tool":{"break_speed":4.0},"max_stack":1,)"
             R"("durability":250,"repair_with":"iron_ingot"})"},
            {R"({"id":"iron_ingot","type":"material"})"},
            {R"({"id":"stone","type":"material"})"},
        };
        for (const auto& d : defs) {
            auto r = items.registerItem(parseJson(d.def));
            if (!r.ok()) {
                ADD_FAILURE() << d.def << ": " << r.error().message;
            }
        }
        auto rr = recipes.registerRecipe(
            parseJson(R"({"id":"reinforced","output":{"item":"stone","count":1},)"
                      R"("ingredients":[{"item":"iron_pickaxe","count":1},)"
                      R"({"item":"iron_ingot","count":1}]})"),
            items);
        if (!rr.ok()) {
            ADD_FAILURE() << rr.error().message;
        }
    }

    ItemID id(const char* name) const { return items.idForName(name); }
};

// One full repair round: click the output (the repaired tool rides the
// cursor, the grid's tool cell empties, one material is spent), then
// click the tool cell to put the tool back (the brief's protocol — the
// tool leaves the grid WITH the carry each round and returns to its
// cell for the next one). `want` = the damage the tool must carry back.
void runRepairRound(Fixture& f, int toolCell, float want) {
    ASSERT_TRUE(f.ui.leftClick(-1, -1, /*onOutput=*/true));
    EXPECT_TRUE(f.grid.at(toolCell).empty()); // the tool left its cell
    const ItemStack carry = f.ui.carry();
    ASSERT_TRUE(carry.item == f.id("iron_pickaxe"));
    EXPECT_EQ(carry.count, 1);
    EXPECT_TRUE(f.ui.carrying());
    EXPECT_FLOAT_EQ(carry.damage, want);
    // Put the tool back into its (now empty) cell.
    f.ui.leftClick(-1, toolCell, /*onOutput=*/false);
    ASSERT_TRUE(f.ui.carry().empty());
}

} // namespace

// -- matchRepair: the pure grid match ------------------------------------------

TEST(MatchRepairTest, EmptyGridIsNullopt) {
    Fixture f;
    EXPECT_FALSE(matchRepair(f.grid, f.items).has_value());
}

TEST(MatchRepairTest, UndamagedToolIsNullopt) {
    // Full durability (damage 0): there is nothing to repair.
    Fixture f;
    f.grid.setSlot(0, ItemStack {f.id("iron_pickaxe"), 1});
    f.grid.setSlot(4, ItemStack {f.id("iron_ingot"), 8});
    EXPECT_FALSE(matchRepair(f.grid, f.items).has_value());
}

TEST(MatchRepairTest, NoMaterialIsNullopt) {
    Fixture f;
    f.grid.setSlot(0, ItemStack {f.id("iron_pickaxe"), 1, 125.0f});
    EXPECT_FALSE(matchRepair(f.grid, f.items).has_value());
}

TEST(MatchRepairTest, WrongMaterialIsNullopt) {
    // The pickaxe names iron_ingot; a leather cell is not its material.
    Fixture f;
    f.grid.setSlot(0, ItemStack {f.id("iron_pickaxe"), 1, 125.0f});
    f.grid.setSlot(4, ItemStack {f.id("leather"), 8});
    EXPECT_FALSE(matchRepair(f.grid, f.items).has_value());
}

TEST(MatchRepairTest, DamagedToolWithMaterialIsExact) {
    // One material = +25% of the max: 125 - 250 x 0.25 = 62.5 exact
    // (the float operation order IS the formula).
    Fixture f;
    f.grid.setSlot(2, ItemStack {f.id("iron_pickaxe"), 1, 125.0f});
    f.grid.setSlot(7, ItemStack {f.id("iron_ingot"), 3});
    auto m = matchRepair(f.grid, f.items);
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(m->itemSlot, 2);
    EXPECT_EQ(m->materialSlot, 7);
    EXPECT_FLOAT_EQ(m->outputDamage, 62.5f);
}

TEST(MatchRepairTest, LowestDamagedToolCellWins) {
    // Two damaged repairables: the cell-index order picks slot 1, not 5.
    Fixture f;
    f.grid.setSlot(1, ItemStack {f.id("leather_helmet"), 1, 10.0f});
    f.grid.setSlot(5, ItemStack {f.id("iron_pickaxe"), 1, 125.0f});
    f.grid.setSlot(7, ItemStack {f.id("leather"), 2});
    auto m = matchRepair(f.grid, f.items);
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(m->itemSlot, 1);
    EXPECT_EQ(m->materialSlot, 7);
}

TEST(MatchRepairTest, LowestMaterialCellWins) {
    // Two cells hold the material: the cell-index order picks slot 1.
    Fixture f;
    f.grid.setSlot(0, ItemStack {f.id("iron_pickaxe"), 1, 125.0f});
    f.grid.setSlot(1, ItemStack {f.id("iron_ingot"), 1});
    f.grid.setSlot(6, ItemStack {f.id("iron_ingot"), 8});
    auto m = matchRepair(f.grid, f.items);
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(m->itemSlot, 0);
    EXPECT_EQ(m->materialSlot, 1);
}

TEST(MatchRepairTest, DamagedToolWithoutRepairWithIsNullopt) {
    // Durable but NOT repairable (no 'repair_with' data): the grid is
    // inert for the repair (the wooden pickaxe ships without the field).
    Fixture f;
    f.grid.setSlot(0, ItemStack {f.id("wooden_pickaxe"), 1, 10.0f});
    f.grid.setSlot(4, ItemStack {f.id("iron_ingot"), 2});
    EXPECT_FALSE(matchRepair(f.grid, f.items).has_value());
}

TEST(MatchRepairTest, ArmorSampleIsExact) {
    // The armor piece: 44 - 55 x 0.25 = 44 - 13.75 = 30.25 exact.
    Fixture f;
    f.grid.setSlot(0, ItemStack {f.id("leather_helmet"), 1, 44.0f});
    f.grid.setSlot(4, ItemStack {f.id("leather"), 1});
    auto m = matchRepair(f.grid, f.items);
    ASSERT_TRUE(m.has_value());
    EXPECT_FLOAT_EQ(m->outputDamage, 30.25f);
}

TEST(MatchRepairTest, SmallDamageClampsToZero) {
    // Below a quarter-max: the subtraction goes negative, the clamp
    // lands exactly on 0 (the full-durability state).
    Fixture f;
    f.grid.setSlot(0, ItemStack {f.id("iron_pickaxe"), 1, 10.0f});
    f.grid.setSlot(4, ItemStack {f.id("iron_ingot"), 1});
    auto m = matchRepair(f.grid, f.items);
    ASSERT_TRUE(m.has_value());
    EXPECT_FLOAT_EQ(m->outputDamage, 0.0f);
}

TEST(MatchRepairTest, FourRepairsConvergeToZero) {
    // 187.5 = 3/4 of the max: four 25% steps (187.5 -> 125 -> 62.5 ->
    // 0, all dyadic-exact) exhaust the damage; the fifth round finds no
    // damaged repairable item (damage 0 is the full state).
    Fixture f;
    f.grid.setSlot(0, ItemStack {f.id("iron_pickaxe"), 1, 187.5f});
    f.grid.setSlot(4, ItemStack {f.id("iron_ingot"), 4});
    const float want[4] = {125.0f, 62.5f, 0.0f, 0.0f}; // per round
    for (int round = 0; round < 3; ++round) {
        auto m = matchRepair(f.grid, f.items);
        ASSERT_TRUE(m.has_value()) << "round " << round;
        EXPECT_FLOAT_EQ(m->outputDamage, want[round]);
        applyRepair(f.grid, *m, f.items);
    }
    // Round 4: the damage is already 0... the convergence ends at 0 in
    // round 3, so round 4 sees no damaged item (the nullopt terminal).
    EXPECT_FALSE(matchRepair(f.grid, f.items).has_value());
    EXPECT_FLOAT_EQ(f.grid.at(0).damage, 0.0f);
    EXPECT_EQ(f.grid.at(4).count, 1); // 4 materials - 3 repairs
}

// -- applyRepair: the in-place grid write ---------------------------------------

TEST(ApplyRepairTest, RepairsInPlaceAndSpendsOneMaterial) {
    Fixture f;
    f.grid.setSlot(0, ItemStack {f.id("iron_pickaxe"), 1, 100.0f});
    f.grid.setSlot(4, ItemStack {f.id("iron_ingot"), 3});
    f.grid.setSlot(7, ItemStack {f.id("stone"), 2});
    const RepairMatch m {0, 4, 100.0f - 250.0f * kRepairFraction};
    applyRepair(f.grid, m, f.items);
    // The tool cell is the repaired stack (same item, count 1, 37.5);
    // the material cell lost exactly one; the untouched cell is intact.
    const ItemStack& tool = f.grid.at(0);
    ASSERT_TRUE(tool.item == f.id("iron_pickaxe"));
    EXPECT_EQ(tool.count, 1);
    EXPECT_FLOAT_EQ(tool.damage, 37.5f);
    const ItemStack& mat = f.grid.at(4);
    ASSERT_TRUE(mat.item == f.id("iron_ingot"));
    EXPECT_EQ(mat.count, 2);
    EXPECT_EQ(f.grid.at(7), (ItemStack {f.id("stone"), 2}));
}

TEST(ApplyRepairTest, PreservesTheToolCount) {
    // The stackable repairable (the flint and steel, max stack 64): a
    // 5-unit stack at damage 10 repairs to 0 (10 - 64 x 0.25 clamps)
    // and KEEPS its 5 units (the repair never changes the unit count —
    // the count-1 output of the max-stack-1 tools is that premise, not
    // a consumption rule).
    Fixture f;
    f.grid.setSlot(0, ItemStack {f.id("flint_and_steel"), 5, 10.0f});
    f.grid.setSlot(4, ItemStack {f.id("flint"), 2});
    auto m = matchRepair(f.grid, f.items);
    ASSERT_TRUE(m.has_value());
    EXPECT_FLOAT_EQ(m->outputDamage, 0.0f);
    applyRepair(f.grid, *m, f.items);
    EXPECT_EQ(f.grid.at(0).count, 5);
    EXPECT_FLOAT_EQ(f.grid.at(0).damage, 0.0f);
    EXPECT_EQ(f.grid.at(4).count, 1);
}

// -- the recipe priority (match() fallback + the click) -------------------------

TEST(RepairPriorityTest, RecipeWinsOverRepair) {
    // The grid holds exactly the repair pair (damaged pickaxe + ingot),
    // which is ALSO the "reinforced" recipe's ingredient bag: match()
    // returns the RECIPE and leaves the repair fallback unset (the
    // fallback is computed only when no recipe matches).
    PriorityFixture f;
    f.grid.setSlot(0, ItemStack {f.id("iron_pickaxe"), 1, 100.0f});
    f.grid.setSlot(4, ItemStack {f.id("iron_ingot"), 1});
    const GridMatch m = f.ui.match();
    ASSERT_TRUE(m.matched());
    ASSERT_TRUE(m.recipe != nullptr);
    EXPECT_EQ(m.recipe->id, "reinforced");
    EXPECT_FALSE(m.repair.has_value());
}

TEST(RepairPriorityTest, OutputClickCraftsTheRecipeNotTheRepair) {
    // The same grid through the click: the recipe is crafted (both
    // ingredient cells consumed — the pickaxe is SPENT as an ingredient,
    // never repaired) while the repair never runs.
    PriorityFixture f;
    f.grid.setSlot(0, ItemStack {f.id("iron_pickaxe"), 1, 100.0f});
    f.grid.setSlot(4, ItemStack {f.id("iron_ingot"), 1});
    ASSERT_TRUE(f.ui.leftClick(-1, -1, /*onOutput=*/true));
    ASSERT_EQ(f.ui.carry(), (ItemStack {f.id("stone"), 1}));
    EXPECT_TRUE(f.grid.empty()); // the pair was consumed as ingredients
}

// -- the 'repair_with' parse + the init cross-check -----------------------------

TEST(RepairParseTest, RejectsTheExplicitEmptyString) {
    // (The probe item is a tool — the durability allowed set is the
    // use-action items, so a bare material could not carry the pair.)
    Fixture f;
    auto r = f.items.registerItem(parseJson(
        R"({"id":"pickaxe","type":"tool","tool":{"break_speed":2.0},)"
        R"("max_stack":1,"durability":59,"repair_with":""})"));
    ASSERT_FALSE(r.ok());
    EXPECT_EQ(r.error().message, "'repair_with' must be a non-empty item name");
}

TEST(RepairParseTest, RejectsTheNonString) {
    Fixture f;
    auto r = f.items.registerItem(parseJson(
        R"({"id":"pickaxe","type":"tool","tool":{"break_speed":2.0},)"
        R"("max_stack":1,"durability":59,"repair_with":7})"));
    ASSERT_FALSE(r.ok());
    EXPECT_EQ(r.error().message, "'repair_with' must be a non-empty item name");
}

TEST(RepairParseTest, AcceptsTheValidName) {
    Fixture f;
    auto r = f.items.registerItem(parseJson(
        R"({"id":"iron_axe","type":"tool","tool":{"break_speed":4.0},)"
        R"("max_stack":1,"durability":250,"repair_with":"iron_ingot"})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(f.items.at(*r).repairWith, "iron_ingot");
}

TEST(RepairInitTest, RejectsRepairWithoutDurability) {
    // The pairing discipline (the game init's check): a 'repair_with'
    // on an item without a 'durability' is a no-op typo.
    ItemRegistry items;
    auto r = items.registerItem(parseJson(
        R"({"id":"stick","type":"material","repair_with":"diamond"})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    auto check = checkRepairMaterials(items);
    ASSERT_FALSE(check.ok());
    EXPECT_EQ(check.error().message,
              "item 'stick' has 'repair_with' but no 'durability' "
              "(repair is only for durable items)");
}

TEST(RepairInitTest, RejectsTheUnknownMaterial) {
    // The existence cross-check (the ammo / grows_to discipline): a
    // material that is not an item fails init loudly, named.
    ItemRegistry items;
    auto r = items.registerItem(parseJson(
        R"({"id":"iron_pickaxe","type":"tool",)"
        R"("tool":{"break_speed":4.0},"max_stack":1,)"
        R"("durability":250,"repair_with":"ingotz"})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    auto check = checkRepairMaterials(items);
    ASSERT_FALSE(check.ok());
    EXPECT_EQ(check.error().message,
              "item 'iron_pickaxe' references unknown repair material "
              "'ingotz'");
}

TEST(RepairInitTest, PassesTheValidPairs) {
    // The fixture registry: every repairable names an existing item —
    // the check passes.
    Fixture f;
    EXPECT_TRUE(checkRepairMaterials(f.items).ok());
}

TEST(RepairInitTest, NamesTheFirstBadItemInRegistryOrder) {
    // Two bad items: the FIRST one registered (the file-sorted order)
    // is the one named (the CRAFT-3 naming discipline).
    ItemRegistry items;
    ASSERT_TRUE(items
                    .registerItem(parseJson(
                        R"({"id":"aaa_stick","type":"material",)"
                        R"("repair_with":"diamond"})"))
                    .ok());
    ASSERT_TRUE(items
                    .registerItem(parseJson(
                        R"({"id":"zzz_pickaxe","type":"tool",)"
                        R"("tool":{"break_speed":2.0},"max_stack":1,)"
                        R"("durability":59,"repair_with":"ingotz"})"))
                    .ok());
    auto check = checkRepairMaterials(items);
    ASSERT_FALSE(check.ok());
    EXPECT_EQ(check.error().message,
              "item 'aaa_stick' has 'repair_with' but no 'durability' "
              "(repair is only for durable items)");
}

// -- the shipped content pin (the real assets) ----------------------------------

TEST(RepairContentTest, Pinned28ExactMaterials) {
    // The real assets: all 28 durable items (the SURV-3 27 + the
    // SURV-5 poison_bow) carry the exact MC repair material — the
    // 9 tools by family (wood -> planks, stone -> cobblestone, iron ->
    // iron_ingot), the 6-bow family -> stick, the flint and steel ->
    // flint, the 12 armor pieces by material tier.
    struct {
        const char* file;
        const char* material;
    } cases[] = {
        {"assets/data/items/wooden_pickaxe.json", "planks"},
        {"assets/data/items/wooden_axe.json", "planks"},
        {"assets/data/items/wooden_shovel.json", "planks"},
        {"assets/data/items/stone_pickaxe.json", "cobblestone"},
        {"assets/data/items/stone_axe.json", "cobblestone"},
        {"assets/data/items/stone_shovel.json", "cobblestone"},
        {"assets/data/items/iron_pickaxe.json", "iron_ingot"},
        {"assets/data/items/iron_axe.json", "iron_ingot"},
        {"assets/data/items/iron_shovel.json", "iron_ingot"},
        {"assets/data/items/bow.json", "stick"},
        {"assets/data/items/power_bow.json", "stick"},
        {"assets/data/items/speed_bow.json", "stick"},
        {"assets/data/items/infinite_bow.json", "stick"},
        {"assets/data/items/master_bow.json", "stick"},
        {"assets/data/items/poison_bow.json", "stick"},
        {"assets/data/items/flint_and_steel.json", "flint"},
        {"assets/data/items/leather_helmet.json", "leather"},
        {"assets/data/items/leather_chestplate.json", "leather"},
        {"assets/data/items/leather_leggings.json", "leather"},
        {"assets/data/items/leather_boots.json", "leather"},
        {"assets/data/items/iron_helmet.json", "iron_ingot"},
        {"assets/data/items/iron_chestplate.json", "iron_ingot"},
        {"assets/data/items/iron_leggings.json", "iron_ingot"},
        {"assets/data/items/iron_boots.json", "iron_ingot"},
        {"assets/data/items/diamond_helmet.json", "diamond"},
        {"assets/data/items/diamond_chestplate.json", "diamond"},
        {"assets/data/items/diamond_leggings.json", "diamond"},
        {"assets/data/items/diamond_boots.json", "diamond"},
    };
    ASSERT_EQ(sizeof(cases) / sizeof(cases[0]), 28u); // the 28-item set
    for (const auto& c : cases) {
        ItemRegistry items;
        auto parsed = Json::parse(readAsset(c.file));
        ASSERT_TRUE(parsed.ok()) << c.file << ": " << parsed.error().message;
        auto reg = items.registerItem(*parsed);
        ASSERT_TRUE(reg.ok()) << c.file << ": " << reg.error().message;
        const Item& it = items.at(*reg);
        EXPECT_EQ(it.repairWith, c.material) << c.file;
        EXPECT_GT(it.durability, 0.0f) << c.file; // it IS repairable
    }
}

TEST(RepairContentTest, NoNonDurableItemCarriesTheField) {
    // The whole explicit item set: 'repair_with' appears exactly on the
    // 28 durable items — never on a non-durable one (the pairing
    // discipline over the real content).
    int durableWithField = 0;
    const std::filesystem::path dir = "assets/data/items";
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.path().extension() != ".json") {
            continue;
        }
        auto parsed = Json::parse(readAsset(entry.path().string()));
        ASSERT_TRUE(parsed.ok())
            << entry.path().string() << ": " << parsed.error().message;
        const Json& def = *parsed;
        const bool hasField = def.find("repair_with") != nullptr;
        const bool durable = def.find("durability") != nullptr;
        if (hasField) {
            ASSERT_TRUE(durable)
                << "non-durable item carries 'repair_with': "
                << entry.path().string();
            if (durable) {
                ++durableWithField;
            }
        }
    }
    EXPECT_EQ(durableWithField, 28);
}

// -- the InventoryUI output-click round (the harness) ---------------------------

TEST(RepairUiTest, FourRoundsConvergeToZero) {
    // The brief's harness: the pickaxe at 240 (a live stack — one unit
    // short of the 250 break — + 8 ingots); four output-click rounds,
    // the tool riding the carry and returning to its cell each time:
    // 240 -> 177.5 -> 115 -> 52.5 -> 0 (the final round clamps the
    // negative 52.5 - 62.5 to 0). Terminal: damage 0, ingots 4, the
    // tool in its cell.
    Fixture f;
    f.grid.setSlot(0, ItemStack {f.id("iron_pickaxe"), 1, 240.0f});
    f.grid.setSlot(4, ItemStack {f.id("iron_ingot"), 8});
    const float wantAfter[4] = {177.5f, 115.0f, 52.5f, 0.0f};
    for (int round = 0; round < 4; ++round) {
        runRepairRound(f, /*toolCell=*/0, wantAfter[round]);
        EXPECT_FLOAT_EQ(f.grid.at(0).damage, wantAfter[round]) << round;
        EXPECT_EQ(f.grid.at(4).count, 8 - (round + 1)) << round;
    }
    EXPECT_FLOAT_EQ(f.grid.at(0).damage, 0.0f);
    EXPECT_EQ(f.grid.at(0).count, 1); // the tool is home at full durability
    EXPECT_EQ(f.grid.at(4).count, 4);
    // A fifth output click: nothing matches (the tool is full, 4 loose
    // ingots craft no recipe) — the click is a no-op.
    EXPECT_FALSE(f.ui.leftClick(-1, -1, /*onOutput=*/true));
}

TEST(RepairUiTest, RightClickBehavesLikeLeft) {
    Fixture f;
    f.grid.setSlot(0, ItemStack {f.id("iron_pickaxe"), 1, 125.0f});
    f.grid.setSlot(4, ItemStack {f.id("iron_ingot"), 1});
    ASSERT_TRUE(f.ui.rightClick(-1, -1, /*onOutput=*/true));
    ASSERT_EQ(f.ui.carry(), (ItemStack {f.id("iron_pickaxe"), 1, 62.5f}));
    EXPECT_TRUE(f.grid.at(0).empty());
    EXPECT_TRUE(f.grid.at(4).empty()); // the single ingot was spent
}

TEST(RepairUiTest, ForeignCarryRejectsTheClick) {
    // The carry guard: a carried stack of another item cannot take the
    // repair output — the click is a no-op and the grid is untouched.
    Fixture f;
    f.inv.setSlot(0, ItemStack {f.id("cobblestone"), 3});
    f.ui.leftClick(0, -1, /*onOutput=*/false); // pick up
    ASSERT_EQ(f.ui.carry(), (ItemStack {f.id("cobblestone"), 3}));
    f.grid.setSlot(0, ItemStack {f.id("iron_pickaxe"), 1, 100.0f});
    f.grid.setSlot(4, ItemStack {f.id("iron_ingot"), 2});
    EXPECT_FALSE(f.ui.leftClick(-1, -1, /*onOutput=*/true));
    EXPECT_EQ(f.grid.at(0).damage, 100.0f); // not repaired
    EXPECT_EQ(f.grid.at(4).count, 2); // not spent
    ASSERT_EQ(f.ui.carry(), (ItemStack {f.id("cobblestone"), 3}));
}

TEST(RepairUiTest, SameItemDifferentDamageRejectsTheClick) {
    // The SURV-3 merge rule over the cursor: a same-item carry with a
    // DIFFERENT damage cannot merge into the repair output (the stack's
    // damage is a per-unit quantity — a mixed-damage carry is
    // undefined), so the click is a no-op.
    Fixture f;
    f.inv.setSlot(0, ItemStack {f.id("flint_and_steel"), 1, 10.0f});
    f.ui.leftClick(0, -1, /*onOutput=*/false); // pick up
    f.grid.setSlot(0, ItemStack {f.id("flint_and_steel"), 5, 20.0f});
    f.grid.setSlot(4, ItemStack {f.id("flint"), 2});
    // The output would be damage 20 - 64 x 0.25 = 4.0; the carry is 10.
    EXPECT_FALSE(f.ui.leftClick(-1, -1, /*onOutput=*/true));
    EXPECT_EQ(f.grid.at(0).count, 5);
    EXPECT_EQ(f.grid.at(0).damage, 20.0f);
    EXPECT_EQ(f.grid.at(4).count, 2);
}

TEST(RepairUiTest, SameItemSameDamageMergesIntoTheCarry) {
    // The merge case (the stackable repairable): the carry already
    // holds the item at the OUTPUT's damage — the repaired units join
    // it (5 + 1, the flint spent), the grid cell empties.
    Fixture f;
    f.inv.setSlot(0, ItemStack {f.id("flint_and_steel"), 1, 4.0f});
    f.ui.leftClick(0, -1, /*onOutput=*/false); // pick up
    f.grid.setSlot(0, ItemStack {f.id("flint_and_steel"), 5, 20.0f});
    f.grid.setSlot(4, ItemStack {f.id("flint"), 2});
    ASSERT_TRUE(f.ui.leftClick(-1, -1, /*onOutput=*/true));
    ASSERT_EQ(f.ui.carry(), (ItemStack {f.id("flint_and_steel"), 6, 4.0f}));
    EXPECT_TRUE(f.grid.at(0).empty());
    EXPECT_EQ(f.grid.at(4).count, 1);
}
