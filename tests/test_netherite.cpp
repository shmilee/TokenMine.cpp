// V3-4 (v3.6.0): the netherite material chain — the fourth material
// tier (the design /tmp/opencode/v360_design.md): the smelted ancient
// debris scrap (the ancient_debris block's new smeltable_to — the
// netherrack -> nether_brick CRAFT-6 inheritance precedent), the
// 4-scrap + 4-gold shapeless ingot, the three tools (durability 2031
// MC-exact, the iron tier's project break-speed / enchantability scale,
// the 21-block fast_against list = iron's 10 stone family + the 11
// nether blocks) and the four armor pieces (the diamond tier's MC-exact
// points, the x1.5 durability, the shapeless diamond-upgrade recipes —
// the no-smithing-table deviation). Pure data: zero core code (the
// project's "a material is data" promise, the V2-5 / V3-2 content
// family's test home pattern).
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "block/block_registry.hpp"
#include "core/asset_manager.hpp"
#include "core/enchanting.hpp" // EnchantClass
#include "core/json.hpp"
#include "item/crafting_grid.hpp"
#include "item/furnace.hpp" // smeltSubstepsFor / tickFurnace (the chain)
#include "item/inventory.hpp"
#include "item/item.hpp"
#include "item/item_registry.hpp"
#include "item/recipe.hpp"
#include "item/repair.hpp" // checkRepairMaterials (the init face)

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

std::string readAll(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    EXPECT_TRUE(in) << path;
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// The project FNV-1a 64 pin (the font.png / skin-tile convention, the
// test_item.cpp's 8-hex form).
std::string fnv1a64(const std::string& data) {
    std::uint64_t h = 1469598103934665603ULL; // FNV-1a 64 offset basis
    for (char ch : data) {
        h ^= static_cast<unsigned char>(ch);
        h *= 1099511628211ULL;
    }
    char hex[17];
    std::snprintf(hex, sizeof(hex), "%016llx",
                  static_cast<unsigned long long>(h));
    return hex;
}

void realAssets(AssetManager& assets) {
    if (!assets.setRoot("assets").ok()) {
        throw std::runtime_error("run the tests from the repository root");
    }
}

// The full real chain (blocks -> block items -> items -> recipes).
struct RealChain {
    AssetManager assets;
    BlockRegistry blocks;
    ItemRegistry items;
    RecipeRegistry recipes;

    RealChain() {
        realAssets(assets);
        if (!blocks.loadDirectory(assets, "data/blocks").ok() ||
            !items.buildBlockItems(blocks).ok() ||
            !items.loadDirectory(assets, "data/items").ok() ||
            !recipes.loadDirectory(assets, items, "data/recipes").ok()) {
            throw std::runtime_error("real chain load failed");
        }
    }
};

ItemStack stk(ItemID id, int count) { return ItemStack {id, count}; }

// The 21-block fast_against list (the iron pickaxe's 10 stone family +
// the 11 nether blocks — the correct-tool semantics, the design M2).
const std::vector<std::string> kNetheriteFastAgainst = {
    "stone",          "cobblestone",     "coal_ore",
    "iron_ore",       "gold_ore",        "diamond_ore",
    "lapis_ore",      "glowstone",       "sandstone",
    "smooth_sandstone", "netherrack",    "crimson_nylium",
    "warped_nylium",  "basalt",          "crimson_stem",
    "warped_stem",    "soul_sand",       "soul_soil",
    "ancient_debris", "nether_brick",    "magma_block",
};

} // namespace

// ===========================================================================
// M1: the material chain (scrap + ingot + the smelt's data face)
// ===========================================================================

TEST(V34NetheriteMaterialChain, ScrapAndIngotContentPinned) {
    // The two material items: the coal-chunk / iron-ingot single-box
    // model silhouettes (the quartz / iron_ingot precedent), default
    // max stack 64, no fuel / no food / no smelt / no repair — pure
    // crafting ingredients.
    RealChain c;
    const struct {
        const char* id;
        float x0, y0, z0, x1, y1, z1; // the single box's corners
    } want[] = {
        // the coal chunk box (the scrap is a molten chunk):
        {"netherite_scrap", 0.25f, 0.28f, 0.3f, 0.75f, 0.72f, 0.7f},
        // the iron_ingot box (the ingot is an ingot):
        {"netherite_ingot", 0.15f, 0.4f, 0.3f, 0.85f, 0.6f, 0.7f},
    };
    for (const auto& w : want) {
        SCOPED_TRACE(w.id);
        const Item* it = c.items.findByName(w.id);
        ASSERT_NE(it, nullptr) << w.id;
        EXPECT_EQ(it->type, ItemType::Material) << w.id;
        EXPECT_EQ(it->maxStack, 64) << w.id; // the default
        EXPECT_EQ(it->icon, w.id) << w.id;
        EXPECT_TRUE(it->smeltableTo.empty()) << w.id;
        EXPECT_FLOAT_EQ(it->fuelTime, 0.0f) << w.id;
        EXPECT_TRUE(it->repairWith.empty()) << w.id;
        EXPECT_EQ(it->enchantClass, EnchantClass::None) << w.id;
        ASSERT_TRUE(it->model.has_value()) << w.id;
        EXPECT_EQ(it->model->texture, w.id) << w.id;
        ASSERT_EQ(it->model->elements.size(), 1u) << w.id;
        const auto& e = it->model->elements[0];
        EXPECT_FLOAT_EQ(e.from.x, w.x0) << w.id;
        EXPECT_FLOAT_EQ(e.from.y, w.y0) << w.id;
        EXPECT_FLOAT_EQ(e.from.z, w.z0) << w.id;
        EXPECT_FLOAT_EQ(e.to.x, w.x1) << w.id;
        EXPECT_FLOAT_EQ(e.to.y, w.y1) << w.id;
        EXPECT_FLOAT_EQ(e.to.z, w.z1) << w.id;
        // The icon texture exists on disk (the icon-file discipline).
        EXPECT_TRUE(c.assets.exists("textures/items/" +
                                    std::string(w.id) + ".png"));
    }
}

