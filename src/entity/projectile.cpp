#include "entity/projectile.hpp"

#include <algorithm>
#include <cmath>

namespace mc {

Projectile::Projectile(const World& world, const BlockRegistry& blocks,
                       Vec3 pos, Vec3 direction, float speed, float damage)
    : Entity(world, blocks, pos, kSize, kSize), m_damage(damage) {
    m_velocity = normalized(direction) * speed;
}

void Projectile::update(double dt) {
    if (dt <= 0.0 || m_dead) {
        return;
    }
    m_age += dt;
    // Peak-speed estimate for the sub-step count: this tick's post-gravity
    // vertical velocity (the horizontal one is constant).
    const float vyAfter = std::max(
        m_velocity.y - kGravity * static_cast<float>(dt), -kTerminalVelocity);
    const float maxSpeed =
        std::max({std::abs(m_velocity.x), std::abs(vyAfter),
                  std::abs(m_velocity.z)});
    moveInSubsteps(dt, maxSpeed);
}

void Projectile::integrate(float h) {
    m_velocity.y -= kGravity * h;
    if (m_velocity.y < -kTerminalVelocity) {
        m_velocity.y = -kTerminalVelocity;
    }
}

void Projectile::afterSubstep(bool blocked) {
    if (blocked) {
        // A solid block stopped one of the axes: the arrow sticks.
        m_dead = true;
    }
}

} // namespace mc
