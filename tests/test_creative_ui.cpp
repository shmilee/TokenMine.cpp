// V3-11b creative tab tests: the layout (the E kind's shared outer + the
// title row's tabs + the search box + the 6x4 grant grid) and the
// CreativeTabUI state machine (the search's filter / the text's
// vocabulary + cap / the scroll's clamps / the grant's merge / replace /
// no-op rules) — pure, no SDL/GL (the EnchantUI's test model).
#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "core/json.hpp"
#include "item/inventory.hpp"
#include "item/item_registry.hpp"
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

struct CreativeFixture {
    ItemRegistry items;
    Inventory inv;
    CreativeTabUI ui;

    CreativeFixture() : ui(inv, items) {
        registerN("a", 30); // 30 aNN materials (the scroll's tail room)
        auto r = items.registerItem(parseJson(
            R"({"id":"stone","type":"material"})"));
        EXPECT_TRUE(r.ok());
        r = items.registerItem(parseJson(
            R"({"id":"stone_bricks","type":"material"})"));
        EXPECT_TRUE(r.ok());
        r = items.registerItem(parseJson(
            R"({"id":"stick","type":"material"})"));
        EXPECT_TRUE(r.ok());
        r = items.registerItem(parseJson(
            R"({"id":"tool","type":"tool","tool":{"break_speed":1.0},"max_stack":1})"));
        EXPECT_TRUE(r.ok());
    }

    void registerN(const char* prefix, int n) {
        for (int i = 0; i < n; ++i) {
            const std::string id = std::string(prefix) +
                (i < 10 ? "0" : "") + std::to_string(i);
            EXPECT_TRUE(items
                            .registerItem(
                                parseJson(R"({"id":")" + id +
                                          R"(","type":"material"})"))
                            .ok());
        }
    }
};

bool disjoint(const UIRect& a, const UIRect& b) {
    return a.x + a.w <= b.x || b.x + b.w <= a.x || a.y + a.h <= b.y ||
           b.y + b.h <= a.y;
}

} // namespace

// -- the layout -----------------------------------------------------------

// The recipeCount keeps the panel's real height (the E panel's 54
// shapeless rows — at 0 rows the panel is too SHORT for the 6x4 grid
// (the layout's fit precondition: the game always opens it with rows)).
constexpr int kLayoutRecipes = 54;

TEST(CreativeLayoutTest, SharedOuterAndGeometry) {
    const int w = 679;
    const int h = 744;
    const CreativeLayout cl = CreativeLayout::compute(w, h, kLayoutRecipes);
    const InventoryLayout inv = InventoryLayout::compute(w, h,
                                                          kLayoutRecipes,
                                                          PanelKind::Inventory);
    // The outer is the E kind's (both tabs share it — the creative's
    // delta is the title row + the 物品栏 tab's content only).
    EXPECT_EQ(cl.panel.x, inv.panel.x);
    EXPECT_EQ(cl.panel.y, inv.panel.y);
    EXPECT_EQ(cl.panel.w, inv.panel.w);
    EXPECT_EQ(cl.panel.h, inv.panel.h);
    // The tabs sit in the title row (inside the panel, above the
    // content), side by side.
    EXPECT_TRUE(cl.panel.contains(cl.tabItems.x, cl.tabItems.y));
    EXPECT_TRUE(cl.panel.contains(cl.tabBackpack.x,
                                  cl.tabBackpack.y));
    EXPECT_EQ(cl.tabItems.y, cl.tabBackpack.y);
    EXPECT_GT(cl.tabBackpack.x, cl.tabItems.x + cl.tabItems.w);
    EXPECT_TRUE(disjoint(cl.tabItems, cl.tabBackpack));
    // The search box is inside the panel, below the tabs.
    EXPECT_TRUE(cl.panel.contains(cl.search.x, cl.search.y));
    EXPECT_TRUE(cl.search.y > cl.tabItems.y + cl.tabItems.h);
    // 24 cells, all inside the panel, all below the search, pairwise
    // disjoint (the click routing's guarantee).
    ASSERT_EQ(cl.cells.size(), CreativeLayout::kVisible);
    for (const UIRect& c : cl.cells) {
        EXPECT_TRUE(cl.panel.contains(c.x, c.y));
        EXPECT_TRUE(c.y >= cl.search.y + cl.search.h);
    }
    for (std::size_t i = 0; i < cl.cells.size(); ++i) {
        for (std::size_t j = i + 1; j < cl.cells.size(); ++j) {
            EXPECT_TRUE(disjoint(cl.cells[i], cl.cells[j]))
                << i << "," << j;
        }
    }
}

