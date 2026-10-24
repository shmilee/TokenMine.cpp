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
    // ENT-4: the fluid tick state (once per update, the player's FLUID-2
    // pattern — consumed per sub-step by integrate): the item floats in
    // water OR lava (the symmetric buoyancy, the documented extension).
    refreshFluidState();
    // Peak-speed estimate for the sub-step count: in fluid (or at its
    // surface) the vertical motion is bounded by the rise/sink caps (the
    // player's FLUID-2 peak set); in air, this tick's post-gravity
    // vertical velocity (UNCHANGED — the on-land trajectory stays
    // bit-identical to the pre-ENT-4 model; the horizontal one only
    // decays).
    const float vyAfter = m_inFluid || m_atSurface
        ? std::max(std::abs(m_velocity.y), kWaterTerminalVelocity)
        : std::max(m_velocity.y - kGravity * static_cast<float>(dt),
                   -kTerminalVelocity);
    const float maxSpeed =
        std::max({std::abs(m_velocity.x), std::abs(vyAfter),
                  std::abs(m_velocity.z)});
    moveInSubsteps(dt, maxSpeed);
}

void ItemEntity::integrate(float h) {
    if (m_inFluid || m_atSurface) {
        // ENT-4: the fluid force model: no gravity in fluid.
        integrateFluid(h);
    } else {
        // Gravity, capped at terminal velocity.
        m_velocity.y -= kGravity * h;
        if (m_velocity.y < -kTerminalVelocity) {
            m_velocity.y = -kTerminalVelocity;
        }
    }
    // Ground friction (UNCHANGED, in fluid AND in air): exponential
    // horizontal decay, then zero the sub-pixel residual so the item
    // truly stops — the float terminal is a STATIONARY item (no surface
    // drift, the documented MC simplification).
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

} // namespace mc
