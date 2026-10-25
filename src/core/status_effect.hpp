// Phase 46: the status-effect system — data-driven effect definitions
// (assets/data/effects/*.json) and the player's active effect instances.
//
// An effect is a DATA object (the core never switches on an effect id):
//   - `tick_type`: "damage" (poison) or "heal" (regeneration) — every
//     `tick_interval` seconds the effect fires one `tick_amount` HP of
//     damage (never below `min_health`, the Minecraft poison floor) or
//     healing (clamped at max health). Absent = the effect has no ticks
//     (a modifier effect like weakness).
//   - `melee_penalty`: attack points subtracted from the player's melee
//     damage while active (weakness; clamped at 0 — a weakened bare hand
//     deals no damage).
//   - `absorb_hp`: (Phase 56) temporary max-health points added while the
//     effect is active (the "absorption layer" — the golden apple's red
//     hearts). The player's effective max health is
//     `max_health + sum(absorb_hp over active instances)`; health above
//     the base max vanishes SILENTLY when the layer shrinks (no damage
//     flash/sound — the layer is not real HP). Applying an increase RAISES
//     the player's health by the added amount (the layer appears full).
//   - `negative`: the effect is cleared by drinking a milk container
//     (Minecraft clears NEGATIVE effects; regeneration survives the milk).
//   - `ignites_on_lava` (Phase 57): the effect is (re-)applied EVERY frame
//     while the player is in the lava (the fire: continuous burn in the
//     lava, and the lingering burn after leaving it — Minecraft). At most
//     ONE effect may carry the flag (init cross-check).
//   - `extinguished_by_water` (Phase 57): the effect is removed INSTANTLY
//     while the player is in water (the fire in water — Minecraft; water
//     beats lava: in both, no fire).
//   - `icon`: the 16 px tile index in assets/textures/effects.png the HUD
//     draws for the effect (cross-checked against the texture at init).
//   - `instant_hp` (SURV-4, potions): a whole-HP change settled the moment
//     the effect is applied (positive = heal, negative = damage with the
//     armor EXEMPTED — Minecraft's magic/potion damage), leaving no
//     lingering duration (a grant of duration 0). Mutually exclusive with
//     `tick_type` (an effect is either a tick action or an instant one).
//     Absent = no instant action (the default).
//
// Timing: the durations and tick accumulators run on REAL dt (the survival
// timer class, like hunger and the eat channel — hard rule 8). The events
// that APPLY an effect (a mob's landed melee hit) are frame-determined, so
// headless runs stay byte-reproducible (the remaining seconds in a save are
// real-dt values, like the hunger level).
//
// v1 scope: effects live on the PLAYER only (mobs cannot be affected —
// there is no item/mob source that applies one to a mob). Sources: a mob's
// `on_hit_effect` (its landed melee hit applies the effect — the spider's
// poison), the LAVA (the `ignites_on_lava` effect, Phase 57 — the fire),
// and save injection (the E2E scenes; weakness has no in-game source in
// v1, its Minecraft source — potions — is not built yet).
#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/error.hpp"
#include "core/json.hpp"

namespace mc {

class AssetManager;

// One data-driven effect definition (a loaded data/effects/*.json file).
struct Effect {
    int id = -1;
    std::string name;
    // The periodic action (None = a modifier effect, no ticks).
    enum class Tick {
        None,
        Damage,
        Heal,
    } tick = Tick::None;
    // One tick's HP (damage or healing); valid when tick != None.
    float tickAmount = 0.0f;
    // Seconds between ticks (REAL dt); valid when tick != None.
    float tickInterval = 0.0f;
    // The health floor a damage tick must not cross (the MC poison
    // semantics): 0 = no floor (the damage may kill) or 1 = the player
    // is kept at >= 1 HP (the effect never kills). Valid only for
    // Tick::Damage (a heal tick cannot floor health).
    float minHealth = 0.0f;
    // Attack points subtracted from the player's melee damage (weakness).
    float meleePenalty = 0.0f;
    // Temporary max-health points while active (the absorption layer,
    // Phase 56). 0 = no layer.
    float absorbHp = 0.0f;
    // Phase 84 beacon effects: the modifier fields (the Minecraft beacon's
    // other primary effects, data-driven like melee_penalty/absorb_hp).
    // Movement speed multiplier while active (the "speed" effect). 1.0 =
    // no change; > 1.0 = faster (Minecraft speed I ~ 1.2). Combined
    // multiplicatively over active instances.
    float speedMultiplier = 1.0f;
    // Melee attack points ADDED while active (the "strength" effect) — the
    // bonus counterpart of melee_penalty (weakness). 0 = no bonus. Combined
    // additively over active instances.
    float meleeBonus = 0.0f;
    // Incoming damage fraction reduced while active (the "resistance"
    // effect): 0 = no reduction, < 1.0 (Minecraft resistance I ~ 0.2).
    // Combined as the single HIGHEST active reduction (the player has one
    // effective resistance, not stacked fractions).
    float damageReduction = 0.0f;
    // SURV-4 (potions): an INSTANT HP change settled the moment the effect
    // is APPLIED (positive = heal, negative = damage with the armor
    // EXEMPTED — Minecraft's magic/potion damage), after which the effect
    // leaves no lingering duration (a grant of duration 0). Mutually
    // exclusive with `tick_type` (an effect is either a tick action or an
    // instant one, not both — one effect, one action form). Stored as a
    // float like absorb_hp but integer-valued (validated at parse).
    // 0.0 = no instant action (the absent-field default).
    float instantHp = 0.0f;
    // Cleared by drinking a milk container (negative effects).
    bool negative = false;
    // (Re-)applied every frame while the player is in the lava (the fire,
    // Phase 57). At most one registered effect may carry this flag.
    bool ignitesOnLava = false;
    // Removed instantly while the player is in water (the fire, Phase 57).
    bool extinguishedByWater = false;
    // The effects.png 16 px tile the HUD draws for this effect.
    int icon = 0;
};

// Registry of the effect definitions (data/effects/*.json). Same shape as
// the block/item/mob registries: strict field validation, name uniqueness,
// sorted directory load, id/name lookups.
class EffectRegistry {
public:
    EffectRegistry() = default;

