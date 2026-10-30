// UX-7 beginner tutorial tests: the strict parse (the closed goal
// vocabulary + the per-step field discipline, every failure named), the
// init cross-check (a qualified goal's name half must exist in the
// registry — real registries, the CRAFT-2 content-pin paradigm), the
// PURE step machine (the campfire pattern — the game's
// emitTutorialEvent is the thin adapter; the logic is owned here), the
// save-restore rules (format 23: the id reset, the count bound), the
// target line / toast constants, the config strict read, and the
// shipped content pin (beginner.json: the 7 steps exact + every text
// character inside the UX-6 392-glyph font set).
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
    return TutorialStep {id, id + " text", goal, count};
}

// The machine's fixture: three steps (a count-1 bare goal, a count-3
// qualified goal, a count-2 qualified goal — the last, so the done
// flip is reachable).
std::vector<TutorialStep> threeSteps() {
    return {step("a", "walk", 1), step("b", "break.wood", 3),
            step("c", "kill.chicken", 2)};
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

std::vector<TutorialStep> loadShippedTutorial() {
    AssetManager assets;
    if (!assets.setRoot("assets").ok()) {
        return {};
    }
    auto text = assets.loadText("data/tutorials/beginner.json");
    if (!text.ok()) {
        return {};
    }
    auto j = Json::parse(*text);
    if (!j.ok()) {
        return {};
    }
    auto steps = parseTutorial(*j);
    if (!steps.ok()) {
        return {};
    }
    return *steps;
}

// True when every codepoint of `text` is either a printable ASCII
// character (32..126 — the font's rows 4..15) or a member of the UX-6
// 392-glyph CJK set (kCjkGlyphs, codepoint-sorted — the binary search).
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
    // registry question).
    EXPECT_TRUE(withGoal("walk").ok());
    EXPECT_TRUE(withGoal("jump").ok());
    EXPECT_TRUE(withGoal("eat").ok());
    // All five prefixes with a name pass the grammar.
    EXPECT_TRUE(withGoal("craft.x").ok());
    EXPECT_TRUE(withGoal("place.x").ok());
    EXPECT_TRUE(withGoal("kill.x").ok());
    EXPECT_TRUE(withGoal("interact.x").ok());
}

// -- init cross-check (real registries) ---------------------------------------

TEST(TutorialValidateTest, ShippedGoalsExistInRealRegistries) {
    RealRegistries r;
    ASSERT_TRUE(r.load()) << "run from the repository root";
    auto steps = loadShippedTutorial();
    ASSERT_FALSE(steps.empty()) << "the shipped file must parse";
    auto v = validateTutorialGoals(steps, r.items, r.blocks, r.mobs);
    ASSERT_TRUE(v.ok()) << v.error().message;
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
    // A bare goal is not a registry question — it passes.
    EXPECT_TRUE(validateTutorialGoals({step("s", "walk", 1)}, r.items,
                                      r.blocks, r.mobs)
                    .ok());
}

// -- the pure step machine ----------------------------------------------------

TEST(TutorialApplyTest, FirstEventAdvancesCountOne) {
    const std::vector<TutorialStep> steps = threeSteps();
    const TutorialState s0 {}; // step 0, progress 0, not done
    const TutorialState s1 = tutorialApply(s0, "walk", 1, steps);
    EXPECT_FALSE(s1.done);
    EXPECT_EQ(s1.stepIndex, 1);
    EXPECT_EQ(s1.progress, 0);
}

TEST(TutorialApplyTest, MismatchedEventIgnored) {
    const std::vector<TutorialStep> steps = threeSteps();
    const TutorialState s0 {};
    const TutorialState s1 = tutorialApply(s0, "jump", 1, steps);
    EXPECT_EQ(s1.stepIndex, 0);
    EXPECT_EQ(s1.progress, 0);
    EXPECT_FALSE(s1.done);
    // A qualified mismatch (the right name, the wrong step) too.
    const TutorialState s2 = tutorialApply(s0, "break.wood", 1, steps);
    EXPECT_EQ(s2.stepIndex, 0);
    EXPECT_EQ(s2.progress, 0);
}

TEST(TutorialApplyTest, ExactCountAdvances) {
    const std::vector<TutorialStep> steps = threeSteps();
    TutorialState s {};
    s = tutorialApply(s, "break.wood", 1, steps); // (step 0 is walk —
    s.stepIndex = 1;                               // place it at "b")
    EXPECT_EQ(s.progress, 0);
    s = tutorialApply(s, "break.wood", 1, steps);
    EXPECT_EQ(s.stepIndex, 1);
    EXPECT_EQ(s.progress, 1);
    s = tutorialApply(s, "break.wood", 1, steps);
    EXPECT_EQ(s.stepIndex, 1);
    EXPECT_EQ(s.progress, 2);
    s = tutorialApply(s, "break.wood", 1, steps);
    EXPECT_EQ(s.stepIndex, 2); // the exact 3rd event advances
    EXPECT_EQ(s.progress, 0);
    EXPECT_FALSE(s.done);
}

