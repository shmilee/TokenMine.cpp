// MobEntity: a data-driven hostile mob (Phase 20).
//
// Physics: the shared Entity AABB physics (gravity / terminal velocity,
// sub-stepped axis moves). Unlike dropped items there is NO ground
// friction: the AI sets the horizontal velocity every frame (exact data
// speed), and knockback decays with its own damping while the knockback
// window is open. ENT-4: while the AABB overlaps a water OR lava cell
// (or the body rests at the fluid's surface) the physics switches to the
// shared fluid model (Entity::refreshFluidState + integrateFluid — the
// input-free twin of the player's FLUID-2 swim: passive rise, the
// geometric W' float, the sink / pool-floor stand), the horizontal speed
// is gated by kUnderwaterSpeedFactor, and the step-up (ENT-3) is
// forbidden in fluid. The AI state machine is untouched — chase / wander
// steer as usual underwater.
//
// AI (deterministic: the same seed + input sequence yields the same
// trajectory). The game steps mobs with a FIXED per-frame dt (the day
// cycle's convention, see Game::kMobStepPerFrame): a moving body's
// position at frame N is then a pure function of N — the real dt varies
// with machine load and would drift a moving mob's screenshot position
// by sub-pixels between runs (the resting entities of the earlier phases
// were immune to that).
//   Wander — the default state. Every `wander_interval` (data) a seeded
//            random decision: 1 in 3 stands still, otherwise walks at
//            speed * kWanderSpeedFactor in a seeded random heading.
//   Chase  — the player is within `detect_radius` (horizontal) AND within
//            kDetectVertical in height: walk at full data speed toward the
//            player's bottom-center, facing it.
//   Attack — within `attack_range` (horizontal) AND kAttackVertical in
//            height: stand still, face the player, and deal
//            `attack_damage` through the hit callback every
//            `attack_cooldown` (the game wires it to Player::damage).
//   RangedAttack (Phase 27) — for mobs whose data `shoot_range > 0`
//            (the skeleton): the player is within `shoot_range` (horizontal)
//            → face it and fire through the shoot callback every
//            `attackCooldown` (the game spawns an arrow at the player's eye
//            with the data shoot_speed / difficulty-scaled damage). While
//            the player is inside `retreat_distance` (0 = never) the mob
//            backs away at full data speed instead of standing. Beyond
//            shoot_range (but inside detect_radius) it chases like a melee
//            mob. A ranged mob's `attack_range` is its (unused) melee
//            range — the skeleton sets it to 0, so it never melee-attacks.
// A dead player is neither chased nor attacked (the mob wanders).
// Passive mobs (Phase 22): detect_radius 0 (and/or attack_range 0 /
// attack_damage 0) make the chase/attack conditions never hold, so the
// mob only ever wanders — no code path distinguishes them from hostile
// mobs, the data alone decides.
//
// Combat (player side): damage() subtracts from health; a killing hit sets
// the dead state — the game removes the mob the same frame and spawns its
// data-defined drops (see Game::updateEntities). addKnockback() applies an
// impulse and opens a kKnockbackTime window during which the AI does not
// override the velocity.
//
// Spawning / despawning are the game's job (mob_spawning.hpp): spawn cells
// come from the deterministic scan, and mobs beyond the despawn distance
// (or below the kill plane, or dead) are removed. Mobs are NOT persisted
// in saves — spawned mobs are ephemeral, as in Minecraft; their drops are
// items and DO save.
#pragma once

#include <cstdint>
#include <functional>

#include "entity/entity.hpp"
#include "entity/mob_registry.hpp"
#include "entity/mob_swing.hpp"
#include "core/status_effect.hpp"

namespace mc {

// The mob's AI state (observable for tests and the debug overlay).
enum class MobState {
    Wander, // no player in range: seeded random decisions
    Chase,  // player in range: walk toward it
    Attack, // in attack range: stand still and swing on the cooldown
    // Ranged attack (Phase 27): fire on the cooldown, back away inside
    // retreat_distance.
    RangedAttack,
};

class MobEntity : public Entity {
public:
    // `def` is a registry entry owned by the game (which outlives every
    // entity). `rngSeed` is a per-mob deterministic seed (the game derives
    // it from the world seed + the spawn counter).
    MobEntity(const World& world, const BlockRegistry& blocks, const Mob& def,
              Vec3 pos, std::uint64_t rngSeed);

    void update(double dt) override;
    bool shouldDespawn() const override {
        return m_dead || belowKillPlane();
    }

