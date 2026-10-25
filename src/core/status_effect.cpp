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
                                                "instant_hp"};
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
        } else if (def.find("min_health") != nullptr) {
            return MC_FAIL("effect '" + out.name + "': 'min_health' is only "
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

void StatusEffects::apply(int effect, double duration) {
    for (ActiveEffect& a : m_active) {
        if (a.effect == effect) {
            a.remaining = duration;  // re-application resets the duration
            return;                  // (the tick phase is kept)
        }
    }
    m_active.push_back(ActiveEffect {effect, duration, 0.0});
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
        sum += effects.at(a.effect).meleePenalty;
    }
    return sum;
}

float StatusEffects::totalAbsorption(const EffectRegistry& effects) const {
    float sum = 0.0f;
    for (const ActiveEffect& a : m_active) {
        sum += effects.at(a.effect).absorbHp;
    }
    return sum;
}

float StatusEffects::totalSpeedMultiplier(const EffectRegistry& effects) const {
    // Multiplicative (each active speed effect scales the move speed); the
    // identity 1.0 when none grant speed.
    float product = 1.0f;
    for (const ActiveEffect& a : m_active) {
        product *= effects.at(a.effect).speedMultiplier;
    }
    return product;
}

float StatusEffects::totalMeleeBonus(const EffectRegistry& effects) const {
    float sum = 0.0f;
    for (const ActiveEffect& a : m_active) {
        sum += effects.at(a.effect).meleeBonus;
    }
    return sum;
}

float StatusEffects::totalDamageReduction(
    const EffectRegistry& effects) const {
    // The single highest active reduction (the player has one effective
    // resistance, not stacked fractions).
    float maxr = 0.0f;
    for (const ActiveEffect& a : m_active) {
        maxr = std::max(maxr, effects.at(a.effect).damageReduction);
    }
    return maxr;
}

} // namespace mc
