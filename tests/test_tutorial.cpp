// UX-7 beginner tutorial tests: the strict parse (the closed goal
// vocabulary + the per-step field discipline, every failure named), the
// init cross-check (a qualified goal's name half must exist in the
// registry — real registries, the CRAFT-2 content-pin paradigm), the
// PURE step machine (the campfire pattern — the game's
// emitTutorialEvent is the thin adapter; the logic is owned here), the
// save-restore rules (format 23/24: the id reset + the count bound;
// format 25 / V2-3: the series dimension — the series reset, the
// value-faithful series-0 default), the target line / tip line / toast
// constants, the config strict read, the global step-id check (V2-3),
// and the shipped content pin (the three series files: beginner.json
// the 11 steps exact — the UX-8a planks step + the reworded table step
// + the UX-9b coal/torch/light steps after spick — farming.json the 7
// steps exact, kitchen.json the 6 steps exact (V2-3) — + every text AND
// tip character inside the V2-3 422-glyph font set).
//
// Run from the repository root (the assets are relative).
#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include "block/block_registry.hpp"
#include "core/asset_manager.hpp"
#include "core/config.hpp"
#include "core/tutorial.hpp"
#include "entity/mob_registry.hpp"
#include "item/item_registry.hpp"
#include "ui/font.hpp"

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

TutorialStep step(const std::string& id, const std::string& goal,
                  int count) {
    // The trailing "" is the optional tip (UX-8b — no tip row for the
    // machine's fixture steps unless a test sets one).
    return TutorialStep {id, id + " text", goal, count, ""};
}

// The machine's fixture: three steps (a count-1 bare goal, a count-3
// qualified goal, a count-2 qualified goal — the last, so the done
// flip is reachable). Single-series (the pre-V2-3 shape, wrapped).
std::vector<TutorialStep> threeSteps() {
    return {step("a", "walk", 1), step("b", "break.wood", 3),
            step("c", "kill.chicken", 2)};
}

std::vector<std::vector<TutorialStep>> oneSeries() {
    return {threeSteps()};
}

// The machine's multi-series fixture (the V2-3 shape): three series —
// a 2-step series (a count-1 bare goal + a count-3 qualified goal), a
// 2-step series (two count-1 qualified goals) and a 1-step series
// (the done flip is reachable at the last step of the last series).
std::vector<std::vector<TutorialStep>> threeSeries() {
    return {{step("walk", "walk", 1), step("chop", "break.wood", 3)},
            {step("till", "craft.farmland", 1),
             step("soil", "place.farmland", 1)},
            {step("meal", "eat", 1)}};
}

// The REAL shipped registries (the CRAFT-2 content-pin paradigm — the
// content pin + the goal cross-checks run against what the game loads).
struct RealRegistries {
    BlockRegistry blocks;
    ItemRegistry items;
    MobRegistry mobs;
    bool load() {
        AssetManager assets;
        if (!assets.setRoot("assets").ok()) {
            return false;
        }
        if (!blocks.loadDirectory(assets, "data/blocks").ok()) {
            return false;
        }
        auto bi = items.buildBlockItems(blocks);
        if (!bi.ok()) {
            return false;
        }
        if (!items.loadDirectory(assets, "data/items").ok()) {
            return false;
        }
        if (!mobs.loadDirectory(assets, "data/mobs").ok()) {
            return false;
        }
        return true;
    }
};

// The REAL shipped tutorial series set (the V2-3 init path reproduced:
// listDir byte order = the series order, the non-.json rejection, the
// strict parse).
struct ShippedTutorials {
    std::vector<std::string> files;
    std::vector<std::vector<TutorialStep>> series;
    bool load() {
        AssetManager assets;
        if (!assets.setRoot("assets").ok()) {
            return false;
        }
        auto listed = assets.listDir("data/tutorials");
        if (!listed.ok()) {
            return false;
        }
        for (const std::string& file : *listed) {
            if (file.size() <= 5 || file.compare(file.size() - 5, 5,
                                                 ".json") != 0) {
                return false;
            }
            auto text = assets.loadText("data/tutorials/" + file);
            if (!text.ok()) {
                return false;
            }
            auto j = Json::parse(*text);
            if (!j.ok()) {
                return false;
            }
            auto steps = parseTutorial(*j);
            if (!steps.ok()) {
                return false;
            }
            series.push_back(std::move(*steps));
        }
        files = *listed;
        return !series.empty();
    }
};

// True when every codepoint of `text` is either a printable ASCII
// character (32..126 — the font's rows 4..15) or a member of the V2-3
// 422-glyph CJK set (kCjkGlyphs, codepoint-sorted — the binary search).
bool allCharsInFontSet(const std::string& text) {
    std::size_t pos = 0;
    while (pos < text.size()) {
        auto cp = utf8NextCodepoint(text, pos);
        if (!cp) {
            return false; // a malformed sequence never belongs in content
        }
        if (*cp >= 32 && *cp <= 126) {
            continue;
        }
        const uint32_t c = *cp;
        const auto it = std::lower_bound(
            std::begin(kCjkGlyphs), std::end(kCjkGlyphs), c,
            [](const CjkGlyph& g, uint32_t v) { return g.cp < v; });
        if (it == std::end(kCjkGlyphs) || it->cp != c) {
            return false;
        }
    }
    return true;
}

} // namespace

// -- strict parse ------------------------------------------------------------

TEST(TutorialParseTest, ValidTwoSteps) {
    const Json def = parseJson(
        R"({"steps": [
            {"id": "s1", "text": "text one", "goal": "walk", "count": 1},
            {"id": "s2", "text": "text two", "goal": "break.wood", "count": 3}
        ]})");
    auto r = parseTutorial(def);
    ASSERT_TRUE(r.ok()) << r.error().message;
    ASSERT_EQ(r->size(), 2u);
    EXPECT_EQ((*r)[0].id, "s1");
    EXPECT_EQ((*r)[0].text, "text one");
    EXPECT_EQ((*r)[0].goal, "walk");
    EXPECT_EQ((*r)[0].count, 1);
    EXPECT_EQ((*r)[1].goal, "break.wood");
    EXPECT_EQ((*r)[1].count, 3);
}

TEST(TutorialParseTest, RejectsNonObject) {
    EXPECT_FALSE(parseTutorial(parseJson(R"([1, 2])")).ok());
}

TEST(TutorialParseTest, RejectsUnknownTopLevelKey) {
    EXPECT_FALSE(
        parseTutorial(parseJson(
            R"({"steps": [{"id": "a", "text": "t", "goal": "walk", "count": 1}], "extra": 1})"))
            .ok());
    EXPECT_FALSE(
        parseTutorial(parseJson(
            R"({"id": "beginner", "steps": [{"id": "a", "text": "t", "goal": "walk", "count": 1}]})"))
            .ok());
}