    const Mob& def() const { return *m_def; }
    MobID typeId() const { return m_def->id; }
    float health() const { return m_health; }
    bool dead() const { return m_dead; }
    MobState state() const { return m_state; }
    // Facing in the camera yaw convention (0 = -Z); the render model
    // rotates by this.
    float facingYaw() const { return m_yaw; }

    // ENT-1: the limb-swing phase (0..kSwingPhaseCount-1) — a pure
    // render-layer cosmetic state (session-local, NEVER saved: mobs are
    // ephemeral, see the file head). Advanced once per update() frame
    // (frame-locked, hard rule 8): kSwingStep while the mob has strict
    // horizontal displacement this frame, kSwingDecay toward 0 (the
    // identity pose) while standing. Types without swing data never
    // advance (structural: their render path stays the static mesh).
    int swingPhase() const { return m_swingPhase; }

    // ENT-4: this tick's fluid state (the strict AABB overlap of at least
    // one water OR lava cell — the player's inWater()/inLava() union; the
    // fire system's checks below stay FRESH per the P57 pattern, a
    // different read). Exposed for tests and the debug overlay.
    bool inFluid() const { return m_inFluid; }

    // Subtract damage; returns true when THIS hit killed the mob.
    bool damage(float amount);
    // Knockback impulse; suppresses AI steering for kKnockbackTime.
    void addKnockback(Vec3 impulse);

    // Fire system (Phase 58, v2: the fire reaches mobs — Minecraft's lava
    // ignites every entity; Phase 73, v3: the FIRE BLOCK does too — the
    // block twin of the lava ignition). The game arms the mob at spawn
    // from the SAME fire definition + config duration as the player (the
    // ignites_on_lava effect + player.fire_duration_seconds); an unarmed
    // mob — an asset set without the fire effect — never burns
    // (structural, the P57 pattern). While in the lava OR strictly
    // overlapping a fire-block cell (the constructor-resolved id, the
    // P70 player pattern — no fire block registered = structurally
    // inert) the burn re-ignites (the duration resets every step = a
    // continuous burn); after leaving, the remaining time counts down
    // (the lingering burn). Water extinguishes instantly, BEFORE the tick
    // (zero burn damage in water; water beats both, the P57 rule). The
    // burn ticks run on the fixed mob step (deterministic — hard rule 8:
    // mobs tick at the fixed 1/60 frame step) with the Phase 46 effect
    // semantics (the expiry step does not tick). Because the step is the
    // exact 1/60 frame grid, the tick cadence is kept in INTEGER frame
    // units (tick k lands on burning frame k * interval*60 — no float
    // phase drift; the game init validates the ignites effect's interval
    // is a whole number of frames so this is exact).
    void armFire(double tickInterval, float tickAmount, double duration);
    bool burning() const { return m_fireRemaining > 0.0; }

    // SURV-5: the mob's active status effects (the shared pure
    // StatusEffects class — the arrow's effect lands here). Apply gate:
    // the effect must be mob-accepting (`affects_mobs`) AND not undead-
    // exempted against an undead mob (Minecraft: poison does not affect
    // the undead; the wither kills them). Both gates are defensive at
    // apply time (the data combinations are cross-checked at game init
    // where they make sense). A dead mob refuses the apply (it despawns
    // this frame anyway).
    void applyEffect(int effect, double duration,
                     const EffectRegistry& effects);
    // SURV-5: advance the mob's active effects by `dt` (the game calls
    // this once per fixed mob step, right after update() — hard rule 8:
    // frame-locked mob time). Damage ticks subtract through damage()
    // with the MOB floor (`min_health_mob`, defaulting to the effect's
    // player `min_health` — poison ships 0: a mob CAN be poisoned to
    // death, the player keeps its 1-HP floor); a killing tick leaves the
    // mob dead (the existing damage/despawn/drop path, unchanged). Heal
    // ticks are a NO-OP (MobEntity has no regeneration — the documented
    // deviation: the project has no mob-heal need). count == 0 fast-
    // exits (a mob without effects costs nothing — the structural-
    // immunity basis).
    void updateStatusEffects(double dt, const EffectRegistry& effects);
    bool hasEffects() const { return m_effects.count() > 0; }
    const StatusEffects& statusEffects() const { return m_effects; }

