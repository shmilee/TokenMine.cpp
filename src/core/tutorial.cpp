#include "core/tutorial.hpp"

#include <set>

#include "block/block_registry.hpp"
#include "entity/mob_registry.hpp"
#include "item/item_registry.hpp"

namespace mc {

namespace {

// The strict key check (save.cpp's rejectUnknownFields is file-local —
// the tutorial module carries its own twin): every member of `obj` must
// be in `allowed` (the first foreign key is named, the `what` prefix
// carries the file path).
bool checkKeys(const Json& obj, const std::set<std::string>& allowed,
               const std::string& what, std::string* error) {
    for (const auto& [key, value] : obj.members()) {
        (void)value;
        if (allowed.count(key) == 0) {
            *error = what + " holds the unknown key '" + key + "'";
            return false;
        }
    }
    return true;
}

bool isWholeNumber(const Json& v, double* out) {
    if (!v.isNumber()) {
        return false;
    }
    const double d = v.asDouble();
    if (d != std::floor(d)) {
        return false;
    }
    *out = d;
    return true;
}

// The closed goal grammar: a bare word, or "<prefix>.<name>" with the
// prefix one of the five and a non-empty, dot-free name. Returns false
// (naming the bad goal) when `goal` is outside the grammar.
bool goalGrammarOk(const std::string& goal, std::string* error) {
    for (const char* const bare : kTutorialBareGoals) {
        if (goal == bare) {
            return true;
        }
    }
    const auto dot = goal.find('.');
    if (dot == std::string::npos) {
        *error = " (bare goals are walk / jump / eat; qualified goals "
                 "are <craft|break|place|kill|interact>.<name>)";
        return false;
    }
    const std::string prefix = goal.substr(0, dot);
    bool prefixKnown = false;
    for (const char* const p : kTutorialGoalPrefixes) {
        if (prefix == p) {
            prefixKnown = true;
            break;
        }
    }
    const std::string name = goal.substr(dot + 1);
    if (!prefixKnown || name.empty() || name.find('.') != std::string::npos) {
        *error = " (a qualified goal is <craft|break|place|kill|"
                 "interact>.<name>, the name dot-free and non-empty)";
        return false;
    }
    return true;
}

} // namespace

Result<std::vector<TutorialStep>> parseTutorial(const Json& def) {
    if (!def.isObject()) {
        return MC_FAIL("tutorial file must be a JSON object "
                       "{\"steps\": [...]}");
    }
    if (def.members().size() != 1 || def.find("steps") == nullptr) {
        return MC_FAIL("tutorial file must hold exactly the 'steps' key");
    }
    const Json* steps = def.find("steps");
    if (!steps->isArray()) {
        return MC_FAIL("tutorial 'steps' must be an array");
    }
    if (steps->size() == 0) {
        return MC_FAIL("tutorial 'steps' must be non-empty");
    }
    std::vector<TutorialStep> out;
    std::set<std::string> seenIds;
    for (std::size_t i = 0; i < steps->size(); ++i) {
        const Json& e = steps->at(i);
        const std::string what = "tutorial 'steps'[" + std::to_string(i) + "]";
        if (!e.isObject()) {
            return MC_FAIL(what +
                           " must be an object {id, text, goal, count} "
                           "(+ the optional tip)");
        }
        std::string keyError;
        if (!checkKeys(e, {"id", "text", "goal", "count", "tip"}, what,
                       &keyError)) {
            return MC_FAIL(keyError);
        }
        const Json* id = e.find("id");
        if (id == nullptr || !id->isString() || id->asString().empty()) {
            return MC_FAIL(what + " requires a non-empty string 'id'");
        }
        if (!seenIds.insert(id->asString()).second) {
            return MC_FAIL(what + " holds the duplicate step id '" +
                           id->asString() + "'");
        }
        const Json* text = e.find("text");
        if (text == nullptr || !text->isString() ||
            text->asString().empty()) {
            return MC_FAIL(what + " requires a non-empty string 'text'");
        }
        const Json* goal = e.find("goal");
        if (goal == nullptr || !goal->isString() || goal->asString().empty()) {
            return MC_FAIL(what + " requires a non-empty string 'goal'");
        }
        std::string grammarError;
        if (!goalGrammarOk(goal->asString(), &grammarError)) {
            return MC_FAIL(what + " goal '" + goal->asString() + "' is not "
                           "in the closed vocabulary" + grammarError);
        }
        const Json* count = e.find("count");
        if (count == nullptr) {
            return MC_FAIL(what + " requires 'count' (a whole number >= 1)");
        }
        double d = 0.0;
        if (!isWholeNumber(*count, &d) || d < 1.0) {
            return MC_FAIL(what + " 'count' must be a whole number >= 1, "
                           "got " +
                           (count->isNumber()
                                ? std::to_string(count->asDouble())
                                : "a non-number"));
        }
        // UX-8b: the optional tip — when present it must be a NON-EMPTY
        // string (an explicit "" is named and rejected — the ammo
        // discipline, the sound_material precedent); absent = no tip.
        const Json* tip = e.find("tip");
        std::string tipStr;
        if (tip != nullptr) {
            if (!tip->isString()) {
                return MC_FAIL(what + " 'tip' must be a string, got a "
                               "non-string");
            }
            if (tip->asString().empty()) {
                return MC_FAIL(what + " 'tip' must be a non-empty string "
                               "(an explicit \"\" is rejected)");
            }
            tipStr = tip->asString();
        }
        out.push_back(TutorialStep {id->asString(), text->asString(),
                                    goal->asString(), static_cast<int>(d),
                                    tipStr});
    }
    return out;
}

Result<void> validateTutorialGoals(const std::vector<TutorialStep>& steps,
                                   const ItemRegistry& items,
                                   const BlockRegistry& blocks,
                                   const MobRegistry& mobs) {
    for (const TutorialStep& s : steps) {
        const std::string& goal = s.goal;
        const auto dot = goal.find('.');
        if (dot == std::string::npos) {
            continue; // a bare goal (the grammar is parse-checked)
        }
        const std::string prefix = goal.substr(0, dot);
        const std::string name = goal.substr(dot + 1);
        const char* kind = nullptr;
        bool exists = false;
        if (prefix == "craft") {
            kind = "item";
            exists = items.findByName(name) != nullptr;
        } else if (prefix == "break" || prefix == "place" ||
                   prefix == "interact") {
            kind = "block";
            exists = blocks.findByName(name) != nullptr;
        } else { // "kill" (the only remaining prefix — the grammar)
            kind = "mob";
            exists = mobs.findByName(name) != nullptr;
        }
        if (!exists) {
            return MC_FAIL("tutorial step '" + s.id + "' goal '" + goal +
                           "' names unknown " + kind + " '" + name + "'");
        }
    }
    return {};
}

TutorialState tutorialApply(const TutorialState& state,
                            const std::string& name, int n,
                            const std::vector<TutorialStep>& steps) {
    TutorialState out = state;
    if (out.done || steps.empty() || n <= 0 ||
        out.stepIndex < 0 ||
        out.stepIndex >= static_cast<int>(steps.size())) {
        return out;
    }
    const TutorialStep& s = steps[static_cast<std::size_t>(out.stepIndex)];
    if (s.goal != name) {
        return out;
    }
    out.progress += n;
    if (out.progress >= s.count) {
        out.stepIndex += 1;
        out.progress = 0;
        if (out.stepIndex >= static_cast<int>(steps.size())) {
            out.stepIndex = static_cast<int>(steps.size()) - 1;
            out.done = true;
        }
    }
    return out;
}

std::string tutorialTargetLine(const TutorialState& state,
                               const std::vector<TutorialStep>& steps) {
    if (state.done || steps.empty() || state.stepIndex < 0 ||
        state.stepIndex >= static_cast<int>(steps.size())) {
        return std::string();
    }
    const TutorialStep& s = steps[static_cast<std::size_t>(state.stepIndex)];
    std::string line = s.text;
    if (s.count > 1) {
        line += " " + std::to_string(state.progress) + "/" +
                std::to_string(s.count);
    }
    return line;
}

std::string tutorialTipLine(const TutorialState& state,
                            const std::vector<TutorialStep>& steps) {
    // The same guards as tutorialTargetLine: done / out-of-range / an
    // empty list (defensive — the parse rejects one) carry no tip row.
    if (state.done || steps.empty() || state.stepIndex < 0 ||
        state.stepIndex >= static_cast<int>(steps.size())) {
        return std::string();
    }
    return steps[static_cast<std::size_t>(state.stepIndex)].tip;
}

Result<TutorialState> tutorialRestore(TutorialSaveState saved,
                                      const std::string& id,
                                      int progress,
                                      const std::vector<TutorialStep>& steps,
                                      bool* reset) {
    if (reset != nullptr) {
        *reset = false;
    }
    if (saved == TutorialSaveState::Done) {
        return TutorialState {0, 0, true};
    }
    if (saved == TutorialSaveState::Absent) {
        return TutorialState {}; // fresh (the system did not exist)
    }
    if (steps.empty()) {
        return MC_FAIL("tutorial restore: the step data is empty (the "
                       "parse rejects an empty file — a contract "
                       "violation)");
    }
    if (progress < 0) {
        return MC_FAIL("save 'player.tutorial' count " +
                       std::to_string(progress) + " is negative");
    }
    int idx = -1;
    for (int i = 0; i < static_cast<int>(steps.size()); ++i) {
        if (steps[static_cast<std::size_t>(i)].id == id) {
            idx = i;
            break;
        }
    }
    if (idx < 0) {
        // The saved step is no longer in the CURRENT data (the data is
        // content — it may have been reworked): reset to the first step
        // (the kickoff decision: a reported reset, not a load failure).
        if (reset != nullptr) {
            *reset = true;
        }
        return TutorialState {};
    }
    if (progress >= steps[static_cast<std::size_t>(idx)].count) {
        return MC_FAIL("save 'player.tutorial' holds progress " +
                       std::to_string(progress) + " for step '" + id +
                       "' whose count is " +
                       std::to_string(steps[static_cast<std::size_t>(idx)]
                                          .count) +
                       " (a completed step must have advanced)");
    }
    return TutorialState {idx, progress, false};
}

} // namespace mc
