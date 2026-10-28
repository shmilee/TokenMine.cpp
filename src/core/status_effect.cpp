#include "core/status_effect.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <set>

#include "core/asset_manager.hpp"
#include "core/logger.hpp"

namespace mc {

namespace {

// Reads a required number from `def` and checks its range: (0, hi] with
// allowZero = false, [0, hi] with allowZero = true.
Result<void> readNumber(const Json& def, const char* key, double hi,
                        float& out, const std::string& what, bool allowZero) {
    const Json* v = def.find(key);
    if (v == nullptr || !v->isNumber()) {
        return MC_FAIL(what + " requires a number '" + std::string(key) +
                       "'");
    }
    const double d = v->asDouble();
    const bool inRange = allowZero ? (d >= 0.0 && d <= hi)
                                   : (d > 0.0 && d <= hi);
    if (!inRange) {
        return MC_FAIL(what + " '" + std::string(key) + "' must be in " +
                       (allowZero ? "[0, " : "(0, ") + std::to_string(hi) +
                       "], got " + std::to_string(d));
    }
    out = static_cast<float>(d);
    return {};
}

} // namespace

Result<void> EffectRegistry::parseEffect(const Json& def, Effect& out) {
    if (!def.isObject()) {
        return MC_FAIL("effect definition must be a JSON object");
    }
    const Json* id = def.find("id");
    if (id == nullptr || !id->isString() || id->asString().empty()) {
        return MC_FAIL("effect definition requires a non-empty string "
                       "'id'");
    }
    out.name = id->asString();

    static const std::set<std::string> kFields {"id", "tick_type",
                                                "tick_amount",
                                                "tick_interval",
                                                "min_health",
                                                "melee_penalty",
                                                "absorb_hp",
                                                "negative", "icon",
                                                "ignites_on_lava",
                                                "extinguished_by_water",
                                                // Phase 84: the beacon's
                                                // other primary effects.
                                                "speed_multiplier",
                                                "melee_bonus",
                                                "damage_reduction",
                                                // SURV-4: the potion's
                                                // instant heal / damage.
                                                "instant_hp",
                                                // SURV-5: the mob carrier
                                                // (affects_mobs + the
                                                // mob-side floor + the
                                                // undead exemption).
                                                "affects_mobs",
                                                "min_health_mob",
                                                "undead_exempt",
                                                // SURV-6: the effect tiers
                                                // (the amplifier levels).
                                                "tier_max",
                                                "tier_steps"};
    for (const auto& [key, value] : def.members()) {
        if (kFields.count(key) == 0) {
            return MC_FAIL("effect '" + out.name + "' has an unknown field "
                           "'" + key + "'");
        }
    }

    const Json* tt = def.find("tick_type");
    if (tt != nullptr) {
        if (!tt->isString()) {
            return MC_FAIL("effect '" + out.name + "': 'tick_type' must be "
                           "a string (\"damage\" or \"heal\")");
        }
        if (tt->asString() == "damage") {
            out.tick = Effect::Tick::Damage;
        } else if (tt->asString() == "heal") {
            out.tick = Effect::Tick::Heal;
        } else {
            return MC_FAIL("effect '" + out.name + "': 'tick_type' must be "
                           "\"damage\" or \"heal\", got '" +
                           tt->asString() + "'");
        }
        auto r = readNumber(def, "tick_amount", 10.0, out.tickAmount,
                            "effect '" + out.name + "'", false);
        MC_TRY(r);
        r = readNumber(def, "tick_interval", 100.0, out.tickInterval,
                       "effect '" + out.name + "'", false);
        MC_TRY(r);
        if (out.tick == Effect::Tick::Damage) {
            // The health floor: v1 is the MC poison semantics — 0 (no
            // floor, the damage may kill) or 1 (the player is kept at 1
            // HP; the effect never kills).
            if (def.find("min_health") != nullptr) {
                auto mr = readNumber(def, "min_health", 1.0, out.minHealth,
                                     "effect '" + out.name + "'", true);
                MC_TRY(mr);
            }
            // SURV-5: the MOB-side floor — the twin of min_health for the
            // mob carrier. Integer 0 (no floor: a damage tick may kill
            // the mob — poison, Minecraft's dual-target rule) or 1 (kept
            // at 1: never kills). ABSENT = the mob floor follows the
            // player's min_health (every pre-SURV-5 effect). A heal tick
            // cannot floor health, so it is damage-only like min_health.
            if (const Json* mm = def.find("min_health_mob")) {
                if (!mm->isNumber() || mm->asDouble() !=
                                            std::floor(mm->asDouble())) {
                    return MC_FAIL("effect '" + out.name +
                                   "': 'min_health_mob' must be an "
                                   "integer (0 or 1), got " +
                                   (mm->isNumber()
                                        ? std::to_string(mm->asDouble())
                                        : std::string("non-number")));
                }
                const double d = mm->asDouble();
                if (d != 0.0 && d != 1.0) {
                    return MC_FAIL("effect '" + out.name +
                                   "': 'min_health_mob' must be 0 (no "
                                   "floor, the mob can die) or 1 (kept "
                                   "at 1), got " + std::to_string(d));
                }
                out.minHealthMob = static_cast<int>(d);
            }
        } else if (def.find("min_health") != nullptr ||
                   def.find("min_health_mob") != nullptr) {
            return MC_FAIL("effect '" + out.name +
                           "': 'min_health' / 'min_health_mob' are only "
                           "valid for a \"damage\" tick_type (a heal tick "
                           "cannot floor health)");
        }
    } else if (def.find("tick_amount") != nullptr ||
               def.find("tick_interval") != nullptr) {
        return MC_FAIL("effect '" + out.name + "': 'tick_amount' / "
                       "'tick_interval' require 'tick_type'");
    }

    const Json* mp = def.find("melee_penalty");
    if (mp != nullptr) {
        auto r = readNumber(def, "melee_penalty", 100.0, out.meleePenalty,
                            "effect '" + out.name + "'", true);
        MC_TRY(r);
    }

    const Json* ah = def.find("absorb_hp");
    if (ah != nullptr) {
        // Integer HP points in [1, 64] (the layer is whole hearts/HP; a
        // fractional layer would fight the 2-HP-per-icon HUD strip).
        if (!ah->isNumber() || ah->asDouble() != std::floor(ah->asDouble()) ||
            ah->asDouble() < 1.0 || ah->asDouble() > 64.0) {
            return MC_FAIL("effect '" + out.name + "': 'absorb_hp' must be "
                           "an integer in [1, 64], got " +
                           std::to_string(ah->asDouble()));
        }
        out.absorbHp = static_cast<float>(ah->asDouble());
    }

    // Phase 84: the modifier fields (the beacon's other primary effects).
    const Json* sm = def.find("speed_multiplier");
    if (sm != nullptr) {
        if (!sm->isNumber() || sm->asDouble() <= 1.0 || sm->asDouble() > 3.0) {
            return MC_FAIL("effect '" + out.name + "': 'speed_multiplier' "
                           "must be a number in (1.0, 3.0], got " +
                           std::to_string(sm->asDouble()));
        }
        out.speedMultiplier = static_cast<float>(sm->asDouble());
    }
    const Json* mb = def.find("melee_bonus");
    if (mb != nullptr) {
        auto r = readNumber(def, "melee_bonus", 100.0, out.meleeBonus,
                            "effect '" + out.name + "'", true);
        MC_TRY(r);
    }
    const Json* dr = def.find("damage_reduction");
    if (dr != nullptr) {
        if (!dr->isNumber() || dr->asDouble() < 0.0 ||
            dr->asDouble() >= 1.0) {
            return MC_FAIL("effect '" + out.name + "': 'damage_reduction' "
                           "must be a number in [0.0, 1.0), got " +
                           std::to_string(dr->asDouble()));
        }
        out.damageReduction = static_cast<float>(dr->asDouble());
    }

    // SURV-6: the effect tiers (the Minecraft amplifier levels I/II/III).
    // `tier_max` (int in [1, 3], absent = 1 = the pre-SURV-6 single tier)
    // counts the tiers a grant may use (0-based: tier < tierMax).
    const Json* tm = def.find("tier_max");
    if (tm != nullptr) {
        if (!tm->isNumber() || tm->asDouble() != std::floor(tm->asDouble()) ||
            tm->asDouble() < 1.0 || tm->asDouble() > 3.0) {
            return MC_FAIL("effect '" + out.name + "': 'tier_max' must be "
                           "an integer in [1, 3], got " +
                           (tm->isNumber() ? std::to_string(tm->asDouble())
                                           : "a non-number"));
        }
        out.tierMax = static_cast<int>(tm->asDouble());
    }
    // `tier_steps` (object, absent = all steps 0): the per-tier increments
    // of the TIERABLE modifier fields only (the five data fields — a step
    // for a tick/instant field is rejected: those forms are single-tier,
    // the documented deviation). Each step is a number >= 0 (the tiers are
    // monotonically stronger — Minecraft). Pairing rule: a NON-ZERO step
    // requires its base field present (the fast_against discipline — a
    // step on an absent base is dead data). No-op rule: tierMax > 1 with
    // every step 0 is rejected (a tier set that changes nothing is a
    // typo — the no-op rule).
    float stepSm = 0.0f, stepAh = 0.0f, stepMb = 0.0f, stepMpen = 0.0f,
        stepDr = 0.0f;
    if (const Json* ts = def.find("tier_steps")) {
        if (!ts->isObject()) {
            return MC_FAIL("effect '" + out.name +
                           "': 'tier_steps' must be an object");
        }
        static const std::set<std::string> kStepFields {"speed_multiplier",
                                                        "absorb_hp",
                                                        "melee_bonus",
                                                        "melee_penalty",
                                                        "damage_reduction"};
        for (const auto& [key, value] : ts->members()) {
            if (kStepFields.count(key) == 0) {
                return MC_FAIL("effect '" + out.name + "' tier_steps holds "
                               "the non-tierable field '" + key +
                               "' (only the five modifier fields are "
                               "tierable — the tick and instant forms are "
                               "single-tier)");
            }
            if (!value.isNumber() || value.asDouble() < 0.0) {
                return MC_FAIL("effect '" + out.name + "' tier_steps." +
                               key + " must be a number >= 0, got " +
                               (value.isNumber()
                                    ? std::to_string(value.asDouble())
                                    : "a non-number"));
            }
            const float v = static_cast<float>(value.asDouble());
            if (key == "speed_multiplier") {
                stepSm = v;
            } else if (key == "absorb_hp") {
                stepAh = v;
            } else if (key == "melee_bonus") {
                stepMb = v;
            } else if (key == "melee_penalty") {
                stepMpen = v;
            } else {
                stepDr = v;
            }
        }
        // The pairing rule: a non-zero step needs its base field.
        if (stepSm != 0.0f && out.speedMultiplier <= 1.0f) {
            return MC_FAIL("effect '" + out.name + "' tier_steps."
                           "speed_multiplier requires 'speed_multiplier' "
                           "present (> 1.0)");
        }
        if (stepAh != 0.0f && out.absorbHp <= 0.0f) {
            return MC_FAIL("effect '" + out.name + "' tier_steps.absorb_hp "
                           "requires 'absorb_hp' present");
        }
        if (stepMb != 0.0f && out.meleeBonus <= 0.0f) {
            return MC_FAIL("effect '" + out.name + "' tier_steps."
                           "melee_bonus requires 'melee_bonus' present");
        }
        if (stepMpen != 0.0f && out.meleePenalty <= 0.0f) {
            return MC_FAIL("effect '" + out.name + "' tier_steps."
                           "melee_penalty requires 'melee_penalty' "
                           "present");
        }
        if (stepDr != 0.0f && out.damageReduction <= 0.0f) {
            return MC_FAIL("effect '" + out.name + "' tier_steps."
                           "damage_reduction requires 'damage_reduction' "
                           "present");
        }
        // The dead-field rule: steps only exist to serve tiers, and the
        // single-tier effect has no tier > 0 — a step on it is dead data.
        if (out.tierMax == 1 && (stepSm != 0.0f || stepAh != 0.0f ||
                                 stepMb != 0.0f || stepMpen != 0.0f ||
                                 stepDr != 0.0f)) {
            return MC_FAIL("effect '" + out.name +
                           "' has 'tier_steps' but tier_max 1 (a single-"
                           "tier effect has no tier to scale — the steps "
                           "are dead data)");
        }
        // The no-op rule: a tiered effect must actually scale.
        if (out.tierMax > 1 && stepSm == 0.0f && stepAh == 0.0f &&
            stepMb == 0.0f && stepMpen == 0.0f && stepDr == 0.0f) {
            return MC_FAIL("effect '" + out.name +
                           "' has tier_max > 1 but no non-zero tier step "
                           "(a tier set that changes nothing is a no-op "
                           "typo)");
        }
        out.stepSpeedMultiplier = stepSm;
        out.stepAbsorbHp = stepAh;
        out.stepMeleeBonus = stepMb;
        out.stepMeleePenalty = stepMpen;
        out.stepDamageReduction = stepDr;
    } else if (out.tierMax > 1) {
        // tierMax > 1 with NO tier_steps object = the no-op rule above
        // (nothing scales).
        return MC_FAIL("effect '" + out.name +
                       "' has tier_max > 1 but no 'tier_steps' (a tiered "
                       "effect must name the fields it scales)");
    }

    // SURV-4: the potion's instant HP change (optional). A whole number of
    // HP in [-32, 32]; the ABSENT field is the "no instant action" default,
    // so an explicit 0 is a typo (the dead-field discipline). Mutually
    // exclusive with `tick_type` (an effect is either a tick action or an
    // instant one, not both — one effect, one action form). Coexistence
    // with the modifier fields (melee_penalty / speed / ...) is allowed
    // (the data permits it; the shipped instant effects use none of them).
    const Json* ih = def.find("instant_hp");
    if (ih != nullptr) {
        if (!ih->isNumber() || ih->asDouble() != std::floor(ih->asDouble())) {
            return MC_FAIL("effect '" + out.name + "': 'instant_hp' must be "
                           "an integer, got " +
                           (ih->isNumber() ? std::to_string(ih->asDouble())
                                           : std::string("non-number")));
        }
        const double d = ih->asDouble();
        if (d == 0.0) {
            return MC_FAIL("effect '" + out.name +
                           "': 'instant_hp' must be non-zero (absent = no "
                           "instant action; an explicit 0 is a typo)");
        }
        if (d < -32.0 || d > 32.0) {
            return MC_FAIL("effect '" + out.name +
                           "': 'instant_hp' must be in [-32, 32], got " +
                           std::to_string(d));
        }
        if (def.find("tick_type") != nullptr) {
            return MC_FAIL("effect '" + out.name +
                           "': an effect is either tick or instant, not "
                           "both ('instant_hp' + 'tick_type')");
        }
        out.instantHp = static_cast<float>(d);
    }

    const Json* neg = def.find("negative");
    if (neg != nullptr) {
        if (!neg->isBool()) {
            return MC_FAIL("effect '" + out.name + "': 'negative' must be a "
                           "boolean");
        }
        out.negative = neg->asBool();
    }

    // Phase 57: the fire-system flags (optional booleans, default false).
    const Json* ig = def.find("ignites_on_lava");
    if (ig != nullptr) {
        if (!ig->isBool()) {
            return MC_FAIL("effect '" + out.name + "': 'ignites_on_lava' "
                           "must be a boolean");
        }
        out.ignitesOnLava = ig->asBool();
    }
    const Json* ew = def.find("extinguished_by_water");
    if (ew != nullptr) {
        if (!ew->isBool()) {
            return MC_FAIL("effect '" + out.name + "': "
                           "'extinguished_by_water' must be a boolean");
        }
        out.extinguishedByWater = ew->asBool();
    }

    // SURV-5: the mob-carrier flags (strict booleans, default false).
    // `affects_mobs` marks the effects a mob CAN carry (Minecraft's
    // "affects monsters": poison / wither); `undead_exempt` marks the ones
    // the UNDEAD refuse (poison — the wither kills them, Minecraft's
    // uniqueness). Both are apply-time combination gates (init only
    // cross-checks the NAMES, not the flags).
    const Json* am = def.find("affects_mobs");
    if (am != nullptr) {
        if (!am->isBool()) {
            return MC_FAIL("effect '" + out.name + "': 'affects_mobs' "
                           "must be a boolean");
        }
        out.affectsMobs = am->asBool();
    }
    const Json* ue = def.find("undead_exempt");
    if (ue != nullptr) {
        if (!ue->isBool()) {
            return MC_FAIL("effect '" + out.name + "': 'undead_exempt' "
                           "must be a boolean");
        }
        out.undeadExempt = ue->asBool();
    }

    const Json* icon = def.find("icon");
    if (icon == nullptr || !icon->isNumber() ||
        icon->asDouble() != std::floor(icon->asDouble()) ||
        icon->asDouble() < 0.0) {
        return MC_FAIL("effect '" + out.name + "': 'icon' is required and "
                       "must be an integer >= 0 (the effects.png tile)");
    }
    out.icon = static_cast<int>(icon->asDouble());

    // An effect must DO something: a tick action, a melee penalty/bonus, an
    // absorption layer, speed, resistance, or an instant HP change (SURV-4).
    if (out.tick == Effect::Tick::None && out.meleePenalty <= 0.0f &&
        out.absorbHp <= 0.0f && out.speedMultiplier <= 1.0f &&
        out.meleeBonus <= 0.0f && out.damageReduction <= 0.0f &&
        out.instantHp == 0.0f) {
        return MC_FAIL("effect '" + out.name + "' is a no-op: give it a "
                       "'tick_type', a positive 'melee_penalty', a positive "
                       "'absorb_hp', a 'speed_multiplier' > 1, a positive "
                       "'melee_bonus', a 'damage_reduction' > 0, or a "
                       "non-zero 'instant_hp'");
    }
    return {};
}

Result<int> EffectRegistry::registerEffect(const Json& def) {
    Effect e;
    auto r = parseEffect(def, e);
    MC_TRY(r);
    if (m_names.count(e.name) != 0) {
        return MC_FAIL("duplicate effect id '" + e.name + "'");
    }
    e.id = static_cast<int>(m_effects.size());
    m_names.emplace(e.name, e.id);
    m_effects.push_back(e);
    return e.id;
}

Result<void> EffectRegistry::loadDirectory(AssetManager& assets,
                                           const std::string& relativeDir) {
    auto dir = assets.resolve(relativeDir);
    MC_TRY(dir);
    if (!std::filesystem::is_directory(*dir)) {
        return MC_FAIL("effect directory not found: '" + (*dir).string() +
                       "'");
    }

    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(*dir)) {
        if (entry.is_regular_file() &&
            entry.path().extension() == ".json") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());

