#include "core/third_person.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

#include "block/block.hpp"
#include "core/camera.hpp"
#include "world/raycast.hpp"
#include "world/world.hpp"

namespace mc {

namespace {

// The DDA first step out of the cell containing `p` along the unit
// direction `dir`: the ray parameter of the FIRST grid plane crossed
// (Amanatides & Woo — the same signed-division arithmetic as
// ddaFirstCell's tMax*0 initialization in world/raycast.cpp, whose walk
// then continues from the neighboring cell). Positive by construction;
// at least one axis of a nonzero direction is non-parallel, so the min
// is finite.
float firstExitDistance(const Vec3& p, const Vec3& dir) {
    // Per-axis floor (hard rule 1: floor semantics for negative
    // coordinates — the raycast's own cell convention).
    const int x = static_cast<int>(std::floorf(p.x));
    const int y = static_cast<int>(std::floorf(p.y));
    const int z = static_cast<int>(std::floorf(p.z));
    float t = std::numeric_limits<float>::infinity();
    if (dir.x > 0.0f) {
        t = std::min(t, (static_cast<float>(x + 1) - p.x) / dir.x);
    } else if (dir.x < 0.0f) {
        t = std::min(t, (static_cast<float>(x) - p.x) / dir.x);
    }
    if (dir.y > 0.0f) {
        t = std::min(t, (static_cast<float>(y + 1) - p.y) / dir.y);
    } else if (dir.y < 0.0f) {
        t = std::min(t, (static_cast<float>(y) - p.y) / dir.y);
    }
    if (dir.z > 0.0f) {
        t = std::min(t, (static_cast<float>(z + 1) - p.z) / dir.z);
    } else if (dir.z < 0.0f) {
        t = std::min(t, (static_cast<float>(z) - p.z) / dir.z);
    }
    return t;
}

} // namespace

Vec3 thirdPersonCameraOffset(float yaw, float pitch, float distance) {
    // The SAME expression as Camera::forward (the view matrix's single
    // trig source — same formula, same operation order), negated and
    // scaled: the camera sits BEHIND the player, on the view's own
    // sphere (the yaw 0 = -Z / pitch positive = up conventions are
    // shared with Camera, hard rule 5).
    Camera cam {};
    cam.yaw = yaw;
    cam.pitch = pitch;
    return (-cam.forward()) * distance;
}

Vec3 resolveThirdPersonCamera(const World& world, const Vec3& orbitCenter,
                              float yaw, float pitch) {
    const Vec3 ideal =
        orbitCenter +
        thirdPersonCameraOffset(yaw, pitch, kThirdPersonDistance);
    // Toward the orbit center (the eye): the unit forward. The offset
    // length is exactly kThirdPersonDistance != 0, so the normalization
    // is safe (normalized(orbitCenter - ideal) == -normalized(offset)).
    const Vec3 dir = normalized(orbitCenter - ideal);

    // The ideal position's cell, per-axis floor (the raycast's own
    // convention).
    const BlockPos cell {static_cast<int>(std::floorf(ideal.x)),
                         static_cast<int>(std::floorf(ideal.y)),
                         static_cast<int>(std::floorf(ideal.z))};

    // The distance t to the first occluding face (missing = unoccluded).
    std::optional<float> t;
    bool embedded = false;
    if (world.getBlock(cell.x, cell.y, cell.z) != BlockID::air()) {
        // The ideal position is EMBEDDED in a block: raycastBlock does
        // not test its origin cell, so measure the exit analytically
        // (the DDA first step out of `cell` along dir).
        embedded = true;
        t = firstExitDistance(ideal, dir);
    } else if (const auto hit =
                   raycastBlock(world, ideal, dir, kThirdPersonDistance)) {
        t = hit->distance;
    }
    if (!t) {
        return ideal; // unoccluded: bit-exactly the ideal position
    }
    if (embedded) {
        // The face is the EYE-SIDE boundary of the occluding cell: exit
        // it, then take the margin OUTSIDE, on the player's side of the
        // face. (Stopping t_exit - margin instead would leave the
        // camera up to a full block deep in the wall — the wall is 1.0
        // thick, the margin 0.1.) Clamped to kThirdPersonDistance: the
        // segment ideal -> eye is exactly that long, so the camera must
        // never pass the orbit center. In practice t_exit <= ~1.1, so
        // the clamp is a defensive bound, not a reachable state.
        const float pull =
            std::min(*t + kCameraOcclusionMargin, kThirdPersonDistance);
        return ideal + dir * pull;
    }
    // The face is AHEAD of the camera: stop the margin short of it.
    // When t < margin the max clamps to zero: the camera stays at
    // `ideal` (its own air cell — the face is within 0.1 blocks of it),
    // a deterministic edge that loses no view.
    return ideal + dir * std::max(0.0f, *t - kCameraOcclusionMargin);
}

} // namespace mc
