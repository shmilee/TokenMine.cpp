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

Mat4 Camera::viewMatrix() const {
    return Mat4::lookAt(position, position + forward(), Vec3::unitY());
}

void Camera::reset() {
    position = {0.0f, 3.0f, 8.0f};
    yaw = 0.0f;
    pitch = 0.0f;
}

} // namespace mc
