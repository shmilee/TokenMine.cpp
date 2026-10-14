// Phase 71 campfire tests: the pure per-substep cooking tick (progress
// accrual / completion / finished-product idle / the stale-progress drop
// rules), the format-11 save round trip and validation (the bidirectional
// campfire cross-check, the progress invariant, the format-10 migration),
// the block registry's interact vocabulary, and the content pinning of the
// shipped campfire block + recipe (the Phase 69 pattern — the JSON values
// are the contract).
#include <gtest/gtest.h>

#include <cstdio>
#include <string>

#include "block/block_registry.hpp"
#include "core/json.hpp"
#include "core/save.hpp"
#include "item/campfire.hpp"
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

// The cooking item set: beef -> cooked (default 10 s = 600 substeps),
// stone (neither smeltable nor fuel).
struct ItemFixture {
    ItemRegistry items;

    ItemFixture() {
        const struct {
            const char* def;
        } defs[] = {
            {R"({"id":"beef","type":"material","smeltable_to":"cooked"})"},
            {R"({"id":"cooked","type":"material","max_stack":64})"},
            {R"({"id":"stone","type":"material","max_stack":64})"},
        };
        for (const auto& d : defs) {
            auto r = items.registerItem(parseJson(d.def));
            if (!r.ok()) {
                ADD_FAILURE() << d.def << ": " << r.error().message;
            }
        }
    }

    ItemID id(const char* name) const { return items.idForName(name); }
};

// ItemStack construction for EXPECT/ASSERT arguments (a brace-init inside
// a macro call would split on the comma).
ItemStack stk(ItemID id, int count) { return ItemStack {id, count}; }

// -- the pure per-substep tick --------------------------------------------

TEST(CampfireTest, TickAccruesOneSubstepPerCall) {
    ItemFixture f;
    CampfireCookState c;
    c.slot = stk(f.id("beef"), 1);
    tickCampfireCook(c, f.items);
    EXPECT_EQ(c.progressSubsteps, 1);
    tickCampfireCook(c, f.items);
    EXPECT_EQ(c.progressSubsteps, 2);
    EXPECT_EQ(c.slot.item, f.id("beef")); // still cooking
    EXPECT_EQ(c.slot.count, 1);
}

TEST(CampfireTest, TickCompletesAtSmeltSubsteps) {
    // beef: the default smelt_time 10 s = 600 substeps (the furnace pace —
    // smeltSubstepsFor is pinned in test_furnace.cpp).
    ItemFixture f;
    CampfireCookState c;
    c.slot = stk(f.id("beef"), 1);
    for (int i = 0; i < 599; ++i) {
        tickCampfireCook(c, f.items);
    }
    EXPECT_EQ(c.progressSubsteps, 599);
    EXPECT_EQ(c.slot.item, f.id("beef")); // not done at 599
    tickCampfireCook(c, f.items); // substep 600: done
    EXPECT_EQ(c.slot.item, f.id("cooked"));
    EXPECT_EQ(c.slot.count, 1);
    EXPECT_EQ(c.progressSubsteps, 0);
}

TEST(CampfireTest, TickFinishedProductIdles) {
    // The product is not smeltable: a finished slot idles forever (no
    // progress, no change).
    ItemFixture f;
    CampfireCookState c;
    c.slot = stk(f.id("cooked"), 1);
    for (int i = 0; i < 1200; ++i) {
        tickCampfireCook(c, f.items);
    }
    EXPECT_EQ(c.slot.item, f.id("cooked"));
    EXPECT_EQ(c.slot.count, 1);
    EXPECT_EQ(c.progressSubsteps, 0);
}

TEST(CampfireTest, TickDropsStaleProgress) {
    // An empty slot never carries progress (the save invariant — the tick
    // drops a stale count).
    ItemFixture f;
    CampfireCookState c;
    c.progressSubsteps = 7;
    tickCampfireCook(c, f.items);
    EXPECT_TRUE(c.empty());
    // A non-smeltable slot (placed by hand) the same way.
    c.slot = stk(f.id("stone"), 1);
    c.progressSubsteps = 3;
    tickCampfireCook(c, f.items);
    EXPECT_EQ(c.slot.item, f.id("stone"));
    EXPECT_EQ(c.progressSubsteps, 0);
}

// -- the block registry's interact vocabulary -------------------------------