TEST(TutorialParseTest, RejectsMissingOrEmptySteps) {
    EXPECT_FALSE(parseTutorial(parseJson(R"({})")).ok());
    EXPECT_FALSE(parseTutorial(parseJson(R"({"steps": {}})")).ok());
    EXPECT_FALSE(parseTutorial(parseJson(R"({"steps": []})")).ok());
}

TEST(TutorialParseTest, RejectsStepFieldViolations) {
    auto wrap = [](const char* stepJson) {
        return std::string(R"({"steps": [)") + stepJson + R"(]})";
    };
    const char* ok = R"({"id": "a", "text": "t", "goal": "walk", "count": 1})";
    ASSERT_TRUE(parseTutorial(parseJson(wrap(ok))).ok());
    // Unknown key.
    EXPECT_FALSE(parseTutorial(parseJson(
                         wrap(R"({"id": "a", "text": "t", "goal": "walk", "count": 1, "x": 1})")))
                        .ok());
    // Missing keys (one at a time).
    EXPECT_FALSE(parseTutorial(
                         parseJson(wrap(R"({"text": "t", "goal": "walk", "count": 1})")))
                         .ok());
    EXPECT_FALSE(
        parseTutorial(parseJson(wrap(R"({"id": "a", "goal": "walk", "count": 1})")))
            .ok());
    EXPECT_FALSE(
        parseTutorial(parseJson(wrap(R"({"id": "a", "text": "t", "count": 1})")))
            .ok());
    EXPECT_FALSE(
        parseTutorial(parseJson(wrap(R"({"id": "a", "text": "t", "goal": "walk"})")))
            .ok());
    // Empty id / text.
    EXPECT_FALSE(parseTutorial(
                         parseJson(wrap(R"({"id": "", "text": "t", "goal": "walk", "count": 1})")))
                         .ok());
    EXPECT_FALSE(parseTutorial(
                         parseJson(wrap(R"({"id": "a", "text": "", "goal": "walk", "count": 1})")))
                         .ok());
}

TEST(TutorialParseTest, OptionalTipParsesWhenPresentAndAbsent) {
    // UX-8b: the tip is OPTIONAL — present = the non-empty string is
    // stored on the step; absent = empty (the step draws no tip row,
    // the v1.41.0 render).
    const std::string def =
        R"({"steps": [
            {"id": "a", "text": "t", "goal": "walk", "count": 1,
             "tip": "the hint"},
            {"id": "b", "text": "u", "goal": "jump", "count": 2}
        ]})";
    auto r = parseTutorial(parseJson(def));
    ASSERT_TRUE(r.ok()) << r.error().message;
    ASSERT_EQ(r->size(), 2u);
    EXPECT_EQ((*r)[0].tip, "the hint");
    EXPECT_TRUE((*r)[1].tip.empty());
}

TEST(TutorialParseTest, RejectsBadTip) {
    auto wrap = [](const char* stepJson) {
        return std::string(R"({"steps": [)") + stepJson + R"(]})";
    };
    // A non-string tip is named (the step + the key, the project's
    // "tutorial 'steps'[i] '<key>'" wording).
    auto r = parseTutorial(parseJson(wrap(
        R"({"id": "a", "text": "t", "goal": "walk", "count": 1, "tip": 1})")));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("steps'[0]"), std::string::npos);
    EXPECT_NE(r.error().message.find("tip"), std::string::npos);
    // An explicit "" is named and rejected (the ammo discipline — the
    // sound_material precedent: the key present-but-empty is a bug, not
    // an absence).
    r = parseTutorial(parseJson(wrap(
        R"({"id": "a", "text": "t", "goal": "walk", "count": 1, "tip": ""})")));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("steps'[0]"), std::string::npos);
    EXPECT_NE(r.error().message.find("tip"), std::string::npos);
    // A valid non-empty tip passes.
    EXPECT_TRUE(parseTutorial(parseJson(wrap(
        R"({"id": "a", "text": "t", "goal": "walk", "count": 1, "tip": "ok"})")))
                    .ok());
    // The unknown-key rejection is unchanged (tip is now a known key,
    // a different foreign key is still named).
    EXPECT_FALSE(parseTutorial(parseJson(wrap(
        R"({"id": "a", "text": "t", "goal": "walk", "count": 1, "tip": "ok", "x": 1})")))
                        .ok());
}

TEST(TutorialParseTest, RejectsDuplicateId) {
    EXPECT_FALSE(
        parseTutorial(parseJson(
            R"({"steps": [
                {"id": "a", "text": "t", "goal": "walk", "count": 1},
                {"id": "a", "text": "u", "goal": "jump", "count": 2}
            ]})"))
            .ok());
}

TEST(TutorialParseTest, RejectsBadCount) {
    auto wrap = [](const char* countJson) {
        return std::string(
                           R"({"steps": [{"id": "a", "text": "t", "goal": "walk", "count": )") +
                countJson + R"(}]})";
    };
    EXPECT_FALSE(parseTutorial(parseJson(wrap("0"))).ok());
    EXPECT_FALSE(parseTutorial(parseJson(wrap("-1"))).ok());
    EXPECT_FALSE(parseTutorial(parseJson(wrap("1.5"))).ok());
    EXPECT_FALSE(parseTutorial(parseJson(wrap(R"("one")"))).ok());
    EXPECT_TRUE(parseTutorial(parseJson(wrap("1"))).ok());
    EXPECT_TRUE(parseTutorial(parseJson(wrap("64"))).ok());
}

TEST(TutorialParseTest, RejectsGoalOutsideClosedVocabulary) {
    auto withGoal = [](const std::string& goal) {
        return parseTutorial(parseJson(
            std::string(R"({"steps": [{"id": "a", "text": "t", "goal": ")") +
            goal + R"(", "count": 1}]})"));
    };
    EXPECT_FALSE(withGoal("craftz.x").ok());   // unknown prefix
    EXPECT_FALSE(withGoal("break.").ok());     // empty name
    EXPECT_FALSE(withGoal("break.a.b").ok());  // a dot in the name
    EXPECT_FALSE(withGoal("break").ok());      // qualified without a dot
    EXPECT_FALSE(withGoal("").ok());           // empty
    // The bare words pass the grammar (their existence is not a
    // registry question). V2-13: "sleep" (the bed's night-skip event —
    // the vocabulary's new bare word) passes too.
    EXPECT_TRUE(withGoal("walk").ok());
    EXPECT_TRUE(withGoal("jump").ok());
    EXPECT_TRUE(withGoal("eat").ok());
    EXPECT_TRUE(withGoal("sleep").ok());
    EXPECT_EQ(kTutorialBareGoalCount, 4u);
    // All seven prefixes with a name pass the grammar (V2-3: pickup +
    // smelt joined the closed vocabulary).
    EXPECT_TRUE(withGoal("craft.x").ok());
    EXPECT_TRUE(withGoal("place.x").ok());
    EXPECT_TRUE(withGoal("kill.x").ok());
    EXPECT_TRUE(withGoal("interact.x").ok());
    EXPECT_TRUE(withGoal("pickup.x").ok());
    EXPECT_TRUE(withGoal("smelt.x").ok());
}

