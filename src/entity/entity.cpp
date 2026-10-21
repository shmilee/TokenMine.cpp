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

float Entity::solidTopHeight(BlockID id, const Vec3& mn, const Vec3& mx,
                             const BlockPos& cell) const {
    // Phase 85: the top of the cell's SOLID region, in cell-local units
    // (0 = not solid, 0.5 = a bottom slab, 1.0 = a full solid block). The
    // collision (moveAxis / hasRoomAt) tests overlap against this region,
    // not the whole cell — so an entity can stand on a slab's top (its feet
    // at y + 0.5) without the AABB counting as "inside" the slab.
    // Phase 87/89: a stair's height varies across the cell along its facing
    // axis (a slope from full at one end to zero at the other). The effective
    // blocking height is the slope at the AABB's LOW end on that axis (the
    // feet rest on the slope at the lowest point under the AABB).
    if (id == BlockID::air() || !id.valid()) {
        return 0.0f;
    }
    const Block& b = m_blocks.at(id);
    if (!b.properties.solid) {
        return 0.0f;
    }
    if (b.model.shape == BlockModel::Shape::Slab) {
        return 0.5f;
    }
    // Phase 95: a wall is solid in y 0..1.5 (a full block + a half cap on
    // top). A barrier: the player cannot pass through (the AABB overlaps the
    // solid region) nor jump over it (the jump is ~1.25 < 1.5).
    if (b.model.shape == BlockModel::Shape::Wall) {
        return 1.5f;
    }
    // Phase 98: a fence is a barrier with the SAME solid region as the wall
    // (y 0..1.5 — the crossed posts + bars span the full height); only the
    // meshing differs (the thin posts, not a full pillar).
    if (b.model.shape == BlockModel::Shape::Fence) {
        return 1.5f;
    }
    // Phase 99: a glass pane is a full-cell solid (y 0..1) — the 1/16
    // sheet is mesh-only, the collision is the whole cell. JUMPABLE (the
    // jump is ~1.125 > 1.0), unlike the wall/fence 1.5 barrier.
    if (b.model.shape == BlockModel::Shape::GlassPane) {
        return 1.0f;
    }
    if (b.model.shape == BlockModel::Shape::Stairs) {
        // The AABB's local range on each axis (clamped to the cell). The
        // blocking height is the slope at the AABB's LOW end on the facing
        // axis (the feet rest on the lowest point under the AABB).
        // The feet must clear the HIGHEST slope point under the AABB's
        // footprint on the facing axis (the high end of that axis), so the
        // blocking height is the slope at the AABB's edge NEAREST the high
        // end — not the far (low) edge (which would let the AABB clip the
        // stair's high part).
        const float mnx = std::clamp(mn.x - cell.x, 0.0f, 1.0f);
        const float mxlx = std::clamp(mx.x - cell.x, 0.0f, 1.0f);
        const float mnlz = std::clamp(mn.z - cell.z, 0.0f, 1.0f);
        const float mxlz = std::clamp(mx.z - cell.z, 0.0f, 1.0f);
        // ENT-2: the per-instance state (the World's per-cell byte — the
        // project's first per-block state; for a stair, the facing). The
        // game sets it when the block is placed (to the block's state
        // default) + on rotation. Unset (the sentinel, e.g. unit tests
        // without a game) falls back to the block's state default.
        const int st = m_world.getBlockState(cell.x, cell.y, cell.z);
        const int facing =
            (st != Chunk::kBlockStateUnset) ? st : b.stateDefault();
        switch (facing) {
            case 0: return 1.0f - mnlz; // north: high at -Z (the AABB's -Z edge)
            case 2: return mxlz;        // south: high at +Z (the AABB's +Z edge)
            case 1: return mxlx;        // east:  high at +X (the AABB's +X edge)
            default: return 1.0f - mnx; // west:  high at -X (the AABB's -X edge)
        }
    }
    return 1.0f;
}

