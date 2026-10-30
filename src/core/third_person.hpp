// UX-4: the third-person back camera — the pure math behind it.
//
// The camera orbits BEHIND the player's eye on a fixed radius
// (kThirdPersonDistance), the Minecraft third-person convention:
//   offset = -forward(yaw, pitch) * distance
// (forward is the view's own direction vector — Camera::forward, the
// single trig source shared with the view matrix). The camera then
// clamps against the world: the first occluding BLOCK face between the
// ideal position and the eye ends the camera kCameraOcclusionMargin
// away from that face, on the EYE'S side (raycast case: the camera
// stops short of the face; embedded case: the camera exits the face and
// takes the gap outside — never left inside the wall). Fluids are
// transparent, like the block raycast; a missing chunk or out-of-range
// Y reads air.
//
// Both functions are pure and read-only (hard rule 8: deterministic —
// a no-input run reproduces byte for byte; hard rule 2: no GL). The
// Game calls resolveThirdPersonCamera once per frame while
// m_cameraMode == ThirdBack; first person never enters this code (the
// Game's else branch keeps the legacy eye-position assignment).
#pragma once

#include "core/math/vec3.hpp"

namespace mc {

class World;

// The camera's orbit radius: ideal position = eye + offset, and
// |offset| = this. 4.0 blocks is the back-camera distance.
constexpr float kThirdPersonDistance = 4.0f;

// How far in FRONT of an occluding face the clamped camera stops.
constexpr float kCameraOcclusionMargin = 0.1f;

// The unoccluded camera offset for a view orientation: the view's unit
// forward negated and scaled to `distance` (the camera sits BEHIND the
// player). pitch 0 -> (0, 0, distance); pitch +90 deg -> (0,
// -distance, 0); yaw +90 deg (facing -X) -> (distance, 0, 0). The
// negation reuses Camera::forward's exact expression, so the orbit stays
// in lock-step with the view matrix if either ever changes.
Vec3 thirdPersonCameraOffset(float yaw, float pitch, float distance);

// The third-person camera position for this frame: the ideal orbit
// position, pulled back to the first occluding face when a block sits
// between it and the eye. orbitCenter = the player's eye position (the
// caller passes eyePosition()).
//
// Occlusion, in order:
//   1. the cell containing the ideal position is checked EXPLICITLY
//      (raycastBlock does not test its origin cell — a camera whose
//      ideal position is embedded in a block must still resolve):
//      non-air -> t = the analytic distance out of that cell along the
//      view direction (the DDA first step, the same arithmetic
//      world/raycast.cpp's walk starts with);
//   2. otherwise t = the raycastBlock hit distance along the segment
//      ideal -> orbitCenter (reach = kThirdPersonDistance, the exact
//      length of that segment), or no hit -> the unoccluded case.
//
//   embedded origin cell -> ideal + dir * min(t + margin, distance)
//                (the face is the eye-side boundary of the occluding
//                cell: exit it, then take the gap OUTSIDE, on the
//                player's side — stopping t - margin would leave the
//                camera up to a full block deep in the wall. The clamp
//                to distance keeps the camera from passing the orbit
//                center; t <= ~1.1 in practice, so it is a defensive
//                bound);
//   raycast hit          -> ideal + dir * max(0, t - margin)
//                (the face is ahead of the camera: stop the margin
//                short of it; when t < margin the max clamps to zero
//                and the camera stays at `ideal`, its own air cell);
//   t missing            -> ideal, returned bit-exactly.
//
// dir = normalize(orbitCenter - ideal) = the unit forward (the offset
// length is exactly kThirdPersonDistance != 0, so the normalization is
// safe). Pure read-only World access.
Vec3 resolveThirdPersonCamera(const World& world, const Vec3& orbitCenter,
                              float yaw, float pitch);

} // namespace mc