TEST(TutorialParseTest, UnknownPrefixErrorListsAllSevenPrefixes) {
    // V2-3: the grammar error message carries the FULL closed prefix
    // list (built from kTutorialGoalPrefixes — a vocabulary change
    // updates the wording automatically).
    auto withGoal = [](const std::string& goal) {
        return parseTutorial(parseJson(
            std::string(R"({"steps": [{"id": "a", "text": "t", "goal": ")") +
            goal + R"(", "count": 1}]})"));
    };
    auto bad = withGoal("foo.x");
    ASSERT_FALSE(bad.ok());
    for (const char* const p : kTutorialGoalPrefixes) {
        EXPECT_NE(bad.error().message.find(p), std::string::npos) << p;
    }
    EXPECT_EQ(kTutorialGoalPrefixCount, 7u);
}

// -- the global step-id check (V2-3) ------------------------------------------

TEST(TutorialGlobalIdsTest, ZeroCollisionsPass) {
    // The multi-series fixture's ids (walk/chop + till/soil + meal) and
    // the shipped set (the content pin below re-runs this on the real
    // files) are collision-free.
    EXPECT_TRUE(checkSeriesGlobalIds(threeSeries()).ok());
    EXPECT_TRUE(checkSeriesGlobalIds(oneSeries()).ok());
    // A degenerate empty set passes (the init rejects it separately —
    // the directory must hold at least one series).
    EXPECT_TRUE(checkSeriesGlobalIds({}).ok());
}

TEST(TutorialGlobalIdsTest, CrossSeriesDuplicateIsNamed) {
    auto s = threeSeries();
    s[1][0].id = "chop"; // series 1 now holds series 0's "chop"
    auto r = checkSeriesGlobalIds(s);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("series 0"), std::string::npos);
    EXPECT_NE(r.error().message.find("chop"), std::string::npos);
    EXPECT_NE(r.error().message.find("collides with series 1"),
              std::string::npos);
    // The collision on the FIRST pair wins (series order).
    s = threeSeries();
    s[2][0].id = "walk"; // series 2 vs series 0
    r = checkSeriesGlobalIds(s);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("collides with series 2"),
              std::string::npos);
}

// -- init cross-check (real registries) ---------------------------------------

TEST(TutorialValidateTest, ShippedGoalsExistInRealRegistries) {
    RealRegistries r;
    ASSERT_TRUE(r.load()) << "run from the repository root";
    ShippedTutorials t;
    ASSERT_TRUE(t.load()) << "the shipped files must parse";
    for (const auto& s : t.series) {
        auto v = validateTutorialGoals(s, r.items, r.blocks, r.mobs);
        ASSERT_TRUE(v.ok()) << v.error().message;
    }
}

TEST(TutorialValidateTest, NamesTheUnknownTarget) {
    RealRegistries r;
    ASSERT_TRUE(r.load()) << "run from the repository root";
    // The step is named + the missing target is named (the grows_to
    // precedent: the registry-order first failure).
    auto v = validateTutorialGoals({step("s", "break.stonex", 1)}, r.items,
                                   r.blocks, r.mobs);
    ASSERT_FALSE(v.ok());
    EXPECT_NE(v.error().message.find("break.stonex"), std::string::npos);
    EXPECT_NE(v.error().message.find("stonex"), std::string::npos);
    v = validateTutorialGoals({step("s", "kill.dragon", 1)}, r.items, r.blocks,
                              r.mobs);
    ASSERT_FALSE(v.ok());
    EXPECT_NE(v.error().message.find("kill.dragon"), std::string::npos);
    v = validateTutorialGoals({step("s", "craft.wooden_sword", 1)}, r.items,
                              r.blocks, r.mobs);
    ASSERT_FALSE(v.ok()); // the project has no sword (the brief's fact)
    // V2-3: pickup./smelt. point at ITEMS (the craft twin): an unknown
    // item is named the same way.
    v = validateTutorialGoals({step("s", "pickup.ghost_item", 1)}, r.items,
                              r.blocks, r.mobs);
    ASSERT_FALSE(v.ok());
    EXPECT_NE(v.error().message.find("pickup.ghost_item"), std::string::npos);
    EXPECT_NE(v.error().message.find("item 'ghost_item'"),
              std::string::npos);
    v = validateTutorialGoals({step("s", "smelt.ghost_item", 1)}, r.items,
                              r.blocks, r.mobs);
    ASSERT_FALSE(v.ok());
    EXPECT_NE(v.error().message.find("smelt.ghost_item"), std::string::npos);
    // Known items pass (wheat_1 / cooked_chicken exist — the block-item
    // build + the shipped item file).
    EXPECT_TRUE(validateTutorialGoals({step("s", "pickup.wheat_1", 1)},
                                      r.items, r.blocks, r.mobs)
                    .ok());
    EXPECT_TRUE(validateTutorialGoals({step("s", "smelt.cooked_chicken", 1)},
                                      r.items, r.blocks, r.mobs)
                    .ok());
    // A bare goal is not a registry question — it passes.
    EXPECT_TRUE(validateTutorialGoals({step("s", "walk", 1)}, r.items,
                                      r.blocks, r.mobs)
                    .ok());
}

TEST(TutorialValidateTest, KillBatIsAClosedVocabularyWord) {
    // V2-14: the bat joins the kill.<mob> closed vocabulary (the grammar
    // accepts the prefix + the init cross-check resolves the name in the
    // MobRegistry — the bat's registration is what makes the word valid;
    // the bat is killable: 6 HP, no drops — the pure ambience mob).
    RealRegistries r;
    ASSERT_TRUE(r.load()) << "run from the repository root";
    auto v = validateTutorialGoals({step("s", "kill.bat", 1)}, r.items,
                                   r.blocks, r.mobs);
    ASSERT_TRUE(v.ok()) << v.error().message;
    // The kill event the game emits for a bat death ("kill." + the data
    // id) is exactly the word the step waits for.
    const TutorialStep s = step("s", "kill.bat", 1);
    const TutorialState s0 {};
    const TutorialState s1 =
        tutorialApply(s0, "kill.bat", 1, {std::vector {s}});
    EXPECT_TRUE(s1.done);  // count 1: the single kill completes the step
    const TutorialState sNoop = tutorialApply(s0, "kill.pig", 1, {std::vector {s}});
    EXPECT_FALSE(sNoop.done);  // a different mob's kill does not match
}

// -- the pure step machine ----------------------------------------------------

TEST(TutorialApplyTest, FirstEventAdvancesCountOne) {
    const std::vector<std::vector<TutorialStep>> series = oneSeries();
    const TutorialState s0 {}; // step 0, progress 0, not done
    const TutorialState s1 = tutorialApply(s0, "walk", 1, series);
    EXPECT_FALSE(s1.done);
    EXPECT_EQ(s1.seriesIndex, 0);
    EXPECT_EQ(s1.stepIndex, 1);
    EXPECT_EQ(s1.progress, 0);
}