bool Entity::hasRoomAt(Vec3 pos) const {
    // The same strict cell scan as the axis resolve in moveAxis: a face
    // resting exactly on a block boundary is not an overlap.
    const float half = m_width * 0.5f;
    const float x0f = pos.x - half;
    const float x1f = pos.x + half;
    const float y0f = pos.y;
    const float y1f = pos.y + m_height;
    const float z0f = pos.z - half;
    const float z1f = pos.z + half;
    const int x0 = static_cast<int>(std::floorf(x0f));
    const int x1 = static_cast<int>(std::floorf(x1f));
    const int y0 = static_cast<int>(std::floorf(y0f));
    const int y1 = static_cast<int>(std::floorf(y1f));
    const int z0 = static_cast<int>(std::floorf(z0f));
    const int z1 = static_cast<int>(std::floorf(z1f));
    // Phase 95: the wall's solid region extends ABOVE its cell (the cap,
    // y 0..1.5). So a cell BELOW the AABB's bottom (y0-1) might hold a wall
    // whose cap reaches into the AABB — extend the y scan to y0-1 (clamped
    // to the world floor). A non-wall cell below has top <= 1, so its solid
    // region does not reach the AABB (a no-op).
    const int y0ext = std::max(0, y0 - 1);
    for (int x = x0; x <= x1; ++x) {
        for (int y = y0ext; y <= y1; ++y) {
            for (int z = z0; z <= z1; ++z) {
                if (!(x0f < x + 1 && x1f > x)) {
                    continue;
                }
                if (!(z0f < z + 1 && z1f > z)) {
                    continue;
                }
                // Phase 85: overlap against the cell's SOLID region [y,
                // y+top], not the whole cell — a slab's top half is air, so
                // standing on a slab's top (feet at y+0.5) leaves room.
                // Phase 87/89: a stair's top varies along its facing axis;
                // the AABB corners (x0f/y0f/z0f..x1f/y1f/z1f) give the slope
                // height at the AABB's low end.
                const Vec3 mn {x0f, y0f, z0f};
                const Vec3 mx {x1f, y1f, z1f};
                const BlockPos cell {x, y, z};
                const float top =
                    solidTopHeight(m_world.getBlock(x, y, z), mn, mx, cell);
                if (top > 0.0f && (y0f < y + top && y1f > y)) {
                    return false;
                }
            }
        }
    }
    return true;
}

bool Entity::overlapsFluid(FluidKind kind) const {
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
                if (m_world.getFluidOfKind(x, y, z, kind) > 0) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool Entity::overlapsBlock(BlockID id) const {
    // The block-cell twin of overlapsFluid (Phase 70): same AABB, same
    // strict per-axis convention, same early exit.
    if (!id.valid()) {
        return false;
    }
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
                if (m_world.getBlock(x, y, z) == id) {
                    return true;
                }
            }
        }
    }
    return false;
}

// -- entity-vs-entity separation (Phase 19) ----------------------------------

EntitySeparation computeEntitySeparation(const Vec3& aMin, const Vec3& aMax,
                                         const Vec3& bMin, const Vec3& bMax,
                                         bool aTieNegative) {
    EntitySeparation sep;
    // Overlap extent per axis (<= 0 on any axis: no 3D overlap).
    const float ox = std::min(aMax.x, bMax.x) - std::max(aMin.x, bMin.x);
    const float oy = std::min(aMax.y, bMax.y) - std::max(aMin.y, bMin.y);
    const float oz = std::min(aMax.z, bMax.z) - std::max(aMin.z, bMin.z);
    if (ox <= 0.0f || oy <= 0.0f || oz <= 0.0f) {
        return sep; // touching faces are not an overlap
    }
    sep.overlap = true;
    // Shallowest-penetration axis, x->y->z priority on exact ties.
    if (ox <= oy && ox <= oz) {
        sep.axis = 0;
    } else if (oy <= oz) {
        sep.axis = 1;
    } else {
        sep.axis = 2;
    }
    const float ac =
        (sep.axis == 0)    ? aMin.x + aMax.x
        : (sep.axis == 1)  ? aMin.y + aMax.y
                           : aMin.z + aMax.z;
    const float bc =
        (sep.axis == 0)    ? bMin.x + bMax.x
        : (sep.axis == 1)  ? bMin.y + bMax.y
                           : bMin.z + bMax.z;
    // a sits on the -axis side; an exact center tie goes to `a` iff the
    // caller resolved it that way (aTieNegative).
    sep.aLeft = ac < bc || (ac == bc && aTieNegative);
    // Face-to-face span: how far `a` must travel along the axis, away from
    // b, until its far face rests flush on b's far face.
    if (sep.aLeft) {
        sep.distance =
            (sep.axis == 0)    ? aMax.x - bMin.x
            : (sep.axis == 1)  ? aMax.y - bMin.y
                               : aMax.z - bMin.z;
    } else {
        sep.distance =
            (sep.axis == 0)    ? bMax.x - aMin.x
            : (sep.axis == 1)  ? bMax.y - aMin.y
                               : bMax.z - aMin.z;
    }
    return sep;
}

