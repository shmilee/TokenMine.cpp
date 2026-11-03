#include "entity/exp_orb.hpp"

#include <algorithm>
#include <cmath>

#include "block/cube_face_table.hpp"

namespace mc {

ExpOrbEntity::ExpOrbEntity(const World& world, const BlockRegistry& blocks,
                           int value, Vec3 pos)
    : Entity(world, blocks, pos, kSize, kSize),
      m_value(std::clamp(value, kMinValue, kMaxValue)) {}

void ExpOrbEntity::update(double dt) {
    if (dt <= 0.0) {
        return;
    }
    m_age += dt;
    // The pickup delay counts down with the simulation clock (the item's
    // real-dt twin — the save continues it exactly on load).
    if (m_pickupDelay > 0.0) {
        m_pickupDelay = std::max(0.0, m_pickupDelay - dt);
    }
    // Peak-speed estimate for the sub-step count: this tick's post-gravity
    // vertical velocity (the item's in-air branch — the orb has no fluid
    // model: an XP orb is not buoyant, the documented simplification of
    // the item's ENT-4 fluid twin).
    const float vyAfter =
        std::max(m_velocity.y - kGravity * static_cast<float>(dt),
                 -kTerminalVelocity);
    const float maxSpeed =
        std::max({std::abs(m_velocity.x), std::abs(vyAfter),
                  std::abs(m_velocity.z)});
    moveInSubsteps(dt, maxSpeed);
}

void ExpOrbEntity::integrate(float h) {
    // Gravity, capped at terminal velocity (the item's air model).
    m_velocity.y -= kGravity * h;
    if (m_velocity.y < -kTerminalVelocity) {
        m_velocity.y = -kTerminalVelocity;
    }
    // Ground friction (the item's rest model): exponential horizontal
    // decay, then zero the sub-pixel residual so the orb truly stops.
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

ItemMeshGeometry makeExpOrbGeometry() {
    // The six-face solid cube (the item cube's convention — every face,
    // kCubeFaces' counter-clockwise-from-outside winding, centered on the
    // origin at kSize). The UVs are (0,0)-(1,1) dummies: the render takes
    // the basic shader's SOLID-COLOR path (u_useTexture = 0 — the arrows'
    // precedent), so no atlas tile is sampled (zero new atlas item). Full
    // sky light is baked like the item cube; the per-draw light override
    // (the itemDropLight pattern) replaces it at render time.
    ItemMeshGeometry out;
    out.vertices.reserve(24);
    out.indices.reserve(36);
    for (const CubeFace& face : kCubeFaces) {
        const std::uint32_t first =
            static_cast<std::uint32_t>(out.vertices.size());
        for (int i = 0; i < 4; ++i) {
            MeshVertex v;
            v.position = {(static_cast<float>(face.corner[i][0]) - 0.5f) *
                              ExpOrbEntity::kSize,
                          (static_cast<float>(face.corner[i][1]) - 0.5f) *
                              ExpOrbEntity::kSize,
                          (static_cast<float>(face.corner[i][2]) - 0.5f) *
                              ExpOrbEntity::kSize};
            v.normal = face.normal;
            v.uv = {0.0f, 0.0f};
            v.skyLight = 1.0f;
            v.blockLight = 0.0f;
            out.vertices.push_back(v);
        }
        out.indices.insert(out.indices.end(),
                           {first, first + 1, first + 2, first, first + 2,
                            first + 3});
    }
    return out;
}

} // namespace mc
