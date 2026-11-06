// Arrow mesh + orientation (Phase 21; PRE-9: the MC-style silhouette).
//
// The geometry is a three-part arrow along the local -Z axis (the
// direction of travel: the render model rotates local -Z onto the
// velocity, see arrowOrientation), centered on the origin — the game
// translates it to the arrow's collision-box center:
//   - a BROWN shaft (the main length, a thin box),
//   - a GRAY tip (a 4-sided pyramid, the pointed front — the -Z end),
//   - a WHITE fletching (a wider box at the back — the +Z end).
// It is drawn in the basic shader's solid-color path (u_useTexture = 0 +
// u_color): three draws per arrow, one per part (the classic MC arrow
// colors; v1 has no arrow texture — see docs/KNOWN_ISSUES.md).
//
// Full sky light is baked (like dropped items / mob models): the shader
// applies the day/night scale, so an arrow brightens and darkens with
// the scene.
#pragma once

#include <utility>

#include "core/math/vec3.hpp"
#include "entity/item_mesh.hpp"

namespace mc {

// Arrow visual size (blocks). Named, not magic: the collision box is
// Projectile::kSize (0.25), the visual is longer and much thinner. The
// three parts sum to kArrowLength (the pre-PRE-9 box length, so the
// overall silhouette size is unchanged — the A/B diff is the shape +
// color, not the size).
inline constexpr float kArrowLength = 0.55f;
inline constexpr float kArrowShaftLength = 0.31f; // the middle brown box
inline constexpr float kArrowTipLength = 0.12f;   // the front gray pyramid
inline constexpr float kArrowFletchLength = 0.12f; // the back white box
inline constexpr float kArrowThickness = 0.05f; // the shaft + tip base
inline constexpr float kArrowFletchWidth = 0.09f; // the fletching (wider)

// The unlit-color render of each arrow part (the classic MC arrow: a
// brown shaft, a gray/stone tip, a white fletching).
inline constexpr float kArrowShaftColorR = 0.36f;
inline constexpr float kArrowShaftColorG = 0.27f;
inline constexpr float kArrowShaftColorB = 0.16f;
inline constexpr float kArrowTipColorR = 0.62f;
inline constexpr float kArrowTipColorG = 0.62f;
inline constexpr float kArrowTipColorB = 0.62f;
inline constexpr float kArrowFletchColorR = 0.90f;
inline constexpr float kArrowFletchColorG = 0.90f;
inline constexpr float kArrowFletchColorB = 0.88f;

// The three arrow parts (local -Z = the direction of travel; the whole
// arrow is centered on the origin, spanning z in
// [-kArrowLength/2, +kArrowLength/2] = [-0.275, +0.275]):
//   tip (front)   z in [-0.275, -0.155]
//   shaft (mid)   z in [-0.155, +0.155]
//   fletch (back) z in [+0.155, +0.275]
struct ArrowGeometry {
    // The brown shaft: a thin box, the middle of the arrow.
    ItemMeshGeometry shaft;
    // The gray tip: a 4-sided pyramid (a kArrowThickness square base at
    // z = -0.155, the apex at z = -0.275, the front).
    ItemMeshGeometry tip;
    // The white fletching: a wider box, the back of the arrow.
    ItemMeshGeometry fletching;
};

// 24 vertices / 36 indices per box part (six 4-vertex faces, the shared
// cube face table scaled anisotropically); the tip is 4 triangular
// faces (12 vertices / 12 indices).
ArrowGeometry makeArrowGeometry();

// V3-14b: the ghast fireball's glow — a solid-color cube (the unlit
// render path's arrow twin: the basic shader's u_color, no texture —
// the fireball's read is the glow, not a surface). Centered on the
// origin (the draw translates it to the projectile's box center),
// no rotation (a blob reads as a sphere from any heading).
inline constexpr float kFireballSize = 0.4f; // the glow cube's edge
inline constexpr float kFireballColorR = 1.0f; // the MC fireball's
inline constexpr float kFireballColorG = 0.55f; // orange glow (the
inline constexpr float kFireballColorB = 0.15f; // blast's read)
ItemMeshGeometry makeFireballGeometry();

// V3-10b: the ender pearl's glow — the fireball's mesh REUSED at a
// smaller draw scale (the 0.25 orb's edge over the fireball's 0.4
// cube: the uniform scale factor below) in the pearl's teal (the MC
// orb's read — the unlit u_color path, the fireball's twin).
inline constexpr float kPearlDrawScale = 0.625f; // 0.25 / 0.4 (the edge ratio)
inline constexpr float kPearlColorR = 0.35f;
inline constexpr float kPearlColorG = 0.85f;
inline constexpr float kPearlColorB = 0.8f;

// The (yaw, pitch) that rotates local -Z onto `velocity` under
// model = rotateY(yaw) * rotateX(pitch) — the camera convention (yaw 0 =
// -Z: forward(yaw, pitch) = (-sin yaw * cos pitch, sin pitch,
// -cos yaw * cos pitch)), so the arrow's -Z face points along its travel
// exactly like the camera looks along its forward.
//   yaw   = atan2(-vx, -vz)   (the horizontal part)
//   pitch = atan2(vy, |horizontal|)   (the elevation)
// `velocity` does not need to be normalized. A zero or near-zero
// horizontal part (straight up/down) pins yaw to 0 (atan2(-0, -0) would
// be -pi on IEEE signed zero) — the pitch then carries the whole
// direction.
std::pair<float, float> arrowOrientation(Vec3 velocity);

} // namespace mc
