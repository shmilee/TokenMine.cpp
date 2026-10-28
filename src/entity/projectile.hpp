// Projectile: a fast-moving entity (Phase 10; launched in-game by the bow
// since Phase 21).
//
// Physics: launched with a direction + speed (kLaunchSpeed is Minecraft's
// full-power arrow), the same sub-stepped collision as every entity, and a
// reduced gravity (Minecraft's arrow: 0.05 blocks/tick^2 = 20 blocks/s^2).
// The first sub-step in which a solid block stops one of its axes STICKS
// it (Phase 24, the Phase 10 header's original "撞墙钉住" intent): the
// arrow stays where it hit, keeps the incoming direction for rendering
// (stuckDir — the collision resolver zeroes the blocked axis of the
// velocity), and despawns after kStuckMaxAge (Phase 42: Minecraft's 100 s
// for stuck arrows; in-flight arrows keep the 30 s kMaxAge). If the block
// it is stuck in is broken or replaced, the game releases it (release()):
// it falls from where it hung and rejoins flight physics (gravity, hit
// tests) with its cumulative age (docs/KNOWN_ISSUES.md).
// It also despawns after its age cap or below the kill plane.
//
// Combat (Phase 21): a projectile may carry `damage` (set at launch from
// the launcher item's data). The GAME runs the swept arrow-vs-mob hit test
// each frame (an arrow flies through every entity — no separation — so the
// hit check is not part of the entity physics); on a hit the game damages
// the mob and consumes the arrow via kill(). Arrows flagged `hurtPlayer`
// (Phase 27: mob-shot arrows, the skeleton) are ALSO swept-tested against
// the player's AABB and damage it; player-shot bow arrows are not flagged
// (no self-damage, single-player v1 — see docs/KNOWN_ISSUES.md).
//
// Pickup (Phase 42): a stuck arrow carries the launcher's ammo item name
// (`ammoItem`, empty = not pickable — the Phase 10 bare projectile). The
// game returns it to the player's inventory when a stuck arrow comes
// within kArrowPickupRadius of the player (Minecraft: walk up to a stuck
// arrow to pick it up).
//
// Knockback (Phase 42): `power` (the shot's charge, 1.0 = full power,
// Phase 10/27 defaults) scales the knockback the game applies on a mob
// hit (core/bow.hpp arrowKnockback).
//
// SURV-5: the arrow may carry a status effect (`effectId`, default -1 =
// none): the game applies it on a landed hit — to the player (the
// skeleton's wither arrow, the Game::applyPlayerEffect choke point) or
// to the mob (the poison bow's arrow, MobEntity::applyEffect's gates).
//
// v1 simplification (docs/KNOWN_ISSUES.md): arrows are not saved (transient
// like mobs).
#pragma once

#include <string>

#include "entity/entity.hpp"

namespace mc {

class MobEntity; // the shooter of a mob-shot arrow (Phase 27)

class Projectile : public Entity {
public:
    static constexpr float kSize = 0.25f;        // collision box
    static constexpr float kLaunchSpeed = 15.0f; // Minecraft full-power arrow
    static constexpr double kMaxAge = 30.0; // Minecraft arrow: 600 ticks
    // Phase 42: a STUCK arrow outlives a flying one (Minecraft keeps
    // stuck arrows 100 s). The age is cumulative — a released (block
    // broken) arrow keeps its age and is then capped by kMaxAge.
    static constexpr double kStuckMaxAge = 100.0;
    // Minecraft's arrow gravity (0.05 blocks/tick^2 at 20 ticks/s).
    // PUBLIC because the ranged-mob aim (Phase 27) must lead the target by
    // the arrow's arc drop (half this over the flight time).
    static constexpr float kGravity = 20.0f;

    // `direction` does not need to be normalized (it is scaled to
    // `speed`). `damage` is the per-hit damage a mob takes when this
    // projectile is the one that hits it (Phase 21; the game runs the hit
    // test). Defaults to 0 = no damage (the Phase 10 bare projectile).
    // `hurtPlayer` (Phase 27, default false): the game's swept test also
    // runs this arrow against the player's AABB (mob-shot arrows); the
    // player's own bow arrows stay unflagged (no self-damage).
    // `power` (Phase 42, default 1.0 = full power): the shot's charge —
    // it scales the knockback the game applies on a mob hit.
    // `ammoItem` (Phase 42, default empty = not pickable): the launcher's
    // ammo item name, returned to the player's inventory when a stuck
    // arrow is picked up.
    // SURV-5: `effectId` (default -1 = NO arrow effect — every
    // pre-SURV-5 call site is bit-identical) + `effectDuration`: the
    // status effect the game applies on a landed hit (an int id, not a
    // name — zero heap allocation; the init-cross-checked name resolves
    // to the id at fire time). arrow->player: the effect goes to the
    // player (the skeleton's wither arrow, through
    // Game::applyPlayerEffect); arrow->mob: to the mob (the poison bow's
    // arrow, through MobEntity::applyEffect's gates).
    Projectile(const World& world, const BlockRegistry& blocks, Vec3 pos,
               Vec3 direction, float speed = kLaunchSpeed,
               float damage = 0.0f, bool hurtPlayer = false,
               float power = 1.0f, std::string ammoItem = {},
               int effectId = -1, double effectDuration = 0.0,
               int effectTier = 0);