    for (const auto& file : files) {
        // AssetManager keys are relative to the root.
        const auto rel =
            std::filesystem::relative(file, assets.root()).generic_string();
        auto text = assets.loadText(rel);
        MC_TRY(text);
        auto parsed = Json::parse(*text);
        if (!parsed.ok()) {
            return MC_FAIL("invalid JSON in effect file '" + rel + "': " +
                           parsed.error().message);
        }
        auto id = registerEffect(*parsed);
        if (!id.ok()) {
            return MC_FAIL("effect file '" + rel + "': " +
                           id.error().message);
        }
        MC_LOG_DEBUG("loaded effect '" + m_effects[id.value()].name +
                     "' from '" + rel + "'");
    }
    return {};
}

const Effect& EffectRegistry::at(int id) const {
    assert(id >= 0 && static_cast<std::size_t>(id) < m_effects.size());
    return m_effects[static_cast<std::size_t>(id)];
}

const Effect* EffectRegistry::find(int id) const {
    if (id < 0 || static_cast<std::size_t>(id) >= m_effects.size()) {
        return nullptr;
    }
    return &m_effects[static_cast<std::size_t>(id)];
}

const Effect* EffectRegistry::findByName(const std::string& name) const {
    const auto it = m_names.find(name);
    if (it == m_names.end()) {
        return nullptr;
    }
    return &m_effects[static_cast<std::size_t>(it->second)];
}

