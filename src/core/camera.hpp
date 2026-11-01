// Camera: a pure first-person view (position + yaw/pitch -> view matrix).
//
// Phase 5: driven by the Player, which owns the first-person state (eye
// position, orientation, movement). The Phase 1 free-fly update() was a
// demo scaffold and has been replaced by the Player.
//
// Conventions (shared with Player):
//   yaw:   radians, 0 = facing -Z, positive turns left (counter-clockwise
//          viewed from above).
//   pitch: radians, 0 = horizontal, positive looks up. (The Player clamps
//          it to +/- ~89 deg.)
#pragma once

#include "core/math/mat4.hpp"
#include "core/math/vec3.hpp"

namespace mc {

class Camera {
public:
    Vec3 position {0.0f, 3.0f, 8.0f};
    float yaw = 0.0f;
    float pitch = 0.0f;

    Vec3 forward() const;
    Vec3 right() const;

    // The view's actual up axis: orthonormal with forward and right,
    // the up that viewMatrix()'s lookAt derives for the player's clamped
    // pitch (|pitch| < 90 deg). Cross of the two existing accessors —
    // no new trig, lock-step with the view matrix.
    Vec3 up() const;

    // World-to-camera matrix.
    Mat4 viewMatrix() const;

    void reset();
};

} // namespace mc
