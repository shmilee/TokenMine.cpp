// Arrow mesh + orientation (Phase 21): the visual of a launched arrow.
//
// The geometry is one box along the local -Z axis (the direction of
// travel: the render model rotates local -Z onto the velocity, see
// arrowOrientation), centered on the origin — the game translates it to
// the arrow's collision-box center. It is drawn in the basic shader's
// solid-color path (u_useTexture = 0 + u_color): v1 has no arrow texture
// (shaft + tip + fletching are one brown; see KNOWN_ISSUES.md).
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
// Projectile::kSize (0.25), the visual is longer and much thinner.
inline constexpr float kArrowLength = 0.55f;
inline constexpr float kArrowThickness = 0.05f;

// The unlit-color render of the arrow (shaft + tip, one brown).
inline constexpr float kArrowColorR = 0.36f;
inline constexpr float kArrowColorG = 0.27f;
inline constexpr float kArrowColorB = 0.16f;

// 24 vertices / 36 indices (six 4-vertex faces, the shared cube face
// table scaled anisotropically: x/y by kArrowThickness, z by
// kArrowLength).
ItemMeshGeometry makeArrowGeometry();

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