TEST(TutorialApplyTest, MismatchedEventIgnored) {
    const std::vector<std::vector<TutorialStep>> series = oneSeries();
    const TutorialState s0 {};
    const TutorialState s1 = tutorialApply(s0, "jump", 1, series);
    EXPECT_EQ(s1.stepIndex, 0);
    EXPECT_EQ(s1.progress, 0);
    EXPECT_FALSE(s1.done);
    // A qualified mismatch (the right name, the wrong step) too.
    const TutorialState s2 = tutorialApply(s0, "break.wood", 1, series);
    EXPECT_EQ(s2.stepIndex, 0);
    EXPECT_EQ(s2.progress, 0);
}

TEST(TutorialApplyTest, ExactCountAdvances) {
    const std::vector<std::vector<TutorialStep>> series = oneSeries();
    TutorialState s {};
    s = tutorialApply(s, "walk", 1, series); // (step 0 is walk —
    s.stepIndex = 1;                          // place it at "b")
    EXPECT_EQ(s.progress, 0);
    s = tutorialApply(s, "break.wood", 1, series);
    EXPECT_EQ(s.stepIndex, 1);
    EXPECT_EQ(s.progress, 1);
    s = tutorialApply(s, "break.wood", 1, series);
    EXPECT_EQ(s.stepIndex, 1);
    EXPECT_EQ(s.progress, 2);
    s = tutorialApply(s, "break.wood", 1, series);
    EXPECT_EQ(s.stepIndex, 2); // the exact 3rd event advances
    EXPECT_EQ(s.progress, 0);
    EXPECT_FALSE(s.done);
}

TEST(TutorialApplyTest, NGreaterThanOneAdvancesOnce) {
    const std::vector<std::vector<TutorialStep>> series = oneSeries();
    TutorialState s {};
    s.stepIndex = 1; // "b": break.wood, count 3
    // An n=5 event (the function supports it, the brief's case)
    // completes the step ONCE — the overshoot is not banked (progress
    // resets to 0, the next step starts clean).
    const TutorialState s2 = tutorialApply(s, "break.wood", 5, series);
    EXPECT_EQ(s2.stepIndex, 2);
    EXPECT_EQ(s2.progress, 0);
    EXPECT_FALSE(s2.done);
}

TEST(TutorialApplyTest, LastStepCompletesToDone) {
    const std::vector<std::vector<TutorialStep>> series = oneSeries();
    TutorialState s {};
    s.stepIndex = 2; // "c": kill.chicken, count 2
    s.progress = 1;
    const TutorialState s2 = tutorialApply(s, "kill.chicken", 1, series);
    EXPECT_TRUE(s2.done);
    EXPECT_EQ(s2.progress, 0);
}

TEST(TutorialApplyTest, SeriesBoundaryTransitionsToNextSeriesFirstStep) {
    // V2-3: the last step of a NON-final series completes -> the next
    // series starts at its first step (seriesIndex + 1, stepIndex 0,
    // progress 0 — not done).
    const std::vector<std::vector<TutorialStep>> series = threeSeries();
    TutorialState s {};
    s = tutorialApply(s, "walk", 1, series); // -> series 0, step "chop"
    ASSERT_FALSE(s.done);
    ASSERT_EQ(s.seriesIndex, 0);
    ASSERT_EQ(s.stepIndex, 1);
    s = tutorialApply(s, "break.wood", 1, series);
    EXPECT_EQ(s.seriesIndex, 0);
    EXPECT_EQ(s.stepIndex, 1);
    EXPECT_EQ(s.progress, 1);
    s = tutorialApply(s, "break.wood", 1, series);
    s = tutorialApply(s, "break.wood", 1, series); // the exact 3rd event
    EXPECT_FALSE(s.done);
    EXPECT_EQ(s.seriesIndex, 1); // the series transition
    EXPECT_EQ(s.stepIndex, 0);
    EXPECT_EQ(s.progress, 0);
    // The transition does not bank the overshoot: an n=5 event at the
    // boundary lands on the next series' first step at progress 0.
    TutorialState s2 {};
    s2.stepIndex = 1; // series 0 "chop", count 3
    s2 = tutorialApply(s2, "break.wood", 5, series);
    EXPECT_FALSE(s2.done);
    EXPECT_EQ(s2.seriesIndex, 1);
    EXPECT_EQ(s2.stepIndex, 0);
    EXPECT_EQ(s2.progress, 0);
}

TEST(TutorialApplyTest, LastSeriesCompletesToDoneClamped) {
    // V2-3: the last step of the LAST series completes -> done, clamped
    // to the last series' last step (the stepIndex clamp discipline).
    const std::vector<std::vector<TutorialStep>> series = threeSeries();
    TutorialState s {};
    s.seriesIndex = 2; // "meal": eat, count 1
    s.stepIndex = 0;
    const TutorialState s2 = tutorialApply(s, "eat", 1, series);
    EXPECT_TRUE(s2.done);
    EXPECT_EQ(s2.seriesIndex, 2);
    EXPECT_EQ(s2.stepIndex, 0); // the last step of the last series
    EXPECT_EQ(s2.progress, 0);
    // The clamp for a MULTI-step last series: the done state sits at
    // that series' LAST step, not the first.
    std::vector<std::vector<TutorialStep>> series2 = threeSeries();
    series2[2].push_back(step("feast", "eat", 2));
    TutorialState s3 {};
    s3.seriesIndex = 2;
    s3.stepIndex = 1; // "feast": count 2
    s3.progress = 1;
    const TutorialState s4 = tutorialApply(s3, "eat", 1, series2);
    EXPECT_TRUE(s4.done);
    EXPECT_EQ(s4.seriesIndex, 2);
    EXPECT_EQ(s4.stepIndex, 1); // clamped to the last step
    EXPECT_EQ(s4.progress, 0);
}

TEST(TutorialApplyTest, DoneIsImmune) {
    const std::vector<std::vector<TutorialStep>> series = oneSeries();
    const TutorialState done {0, 0, true};
    const TutorialState s1 = tutorialApply(done, "walk", 1, series);
    EXPECT_TRUE(s1.done);
    EXPECT_EQ(s1.seriesIndex, 0);
    EXPECT_EQ(s1.stepIndex, 0);
    EXPECT_EQ(s1.progress, 0);
}

TEST(TutorialApplyTest, MultipleEventsSameFrameAccumulate) {
    const std::vector<std::vector<TutorialStep>> series = oneSeries();
    TutorialState s {};
    s.stepIndex = 1; // "b": count 3
    s = tutorialApply(s, "break.wood", 1, series); // event 1 of the frame
    s = tutorialApply(s, "break.wood", 1, series); // event 2 of the frame
    EXPECT_EQ(s.stepIndex, 1);
    EXPECT_EQ(s.progress, 2);
    s = tutorialApply(s, "break.wood", 1, series); // the frame's 3rd
    EXPECT_EQ(s.stepIndex, 2);
    EXPECT_EQ(s.progress, 0);
}

