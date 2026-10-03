// Shared six-face table for a full-cube block model.
//
// Used by both the chunk mesher (one block per cell, neighbor culling) and
// the dropped-item mesh (a single floating cube, all faces). Keeping the
// corner order and winding in one place guarantees the two renderers agree.
//
// Corner offsets are 0/1 per axis relative to the block's lower corner;
// corners are ordered counter-clockwise when viewed from outside the block
// (GL front faces). Winding is guarded by unit tests
// (FaceWindingIsCounterClockwiseFromOutside and the item-mesh equivalent):
// cross(corner1 - corner0, corner2 - corner0) must point along the face
// normal for every face.
//
// NOTE: the Phase 1/2 unit-cube table had the +Y/-Y rows wound backwards
// (visible only from inside the cube). Its screenshot verification sampled
// side faces, which hid the bug; the Y rows here are the corrected versions
// (the Phase 3 fix, archived in KNOWN_ISSUES.md).
#pragma once

#include "block/block.hpp"
#include "core/math/vec3.hpp"

namespace mc {

struct CubeFace {
    int dx, dy, dz; // neighbor block offset
    Vec3 normal;
    int corner[4][3];
    int textureSlot; // 0 = top, 1 = side, 2 = bottom
};

inline constexpr CubeFace kCubeFaces[6] = {
    {0, 1, 0, {0.0f, 1.0f, 0.0f},
     {{0, 1, 0}, {0, 1, 1}, {1, 1, 1}, {1, 1, 0}}, 0}, // +Y (top)
    {0, -1, 0, {0.0f, -1.0f, 0.0f},
     {{0, 0, 1}, {0, 0, 0}, {1, 0, 0}, {1, 0, 1}}, 2}, // -Y (bottom)
    {0, 0, 1, {0.0f, 0.0f, 1.0f},
     {{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}}, 1}, // +Z
    {0, 0, -1, {0.0f, 0.0f, -1.0f},
     {{1, 0, 0}, {0, 0, 0}, {0, 1, 0}, {1, 1, 0}}, 1}, // -Z
    {1, 0, 0, {1.0f, 0.0f, 0.0f},
     {{1, 0, 1}, {1, 0, 0}, {1, 1, 0}, {1, 1, 1}}, 1}, // +X
    {-1, 0, 0, {-1.0f, 0.0f, 0.0f},
     {{0, 0, 0}, {0, 0, 1}, {0, 1, 1}, {0, 1, 0}}, 1}, // -X
};

// Which texture name is applied to a face slot (top/side/bottom).
inline const std::string& cubeFaceTexture(const Block& block, int slot) {
    switch (slot) {
        case 0:
            return block.model.textures.top;
        case 2:
            return block.model.textures.bottom;
        default:
            return block.model.textures.side;
    }
}

} // namespace mc