TEST(CreativeLayoutTest, HitRouting) {
    const CreativeLayout cl = CreativeLayout::compute(679, 744,
                                                      kLayoutRecipes);
    // Each tab hit-tests at its center.
    EXPECT_EQ(cl.tabAt(cl.tabItems.x + cl.tabItems.w / 2,
                       cl.tabItems.y + cl.tabItems.h / 2),
              0);
    EXPECT_EQ(cl.tabAt(cl.tabBackpack.x + cl.tabBackpack.w / 2,
                       cl.tabBackpack.y + cl.tabBackpack.h / 2),
              1);
    // Each cell hit-tests at its center; a point in the panel corner's
    // padding misses everything.
    for (int i = 0; i < CreativeLayout::kVisible; ++i) {
        const UIRect& c = cl.cells[static_cast<std::size_t>(i)];
        EXPECT_EQ(cl.cellAt(c.x + c.w / 2, c.y + c.h / 2), i) << i;
    }
    EXPECT_EQ(cl.tabAt(cl.panel.x + 1, cl.panel.y + 1), -1);
    EXPECT_EQ(cl.cellAt(cl.panel.x + 1, cl.panel.y + 1), -1);
    // The search box is not a cell (the click's grant routing never
    // fires on a search-box hit).
    EXPECT_EQ(cl.cellAt(cl.search.x + cl.search.w / 2,
                        cl.search.y + cl.search.h / 2),
              -1);
}

// -- the search's filter ---------------------------------------------------

TEST(CreativeTabUITest, FilterPrefixAndNameSort) {
    CreativeFixture f;
    // Empty text = every grantable item (34 = 30 aNN + stone +
    // stone_bricks + stick + tool), name-ascending.
    const std::vector<ItemID>& all = f.ui.filtered();
    ASSERT_EQ(all.size(), 34u);
    for (std::size_t i = 1; i < all.size(); ++i) {
        EXPECT_LT(f.items.at(all[i - 1]).name,
                  f.items.at(all[i]).name);
    }
    // The prefix filter: "sto" = stone + stone_bricks (aNN misses).
    f.ui.appendFrameText("sto");
    const std::vector<ItemID>& sto = f.ui.filtered();
    ASSERT_EQ(sto.size(), 2u);
    EXPECT_EQ(f.items.at(sto[0]).name, "stone");
    EXPECT_EQ(f.items.at(sto[1]).name, "stone_bricks");
    // A no-match prefix = the empty list (the grid renders empty).
    f.ui.appendFrameText("zzz");
    EXPECT_TRUE(f.ui.filtered().empty());
    f.ui.reset();
    EXPECT_EQ(f.ui.filtered().size(), 34u);
}

// -- the text's vocabulary --------------------------------------------------

TEST(CreativeTabUITest, TextVocabularyAndCap) {
    CreativeFixture f;
    // Uppercase lowercases; space / punctuation / non-ASCII drop.
    f.ui.appendFrameText("Sto-N");
    EXPECT_EQ(f.ui.text(), "ston");
    // The cap (kSearchTextMax = 24): a longer frame keeps the first 24.
    f.ui.reset();
    f.ui.appendFrameText(
        "abcdefghijklmnopqrstuvwxyz0123"); // 30 chars
    ASSERT_EQ(f.ui.text().size(), CreativeTabUI::kSearchTextMax);
    EXPECT_EQ(f.ui.text(), "abcdefghijklmnopqrstuvwx");
    // Backspace pops one (and a no-op on empty).
    f.ui.backspace();
    EXPECT_EQ(f.ui.text(), "abcdefghijklmnopqrstuvw");
    f.ui.reset();
    f.ui.backspace();
    EXPECT_EQ(f.ui.text(), "");
}

// -- the scroll -------------------------------------------------------------

TEST(CreativeTabUITest, ScrollClampsAndShrinkReclamps) {
    CreativeFixture f;
    const int total = static_cast<int>(f.ui.filtered().size()); // 34
    const int tail = total - CreativeLayout::kVisible; // 10
    // A deep scroll clamps to the tail.
    f.ui.scrollBy(999);
    EXPECT_EQ(f.ui.scroll(), tail);
    f.ui.scrollBy(-999);
    EXPECT_EQ(f.ui.scroll(), 0);
    // The text shrinks the list under a scrolled window: the filtered()
    // read re-clamps (no dangling start past the new tail).
    f.ui.scrollBy(999); // back to the tail (10)
    f.ui.appendFrameText("a0"); // only a00..a09 match (10 items)
    const std::vector<ItemID>& small = f.ui.filtered();
    ASSERT_EQ(small.size(), 10u);
    EXPECT_EQ(f.ui.visibleStart(), 0);
    // A mid window stays put.
    f.ui.reset();
    f.ui.scrollBy(3);
    EXPECT_EQ(f.ui.visibleStart(), 3);
    EXPECT_EQ(f.ui.filtered().size(), static_cast<std::size_t>(total));
}