TEST(V34NetheriteMaterialChain, AncientDebrisSmeltDataPinned) {
    // The chain's smelt stage (the design M1): the ancient_debris block
    // gained smeltable_to netherite_scrap (the netherrack -> nether_
    // brick precedent, the CRAFT-6 block-item inheritance — the item
    // file is impossible, the block owns the name).
    RealChain c;
    const Block* debris = c.blocks.findByName("ancient_debris");
    ASSERT_NE(debris, nullptr);
    EXPECT_EQ(debris->properties.smeltableTo, "netherite_scrap");
    const Item* debrisItem = c.items.findByName("ancient_debris");
    ASSERT_NE(debrisItem, nullptr);
    EXPECT_EQ(debrisItem->type, ItemType::Block);
    EXPECT_EQ(debrisItem->smeltableTo, "netherite_scrap"); // inherited
    EXPECT_EQ(smeltSubstepsFor(*debrisItem), 600); // the 10 s default
    // The product resolves (the init cross-check's unit face).
    ASSERT_NE(c.items.findByName("netherite_scrap"), nullptr);
    // The product's smelt_xp (the smelt-complete grant reads the
    // PRODUCT's value — the V2-11 default 1, no explicit field).
    EXPECT_EQ(c.items.at(c.items.idForName("netherite_scrap")).smeltXp, 1);
    // The existing smelt regression (netherrack -> nether_brick, the
    // V3-2 Part 1's chain — the item-side face).
    const Item* netherrackItem = c.items.findByName("netherrack");
    ASSERT_NE(netherrackItem, nullptr);
    EXPECT_EQ(netherrackItem->smeltableTo, "nether_brick");
    ASSERT_NE(c.items.findByName("nether_brick"), nullptr);
}

// ===========================================================================
// M2: the three tools (the content pins + the fast_against semantics)
// ===========================================================================

TEST(V34NetheriteMaterialChain, ToolsContentPinned) {
    RealChain c;
    const struct {
        const char* id;
        EnchantClass cls;
        float attack;
        int modelBoxes;
    } want[] = {
        {"netherite_pickaxe", EnchantClass::Pick, 4.0f, 3},
        {"netherite_axe", EnchantClass::Axe, 5.0f, 2},
        {"netherite_shovel", EnchantClass::Shovel, 4.5f, 2},
    };
    for (const auto& w : want) {
        SCOPED_TRACE(w.id);
        const Item* it = c.items.findByName(w.id);
        ASSERT_NE(it, nullptr) << w.id;
        EXPECT_EQ(it->type, ItemType::Tool) << w.id;
        // The enchant pairing (the project's iron tier scale 14, the
        // design's documented deviation 5 — not MC's 15).
        EXPECT_EQ(it->enchantClass, w.cls) << w.id;
        EXPECT_EQ(it->enchantability, 14) << w.id;
        EXPECT_FLOAT_EQ(it->durability, 2031.0f) << w.id; // MC exact
        EXPECT_FLOAT_EQ(it->attackDamage, w.attack) << w.id; // iron +1
        EXPECT_EQ(it->maxStack, 1) << w.id;
        EXPECT_EQ(it->icon, w.id) << w.id;
        EXPECT_EQ(it->repairWith, "netherite_ingot") << w.id;
        // The fast_against: the 21-block list (order pinned, the
        // stone family first then the nether blocks) + the multiplier.
        EXPECT_EQ(it->fastAgainst, kNetheriteFastAgainst) << w.id;
        EXPECT_FLOAT_EQ(it->fastAgainstMultiplier, 2.0f) << w.id;
        // The model: the iron tier's geometry cloned, the texture
        // renamed (the iron pickaxe's 3 boxes / axe + shovel's 2).
        ASSERT_TRUE(it->model.has_value()) << w.id;
        EXPECT_EQ(it->model->texture, w.id) << w.id;
        EXPECT_EQ(static_cast<int>(it->model->elements.size()), w.modelBoxes)
            << w.id;
    }
    // The break_speed is the pickaxe's channel (the iron pickaxe's
    // project scale 4.0, deviation 5); the axe / shovel carry no tool
    // section (the iron tier's form — the default 1.0).
    const Item* pick = c.items.findByName("netherite_pickaxe");
    ASSERT_NE(pick, nullptr);
    EXPECT_FLOAT_EQ(pick->breakSpeed, 4.0f);
    EXPECT_FLOAT_EQ(c.items.at(c.items.idForName("netherite_axe")).breakSpeed,
                    1.0f);
    EXPECT_FLOAT_EQ(
        c.items.at(c.items.idForName("netherite_shovel")).breakSpeed, 1.0f);
}