TEST(TutorialApplyTest, DegenerateInputsLeaveStateAlone) {
    const std::vector<std::vector<TutorialStep>> series = threeSeries();
    const TutorialState s0 {};
    // An empty series list (defensive — the init rejects one).
    const TutorialState e1 = tutorialApply(s0, "walk", 1, {});
    EXPECT_EQ(e1.seriesIndex, 0);
    EXPECT_EQ(e1.stepIndex, 0);
    EXPECT_EQ(e1.progress, 0);
    EXPECT_FALSE(e1.done);
    // n <= 0 (the game never emits one; the machine must not regress).
    EXPECT_EQ(tutorialApply(s0, "walk", 0, series).progress, 0);
    EXPECT_EQ(tutorialApply(s0, "walk", -2, series).progress, 0);
    // Out of range on the SERIES axis (the defensive bound): a state
    // naming series 5 of a 3-series set is left alone.
    TutorialState bad {};
    bad.seriesIndex = 5;
    EXPECT_EQ(tutorialApply(bad, "walk", 1, series).seriesIndex, 5);
    bad.seriesIndex = -1;
    EXPECT_EQ(tutorialApply(bad, "walk", 1, series).seriesIndex, -1);
    // Out of range on the STEP axis (the pre-V2-3 guard, kept).
    bad = TutorialState {};
    bad.stepIndex = 42;
    EXPECT_EQ(tutorialApply(bad, "walk", 1, series).stepIndex, 42);
}

// -- the target line + the tip line + the toast constant -----------------------

TEST(TutorialUiTest, TargetLineCountOneIsTextOnly) {
    const std::vector<std::vector<TutorialStep>> series = oneSeries();
    const TutorialState s0 {}; // "a" (walk, count 1)
    EXPECT_EQ(tutorialTargetLine(s0, series), "a text");
}

TEST(TutorialUiTest, TargetLineCountThreeCarriesProgress) {
    const std::vector<std::vector<TutorialStep>> series = oneSeries();
    TutorialState s {};
    s.stepIndex = 1; // "b": break.wood, count 3
    s.progress = 1;
    EXPECT_EQ(tutorialTargetLine(s, series), "b text 1/3");
    s.progress = 0;
    EXPECT_EQ(tutorialTargetLine(s, series), "b text 0/3");
}

TEST(TutorialUiTest, TargetLineTakesTheStepOfTheCurrentSeries) {
    // V2-3: the line is series[state.seriesIndex][state.stepIndex] — a
    // state in series 1 draws that series' step, not series 0's.
    const std::vector<std::vector<TutorialStep>> series = threeSeries();
    TutorialState s {};
    s.seriesIndex = 1;
    s.stepIndex = 0;
    EXPECT_EQ(tutorialTargetLine(s, series), "till text");
    s.stepIndex = 1;
    EXPECT_EQ(tutorialTargetLine(s, series), "soil text");
}

TEST(TutorialUiTest, TargetLineDoneIsEmpty) {
    const std::vector<std::vector<TutorialStep>> series = oneSeries();
    const TutorialState done {0, 0, true};
    EXPECT_TRUE(tutorialTargetLine(done, series).empty());
    // A done state clamped deep in a multi-series set too.
    const std::vector<std::vector<TutorialStep>> multi = threeSeries();
    const TutorialState done2 {0, 0, true, 2};
    EXPECT_TRUE(tutorialTargetLine(done2, multi).empty());
}

TEST(TutorialUiTest, TipLineInProgressCarriesTheTip) {
    // UX-8b: the tip line is the current step's tip while in progress —
    // a tip-less step (or a step out of range) carries the empty line,
    // so the UI adds zero rows (bit-identical to v1.41.0).
    std::vector<std::vector<TutorialStep>> series = oneSeries();
    series[0][1].tip = "the hint";
    TutorialState s {};
    s.stepIndex = 1;
    EXPECT_EQ(tutorialTipLine(s, series), "the hint");
    EXPECT_TRUE(tutorialTipLine({}, series).empty());       // step 0: no tip
    EXPECT_TRUE(tutorialTipLine(s, oneSeries()).empty()); // tip-less data
}

TEST(TutorialUiTest, TipLineTakesTheStepOfTheCurrentSeries) {
    // V2-3: the tip is the CURRENT series' step's tip (a series-0 tip
    // never leaks into a series-1 step).
    std::vector<std::vector<TutorialStep>> series = threeSeries();
    series[0][0].tip = "the series-0 hint";
    series[1][0].tip = "the series-1 hint";
    TutorialState s {};
    s.seriesIndex = 1;
    EXPECT_EQ(tutorialTipLine(s, series), "the series-1 hint");
    s.seriesIndex = 0;
    EXPECT_EQ(tutorialTipLine(s, series), "the series-0 hint");
    s.seriesIndex = 1;
    s.stepIndex = 1; // a tip-less step of series 1
    EXPECT_TRUE(tutorialTipLine(s, series).empty());
}

TEST(TutorialUiTest, TipLineDoneIsEmpty) {
    // Done: the toast owns the slot — no tip row even for a tip step.
    std::vector<std::vector<TutorialStep>> series = oneSeries();
    series[0][0].tip = "the hint";
    const TutorialState done {0, 0, true};
    EXPECT_TRUE(tutorialTipLine(done, series).empty());
}

TEST(TutorialUiTest, ToastWindowIs120Frames) {
    EXPECT_EQ(kTutorialToastFrames, 120);
    // The toast text is in the font set (the brief's character audit).
    EXPECT_TRUE(allCharsInFontSet(kTutorialDoneText));
    EXPECT_EQ(std::string(kTutorialDoneText), "全部完成！");
}

// -- the save-restore rules -----------------------------------------------------

TEST(TutorialRestoreTest, AbsentLoadsFresh) {
    const std::vector<std::vector<TutorialStep>> series = oneSeries();
    bool reset = true;
    auto r = tutorialRestore(TutorialSaveState::Absent, 0, "", 0, series,
                             &reset);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(r->seriesIndex, 0);
    EXPECT_EQ(r->stepIndex, 0);
    EXPECT_EQ(r->progress, 0);
    EXPECT_FALSE(r->done);
    EXPECT_FALSE(reset);
}

TEST(TutorialRestoreTest, DoneLoadsDone) {
    const std::vector<std::vector<TutorialStep>> series = oneSeries();
    auto r = tutorialRestore(TutorialSaveState::Done, 0, "", 0, series);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_TRUE(r->done);
}

TEST(TutorialRestoreTest, DoneClampsToTheLastSeriesLastStep) {
    // V2-3: the done state is clamped to the last series' last step
    // (the apply-done discipline — the save's {} shape carries no
    // indexes, the clamp is the machine's canonical done form).
    const std::vector<std::vector<TutorialStep>> series = threeSeries();
    auto r = tutorialRestore(TutorialSaveState::Done, 0, "", 0, series);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_TRUE(r->done);
    EXPECT_EQ(r->seriesIndex, 2);
    EXPECT_EQ(r->stepIndex, 0); // the last step of the last series
}