TEST(CampfireTest, InteractCampfireIsAccepted) {
    BlockRegistry blocks;
    auto r = blocks.registerBlock(parseJson(
        R"({"id":"campfire","properties":{"interact":"campfire"},)"
        R"("model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(blocks.at(*r).properties.interact, "campfire");
}

TEST(CampfireTest, InteractUnknownValueIsRejected) {
    BlockRegistry blocks;
    auto r = blocks.registerBlock(parseJson(
        R"({"id":"campfirez","properties":{"interact":"campfirez"},)"
        R"("model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("campfirez"), std::string::npos)
        << r.error().message;
    EXPECT_NE(r.error().message.find("'campfire'"), std::string::npos)
        << "the vocabulary error must name the valid values: "
        << r.error().message;
}

// -- the format-11 save ------------------------------------------------------

// The save fixture: the campfire block (the block edits that reference it
// must be registered) + the items the minimal save below holds.
struct SaveFixture {
    BlockRegistry blocks;
    ItemRegistry items;
    EffectRegistry effects;

    SaveFixture() {
        blocks.registerBlock(parseJson(
            R"({"id":"stone","model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
        blocks.registerBlock(parseJson(
            R"({"id":"dirt","model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
        blocks.registerBlock(parseJson(
            R"({"id":"campfire","properties":{"solid":false,"opaque":false,)"
            R"("transparent":true,"interact":"campfire"},)"
            R"("model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
        auto r = items.buildBlockItems(blocks);
        if (!r.ok()) {
            ADD_FAILURE() << r.error().message;
        }
        const struct {
            const char* def;
        } itemDefs[] = {
            {R"({"id":"beef","type":"material","smeltable_to":"cooked"})"},
            {R"({"id":"cooked","type":"material","max_stack":64})"},
        };
        for (const auto& d : itemDefs) {
            auto ir = items.registerItem(parseJson(d.def));
            if (!ir.ok()) {
                ADD_FAILURE() << d.def << ": " << ir.error().message;
            }
        }
    }
};

// A complete, valid save (no campfire — the mutation tests add one).
SaveGame makeSave(const SaveFixture& f) {
    SaveGame s;
    s.seed = 42;
    s.time = 0.123456789;
    s.playerPos = {1.5f, 53.25f, -2.75f};
    s.playerYaw = 0.785f;
    s.playerPitch = -0.4f;
    s.playerAbsorption = 0.0f; // Phase 72 (no layer in the fixture)
    s.selectedSlot = 0;
    s.slots.resize(Inventory::kTotalSize);
    s.slots[0] = ItemStack {f.items.idForName("stone"), 64};
    s.blockEdits = {{BlockPos {0, 53, 0}, "air"},
                    {BlockPos {0, 54, -1}, "dirt"}};
    s.fluidEdits = {{BlockPos {4, 50, 4}, FluidKind::Water, 5}};
    return s;
}

// A save with ONE campfire block (at 2,53,2) holding the given cooking
// state.
SaveGame makeCampfireSave(const SaveFixture& f, const CampfireCookState& st) {
    SaveGame s = makeSave(f);
    s.blockEdits.push_back({BlockPos {2, 53, 2}, "campfire"});
    s.campfires.push_back({BlockPos {2, 53, 2}, st});
    return s;
}

// Copies a JSON object minus one key (Json has no erase).
Json withoutKey(const Json& root, const std::string& key) {
    Json out = Json::object();
    for (const auto& [k, v] : root.members()) {
        if (k != key) {
            out[k] = v;
        }
    }
    return out;
}

TEST(CampfireSaveTest, FormatIsTwelveAndRoundTrips) {
    SaveFixture f;
    CampfireCookState cooking;
    cooking.slot = stk(f.items.idForName("beef"), 1);
    cooking.progressSubsteps = 300;
    SaveGame s = makeCampfireSave(f, cooking);
    // A second, EMPTY campfire (the empty-state entries must round-trip).
    s.blockEdits.push_back({BlockPos {3, 53, 3}, "campfire"});
    s.campfires.push_back({BlockPos {3, 53, 3}, CampfireCookState {}});
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok()) << j.error().message;
    // Phase 72: writes are always format 12.
    EXPECT_EQ(j->find("format")->asInt(), 12);
    auto out = parseSave(*j, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    ASSERT_EQ(out->campfires.size(), 2u);
    const BlockPos p0 {2, 53, 2};
    const BlockPos p1 {3, 53, 3};
    EXPECT_EQ(out->campfires[0].pos, p0);
    EXPECT_EQ(out->campfires[0].state, cooking);
    EXPECT_EQ(out->campfires[1].pos, p1);
    EXPECT_TRUE(out->campfires[1].state.empty());
}

TEST(CampfireSaveTest, FormatTenLoadsWithoutCampfires) {
    // A format-10 file (the campfire block did not exist) loads with no
    // entries.
    SaveFixture f;
    SaveGame s = makeCampfireSave(f, CampfireCookState {});
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    Json root = *j;
    root["format"] = Json(10);
    root = withoutKey(root, "campfires"); // format 10 predates the campfire
    root["player"] = withoutKey(root["player"], "absorption"); // format 10 predates the absorption pool
    auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    EXPECT_TRUE(out->campfires.empty());
}

TEST(CampfireSaveTest, FormatTenCampfireEditMigratesEmpty) {
    // A hand-edited format-10 file with a campfire block edit: the parse
    // keeps the entry list EMPTY (the game adds the empty states on load,
    // like the empty furnace states).
    SaveFixture f;
    SaveGame s = makeSave(f);
    s.blockEdits.push_back({BlockPos {2, 53, 2}, "campfire"});
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    Json root = *j;
    root["format"] = Json(10);
    root = withoutKey(root, "campfires"); // format 10 predates the campfire
    root["player"] = withoutKey(root["player"], "absorption"); // format 10 predates the absorption pool
    auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    EXPECT_TRUE(out->campfires.empty());
}

TEST(CampfireSaveTest, RejectsCampfiresFieldInFormatTen) {
    SaveFixture f;
    SaveGame s = makeCampfireSave(f, CampfireCookState {});
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    Json root = *j;
    root["format"] = Json(10); // keeps the "campfires" field
    root["player"] = withoutKey(root["player"], "absorption"); // the rejection under test is "campfires"
    auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
    ASSERT_FALSE(out.ok());
    EXPECT_NE(out.error().message.find("format 11"), std::string::npos)
        << out.error().message;
}

TEST(CampfireSaveTest, RejectsEntryBlockMismatch) {
    // The bidirectional cross-check: an entry whose position holds no
    // campfire block, or a campfire block with no entry.
    SaveFixture f;
    // Entry without a block: drop the block edit, keep the entry.
    {
        SaveGame s = makeCampfireSave(f, CampfireCookState {});
        s.blockEdits.pop_back();
        auto j = serializeSave(s, f.items);
        ASSERT_TRUE(j.ok());
        auto out = parseSave(*j, f.items, f.blocks, f.effects, 42);
        ASSERT_FALSE(out.ok());
        EXPECT_NE(out.error().message.find("not a campfire block"),
                  std::string::npos)
            << out.error().message;
    }
    // Block without an entry: keep the block edit, drop the entry.
    {
        SaveGame s = makeCampfireSave(f, CampfireCookState {});
        s.campfires.clear();
        auto j = serializeSave(s, f.items);
        ASSERT_TRUE(j.ok());
        auto out = parseSave(*j, f.items, f.blocks, f.effects, 42);
        ASSERT_FALSE(out.ok());
        EXPECT_NE(out.error().message.find("no 'campfires' entry"),
                  std::string::npos)
            << out.error().message;
    }
}

TEST(CampfireSaveTest, RejectsProgressInvariantViolations) {
    // progress > 0 requires the slot to hold EXACTLY ONE smeltable item.
    SaveFixture f;
    // A non-smeltable item with progress.
    {
        SaveGame s = makeSave(f);
        s.blockEdits.push_back({BlockPos {2, 53, 2}, "campfire"});
        s.campfires.push_back(
            {BlockPos {2, 53, 2},
             CampfireCookState {stk(f.items.idForName("cooked"), 1), 5}});
        auto j = serializeSave(s, f.items);
        ASSERT_TRUE(j.ok());
        auto out = parseSave(*j, f.items, f.blocks, f.effects, 42);
        ASSERT_FALSE(out.ok());
        EXPECT_NE(out.error().message.find("no smeltable item"),
                  std::string::npos)
            << out.error().message;
    }
    // A smeltable item of the wrong count with progress.
    {
        SaveGame s = makeSave(f);
        s.blockEdits.push_back({BlockPos {2, 53, 2}, "campfire"});
        s.campfires.push_back(
            {BlockPos {2, 53, 2},
             CampfireCookState {stk(f.items.idForName("beef"), 2), 5}});
        auto j = serializeSave(s, f.items);
        ASSERT_TRUE(j.ok());
        auto out = parseSave(*j, f.items, f.blocks, f.effects, 42);
        ASSERT_FALSE(out.ok());
        EXPECT_NE(out.error().message.find("no smeltable item"),
                  std::string::npos)
            << out.error().message;
    }
    // An EMPTY slot with progress.
    {
        SaveGame s = makeSave(f);
        s.blockEdits.push_back({BlockPos {2, 53, 2}, "campfire"});
        s.campfires.push_back(
            {BlockPos {2, 53, 2}, CampfireCookState {ItemStack {}, 5}});
        auto j = serializeSave(s, f.items);
        ASSERT_TRUE(j.ok());
        auto out = parseSave(*j, f.items, f.blocks, f.effects, 42);
        ASSERT_FALSE(out.ok());
        EXPECT_NE(out.error().message.find("no smeltable item"),
                  std::string::npos)
            << out.error().message;
    }
}

TEST(CampfireSaveTest, RejectsMalformedEntries) {
    SaveFixture f;
    SaveGame s = makeCampfireSave(f, CampfireCookState {});
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok());
    // Unknown item name in the slot.
    {
        Json root = *j;
        // Json's array access is const-only: mutate a copy, rebuild the
        // array.
        Json e = root.find("campfires")->at(0);
        e["item"] = Json(std::string("unobtainium"));
        Json arr = Json::array();
        arr.push_back(std::move(e));
        root["campfires"] = std::move(arr);
        auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
        ASSERT_FALSE(out.ok());
        EXPECT_NE(out.error().message.find("unobtainium"), std::string::npos)
            << out.error().message;
    }
    // An out-of-range progress count.
    {
        Json root = *j;
        Json e = root.find("campfires")->at(0);
        e["progress_substeps"] = Json(kFurnaceMaxProgressSubsteps + 1);
        Json arr = Json::array();
        arr.push_back(std::move(e));
        root["campfires"] = std::move(arr);
        auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
        ASSERT_FALSE(out.ok());
        EXPECT_NE(out.error().message.find("progress_substeps"),
                  std::string::npos)
            << out.error().message;
    }
    // An unknown field in the entry.
    {
        Json root = *j;
        Json e = root.find("campfires")->at(0);
        e["extra"] = Json(1);
        Json arr = Json::array();
        arr.push_back(std::move(e));
        root["campfires"] = std::move(arr);
        auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
        ASSERT_FALSE(out.ok());
        EXPECT_NE(out.error().message.find("extra"), std::string::npos)
            << out.error().message;
    }
}

// -- content pinning (the shipped JSON is the contract) ---------------------

TEST(CampfireTest, CampfireBlockContentPinned) {
    // The real asset: the campfire block (Phase 71). The values are the
    // contract (light 15 = the Minecraft campfire; the cross model; the
    // interact names the cooking behavior; it drops itself).
    BlockRegistry blocks;
    // Read the shipped file text through the same JSON parser the registry
    // uses (the test runs from the repo root).
    FILE* fp = std::fopen("assets/data/blocks/campfire.json", "rb");
    ASSERT_NE(fp, nullptr) << "run from the repo root";
    std::string text;
    char buf[4096];
    std::size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), fp)) > 0) {
        text.append(buf, n);
    }
    std::fclose(fp);
    auto parsed = Json::parse(text);
    ASSERT_TRUE(parsed.ok()) << parsed.error().message;
    auto reg = blocks.registerBlock(*parsed);
    ASSERT_TRUE(reg.ok()) << reg.error().message;
    const Block& b = blocks.at(*reg);
    EXPECT_EQ(b.name, "campfire");
    EXPECT_DOUBLE_EQ(b.properties.hardness, 0.5);
    EXPECT_EQ(b.properties.lightEmission, 15);
    EXPECT_FALSE(b.properties.solid);
    EXPECT_TRUE(b.properties.transparent);
    EXPECT_EQ(b.properties.interact, "campfire");
    EXPECT_EQ(b.properties.drops, "campfire");
    EXPECT_FALSE(b.properties.flamable); // a light source, not a fuel
    EXPECT_EQ(b.model.shape, BlockModel::Shape::Cross);
}

TEST(CampfireTest, CampfireRecipeContentPinned) {
    // The shipped recipe: the Minecraft campfire shape (5 logs + 3 sticks +
    // 1 coal). The project's log is the wood block.
    FILE* fp = std::fopen("assets/data/recipes/campfire.json", "rb");
    ASSERT_NE(fp, nullptr) << "run from the repo root";
    std::string text;
    char buf[4096];
    std::size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), fp)) > 0) {
        text.append(buf, n);
    }
    std::fclose(fp);
    auto parsed = Json::parse(text);
    ASSERT_TRUE(parsed.ok()) << parsed.error().message;
    const Json& def = *parsed;
    EXPECT_EQ(def.find("type")->asString(), "shaped");
    EXPECT_EQ(def.find("output")->find("item")->asString(), "campfire");
    EXPECT_EQ(def.find("output")->find("count")->asInt(), 1);
    const Json& pattern = *def.find("pattern");
    ASSERT_EQ(pattern.size(), 3u);
    EXPECT_EQ(pattern.at(0).asString(), "SSS");
    EXPECT_EQ(pattern.at(1).asString(), "LCL");
    EXPECT_EQ(pattern.at(2).asString(), "LLL");
    const Json& key = *def.find("key");
    EXPECT_EQ(key.find("S")->asString(), "stick");
    EXPECT_EQ(key.find("L")->asString(), "wood");
    EXPECT_EQ(key.find("C")->asString(), "coal");
}

} // namespace
