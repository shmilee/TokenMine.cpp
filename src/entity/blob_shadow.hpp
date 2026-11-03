// Blob shadow geometry (V2-6): the flat quad under each mob (and the
// player, in third person) — the "no blob shadow" documented deviation
// consumed. Pure geometry: the render (Game::drawMobShadows) scales a
// unit quad to blobShadowSide(width) at blobShadowCenter(position).
//
// The shadow sits at the TOP of the block directly below the entity's
// bottom-center (feet): an entity resting on a block surface has an
// integer feet y, so this is exactly the supporting face; mid-air (a
// jump) it sticks to the top of the cell below the feet — the Minecraft
// blob look (the shadow does not float at the feet).
#pragma once

#include <cmath>

#include "core/math/vec3.hpp"

namespace mc {

// The shadow quad's side as a multiple of the entity's model width
// (1.5 = a bit wider than the body, the MC blob look).
inline constexpr float kMobShadowSideScale = 1.5f;
// The lift of the quad above the supporting face (blocks): at exact
// rest the quad would coincide with the block's top face and z-fight it
// under the LESS depth test; 0.01 keeps it visually on the face
// (sub-pixel at every view distance the quad is visible at).
inline constexpr float kMobShadowLift = 0.01f;
// The shadow's black translucency (the MC blob alpha, ~0.3).
inline constexpr float kMobShadowAlpha = 0.3f;

// The quad's center: under the entity's bottom-center, at the top of
// the block below the feet + kMobShadowLift (see the file head).
inline Vec3 blobShadowCenter(const Vec3& pos) {
    return {pos.x, std::floor(pos.y) + kMobShadowLift, pos.z};
}

// The quad's side (blocks) for an entity model of `width`.
inline float blobShadowSide(float width) {
    return width * kMobShadowSideScale;
}

} // namespace mc
