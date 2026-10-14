// Minimal 4x4 matrix, column-major (OpenGL native layout).
//
// Indexing: m[col * 4 + row]. Standard right-handed coordinate system,
// OpenGL conventions (perspective maps z to [-1, 1]).
#pragma once

#include <cassert>
#include <cmath>

#include "core/math/vec3.hpp"
#include "core/math/vec4.hpp"

namespace mc {

class Mat4 {
public:
    // Column-major storage, identity by default.
    float m[16] = {1, 0, 0, 0,  //
                   0, 1, 0, 0,  //
                   0, 0, 1, 0,  //
                   0, 0, 0, 1};

    constexpr Mat4() = default;

    // row/col in [0, 4)
    constexpr float at(int row, int col) const { return m[col * 4 + row]; }
    void set(int row, int col, float value) { m[col * 4 + row] = value; }

    bool operator==(const Mat4& o) const {
        for (int i = 0; i < 16; ++i) {
            if (m[i] != o.m[i]) {
                return false;
            }
        }
        return true;
    }

    // -- constructors --------------------------------------------------------
    static constexpr Mat4 identity() { return Mat4(); }

    // fov in radians, vertical field of view.
    static Mat4 perspective(float fovyRadians, float aspect, float near,
                            float far) {
        assert(fovyRadians > 0.0f && fovyRadians < static_cast<float>(M_PI));
        assert(aspect > 0.0f && near > 0.0f && far > near);
        Mat4 result {};
        const float f = 1.0f / std::tan(fovyRadians * 0.5f);
        result.set(0, 0, f / aspect);
        result.set(1, 1, f);
        result.set(2, 2, -(far + near) / (far - near));
        result.set(2, 3, -2.0f * far * near / (far - near));
        result.set(3, 2, -1.0f);
        result.set(3, 3, 0.0f); // identity leaves 1 here; perspective needs 0
        return result;
    }

    static Mat4 ortho(float left, float right, float bottom, float top,
                      float near, float far) {
        Mat4 result {};
        result.set(0, 0, 2.0f / (right - left));
        result.set(1, 1, 2.0f / (top - bottom));
        result.set(2, 2, -2.0f / (far - near));
        result.set(0, 3, -(right + left) / (right - left));
        result.set(1, 3, -(top + bottom) / (top - bottom));
        result.set(2, 3, -(far + near) / (far - near));
        return result;
    }

    // Uniform scale about the origin (the Phase 15 screen-space icons
    // compose translate * rotate * scale).
    static Mat4 scale(float s) {
        Mat4 result {};
        result.set(0, 0, s);
        result.set(1, 1, s);
        result.set(2, 2, s);
        return result;
    }
    // Non-uniform scale (Phase 83 beacon beam: a unit column stretched in
    // Y to the level's height, x/z at 1.0).
    static Mat4 scale(float sx, float sy, float sz) {
        Mat4 result {};
        result.set(0, 0, sx);
        result.set(1, 1, sy);
        result.set(2, 2, sz);
        return result;
    }

    static Mat4 translate(float x, float y, float z) {
        Mat4 result {};
        result.set(0, 3, x);
        result.set(1, 3, y);
        result.set(2, 3, z);
        return result;
    }
    static Mat4 translate(const Vec3& v) { return translate(v.x, v.y, v.z); }

    static Mat4 rotateX(float rad) {
        Mat4 result {};
        const float c = std::cos(rad), s = std::sin(rad);
        result.set(1, 1, c);
        result.set(1, 2, -s);
        result.set(2, 1, s);
        result.set(2, 2, c);
        return result;
    }

    static Mat4 rotateY(float rad) {
        Mat4 result {};
        const float c = std::cos(rad), s = std::sin(rad);
        result.set(0, 0, c);
        result.set(0, 2, s);
        result.set(2, 0, -s);
        result.set(2, 2, c);
        return result;
    }

