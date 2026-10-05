// MobEntity: a data-driven hostile mob (Phase 20).
//
// Physics: the shared Entity AABB physics (gravity / terminal velocity,
// sub-stepped axis moves). Unlike dropped items there is NO ground
// friction: the AI sets the horizontal velocity every frame (exact data
// speed), and knockback decays with its own damping while the knockback
// window is open.
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

    // Subtract damage; returns true when THIS hit killed the mob.
    bool damage(float amount);
    // Knockback impulse; suppresses AI steering for kKnockbackTime.
    void addKnockback(Vec3 impulse);

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

private:
    void integrate(float h) override;
    void runAi(double dt);

    const Mob* m_def;
    float m_health = 0.0f;
    bool m_dead = false;
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
};

} // namespace mc