TEST(TutorialApplyTest, NGreaterThanOneAdvancesOnce) {
    const std::vector<TutorialStep> steps = threeSteps();
    TutorialState s {};
    s.stepIndex = 1; // "b": break.wood, count 3
    // An n=5 event (the function supports it, the brief's case)
    // completes the step ONCE — the overshoot is not banked (progress
    // resets to 0, the next step starts clean).
    const TutorialState s2 = tutorialApply(s, "break.wood", 5, steps);
    EXPECT_EQ(s2.stepIndex, 2);
    EXPECT_EQ(s2.progress, 0);
    EXPECT_FALSE(s2.done);
}

TEST(TutorialApplyTest, LastStepCompletesToDone) {
    const std::vector<TutorialStep> steps = threeSteps();
    TutorialState s {};
    s.stepIndex = 2; // "c": kill.chicken, count 2
    s.progress = 1;
    const TutorialState s2 = tutorialApply(s, "kill.chicken", 1, steps);
    EXPECT_TRUE(s2.done);
    EXPECT_EQ(s2.progress, 0);
}

TEST(TutorialApplyTest, DoneIsImmune) {
    const std::vector<TutorialStep> steps = threeSteps();
    const TutorialState done {0, 0, true};
    const TutorialState s1 = tutorialApply(done, "walk", 1, steps);
    EXPECT_TRUE(s1.done);
    EXPECT_EQ(s1.stepIndex, 0);
    EXPECT_EQ(s1.progress, 0);
}

TEST(TutorialApplyTest, MultipleEventsSameFrameAccumulate) {
    const std::vector<TutorialStep> steps = threeSteps();
    TutorialState s {};
    s.stepIndex = 1; // "b": count 3
    s = tutorialApply(s, "break.wood", 1, steps); // event 1 of the frame
    s = tutorialApply(s, "break.wood", 1, steps); // event 2 of the frame
    EXPECT_EQ(s.stepIndex, 1);
    EXPECT_EQ(s.progress, 2);
    s = tutorialApply(s, "break.wood", 1, steps); // the frame's 3rd
    EXPECT_EQ(s.stepIndex, 2);
    EXPECT_EQ(s.progress, 0);
}

TEST(TutorialApplyTest, DegenerateInputsLeaveStateAlone) {
    const std::vector<TutorialStep> steps = threeSteps();
    const TutorialState s0 {};
    // An empty step list (defensive — the parse rejects one).
    EXPECT_EQ(tutorialApply(s0, "walk", 1, {}).stepIndex, 0);
    EXPECT_EQ(tutorialApply(s0, "walk", 1, {}).progress, 0);
    // n <= 0 (the game never emits one; the machine must not regress).
    EXPECT_EQ(tutorialApply(s0, "walk", 0, steps).progress, 0);
    EXPECT_EQ(tutorialApply(s0, "walk", -2, steps).progress, 0);
}

// -- the target line + the toast constant --------------------------------------

TEST(TutorialUiTest, TargetLineCountOneIsTextOnly) {
    const std::vector<TutorialStep> steps = threeSteps();
    const TutorialState s0 {}; // "a" (walk, count 1)
    EXPECT_EQ(tutorialTargetLine(s0, steps), "a text");
}

TEST(TutorialUiTest, TargetLineCountThreeCarriesProgress) {
    const std::vector<TutorialStep> steps = threeSteps();
    TutorialState s {};
    s.stepIndex = 1; // "b": break.wood, count 3
    s.progress = 1;
    EXPECT_EQ(tutorialTargetLine(s, steps), "b text 1/3");
    s.progress = 0;
    EXPECT_EQ(tutorialTargetLine(s, steps), "b text 0/3");
}

TEST(TutorialUiTest, TargetLineDoneIsEmpty) {
    const std::vector<TutorialStep> steps = threeSteps();
    const TutorialState done {0, 0, true};
    EXPECT_TRUE(tutorialTargetLine(done, steps).empty());
}

TEST(TutorialUiTest, ToastWindowIs120Frames) {
    EXPECT_EQ(kTutorialToastFrames, 120);
    // The toast text is in the font set (the brief's character audit).
    EXPECT_TRUE(allCharsInFontSet(kTutorialDoneText));
    EXPECT_EQ(std::string(kTutorialDoneText), "全部完成！");
}

// -- the save-restore rules -----------------------------------------------------

