#include "entity/item_entity.hpp"

#include <algorithm>
#include <cmath>

namespace mc {

ItemEntity::ItemEntity(const World& world, const BlockRegistry& blocks,
                       BlockID block, Vec3 pos)
    : Entity(world, blocks, pos, kSize, kSize), m_block(block) {}

void ItemEntity::update(double dt) {
    if (dt <= 0.0) {
        return;
    }
    m_age += dt;
    // Peak-speed estimate for the sub-step count: this tick's post-gravity
    // vertical velocity (the horizontal one only decays).
    const float vyAfter = std::max(
        m_velocity.y - kGravity * static_cast<float>(dt), -kTerminalVelocity);
    const float maxSpeed =
        std::max({std::abs(m_velocity.x), std::abs(vyAfter),
                  std::abs(m_velocity.z)});
    moveInSubsteps(dt, maxSpeed);
}

void ItemEntity::integrate(float h) {
    // Gravity, capped at terminal velocity.
    m_velocity.y -= kGravity * h;
    if (m_velocity.y < -kTerminalVelocity) {
        m_velocity.y = -kTerminalVelocity;
    }
    // Ground friction: exponential horizontal decay, then zero the
    // sub-pixel residual.
    const float f = std::exp(-kFriction * h);
    m_velocity.x *= f;
    m_velocity.z *= f;
    if (std::abs(m_velocity.x) < kStopSpeed) {
        m_velocity.x = 0.0f;
    }
    if (std::abs(m_velocity.z) < kStopSpeed) {
        m_velocity.z = 0.0f;
    }
}

bool ItemEntity::canMergeInto(const ItemEntity& other) const {
    if (m_block != other.m_block ||
        other.m_count + m_count > kMaxStack) {
        return false;
    }
    const Vec3 d = position() - other.position();
    return lengthSq(d) < kMergeDistance * kMergeDistance;
}

void ItemEntity::absorb(const ItemEntity& other) {
    m_count = std::min(m_count + other.m_count, kMaxStack);
}

} // namespace mc
