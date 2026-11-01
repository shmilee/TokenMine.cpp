#include "core/camera.hpp"

#include <cmath>

namespace mc {

Vec3 Camera::forward() const {
    const float cp = std::cos(pitch);
    return {std::sin(yaw) * -cp, std::sin(pitch), std::cos(yaw) * -cp};
}

Vec3 Camera::right() const {
    return {std::cos(yaw), 0.0f, std::sin(yaw) * -1.0f};
}

Vec3 Camera::up() const {
    // The up that viewMatrix()'s lookAt actually builds: lookAt derives
    // s = normalized(cross(f, unitY)) and u = cross(s, f) with
    // f = forward(). cross(f, unitY) = cos(pitch) * right() (a POSITIVE
    // scalar — the player clamps pitch to +/- ~89 deg,
    // player::kMaxPitch), so s = right() and u = cross(right, f). The
    // cross reuses forward()/right()'s single trig source: orthonormal
    // by construction, no new trig.
    return cross(right(), forward());
}

Mat4 Camera::viewMatrix() const {
    return Mat4::lookAt(position, position + forward(), Vec3::unitY());
}

void Camera::reset() {
    position = {0.0f, 3.0f, 8.0f};
    yaw = 0.0f;
    pitch = 0.0f;
}

} // namespace mc