TEST(V34NetheriteMaterialChain, FastAgainstExtendsTheIronList) {
    // The list's composition pin (the design M2): the netherite tools'
    // 21 = the iron tools' 10 stone family (identical prefix, the
    // correct-tool semantics preserved) + the 11 nether blocks.
    RealChain c;
    const Item* iron = c.items.findByName("iron_pickaxe");
    const Item* nether = c.items.findByName("netherite_pickaxe");
    ASSERT_NE(iron, nullptr);
    ASSERT_NE(nether, nullptr);
    ASSERT_EQ(iron->fastAgainst.size(), 10u);
    EXPECT_EQ(std::equal(iron->fastAgainst.begin(),
                         iron->fastAgainst.end(),
                         nether->fastAgainst.begin()),
              true)
        << "the stone-family prefix must be the iron list verbatim";
    for (std::size_t i = 10u; i < nether->fastAgainst.size(); ++i) {
        EXPECT_EQ(c.blocks.idForName(nether->fastAgainst[i]).valid(), true)
            << "nether fast_against[" << i << "]";
    }
}

TEST(V34NetheriteMaterialChain, FastAgainstBlocksResolve) {
    // The init cross-check's unit face (game_init's per-entry block
    // existence — a typo must fail init loudly): every one of the 21
    // names resolves in the real block registry.
    RealChain c;
    for (const std::string& n : kNetheriteFastAgainst) {
        EXPECT_TRUE(c.blocks.idForName(n).valid()) << n;
    }
}

TEST(V34NetheriteMaterialChain, FastAgainstBreakSpeedSemantics) {
    // The netherite pickaxe's 4.0 break speed + 2.0 multiplier on its
    // 21 blocks (effectiveBreakSpeed's pure face): a nether block is
    // 8.0 (= 4.0 x 2.0), an off-list block is the wrong-tool 0.4
    // (= 4.0 x 0.1) — and the IRON pickaxe is NOT fast against the
    // nether blocks (its 10-block list: netherrack is the wrong-tool
    // 0.4 there too).
    RealChain c;
    const Item* nether = c.items.findByName("netherite_pickaxe");
    const Item* iron = c.items.findByName("iron_pickaxe");
    ASSERT_NE(nether, nullptr);
    ASSERT_NE(iron, nullptr);
    const Block* rack = c.blocks.findByName("netherrack");
    const Block* debris = c.blocks.findByName("ancient_debris");
    const Block* brick = c.blocks.findByName("nether_brick");
    const Block* bedrock = c.blocks.findByName("bedrock");
    ASSERT_NE(rack, nullptr);
    ASSERT_NE(debris, nullptr);
    ASSERT_NE(brick, nullptr);
    ASSERT_NE(bedrock, nullptr);
    EXPECT_FLOAT_EQ(effectiveBreakSpeed(*nether, *rack), 8.0f);
    EXPECT_FLOAT_EQ(effectiveBreakSpeed(*nether, *debris), 8.0f);
    EXPECT_FLOAT_EQ(effectiveBreakSpeed(*nether, *brick), 8.0f);
    EXPECT_FLOAT_EQ(effectiveBreakSpeed(*nether, *bedrock), 0.4f);
    // The iron tier is unchanged (the 10-block list's semantics).
    EXPECT_FLOAT_EQ(effectiveBreakSpeed(*iron, *rack), 0.4f);
    EXPECT_FLOAT_EQ(effectiveBreakSpeed(*iron, *bedrock), 0.4f);
    const Block* stone = c.blocks.findByName("stone");
    ASSERT_NE(stone, nullptr);
    EXPECT_FLOAT_EQ(effectiveBreakSpeed(*iron, *stone), 8.0f);
    // The duration's face (the kBreakBaseSeconds x hardness / speed):
    // the netherite pickaxe on netherrack (hardness 0.4) is 20x faster
    // than the iron pickaxe on the same block (8.0 vs 0.4 effective).
    const double netherDur =
        breakDurationSeconds(rack->properties.hardness, 8.0f);
    const double ironDur =
        breakDurationSeconds(rack->properties.hardness, 0.4f);
    EXPECT_NEAR(ironDur / netherDur, 20.0, 1e-5);
}

// ===========================================================================
// M3: the four armor pieces (the content pins + the diamond x1.5 scale)
// ===========================================================================

TEST(V34NetheriteMaterialChain, ArmorContentPinned) {
    RealChain c;
    const struct {
        const char* id;
        const char* slot;
        int points;
        float durability;
    } want[] = {
        {"netherite_helmet", "head", 3, 545.0f},
        {"netherite_chestplate", "chest", 8, 792.0f},
        {"netherite_leggings", "legs", 6, 743.0f},
        {"netherite_boots", "feet", 3, 644.0f},
    };
    int fullSet = 0;
    for (const auto& w : want) {
        SCOPED_TRACE(w.id);
        const Item* it = c.items.findByName(w.id);
        ASSERT_NE(it, nullptr) << w.id;
        EXPECT_EQ(it->type, ItemType::Material) << w.id;
        EXPECT_EQ(it->armorSlot, w.slot) << w.id;
        EXPECT_EQ(it->armorPoints, w.points) << w.id; // MC exact
        EXPECT_FLOAT_EQ(it->durability, w.durability) << w.id; // MC exact
        EXPECT_EQ(it->enchantClass, EnchantClass::Armor) << w.id;
        EXPECT_EQ(it->enchantability, 10) << w.id; // the diamond scale
        EXPECT_EQ(it->maxStack, 1) << w.id;
        EXPECT_EQ(it->icon, w.id) << w.id;
        EXPECT_EQ(it->repairWith, "netherite_ingot") << w.id;
        ASSERT_TRUE(it->model.has_value()) << w.id;
        EXPECT_EQ(it->model->texture, w.id) << w.id;
        EXPECT_EQ(it->model->elements.size(), 1u) << w.id;
        fullSet += it->armorPoints;
    }
    // The full set's 20 points (the MC diamond/netherite full-set
    // value — the max armor, the damage-reduction cap).
    EXPECT_EQ(fullSet, 20);
}

