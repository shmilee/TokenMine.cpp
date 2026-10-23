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
      m_rng(rngSeed),
      // Fire v3 (Phase 73): the fire-block contact source, resolved once
      // (invalid = no fire block registered = structurally inert, the
      // P70 player pattern).
      m_fireBlockId(blocks.idForName("fire")) {}

void MobEntity::update(double dt) {
    if (m_dead || dt <= 0.0) {
        return;
    }
    // ENT-1: captured BEFORE the AI + physics so the "moving" decision
    // is the strict frame-boundary horizontal displacement (see below).
    const float prevX = position().x;
    const float prevZ = position().z;
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

    // ENT-1: advance the limb-swing phase (render-only, never saved —
    // mobs are ephemeral). "Moving" = the STRICT frame-boundary
    // horizontal displacement (hard rule 8: frame-locked, no AI intent
    // and no real dt read — a knocked-back or falling-straight-down mob
    // swings or rests exactly as its position says). Only types with
    // swing data advance (structural gate: a swing-less model's phase
    // stays 0 forever = its render fast path is the static mesh).
    if (!m_def->model.swings.empty()) {
        const Vec3 pos = position();
        const bool moving = pos.x != prevX || pos.z != prevZ;
        m_swingPhase = advanceSwingPhase(m_swingPhase, moving);
    }

    // Fire system (Phase 58, v3 in Phase 73): the lava AND the fire
    // block ignite the mob with the player's fire (same effect
    // definition + config duration — the game arms it at spawn; an
    // unarmed mob never burns). The checks use the FINAL position of
    // this step (the AI + physics ran above). While in the lava or
    // overlapping a fire block the burn re-ignites (the duration resets
    // = continuous burn); water extinguishes instantly BEFORE the tick
    // (zero burn damage in water; water beats both, the P57 rule). The
    // countdown + tick run after leaving (the lingering burn), with the
    // Phase 46 effect semantics (the expiry step does not tick).
    if (m_fireArmed) {
        const bool inLava = overlapsFluid(FluidKind::Lava);
        // Fire v3 (Phase 73): the block twin of the lava ignition — the
        // strict AABB overlap of a fire-block cell (the cross-model fire,
        // never a fluid; the invalid id = never, resolved at
        // construction).
        const bool inFire = overlapsBlock(m_fireBlockId);
        const bool inWater = overlapsFluid(FluidKind::Water);
        if (inLava || inFire) {
            m_fireRemaining = m_fireDuration;
        }
        if (inWater) {
            m_fireRemaining = 0.0;
            m_fireFrames = 0;
            m_fireDueFrames = m_fireIntervalFrames;
        }
        if (m_fireRemaining > 0.0) {
            m_fireRemaining -= dt;
            if (m_fireRemaining > 0.0) {
                // Tick on the integer frame grid: tick k lands on burning
                // frame k * intervalFrames (exact — no float phase drift).
                ++m_fireFrames;
                while (m_fireFrames >= m_fireDueFrames) {
                    m_fireDueFrames += m_fireIntervalFrames;
                    if (damage(m_fireAmount)) {
                        break;
                    }
                }
            } else {
                // Expired: clean the tick phase for the next ignition.
                m_fireFrames = 0;
                m_fireDueFrames = m_fireIntervalFrames;
            }
        }
    }
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

    if (seesPlayer && p.shootRange > 0.0f && distH <= p.shootRange) {
        // Ranged attack (Phase 27, the skeleton): face the player and fire
        // on the cooldown. While the player is inside retreat_distance the
        // mob backs away at full data speed (Minecraft's kite); otherwise
        // it stands and shoots. It never closes in to melee — a ranged
        // mob's data attack_range is its (unused) melee range, 0 for the
        // skeleton, so the melee branch below can never hold for it.
        m_state = MobState::RangedAttack;
        if (distH > 1e-4f) {
            // Camera yaw convention: forward(yaw) = (-sin, 0, -cos).
            m_yaw = std::atan2(-dx, -dz);
        }
        if (distH <= p.retreatDistance) {
            const float inv = 1.0f / std::max(distH, 1e-4f);
            m_velocity.x = -dx * inv * p.speed;
            m_velocity.z = -dz * inv * p.speed;
        } else {
            m_velocity.x = 0.0f;
            m_velocity.z = 0.0f;
        }
        if (m_attackCooldown <= 0.0) {
            m_attackCooldown = p.attackCooldown;
            if (m_shootPlayer) {
                m_shootPlayer();
            }
        }
        return;
    }

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

int MobEntity::rollDropCount(int minCount, int maxCount) {
    if (minCount >= maxCount) {
        return minCount;
    }
    // Uniform over the (max - min + 1) integer outcomes; the stream
    // advances exactly once (the wander-decision convention).
    const std::uint64_t span =
        static_cast<std::uint64_t>(maxCount - minCount + 1);
    return minCount + static_cast<int>(nextMix(m_rng) % span);
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

void MobEntity::armFire(double tickInterval, float tickAmount,
                        double duration) {
    m_fireArmed = true;
    m_fireAmount = tickAmount;
    m_fireDuration = duration;
    // The mob steps on the fixed 1/60 frame grid (hard rule 8), so the
    // cadence is exact in integer frame units: interval * 60 (the game
    // init validates the ignites effect's interval is a whole number of
    // frames — for the shipped fire, 0.2 s = 12 frames — so the rounding
    // is exact and tick k lands on burning frame k * intervalFrames).
    m_fireIntervalFrames = static_cast<int>(std::llround(tickInterval * 60.0));
    m_fireDueFrames = m_fireIntervalFrames;
}

} // namespace mc