    static Mat4 rotateZ(float rad) {
        Mat4 result {};
        const float c = std::cos(rad), s = std::sin(rad);
        result.set(0, 0, c);
        result.set(0, 1, -s);
        result.set(1, 0, s);
        result.set(1, 1, c);
        return result;
    }

    // World-to-camera view matrix for a camera at `eye` looking at `center`.
    static Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
        const Vec3 f = normalized(center - eye);
        Vec3 s = cross(f, up);
        if (lengthSq(s) <= 0.0f) {
            // eye direction parallel to up; degenerate camera.
            s = Vec3::unitX();
        } else {
            s = normalized(s);
        }
        const Vec3 u = cross(s, f);

        Mat4 result {};
        result.set(0, 0, s.x);
        result.set(0, 1, s.y);
        result.set(0, 2, s.z);
        result.set(0, 3, -dot(s, eye));
        result.set(1, 0, u.x);
        result.set(1, 1, u.y);
        result.set(1, 2, u.z);
        result.set(1, 3, -dot(u, eye));
        result.set(2, 0, -f.x);
        result.set(2, 1, -f.y);
        result.set(2, 2, -f.z);
        result.set(2, 3, dot(f, eye));
        return result;
    }

    // -- operations ----------------------------------------------------------
    // a * b (apply b first, then a).
    static Mat4 multiply(const Mat4& a, const Mat4& b) {
        Mat4 result {};
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                float sum = 0.0f;
                for (int k = 0; k < 4; ++k) {
                    sum += a.at(row, k) * b.at(k, col);
                }
                result.set(row, col, sum);
            }
        }
        return result;
    }

    // Inverse via cofactor expansion. Returns identity for singular
    // matrices (with an assert) so callers cannot silently corrupt state.
    Mat4 inverted() const {
        const float* a = m;
        float inv[16];
        inv[0] = a[5] * a[10] * a[15] - a[5] * a[11] * a[14] - a[9] * a[6] * a[15] +
                 a[9] * a[7] * a[14] + a[13] * a[6] * a[11] - a[13] * a[7] * a[10];
        inv[4] = -a[4] * a[10] * a[15] + a[4] * a[11] * a[14] + a[8] * a[6] * a[15] -
                 a[8] * a[7] * a[14] - a[12] * a[6] * a[11] + a[12] * a[7] * a[10];
        inv[8] = a[4] * a[9] * a[15] - a[4] * a[11] * a[13] - a[8] * a[5] * a[15] +
                 a[8] * a[7] * a[13] + a[12] * a[5] * a[11] - a[12] * a[7] * a[9];
        inv[12] = -a[4] * a[9] * a[14] + a[4] * a[10] * a[13] + a[8] * a[5] * a[14] -
                  a[8] * a[6] * a[13] - a[12] * a[5] * a[10] + a[12] * a[6] * a[9];
        inv[1] = -a[1] * a[10] * a[15] + a[1] * a[11] * a[14] + a[9] * a[2] * a[15] -
                 a[9] * a[3] * a[14] - a[13] * a[2] * a[11] + a[13] * a[3] * a[10];
        inv[5] = a[0] * a[10] * a[15] - a[0] * a[11] * a[14] - a[8] * a[2] * a[15] +
                 a[8] * a[3] * a[14] + a[12] * a[2] * a[11] - a[12] * a[3] * a[10];
        inv[9] = -a[0] * a[9] * a[15] + a[0] * a[11] * a[13] + a[8] * a[1] * a[15] -
                 a[8] * a[3] * a[13] - a[12] * a[1] * a[11] + a[12] * a[3] * a[9];
        inv[13] = a[0] * a[9] * a[14] - a[0] * a[10] * a[13] - a[8] * a[1] * a[14] +
                  a[8] * a[2] * a[13] + a[12] * a[1] * a[10] - a[12] * a[2] * a[9];
        inv[2] = a[1] * a[6] * a[15] - a[1] * a[7] * a[14] - a[5] * a[2] * a[15] +
                 a[5] * a[3] * a[14] + a[13] * a[2] * a[7] - a[13] * a[3] * a[6];
        inv[6] = -a[0] * a[6] * a[15] + a[0] * a[7] * a[14] + a[4] * a[2] * a[15] -
                 a[4] * a[3] * a[14] - a[12] * a[2] * a[7] + a[12] * a[3] * a[6];
        inv[10] = a[0] * a[5] * a[15] - a[0] * a[7] * a[13] - a[4] * a[1] * a[15] +
                  a[4] * a[3] * a[13] + a[12] * a[1] * a[7] - a[12] * a[3] * a[5];
        inv[14] = -a[0] * a[5] * a[14] + a[0] * a[6] * a[13] + a[4] * a[1] * a[14] -
                  a[4] * a[2] * a[13] - a[12] * a[1] * a[6] + a[12] * a[2] * a[5];
        inv[3] = -a[1] * a[6] * a[11] + a[1] * a[7] * a[10] + a[5] * a[2] * a[11] -
                 a[5] * a[3] * a[10] - a[9] * a[2] * a[7] + a[9] * a[3] * a[6];
        inv[7] = a[0] * a[6] * a[11] - a[0] * a[7] * a[10] - a[4] * a[2] * a[11] +
                 a[4] * a[3] * a[10] + a[8] * a[2] * a[7] - a[8] * a[3] * a[6];
        inv[11] = -a[0] * a[5] * a[11] + a[0] * a[7] * a[9] + a[4] * a[1] * a[11] -
                  a[4] * a[3] * a[9] - a[8] * a[1] * a[7] + a[8] * a[3] * a[5];
        inv[15] = a[0] * a[5] * a[10] - a[0] * a[6] * a[9] - a[4] * a[1] * a[10] +
                  a[4] * a[2] * a[9] + a[8] * a[1] * a[6] - a[8] * a[2] * a[5];

        float det = a[0] * inv[0] + a[1] * inv[4] + a[2] * inv[8] + a[3] * inv[12];
        if (std::fabs(det) < 1e-12f) {
            assert(false && "inverted(): singular matrix");
            return identity();
        }
        det = 1.0f / det;

        Mat4 result {};
        for (int i = 0; i < 16; ++i) {
            result.m[i] = inv[i] * det;
        }
        return result;
    }

    Mat4 transposed() const {
        Mat4 result {};
        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 4; ++col) {
                result.set(row, col, at(col, row));
            }
        }
        return result;
    }

    // -- transforms ----------------------------------------------------------
    Vec4 transformVec4(const Vec4& v) const {
        return {at(0, 0) * v.x + at(0, 1) * v.y + at(0, 2) * v.z + at(0, 3) * v.w,
                at(1, 0) * v.x + at(1, 1) * v.y + at(1, 2) * v.z + at(1, 3) * v.w,
                at(2, 0) * v.x + at(2, 1) * v.y + at(2, 2) * v.z + at(2, 3) * v.w,
                at(3, 0) * v.x + at(3, 1) * v.y + at(3, 2) * v.z + at(3, 3) * v.w};
    }

    // Transforms a point (w = 1) and performs perspective division.
    Vec3 transformPoint(const Vec3& p) const {
        const Vec4 c = transformVec4(Vec4::fromVec3(p, 1.0f));
        if (std::fabs(c.w) < 1e-12f) {
            // Point at infinity / behind singular projection; return zeros
            // rather than dividing by zero.
            return Vec3::zero();
        }
        return {c.x / c.w, c.y / c.w, c.z / c.w};
    }

    // Transforms a direction (w = 0); no translation, no division.
    Vec3 transformDirection(const Vec3& d) const {
        return {at(0, 0) * d.x + at(0, 1) * d.y + at(0, 2) * d.z,
                at(1, 0) * d.x + at(1, 1) * d.y + at(1, 2) * d.z,
                at(2, 0) * d.x + at(2, 1) * d.y + at(2, 2) * d.z};
    }
};

} // namespace mc
