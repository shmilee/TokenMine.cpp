#include <gtest/gtest.h>

#include <cmath>

#include "core/math/mat4.hpp"
#include "core/math/vec3.hpp"
#include "core/math/vec4.hpp"

using namespace mc;

namespace {
constexpr float kPi = 3.14159265358979f;
constexpr float kEps = 1e-4f;

bool vecNear(const Vec3& a, const Vec3& b, float eps = kEps) {
    return std::fabs(a.x - b.x) < eps && std::fabs(a.y - b.y) < eps &&
           std::fabs(a.z - b.z) < eps;
}
} // namespace

// ---------------------------------------------------------------------------
// Vec3
// ---------------------------------------------------------------------------

TEST(Vec3Test, Arithmetic) {
    const Vec3 a {1, 2, 3};
    const Vec3 b {4, 5, 6};
    EXPECT_EQ(a + b, (Vec3{5, 7, 9}));
    EXPECT_EQ(b - a, (Vec3{3, 3, 3}));
    EXPECT_EQ(a * 2.0f, (Vec3{2, 4, 6}));
    EXPECT_EQ(2.0f * a, (Vec3{2, 4, 6}));
    EXPECT_EQ((Vec3{2, 4, 6}) / 2.0f, a);
    Vec3 c = a;
    c += b;
    EXPECT_EQ(c, (Vec3{5, 7, 9}));
    c -= b;
    EXPECT_EQ(c, a);
    c *= 3.0f;
    EXPECT_EQ(c, (Vec3{3, 6, 9}));
}

TEST(Vec3Test, DotCross) {
    EXPECT_FLOAT_EQ(dot((Vec3{1, 2, 3}), (Vec3{4, 5, 6})), 32.0f);
    EXPECT_EQ(cross(Vec3::unitX(), Vec3::unitY()), Vec3::unitZ());
    EXPECT_EQ(cross(Vec3::unitY(), Vec3::unitX()), (Vec3{0, 0, -1}));
    EXPECT_FLOAT_EQ(dot(cross((Vec3{1, 2, 3}), (Vec3{4, 5, 6})),
                        (Vec3{1, 2, 3})),
                    0.0f); // cross product orthogonal to inputs
}

TEST(Vec3Test, LengthAndNormalize) {
    EXPECT_FLOAT_EQ(length((Vec3{3, 4, 0})), 5.0f);
    EXPECT_FLOAT_EQ(lengthSq((Vec3{3, 4, 0})), 25.0f);
    EXPECT_TRUE(vecNear(normalized((Vec3{3, 4, 0})),
                        (Vec3{0.6f, 0.8f, 0.0f})));
    // Zero vector must not produce NaN.
    const Vec3 z = normalized(Vec3::zero());
    EXPECT_EQ(z, Vec3::zero());
}

TEST(Vec3Test, Lerp) {
    EXPECT_TRUE(vecNear(lerp((Vec3{0, 0, 0}), (Vec3{10, 20, 30}), 0.5f),
                        (Vec3{5, 10, 15})));
    EXPECT_TRUE(vecNear(lerp((Vec3{1, 1, 1}), (Vec3{2, 2, 2}), 1.0f),
                        (Vec3{2, 2, 2})));
}

// ---------------------------------------------------------------------------
// Mat4: construction & transforms
// ---------------------------------------------------------------------------

TEST(Mat4Test, Identity) {
    const Mat4 I = Mat4::identity();
    EXPECT_EQ(I.at(0, 0), 1.0f);
    EXPECT_EQ(I.at(1, 1), 1.0f);
    EXPECT_EQ(I.at(2, 2), 1.0f);
    EXPECT_EQ(I.at(3, 3), 1.0f);
    EXPECT_EQ(I.at(0, 1), 0.0f);
    EXPECT_EQ(I.at(2, 0), 0.0f);
    EXPECT_TRUE(vecNear(I.transformPoint((Vec3{7, -3, 2})), (Vec3{7, -3, 2})));
    EXPECT_TRUE(vecNear(I.transformDirection((Vec3{0, 1, 0})),
                        (Vec3{0, 1, 0})));
}

