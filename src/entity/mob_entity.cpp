#include "entity/mob_entity.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "world/noise.hpp"

namespace mc {

namespace {

// Advances the splitmix64 stream and returns the next mixed value (the
// standard stream: state += gamma, result = mix(state)).
std::uint64_t nextMix(std::uint64_t& state) {
    state += 0x9E3779B97F4A7C15ULL;
    return splitmix64(state);
}

// 53-bit uniform in [0, 1) from the next stream value.
double uniform01(std::uint64_t& state) {
    return static_cast<double>(nextMix(state) >> 11) /
           static_cast<double>(1ull << 53);
}

} // namespace

MobEntity::MobEntity(const World& world, const BlockRegistry& blocks,
                     const Mob& def, Vec3 pos, std::uint64_t rngSeed)
    : Entity(world, blocks, pos, def.model.width, def.model.height),
      m_def(&def),
      m_health(def.properties.health),
      m_rng(rngSeed) {}

void MobEntity::update(double dt) {
    if (m_dead || dt <= 0.0) {
        return;
    }
    if (m_attackCooldown > 0.0) {
        m_attackCooldown = std::max(0.0, m_attackCooldown - dt);
    }
    if (m_knockbackTime > 0.0) {
        m_knockbackTime = std::max(0.0, m_knockbackTime - dt);
    }
    // The knockback window suppresses AI steering: the impulse carries the
    // mob (decayed by integrate) and the AI resumes after.
    if (m_knockbackTime <= 0.0) {
        runAi(dt);
    }
    // Peak-speed estimate for the sub-step count: this tick's post-gravity
    // vertical velocity (the horizontal one only decays or is reset).
    const float vyAfter = std::max(
        m_velocity.y - kGravity * static_cast<float>(dt), -kTerminalVelocity);
    const float maxSpeed =
        std::max({std::abs(m_velocity.x), std::abs(vyAfter),
                  std::abs(m_velocity.z)});
    moveInSubsteps(dt, maxSpeed);
}

void MobEntity::integrate(float h) {
    // Gravity, capped at terminal velocity. No ground friction: the AI
    // sets the exact data speed every frame (a friction decay would make
    // the walking speed a function of the sub-step count). During the
    // knockback window the impulse decays exponentially instead.
    m_velocity.y -= kGravity * h;
    if (m_velocity.y < -kTerminalVelocity) {
        m_velocity.y = -kTerminalVelocity;
    }
    if (m_knockbackTime > 0.0) {
        const float f = std::exp(-kKnockbackDamping * h);
        m_velocity.x *= f;
        m_velocity.z *= f;
    }
}

void MobEntity::runAi(double dt) {
    const MobProperties& p = m_def->properties;
    const Vec3 pos = position();
    const float dx = m_playerPos.x - pos.x;
    const float dz = m_playerPos.z - pos.z;
    const float distH = std::hypot(dx, dz);
    const float dy = m_playerPos.y - pos.y;
    const bool seesPlayer = m_playerAlive && distH <= p.detectRadius &&
                            std::abs(dy) <= kDetectVertical;

    if (seesPlayer && distH <= p.attackRange &&
        std::abs(dy) <= kAttackVertical) {
        // Attack: stand still, face the player, swing on the cooldown.
        m_state = MobState::Attack;
        m_velocity.x = 0.0f;
        m_velocity.z = 0.0f;
        if (distH > 1e-4f) {
            // Camera yaw convention: forward(yaw) = (-sin, 0, -cos).
            m_yaw = std::atan2(-dx, -dz);
        }
        if (m_attackCooldown <= 0.0) {
            m_attackCooldown = p.attackCooldown;
            if (m_hitPlayer) {
                m_hitPlayer(p.attackDamage);
            }
        }
        return;
    }

    if (seesPlayer) {
        // Chase: full data speed toward the player's bottom-center.
        m_state = MobState::Chase;
        const float inv = 1.0f / std::max(distH, 1e-4f);
        m_velocity.x = dx * inv * p.speed;
        m_velocity.z = dz * inv * p.speed;
        m_yaw = std::atan2(-dx, -dz);
        return;
    }

    // Wander: a seeded decision every wander_interval (the first update
    // decides immediately: the timer starts at 0).
    m_state = MobState::Wander;
    m_wanderTimer -= dt;
    if (m_wanderTimer > 0.0) {
        return;  // keep the current heading / stand-still decision
    }
    m_wanderTimer = p.wanderInterval;
    if (nextMix(m_rng) % 3 == 0) {
        m_velocity.x = 0.0f;
        m_velocity.z = 0.0f;
        return;
    }
    const float angle =
        static_cast<float>(uniform01(m_rng) * 2.0 * std::numbers::pi);
    m_yaw = angle;
    const float s = p.speed * kWanderSpeedFactor;
    m_velocity.x = -std::sin(angle) * s;
    m_velocity.z = -std::cos(angle) * s;
}

bool MobEntity::damage(float amount) {
    if (m_dead || amount <= 0.0f) {
        return m_dead;
    }
    m_health -= amount;
    if (m_health <= 0.0f) {
        m_health = 0.0f;
        m_dead = true;
    }
    return m_dead;
}

void MobEntity::addKnockback(Vec3 impulse) {
    m_velocity += impulse;
    m_knockbackTime = kKnockbackTime;
}

} // namespace mc