TEST(V34NetheriteMaterialChain, ArmorDurabilityIsTheDiamondScaleTimes15) {
    // The design's x1.5 verification (363 -> 544.5 -> 545 etc.): every
    // netherite piece = round(diamond piece x 1.5), over the SHIPPED
    // diamond values (the cross-material consistency pin).
    RealChain c;
    const char* slots[] = {"helmet", "chestplate", "leggings", "boots"};
    for (const char* s : slots) {
        const std::string dia = std::string("diamond_") + s;
        const std::string net = std::string("netherite_") + s;
        const Item* di = c.items.findByName(dia);
        const Item* ni = c.items.findByName(net);
        ASSERT_NE(di, nullptr) << dia;
        ASSERT_NE(ni, nullptr) << net;
        // round-half-up of the diamond value x 1.5 (the design's
        // 544.5 -> 545 / 742.5 -> 743 / 643.5 -> 644; the chestplate's
        // 792.0 is exact).
        const float rounded = std::floor(di->durability * 1.5f + 0.5f);
        EXPECT_FLOAT_EQ(ni->durability, rounded) << net;
    }
}

TEST(V34NetheriteMaterialChain, RepairMaterialsCheckPinned) {
    // The init cross-check (checkRepairMaterials' pure face): the real
    // registry passes (every repair_with — netherite_ingot included —
    // exists), and a registry missing the ingot names it.
    RealChain c;
    EXPECT_TRUE(checkRepairMaterials(c.items).ok());
    // The missing-material form: a netherite tool whose ingot is not
    // registered (the synthetic twin — the real file's exact fields).
    ItemRegistry bare;
    auto r = bare.registerItem(parseDef(
        R"({"id":"netherite_pickaxe","type":"tool",
            "durability":2031,"repair_with":"netherite_ingot",
            "tool":{"break_speed":4.0}})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    auto bad = checkRepairMaterials(bare);
    EXPECT_FALSE(bad.ok());
    EXPECT_NE(bad.error().message.find("netherite_ingot"),
              std::string::npos);
}

// ===========================================================================
// The icon census (the 9 new FNV pins + the 84 -> 93 count)
// ===========================================================================

TEST(V34NetheriteMaterialChain, IconCensusPinned) {
    // The 9 new icons byte-pinned (generate_item_icons.py salt block
    // 158-179 — the re-run's 84-existing-unchanged proof is the gate's
    // md5 check, the pork / string precedent) + the directory census
    // (84 -> 93 item icons, 93 -> 93 item files: the 1:1 naming).
    const struct {
        const char* path;
        const char* fnv;
    } icons[] = {
        {"assets/textures/items/netherite_scrap.png", "b3254cd7d7c2a5f7"},
        {"assets/textures/items/netherite_ingot.png", "3297acf874a17ad7"},
        {"assets/textures/items/netherite_pickaxe.png", "f6a9651730e72708"},
        {"assets/textures/items/netherite_axe.png", "381c9ed096829f7e"},
        {"assets/textures/items/netherite_shovel.png", "15fab2cd94d91877"},
        {"assets/textures/items/netherite_helmet.png", "82e1cff9488dd633"},
        {"assets/textures/items/netherite_chestplate.png", "68e47a250f09adc8"},
        {"assets/textures/items/netherite_leggings.png", "51df9e2149123625"},
        {"assets/textures/items/netherite_boots.png", "57036b9c13dd0ced"},
    };
    for (const auto& i : icons) {
        EXPECT_EQ(fnv1a64(readAll(i.path)), i.fnv) << i.path;
    }
    int iconCount = 0;
    for (const auto& e :
         std::filesystem::directory_iterator("assets/textures/items")) {
        if (e.is_regular_file() && e.path().extension() == ".png") {
            ++iconCount;
        }
    }
    EXPECT_EQ(iconCount, 97); // 84 pre-V3-4 + 9 + the V3-3 hoglin meat
                              // chain's 2 + the V3-5 building family's 2
                              // (clay_ball / brick)
    int itemCount = 0;
    for (const auto& e :
         std::filesystem::directory_iterator("assets/data/items")) {
        if (e.is_regular_file() && e.path().extension() == ".json") {
            ++itemCount;
        }
    }
    EXPECT_EQ(itemCount, 97); // the 1:1 icon naming (no planks-style
                              // block-item retirement in this family)
}

// ===========================================================================
// The eight recipes (the content pins + the behavior faces)
// ===========================================================================