TEST(Mat4Test, Translate) {
    const Mat4 T = Mat4::translate(1, 2, 3);
    EXPECT_TRUE(vecNear(T.transformPoint(Vec3::zero()), (Vec3{1, 2, 3})));
    // Directions are unaffected by translation.
    EXPECT_TRUE(vecNear(T.transformDirection((Vec3{1, 0, 0})),
                        (Vec3{1, 0, 0})));
}

TEST(Mat4Test, RotateY90) {
    const Mat4 R = Mat4::rotateY(kPi / 2.0f);
    // (1,0,0) rotated +90deg about Y goes to (0,0,-1).
    EXPECT_TRUE(vecNear(R.transformDirection((Vec3{1, 0, 0})),
                        (Vec3{0, 0, -1})));
    // (0,0,1) goes to (1,0,0).
    EXPECT_TRUE(vecNear(R.transformDirection((Vec3{0, 0, 1})),
                        (Vec3{1, 0, 0})));
    // Y axis unchanged.
    EXPECT_TRUE(vecNear(R.transformDirection((Vec3{0, 1, 0})),
                        (Vec3{0, 1, 0})));
}

TEST(Mat4Test, RotateX90) {
    const Mat4 R = Mat4::rotateX(kPi / 2.0f);
    EXPECT_TRUE(vecNear(R.transformDirection((Vec3{0, 1, 0})),
                        (Vec3{0, 0, 1})));
    EXPECT_TRUE(vecNear(R.transformDirection((Vec3{0, 0, -1})),
                        (Vec3{0, 1, 0})));
}

TEST(Mat4Test, RotateZ90) {
    const Mat4 R = Mat4::rotateZ(kPi / 2.0f);
    EXPECT_TRUE(vecNear(R.transformDirection((Vec3{1, 0, 0})),
                        (Vec3{0, 1, 0})));
}

TEST(Mat4Test, MultiplyOrder) {
    // M = rotateY(90) * translate(1,0,0): translate applies first.
    const Mat4 M =
        Mat4::multiply(Mat4::rotateY(kPi / 2.0f), Mat4::translate(1, 0, 0));
    EXPECT_TRUE(vecNear(M.transformPoint(Vec3::zero()), (Vec3{0, 0, -1})));

    // Matrix multiplication must not be commutative in general.
    const Mat4 A = Mat4::multiply(Mat4::rotateX(kPi / 2.0f),
                                  Mat4::rotateY(kPi / 2.0f));
    const Mat4 B = Mat4::multiply(Mat4::rotateY(kPi / 2.0f),
                                  Mat4::rotateX(kPi / 2.0f));
    EXPECT_NE(A, B);
    // But A * A^-1 == identity for any matrix.
    const Mat4 AB = Mat4::multiply(A, A.inverted());
    EXPECT_TRUE(vecNear(AB.transformPoint((Vec3{3, -2, 5})), (Vec3{3, -2, 5})));
}

// ---------------------------------------------------------------------------
// Perspective
// ---------------------------------------------------------------------------

TEST(Mat4Test, PerspectiveElements) {
    // 90deg FOV, aspect 1, near 1, far 10: f = 1/tan(45deg) = 1.
    const Mat4 P = Mat4::perspective(kPi / 2.0f, 1.0f, 1.0f, 10.0f);
    EXPECT_NEAR(P.at(0, 0), 1.0f, kEps);
    EXPECT_NEAR(P.at(1, 1), 1.0f, kEps);
    EXPECT_NEAR(P.at(2, 2), -(11.0f / 9.0f), kEps);
    EXPECT_NEAR(P.at(2, 3), -(20.0f / 9.0f), kEps);
    EXPECT_NEAR(P.at(3, 2), -1.0f, kEps);
    EXPECT_NEAR(P.at(3, 3), 0.0f, kEps);
}

TEST(Mat4Test, PerspectiveNdc) {
    const Mat4 P = Mat4::perspective(kPi / 2.0f, 1.0f, 1.0f, 10.0f);
    // View space looks down -Z, so the point must be in front of the camera.
    // Point (1, 2, -3): clip = (1, 2, 33/9 - 20/9 = 13/9, w = 3)
    // ndc = clip / w = (1/3, 2/3, 13/27)
    const Vec3 ndc = P.transformPoint((Vec3{1, 2, -3}));
    EXPECT_NEAR(ndc.x, 1.0f / 3.0f, 1e-3f);
    EXPECT_NEAR(ndc.y, 2.0f / 3.0f, 1e-3f);
    EXPECT_NEAR(ndc.z, 13.0f / 27.0f, 1e-3f);
    // The near plane (z = -1) must map to ndc z = -1.
    EXPECT_NEAR(P.transformPoint((Vec3{0, 0, -1})).z, -1.0f, 1e-3f);
    // The far plane (z = -10) must map to ndc z = +1.
    EXPECT_NEAR(P.transformPoint((Vec3{0, 0, -10})).z, 1.0f, 1e-3f);
}

