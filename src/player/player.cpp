#include "player/player.hpp"

#include <algorithm>
#include <cmath>

#include "block/block.hpp"
#include "block/block_registry.hpp"
#include "core/math/vec2.hpp"

namespace mc {

Player::Player(const World& world, const BlockRegistry& blocks, Vec3 spawnPos)
    : m_world(world), m_blocks(blocks), m_spawnPos(spawnPos) {
    teleportTo(spawnPos);
}

void Player::teleportTo(Vec3 pos) {
    m_position = pos;
    m_velocity = Vec3::zero();
    m_onGround = false;
}

Vec3 Player::aabbMin() const {
    const float half = m_params.width * 0.5f;
    return {m_position.x - half, m_position.y, m_position.z - half};
}

Vec3 Player::aabbMax() const {
    const float half = m_params.width * 0.5f;
    return {m_position.x + half, m_position.y + m_params.height,
            m_position.z + half};
}

bool Player::isSolid(BlockID id) const {
    if (id == BlockID::air() || !id.valid()) {
        return false;
    }
    return m_blocks.at(id).properties.solid;
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
    if (dt <= 0.0) {
        return;
    }

    // 1. Mouse look (only while the pointer is captured).
    if (lookEnabled) {
        const Vec2 mouse = input.mouseDelta();
        yaw -= mouse.x * m_params.mouseSensitivity;
        pitch -= mouse.y * m_params.mouseSensitivity;
        pitch = std::clamp(pitch, -kMaxPitch, kMaxPitch);
    }

    // 2. Desired horizontal velocity (instantaneous, no inertia).
    const float cp = std::cos(pitch);
    const Vec3 flatFwd {std::sin(yaw) * -cp, 0.0f, std::cos(yaw) * -cp};
    const Vec3 fwd = normalized(flatFwd);
    const Vec3 right {fwd.z * -1.0f, 0.0f, fwd.x};

    Vec3 wish {0.0f, 0.0f, 0.0f};
    if (input.isDown(GameKey::W)) {
        wish += fwd;
    }
    if (input.isDown(GameKey::S)) {
        wish -= fwd;
    }
    if (input.isDown(GameKey::D)) {
        wish += right;
    }
    if (input.isDown(GameKey::A)) {
        wish -= right;
    }
    if (lengthSq(wish) > 0.0f) {
        const float speed = input.isDown(GameKey::LeftShift)
                                ? m_params.sprintSpeed
                                : m_params.walkSpeed;
        const Vec3 dir = normalized(wish);
        m_velocity.x = dir.x * speed;
        m_velocity.z = dir.z * speed;
    } else {
        m_velocity.x = 0.0f;
        m_velocity.z = 0.0f;
    }

    // 3. Sub-step the tick so no axis moves more than kMaxSubstepMove per
    //    step: at terminal velocity a 0.25 s hiccup would otherwise move
    //    ~9 blocks and tunnel straight through the floor.
    const float vyAfter =
        std::max(m_velocity.y - m_params.gravity * static_cast<float>(dt),
                 -m_params.terminalVelocity);
    const float maxDisp =
        std::max({std::abs(m_velocity.x), std::abs(vyAfter),
                  std::abs(m_velocity.z)}) *
        static_cast<float>(dt);
    int steps = 1;
    if (maxDisp > kMaxSubstepMove) {
        steps = std::min(static_cast<int>(std::ceil(maxDisp / kMaxSubstepMove)),
                         kMaxSubsteps);
    }
    const double h = dt / static_cast<double>(steps);
    for (int i = 0; i < steps; ++i) {
        step(h, input);
    }

    // Fell out of the world (the demo world's edge is void).
    if (m_position.y < kKillPlaneY) {
        teleportTo(m_spawnPos);
    }
}

void Player::step(double h, const Input& input) {
    const float fh = static_cast<float>(h);

    // Gravity, capped at terminal velocity.
    m_velocity.y -= m_params.gravity * fh;
    if (m_velocity.y < -m_params.terminalVelocity) {
        m_velocity.y = -m_params.terminalVelocity;
    }
    // Jump: only from the ground (m_onGround is the previous sub-step's
    // result, so one press cannot re-trigger mid-air).
    if (input.isDown(GameKey::Space) && m_onGround) {
        m_velocity.y = m_params.jumpSpeed;
    }

    // Integrate per axis; the Y resolve is what (re)establishes onGround.
    m_onGround = false;
    moveAxis(0, m_velocity.x * fh);
    moveAxis(2, m_velocity.z * fh);
    moveAxis(1, m_velocity.y * fh);
}

void Player::moveAxis(int axis, float delta) {
    if (delta == 0.0f) {
        return;
    }

    switch (axis) {
        case 0: m_position.x += delta; break;
        case 1: m_position.y += delta; break;
        default: m_position.z += delta; break;
    }

    const Vec3 mn = aabbMin();
    const Vec3 mx = aabbMax();

    // Scan the block cells the new AABB overlaps; strict per-axis test
    // (`min < b+1 && max > b`) so a face resting exactly on a boundary is
    // not an overlap.
    const int x0 = static_cast<int>(std::floorf(mn.x));
    const int x1 = static_cast<int>(std::floorf(mx.x));
    const int y0 = static_cast<int>(std::floorf(mn.y));
    const int y1 = static_cast<int>(std::floorf(mx.y));
    const int z0 = static_cast<int>(std::floorf(mn.z));
    const int z1 = static_cast<int>(std::floorf(mx.z));

    float limit = 0.0f;
    bool hit = false;
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
                if (!isSolid(m_world.getBlock(x, y, z))) {
                    continue;
                }
                // The face of this cell opposing the travel direction.
                const float face =
                    (axis == 0)
                        ? static_cast<float>(delta > 0 ? x : x + 1)
                    : (axis == 1)
                          ? static_cast<float>(delta > 0 ? y : y + 1)
                          : static_cast<float>(delta > 0 ? z : z + 1);
                if (!hit) {
                    hit = true;
                    limit = face;
                } else if (delta > 0) {
                    limit = std::min(limit, face);
                } else {
                    limit = std::max(limit, face);
                }
            }
        }
    }
    if (!hit) {
        return;
    }

    // Clamp flush against the limiting face and zero this axis's velocity.
    const float half = m_params.width * 0.5f;
    switch (axis) {
        case 0:
            m_position.x = delta > 0 ? limit - half : limit + half;
            m_velocity.x = 0.0f;
            break;
        case 1:
            // m_position is the BOTTOM face: rising clamps the top to the
            // face, falling clamps the bottom.
            m_position.y = delta > 0 ? limit - m_params.height : limit;
            m_velocity.y = 0.0f;
            if (delta < 0) {
                m_onGround = true;
            }
            break;
        default:
            m_position.z = delta > 0 ? limit - half : limit + half;
            m_velocity.z = 0.0f;
            break;
    }
}

} // namespace mc
