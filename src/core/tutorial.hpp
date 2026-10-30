// UX-7: the beginner tutorial (the objective-style onboarding guide).
//
// The content is DATA (assets/data/tutorials/beginner.json — a step list,
// each step {id, text, goal, count}; strict parsing with the project's
// naming discipline). The step machine is a PURE function (the campfire
// pattern — the unit tests own it). The game is the thin adapter: one
// emitTutorialEvent choke point per event class, fed by frame-locked
// events only (hard rule 8 — the tutorial has NO real-dt component), so
// a no-input run emits zero events and the state never moves (the base
// gate's structural immunity).
//
// The goal vocabulary is CLOSED: the bare words (walk / jump / eat) or
// "<prefix>.<name>" with prefix in craft / break / place / kill /
// interact. Parse enforces the GRAMMAR; the name half's existence is
// the init cross-check's job (craft -> item name, break / place /
// interact -> block name, kill -> mob name — the grows_to precedent).
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "core/config.hpp"
#include "core/error.hpp"
#include "core/json.hpp"

namespace mc {

class BlockRegistry;
class ItemRegistry;
class MobRegistry;

// One tutorial step (the parsed data). `goal` is the closed-vocabulary
// event name the step waits for; `count` is the number of such events
// that complete it (>= 1). `text` is the HUD line (Chinese, every
// character inside the UX-6 392-glyph font set — the content pin test
// checks it against the real charset).
struct TutorialStep {
    std::string id;   // non-empty, unique within the file
    std::string text; // the HUD line (non-empty)
    std::string goal; // "walk" | "jump" | "eat" | "<prefix>.<name>"
    int count = 0;    // [1, ...]
};

// The player's progress through the step list (the frame-locked state;
// format 23 carries it as {step id, progress} or {} (done/skipped)).
// stepIndex is valid in [0, steps.size()) while !done.
struct TutorialState {
    int stepIndex = 0;
    int progress = 0;
    bool done = false;
};

// The save-file state of the tutorial segment (format 23): Absent = the
// file predates format 23 (the system did not exist — a fresh start);
// Done = the empty object (finished or skipped); InProgress = the
// {id, count} shape. The id + the count-bound cross-checks run at GAME
// load (tutorialRestore — the tutorial data is not a parseSave
// parameter, the validateStats pattern).
enum class TutorialSaveState { Absent, Done, InProgress };

// The completion-toast window, in FRAMES (the frame-locked count: the
// done flip / the T skip arms it to this value, the update decrements it
// once per frame, the render draws while > 0). 120 = 2 s at 60 fps.
inline constexpr int kTutorialToastFrames = 120;

// The completion-toast text. The brief's "教程完成！" fine-tuned — 教 and
// 程 are outside the UX-6 392-glyph font set, "全部完成！" is in-set and
// semantically equivalent (the content pin test asserts the membership).
inline constexpr const char* kTutorialDoneText = "全部完成！";

// The closed goal vocabulary: the bare (unqualified) event words, and
// the qualified form's five prefixes (each followed by "." + a
// registry name).
inline constexpr const char* const kTutorialBareGoals[] = {
    "walk", "jump", "eat"};
inline constexpr std::size_t kTutorialBareGoalCount =
    sizeof(kTutorialBareGoals) / sizeof(kTutorialBareGoals[0]);
inline constexpr const char* const kTutorialGoalPrefixes[] = {
    "craft", "break", "place", "kill", "interact"};
inline constexpr std::size_t kTutorialGoalPrefixCount =
    sizeof(kTutorialGoalPrefixes) / sizeof(kTutorialGoalPrefixes[0]);

// The strict read of tutorial.enabled (the SAVE-1 discipline — the
// Config getters would SILENTLY fall back to the default on a type
// mismatch, so the raw value is read): absent key = true (the code
// default, the render.fog precedent — the committed config.json carries
// no tutorial section); a present key must be a JSON boolean; anything
// else is named and rejected at init.
Result<bool> tutorialEnabled(const Config& config);

// Strict parse of a tutorial file: the top level is an object holding
// EXACTLY the "steps" key (a non-empty array); each step is an object
// holding EXACTLY {id, text, goal, count} (every deviation named
// "tutorial 'steps'[i]" / "'steps'[i].<key>"); id is a non-empty string,
// unique in the file; text a non-empty string; goal in the closed
// grammar; count a whole number >= 1.
Result<std::vector<TutorialStep>> parseTutorial(const Json& def);

// The init cross-check (the grows_to precedent — steps in file order,
// the first failing goal names its step + the missing name): every
// qualified goal's name half must exist — "craft" -> an item, "break" /
// "place" / "interact" -> a block, "kill" -> a mob. Bare goals pass.
Result<void> validateTutorialGoals(const std::vector<TutorialStep>& steps,
                                   const ItemRegistry& items,
                                   const BlockRegistry& blocks,
                                   const MobRegistry& mobs);

inline Result<bool> tutorialEnabled(const Config& config) {
    const Json* j = config.at("tutorial.enabled");
    if (j == nullptr) {
        return true; // absent: the code default (the render.fog precedent)
    }
    if (!j->isBool()) {
        return MC_FAIL("config tutorial.enabled must be a boolean "
                       "(true/false), got a non-boolean");
    }
    return j->asBool();
}

// The step machine (PURE — zero side effects; the unit tests own it).
// `n` events named `name` on `state`: done -> unchanged; a non-matching
// event (or n <= 0, or an empty step list) -> unchanged; the current
// step's goal matching -> progress += n; progress reaching the step's
// count -> the step advances (index + 1, progress 0) — advancing past
// the last step sets done.
TutorialState tutorialApply(const TutorialState& state,
                            const std::string& name, int n,
                            const std::vector<TutorialStep>& steps);

// The HUD target line: the current step's text, plus " progress/count"
// (e.g. " 1/3") when the step counts more than one event — a count-1
// step shows the text only (the brief's rule). Empty string when done
// (the caller draws the toast text instead).
std::string tutorialTargetLine(const TutorialState& state,
                               const std::vector<TutorialStep>& steps);

// The save segment's restore (the game load's cross-check + migration —
// the pure half; the game logs the reset and owns the state write):
//   - InProgress: the id must name a step of the CURRENT data, else the
//     result RESETS to the first step (the data is content, the save is
//     state — the kickoff decision: a reported reset, not a load
//     failure; `reset` carries the flag for the game's info log). The
//     progress must sit strictly BELOW the step's count (a completed
//     step would have advanced — corruption, named rejection).
//   - Done: the finished state. Absent: fresh (the system did not
//     exist — the format <= 22 migration, no data needed).
// `steps` must be non-empty (the parse rejects an empty list — the
// guard is defensive).
Result<TutorialState> tutorialRestore(TutorialSaveState saved,
                                      const std::string& id,
                                      int progress,
                                      const std::vector<TutorialStep>& steps,
                                      bool* reset = nullptr);

} // namespace mc
