// V3-5 (v3.9.0): the building content family — the clay chain (the
// under-water sediment's 4x clay_ball drop, the clay_ball -> brick smelt,
// the 2x2 bricks recipe, the bricks -> terracotta smelt), the stone
// masonry (stone_bricks / mossy_stone_bricks), and the 15-color dyed
// terracotta family (the Phase 41 base + 14 variants — the project's dye
// vocabulary, the wool / bed's exact set: no lime, the documented
// deviation; the design's "16 colors" is the Minecraft set, of which the
// project ships 15 including white). Content is data: the block / item /
// recipe JSONs, the 20 tiles + 2 icons (deterministic generator re-run),
// the atlas layout's 20 free cells, and the worldgen sediment pass
// (test_world_generator's SedimentTest family).
#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "block/block.hpp"
#include "block/block_registry.hpp"
#include "core/asset_manager.hpp"
#include "item/crafting_grid.hpp" // matchGridRecipe (the shaped face)
#include "item/furnace.hpp" // tickFurnace (the smelt chain's pure face)
#include "item/inventory.hpp"
#include "item/item.hpp"
#include "item/item_registry.hpp"
#include "item/recipe.hpp"

using namespace mc;

namespace {

std::string readAll(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    EXPECT_TRUE(in) << path;
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// The project FNV-1a 64 pin (the netherite IconCensusPinned convention —
// the generator re-run's byte determinism proof).
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

// The project's dye vocabulary (the wool / bed's exact 15-color set —
// white is the base member, no lime: the documented family).
const std::vector<std::string>& terracottaColors() {
    static const std::vector<std::string> colors {
        "white", "black", "blue", "brown", "cyan", "gray", "green",
        "light_blue", "magenta", "orange", "pink", "purple", "red",
        "silver", "yellow"};
    return colors;
}

std::string terracottaName(const std::string& color) {
    return color == "white" ? std::string("terracotta")
                            : "terracotta_" + color;
}

} // namespace

// ===========================================================================
// The six base blocks (the content pins)
// ===========================================================================

TEST(V390BuildingContent, ClayContentPinned) {
    // The under-water sediment's deposit: MC-exact hardness 0.6, the
    // 4x clay_ball break drop (the chain's start), stone sounds, an
    // opaque full cube (the light_opacity key ABSENT — it derives 15,
    // the replaced sediment's exact attenuation: the design's "same-
    // opacity = zero light change" strongest form).
    RealChain c;
    const Block& b = c.blocks.at(c.blocks.idForName("clay"));
    EXPECT_FLOAT_EQ(b.properties.hardness, 0.6f);
    EXPECT_EQ(b.properties.drops, "clay_ball");
    EXPECT_EQ(b.properties.dropsCount, 4);
    EXPECT_EQ(b.properties.soundMaterial, "stone");
    EXPECT_TRUE(b.properties.opaque);
    EXPECT_EQ(b.properties.lightOpacity, 15); // derived, same as dirt et al
    EXPECT_EQ(b.model.shape, BlockModel::Shape::FullCube);
    EXPECT_EQ(b.model.textures.top, "clay");
    EXPECT_EQ(b.model.textures.side, "clay");
    EXPECT_EQ(b.model.textures.bottom, "clay");
    // No smelt on the block (the clay_ball item carries the smelt).
    EXPECT_TRUE(b.properties.smeltableTo.empty());
}

TEST(V390BuildingContent, MasonryBlocksContentPinned) {
    // The four masonry blocks: hardness MC-exact (2.0 / 1.5 / 1.5),
    // stone sounds, self drops, opaque full cubes.
    const struct {
        const char* name;
        float hardness;
        const char* smeltTo; // "" = no smeltable_to
    } want[] = {
        {"mossy_cobblestone", 2.0f, ""},
        {"bricks", 2.0f, "terracotta"},
        {"stone_bricks", 1.5f, ""},
        {"mossy_stone_bricks", 1.5f, ""},
    };
    RealChain c;
    for (const auto& w : want) {
        SCOPED_TRACE(w.name);
        const Block& b = c.blocks.at(c.blocks.idForName(w.name));
        EXPECT_FLOAT_EQ(b.properties.hardness, w.hardness);
        EXPECT_EQ(b.properties.soundMaterial, "stone");
        EXPECT_EQ(b.properties.drops, w.name);
        EXPECT_EQ(b.properties.dropsCount, 1);
        EXPECT_EQ(b.properties.smeltableTo, w.smeltTo);
        EXPECT_EQ(b.model.shape, BlockModel::Shape::FullCube);
        EXPECT_EQ(b.model.textures.top, w.name);
    }
    // The bricks -> terracotta smelt is the block-side data (the
    // netherrack -> nether_brick CRAFT-6 inheritance: the brick BLOCK
    // item smelts to the terracotta BLOCK item — no recipe file).
    const Item& brickItem =
        c.items.at(c.items.idForName("bricks"));
    EXPECT_EQ(brickItem.smeltableTo, "terracotta");
    EXPECT_EQ(brickItem.block, c.blocks.idForName("bricks"));
}

// ===========================================================================
// The dyed terracotta family (the Phase 41 completeness, the bed twin)
// ===========================================================================

TEST(V390BuildingContent, FifteenBlocksAreTheDyeFamily) {
    // The base (the white member, no suffix — the Phase 41 naming
    // discipline) + the 14 non-white variants: each carries the family
    // pair, the MC-exact hardness 1.25, stone sounds, the self drop, and
    // its own tile (the atlas's 15 terracotta cells).
    RealChain c;
    for (const std::string& color : terracottaColors()) {
        const std::string name = terracottaName(color);
        const Block* b = c.blocks.findByName(name);
        ASSERT_TRUE(b != nullptr) << name;
        EXPECT_EQ(b->properties.dyeBase, "terracotta") << name;
        EXPECT_EQ(b->properties.dyeColor, color) << name;
        EXPECT_FLOAT_EQ(b->properties.hardness, 1.25f) << name;
        EXPECT_EQ(b->properties.soundMaterial, "stone") << name;
        EXPECT_EQ(b->properties.drops, name) << name;
        EXPECT_EQ(b->properties.dropsCount, 1) << name;
        EXPECT_TRUE(b->properties.interact.empty()) << name;
        EXPECT_EQ(b->model.textures.top, name) << name;
        EXPECT_EQ(b->model.textures.side, name) << name;
        EXPECT_EQ(b->model.textures.bottom, name) << name;
    }
    // The base IS the white member (the dyeVariantName convention, the
    // bed test's pin mirrored): no "terracotta_white" block exists.
    EXPECT_EQ(dyeVariantName("terracotta", kDyeColorWhite), "terracotta");
    EXPECT_EQ(c.blocks.findByName("terracotta_white"), nullptr);
}

TEST(V390BuildingContent, FamilyCoversTheFullDyeVocabulary) {
    // The init cross-check's data-level twin (the bed test's pin
    // mirrored): for every dye ITEM color (the registry's vocabulary),
    // the terracotta family has its variant block (a missing one would
    // be a dead dye — the Phase 41 completeness rule the game init
    // enforces over every family).
    RealChain c;
    std::size_t colors = 0;
    for (const ItemID id : c.items.allIds()) {
        if (c.items.at(id).dyeColor.empty()) {
            continue;
        }
        ++colors;
        const std::string variant =
            dyeVariantName("terracotta", c.items.at(id).dyeColor);
        EXPECT_TRUE(c.blocks.findByName(variant) != nullptr) << variant;
    }
    EXPECT_EQ(colors, terracottaColors().size());
}

// ===========================================================================
// The two material items (the clay chain's middle)
// ===========================================================================

TEST(V390BuildingContent, ClayBallAndBrickItemsPinned) {
    // The clay ball: the clay block's 4x drop target, stack 64, the
    // item-side smelt to brick (the CRAFT-6 item smeltable_to, the
    // netherite_scrap's model twin) + the single-box model (the
    // quartz / netherite_scrap silhouette convention). The brick: the
    // smelt product + the 2x2 bricks recipe's key, stack 64, no smelt.
    RealChain c;
    const Item& ball = c.items.at(c.items.idForName("clay_ball"));
    EXPECT_EQ(ball.type, ItemType::Material);
    EXPECT_EQ(ball.maxStack, 64);
    EXPECT_EQ(ball.smeltableTo, "brick");
    EXPECT_EQ(ball.icon, "clay_ball");
    ASSERT_TRUE(ball.model.has_value());
    EXPECT_EQ(ball.model->texture, "clay_ball");
    ASSERT_EQ(ball.model->elements.size(), 1u);
    EXPECT_EQ(ball.model->elements[0].from, (Vec3 {0.3f, 0.3f, 0.3f}));
    EXPECT_EQ(ball.model->elements[0].to, (Vec3 {0.7f, 0.7f, 0.7f}));
    // The clay's break drop resolves to THIS item (the init cross-
    // check's data side: the drops name exists, 4 fits the stack 64).
    const Block& clay = c.blocks.at(c.blocks.idForName("clay"));
    EXPECT_EQ(c.items.idForName(clay.properties.drops),
              c.items.idForName("clay_ball"));
    EXPECT_LE(clay.properties.dropsCount, ball.maxStack);

    const Item& brick = c.items.at(c.items.idForName("brick"));
    EXPECT_EQ(brick.type, ItemType::Material);
    EXPECT_EQ(brick.maxStack, 64);
    EXPECT_TRUE(brick.smeltableTo.empty());
    EXPECT_EQ(brick.icon, "brick");
    ASSERT_TRUE(brick.model.has_value());
    EXPECT_EQ(brick.model->texture, "brick");
    ASSERT_EQ(brick.model->elements.size(), 1u);
}

// ===========================================================================
// The 17 recipes (3 masonry + 14 dyed terracotta)
// ===========================================================================

TEST(V390BuildingContent, MasonryRecipesPinned) {
    RealChain c;
    // bricks: the 2x2 brick grid (MC exact).
    const Recipe* bricks = c.recipes.find("bricks");
    ASSERT_NE(bricks, nullptr);
    EXPECT_EQ(bricks->type, RecipeType::Shaped);
    EXPECT_EQ(bricks->output, c.items.idForName("bricks"));
    EXPECT_EQ(bricks->outputCount, 1);
    EXPECT_EQ(bricks->pattern, (std::vector<std::string> {"BB", "BB"}));
    ASSERT_EQ(bricks->key.size(), 1u);
    EXPECT_EQ(bricks->key.at('B'), c.items.idForName("brick"));

    // stone_bricks: the 2x2 cobblestone grid (MC exact).
    const Recipe* sbricks = c.recipes.find("stone_bricks");
    ASSERT_NE(sbricks, nullptr);
    EXPECT_EQ(sbricks->type, RecipeType::Shaped);
    EXPECT_EQ(sbricks->output, c.items.idForName("stone_bricks"));
    EXPECT_EQ(sbricks->outputCount, 1);
    EXPECT_EQ(sbricks->pattern,
              (std::vector<std::string> {"CC", "CC"}));
    ASSERT_EQ(sbricks->key.size(), 1u);
    EXPECT_EQ(sbricks->key.at('C'), c.items.idForName("cobblestone"));

    // mossy_stone_bricks: the shapeless 3+1 material set (the recorded
    // deviation 2 — the MC 2x2+center pattern's content-equivalent).
    const Recipe* mossy = c.recipes.find("mossy_stone_bricks");
    ASSERT_NE(mossy, nullptr);
    EXPECT_EQ(mossy->type, RecipeType::Shapeless);
    EXPECT_EQ(mossy->output, c.items.idForName("mossy_stone_bricks"));
    EXPECT_EQ(mossy->outputCount, 1);
    ASSERT_EQ(mossy->ingredients.size(), 2u);
    const struct {
        const char* name;
        int count;
    } want[] = {{"stone_bricks", 3}, {"mossy_cobblestone", 1}};
    for (const auto& w : want) {
        bool found = false;
        for (const auto& [item, n] : mossy->ingredients) {
            if (c.items.at(item).name == w.name && n == w.count) {
                found = true;
            }
        }
        EXPECT_TRUE(found) << "missing " << w.name << " x" << w.count;
    }
}

TEST(V390BuildingContent, DyedTerracottaRecipesPinned) {
    // The 14 shapeless dyes (the generic dye + base block mechanism —
    // the recorded deviation 3's "MC-identical" face: dye + terracotta
    // -> the colored member; the dyed block has no second dye, MC
    // twin).
    RealChain c;
    int checked = 0;
    for (const std::string& color : terracottaColors()) {
        if (color == "white") {
            continue; // the base has no dye recipe (it IS white)
        }
        const std::string name = terracottaName(color);
        const Recipe* r = c.recipes.find(name);
        ASSERT_NE(r, nullptr) << name;
        EXPECT_EQ(r->type, RecipeType::Shapeless) << name;
        EXPECT_EQ(r->output, c.items.idForName(name)) << name;
        EXPECT_EQ(r->outputCount, 1) << name;
        ASSERT_EQ(r->ingredients.size(), 2u) << name;
        bool dye = false, base = false;
        for (const auto& [item, n] : r->ingredients) {
            const std::string iname = c.items.at(item).name;
            if (iname == "dye_" + color && n == 1) {
                dye = true;
            }
            if (iname == "terracotta" && n == 1) {
                base = true;
            }
        }
        EXPECT_TRUE(dye) << name << " lost its dye ingredient";
        EXPECT_TRUE(base) << name << " lost its terracotta ingredient";
        ++checked;
    }
    EXPECT_EQ(checked, 14);
}

TEST(V390BuildingContent, RecipeCensusPinned) {
    // The recipe directory's count face (95 pre-V3-5 + 17 = 112): the
    // two smelts are data fields, not recipe files (the CRAFT-6
    // discipline).
    RealChain c;
    int recipeFiles = 0;
    for (const auto& e :
         std::filesystem::directory_iterator("assets/data/recipes")) {
        if (e.is_regular_file() && e.path().extension() == ".json") {
            ++recipeFiles;
        }
    }
    EXPECT_EQ(c.recipes.count(), 112u);
    EXPECT_EQ(recipeFiles, 112);
}

// ===========================================================================
// The smelt chain over the pure tick (the RealTick precedent)
// ===========================================================================

TEST(V390BuildingContent, SmeltTickClayBallToBrick) {
    // The chain's first stage: the clay_ball (item-side smeltable_to)
    // in a coal-fueled furnace completes on the 600th substep (the 10 s
    // default) with one brick.
    RealChain c;
    const ItemID ball = c.items.idForName("clay_ball");
    const ItemID coal = c.items.idForName("coal");
    const ItemID brick = c.items.idForName("brick");
    FurnaceState st;
    st.input = stk(ball, 1);
    st.fuel = stk(coal, 1);
    for (int i = 0; i < 600; ++i) {
        tickFurnace(st, c.items);
    }
    EXPECT_EQ(st.output, stk(brick, 1));
    EXPECT_TRUE(st.input.empty());
    EXPECT_EQ(st.progressSubsteps, 0);
}

TEST(V390BuildingContent, SmeltTickBricksToTerracotta) {
    // The chain's second stage: the bricks BLOCK item (the CRAFT-6
    // block-side smeltable_to inheritance — the netherrack ->
    // nether_brick tick face) completes to one terracotta.
    RealChain c;
    const ItemID bricks = c.items.idForName("bricks");
    const ItemID coal = c.items.idForName("coal");
    const ItemID terra = c.items.idForName("terracotta");
    FurnaceState st;
    st.input = stk(bricks, 1);
    st.fuel = stk(coal, 1);
    for (int i = 0; i < 600; ++i) {
        tickFurnace(st, c.items);
    }
    EXPECT_EQ(st.output, stk(terra, 1));
    EXPECT_TRUE(st.input.empty());
}

// ===========================================================================
// The craft behavior over the real chain (the simulateCraft face)
// ===========================================================================

TEST(V390BuildingContent, CraftBehavior) {
    RealChain c;
    const ItemID brick = c.items.idForName("brick");
    const ItemID cobble = c.items.idForName("cobblestone");
    const ItemID sbricks = c.items.idForName("stone_bricks");
    const ItemID mossyCob = c.items.idForName("mossy_cobblestone");
    const ItemID dyeRed = c.items.idForName("dye_red");
    const ItemID terra = c.items.idForName("terracotta");
    const ItemID terraRed = c.items.idForName("terracotta_red");

    // 4 brick in a 2x2 grid (slots 0, 1, 3, 4 — the top-left 2x2) ->
    // 1 bricks (the shaped match, the grid face — the netherite
    // ToolGridCraftBehavior precedent).
    {
        CraftingGrid grid;
        for (int i : {0, 1, 3, 4}) {
            grid.setSlot(i, stk(brick, 1));
        }
        GridMatch m = matchGridRecipe(grid, c.recipes);
        ASSERT_TRUE(m.matched()) << "the full 2x2 brick grid";
        EXPECT_EQ(m.recipe, c.recipes.find("bricks"));
    }
    // One brick short: the L of 3 bricks does not fill the 2x2.
    {
        CraftingGrid grid;
        grid.setSlot(0, stk(brick, 1));
        grid.setSlot(1, stk(brick, 1));
        grid.setSlot(3, stk(brick, 1));
        GridMatch m = matchGridRecipe(grid, c.recipes);
        EXPECT_FALSE(m.matched()) << "3 bricks cannot fill the 2x2";
    }
    // 4 cobblestone in a 2x2 grid -> 1 stone_bricks.
    {
        CraftingGrid grid;
        for (int i : {0, 1, 3, 4}) {
            grid.setSlot(i, stk(cobble, 1));
        }
        GridMatch m = matchGridRecipe(grid, c.recipes);
        ASSERT_TRUE(m.matched()) << "the full 2x2 cobblestone grid";
        EXPECT_EQ(m.recipe, c.recipes.find("stone_bricks"));
    }
    // 3 stone_bricks + 1 mossy_cobblestone -> 1 mossy_stone_bricks
    // (the shapeless bag face — canCraft/craft over the inventory).
    {
        Inventory inv;
        inv.setSlot(0, stk(sbricks, 3));
        inv.setSlot(5, stk(mossyCob, 1));
        const Recipe* r = c.recipes.find("mossy_stone_bricks");
        ASSERT_NE(r, nullptr);
        EXPECT_TRUE(canCraft(inv, *r, c.items));
        ASSERT_TRUE(craft(inv, *r, c.items).ok());
        int made = 0;
        for (std::size_t i = 0; i < inv.size(); ++i) {
            if (inv.at(i).item ==
                c.items.idForName("mossy_stone_bricks")) {
                made += inv.at(i).count;
            }
        }
        EXPECT_EQ(made, 1);
    }
    // dye_red + terracotta -> 1 terracotta_red (the generic dye recipe,
    // the shapeless bag face) + the missing-ingredient rejection.
    {
        Inventory inv;
        inv.setSlot(0, stk(dyeRed, 1));
        inv.setSlot(3, stk(terra, 1));
        const Recipe* r = c.recipes.find("terracotta_red");
        ASSERT_NE(r, nullptr);
        EXPECT_TRUE(canCraft(inv, *r, c.items));
        ASSERT_TRUE(craft(inv, *r, c.items).ok());
        int made = 0;
        for (std::size_t i = 0; i < inv.size(); ++i) {
            if (inv.at(i).item == terraRed) {
                made += inv.at(i).count;
            }
        }
        EXPECT_EQ(made, 1);
    }
    {
        Inventory inv;
        inv.setSlot(0, stk(dyeRed, 1)); // no terracotta: missing ingredient
        const Recipe* r = c.recipes.find("terracotta_red");
        EXPECT_FALSE(canCraft(inv, *r, c.items));
    }
}

// ===========================================================================
// The content census + the generator determinism (the FNV pins)
// ===========================================================================

TEST(V390BuildingContent, ContentCensusPinned) {
    // The directory census (a new file is a conscious update): blocks
    // 107 -> 127 (the 6 base + the 14 dyed terracotta), items 95 -> 97,
    // recipes 95 -> 112, the 20 new block tiles + 2 new icons on disk.
    int blockCount = 0;
    for (const auto& e :
         std::filesystem::directory_iterator("assets/data/blocks")) {
        if (e.is_regular_file() && e.path().extension() == ".json") {
            ++blockCount;
        }
    }
    // V3-10a re-pin: 127 -> 131 (the End's four minimum blocks —
    // endstone / end_portal / the two frames; the zero-shift red line).
    EXPECT_EQ(blockCount, 131);
    int itemJson = 0;
    for (const auto& e :
         std::filesystem::directory_iterator("assets/data/items")) {
        if (e.is_regular_file() && e.path().extension() == ".json") {
            ++itemJson;
        }
    }
    EXPECT_EQ(itemJson, 99); // + the V3-14b ghast_membrane + the V3-10a ender_eye
    int tilePng = 0;
    for (const auto& e :
         std::filesystem::directory_iterator("assets/textures/blocks")) {
        if (e.is_regular_file() && e.path().extension() == ".png") {
            ++tilePng;
        }
    }
    EXPECT_EQ(tilePng, 110 + 20 + 4); // 110 pre-V3-5 + the family's 20 + the V3-10a End's 4
    int iconPng = 0;
    for (const auto& e :
         std::filesystem::directory_iterator("assets/textures/items")) {
        if (e.is_regular_file() && e.path().extension() == ".png") {
            ++iconPng;
        }
    }
    EXPECT_EQ(iconPng, 95 + 2 + 1 + 1); // 95 pre-V3-5 + the family's 2 + the V3-14b membrane + the V3-10a ender_eye
}

TEST(V390BuildingContent, NewAssetsFNVpinned) {
    // The 20 new tiles + 2 new icons + 2 item JSONs byte-pinned
    // (generate_block_textures' salts 1065..1076 + the dyed blend,
    // generate_item_icons' salt block 180-183 — the re-run's zero-
    // existing-change proof is the gate's md5 check, the pork / netherite
    // precedent).
    const struct {
        const char* path;
        const char* fnv;
    } files[] = {
        {"assets/textures/blocks/clay.png", "280466b3d5d78fc7"},
        {"assets/textures/blocks/mossy_cobblestone.png", "d61340415c868843"},
        {"assets/textures/blocks/bricks.png", "5e8bdfc193a1f212"},
        {"assets/textures/blocks/stone_bricks.png", "83c2b427e40014a1"},
        {"assets/textures/blocks/mossy_stone_bricks.png", "0015c34b9e850b01"},
        {"assets/textures/blocks/terracotta.png", "91e6c6372771a8fe"},
        {"assets/textures/blocks/terracotta_black.png", "07bc2e63e1d53107"},
        {"assets/textures/blocks/terracotta_blue.png", "836ab9a51971df3d"},
        {"assets/textures/blocks/terracotta_brown.png", "0decf822e12a87d8"},
        {"assets/textures/blocks/terracotta_cyan.png", "1ece4501fe945833"},
        {"assets/textures/blocks/terracotta_gray.png", "5d9dbecd742db5fb"},
        {"assets/textures/blocks/terracotta_green.png", "b090892bfa4806ba"},
        {"assets/textures/blocks/terracotta_light_blue.png", "17595893900cdc57"},
        {"assets/textures/blocks/terracotta_magenta.png", "9541a1973d511744"},
        {"assets/textures/blocks/terracotta_orange.png", "5dfc33e05ac7271a"},
        {"assets/textures/blocks/terracotta_pink.png", "2cfb1409666ffba9"},
        {"assets/textures/blocks/terracotta_purple.png", "bb57805a90028cc6"},
        {"assets/textures/blocks/terracotta_red.png", "9db2096d7af25293"},
        {"assets/textures/blocks/terracotta_silver.png", "c0bac7414a46eb9c"},
        {"assets/textures/blocks/terracotta_yellow.png", "8556cc5c59dc82b6"},
        {"assets/textures/items/clay_ball.png", "95a76300dcd81c6e"},
        {"assets/textures/items/brick.png", "7d19aaef53152437"},
        {"assets/data/items/clay_ball.json", "9f7872e4efbdefa0"},
        {"assets/data/items/brick.json", "abb24970b40f6752"},
    };
    for (const auto& f : files) {
        EXPECT_EQ(fnv1a64(readAll(f.path)), f.fnv) << f.path;
    }
}
