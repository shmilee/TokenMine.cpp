// Entity: the unified moving-actor base (Phase 10).
//
// Position convention (Minecraft-style, shared with the player): `position`
// is the CENTER of the BOTTOM face of the entity's AABB, in world block
// units. The AABB is
//   min = {x - width/2, y, z - width/2}
//   max = {x + width/2, y + height, z + width/2}
//
// Physics model (per update):
//   - the tick is sub-stepped so no axis moves more than kMaxSubstepMove
//     per step; each sub-step calls integrate(h) (forces: gravity, jump,
//     friction — entity-specific), then moves X, Z, Y in order, each axis
//     resolved independently against solid blocks. A blocked axis is
//     clamped flush to the limiting face and zeroes its velocity, which is
//     what produces sliding.
//   - a block cell [b, b+1) overlaps the AABB iff `min < b+1 && max > b`
//     (strict). Block boundaries are integers, exactly representable in
//     float, so resting exactly on a face is not an overlap: gravity
//     re-penetrates the face each sub-step and the Y resolve clamps back to
//     the exact integer boundary (no jitter).
//   - onGround = the Y resolve clamped downward this sub-step.
//
// The World + BlockRegistry are the sole source of solid/air: an entity
// reads properties.solid and never switches on block identity. Fluid cells
// are non-solid (entities pass through them).
//
// Ownership: the game owns the entity list and calls update() on each
// entity. Entities never delete themselves — the game removes an entity
// once shouldDespawn() reports true (after update, so the final state is
// observable).
#pragma once

#include "block/block_registry.hpp"
#include "core/math/vec3.hpp"
#include "world/world.hpp"

namespace mc {

class Entity {
public:
    virtual ~Entity() = default;

    // Advance the simulation by dt seconds.
    virtual void update(double dt) = 0;

    // True once the entity has served its life (age, kill plane, impact...).
    virtual bool shouldDespawn() const { return false; }

    // -- state ---------------------------------------------------------------
    const Vec3& position() const { return m_position; } // bottom-center
    const Vec3& velocity() const { return m_velocity; } // blocks / second
    bool onGround() const { return m_onGround; }
    float width() const { return m_width; }
    float height() const { return m_height; }

    void setVelocity(Vec3 v) { m_velocity = v; }

    Vec3 aabbMin() const {
        const float half = m_width * 0.5f;
        return {m_position.x - half, m_position.y, m_position.z - half};
    }
    Vec3 aabbMax() const {
        const float half = m_width * 0.5f;
        return {m_position.x + half, m_position.y + m_height,
                m_position.z + half};
    }

    // Move to `pos` (bottom-center) and zero the velocity.
    void teleportTo(Vec3 pos) {
        m_position = pos;
        m_velocity = Vec3::zero();
        m_onGround = false;
    }

    const World& world() const { return m_world; }

    // Fell out of the world (the world's edge is void).
    bool belowKillPlane() const { return m_position.y < kKillPlaneY; }

    static constexpr float kKillPlaneY = -32.0f; // below this: despawn/respawn

protected:
    Entity(const World& world, const BlockRegistry& blocks, Vec3 pos,
           float width, float height);

    // One sub-step of force integration (gravity, jump, friction...).
    // Called by moveInSubsteps() before the axis moves; `h` is the sub-step
    // length in seconds.
    virtual void integrate(float h) = 0;

    // Called after each sub-step's three axis moves. `blocked` is true if
    // any axis was stopped by a solid block (Projectiles use it to detect a
    // hit); the default does nothing.
    virtual void afterSubstep(bool blocked) { (void)blocked; }

    // Run `dt` of physics as sub-steps of integrate + X/Z/Y moves.
    // `maxSpeed` is the caller's estimate of the peak speed reached during
    // the tick; the sub-step count keeps every axis under
    // kMaxSubstepMove per step at that speed (the tunneling bound).
    void moveInSubsteps(double dt, float maxSpeed);

    // Move along one axis by `delta` and resolve against solid blocks.
    // Returns true if a solid block stopped the move (the position is
    // clamped flush to the limiting face and that axis's velocity zeroed).
    // A downward Y stop also sets m_onGround.
    bool moveAxis(int axis, float delta);

    const World& m_world;
    const BlockRegistry& m_blocks;
    Vec3 m_position {0.0f, 0.0f, 0.0f}; // bottom-center of the AABB
    Vec3 m_velocity {0.0f, 0.0f, 0.0f}; // blocks / second
    bool m_onGround = false;
    float m_width = 0.0f;
    float m_height = 0.0f;

    static constexpr float kMaxSubstepMove = 0.4f; // blocks per physics sub-step
    static constexpr int kMaxSubsteps = 64;

private:
    // Solid = registered block with properties.solid (air is never solid).
    bool isSolid(BlockID id) const;
};

} // namespace mc