TEST(V34NetheriteMaterialChain, IngotRecipePinned) {
    RealChain c;
    const Recipe* r = c.recipes.find("netherite_ingot");
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->type, RecipeType::Shapeless);
    EXPECT_EQ(r->output, c.items.idForName("netherite_ingot"));
    EXPECT_EQ(r->outputCount, 1);
    // The MC 4-scrap + 4-gold-NUGGET reduced to the ingot (the
    // documented deviation 1: the project has no nugget item).
    ASSERT_EQ(r->ingredients.size(), 2u);
    const struct {
        const char* name;
        int count;
    } want[] = {{"netherite_scrap", 4}, {"gold_ingot", 4}};
    for (const auto& w : want) {
        bool found = false;
        for (const auto& [item, n] : r->ingredients) {
            if (c.items.at(item).name == w.name && n == w.count) {
                found = true;
            }
        }
        EXPECT_TRUE(found) << "missing " << w.name << " x" << w.count;
    }
}

TEST(V34NetheriteMaterialChain, ToolRecipesPinned) {
    // The iron tier's shapes cloned (the documented deviation 2: no
    // diamond tier -> direct crafting, the key = netherite_ingot).
    RealChain c;
    const struct {
        const char* id;
        std::vector<std::string> pattern;
    } want[] = {
        {"netherite_pickaxe", {"III", " S ", " S "}},
        {"netherite_axe", {"II", "IX", " X"}},
        {"netherite_shovel", {"M", "S", "S"}},
    };
    for (const auto& w : want) {
        SCOPED_TRACE(w.id);
        const Recipe* r = c.recipes.find(w.id);
        ASSERT_NE(r, nullptr) << w.id;
        EXPECT_EQ(r->type, RecipeType::Shaped) << w.id;
        EXPECT_EQ(r->output, c.items.idForName(w.id)) << w.id;
        EXPECT_EQ(r->outputCount, 1) << w.id;
        EXPECT_EQ(r->pattern, w.pattern) << w.id;
        ASSERT_EQ(r->key.size(), 2u) << w.id;
        bool ingot = false, stick = false;
        for (const auto& [ch, item] : r->key) {
            const std::string n = c.items.at(item).name;
            ingot = ingot || (n == "netherite_ingot");
            stick = stick || (n == "stick");
        }
        EXPECT_TRUE(ingot) << w.id << " key lost its netherite_ingot";
        EXPECT_TRUE(stick) << w.id << " key lost its stick";
    }
}

TEST(V34NetheriteMaterialChain, UpgradeRecipesPinned) {
    // The four shapeless upgrades (the documented deviation 3: no
    // smithing table — the ingot + the DIAMOND piece, the tier above).
    RealChain c;
    const char* slots[] = {"helmet", "chestplate", "leggings", "boots"};
    for (const char* s : slots) {
        SCOPED_TRACE(s);
        const std::string id = std::string("netherite_") + s;
        const std::string dia = std::string("diamond_") + s;
        const Recipe* r = c.recipes.find(id);
        ASSERT_NE(r, nullptr) << id;
        EXPECT_EQ(r->type, RecipeType::Shapeless) << id;
        EXPECT_EQ(r->output, c.items.idForName(id)) << id;
        EXPECT_EQ(r->outputCount, 1) << id;
        // The mutual-exclusion face: EXACTLY the ingot + the diamond
        // piece (two entries, one each — no iron / leather / netherite
        // input, no double-ingot form).
        ASSERT_EQ(r->ingredients.size(), 2u) << id;
        const struct {
            const char* name;
            int count;
        } want[2] = {{"netherite_ingot", 1}, {dia.c_str(), 1}};
        for (int i = 0; i < 2; ++i) {
            bool found = false;
            for (const auto& [item, n] : r->ingredients) {
                if (c.items.at(item).name == want[i].name && n == want[i].count) {
                    found = true;
                }
            }
            EXPECT_TRUE(found) << id << " missing " << want[i].name;
        }
    }
}

TEST(V34NetheriteMaterialChain, RecipeCensusPinned) {
    // The recipe registry's count face (87 pre-V3-4 + 8 = 95; V3-5
    // building family +17 — the 3 masonry recipes + the 14 dyed-
    // terracotta shapeless recipes; the two smelts are the data-side
    // smeltable_to fields, not recipe files): a new recipe file is a
    // conscious census update.
    RealChain c;
    EXPECT_EQ(c.recipes.count(), 112u);
}

// ===========================================================================
// The craft behavior (the simulateCraft/craft pure face over real data)
// ===========================================================================