// ---------------------------------------------------------------------------
// lookAt
// ---------------------------------------------------------------------------

TEST(Mat4Test, LookAtAlongZ) {
    const Mat4 V = Mat4::lookAt((Vec3{0, 0, 5}), (Vec3{0, 0, 0}), (Vec3{0, 1, 0}));
    // World origin ends up 5 units in front of the camera (camera -Z).
    EXPECT_TRUE(vecNear(V.transformPoint(Vec3::zero()), (Vec3{0, 0, -5})));
    // The eye position maps to the camera origin.
    EXPECT_TRUE(vecNear(V.transformPoint((Vec3{0, 0, 5})), Vec3::zero()));
    // Camera up is world +Y.
    EXPECT_TRUE(vecNear(V.transformDirection((Vec3{0, 1, 0})),
                        (Vec3{0, 1, 0})));
}

TEST(Mat4Test, LookAtArbitrary) {
    const Vec3 eye {3, 4, 0};
    const Mat4 V = Mat4::lookAt(eye, Vec3::zero(), (Vec3{0, 1, 0}));
    EXPECT_TRUE(vecNear(V.transformPoint(Vec3::zero()), (Vec3{0, 0, -5})));
    EXPECT_TRUE(vecNear(V.transformPoint(eye), Vec3::zero()));
}

TEST(Mat4Test, ViewMatrixInvertsCameraPlacement) {
    // A camera placed by T*R must bring its own origin to (0,0,0).
    const Mat4 cam =
        Mat4::multiply(Mat4::translate(2, 5, -3), Mat4::rotateY(0.7f));
    const Mat4 view = cam.inverted();
    EXPECT_TRUE(vecNear(view.transformPoint((Vec3{2, 5, -3})), Vec3::zero()));
}

// ---------------------------------------------------------------------------
// Inverse / transpose
// ---------------------------------------------------------------------------

TEST(Mat4Test, InverseTranslate) {
    const Mat4 T = Mat4::translate(1, 2, 3);
    const Mat4 Tinv = T.inverted();
    EXPECT_TRUE(vecNear(Tinv.transformPoint((Vec3{1, 2, 3})), Vec3::zero()));
    const Mat4 I = Mat4::multiply(T, Tinv);
    EXPECT_TRUE(vecNear(I.transformPoint((Vec3{9, 1, 4})), (Vec3{9, 1, 4})));
}

TEST(Mat4Test, InverseRotation) {
    const Mat4 R = Mat4::rotateY(kPi / 2.0f);
    const Mat4 Rinv = R.inverted();
    // R maps (1,0,0) -> (0,0,-1); the inverse must map it back.
    EXPECT_TRUE(vecNear(Rinv.transformDirection((Vec3{0, 0, -1})),
                        (Vec3{1, 0, 0})));
    const Mat4 I = Mat4::multiply(R, Rinv);
    EXPECT_TRUE(vecNear(I.transformPoint((Vec3{0.3f, -1.2f, 7.7f})),
                        (Vec3{0.3f, -1.2f, 7.7f})));
}

TEST(Mat4Test, Transpose) {
    Mat4 M = Mat4::identity();
    M.set(0, 1, 5.0f);
    M.set(3, 2, -2.0f);
    const Mat4 T = M.transposed();
    EXPECT_EQ(T.at(1, 0), 5.0f);
    EXPECT_EQ(T.at(2, 3), -2.0f);
    EXPECT_EQ(T.at(0, 1), 0.0f);
}

// ---------------------------------------------------------------------------
// Vec4
// ---------------------------------------------------------------------------

TEST(Vec4Test, FromVec3) {
    const Vec4 v = Vec4::fromVec3((Vec3{1, 2, 3}), 2.0f);
    EXPECT_EQ(v.w, 2.0f);
    EXPECT_EQ(v.toVec3(), (Vec3{1, 2, 3}));
}