TEST(TutorialRestoreTest, InProgressRestoresSeriesIndexAndCount) {
    // V2-3: the format-25 shape {series, id, count} restores the series
    // + the step + the progress exactly.
    const std::vector<std::vector<TutorialStep>> series = threeSeries();
    auto r = tutorialRestore(TutorialSaveState::InProgress, 1, "soil", 0,
                             series);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(r->seriesIndex, 1);
    EXPECT_EQ(r->stepIndex, 1);
    EXPECT_EQ(r->progress, 0);
    EXPECT_FALSE(r->done);
    r = tutorialRestore(TutorialSaveState::InProgress, 0, "chop", 2, series);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(r->seriesIndex, 0);
    EXPECT_EQ(r->stepIndex, 1);
    EXPECT_EQ(r->progress, 2);
}

TEST(TutorialRestoreTest, Format24ShapeDefaultsSeriesZeroValueFaithful) {
    // V2-3: a format 23/24 save carries {id, count} (the game passes
    // series 0 — the default). The series-0 data is byte-unchanged by
    // V2-3, so the restore lands on the SAME step at the SAME progress
    // bound (value-faithful, no migration — the UX-8a twin).
    const std::vector<std::vector<TutorialStep>> series = threeSeries();
    auto r = tutorialRestore(TutorialSaveState::InProgress, 0, "chop", 2,
                             series);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(r->seriesIndex, 0);
    EXPECT_EQ(r->stepIndex, 1); // the "chop" step of series 0
    EXPECT_EQ(r->progress, 2);
    EXPECT_FALSE(r->done);
}

TEST(TutorialRestoreTest, IdNotInThatSeriesResets) {
    // V2-3: the id exists in the DATA but NOT in the saved series — the
    // {series, id} pair is what the save names, so a cross-series id is
    // a reset (the id-reset semantics generalized to the pair).
    const std::vector<std::vector<TutorialStep>> series = threeSeries();
    bool reset = false;
    auto r = tutorialRestore(TutorialSaveState::InProgress, 1, "walk", 0,
                             series, &reset);
    ASSERT_TRUE(r.ok()) << "a cross-series id is a RESET, not a failure: "
                        << r.error().message;
    EXPECT_TRUE(reset);
    EXPECT_EQ(r->seriesIndex, 0);
    EXPECT_EQ(r->stepIndex, 0);
    EXPECT_EQ(r->progress, 0);
    EXPECT_FALSE(r->done);
}

TEST(TutorialRestoreTest, SeriesOutOfRangeResets) {
    // V2-3: the series is >= the series count (the data is content —
    // the series set may have shrunk): a RESET, not a failure (the
    // kickoff decision: a reported reset, the game logs it).
    const std::vector<std::vector<TutorialStep>> series = threeSeries();
    bool reset = false;
    auto r = tutorialRestore(TutorialSaveState::InProgress, 5, "walk", 0,
                             series, &reset);
    ASSERT_TRUE(r.ok()) << "a shrunken series set is a RESET, not a "
                        << "failure: "
                        << r.error().message;
    EXPECT_TRUE(reset);
    EXPECT_EQ(r->seriesIndex, 0);
    EXPECT_EQ(r->stepIndex, 0);
    EXPECT_EQ(r->progress, 0);
    EXPECT_FALSE(r->done);
    // A negative series (a hand-rolled file — the parse rejects it,
    // the pure function must stay safe) is out of range too.
    reset = false;
    r = tutorialRestore(TutorialSaveState::InProgress, -1, "walk", 0, series,
                        &reset);
    ASSERT_TRUE(r.ok());
    EXPECT_TRUE(reset);
}

TEST(TutorialRestoreTest, UnknownIdResetsToFirstStep) {
    const std::vector<std::vector<TutorialStep>> series = oneSeries();
    bool reset = false;
    auto r = tutorialRestore(TutorialSaveState::InProgress, 0, "gone", 1,
                             series, &reset);
    ASSERT_TRUE(r.ok()) << "a data rework is a RESET, not a failure: "
                        << r.error().message;
    EXPECT_TRUE(reset);
    EXPECT_EQ(r->seriesIndex, 0);
    EXPECT_EQ(r->stepIndex, 0);
    EXPECT_EQ(r->progress, 0);
    EXPECT_FALSE(r->done);
}

TEST(TutorialRestoreTest, ProgressAtOrAboveStepCountIsCorrupt) {
    const std::vector<std::vector<TutorialStep>> series = threeSeries();
    // "chop" (series 0) count 3
    auto r = tutorialRestore(TutorialSaveState::InProgress, 0, "chop", 3,
                             series);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("chop"), std::string::npos);
    r = tutorialRestore(TutorialSaveState::InProgress, 0, "chop", 42, series);
    ASSERT_FALSE(r.ok());
}

TEST(TutorialRestoreTest, NegativeProgressIsCorrupt) {
    const std::vector<std::vector<TutorialStep>> series = oneSeries();
    auto r = tutorialRestore(TutorialSaveState::InProgress, 0, "b", -1,
                             series);
    EXPECT_FALSE(r.ok());
}

// -- the config strict read ------------------------------------------------------

TEST(TutorialConfigTest, AbsentDefaultsToEnabled) {
    Config config; // empty
    auto r = tutorialEnabled(config);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_TRUE(*r);
}

TEST(TutorialConfigTest, ExplicitFalseDisables) {
    Config config;
    config.set("tutorial.enabled", Json(false));
    auto r = tutorialEnabled(config);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_FALSE(*r);
    Config config2;
    config2.set("tutorial.enabled", Json(true));
    EXPECT_TRUE(*tutorialEnabled(config2));
}

TEST(TutorialConfigTest, NonBoolIsNamedAndRejected) {
    Config number;
    number.set("tutorial.enabled", Json(1));
    auto r = tutorialEnabled(number);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("tutorial.enabled"), std::string::npos);
    Config text;
    text.set("tutorial.enabled", Json(std::string("yes")));
    EXPECT_FALSE(tutorialEnabled(text).ok());
}

// -- the shipped content pin (real assets) ---------------------------------------

TEST(TutorialContentTest, ShippedSeriesSetIsTheThreeJsonFiles) {
    // V2-3: the series collection is the data/tutorials directory — the
    // file-name BYTE ORDER is the series order (the registry
    // file-order paradigm): beginner (series 0, byte-unchanged by V2-3)
    // -> farming (series 1) -> kitchen (series 2).
    ShippedTutorials t;
    ASSERT_TRUE(t.load()) << "run from the repository root";
    ASSERT_EQ(t.files.size(), 3u);
    EXPECT_EQ(t.files[0], "beginner.json");
    EXPECT_EQ(t.files[1], "farming.json");
    EXPECT_EQ(t.files[2], "kitchen.json");
    ASSERT_EQ(t.series.size(), 3u);
    EXPECT_EQ(t.series[0].size(), 11u);
    EXPECT_EQ(t.series[1].size(), 7u);
    EXPECT_EQ(t.series[2].size(), 6u);
    // The 11 + 7 + 6 step ids are globally unique (the save's
    // {series, id} pair must name one step).
    auto g = checkSeriesGlobalIds(t.series);
    ASSERT_TRUE(g.ok()) << g.error().message;
}