TEST(V34NetheriteMaterialChain, IngotCraftBehavior) {
    RealChain c;
    const ItemID scrap = c.items.idForName("netherite_scrap");
    const ItemID gold = c.items.idForName("gold_ingot");
    const ItemID ingot = c.items.idForName("netherite_ingot");
    const Recipe* r = c.recipes.find("netherite_ingot");
    ASSERT_NE(r, nullptr);
    // The full bag (scattered, gold slots first — the order-
    // independence's shapeless form): canCraft + one craft.
    Inventory inv;
    inv.setSlot(0, stk(gold, 2));
    inv.setSlot(3, stk(scrap, 2));
    inv.setSlot(5, stk(gold, 1));
    inv.setSlot(7, stk(scrap, 1));
    inv.setSlot(9, stk(gold, 1));
    inv.setSlot(12, stk(scrap, 1));
    EXPECT_TRUE(canCraft(inv, *r, c.items));
    auto cr = craft(inv, *r, c.items);
    ASSERT_TRUE(cr.ok()) << cr.error().message;
    int got = 0, goldLeft = 0, scrapLeft = 0;
    for (std::size_t i = 0; i < inv.size(); ++i) {
        if (inv.at(i).item == ingot) got += inv.at(i).count;
        if (inv.at(i).item == gold) goldLeft += inv.at(i).count;
        if (inv.at(i).item == scrap) scrapLeft += inv.at(i).count;
    }
    EXPECT_EQ(got, 1);
    EXPECT_EQ(goldLeft, 0);
    EXPECT_EQ(scrapLeft, 0);
    // One unit short on either side: no craft.
    Inventory shortScrap;
    shortScrap.setSlot(0, stk(gold, 4));
    shortScrap.setSlot(1, stk(scrap, 3));
    EXPECT_FALSE(canCraft(shortScrap, *r, c.items));
    Inventory shortGold;
    shortGold.setSlot(0, stk(gold, 3));
    shortGold.setSlot(1, stk(scrap, 4));
    EXPECT_FALSE(canCraft(shortGold, *r, c.items));
    // Surplus: exactly one craft consumes exactly the bag (the
    // simulateCraft's per-unit form — the output is count 1).
    Inventory surplus;
    surplus.setSlot(0, stk(gold, 8));
    surplus.setSlot(1, stk(scrap, 8));
    ASSERT_TRUE(craft(surplus, *r, c.items).ok());
    goldLeft = 0;
    scrapLeft = 0;
    got = 0;
    for (std::size_t i = 0; i < surplus.size(); ++i) {
        if (surplus.at(i).item == ingot) got += surplus.at(i).count;
        if (surplus.at(i).item == gold) goldLeft += surplus.at(i).count;
        if (surplus.at(i).item == scrap) scrapLeft += surplus.at(i).count;
    }
    EXPECT_EQ(got, 1);
    EXPECT_EQ(goldLeft, 4);
    EXPECT_EQ(scrapLeft, 4);
}

TEST(V34NetheriteMaterialChain, UpgradeCraftBehavior) {
    RealChain c;
    const ItemID ingot = c.items.idForName("netherite_ingot");
    const ItemID diaHelmet = c.items.idForName("diamond_helmet");
    const ItemID ironHelmet = c.items.idForName("iron_helmet");
    const ItemID netHelmet = c.items.idForName("netherite_helmet");
    const ItemID diaChest = c.items.idForName("diamond_chestplate");
    const Recipe* r = c.recipes.find("netherite_helmet");
    ASSERT_NE(r, nullptr);
    // The diamond tier upgrades; the iron tier does NOT (the
    // cross-slot / cross-tier exclusion's behavior face).
    Inventory good;
    good.setSlot(0, stk(ingot, 1));
    good.setSlot(4, stk(diaHelmet, 1));
    EXPECT_TRUE(canCraft(good, *r, c.items));
    auto cr = craft(good, *r, c.items);
    ASSERT_TRUE(cr.ok()) << cr.error().message;
    bool made = false, ingotGone = true, diaGone = true;
    for (std::size_t i = 0; i < good.size(); ++i) {
        if (good.at(i).item == netHelmet) made = true;
        if (good.at(i).item == ingot) ingotGone = false;
        if (good.at(i).item == diaHelmet) diaGone = false;
    }
    EXPECT_TRUE(made);
    EXPECT_TRUE(ingotGone);
    EXPECT_TRUE(diaGone);
    Inventory iron;
    iron.setSlot(0, stk(ingot, 1));
    iron.setSlot(1, stk(ironHelmet, 1));
    EXPECT_FALSE(canCraft(iron, *r, c.items));
    // The cross-slot form: the chestplate's ingredients do not craft
    // the helmet (the diamond input is slot-specific).
    Inventory wrongSlot;
    wrongSlot.setSlot(0, stk(ingot, 1));
    wrongSlot.setSlot(1, stk(diaChest, 1));
    EXPECT_FALSE(canCraft(wrongSlot, *r, c.items));
    EXPECT_TRUE(
        canCraft(wrongSlot, *c.recipes.find("netherite_chestplate"),
                 c.items));
}

TEST(V34NetheriteMaterialChain, ToolCraftGridBehavior) {
    // The matcher face (the crafting-table UI's entry point): the
    // pickaxe pattern at the grid's origin matches the shaped recipe
    // (3 ingots top row + 2 sticks), and the same BAG scattered does
    // NOT (the shape is load-bearing — the shaped vs shapeless pin).
    RealChain c;
    const ItemID ingot = c.items.idForName("netherite_ingot");
    const ItemID stick = c.items.idForName("stick");
    const Recipe* want = c.recipes.find("netherite_pickaxe");
    ASSERT_NE(want, nullptr);
    CraftingGrid grid;
    grid.setSlot(0, stk(ingot, 1));
    grid.setSlot(1, stk(ingot, 1));
    grid.setSlot(2, stk(ingot, 1));
    grid.setSlot(4, stk(stick, 1));
    grid.setSlot(7, stk(stick, 1));
    GridMatch m = matchGridRecipe(grid, c.recipes);
    ASSERT_TRUE(m.matched());
    EXPECT_EQ(m.recipe, want);
    EXPECT_EQ(m.dx, 0);
    EXPECT_EQ(m.dy, 0);
    // The scattered form (no 3+2 L anywhere): the pickaxe's pattern
    // does not match (matchesShaped's direct face).
    CraftingGrid scattered;
    scattered.setSlot(0, stk(ingot, 1));
    scattered.setSlot(1, stk(ingot, 1));
    scattered.setSlot(3, stk(ingot, 1));
    scattered.setSlot(6, stk(stick, 1));
    scattered.setSlot(7, stk(stick, 1));
    int dx = -1, dy = -1;
    EXPECT_FALSE(matchesShaped(scattered, *want, &dx, &dy));
    // The grid with the wrong material (iron ingots) crafts the iron
    // pickaxe, NOT the netherite one (the key's item identity).
    const ItemID iron = c.items.idForName("iron_ingot");
    CraftingGrid ironGrid;
    ironGrid.setSlot(0, stk(iron, 1));
    ironGrid.setSlot(1, stk(iron, 1));
    ironGrid.setSlot(2, stk(iron, 1));
    ironGrid.setSlot(4, stk(stick, 1));
    ironGrid.setSlot(7, stk(stick, 1));
    GridMatch mIron = matchGridRecipe(ironGrid, c.recipes);
    ASSERT_TRUE(mIron.matched());
    EXPECT_EQ(mIron.recipe, c.recipes.find("iron_pickaxe"));
    EXPECT_NE(mIron.recipe, want);
}

