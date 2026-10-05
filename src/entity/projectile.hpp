// Projectile: a fast-moving entity that despawns on impact (Phase 10;
// launched in-game by the bow since Phase 21).
//
// Physics: launched with a direction + speed (kLaunchSpeed is Minecraft's
// full-power arrow), the same sub-stepped collision as every entity, and a
// reduced gravity (Minecraft's arrow: 0.05 blocks/tick^2 = 20 blocks/s^2).
// The first sub-step in which a solid block stops one of its axes marks it
// dead — the arrow stops where it hits. It also despawns after kMaxAge
// (Minecraft's 600 ticks) or below the kill plane.
//
// Combat (Phase 21): a projectile may carry `damage` (set at launch from
// the launcher item's data). The GAME runs the swept arrow-vs-mob hit test
// each frame (an arrow flies through every entity — no separation — so the
// hit check is not part of the entity physics); on a hit the game damages
// the mob and consumes the arrow via kill(). Arrows do not damage the
// player (single-player v1; see KNOWN_ISSUES.md).
//
// v1 simplifications (KNOWN_ISSUES.md): no charge-up (a shot always fires
// at the item's full data speed), arrows are not items (no ammo to
// consume), and an impact removes the arrow (no stuck arrows).
#pragma once

#include "entity/entity.hpp"

namespace mc {

class Projectile : public Entity {
public:
    static constexpr float kSize = 0.25f;       // collision box
    static constexpr float kLaunchSpeed = 15.0f; // Minecraft full-power arrow
    static constexpr double kMaxAge = 30.0; // Minecraft arrow: 600 ticks

    // `direction` does not need to be normalized (it is scaled to
    // `speed`). `damage` is the per-hit damage a mob takes when this
    // projectile is the one that hits it (Phase 21; the game runs the hit
    // test). Defaults to 0 = no damage (the Phase 10 bare projectile).
    Projectile(const World& world, const BlockRegistry& blocks, Vec3 pos,
               Vec3 direction, float speed = kLaunchSpeed,
               float damage = 0.0f);

    void update(double dt) override;
    bool shouldDespawn() const override {
        return m_dead || m_age >= kMaxAge || belowKillPlane();
    }
    bool dead() const { return m_dead; }
    double age() const { return m_age; }
    // The per-hit damage this projectile deals (Phase 21, set at launch
    // from the launcher item's 'shoot_damage' data).
    float damage() const { return m_damage; }

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

    bool m_dead = false; // a solid block stopped one of the axes
    double m_age = 0.0;
    float m_damage = 0.0f; // per-hit mob damage (Phase 21, set at launch)

    // Minecraft's arrow gravity: 0.05 blocks/tick^2 at 20 ticks/s.
    static constexpr float kGravity = 20.0f;
    // Same fall cap as the player: keeps the sub-stepper's tunneling bound
    // (an uncapped arrow could reach 600 blocks/s after 30 s).
    static constexpr float kTerminalVelocity = 38.0f;
};

} // namespace mc
