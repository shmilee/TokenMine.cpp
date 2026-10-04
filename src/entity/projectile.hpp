// Projectile: a fast-moving entity that despawns on impact (Phase 10).
//
// Physics: launched with a direction + speed (kLaunchSpeed is Minecraft's
// full-power arrow), the same sub-stepped collision as every entity, and a
// reduced gravity (Minecraft's arrow: 0.05 blocks/tick^2 = 20 blocks/s^2).
// The first sub-step in which a solid block stops one of its axes marks it
// dead — Minecraft arrows stick where they hit. It also despawns after
// kMaxAge (Minecraft's 600 ticks) or below the kill plane.
//
// There is no in-game launcher yet (a bow is a later phase); the entity and
// its impact/despawn paths are exercised by the unit tests and will be fed
// by the bow when it lands. See KNOWN_ISSUES.md.
#pragma once

#include "entity/entity.hpp"

namespace mc {

class Projectile : public Entity {
public:
    static constexpr float kSize = 0.25f;       // collision box
    static constexpr float kLaunchSpeed = 15.0f; // Minecraft full-power arrow
    static constexpr double kMaxAge = 30.0; // Minecraft arrow: 600 ticks

    // `direction` does not need to be normalized (it is scaled to `speed`).
    Projectile(const World& world, const BlockRegistry& blocks, Vec3 pos,
               Vec3 direction, float speed = kLaunchSpeed);

    void update(double dt) override;
    bool shouldDespawn() const override {
        return m_dead || m_age >= kMaxAge || belowKillPlane();
    }
    bool dead() const { return m_dead; }
    double age() const { return m_age; }

    // Entity separation (Phase 19): projectiles fly through every entity
    // (Minecraft arrows pass through items/mobs) — the separation pass
    // skips them entirely.
    bool collidesWithEntities() const override { return false; }

private:
    void integrate(float h) override;
    void afterSubstep(bool blocked) override;

    bool m_dead = false; // a solid block stopped one of the axes
    double m_age = 0.0;

    // Minecraft's arrow gravity: 0.05 blocks/tick^2 at 20 ticks/s.
    static constexpr float kGravity = 20.0f;
    // Same fall cap as the player: keeps the sub-stepper's tunneling bound
    // (an uncapped arrow could reach 600 blocks/s after 30 s).
    static constexpr float kTerminalVelocity = 38.0f;
};

} // namespace mc