int EffectRegistry::idForName(const std::string& name) const {
    const auto it = m_names.find(name);
    return it == m_names.end() ? -1 : it->second;
}

std::vector<int> EffectRegistry::allIds() const {
    std::vector<int> ids;
    ids.reserve(m_effects.size());
    for (const Effect& e : m_effects) {
        ids.push_back(e.id);
    }
    return ids;
}

void EffectRegistry::clear() {
    m_effects.clear();
    m_names.clear();
}

// -- StatusEffects ------------------------------------------------------------

void StatusEffects::apply(int effect, double duration, int tier) {
    for (ActiveEffect& a : m_active) {
        if (a.effect == effect) {
            // Re-application resets the duration AND the tier (the grant's
            // tier replaces the instance's — no amplifier stacking); the
            // tick phase is kept.
            a.remaining = duration;
            a.tier = tier;
            return;
        }
    }
    m_active.push_back(ActiveEffect {effect, duration, 0.0, tier});
}

void StatusEffects::clearNegative(const EffectRegistry& effects) {
    m_active.erase(
        std::remove_if(m_active.begin(), m_active.end(),
                       [&](const ActiveEffect& a) {
                           return effects.at(a.effect).negative;
                       }),
        m_active.end());
}

void StatusEffects::clearExtinguishable(const EffectRegistry& effects) {
    m_active.erase(
        std::remove_if(m_active.begin(), m_active.end(),
                       [&](const ActiveEffect& a) {
                           return effects.at(a.effect).extinguishedByWater;
                       }),
        m_active.end());
}

