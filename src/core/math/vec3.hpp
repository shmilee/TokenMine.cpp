// Minimal 3D vector. Header-only, no dependencies.
#pragma once

#include <cmath>

namespace mc {

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    constexpr Vec3() = default;
    constexpr Vec3(float x, float y, float z) : x(x), y(y), z(z) {}

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3 operator/(float s) const { return {x / s, y / s, z / s}; }
    Vec3 operator-() const { return {-x, -y, -z}; }

    Vec3& operator+=(const Vec3& o) {
        x += o.x;
        y += o.y;
        z += o.z;
        return *this;
    }
    Vec3& operator-=(const Vec3& o) {
        x -= o.x;
        y -= o.y;
        z -= o.z;
        return *this;
    }
    Vec3& operator*=(float s) {
        x *= s;
        y *= s;
        z *= s;
        return *this;
    }
    Vec3& operator/=(float s) {
        x /= s;
        y /= s;
        z /= s;
        return *this;
    }

    bool operator==(const Vec3& o) const {
        return x == o.x && y == o.y && z == o.z;
    }

    static constexpr Vec3 zero() { return {0.0f, 0.0f, 0.0f}; }
    static constexpr Vec3 unitX() { return {1.0f, 0.0f, 0.0f}; }
    static constexpr Vec3 unitY() { return {0.0f, 1.0f, 0.0f}; }
    static constexpr Vec3 unitZ() { return {0.0f, 0.0f, 1.0f}; }
};

inline Vec3 operator*(float s, const Vec3& v) { return v * s; }

inline float dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}

inline float lengthSq(const Vec3& v) { return dot(v, v); }

inline float length(const Vec3& v) { return std::sqrt(dot(v, v)); }

// Returns the zero vector for zero-length input (avoids division by zero;
// callers that must reject zero vectors should check length first).
inline Vec3 normalized(const Vec3& v) {
    const float len = length(v);
    if (len <= 0.0f) {
        return Vec3::zero();
    }
    return v / len;
}

inline Vec3 lerp(const Vec3& a, const Vec3& b, float t) {
    return a + (b - a) * t;
}

} // namespace mc
