// Minimal 4D vector (homogeneous coordinates). Header-only.
#pragma once

#include "core/math/vec3.hpp"

namespace mc {

struct Vec4 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;

    constexpr Vec4() = default;
    constexpr Vec4(float x, float y, float z, float w)
        : x(x), y(y), z(z), w(w) {}

    static constexpr Vec4 fromVec3(const Vec3& v, float w = 1.0f) {
        return {v.x, v.y, v.z, w};
    }

    Vec3 toVec3() const { return {x, y, z}; }
};

} // namespace mc