    // Parse and register one effect. Assigns the next free id.
    Result<int> registerEffect(const Json& def);

    // Load and register every *.json file in `relativeDir` under the asset
    // root (sorted for determinism). Non-const: text loading updates the
    // AssetManager cache.
    Result<void> loadDirectory(AssetManager& assets,
                               const std::string& relativeDir =
                                   "data/effects");

    // -- lookups --------------------------------------
    const Effect& at(int id) const;      // asserts valid id
    const Effect* find(int id) const;
    const Effect* findByName(const std::string& name) const;
    int idForName(const std::string& name) const; // -1 when absent
    std::size_t count() const { return m_effects.size(); }
    std::vector<int> allIds() const;

    void clear();

    // Pure parse helper (unit-testable without a registry).
    static Result<void> parseEffect(const Json& def, Effect& out);

private:
    std::vector<Effect> m_effects;
    std::unordered_map<std::string, int> m_names;
};

// One ACTIVE effect instance on the player. `effect` indexes the
// EffectRegistry; `remaining` is the real-dt countdown; `accum` is the
// tick-phase accumulator (live state, NOT saved — a reload restarts the
// tick phase at 0, the next tick lands at the full interval).
struct ActiveEffect {
    int effect = -1;
    double remaining = 0.0;
    double accum = 0.0;
};

// The player's active status effects (Phase 46). Deterministic order:
// effects are appended in application order and never reordered; removal
// (expiry) keeps the survivors' order. The game owns the instance and the
// registry; this class is pure (no game/GL dependency — unit-testable).
class StatusEffects {
public:
    StatusEffects() = default;

    // Apply `effect` for `duration` seconds. An already-active instance is
    // RESET to the new duration (Minecraft's re-application semantics,
    // without amplifiers); its tick phase is kept.
    void apply(int effect, double duration);

    // Remove every NEGATIVE effect (drinking a milk container — the
    // positive effects, like regeneration, survive).
    void clearNegative(const EffectRegistry& effects);

    // Remove every effect flagged `extinguished_by_water` (the player in
    // water, Phase 57 — the fire is extinguished instantly).
    void clearExtinguishable(const EffectRegistry& effects);

    // Remove everything (the death/respawn path).
    void clearAll();

    // Advance by dt REAL seconds. Each instance counts `remaining` down
    // and removes at <= 0; a tick effect also accumulates and fires one
    // `onTick(effect)` per interval crossed (a large dt can cross several).
    // `onTick` receives the effect definition (the game applies the HP
    // change against the player there — the floor/max checks need the
    // player's current health). `effects` resolves the definitions.
    void update(double dt, const EffectRegistry& effects,
                const std::function<void(const Effect&)>& onTick);

    // The total melee penalty (sum over the active instances' definitions).
    float totalMeleePenalty(const EffectRegistry& effects) const;

    // The total absorption layer (Phase 56): the sum of `absorb_hp` over
    // the active instances' definitions (0 when none grant a layer).
    float totalAbsorption(const EffectRegistry& effects) const;

    // Phase 84: the total movement speed multiplier (the PRODUCT of
    // `speed_multiplier` over the active instances; 1.0 when none grant
    // speed — the multiplicative identity).
    float totalSpeedMultiplier(const EffectRegistry& effects) const;
    // Phase 84: the total melee bonus (the SUM of `melee_bonus` over the
    // active instances; 0 when none grant strength).
    float totalMeleeBonus(const EffectRegistry& effects) const;
    // Phase 84: the total damage reduction (the MAX of `damage_reduction`
    // over the active instances; 0 when none grant resistance).
    float totalDamageReduction(const EffectRegistry& effects) const;

    std::size_t count() const { return m_active.size(); }
    const std::vector<ActiveEffect>& list() const { return m_active; }

private:
    std::vector<ActiveEffect> m_active;
};

} // namespace mc