// -- the grant ----------------------------------------------------------------

TEST(CreativeTabUITest, GrantFullSingleMergeReplace) {
    CreativeFixture f;
    const ItemID stone =
        f.items.idForName("stone"); // max_stack 64
    const ItemID tool = f.items.idForName("tool"); // max_stack 1
    // The visible window starts at 0: a00 is the first name (the grid's
    // cell 0). Grant a00 full to the empty slot 0.
    const ItemID a00 = f.ui.filtered()[0];
    ASSERT_TRUE(f.ui.grant(0, 0, /*full=*/true));
    EXPECT_EQ(f.inv.at(0).item, a00);
    EXPECT_EQ(f.inv.at(0).count, 64);
    // A single grant to the same item MERGES (already full = no-op).
    EXPECT_FALSE(f.ui.grant(0, 0, /*full=*/false)); // full slot: no-op
    // A different item REPLACES (the creative's rule — the old stack is
    // gone, nothing returns anywhere).
    // Find stone's cell: with the empty search it is in the list; the
    // visible window is the first 24 name-sorted entries (a00..a23) —
    // stone is past that. Filter to it and grant single.
    f.ui.reset();
    f.ui.appendFrameText("stone");
    ASSERT_EQ(f.ui.filtered().size(), 2u); // stone, stone_bricks
    ASSERT_TRUE(f.ui.grant(0, 0, /*full=*/false));
    EXPECT_EQ(f.inv.at(0).item, stone);
    EXPECT_EQ(f.inv.at(0).count, 1);
    // A full grant now = a merge to the cap (1 -> 64).
    ASSERT_TRUE(f.ui.grant(0, 0, /*full=*/true));
    EXPECT_EQ(f.inv.at(0).count, 64);
    // A max-stack-1 item: full = single = 1 (the cap's read).
    f.ui.reset();
    f.ui.appendFrameText("tool");
    ASSERT_EQ(f.ui.filtered().size(), 1u);
    ASSERT_TRUE(f.ui.grant(1, 0, /*full=*/true));
    EXPECT_EQ(f.inv.at(1).item, tool);
    EXPECT_EQ(f.inv.at(1).count, 1);
    EXPECT_FALSE(f.ui.grant(1, 0, /*full=*/true)); // already at the cap
}

TEST(CreativeTabUITest, GrantOutOfRangeNoOps) {
    CreativeFixture f;
    const ItemStack before = f.inv.at(5);
    // A cell past the visible window's filtered tail (filter to one
    // item, then a cell index of 1 = past the single list entry).
    f.ui.appendFrameText("tool");
    ASSERT_EQ(f.ui.filtered().size(), 1u);
    EXPECT_FALSE(f.ui.grant(5, 1, /*full=*/true));
    // A negative slot / cell.
    EXPECT_FALSE(f.ui.grant(-1, 0, /*full=*/true));
    EXPECT_FALSE(f.ui.grant(5, -1, /*full=*/true));
    EXPECT_EQ(f.inv.at(5), before);
}

TEST(CreativeTabUITest, GrantDropsDamageAndEnchants) {
    CreativeFixture f;
    // A slot carrying a damaged + enchanted stack of the granted item:
    // the merge keeps the item + count but drops the damage / enchants
    // (a creative grant carries neither — the fresh stack's rule).
    const ItemID a00 = f.ui.filtered()[0];
    f.inv.at(0) = ItemStack {a00, 10, 5.0f, {{"sharpness", 1}}};
    ASSERT_TRUE(f.ui.grant(0, 0, /*full=*/false)); // single: 10 -> 11
    EXPECT_EQ(f.inv.at(0).item, a00);
    EXPECT_EQ(f.inv.at(0).count, 11);
    EXPECT_FLOAT_EQ(f.inv.at(0).damage, 0.0f);
    EXPECT_TRUE(f.inv.at(0).enchants.empty());
    // A replace (a different item) seeds a fresh stack (no damage /
    // enchants either).
    f.ui.reset();
    f.ui.appendFrameText("stone");
    ASSERT_EQ(f.ui.filtered().size(), 2u);
    ASSERT_TRUE(f.ui.grant(0, 0, /*full=*/true));
    EXPECT_EQ(f.inv.at(0).item, f.items.idForName("stone"));
    EXPECT_FLOAT_EQ(f.inv.at(0).damage, 0.0f);
    EXPECT_TRUE(f.inv.at(0).enchants.empty());
}