TEST(TutorialRestoreTest, AbsentLoadsFresh) {
    const std::vector<TutorialStep> steps = threeSteps();
    bool reset = true;
    auto r = tutorialRestore(TutorialSaveState::Absent, "", 0, steps, &reset);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(r->stepIndex, 0);
    EXPECT_EQ(r->progress, 0);
    EXPECT_FALSE(r->done);
    EXPECT_FALSE(reset);
}

TEST(TutorialRestoreTest, DoneLoadsDone) {
    const std::vector<TutorialStep> steps = threeSteps();
    auto r = tutorialRestore(TutorialSaveState::Done, "", 0, steps);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_TRUE(r->done);
}

TEST(TutorialRestoreTest, InProgressRestoresIndexAndCount) {
    const std::vector<TutorialStep> steps = threeSteps();
    auto r = tutorialRestore(TutorialSaveState::InProgress, "b", 2, steps);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(r->stepIndex, 1);
    EXPECT_EQ(r->progress, 2);
    EXPECT_FALSE(r->done);
}

TEST(TutorialRestoreTest, UnknownIdResetsToFirstStep) {
    const std::vector<TutorialStep> steps = threeSteps();
    bool reset = false;
    auto r = tutorialRestore(TutorialSaveState::InProgress, "gone", 1, steps,
                             &reset);
    ASSERT_TRUE(r.ok()) << "a data rework is a RESET, not a failure: "
                        << r.error().message;
    EXPECT_TRUE(reset);
    EXPECT_EQ(r->stepIndex, 0);
    EXPECT_EQ(r->progress, 0);
    EXPECT_FALSE(r->done);
}

TEST(TutorialRestoreTest, ProgressAtOrAboveStepCountIsCorrupt) {
    const std::vector<TutorialStep> steps = threeSteps(); // "b" count 3
    auto r = tutorialRestore(TutorialSaveState::InProgress, "b", 3, steps);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("b"), std::string::npos);
    r = tutorialRestore(TutorialSaveState::InProgress, "b", 42, steps);
    ASSERT_FALSE(r.ok());
}

TEST(TutorialRestoreTest, NegativeProgressIsCorrupt) {
    const std::vector<TutorialStep> steps = threeSteps();
    auto r = tutorialRestore(TutorialSaveState::InProgress, "b", -1, steps);
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

TEST(TutorialContentTest, ShippedBeginnerIsTheSevenStepChain) {
    auto steps = loadShippedTutorial();
    ASSERT_EQ(steps.size(), 7u) << "the shipped beginner.json is the 7-step "
                                   "chain (parse error above)";
    const std::vector<std::tuple<const char*, const char*, int>> pinned = {
        {"walk", "walk", 1},
        {"chop", "break.wood", 3},
        {"table", "craft.crafting_table", 1},
        {"wpick", "craft.wooden_pickaxe", 1},
        {"stone", "break.stone", 5},
        {"spick", "craft.stone_pickaxe", 1},
        {"kill", "kill.chicken", 1},
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
}

TEST(TutorialContentTest, ShippedTextsStayInsideTheFontSet) {
    // The brief's red line: every text character (and the toast text)
    // is either printable ASCII or a member of the UX-6 392-glyph set —
    // a missing glyph would render as '?' for the player.
    auto steps = loadShippedTutorial();
    ASSERT_EQ(steps.size(), 7u);
    for (const TutorialStep& s : steps) {
        EXPECT_TRUE(allCharsInFontSet(s.text)) << "step '" << s.id
                                               << "': " << s.text;
    }
    EXPECT_TRUE(allCharsInFontSet(kTutorialDoneText));
}

TEST(TutorialContentTest, ShippedGoalsSurviveTheRealInitCrossCheck) {
    RealRegistries r;
    ASSERT_TRUE(r.load()) << "run from the repository root";
    auto steps = loadShippedTutorial();
    ASSERT_EQ(steps.size(), 7u);
    auto v = validateTutorialGoals(steps, r.items, r.blocks, r.mobs);
    ASSERT_TRUE(v.ok()) << v.error().message;
    // The chain's targets are all reachable with shipped content:
    // wood (worldgen), the three craft outputs (the recipes exist —
    // the init's recipe cross-check owns their existence), stone
    // (worldgen), chicken (the spawn config + the mob registry).
    EXPECT_TRUE(r.blocks.findByName("wood") != nullptr);
    EXPECT_TRUE(r.blocks.findByName("stone") != nullptr);
    EXPECT_TRUE(r.items.findByName("crafting_table") != nullptr);
    EXPECT_TRUE(r.items.findByName("wooden_pickaxe") != nullptr);
    EXPECT_TRUE(r.items.findByName("stone_pickaxe") != nullptr);
    EXPECT_TRUE(r.mobs.findByName("chicken") != nullptr);
}