// ===========================================================================
// The smelt chain over the pure tick (the CRAFT-6 RealAssetsInherit +
// RealTick precedent, the ancient_debris block item's new inheritance)
// ===========================================================================

TEST(V34NetheriteMaterialChain, SmeltTickDebrisToScrap) {
    // The chain's first stage end-to-end over the shipped data: the
    // ancient_debris BLOCK item (the smeltable_to's inheritance) in a
    // coal-fueled furnace completes on the 600th substep (the 10 s
    // default) with one netherite_scrap — the block items flow the
    // SAME tick path as explicit items (the sand -> glass precedent).
    RealChain c;
    const ItemID debris = c.items.idForName("ancient_debris");
    const ItemID coal = c.items.idForName("coal");
    const ItemID scrap = c.items.idForName("netherite_scrap");
    FurnaceState st;
    st.input = stk(debris, 1);
    st.fuel = stk(coal, 1);
    tickFurnace(st, c.items);
    ASSERT_TRUE(st.fuel.empty()); // the coal was consumed as fuel
    EXPECT_EQ(st.fuelSubsteps, 4800); // the 80 s coal (the V2-7 value)
    EXPECT_EQ(st.progressSubsteps, 1);
    for (int i = 1; i < 600; ++i) {
        tickFurnace(st, c.items);
    }
    EXPECT_EQ(st.output, stk(scrap, 1));
    EXPECT_TRUE(st.input.empty());
    EXPECT_EQ(st.progressSubsteps, 0);
    EXPECT_GT(st.fuelSubsteps, 0); // 4800 - 600: still burning
    // The regression twin: the netherrack -> nether_brick's tick face
    // (the V3-2 Part 1's smelt, unchanged).
    const ItemID rack = c.items.idForName("netherrack");
    const ItemID brick = c.items.idForName("nether_brick");
    FurnaceState st2;
    st2.input = stk(rack, 1);
    st2.fuel = stk(coal, 1);
    for (int i = 0; i < 600; ++i) {
        tickFurnace(st2, c.items);
    }
    EXPECT_EQ(st2.output, stk(brick, 1));
    EXPECT_TRUE(st2.input.empty());
}

TEST(V34NetheriteMaterialChain, SmeltThenCraftEndToEnd) {
    // The full chain the player walks: smelt 4 debris -> 4 scrap ->
    // craft 4 scrap + 4 gold -> 1 ingot (the two stages' unit-level
    // end-to-end, the g183 no-input smelt arm's data twin).
    RealChain c;
    const ItemID debris = c.items.idForName("ancient_debris");
    const ItemID coal = c.items.idForName("coal");
    const ItemID scrap = c.items.idForName("netherite_scrap");
    const ItemID gold = c.items.idForName("gold_ingot");
    const ItemID ingot = c.items.idForName("netherite_ingot");
    // Stage 1: four smelts on one coal (4 x 600 = 2400 < 4800).
    FurnaceState st;
    st.input = stk(debris, 4);
    st.fuel = stk(coal, 1);
    for (int i = 0; i < 2400; ++i) {
        tickFurnace(st, c.items);
    }
    EXPECT_EQ(st.output, stk(scrap, 4));
    EXPECT_TRUE(st.input.empty());
    // Stage 2: the smelted scrap + 4 gold into the crafting inventory.
    Inventory inv;
    inv.setSlot(0, st.output);
    for (int i = 0; i < 4; ++i) {
        inv.setSlot(static_cast<std::size_t>(10 + i), stk(gold, 1));
    }
    const Recipe* r = c.recipes.find("netherite_ingot");
    ASSERT_NE(r, nullptr);
    ASSERT_TRUE(craft(inv, *r, c.items).ok());
    int made = 0, left = 0;
    for (std::size_t i = 0; i < inv.size(); ++i) {
        if (inv.at(i).item == ingot) made += inv.at(i).count;
        if (inv.at(i).item == scrap || inv.at(i).item == gold) {
            left += inv.at(i).count;
        }
    }
    EXPECT_EQ(made, 1);
    EXPECT_EQ(left, 0);
}

// ===========================================================================
// The S6 rejection faces (the parse-level named rejections over
// netherite-shaped definitions — the ship's data passes every one)
// ===========================================================================

