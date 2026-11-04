#include "world/ao.hpp"

#include <cmath>

#include "block/block_registry.hpp"
#include "core/config.hpp"
#include "core/json.hpp"
#include "world/world.hpp"

namespace mc {

namespace {

// The face normal's axis (0=x, 1=y, 2=z) and sign — the kCubeFaces
// normals are exact unit axes.
int normalAxis(const Vec3& n) {
    if (n.x != 0.0f) {
        return 0;
    }
    if (n.z != 0.0f) {
        return 2;
    }
    return 1;
}

int normalSign(const Vec3& n) {
    if (n.x != 0.0f) {
        return n.x > 0.0f ? 1 : -1;
    }
    if (n.z != 0.0f) {
        return n.z > 0.0f ? 1 : -1;
    }
    return n.y > 0.0f ? 1 : -1;
}

// The two in-plane axes for a normal axis (fixed ascending order — the
// level is symmetric in side1/side2, so the order is behaviorally inert).
int inPlaneAxis1(int nAxis) {
    return nAxis == 0 ? 1 : 0;
}

int inPlaneAxis2(int nAxis) {
    return nAxis == 2 ? 1 : 2;
}

// The component of a 0/1 corner triple along one axis.
int cornerAxis(int cornerX, int cornerY, int cornerZ, int axis) {
    switch (axis) {
        case 0:
            return cornerX;
        case 2:
            return cornerZ;
        default:
            return cornerY;
    }
}

// The e direction along one in-plane axis, AWAY from the cell at the
// vertex (the corner on the cell's min side -> -axis, max side -> +axis;
// the in-front edge-sharing cells are the check set — see the header).
int edgeSign(int corner) {
    return corner == 0 ? -1 : 1;
}

// One query cell's occlusion band: the block id (air / missing chunk /
// out-of-range all read air = no band) + the slab's resolved state (the
// kBlockStateUnset sentinel -> the block's state default, the emitSlab
// rule — a stateless slab never reads a state, like solidRange).
AoBand queryBand(const World& world, const BlockRegistry& blocks,
                 const BlockPos& q) {
    const BlockID id = world.getBlock(q.x, q.y, q.z);
    if (id == BlockID::air() || !id.valid()) {
        return AoBand::none();
    }
    const Block& block = blocks.at(id);
    int state = 0;
    if (block.model.shape == BlockModel::Shape::Slab && block.hasState()) {
        const int st = world.getBlockState(q.x, q.y, q.z);
        state = (st != Chunk::kBlockStateUnset) ? st : block.stateDefault();
    }
    return aoOccluderBand(block, state);
}

} // namespace

AoBand aoOccluderBand(const Block& block, int state) {
    if (block.id == BlockID::air() || !block.id.valid()) {
        return AoBand::none(); // the air sentinel: no occluder
    }
    if (!block.properties.opaque) {
        return AoBand::none(); // the culling's same opaque flag
    }
    if (block.model.shape == BlockModel::Shape::Slab) {
        // The slab's within-cell band by state (the Entity::solidRange
        // {bottom, top} values, closed — the bed family is stateless ->
        // the caller passes 0, the bottom half, its geometry).
        if (state == 1) {
            return {kSlabHalf, 1.0f};
        }
        if (state >= 2) {
            return {0.0f, 1.0f};
        }
        return {0.0f, kSlabHalf};
    }
    // Full cube, stairs (deviation 2: the slope's high end over-
    // occludes — documented), wall and fence (their within-cell solid
    // regions span the whole cell: the 1.5 pillar/cap and the posts
    // both fill y 0..1 here).
    return {0.0f, 1.0f};
}

bool aoBandCovers(float y, int rowOffset, const AoBand& band) {
    if (band.empty()) {
        return false;
    }
    const float yInRow = y - static_cast<float>(rowOffset);
    // Closed interval, exact float (every value is a multiple of 0.5):
    // the slab seam vertex at 0.5 counts for the neighbor's [0, 0.5]
    // and the [0.5, 1] band alike (the design's "含端点" rule).
    return band.lo <= yInRow && yInRow <= band.hi;
}

std::uint8_t aoCornerLevel(bool side1, bool side2, bool corner) {
    // Minecraft's exact rule: both sides solid = the corner darkness
    // (level 3, factor 0.3) — the plain count would say 2 (0.5).
    if (side1 && side2) {
        return 3;
    }
    return static_cast<std::uint8_t>((side1 ? 1 : 0) + (side2 ? 1 : 0) +
                                     (corner ? 1 : 0));
}

float aoFactor(std::uint8_t level) {
    switch (level) {
        case 1:
            return kAoOne;
        case 2:
            return kAoTwo;
        case 3:
            return kAoCorner;
        default:
            return kAoFull; // level 0 (and any stray value: no occlusion)
    }
}

std::uint8_t aoVertexLevel(const World& world, const BlockRegistry& blocks,
                           const BlockPos& cell, const Vec3& normal,
                           int cornerX, int cornerY, int cornerZ,
                           float yLocal) {
    const int nAxis = normalAxis(normal);
    const int nSign = normalSign(normal);
    const int a1 = inPlaneAxis1(nAxis);
    const int a2 = inPlaneAxis2(nAxis);
    const int e1 = edgeSign(cornerAxis(cornerX, cornerY, cornerZ, a1));
    const int e2 = edgeSign(cornerAxis(cornerX, cornerY, cornerZ, a2));

    // The viewer cell (the face's in-front cell) and the three check
    // cells around the vertex.
    int vx = cell.x;
    int vy = cell.y;
    int vz = cell.z;
    if (nAxis == 0) {
        vx += nSign;
    } else if (nAxis == 1) {
        vy += nSign;
    } else {
        vz += nSign;
    }
    const auto offset = [&](int& x, int& y, int& z, int axis, int sign) {
        if (axis == 0) {
            x += sign;
        } else if (axis == 1) {
            y += sign;
        } else {
            z += sign;
        }
    };
    int q1x = vx, q1y = vy, q1z = vz;
    offset(q1x, q1y, q1z, a1, e1);
    int q2x = vx, q2y = vy, q2z = vz;
    offset(q2x, q2y, q2z, a2, e2);
    int qcx = q1x, qcy = q1y, qcz = q1z;
    offset(qcx, qcy, qcz, a2, e2);

    const AoBand b1 = queryBand(world, blocks, BlockPos{q1x, q1y, q1z});
    const AoBand b2 = queryBand(world, blocks, BlockPos{q2x, q2y, q2z});
    const AoBand bc = queryBand(world, blocks, BlockPos{qcx, qcy, qcz});
    const int row1 = q1y - cell.y;
    const int row2 = q2y - cell.y;
    const int rowc = qcy - cell.y;
    return aoCornerLevel(aoBandCovers(yLocal, row1, b1),
                         aoBandCovers(yLocal, row2, b2),
                         aoBandCovers(yLocal, rowc, bc));
}

void aoCellCornerLevels(const World& world, const BlockRegistry& blocks,
                        const BlockPos& cell, const CubeFace& face,
                        std::array<std::uint8_t, 4>& out) {
    const int nAxis = normalAxis(face.normal);
    const int nSign = normalSign(face.normal);
    const int a1 = inPlaneAxis1(nAxis);
    const int a2 = inPlaneAxis2(nAxis);

    // The viewer cell (V = cell + normal).
    int vx = cell.x;
    int vy = cell.y;
    int vz = cell.z;
    if (nAxis == 0) {
        vx += nSign;
    } else if (nAxis == 1) {
        vy += nSign;
    } else {
        vz += nSign;
    }

    // The 8 in-front cells around V (V±u1, V±u2, V±u1±u2): the four
    // corners' 3 checks all draw from this set, so each cell's band is
    // fetched ONCE (the mask build is the hot path: 4 corners x 3
    // checks would refetch the shared side cells 3x).
    // bandX[i] = V + e*unit(a1), bandZ[i] = V + e*unit(a2), bandD[i][j]
    // = V + e*unit(a1) + f*unit(a2) — i, j index the SIGN (i 0 = e -1,
    // i 1 = e +1, sign(i) = 2i - 1); rowX/Z/D = the cell's Y offset from
    // the face's cell row (the band test's rowOffset).
    AoBand bandX[2], bandZ[2], bandD[2][2];
    int rowX[2] = {}, rowZ[2] = {}, rowD[2][2] = {};
    for (int i = 0; i < 2; ++i) {
        const int e = 2 * i - 1; // -1 (i 0) / +1 (i 1)
        int x = vx, y = vy, z = vz;
        if (a1 == 0) {
            x += e;
        } else if (a1 == 1) {
            y += e;
        } else {
            z += e;
        }
        bandX[i] = queryBand(world, blocks, BlockPos{x, y, z});
        rowX[i] = y - cell.y;
        x = vx;
        y = vy;
        z = vz;
        if (a2 == 0) {
            x += e;
        } else if (a2 == 1) {
            y += e;
        } else {
            z += e;
        }
        bandZ[i] = queryBand(world, blocks, BlockPos{x, y, z});
        rowZ[i] = y - cell.y;
    }
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 2; ++j) {
            const int e = 2 * i - 1;
            const int f = 2 * j - 1;
            int x = vx, y = vy, z = vz;
            if (a1 == 0) {
                x += e;
            } else if (a1 == 1) {
                y += e;
            } else {
                z += e;
            }
            if (a2 == 0) {
                x += f;
            } else if (a2 == 1) {
                y += f;
            } else {
                z += f;
            }
            bandD[i][j] = queryBand(world, blocks, BlockPos{x, y, z});
            rowD[i][j] = y - cell.y;
        }
    }

    // The face table's corners c0..c3 (out[0..3], the emitMergedQuad
    // vertex order for a 1x1 run — the mask ao[4] convention). The e
    // direction at corner k is away from the cell at that corner
    // (edgeSign of the corner's ACTUAL component on each in-plane axis
    // — the table's edgeA/edgeB axes cross the ascending in-plane pair,
    // so the component, not the k-bit, selects it); yLocal is the
    // corner's table y (a full-cube face corner's y: 0 or 1).
    for (int k = 0; k < 4; ++k) {
        const int e1 = edgeSign(cornerAxis(face.corner[k][0],
                                           face.corner[k][1],
                                           face.corner[k][2], a1));
        const int e2 = edgeSign(cornerAxis(face.corner[k][0],
                                           face.corner[k][1],
                                           face.corner[k][2], a2));
        const int i1 = (e1 > 0) ? 1 : 0; // the sign-index (0 = -1, 1 = +1)
        const int i2 = (e2 > 0) ? 1 : 0;
        const float yLocal = static_cast<float>(face.corner[k][1]);
        out[k] = aoCornerLevel(
            aoBandCovers(yLocal, rowX[i1], bandX[i1]),
            aoBandCovers(yLocal, rowZ[i2], bandZ[i2]),
            aoBandCovers(yLocal, rowD[i1][i2], bandD[i1][i2]));
    }
}

Result<bool> aoEnabled(const Config& config) {
    const Json* j = config.at("render.ao");
    if (j == nullptr) {
        return true; // absent: the code default (the render.weather
                     // precedent — config.json is untouched)
    }
    if (!j->isBool()) {
        return MC_FAIL("config render.ao must be a boolean (true/false), "
                       "got a non-boolean");
    }
    return j->asBool();
}

} // namespace mc