namespace {

// The AABB of `e` translated by `d` along the given axis (a candidate
// position for the separation push).
Vec3 shiftedPosition(const Entity& e, int axis, float d) {
    Vec3 p = e.position();
    if (axis == 0) {
        p.x += d;
    } else if (axis == 1) {
        p.y += d;
    } else {
        p.z += d;
    }
    return p;
}

} // namespace

bool Entity::separateFrom(const Vec3& otherMin, const Vec3& otherMax) {
    // One-sided push: this body moves, the other is immovable. An exact
    // center tie pushes THIS body to the -axis (the Phase 16 direction).
    const EntitySeparation sep =
        computeEntitySeparation(aabbMin(), aabbMax(), otherMin, otherMax,
                                /*aTieNegative=*/true);
    if (!sep.overlap) {
        return false;
    }
    // This body takes the whole separation (the other one is immovable).
    const Vec3 next = shiftedPosition(*this, sep.axis,
                                        sep.distance * (sep.aLeft ? -1.0f
                                                                  : 1.0f));
    if (!hasRoomAt(next)) {
        return false; // the push would embed this body in a solid block
    }
    setPosition(next);
    return true;
}

bool separateEntityPair(Entity& a, Entity& b, bool aFirst) {
    const bool aMoves = a.pushableByEntities();
    const bool bMoves = b.pushableByEntities();
    if (!aMoves && !bMoves) {
        return false;
    }
    // Exact-center tie: the pushable body goes to the -axis side (keeps the
    // Phase 16 player-vs-item direction — the item goes negative); when
    // both are pushable the later body in the list goes negative (aFirst
    // marks which of a/b the pass sees first).
    const bool aTieNegative =
        (aMoves && !bMoves) || (aMoves && bMoves && !aFirst);
    const EntitySeparation sep =
        computeEntitySeparation(a.aabbMin(), a.aabbMax(), b.aabbMin(),
                                b.aabbMax(), aTieNegative);
    if (!sep.overlap) {
        return false;
    }
    // The two bodies move in opposite directions along the axis.
    const float dirA = sep.aLeft ? -1.0f : 1.0f;
    const float dirB = -dirA;
    float da = 0.0f;
    float db = 0.0f;
    if (aMoves && bMoves) {
        // 50/50 split: each body moves half the face span, which lands the
        // pair face-to-face (touching).
        da = sep.distance * 0.5f;
        db = da;
        if (!a.hasRoomAt(shiftedPosition(a, sep.axis, dirA * da)) ||
            !b.hasRoomAt(shiftedPosition(b, sep.axis, dirB * db))) {
            // A blocked share: hand the full separation to the other body.
            da = 0.0f;
            db = 0.0f;
            if (a.hasRoomAt(shiftedPosition(a, sep.axis,
                                              dirA * sep.distance))) {
                da = sep.distance;
            } else if (b.hasRoomAt(shiftedPosition(b, sep.axis,
                                                     dirB * sep.distance))) {
                db = sep.distance;
            } else {
                return false; // both directions are walled off: leave the pair
            }
        }
    } else if (aMoves) {
        da = sep.distance;
        if (!a.hasRoomAt(shiftedPosition(a, sep.axis, dirA * da))) {
            return false;
        }
    } else {
        db = sep.distance;
        if (!b.hasRoomAt(shiftedPosition(b, sep.axis, dirB * db))) {
            return false;
        }
    }
    if (da != 0.0f) {
        a.setPosition(shiftedPosition(a, sep.axis, dirA * da));
    }
    if (db != 0.0f) {
        b.setPosition(shiftedPosition(b, sep.axis, dirB * db));
    }
    return true;
}

