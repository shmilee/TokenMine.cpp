#include "core/camera.hpp"

#include <algorithm>
#include <cmath>

#include "core/input.hpp"

namespace mc {

void Camera::update(double dt, const Input& input) {
    // Mouse look.
    const Vec2 delta = input.mouseDelta();
    yaw -= delta.x * mouseSensitivity;
    pitch -= delta.y * mouseSensitivity;
    pitch = std::clamp(pitch, -kMaxPitch, kMaxPitch);

    // Movement (planar).
    Vec3 direction {0.0f, 0.0f, 0.0f};
    const Vec3 fwd = forward();
    const Vec3 flat {fwd.x, 0.0f, fwd.z};
    const Vec3 flatFwd = normalized(flat);
    const Vec3 flatRight {flatFwd.z * -1.0f, 0.0f, flatFwd.x};

    if (input.isDown(GameKey::W)) {
        direction += flatFwd;
    }
    if (input.isDown(GameKey::S)) {
        direction -= flatFwd;
    }
    if (input.isDown(GameKey::D)) {
        direction += flatRight;
    }
    if (input.isDown(GameKey::A)) {
        direction -= flatRight;
    }
    if (input.isDown(GameKey::Space)) {
        direction.y += 1.0f;
    }

    if (lengthSq(direction) > 0.0f) {
        const float speed = input.isDown(GameKey::LeftShift)
                                ? moveSpeed * sprintMultiplier
                                : moveSpeed;
        position += normalized(direction) * (speed * static_cast<float>(dt));
    }
}

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
