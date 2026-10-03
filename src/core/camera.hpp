// First-person camera. Phase 1 provides free-fly demo controls (WASD +
// mouse look); gravity/collision are added with the Player in Phase 5.
//
// Conventions:
//   yaw:   radians, 0 = facing -Z, positive turns left (counter-clockwise
//          viewed from above).
//   pitch: radians, 0 = horizontal, positive looks up. Clamped to
//          +/- (90 deg - 0.5 deg) to keep the view matrix non-degenerate.
#pragma once

#include "core/math/mat4.hpp"
#include "core/math/vec3.hpp"

namespace mc {

class Input;

class Camera {
public:
    Vec3 position {0.0f, 3.0f, 8.0f};
    float yaw = 0.0f;
    float pitch = 0.0f;

    float moveSpeed = 5.0f;         // units / second
    float sprintMultiplier = 2.0f;
    float mouseSensitivity = 0.0025f; // radians / pixel

    // Apply movement (WASD on the ground plane, Space = up) and mouse look.
    void update(double dt, const Input& input);

    Vec3 forward() const;
    Vec3 right() const;

    // World-to-camera matrix.
    Mat4 viewMatrix() const;

    void reset();

private:
    static constexpr float kMaxPitch = 1.5533f; // ~89 deg
};

} // namespace mc