int separateEntityList(const std::vector<Entity*>& bodies) {
    const std::size_t n = bodies.size();
    std::vector<Vec3> before(n);
    const float convSq = kSeparationConvergence * kSeparationConvergence;
    int movedSweeps = 0;
    for (int sweep = 0; sweep < kSeparationSweeps; ++sweep) {
        for (std::size_t i = 0; i < n; ++i) {
            before[i] = bodies[i]->position();
        }
        bool moved = false;
        for (std::size_t i = 0; i + 1 < n; ++i) {
            if (!bodies[i]->collidesWithEntities()) {
                continue;
            }
            for (std::size_t j = i + 1; j < n; ++j) {
                if (!bodies[j]->collidesWithEntities()) {
                    continue;
                }
                if (separateEntityPair(*bodies[i], *bodies[j],
                                       /*aFirst=*/true)) {
                    moved = true;
                }
            }
        }
        if (!moved) {
            break;
        }
        ++movedSweeps;
        // Convergence: a sweep whose largest body move is sub-visual has
        // left only a sub-visual residual (the cap bounds the cost
        // regardless).
        float maxMoveSq = 0.0f;
        for (std::size_t i = 0; i < n; ++i) {
            const Vec3 d = bodies[i]->position() - before[i];
            maxMoveSq = std::max(maxMoveSq, lengthSq(d));
        }
        if (maxMoveSq <= convSq) {
            break;
        }
    }
    return movedSweeps;
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
    // Phase 95: the wall's solid region extends ABOVE its cell (the cap,
    // y 0..1.5). So a cell BELOW the AABB's bottom (y0-1) might hold a wall
    // whose cap reaches into the AABB — extend the y scan to y0-1 (clamped
    // to the world floor). A non-wall cell below has top <= 1, so its solid
    // region does not reach the AABB (a no-op).
    const int y0ext = std::max(0, y0 - 1);

    float limit = 0.0f;
    bool hit = false;
    for (int x = x0; x <= x1; ++x) {
        for (int y = y0ext; y <= y1; ++y) {
            for (int z = z0; z <= z1; ++z) {
                if (!(mn.x < x + 1 && mx.x > x)) {
                    continue;
                }
                if (!(mn.z < z + 1 && mx.z > z)) {
                    continue;
                }
                // Phase 85: overlap against the cell's SOLID region [y,
                // y+top], not the whole cell (a slab is solid in its bottom
                // half only). A face resting exactly on the region's top is
                // not an overlap (the strict convention). Phase 87/89: a
                // stair's top varies along its facing axis — the AABB corners
                // (mn/mx) give the slope height at the AABB's low end.
                const BlockPos cell {x, y, z};
                const float top =
                    solidTopHeight(m_world.getBlock(x, y, z), mn, mx, cell);
                if (top <= 0.0f) {
                    continue;
                }
                if (!(mn.y < y + top && mx.y > y)) {
                    continue;
                }
                // The face of this cell opposing the travel direction. For
                // Y travel the limit is the slab's TOP (feet landing,
                // delta < 0) or BOTTOM (head bumping, delta > 0); a full
                // block (top = 1.0) reproduces the old y / y+1 faces.
                const float face =
                    (axis == 0)
                        ? static_cast<float>(delta > 0 ? x : x + 1)
                    : (axis == 1)
                          ? static_cast<float>(delta > 0 ? y : y + top)
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
