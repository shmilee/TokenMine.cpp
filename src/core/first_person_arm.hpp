// First-person arm (V2-6): the viewmodel arm at the bottom right of the
// first-person view — the "no hand skin, the held item only" documented
// deviation consumed. Pure gate + pose, header-only, so the game
// (drawHeldHand) and the unit tests share the exact matrix.
//
// The geometry is the player model's 'arm_r' box (data/player_body.json —
// init-validated to exist), re-centered at the origin and textured by the
// Steve-skin pa_arm tile (V2-2 — ZERO new texture; the tile's lower rows
// are the skin, so the hand reads as a skin hand, sleeve up the arm).
// The arm is STATIC (no walking swing — the documented deviation: the
// third-person body swings, the first-person viewmodel does not), but it
// FOLLOWS the held item's offset, so the bow-draw / eat-lift channels
// carry the arm with the hand.
#pragma once

#include <cmath>

#include "core/game.hpp" // Game::CameraMode (the gate's domain)
#include "core/math/mat4.hpp"
#include "core/math/vec3.hpp"

namespace mc {

// The arm box's center as an offset from the held item's (already
// bow/eat-adjusted) camera-frame offset: right of, below and slightly
// behind the hand — the arm runs from the bottom-right screen edge up to
// the hand.
inline constexpr float kArmCenterDeltaX = 0.12f;
inline constexpr float kArmCenterDeltaY = -0.28f;
inline constexpr float kArmCenterDeltaZ = 0.18f;
// The lean of the hand end from vertical (radians, toward the view
// center): the arm points up-left from the bottom-right corner.
inline constexpr float kArmLean = 0.5f;

// The visibility gate: the arm draws ONLY in first person, while the
// pointer is locked, and no panel is open — the existing held-hand gate
// (the third-person / front views hide it: the world-space body is
// visible instead; a no-input run is never locked, so it renders the
// pre-V2-6 image — the byte-identical baseline's structural-immunity
// root).
inline bool firstPersonArmVisible(Game::CameraMode mode, bool pointerLocked,
                                  bool panelOpen) {
    return mode == Game::CameraMode::First && pointerLocked && !panelOpen;
}

// The arm's placement in the CAMERA frame (x = right, y = up, z = behind
// the eye): the box centered at handOffset + the delta, rotated about Z
// by π + kArmLean. The π flips the box so the SKIN end (the pa_arm tile's
// lower rows — the hand) is UP toward the item instead of the sleeve,
// and kArmLean leans that top end left, toward the view center (the
// bottom-right sleeve end is the shoulder, toward the screen corner).
// `handOffset` carries the item's animation offsets, so the arm follows
// the bow draw / eat lift.
inline Mat4 firstPersonArmLocal(const Vec3& handOffset) {
    const Vec3 center {handOffset.x + kArmCenterDeltaX,
                       handOffset.y + kArmCenterDeltaY,
                       handOffset.z + kArmCenterDeltaZ};
    const float rot = static_cast<float>(M_PI) + kArmLean;
    return Mat4::multiply(Mat4::translate(center), Mat4::rotateZ(rot));
}

} // namespace mc