TEST(TutorialContentTest, ShippedBeginnerIsTheElevenStepChain) {
    // UX-8a: the 7-step chain grew a planks step after chop (the
    // "wood -> planks -> table" cognition link the real-player feedback
    // was missing) + the table step reworded with the 2x2 layout. UX-9b:
    // the 8-step chain grew the coal/torch/light steps after spick (the
    // "fell into a dark cave" real-player feedback: the chain now
    // teaches a torch before the kill step). V2-3: the file is
    // byte-unchanged (the zero-change proof — the pins below are the
    // pre-V2-3 ones, re-run on the series-0 data).
    ShippedTutorials t;
    ASSERT_TRUE(t.load()) << "run from the repository root";
    const std::vector<TutorialStep>& steps = t.series[0];
    ASSERT_EQ(steps.size(), 11u)
        << "the shipped beginner.json is the 11-step "
           "chain (parse error above)";
    const std::vector<std::tuple<const char*, const char*, int>> pinned = {
        {"walk", "walk", 1},
        {"chop", "break.wood", 3},
        {"planks", "craft.planks", 1},
        {"table", "craft.crafting_table", 1},
        {"wpick", "craft.wooden_pickaxe", 1},
        {"stone", "break.stone", 5},
        {"spick", "craft.stone_pickaxe", 1},
        {"coal", "break.coal_ore", 1},
        {"torch", "craft.torch", 1},
        {"light", "place.torch", 1},
        {"kill", "kill.chicken", 1},
    };
    for (int i = 0; i < 11; ++i) {
        EXPECT_EQ(steps[static_cast<std::size_t>(i)].id,
                  std::get<0>(pinned[static_cast<std::size_t>(i)]))
            << "step " << i;
        EXPECT_EQ(steps[static_cast<std::size_t>(i)].goal,
                  std::get<1>(pinned[static_cast<std::size_t>(i)]))
            << "step " << i;
        EXPECT_EQ(steps[static_cast<std::size_t>(i)].count,
                  std::get<2>(pinned[static_cast<std::size_t>(i)]))
            << "step " << i;
    }
    // The planks step counts ONE event: the craft event is emitted at
    // the successful output click with n=1 (one click crafts the whole
    // 4-plank output, but the event counts as one — the UX-8a brief's
    // rule; count 4 would stall the chain).
    EXPECT_EQ(steps[2].count, 1);
    // The table step text carries the ASCII "2x2" (the multiplication
    // sign is outside the font set — the brief's rule).
    EXPECT_NE(steps[3].text.find("2x2"), std::string::npos);
    EXPECT_EQ(steps[3].text, "做 1 个工作台（4 木板排 2x2）");
    // The planks step text is pinned exactly too (the cognition link:
    // 1 wood becomes 4 planks).
    EXPECT_EQ(steps[2].text, "合成 4 个木板（1 木头变 4）");
    // The UX-8b tips: planks + table carry them; the UX-9b tips: coal +
    // torch + light carry them; the other eight do not (zero tip rows =
    // the v1.41.0 render).
    EXPECT_EQ(steps[2].tip, "按 E 打开面板，选木头点输出");
    EXPECT_EQ(steps[3].tip, "E 面板 2x2 格摆满木板，点输出");
    // The UX-9b step texts are pinned exactly (the torch chain: the
    // coal ore, the 1-coal + 1-stick craft, the placement).
    EXPECT_EQ(steps[7].text, "挖 1 个煤矿石");
    EXPECT_EQ(steps[7].tip, "煤矿石多在地下洞中，用石镐挖");
    EXPECT_EQ(steps[8].text, "合成 1 个火把（1 煤 + 1 木棍）");
    EXPECT_EQ(steps[8].tip, "木棍 = 2 木板（E 面板配方）");
    EXPECT_EQ(steps[9].text, "放 1 个火把");
    EXPECT_EQ(steps[9].tip, "暗处放火把照亮，敌对不在亮处生成");
    for (int i : {0, 1, 4, 5, 6, 10}) {
        EXPECT_TRUE(steps[static_cast<std::size_t>(i)].tip.empty())
            << "step " << i;
    }
}

TEST(TutorialContentTest, ShippedFarmingIsTheSevenStepChain) {
    // V2-3: the farming series (series 1) — grass seed -> farmland ->
    // plant -> harvest -> bread -> eat. The texts/tips are pinned
    // EXACTLY (the content is data; a reword is a conscious change).
    // The V2-3 design fix: the first five steps count 2 (wheat_7
    // drops 2 wheat but bread needs 3 — 2 harvests = 4 wheat = the
    // bread + 1 spare; count-1 throughout was a tutorial dead end).
    ShippedTutorials t;
    ASSERT_TRUE(t.load()) << "run from the repository root";
    ASSERT_EQ(t.files.size(), 3u);
    ASSERT_EQ(t.files[1], "farming.json"); // the byte-order position
    const std::vector<TutorialStep>& steps = t.series[1];
    ASSERT_EQ(steps.size(), 7u);
    const std::vector<std::tuple<const char*, const char*, int>> pinned = {
        {"seed", "pickup.wheat_1", 2},
        {"till", "craft.farmland", 2},
        {"soil", "place.farmland", 2},
        {"plant", "place.wheat_1", 2},
        {"harvest", "break.wheat_7", 2},
        {"bread", "craft.bread", 1},
        {"meal", "eat", 1},
    };
    for (int i = 0; i < 7; ++i) {
        EXPECT_EQ(steps[static_cast<std::size_t>(i)].id,
                  std::get<0>(pinned[static_cast<std::size_t>(i)]))
            << "step " << i;
        EXPECT_EQ(steps[static_cast<std::size_t>(i)].goal,
                  std::get<1>(pinned[static_cast<std::size_t>(i)]))
            << "step " << i;
        EXPECT_EQ(steps[static_cast<std::size_t>(i)].count,
                  std::get<2>(pinned[static_cast<std::size_t>(i)]))
            << "step " << i;
    }
    EXPECT_EQ(steps[0].text, "割草捡 2 颗小麦种子");
    EXPECT_EQ(steps[0].tip, "草方块有 1/8 概率掉种子");
    EXPECT_EQ(steps[1].text, "合成 2 块耕地（泥土变耕地）");
    EXPECT_EQ(steps[1].tip, "按 E 打开面板，选泥土点输出");
    EXPECT_EQ(steps[2].text, "放 2 块耕地");
    EXPECT_EQ(steps[3].text, "在耕地上种 2 颗种子");
    EXPECT_EQ(steps[3].tip, "选种子右键耕地");
    EXPECT_EQ(steps[4].text, "等小麦金黄（约 1 分钟）收 2 株");
    EXPECT_EQ(steps[4].tip, "未熟会掉回种子，可重种");
    EXPECT_EQ(steps[5].text, "合成 1 个面包（3 小麦）");
    EXPECT_EQ(steps[6].text, "吃 1 个面包");
    for (int i : {2, 5, 6}) {
        EXPECT_TRUE(steps[static_cast<std::size_t>(i)].tip.empty())
            << "step " << i;
    }
}

