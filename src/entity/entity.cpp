#include "entity/entity.hpp"

#include <algorithm>
#include <cmath>

namespace mc {

Entity::Entity(const World& world, const BlockRegistry& blocks, Vec3 pos,
               float width, float height)
    : m_world(world),
      m_blocks(blocks),
      m_position(pos),
      m_width(width),
      m_height(height) {}

bool Entity::isSolid(BlockID id) const {
    if (id == BlockID::air() || !id.valid()) {
        return false;
    }
    return m_blocks.at(id).properties.solid;
}

void Entity::moveInSubsteps(double dt, float maxSpeed) {
    // Sub-step the tick so no axis moves more than kMaxSubstepMove per step:
    // at terminal velocity a 0.25 s hiccup would otherwise move ~9 blocks
    // and tunnel straight through the floor.
    const float maxDisp = maxSpeed * static_cast<float>(dt);
    int steps = 1;
    if (maxDisp > kMaxSubstepMove) {
        steps = std::min(
            static_cast<int>(std::ceil(maxDisp / kMaxSubstepMove)), kMaxSubsteps);
    }
    const double h = dt / static_cast<double>(steps);
    for (int i = 0; i < steps; ++i) {
        const float fh = static_cast<float>(h);
        integrate(fh);
        // Integrate per axis; the Y resolve is what (re)establishes onGround.
        m_onGround = false;
        bool blocked = false;
        blocked = moveAxis(0, m_velocity.x * fh) || blocked;
        blocked = moveAxis(2, m_velocity.z * fh) || blocked;
        blocked = moveAxis(1, m_velocity.y * fh) || blocked;
        afterSubstep(blocked);
    }
}

bool Entity::moveAxis(int axis, float delta) {
    if (delta == 0.0f) {
        return false;
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
        return false;
    }

    // Clamp flush against the limiting face and zero this axis's velocity.
    const float half = m_width * 0.5f;
    switch (axis) {
        case 0:
            m_position.x = delta > 0 ? limit - half : limit + half;
            m_velocity.x = 0.0f;
            break;
        case 1:
            // m_position is the BOTTOM face: rising clamps the top to the
            // face, falling clamps the bottom.
            m_position.y = delta > 0 ? limit - m_height : limit;
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
    return true;
}

} // namespace mc
