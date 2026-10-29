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
//   - `affects_mobs` (SURV-5): the effect is accepted by the MOB carrier
//     (Minecraft's "affects monsters" line). Absent/false = player-only
//     (every pre-SURV-5 effect; the bow arrow's effect must carry it —
//     init cross-check).
//   - `min_health_mob` (SURV-5): the health floor a MOB's damage tick must
//     not cross — the twin of `min_health` for the mob carrier. Integer 0
//     or 1; ABSENT = follows `min_health` (the player's floor). Poison
//     ships `min_health_mob: 0`: the player keeps its 1-HP floor while a
//     mob CAN be poisoned to death (Minecraft's dual-target rule). Valid
//     only for a "damage" tick (like `min_health`).
//   - `undead_exempt` (SURV-5): the effect is IGNORED by UNDEAD mobs
//     (Minecraft: poison does not affect the undead). Poison ships it;
//     the wither does not (the wither is the effect that kills undead —
//     Minecraft's uniqueness). Combined with the mob's `undead` flag
//     (data/mobs/*.json) at apply time.
//
// Timing: the durations and tick accumulators run on REAL dt (the survival
// timer class, like hunger and the eat channel — hard rule 8). The events
// that APPLY an effect (a mob's landed melee hit) are frame-determined, so
// headless runs stay byte-reproducible (the remaining seconds in a save are
// real-dt values, like the hunger level).
//
// v1 scope: effects live on the PLAYER (SURV-5 extends the carrier to
// mobs, data-gated by `affects_mobs`: the bow's arrow applies its effect
// to the hit mob, the skeleton's arrow applies the wither to the player;
// heal ticks are a no-op on mobs — MobEntity has no regeneration).
// Sources: a mob's `on_hit_effect` (its landed melee hit applies the
// effect — the spider's poison), the LAVA (the `ignites_on_lava` effect,
// Phase 57 — the fire), a bow's `arrow_effect` (SURV-5, its arrow's hit
// on a mob) and save injection (the E2E scenes; weakness has no in-game
// source, its Minecraft source — potions — arrived in SURV-4).
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
    // SURV-5: accepted by the mob carrier (Minecraft's "affects monsters";
    // poison / wither = true). A bow's arrow_effect must name an effect
    // with this flag (init cross-check — a bow's arrow only ever hits
    // mobs). false = player-only (every pre-SURV-5 effect).
    bool affectsMobs = false;
    // SURV-5: the health floor a MOB's damage tick must not cross — the
    // mob-side twin of `minHealth`. -1 = the ABSENT field (the mob floor
    // follows `minHealth`); 0 = no floor (a damage tick may kill the mob);
    // 1 = the mob is kept at >= 1 (the effect never kills a mob). Valid
    // only for Tick::Damage (a heal tick cannot floor health, like
    // `min_health`). Poison ships 0 (player floor 1 kept, mobs die —
    // Minecraft's dual-target rule).
    int minHealthMob = -1;
    // SURV-5: the effect is ignored when applied to an UNDEAD mob
    // (Minecraft: poison does not affect the undead — the wither does).
    // Poison ships it; a pure apply-time combination gate with the mob's
    // data `undead` flag (no init check needed — both defaults false).
    bool undeadExempt = false;
    // (Re-)applied every frame while the player is in the lava (the fire,
    // Phase 57). At most one registered effect may carry this flag.
    bool ignitesOnLava = false;
    // Removed instantly while the player is in water (the fire, Phase 57).
    bool extinguishedByWater = false;
    // The effects.png 16 px tile the HUD draws for this effect.
    int icon = 0;
    // PRE-7: the full-screen veil rgba while the effect is active on the
    // PLAYER (the Minecraft potion tints — poison = the purple veil).
    // [r, g, b, a] each in [0, 1]; a = 0 (the absent-field default) = no
    // veil (zero draws — the structural-immunity root). The constant
    // alpha is the v1: Minecraft's pulsing poison veil is a documented
    // deviation. A positive alpha also satisfies the no-op rule (a
    // presentation-layer action counts as "doing something").
    float tintR = 0.0f;
    float tintG = 0.0f;
    float tintB = 0.0f;
    float tintA = 0.0f;
    // SURV-6 (effect tiers): how many tiers this effect has (1 = single
    // tier, the pre-SURV-6 behavior; the shipped tierable effects have 3 =
    // Minecraft's I/II/III). A grant's `tier` is 0-based and must be <
    // tierMax. Validated at parse: int in [1, 3], and tierMax > 1 REQUIRES
    // at least one non-zero tier step (a no-op tier set is rejected — the
    // no-op rule).
    int tierMax = 1;
    // SURV-6: the per-tier INCREMENTS of the tierable modifier fields
    // (the Minecraft amplifier shape: value(tier) = base + tier × step).
    // 0 = that field does not scale (the default for every pre-SURV-6
    // effect). A step for a field is only legal while the field itself is
    // present and non-default (the pairing rule, the fast_against
    // discipline). Only the five modifier fields are tierable — the tick
    // (tick_interval/tick_amount) and instant_hp forms are NOT (a tiered
    // interval would change the tick grid; the shipped tick/instant
    // effects stay single-tier, the documented deviation).
    float stepSpeedMultiplier = 0.0f;
    float stepAbsorbHp = 0.0f;
    float stepMeleeBonus = 0.0f;
    float stepMeleePenalty = 0.0f;
    float stepDamageReduction = 0.0f;
};