TEST(TutorialContentTest, ShippedKitchenIsTheSixStepChain) {
    // V2-3: the kitchen series (series 2) — stone -> furnace -> feed ->
    // smelt -> eat. The texts/tips are pinned EXACTLY.
    ShippedTutorials t;
    ASSERT_TRUE(t.load()) << "run from the repository root";
    ASSERT_EQ(t.files.size(), 3u);
    ASSERT_EQ(t.files[2], "kitchen.json"); // the byte-order position
    const std::vector<TutorialStep>& steps = t.series[2];
    ASSERT_EQ(steps.size(), 6u);
    const std::vector<std::tuple<const char*, const char*, int>> pinned = {
        {"cobble", "break.stone", 3},
        {"furnace", "craft.furnace", 1},
        {"set", "place.furnace", 1},
        {"feed", "interact.furnace", 1},
        {"cook", "smelt.cooked_chicken", 1},
        {"eat", "eat", 1},
    };
    for (int i = 0; i < 6; ++i) {
        EXPECT_EQ(steps[static_cast<std::size_t>(i)].id,
                  std::get<0>(pinned[static_cast<std::size_t>(i)]))
            << "step " << i;
        EXPECT_EQ(steps[static_cast<std::size_t>(i)].goal,
                  std::get<1>(pinned[static_cast<std::size_t>(i)]))
            << "step " << i;
        EXPECT_EQ(steps[static_cast<std::size_t>(i)].count,
                  std::get<2>(pinned[static_cast<std::size_t>(i)]))
            << "step " << i;
    }
    EXPECT_EQ(steps[0].text, "再挖 3 个石头（炉子要 8 锻石）");
    EXPECT_EQ(steps[0].tip, "基础链已挖 5 个");
    EXPECT_EQ(steps[1].text, "在工作台合成 1 个炉子");
    EXPECT_EQ(steps[1].tip, "8 锻石围一圈");
    EXPECT_EQ(steps[2].text, "放 1 个炉子");
    EXPECT_EQ(steps[3].text, "打开炉子，放燃料和鸡肉");
    EXPECT_EQ(steps[3].tip, "煤炭是燃料，杀鸡得鸡肉");
    EXPECT_EQ(steps[4].text, "等熔炼完成（约 10 秒）");
    EXPECT_EQ(steps[5].text, "吃 1 块熟鸡肉");
    for (int i : {2, 4, 5}) {
        EXPECT_TRUE(steps[static_cast<std::size_t>(i)].tip.empty())
            << "step " << i;
    }
}

TEST(TutorialContentTest, ShippedTextsStayInsideTheFontSet) {
    // The brief's red line: every text AND tip character (and the toast
    // text) of EVERY shipped series is either printable ASCII or a
    // member of the V2-3 422-glyph set — a missing glyph would render
    // as '?' for the player. UX-8b: the tips are checked per character
    // too (the tip row draws the same way as the target line).
    ShippedTutorials t;
    ASSERT_TRUE(t.load()) << "run from the repository root";
    for (const std::vector<TutorialStep>& series : t.series) {
        for (const TutorialStep& s : series) {
            EXPECT_TRUE(allCharsInFontSet(s.text)) << "step '" << s.id
                                                   << "': " << s.text;
            if (!s.tip.empty()) {
                EXPECT_TRUE(allCharsInFontSet(s.tip)) << "step '" << s.id
                                                      << "' tip: " << s.tip;
            }
        }
    }
    EXPECT_TRUE(allCharsInFontSet(kTutorialDoneText));
}

TEST(TutorialContentTest, ShippedGoalsSurviveTheRealInitCrossCheck) {
    RealRegistries r;
    ASSERT_TRUE(r.load()) << "run from the repository root";
    ShippedTutorials t;
    ASSERT_TRUE(t.load()) << "run from the repository root";
    ASSERT_EQ(t.series.size(), 3u);
    for (const auto& s : t.series) {
        auto v = validateTutorialGoals(s, r.items, r.blocks, r.mobs);
        ASSERT_TRUE(v.ok()) << v.error().message;
    }
    // The chains' targets are all reachable with shipped content:
    // wood (worldgen), planks (the wood -> 4 planks recipe), the six
    // craft outputs (the recipes exist — the init's recipe
    // cross-check owns their existence), stone + coal_ore (worldgen),
    // torch + stick (the recipes), chicken (the spawn config + the mob
    // registry). V2-3 additions: the wheat growth chain (the grass's
    // 1/8 chance_drop wheat_1, the farmland craft + place, the wheat_7
    // harvest, the bread craft), the furnace chain (the stone break ->
    // cobblestone drops, the furnace craft + place + interact, the
    // chicken smelt -> cooked_chicken).
    EXPECT_TRUE(r.blocks.findByName("wood") != nullptr);
    EXPECT_TRUE(r.items.findByName("planks") != nullptr);
    EXPECT_TRUE(r.blocks.findByName("stone") != nullptr);
    EXPECT_TRUE(r.items.findByName("cobblestone") != nullptr);
    EXPECT_TRUE(r.blocks.findByName("coal_ore") != nullptr);
    EXPECT_TRUE(r.items.findByName("crafting_table") != nullptr);
    EXPECT_TRUE(r.items.findByName("wooden_pickaxe") != nullptr);
    EXPECT_TRUE(r.items.findByName("stone_pickaxe") != nullptr);
    EXPECT_TRUE(r.items.findByName("torch") != nullptr);
    EXPECT_TRUE(r.items.findByName("stick") != nullptr);
    EXPECT_TRUE(r.mobs.findByName("chicken") != nullptr);
    // The V2-3 fact anchors (the design brief's verified facts, re-
    // pinned against the real registries):
    EXPECT_TRUE(r.blocks.findByName("wheat_1") != nullptr);
    EXPECT_TRUE(r.items.findByName("wheat_1") != nullptr);
    EXPECT_TRUE(r.blocks.findByName("wheat_7") != nullptr);
    EXPECT_TRUE(r.blocks.findByName("farmland") != nullptr);
    EXPECT_TRUE(r.items.findByName("farmland") != nullptr);
    EXPECT_TRUE(r.items.findByName("bread") != nullptr);
    EXPECT_TRUE(r.blocks.findByName("furnace") != nullptr);
    EXPECT_TRUE(r.items.findByName("furnace") != nullptr);
    EXPECT_TRUE(r.items.findByName("cooked_chicken") != nullptr);
}
