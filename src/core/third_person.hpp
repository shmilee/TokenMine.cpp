// UX-4: the third-person back camera — the pure math behind it.
//
// The camera orbits BEHIND the player's eye on a fixed radius
// (kThirdPersonDistance by default; V2-19: the wheel-adjusted session
// distance, resolveThirdPersonCamera's `distance` argument), the
// Minecraft third-person convention:
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
// Game calls resolveThirdPersonCamera once per frame while in third
// person: the back view at the player's own angles, and (V2-1) the
// front view at the flipped angles (thirdPersonFrontAngles); first
// person never enters this code (the Game's else branch keeps the
// legacy eye-position assignment).
#pragma once

#include "core/math/vec3.hpp"

namespace mc {

class World;
class BlockRegistry;

// The camera's orbit radius: ideal position = eye + offset, and
// |offset| = this. 4.0 blocks is the back-camera distance — and the
// DEFAULT orbit radius (V2-19: the wheel zoom's session distance starts
// here and is the resolve's default argument, so every pre-V2-19 call
// site and unit test stays bit-identical).
constexpr float kThirdPersonDistance = 4.0f;

// V2-19 (the wheel zoom): the clamp band + the per-tick step of the
// third-person orbit distance the mouse wheel adjusts (the Game owns the
// session value — m_thirdPersonDistance — and passes it to
// resolveThirdPersonCamera as `distance`). Documented deviation: the
// vanilla Minecraft has no wheel zoom (the mod-convention range, the
// user's request); the default kThirdPersonDistance keeps a no-input run
// bit-identical (the structural-immunity root).
constexpr float kZoomMin = 2.0f;
constexpr float kZoomMax = 8.0f;
constexpr float kZoomStep = 0.5f;

// How far in FRONT of an occluding face the clamped camera stops.
constexpr float kCameraOcclusionMargin = 0.1f;

// The orbit center's lift along the view's up axis (Camera::up): the
// camera orbits a point kThirdPersonOrbitHeight ABOVE the eye instead
// of the eye itself — the Minecraft third-person convention, the 2.0.0
// fix for the screen-center crosshair sitting on the player's head.
// Lifted along the view's own up axis, the head drops BELOW the screen
// center by a fixed amount at every yaw/pitch (up_cam is orthonormal
// with the view forward by construction, so looking up or down never
// puts the head back on the crosshair).
//
// Derivation at pitch 0 (up_cam = +Y), the shipped 70 deg vertical FOV
// in a 744 px window (~531 px per world unit at 1 block): the head's
// top (eye + 0.18 — the 1.8 body / 1.62 eye) projects
// (0.4 - 0.18)/4 ≈ 29 px below the screen center, and the head box's
// bottom edge (eye - 0.32 — the 0.5 box centered 1.55)
// (0.4 + 0.32)/4 ≈ 96 px below — the 2 px crosshair sits clear of the
// head with a wide margin.
//
// The aim rays are UNTOUCHED (UX-8g red line): they still leave from
// eyePosition() in both modes. The screen-center ray (the view's own
// forward) is PARALLEL to the eye ray, offset by this lift along
// up_cam — a fixed third-person parallax (the Minecraft third-person
// look), documented deviation, no code compensation.
constexpr float kThirdPersonOrbitHeight = 0.4f;

// V2-1 (front view): pi as a float — the single constant the front-view
// yaw flip needs (thirdPersonFrontAngles). The codebase's other pi uses
// are <cmath> M_PI doubles in unrelated modules; none is a named float
// constant to reuse.
constexpr float kPi = 3.14159265f;

// The orbit center for a third-person frame: the eye lifted along the
// view's up axis (Camera::up) by kThirdPersonOrbitHeight. The caller
// passes eyePosition() + yaw + pitch — the same three the view matrix
// is built from, so the lift is constant for every orientation. Crouch
// needs no extra code: eyePosition() already carries the crouch eye
// drop, so the orbit center descends with it.
Vec3 thirdPersonOrbitCenter(const Vec3& eye, float yaw, float pitch);

// The unoccluded camera offset for a view orientation: the view's unit
// forward negated and scaled to `distance` (the camera sits BEHIND the
// player). pitch 0 -> (0, 0, distance); pitch +90 deg -> (0,
// -distance, 0); yaw +90 deg (facing -X) -> (distance, 0, 0). The
// negation reuses Camera::forward's exact expression, so the orbit stays
// in lock-step with the view matrix if either ever changes.
Vec3 thirdPersonCameraOffset(float yaw, float pitch, float distance);

// The third-person camera position for this frame: the ideal orbit
// position, pulled back to the first occluding face when a block sits
// between it and the eye. orbitCenter = the lifted orbit point (the
// caller passes thirdPersonOrbitCenter(eyePosition(), yaw, pitch) —
// 2.0.0: the eye kThirdPersonOrbitHeight up the view's up axis). The
// function itself is agnostic: the ideal->orbitCenter segment has
// length `distance` either way, so the occlusion logic is unchanged.
//
// V2-19 (the wheel zoom): `distance` is the orbit radius — the
// wheel-adjusted session value the Game passes (clamped to
// [kZoomMin, kZoomMax]). The default kThirdPersonDistance keeps every
// pre-V2-19 call site AND unit test bit-identical (the red line): the
// occlusion clamp simply follows the radius (a farther camera is
// clamped by the same first face — the correct behavior).
//
// Occlusion, in order:
//   1. the cell containing the ideal position is checked EXPLICITLY
//      (raycastBlock does not test its origin cell — a camera whose
//      ideal position is embedded in a block must still resolve):
//      non-air -> t = the analytic distance out of that cell along the
//      view direction (the DDA first step, the same arithmetic
//      world/raycast.cpp's walk starts with);
//   2. otherwise t = the raycastBlock hit distance along the segment
//      ideal -> orbitCenter (reach = distance, the exact length of that
//      segment), or no hit -> the unoccluded case.
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
// length is exactly distance != 0, so the normalization is safe). Pure
// read-only World access. V2-17 gap fill: the occlusion ray is
// partial-solid-aware for slab cells (the raycastBlock twin — the
// registry is needed for the shape / state read), so a camera between
// the eye and a slab's AIR half passes through it (the slab's interior
// surface or the block behind occludes).
Vec3 resolveThirdPersonCamera(const World& world, const BlockRegistry& blocks,
                              const Vec3& orbitCenter, float yaw, float pitch,
                              float distance = kThirdPersonDistance);

// V2-1: the third-person FRONT view — the back camera's math executed at
// the flipped view angles.
//
// The flip: yaw' = yaw + kPi, pitch' = -pitch. Derivation (one trig
// source, Camera::forward = (sin yaw * -cos pitch, sin pitch,
// cos yaw * -cos pitch), hard rule 5):
//   forward(yaw', pitch') = -forward(yaw, pitch)
// so -forward(yaw', pitch') = +forward(yaw, pitch) = the player's
// FORWARD: resolveThirdPersonCamera (the back camera's identical
// geometry) places the camera kThirdPersonDistance IN FRONT of the eye,
// looking at the player — the player's front is visible (the user's
// request: "see my own front").
//
// The view up is invariant under the flip: up_cam = cross(right,
// forward) and right(yaw') = -right(yaw), forward(yaw') = -forward
// (each to the sin(yaw + kPi) ulp), so up_cam(yaw', pitch') =
// cross(-right, -forward) = up_cam(yaw, pitch). That is why the Game
// keeps passing the PLAYER's angles to thirdPersonOrbitCenter: the same
// kThirdPersonOrbitHeight lift, the head stays 29-96 px below the
// screen center exactly as in the back view. The occlusion / fluid /
// embedded clamps of resolveThirdPersonCamera apply symmetrically — it
// is the same geometry mirrored about the orbit center, no new branch.
struct ThirdPersonAngles {
    float yaw;
    float pitch;
};
ThirdPersonAngles thirdPersonFrontAngles(float yaw, float pitch);

} // namespace mc