    void update(double dt) override;
    bool shouldDespawn() const override {
        // Phase 42: a stuck arrow ages out at kStuckMaxAge; a flying one
        // (including a released arrow — the age is cumulative) at kMaxAge.
        return m_dead ||
               m_age >= (m_stuck ? kStuckMaxAge : kMaxAge) ||
               belowKillPlane();
    }
    bool dead() const { return m_dead; }
    // True once a solid block stopped one of the axes (Phase 24): the
    // arrow is stuck where it hit — no further physics, no mob hit tests,
    // and it despawns when its age (which keeps running) reaches kMaxAge.
    bool stuck() const { return m_stuck; }
    // The incoming direction at the moment the arrow stuck (normalized;
    // the render pass orients a stuck arrow with this instead of the
    // velocity, whose blocked axis the collision resolver zeroed).
    Vec3 stuckDir() const { return m_stuckDir; }
    double age() const { return m_age; }
    // The per-hit damage this projectile deals (Phase 21, set at launch
    // from the launcher item's 'shoot_damage' data).
    float damage() const { return m_damage; }
    // The shot's charge (Phase 42, 1.0 = full power): scales the mob-hit
    // knockback (core/bow.hpp arrowKnockback).
    float power() const { return m_power; }
    // The launcher's ammo item name (Phase 42, empty = not pickable):
    // returned to the player's inventory when a stuck arrow is picked up.
    const std::string& ammoItem() const { return m_ammoItem; }
    // SURV-5: the effect id this arrow applies on a landed hit
    // (-1 = no effect, the pre-SURV-5 default) + its duration (seconds;
    // 0 with -1).
    int effectId() const { return m_effectId; }
    double effectDuration() const { return m_effectDuration; }
    // SURV-6: the tier the arrow applies (0-based; 0 = the pre-SURV-6
    // value, the trailing default keeps every legacy call site
    // bit-identical).
    int effectTier() const { return m_effectTier; }
    // The game calls this when the block the arrow is stuck in is broken
    // or replaced (Phase 42): the arrow falls from where it hung (zero
    // velocity + gravity) and rejoins flight physics. No-op when not
    // stuck.
    void release() {
        if (m_stuck) {
            m_stuck = false;
            m_velocity = {0.0f, 0.0f, 0.0f};
        }
    }
    // True when the game's swept test may hit the player with this arrow
    // (Phase 27: mob-shot arrows).
    bool hurtPlayer() const { return m_hurtPlayer; }
    // The mob that launched this arrow (Phase 27; null for the player's
    // bow arrows). The game's swept arrow-vs-mob test SKIPS the source —
    // the spawn point sits inside the shooter's own AABB (head + the 0.3
    // block launch offset < the body's half-width), so without the
    // exclusion every arrow would hit its own shooter on the next frame
    // (t = 0: starts inside the box) and self-consume.
    MobEntity* source() const { return m_source; }
    void setSource(MobEntity* s) { m_source = s; }

    // Consumed by a NON-block source (a mob hit, Phase 21): the same
    // removal path as a block impact (shouldDespawn) but no sticking
    // semantics. The game calls this when its swept hit test lands.
    void kill() { m_dead = true; }

    // Entity separation (Phase 19): projectiles fly through every entity
    // (Minecraft arrows pass through items/mobs) — the separation pass
    // skips them entirely.
    bool collidesWithEntities() const override { return false; }

private:
    void integrate(float h) override;
    void afterSubstep(bool blocked) override;

    bool m_dead = false;   // consumed by a non-block source (kill, Phase 21)
    bool m_stuck = false;  // a solid block stopped one of the axes (Phase 24)
    Vec3 m_stuckDir {0.0f, 0.0f, -1.0f}; // incoming direction at impact
    double m_age = 0.0;
    float m_damage = 0.0f; // per-hit mob damage (Phase 21, set at launch)
    bool m_hurtPlayer = false; // swept test may hit the player (Phase 27)
    MobEntity* m_source = nullptr; // shooter (skipped by the hit test)
    float m_power = 1.0f; // shot charge, scales mob-hit knockback (Phase 42)
    std::string m_ammoItem; // pickup returns this (Phase 42, "" = none)
    // SURV-5: the arrow's status effect (id -1 = none, the default —
    // every pre-SURV-5 arrow is bit-identical) + its duration.
    int m_effectId = -1;
    double m_effectDuration = 0.0;
    int m_effectTier = 0; // SURV-6 (0-based; 0 = the pre-SURV-6 value)

    // Same fall cap as the player: keeps the sub-stepper's tunneling bound
    // (an uncapped arrow could reach 600 blocks/s after 30 s).
    static constexpr float kTerminalVelocity = 38.0f;
};

// Aim direction for a gravity-affected projectile (Phase 27's arc
// compensation, Phase 38's target leading). From `origin`, launched at
// `speed`, where to aim so the arrow meets `target` while it moves at
// `targetVel` (blocks/second, the player's velocity at shot time): the
// flight time t = |aim| / speed is iterated twice against the predicted
// position (target + targetVel * t), then the final direction is
// normalized(aim + (0, 0.5 * g * t^2, 0)) — the arc drop over the flight
// time. A STATIONARY target (zero velocity) reproduces the Phase 27
// formula bit-exactly (the second iteration is a no-op), so the leading
// changes nothing for a standing player. Pure math — unit-testable
// without a world.
Vec3 computeAimDirection(Vec3 origin, Vec3 target, Vec3 targetVel,
                         float speed);

} // namespace mc
