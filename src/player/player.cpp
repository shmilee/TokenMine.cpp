#include "player/player.hpp"

#include <algorithm>
#include <cmath>

#include "block/block.hpp"
#include "block/block_registry.hpp"
#include "core/math/vec2.hpp"

namespace mc {

Player::Player(const World& world, const BlockRegistry& blocks, Vec3 spawnPos)
    : Entity(world, blocks, spawnPos, Params {}.width, Params {}.height),
      m_spawnPos(spawnPos) {
    teleportTo(spawnPos);
}

bool Player::hasRoomAt(const World& world, const BlockRegistry& blocks,
                       Vec3 bottomCenter, float width, float height) {
    const float half = width * 0.5f;
    const Vec3 mn {bottomCenter.x - half, bottomCenter.y,
                   bottomCenter.z - half};
    const Vec3 mx {bottomCenter.x + half, bottomCenter.y + height,
                   bottomCenter.z + half};

    const int x0 = static_cast<int>(std::floorf(mn.x));
    const int x1 = static_cast<int>(std::floorf(mx.x));
    const int y0 = static_cast<int>(std::floorf(mn.y));
    const int y1 = static_cast<int>(std::floorf(mx.y));
    const int z0 = static_cast<int>(std::floorf(mn.z));
    const int z1 = static_cast<int>(std::floorf(mx.z));
    for (int x = x0; x <= x1; ++x) {
        for (int y = y0; y <= y1; ++y) {
            for (int z = z0; z <= z1; ++z) {
                if (!(mn.x < x + 1 && mx.x > x)) {
                    continue;
                }
                if (!(mn.y < y + 1 && mx.y > y)) {
                    continue;
                }
                if (!(mn.z < z + 1 && mx.z > z)) {
                    continue;
                }
                const BlockID id = world.getBlock(x, y, z);
                if (id == BlockID::air() || !id.valid()) {
                    continue;
                }
                if (blocks.at(id).properties.solid) {
                    return false;
                }
            }
        }
    }
    return true;
}

void Player::update(double dt, const Input& input, bool lookEnabled) {
    m_input = &input;
    m_lookEnabled = lookEnabled;
    update(dt);
}

void Player::update(double dt) {
    if (dt <= 0.0 || m_dead) {
        return; // a dead player is frozen; the game re-enters via respawn()
    }

    // 1. Mouse look (only while the pointer is captured).
    if (m_lookEnabled) {
        const Vec2 mouse = m_input->mouseDelta();
        yaw -= mouse.x * m_params.mouseSensitivity;
        pitch -= mouse.y * m_params.mouseSensitivity;
        pitch = std::clamp(pitch, -kMaxPitch, kMaxPitch);
    }

    // 2. Desired horizontal velocity (instantaneous, no inertia).
    const float cp = std::cos(pitch);
    const Vec3 flatFwd {std::sin(yaw) * -cp, 0.0f, std::cos(yaw) * -cp};
    const Vec3 fwd = normalized(flatFwd);
    const Vec3 right {fwd.z * -1.0f, 0.0f, fwd.x};

    // 2a. Water state: while the AABB overlaps a fluid cell the player
    // swims (reduced gravity/sink, reduced speed, Space swims up).
    m_inWater = isInWater();

    // 2b. Crouch (LeftControl): the height state machine runs before the
    // speed computation (crouch scales the speed) and before the edge
    // guard (crouching on the ground holds at ledges).
    m_crouching = m_input->isDown(GameKey::LeftControl);
    updateCrouchHeight();

    Vec3 wish {0.0f, 0.0f, 0.0f};
    if (m_input->isDown(GameKey::W)) {
        wish += fwd;
    }
    if (m_input->isDown(GameKey::S)) {
        wish -= fwd;
    }
    if (m_input->isDown(GameKey::D)) {
        wish += right;
    }
    if (m_input->isDown(GameKey::A)) {
        wish -= right;
    }
    Vec3 dir {0.0f, 0.0f, 0.0f};
    if (lengthSq(wish) > 0.0f) {
        float speed = m_inWater
                          ? m_params.swimSpeed
                          : (m_input->isDown(GameKey::LeftShift)
                                  ? m_params.sprintSpeed
                                  : m_params.walkSpeed);
        if (m_crouching && !m_inWater) {
            speed *= m_params.crouchSpeedScale;
        }
        dir = normalized(wish);
        m_velocity.x = dir.x * speed;
        m_velocity.z = dir.z * speed;
        // Crouching on the ground: do not walk off a 1-block ledge.
        if (m_crouching && m_onGround) {
            applyCrouchEdgeGuard(dir, dt);
        }
    } else {
        m_velocity.x = 0.0f;
        m_velocity.z = 0.0f;
    }

    // 3. Sub-step the tick (see Entity::moveInSubsteps); the peak-speed
    //    estimate uses this tick's post-gravity vertical velocity.
    const float vyAfter =
        m_inWater
            ? std::max(m_velocity.y - m_params.waterGravity *
                                      static_cast<float>(dt),
                       -m_params.waterTerminalVelocity)
            : std::max(m_velocity.y - m_params.gravity *
                           static_cast<float>(dt),
                       -m_params.terminalVelocity);
    const float maxSpeed =
        std::max({std::abs(m_velocity.x), std::abs(vyAfter),
                  std::abs(m_velocity.z)});
    // The tick's horizontal speed before the sub-steps (a clamped axis
    // zeroes its velocity inside the sub-steps; stepUp needs the value).
    const float preMoveSpeed =
        std::max(std::abs(m_velocity.x), std::abs(m_velocity.z));
    moveInSubsteps(dt, maxSpeed);

    // 3b. Step up 1 block when flush against a step (not crouching, on
    // the ground).
    if (!m_crouching && m_onGround && lengthSq(wish) > 0.0f) {
        stepUp(dir, preMoveSpeed * static_cast<float>(dt));
    }

    // 4. Survival (Phase 16): fall damage on landing, regeneration, and
    // the kill plane is now DEATH (the old respawn-teleport is gone).
    if (m_onGround && !m_wasOnGround) {
        // Just landed: the net drop since leaving the ground. No damage
        // for the first fallDamageOffset blocks, 1 HP per block above
        // (Minecraft's rule); landing in water cancels the fall damage.
        // The water check is FRESH (not the tick's m_inWater): a falling
        // player enters a 1-deep pool one tick after the last airborne
        // sample, so the tick state would miss the landing-in-water.
        const float fall = m_fallStartY - m_position.y;
        if (fall > m_params.fallDamageOffset && !isInWater()) {
            damage(fall - m_params.fallDamageOffset);
        }
        m_fallStartY = m_position.y;
    }
    if (!m_onGround && m_wasOnGround) {
        m_fallStartY = m_position.y; // just left the ground
    }
    m_wasOnGround = m_onGround;

    // Regeneration is GATED by hunger (Phase 23, Minecraft's rule: below
    // the threshold there is no natural regen). The accumulator pauses
    // rather than resets while the gate is closed, so a full meal makes
    // the pending progress count.
    if (m_timeSinceDamage >= m_params.regenDelaySeconds &&
        m_hunger >= m_params.regenHungerThreshold) {
        m_regenAccum += dt;
        while (m_regenAccum >= m_params.regenIntervalSeconds) {
            m_regenAccum -= m_params.regenIntervalSeconds;
            m_health = std::min(m_params.maxHealth, m_health + 1.0f);
        }
    }
    m_timeSinceDamage += dt;

    // Hunger (Phase 23): one point every hungerDecaySeconds while above
    // zero (no saturation buffer in v1 — see KNOWN_ISSUES); at zero the
    // player starves (1 HP per interval, through the normal damage path so
    // the game's hurt flash/sound and death handling all fire).
    if (m_hunger > 0.0f) {
        m_starveAccum = 0.0; // a meal resets the pending starvation tick
        m_hungerAccum += dt;
        while (m_hungerAccum >= m_params.hungerDecaySeconds) {
            m_hungerAccum -= m_params.hungerDecaySeconds;
            m_hunger = std::max(0.0f, m_hunger - 1.0f);
            if (m_hunger <= 0.0f) {
                m_hungerAccum = 0.0;
                break;
            }
        }
    }
    if (m_hunger <= 0.0f && !m_dead) {
        m_starveAccum += dt;
        while (m_starveAccum >= m_params.starveDamageIntervalSeconds) {
            m_starveAccum -= m_params.starveDamageIntervalSeconds;
            damage(1.0f);
            if (m_dead) {
                break;
            }
        }
    }

    // Fell out of the world (the world's edge is void): death.
    if (belowKillPlane()) {
        damage(m_params.maxHealth * 1000.0f); // instant, always lethal
    }
}

bool Player::damage(float amount) {
    if (m_dead || amount <= 0.0f) {
        return false;
    }
    m_health -= amount;
    m_timeSinceDamage = 0.0;
    m_regenAccum = 0.0;
    if (m_health <= 0.0f) {
        m_health = 0.0f;
        m_dead = true;
    }
    return true;
}

void Player::respawn() {
    m_dead = false;
    m_health = m_params.maxHealth;
    m_timeSinceDamage = 0.0;
    m_regenAccum = 0.0;
    // Respawn also refills the hunger (Minecraft: a fresh start is full).
    m_hunger = m_params.maxHunger;
    m_hungerAccum = 0.0;
    m_starveAccum = 0.0;
    m_wasOnGround = false;
    m_fallStartY = m_spawnPos.y;
    teleportTo(m_spawnPos);
}

void Player::updateCrouchHeight() {
    if (m_crouching) {
        m_height = m_params.crouchHeight;
    } else if (m_height < m_params.height) {
        // Stand back up only when the headroom fits the full height
        // (trapped under a low ceiling: stay crouched).
        if (hasRoomAt(m_world, m_blocks, m_position, m_params.width,
                      m_params.height)) {
            m_height = m_params.height;
        }
    }
}

void Player::applyCrouchEdgeGuard(const Vec3& dir, double dt) {
    const float half = m_width * 0.5f;
    // The SUPPORT cell under the front face: on the ground the feet sit
    // exactly on an integer boundary, so the support block is one cell
    // BELOW floor(position.y) (floor(position.y) is the air the feet are
    // in). The guard only runs while onGround, so the subtraction is safe.
    const int fy = static_cast<int>(std::floorf(m_position.y - kFaceEps));
    // PREDICTIVE: test the cell the AABB face would reach at the END of
    // this tick (face + this tick's remaining move, velocity * dt). The
    // guard can only zero the velocity (it cannot rewind an overshoot), so
    // looking one tick ahead keeps the face from ever crossing the edge.
    if (std::abs(dir.x) > 0.01f) {
        const float reach = m_position.x + dir.x * (half + kFaceEps) +
                            m_velocity.x * static_cast<float>(dt);
        const int cx = static_cast<int>(std::floorf(reach));
        if (!isSolidCell(cx, fy,
                         static_cast<int>(std::floorf(m_position.z)))) {
            m_velocity.x = 0.0f;
        }
    }
    if (std::abs(dir.z) > 0.01f) {
        const float reach = m_position.z + dir.z * (half + kFaceEps) +
                            m_velocity.z * static_cast<float>(dt);
        const int cz = static_cast<int>(std::floorf(reach));
        if (!isSolidCell(static_cast<int>(std::floorf(m_position.x)), fy,
                         cz)) {
            m_velocity.z = 0.0f;
        }
    }
}

void Player::stepUp(const Vec3& wishDir, float distPerTick) {
    const float half = m_width * 0.5f;
    const int fx0 = static_cast<int>(std::floorf(m_position.x));
    const int fy = static_cast<int>(std::floorf(m_position.y));
    const int fz0 = static_cast<int>(std::floorf(m_position.z));
    // x axis first, then z (fixed order); at most one step per tick.
    for (int axis = 0; axis < 2; ++axis) {
        const float w = axis == 0 ? wishDir.x : wishDir.z;
        if (std::abs(w) <= 0.01f) {
            continue;
        }
        // Only a player flush against a block face can be blocked by a
        // step; the face position is exact after the collision clamp.
        const float face = axis == 0
            ? m_position.x + (w > 0.0f ? 1.0f : -1.0f) * half
            : m_position.z + (w > 0.0f ? 1.0f : -1.0f) * half;
        const float snapped = std::roundf(face);
        if (std::abs(face - snapped) > kFaceEps) {
            continue;
        }
        const int front = static_cast<int>(snapped) + (w > 0.0f ? 1 : -1);
        // axis 0 is X, axis 1 is Z (the loop index, NOT the moveAxis
        // numbering 0=x, 1=y, 2=z — that mapping is applied at the
        // moveAxis call below).
        const int cx = axis == 0 ? front : fx0;
        const int cz = axis == 1 ? front : fz0;
        // The step exists (solid at foot level) and the lifted player
        // fits (the two cells above the step are free).
        if (!isSolidCell(cx, fy, cz) || isSolidCell(cx, fy + 1, cz) ||
            isSolidCell(cx, fy + 2, cz)) {
            continue;
        }
        m_position.y += 1.0f;
        // Re-apply this tick's horizontal move along the stepped axis: the
        // sub-steps consumed it, clamped against the step face. The
        // per-tick distance (speed*dt, max 5.6/60 ~ 0.093 blocks) is well
        // under kMaxSubstepMove, so one moveAxis call is tunnel-safe.
        moveAxis(axis == 0 ? 0 : 2, w * distPerTick);
        return;
    }
}

bool Player::isSolidCell(int x, int y, int z) const {
    const BlockID id = m_world.getBlock(x, y, z);
    if (!id.valid() || id == BlockID::air()) {
        return false;
    }
    return m_blocks.at(id).properties.solid;
}

void Player::integrate(float h) {
    if (m_inWater) {
        // Swim: reduced gravity, a low sink cap, and Space raises the
        // player (also from the ground — a shallow pool floor does not
        // jump, it surfaces).
        m_velocity.y -= m_params.waterGravity * h;
        if (m_velocity.y < -m_params.waterTerminalVelocity) {
            m_velocity.y = -m_params.waterTerminalVelocity;
        }
        if (m_input->isDown(GameKey::Space)) {
            m_velocity.y = m_params.swimUpSpeed;
        }
    } else {
        // Gravity, capped at terminal velocity.
        m_velocity.y -= m_params.gravity * h;
        if (m_velocity.y < -m_params.terminalVelocity) {
            m_velocity.y = -m_params.terminalVelocity;
        }
        // Jump: only from the ground (m_onGround is the previous
        // sub-step's result, so one press cannot re-trigger mid-air).
        if (m_input->isDown(GameKey::Space) && m_onGround) {
            m_velocity.y = m_params.jumpSpeed;
        }
    }
}

bool Player::isInWater() const {
    const Vec3 mn = aabbMin();
    const Vec3 mx = aabbMax();
    const int x0 = static_cast<int>(std::floorf(mn.x));
    const int x1 = static_cast<int>(std::floorf(mx.x));
    const int y0 = static_cast<int>(std::floorf(mn.y));
    const int y1 = static_cast<int>(std::floorf(mx.y));
    const int z0 = static_cast<int>(std::floorf(mn.z));
    const int z1 = static_cast<int>(std::floorf(mx.z));
    for (int x = x0; x <= x1; ++x) {
        for (int y = y0; y <= y1; ++y) {
            for (int z = z0; z <= z1; ++z) {
                if (!(mn.x < x + 1 && mx.x > x)) {
                    continue;
                }
                if (!(mn.y < y + 1 && mx.y > y)) {
                    continue;
                }
                if (!(mn.z < z + 1 && mx.z > z)) {
                    continue;
                }
                if (m_world.getFluid(x, y, z) > 0) {
                    return true;
                }
            }
        }
    }
    return false;
}

} // namespace mc