void StatusEffects::clearAll() { m_active.clear(); }

void StatusEffects::update(
    double dt, const EffectRegistry& effects,
    const std::function<void(const Effect&)>& onTick) {
    for (ActiveEffect& a : m_active) {
        const Effect& def = effects.at(a.effect);
        a.remaining -= dt;
        // The tick phase only advances while the effect is still alive
        // (an expiry this frame does not fire a tick). A large dt can
        // cross several intervals — fire one tick per crossing.
        if (a.remaining > 0.0 && def.tick != Effect::Tick::None) {
            a.accum += dt;
            while (a.accum >= def.tickInterval) {
                a.accum -= def.tickInterval;
                onTick(def);
            }
        }
    }
    m_active.erase(
        std::remove_if(m_active.begin(), m_active.end(),
                       [](const ActiveEffect& a) {
                           return a.remaining <= 0.0;
                       }),
        m_active.end());
}

float StatusEffects::totalMeleePenalty(const EffectRegistry& effects) const {
    float sum = 0.0f;
    for (const ActiveEffect& a : m_active) {
        const Effect& e = effects.at(a.effect);
        sum += effectTieredValue(a.tier, e.meleePenalty,
                                 e.stepMeleePenalty);
    }
    return sum;
}

float StatusEffects::totalAbsorption(const EffectRegistry& effects) const {
    float sum = 0.0f;
    for (const ActiveEffect& a : m_active) {
        const Effect& e = effects.at(a.effect);
        sum += effectTieredValue(a.tier, e.absorbHp, e.stepAbsorbHp);
    }
    return sum;
}