    // Per-frame player state; the game sets it right before update()
    // (bottom-center position + whether the player can be attacked).
    void setPlayer(Vec3 pos, bool alive) {
        m_playerPos = pos;
        m_playerAlive = alive;
    }
    // The AI invokes this when an attack lands (the game wires it to
    // Player::damage; tests record calls).
    void setHitCallback(std::function<void(float)> cb) {
        m_hitPlayer = std::move(cb);
    }
    // Ranged mobs (Phase 27): the AI invokes this to fire; the game
    // spawns the arrow (data shoot_speed, difficulty-scaled damage) aimed
    // at the player's eye. The callback reads the mob through the pointer
    // the game captured at spawn, so the mob must outlive its callback.
    void setShootCallback(std::function<void()> cb) {
        m_shootPlayer = std::move(cb);
    }
    // Head height for ranged shots (80% of the model height): the arrow
    // spawn point (the game aims from here at the player's eye).
    Vec3 eyePosition() const {
        return position() +
               Vec3 {0.0f, m_def->model.height * kEyeHeightFraction, 0.0f};
    }

    // Phase 38: roll one death-drop entry's count in [minCount, maxCount]
    // (both inclusive) with the mob's seeded splitmix64 stream — the same
    // state-advancing flow as the wander decisions. Deaths happen on a
    // deterministic frame (fixed step), so the roll is reproducible
    // run-to-run for the same world seed + input sequence. A degenerate
    // range (min == max) returns the constant without advancing the
    // stream, so fixed-count drops keep the pre-Phase-38 stream exactly.
    int rollDropCount(int minCount, int maxCount);

private:
    void integrate(float h) override;
    void runAi(double dt);

    const Mob* m_def;
    float m_health = 0.0f;
    bool m_dead = false;
    // ENT-1: the limb-swing phase (see swingPhase()). Spawn = 0 (the
    // identity pose); render-only, never saved.
    int m_swingPhase = 0;
    Vec3 m_playerPos = Vec3::zero();
    bool m_playerAlive = true;
    MobState m_state = MobState::Wander;
    float m_yaw = 0.0f;
    double m_wanderTimer = 0.0;   // <= 0: decide this frame
    double m_attackCooldown = 0.0;  // seconds until the next hit
    double m_knockbackTime = 0.0;   // seconds of AI suppression left
    std::uint64_t m_rng = 0;        // splitmix64 state (per-mob)
    std::function<void(float)> m_hitPlayer;
    std::function<void()> m_shootPlayer;  // ranged fire (Phase 27)

    // Fire system (Phase 58) state: armed by the game at spawn (the fire
    // effect's tick parameters + the config duration). `remaining` counts
    // down the lingering burn; the tick phase is on the integer frame grid
    // (`frames` = burning steps so far, `due` = the next tick's frame,
    // `intervalFrames` = frames per tick — exact, init-validated).
    // `m_fireBlockId` (Phase 73): the fire block, constructor-resolved
    // (BlockID::invalid when the asset set has none — the contact check
    // is then structurally inert, the P70 player pattern).
    BlockID m_fireBlockId = BlockID::invalid();
    bool m_fireArmed = false;
    float m_fireAmount = 0.0f;
    double m_fireDuration = 0.0;
    double m_fireRemaining = 0.0;
    int m_fireIntervalFrames = 0;
    int m_fireFrames = 0;
    int m_fireDueFrames = 0;

    // SURV-5: the mob's active status effects (the shared pure class,
    // the player's m_playerEffects twin). Ephemeral like the mob itself
    // (never saved — zero format); count 0 = the update fast exit.
    StatusEffects m_effects;

    // Shared fall (same numbers as dropped items / player).
    static constexpr float kGravity = 32.0f;
    static constexpr float kTerminalVelocity = 38.0f;
    // Wander walks at half the chase speed (named, not magic).
    static constexpr float kWanderSpeedFactor = 0.5f;
    // Chase/attack only at comparable height (vertical band, blocks).
    static constexpr float kDetectVertical = 2.0f;
    static constexpr float kAttackVertical = 1.0f;
    // Knockback window: the impulse decays (kKnockbackDamping per second)
    // while the AI holds off; then steering resumes.
    static constexpr double kKnockbackTime = 0.3;
    static constexpr float kKnockbackDamping = 6.0f;
    // The ranged-shot origin as a fraction of the model height (the head).
    static constexpr float kEyeHeightFraction = 0.8f;
    // ENT-4: the underwater horizontal speed gate — the AI steering AND
    // the knockback slide are multiplied by this while in fluid (or at
    // its surface, the player's m_atSurface swim-pace analog). A named
    // constant, not a per-mob data field (documented YAGNI deviation:
    // every mob swims at the same pace, like the player's single
    // swimSpeed).
    static constexpr float kUnderwaterSpeedFactor = 0.6f;
};

} // namespace mc
