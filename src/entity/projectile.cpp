#include "entity/projectile.hpp"

#include <algorithm>
#include <cmath>

#include "core/logger.hpp"
#include "core/math/vec3.hpp"

namespace mc {

Projectile::Projectile(const World& world, const BlockRegistry& blocks,
                       Vec3 pos, Vec3 direction, float speed, float damage,
                       bool hurtPlayer, float power, std::string ammoItem)
    : Entity(world, blocks, pos, kSize, kSize), m_damage(damage),
      m_hurtPlayer(hurtPlayer), m_power(power), m_ammoItem(std::move(ammoItem)) {
    m_velocity = normalized(direction) * speed;
}

void Projectile::update(double dt) {
    if (dt <= 0.0 || m_dead) {
        return;
    }
    m_age += dt;
    if (m_stuck) {
        // A stuck arrow (Phase 24) no longer integrates: it ages in place
        // until the kMaxAge despawn (the age line above keeps running).
        return;
    }
    // Peak-speed estimate for the sub-step count: this tick's post-gravity
    // vertical velocity (the horizontal one is constant).
    const float vyAfter = std::max(
        m_velocity.y - kGravity * static_cast<float>(dt), -kTerminalVelocity);
    const float maxSpeed =
        std::max({std::abs(m_velocity.x), std::abs(vyAfter),
                  std::abs(m_velocity.z)});
    // Capture the incoming direction before the sub-steps: if a solid
    // stops one of the axes this tick, the collision resolver zeroes that
    // axis of m_velocity, and the stuck render orientation (stuckDir)
    // needs the pre-impact direction (the per-sub-step gravity delta over
    // one 1/60 s tick is orientation-negligible).
    m_stuckDir = normalized(m_velocity);
    moveInSubsteps(dt, maxSpeed);
}

void Projectile::integrate(float h) {
    m_velocity.y -= kGravity * h;
    if (m_velocity.y < -kTerminalVelocity) {
        m_velocity.y = -kTerminalVelocity;
    }
}

void Projectile::afterSubstep(bool blocked) {
    if (blocked && !m_stuck) {
        // A solid block stopped one of the axes: the arrow sticks where
        // it hit (Phase 24 — the Phase 10 "撞墙钉住" intent). It keeps
        // rendering (oriented along m_stuckDir) and despawns by age.
        m_stuck = true;
        if (m_hurtPlayer) {
            const Vec3 p = position();
            MC_LOG_DEBUG("mob arrow stuck in block at (" +
                         std::to_string(p.x) + ", " + std::to_string(p.y) +
                         ", " + std::to_string(p.z) + ")");
        }
    }
}

Vec3 computeAimDirection(Vec3 origin, Vec3 target, Vec3 targetVel,
                         float speed) {
    // Predict the target's position at the (iterated) flight time, twice:
    // the first pass uses the straight-line time, the second re-solves
    // against the predicted position (the player's speed is bounded and
    // the second correction is already sub-block — two passes is the same
    // practical convergence Minecraft's own predictor settles for).
    Vec3 to = target - origin;
    float dist = std::sqrt(dot(to, to));
    for (int pass = 0; pass < 2; ++pass) {
        const float t = dist / speed;
        to = (target + targetVel * t) - origin;
        dist = std::sqrt(dot(to, to));
    }
    // Arc compensation (Phase 27): the arrow drops 0.5 * g * t^2 over the
    // flight, so aim ABOVE the predicted point by exactly that drop.
    const float t = dist / speed;
    to.y += 0.5f * Projectile::kGravity * t * t;
    return normalized(to);
}

} // namespace mc