TEST(V34NetheriteMaterialChain, RejectionDurabilityZero) {
    // An explicit durability 0 is a typo (the "absent = not
    // durable" default is the only no-durability form — the
    // parseDurability's > 0 rule, both tier shapes).
    ItemRegistry reg;
    auto badTool = reg.registerItem(parseDef(
        R"({"id":"netherite_pickaxe","type":"tool","durability":0,
            "repair_with":"netherite_ingot",
            "tool":{"break_speed":4.0}})"));
    EXPECT_FALSE(badTool.ok());
    EXPECT_NE(badTool.error().message.find("durability"),
              std::string::npos);
    auto badArmor = reg.registerItem(parseDef(
        R"({"id":"netherite_helmet","type":"material","durability":0,
            "armor_slot":"head","armor_points":3,
            "repair_with":"netherite_ingot"})"));
    EXPECT_FALSE(badArmor.ok());
    EXPECT_NE(badArmor.error().message.find("durability"),
              std::string::npos);
}

TEST(V34NetheriteMaterialChain, RejectionEnchantClassMismatch) {
    // The enchant pairing's faces over the netherite shapes: the
    // unknown class name (a "netherite" class is a material-name
    // typo — the vocabulary is sword/axe/pick/shovel/armor/bow/hoe)
    // and the class-without-ability half-pair (the strict pair).
    ItemRegistry reg;
    auto unknown = reg.registerItem(parseDef(
        R"({"id":"netherite_pickaxe","type":"tool",
            "enchant_class":"netherite","enchantability":14,
            "tool":{"break_speed":4.0}})"));
    EXPECT_FALSE(unknown.ok());
    EXPECT_NE(unknown.error().message.find("unknown class"),
              std::string::npos);
    auto halfPair = reg.registerItem(parseDef(
        R"({"id":"netherite_helmet","type":"material",
            "enchant_class":"armor",
            "armor_slot":"head","armor_points":3,"durability":545})"));
    EXPECT_FALSE(halfPair.ok());
    EXPECT_NE(halfPair.error().message.find("strict pair"),
              std::string::npos);
}

TEST(V34NetheriteMaterialChain, RejectionUpgradeRecipeUnknownIngot) {
    // The upgrade recipe's ingot reference: a registry missing the
    // netherite_ingot (a renamed / dropped file) rejects the shipped
    // recipe BY NAME (the unknown-ingredient's init face — the typo
    // must fail loudly, not silently craft nothing).
    AssetManager assets;
    realAssets(assets);
    BlockRegistry blocks;
    ASSERT_TRUE(blocks.loadDirectory(assets, "data/blocks").ok());
    ItemRegistry items;
    ASSERT_TRUE(items.buildBlockItems(blocks).ok());
    // The real items minus the netherite_ingot (the missing-file form).
    std::vector<std::filesystem::path> files;
    for (const auto& e :
         std::filesystem::directory_iterator("assets/data/items")) {
        if (e.is_regular_file() && e.path().extension() == ".json" &&
            e.path().stem() != "netherite_ingot") {
            files.push_back(e.path());
        }
    }
    std::sort(files.begin(), files.end());
    for (const auto& f : files) {
        // The AssetManager's paths are relative to its root ("assets/").
        const std::string rel =
            f.generic_string().substr(std::string("assets/").size());
        auto t = assets.loadText(rel);
        ASSERT_TRUE(t.ok()) << rel;
        auto p = Json::parse(*t);
        ASSERT_TRUE(p.ok());
        auto r = items.registerItem(*p);
        ASSERT_TRUE(r.ok()) << f.generic_string() << ": "
                            << r.error().message;
    }
    ASSERT_TRUE(items.findByName("netherite_helmet") != nullptr);
    ASSERT_TRUE(items.findByName("netherite_ingot") == nullptr);
    auto recipeText = assets.loadText("data/recipes/netherite_helmet.json");
    ASSERT_TRUE(recipeText.ok());
    auto recipeDef = Json::parse(*recipeText);
    ASSERT_TRUE(recipeDef.ok());
    RecipeRegistry recipes;
    auto rejected = recipes.registerRecipe(*recipeDef, items);
    EXPECT_FALSE(rejected.ok());
    EXPECT_NE(rejected.error().message.find("netherite_ingot"),
              std::string::npos);
}

TEST(V34NetheriteMaterialChain, RejectionFastAgainstBadEntry) {
    // The fast_against entry discipline (the per-index named rejection
    // — the non-string / empty-string typos; the unknown-block-name
    // form is the init cross-check, pinned resolved over the real
    // registry in FastAgainstBlocksResolve).
    ItemRegistry reg;
    auto nonString = reg.registerItem(parseDef(
        R"({"id":"netherite_pickaxe","type":"tool",
            "fast_against":["netherrack", 7],
            "fast_against_multiplier":2.0,
            "tool":{"break_speed":4.0}})"));
    EXPECT_FALSE(nonString.ok());
    EXPECT_NE(nonString.error().message.find("fast_against[1]"),
              std::string::npos);
    auto empty = reg.registerItem(parseDef(
        R"({"id":"netherite_pickaxe","type":"tool",
            "fast_against":[""],
            "fast_against_multiplier":2.0,
            "tool":{"break_speed":4.0}})"));
    EXPECT_FALSE(empty.ok());
    EXPECT_NE(empty.error().message.find("fast_against[0]"),
              std::string::npos);
    // The pair's half form (the multiplier without the list).
    auto noList = reg.registerItem(parseDef(
        R"({"id":"netherite_pickaxe","type":"tool",
            "fast_against_multiplier":2.0,
            "tool":{"break_speed":4.0}})"));
    EXPECT_FALSE(noList.ok());
    EXPECT_NE(noList.error().message.find("strict pair"),
              std::string::npos);
}
