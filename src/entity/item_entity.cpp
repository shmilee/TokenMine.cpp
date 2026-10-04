#include "entity/item_entity.hpp"

#include <algorithm>
#include <cmath>

namespace mc {

ItemEntity::ItemEntity(const World& world, const BlockRegistry& blocks,
                       const std::string& item, int maxStack, int count,
                       Vec3 pos)
    : Entity(world, blocks, pos, kSize, kSize),
      m_item(item),
      m_maxStack(std::min(maxStack, kMaxStack)),
      m_count(std::max(1, count)) {}

void ItemEntity::update(double dt) {
    if (dt <= 0.0) {
        return;
    }
    m_age += dt;
    // The pickup delay counts down with the simulation clock.
    if (m_pickupDelay > 0.0) {
        m_pickupDelay = std::max(0.0, m_pickupDelay - dt);
    }
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
    if (m_item != other.m_item ||
        other.m_count + m_count > other.m_maxStack) {
        return false;
    }
    const Vec3 d = position() - other.position();
    return lengthSq(d) < kMergeDistance * kMergeDistance;
}

void ItemEntity::absorb(const ItemEntity& other) {
    m_count = std::min(m_count + other.m_count, m_maxStack);
}

bool ItemEntity::separateFrom(const Vec3& otherMin, const Vec3& otherMax) {
    const Vec3 mn = aabbMin();
    const Vec3 mx = aabbMax();
    // Overlap extent per axis (<= 0 on any axis: no 3D overlap).
    const float ox = std::min(mx.x, otherMax.x) - std::max(mn.x, otherMin.x);
    const float oy = std::min(mx.y, otherMax.y) - std::max(mn.y, otherMin.y);
    const float oz = std::min(mx.z, otherMax.z) - std::max(mn.z, otherMin.z);
    if (ox <= 0.0f || oy <= 0.0f || oz <= 0.0f) {
        return false;
    }
    // Shallowest-penetration axis (the overlap extent), x→y→z priority on
    // exact ties. Along that axis the item is pushed AWAY from the other
    // body's center (+axis when the centers coincide), by just enough that
    // its far face lands on the other body's face: the push distance is
    // the face-to-face span, not the overlap (pushing by the overlap alone
    // leaves a residual overlap).
    Vec3 next = position();
    if (ox <= oy && ox <= oz) {
        if (mn.x + mx.x <= otherMin.x + otherMax.x) {
            next.x -= mx.x - otherMin.x;
        } else {
            next.x += otherMax.x - mn.x;
        }
    } else if (oy <= oz) {
        if (mn.y + mx.y <= otherMin.y + otherMax.y) {
            next.y -= mx.y - otherMin.y;
        } else {
            next.y += otherMax.y - mn.y;
        }
    } else {
        if (mn.z + mx.z <= otherMin.z + otherMax.z) {
            next.z -= mx.z - otherMin.z;
        } else {
            next.z += otherMax.z - mn.z;
        }
    }
    setPosition(next);
    return true;
}

} // namespace mc
