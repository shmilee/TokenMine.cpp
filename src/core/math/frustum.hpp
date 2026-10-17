// Frustum: the six view-clip planes extracted from a model-view-projection
// matrix (Gribb-Hartmann row combinations, column-major storage), used for
// conservative axis-aligned bounding box culling (Phase 99.9: the render
// loop must not submit draw calls for chunks the camera cannot see — at
// view distance 80 the loaded ring holds ~200k chunks but the frustum
// covers a fraction of them; drawing them all is the vd80 frame collapse).
//
// A box is culled only when it lies entirely outside at least one plane
// (the positive-vertex test), so a box touching the view volume — even
// tangentially — is kept: culling can only REMOVE geometry the GPU would
// have fully clipped anyway (zero coverage), never remove visible pixels.
#pragma once

#include "core/math/mat4.hpp"
#include "core/math/vec3.hpp"

namespace mc {

struct Frustum {
    // Plane (a, b, c, d): a*x + b*y + c*z + d = 0, the inside is >= 0.
    struct Plane {
        float a = 1.0f, b = 0.0f, c = 0.0f, d = 0.0f;
        float* data() { return &a; }
        const float* data() const { return &a; }
    };
    Plane planes[6]; // left, right, bottom, top, near, far

    // Extracts the six planes from the MVP matrix. The clip-space bounds
    // are matrix row3 +/- rowK for K = 0, 1, 2 (rows x, y, z). Column-
    // major storage: row K is the strided element set (at(K,0)..at(K,3)).
    static Frustum fromMVP(const Mat4& m) {
        Frustum f;
        float rows[3][4];
        for (int k = 0; k < 3; ++k) {
            for (int i = 0; i < 4; ++i) {
                rows[k][i] = m.at(k, i);
            }
        }
        const float r3[4] = {m.at(3, 0), m.at(3, 1), m.at(3, 2), m.at(3, 3)};
        for (int k = 0; k < 3; ++k) {
            // k=0: left/right from row0; k=1: bottom/top from row1;
            // k=2: near/far from row2.
            const int idx = 2 * k;
            float* pos = f.planes[idx].data();
            float* neg = f.planes[idx + 1].data();
            for (int i = 0; i < 4; ++i) {
                pos[i] = r3[i] + rows[k][i];
                neg[i] = r3[i] - rows[k][i];
            }
        }
        return f;
    }

    // True when the AABB [min, max] intersects or touches the frustum.
    bool intersectsAabb(const Vec3& min, const Vec3& max) const {
        for (const Plane& p : planes) {
            // The positive vertex (the corner farthest along the plane
            // normal): if even it is behind the plane, the box is out.
            const float d = p.a * (p.a > 0.0f ? max.x : min.x) +
                            p.b * (p.b > 0.0f ? max.y : min.y) +
                            p.c * (p.c > 0.0f ? max.z : min.z) + p.d;
            if (d < 0.0f) {
                return false;
            }
        }
        return true;
    }
};

} // namespace mc
