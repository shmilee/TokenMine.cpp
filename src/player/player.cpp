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
    if (dt <= 0.0) {
        return;
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
    if (lengthSq(wish) > 0.0f) {
        const float speed = m_inWater
                                ? m_params.swimSpeed
                                : (m_input->isDown(GameKey::LeftShift)
                                        ? m_params.sprintSpeed
                                        : m_params.walkSpeed);
        const Vec3 dir = normalized(wish);
        m_velocity.x = dir.x * speed;
        m_velocity.z = dir.z * speed;
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
    moveInSubsteps(dt, maxSpeed);

    // Fell out of the world (the demo world's edge is void).
    if (belowKillPlane()) {
        teleportTo(m_spawnPos);
    }
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