float StatusEffects::totalSpeedMultiplier(const EffectRegistry& effects) const {
    // Multiplicative (each active speed effect scales the move speed); the
    // identity 1.0 when none grant speed. SURV-6: the tier-adjusted value
    // per instance.
    float product = 1.0f;
    for (const ActiveEffect& a : m_active) {
        const Effect& e = effects.at(a.effect);
        product *= effectTieredValue(a.tier, e.speedMultiplier,
                                     e.stepSpeedMultiplier);
    }
    return product;
}

float StatusEffects::totalMeleeBonus(const EffectRegistry& effects) const {
    float sum = 0.0f;
    for (const ActiveEffect& a : m_active) {
        const Effect& e = effects.at(a.effect);
        sum += effectTieredValue(a.tier, e.meleeBonus, e.stepMeleeBonus);
    }
    return sum;
}

float StatusEffects::totalDamageReduction(
    const EffectRegistry& effects) const {
    // The single highest active reduction (the player has one effective
    // resistance, not stacked fractions). SURV-6: the tier-adjusted value
    // per instance.
    float maxr = 0.0f;
    for (const ActiveEffect& a : m_active) {
        const Effect& e = effects.at(a.effect);
        maxr = std::max(maxr,
                        effectTieredValue(a.tier, e.damageReduction,
                                          e.stepDamageReduction));
    }
    return maxr;
}

} // namespace mc