// SURV-6: the tier-adjusted value of one modifier field for an active
// instance: base + tier × step. Tier 0 returns the base value bit-for-bit
// (every pre-SURV-6 consumer), and a zero step returns the base regardless
// of the tier (single-tier effects keep their exact values).
inline float effectTieredValue(int tier, float base, float step) {
    return base + static_cast<float>(tier) * step;
}

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
    // SURV-6: the applied tier (0-based; the grant's tier — a re-apply
    // REPLACES it with the new grant's tier, like the duration). 0 = the
    // pre-SURV-6 value (bit-for-bit).
    int tier = 0;
};

// The player's active status effects (Phase 46). Deterministic order:
// effects are appended in application order and never reordered; removal
// (expiry) keeps the survivors' order. The game owns the instance and the
// registry; this class is pure (no game/GL dependency — unit-testable).
class StatusEffects {
public:
    StatusEffects() = default;

    // Apply `effect` for `duration` seconds at `tier` (SURV-6, 0-based,
    // the grant's tier). An already-active instance is RESET to the new
    // duration AND the new tier (Minecraft's re-application semantics:
    // drinking the tier-1 potion replaces the tier-0 instance's tier, it
    // does not stack); its tick phase is kept. `tier` 0 = the pre-SURV-6
    // behavior bit-for-bit.
    void apply(int effect, double duration, int tier = 0);

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

    // The total melee penalty (sum over the active instances' tier-adjusted
    // definitions, SURV-6: base + tier × step per instance).
    float totalMeleePenalty(const EffectRegistry& effects) const;

    // The total absorption layer (Phase 56): the sum of the tier-adjusted
    // `absorb_hp` over the active instances (0 when none grant a layer).
    float totalAbsorption(const EffectRegistry& effects) const;

    // Phase 84 / SURV-6: the total movement speed multiplier (the PRODUCT
    // of the tier-adjusted `speed_multiplier` over the active instances;
    // 1.0 when none grant speed — the multiplicative identity).
    float totalSpeedMultiplier(const EffectRegistry& effects) const;
    // Phase 84 / SURV-6: the total melee bonus (the SUM of the
    // tier-adjusted `melee_bonus` over the active instances; 0 when none
    // grant strength).
    float totalMeleeBonus(const EffectRegistry& effects) const;
    // Phase 84 / SURV-6: the total damage reduction (the MAX of the
    // tier-adjusted `damage_reduction` over the active instances; 0 when
    // none grant resistance).
    float totalDamageReduction(const EffectRegistry& effects) const;

    std::size_t count() const { return m_active.size(); }
    const std::vector<ActiveEffect>& list() const { return m_active; }

private:
    std::vector<ActiveEffect> m_active;
};

// PRE-7: one full-screen veil color (the rgba of an active tinted
// effect). The render pass draws one veil per entry.
struct EffectTint {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 0.0f;
};

// PRE-7: the veil colors of the player's ACTIVE effects — one entry per
// active instance whose definition carries a tint (alpha > 0), in the
// StatusEffects' deterministic application order. Empty when no active
// effect has a tint = zero draws (the structural immunity: the no-input
// baseline carries no effects).
std::vector<EffectTint> activeEffectTints(const StatusEffects& effects,
                                          const EffectRegistry& registry);

} // namespace mc
