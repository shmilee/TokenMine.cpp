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

#include <vector>

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

    // Set the position (bottom-center) WITHOUT touching the velocity or the
    // ground state. Used by the entity separation (Phases 16/19): a pushed
    // item keeps moving (a teleport would kill its slide).
    void setPosition(Vec3 pos) { m_position = pos; }

    // Would the AABB placed at `pos` (bottom-center) overlap a solid block?
    // Same strict convention as the collision resolve: a face resting
    // exactly on a block boundary is free. The entity-separation pass
    // (Phase 19) never moves a body into a position this rejects.
    bool hasRoomAt(Vec3 pos) const;

    // Does the AABB strictly overlap at least one cell holding a fluid of
    // `kind` (the same per-axis strict convention as the collision scan: a
    // face resting exactly on a cell boundary is not an overlap)? The
    // player uses this for the swim state + the lava burn (Phase 54/57);
    // the mobs use it for the fire system (Phase 58: lava ignites, water
    // extinguishes).
    bool overlapsFluid(FluidKind kind) const;

    // Does the AABB strictly overlap at least one cell holding the BLOCK
    // `id` (the same per-axis strict convention as overlapsFluid)? The
    // caller passes the id — the method never names a block (the player
    // uses it for the fire contact damage, Phase 70, with the game
    // init's resolved fire id). An invalid id means "never" (a test
    // fixture without that block, the fluid.cpp fallback).
    bool overlapsBlock(BlockID id) const;

    // Entity-vs-entity collision (Phase 19): does this entity participate
    // in the separation pass at all? Projectiles fly through every entity
    // (Minecraft arrows) and opt out.
    virtual bool collidesWithEntities() const { return true; }

    // Can the separation pass move this entity? The player is immovable —
    // items are pushed out of the player (Minecraft's behavior, Phase 16),
    // never the other way around.
    virtual bool pushableByEntities() const { return true; }

    // Push this entity out of the IMMOVABLE AABB [otherMin, otherMax]
    // (Phase 16 primitive, kept for tests and as the one-sided special
    // case of the pair pass below): along the axis of MINIMUM penetration
    // (x->y->z priority on exact ties, deterministic), AWAY from the other
    // body's center (-axis when the centers coincide, the Phase 16
    // direction), by the
    // face-to-face span — the push that lands this body's far face flush
    // on the other body's face (pushing by the overlap alone leaves a
    // residual overlap when the other body is wider). The body only moves
    // to a position its AABB can occupy (hasRoomAt); a blocked push leaves
    // it in place. The velocity is untouched (a slide continues). Returns
    // true when the entity moved.
    bool separateFrom(const Vec3& otherMin, const Vec3& otherMax);

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
    // Phase 85: the top of the cell's SOLID region in cell-local units (0 =
    // not solid, 0.5 = bottom slab, 1.0 = full block) — the collision tests
    // overlap against this region, not the whole cell.
    float solidTopOffset(BlockID id) const;
};

// -- entity-vs-entity separation (Phase 19) ----------------------------------

// The resolved geometry of one overlapping pair (a vs b).
struct EntitySeparation {
    bool overlap = false;  // 3D AABB overlap (strict: touching faces don't count)
    int axis = 0;          // 0=x 1=y 2=z: the minimum-penetration axis (x->y->z
                           // priority on exact ties)
    bool aLeft = false;    // a sits on the -axis side (an exact center tie is
                           // resolved by the caller's aTieNegative)
    float distance = 0.0f; // face-to-face span: how far `a` must travel along
                           // the axis, away from b, to clear it completely
};

// Pure geometry: overlap test, axis/orientation and the clearance span.
// `aTieNegative` resolves the exact-center tie: true = a sits on the
// -axis side when the two centers coincide exactly.
EntitySeparation computeEntitySeparation(const Vec3& aMin, const Vec3& aMax,
                                         const Vec3& bMin, const Vec3& bMax,
                                         bool aTieNegative);

// Resolve the overlap of two entities: the pair is pushed apart along the
// minimum-penetration axis — 50/50 when both bodies are pushable, the
// pushable one takes it all against an immovable one (the Phase 16
// player-vs-item rule). A body only moves to a position its AABB can occupy
// (hasRoomAt): a blocked share falls back to the other body taking the full
// separation; if both sides are blocked the pair is left as is. An exact
// center tie pushes the pushable body to the -axis side — which keeps the
// Phase 16 player-vs-item direction (the item goes negative); when both
// bodies are pushable the later one in the list goes negative (`aFirst`
// marks which body the pass sees first). Returns true when at least one
// body moved.
bool separateEntityPair(Entity& a, Entity& b, bool aFirst);

// The per-frame separation pass: repeated sweeps over `bodies` (pairs
// (i, j), i < j, in list order — the caller puts the player first; the list
// order decides the exact-center tie between two pushable bodies: the
// later one goes negative). Pairs where either side opts out
// (collidesWithEntities) are skipped. Sweeps repeat until one
// moves nothing, until a sweep's largest body move drops below
// kSeparationConvergence (the competing constraints — e.g. an item flush
// against the player AND touching the next item — halve the residual
// interpenetration per sweep, so this stops at a sub-visual residual), or
// until kSeparationSweeps is reached (the hard cap keeps the worst case
// bounded and deterministic). Returns the number of sweeps that moved
// something.
int separateEntityList(const std::vector<Entity*>& bodies);

// Per-sweep convergence threshold of the separation pass (blocks): below
// this, the residual interpenetration is sub-visual (1e-6 blocks is about
// 1e-5 px at typical view distances).
inline constexpr float kSeparationConvergence = 1e-6f;

// Hard cap on the separation pass (see separateEntityList): a realistic
// drop cluster converges in ~20 sweeps; the cap bounds pathological
// oscillation and pins the result.
inline constexpr int kSeparationSweeps = 32;

} // namespace mc
