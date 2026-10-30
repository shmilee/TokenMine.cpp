// Mob limb swing (ENT-1): the pure (no GL / no SDL / no runtime libm)
// math of the per-instance leg/arm animation.
//
// Determinism (hard rule 8): the whole pipeline is integer / fixed-point
// and frame-locked, mirroring the project's animation precedent (the day
// cycle's m_dayCycle, the growth engine's m_states.crops):
//   - the MOB's state is ONE integer, the swing phase (0..N-1), advanced
//     per frame by MobEntity::update (MobEntity holds nothing else of
//     the animation — the render isolation rule);
//   - the phase -> angle mapping is a COMPILE-TIME fixed-point sine table
//     (a constexpr Taylor; std::sin is deliberately NOT used at runtime —
//     it is not bit-stable across platforms/libcs);
//   - the limb angle -> vertex rotation is a pure function of
//     (base vertices, phase) — the same mob at the same frame always
//     yields the same bytes.
//
// Math (all 16.16 fixed point unless noted):
//   - table:  kSwingSinTable[i] = round(2^16 * sin(2*pi*i / N)), N = 64,
//     generated at compile time by a 14-term Taylor (bound in the
//     generator's comment). cos of the phase = the table shifted by N/4
//     (sin(x + pi/2) — not needed separately: the limb-angle series
//     below yields both).
//   - angle:  theta = amplitude * kSwingSinTable[effPhase], where
//     amplitude is the per-part DATA (radians, (0, 1]) quantized to
//     16.16 at model build and effPhase is the part's effective phase:
//     group 0 = phase, group 1 = (phase + N/2) % N (the half-cycle
//     offset = the diagonal gait: chicken's opposite legs, quadruped
//     diagonal leg pairs, spider even/odd leg rows).
//   - rotate: a swing part's vertices (and normals) rotate about its
//     PIVOT on the mob-local X axis (the local frame BEFORE the facing
//     yaw — the model matrix applies the yaw afterwards):
//         y' = pivot.y + (y - pivot.y) cos t - (z - pivot.z) sin t
//         z' = pivot.z + (y - pivot.y) sin t + (z - pivot.z) cos t
//     sin t / cos t come from the integer Taylor swingSinCosFixed (t in
//     16.16, |t| <= 1 rad = the data amplitude range). Positive t swings
//     a hanging limb toward -Z (the mob's front: the -Z face is the
//     front, see mob_mesh.hpp).
//   - pivot: the TOP-CENTER of the part's box (a leg's hip / an arm's
//     shoulder — the box hangs from this point; the pivot lies inside
//     the body box for every shipped part, so the joint is never visible
//     at rest or mid-swing).
//
// The phase == 0 frame is the identity pose for every part (the table's
// 0 and N/2 entries are exactly 0), so a resting mob is byte-identical
// to the static per-type mesh — the fast path (structural immunity) in
// Game::drawMobs takes mobs that are not swinging through the existing
// m_mobMeshes cache untouched.
#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "core/math/vec3.hpp"
#include "entity/mob_registry.hpp"
#include "renderer/mesh.hpp"
#include "renderer/texture_atlas.hpp"

namespace mc {

// The swing cycle: N phase slots, one 16.16 sine quantum each.
constexpr int kSwingPhaseCount = 64;
constexpr int kSwingHalf = kSwingPhaseCount / 2; // half-cycle offset
// 16.16 fixed-point scale (2^16).
constexpr int kSwingFixedScale = 1 << 16;
// Frames per phase slot while moving: 64/2 = 32 frames = 0.533 s per
// full swing cycle (the calibration target ~0.4-0.7 s).
constexpr int kSwingStep = 2;
// Phase decay toward the rest pose (0) while standing: 63/4 -> 16 frames
// from a full phase = a smooth step-off (~15-20 frames).
constexpr int kSwingDecay = 4;
// Vertices of one box (the shared six-face table: 6 faces x 4 corners —
// see mob_mesh.cpp / cube_face_table.hpp).
constexpr int kBoxVertexCount = 24;

// One frame of the swing phase (the pure, testable advance):
// moving (strict frame-boundary horizontal displacement) steps forward
// kSwingStep (wrapping at N); standing decays kSwingDecay toward 0 (the
// identity pose) and clamps there.
constexpr int advanceSwingPhase(int phase, bool moving) {
    if (moving) {
        return (phase + kSwingStep) % kSwingPhaseCount;
    }
    const int decayed = phase - kSwingDecay;
    return decayed > 0 ? decayed : 0;
}

// The render-side swing gate (the fast path = structural immunity):
// config off, a type without swing data, or a resting phase (0 = the
// identity pose) all draw through the STATIC per-type mesh (zero new
// code-path activity).
constexpr bool limbSwingActive(bool configEnabled, bool modelHasSwings,
                               int phase) {
    return configEnabled && modelHasSwings && phase != 0;
}

// The swing pivot of a part: the TOP-CENTER of its box (a leg's hip / an
// arm's shoulder — the box hangs from this point; the pivot lies inside
// the body box for every shipped part, so the joint is never visible at
// rest or mid-swing).
constexpr Vec3 swingPivot(const MobBox& box) {
    return Vec3 {box.center.x,
                 box.center.y + box.size.y * 0.5f,
                 box.center.z};
}

// 16.16 fixed-point multiply, round-to-nearest (ties away from zero).
// |a * b| <= 2^32, so the 64-bit product is exact.
constexpr std::int32_t fmul(std::int32_t a, std::int32_t b) {
    const std::int64_t p =
        static_cast<std::int64_t>(a) * static_cast<std::int64_t>(b);
    const std::int64_t r = (p >= 0) ? (p + 32768) : (p - 32768);
    return static_cast<std::int32_t>(r >> 16);
}

// The compile-time 16.16 sine table (see the file head for the formula).
//
// Generator: the 14-term Taylor sin(x) = sum_k (-1)^k x^(2k+1)/(2k+1)!,
// evaluated in COMPILE-TIME double (C++20 allows float-point in constant
// expressions; no runtime libm). Remainder bound: the first omitted term
// is x^29/29!; at x = 2*pi*63/64 <= 6.182, ln(x^29/29!) =
// 29*ln(6.182) - ln(29!) <= 52.83 - 71.33 < -18.5, i.e. <= 9.2e-9 <<
// half a 2^-16 quantum (7.6e-6), so the round() below is exact for every
// entry. The twoPi literal is the nearest double to 2*pi
// (6.283185307179586...).
namespace {

constexpr std::array<std::int32_t, kSwingPhaseCount> makeSwingSinTable() {
    std::array<std::int32_t, kSwingPhaseCount> table {};
    const double twoPi = 6.283185307179586;
    for (int i = 0; i < kSwingPhaseCount; ++i) {
        const double x = twoPi * i / static_cast<double>(kSwingPhaseCount);
        double term = x; // x^1/1!
        double sum = 0.0;
        for (int k = 0; k < 14; ++k) { // up to x^27/27!
            sum += term;
            term = -term * x * x / ((2 * k + 2) * (2 * k + 3));
        }
        // Round sum*2^16 to the nearest integer (ties away from zero).
        const double q = sum * 65536.0;
        const double a = (q < 0.0) ? -q : q;
        const std::int64_t r = static_cast<std::int64_t>(a + 0.5);
        table[i] = static_cast<std::int32_t>((q < 0.0) ? -r : r);
    }
    return table;
}

} // namespace

// (constexpr = const = internal linkage per translation unit — no ODR
// emission, and every use folds to the same compile-time constant.)
constexpr std::array<std::int32_t, kSwingPhaseCount> kSwingSinTable =
    [] {
        return makeSwingSinTable();
    }();

// sin/cos of a 16.16 fixed-point angle t (|t| <= 65536 = 1 rad = the data
// amplitude range) via the INTEGER Taylor (the limb angle's rotation
// terms — no runtime libm; every op is an exact integer multiply/shift or
// a truncating division, bit-stable everywhere):
//   sin t = t - t^3/6 + t^5/120 - t^7/5040 + t^9/362880 - t^11/39916800
//   cos t = 1 - t^2/2 + t^4/24 - t^6/720 + t^8/40320
// (16.16 throughout; the error is <= 2 ulp for |t| <= 1 rad — the terms
// beyond the last are < 1 ulp of 2^16.)
constexpr void swingSinCosFixed(std::int32_t t, std::int32_t& outSin,
                                std::int32_t& outCos) {
    const std::int32_t t2 = fmul(t, t);
    const std::int32_t t3 = fmul(t2, t);
    const std::int32_t t4 = fmul(t2, t2);
    const std::int32_t t5 = fmul(t4, t);
    const std::int32_t t6 = fmul(t4, t2);
    const std::int32_t t7 = fmul(t6, t);
    const std::int32_t t8 = fmul(t4, t4);
    const std::int32_t t9 = fmul(t8, t);
    const std::int32_t t11 = fmul(t9, t2);
    outSin =
        t - t3 / 6 + t5 / 120 - t7 / 5040 + t9 / 362880 - t11 / 39916800;
    outCos = kSwingFixedScale - t2 / 2 + t4 / 24 - t6 / 720 + t8 / 40320;
}

// Radians (the data amplitude) -> 16.16 fixed-point, round-to-nearest
// (the power-of-two multiply is exact; no libm).
constexpr std::int32_t swingFixedFromRadian(float radian) {
    const double q = static_cast<double>(radian) * 65536.0;
    const double a = (q < 0.0) ? -q : q;
    const std::int64_t r = static_cast<std::int64_t>(a + 0.5);
    return static_cast<std::int32_t>((q < 0.0) ? -r : r);
}

// Rotate a POINT about `pivot` on the local X axis by the (s, c) of the
// part's current limb angle (16.16 fixed-point passed as EXACT float —
// int32 / 2^16 is a power-of-two division, exact in binary32). The
// zero-angle (s = 0, c = 1) short-circuits to a BIT-EXACT identity (the
// phase-0 frame must equal the static mesh byte for byte).
inline Vec3 swingRotateVertex(Vec3 v, Vec3 pivot, float s, float c) {
    if (s == 0.0f && c == 1.0f) {
        return v;
    }
    const float dy = v.y - pivot.y;
    const float dz = v.z - pivot.z;
    return Vec3 {v.x,
                 pivot.y + dy * c - dz * s,
                 pivot.z + dy * s + dz * c};
}

// Rotate a DIRECTION (a normal) about the X axis by the same (s, c): the
// rigid part rotation applies to the face normals too, so the shader's
// per-face shade buckets (basic.vert) follow the limb. Bit-exact identity
// short-circuit, same as swingRotateVertex.
inline Vec3 swingRotateNormal(Vec3 n, float s, float c) {
    if (s == 0.0f && c == 1.0f) {
        return n;
    }
    return Vec3 {n.x, n.y * c - n.z * s, n.y * s + n.z * c};
}

// One swing part of a built SwingModel (per mob type): the box's vertex
// range in the base mesh, its pivot, its data amplitude (16.16) and its
// precomputed per-effective-phase (sin, cos) float pairs (the exact
// 16.16 ratios — int32/2^16 is exact in binary32).
struct SwingPart {
    int boxIndex = 0;
    int vertexStart = 0; // boxIndex * kBoxVertexCount
    Vec3 pivot = Vec3::zero();
    int amplitudeFixed = 0; // 16.16 radians (the data amplitude)
    int group = 0;          // 0 or 1 (the half-cycle offset)
    float sinCos[2 * kSwingPhaseCount]; // per eff phase: [sin, cos]
};

// The per-type swing geometry: the CPU copy of the STATIC box mesh (the
// base every frame starts from — identical to the m_mobMeshes upload) +
// the swing parts (empty = a type that never swings: zombie/skeleton
// have no limb parts, the 99.7 geometry is head + torso only).
struct SwingModel {
    std::vector<MeshVertex> base;
    std::vector<std::uint32_t> indices;
    std::vector<SwingPart> parts;

    bool active() const { return !parts.empty(); }
};

// Build the per-type swing geometry from the parsed model (the game
// does this lazily on a swinging type's first render frame, with the
// atlas layout for the UVs). Pure: the same model always yields the same
// bytes. A model without swings yields the base mesh + no parts.
SwingModel buildSwingModel(const MobModel& model, const AtlasLayout& layout);

// Build ONE frame's full vertex buffer for a mob at `phase`: the base
// mesh with every swing part's kBoxVertexCount vertices (positions AND
// normals) rotated about its pivot by the part's (s, c) at its
// effective phase. Pure (same inputs = same bytes); `out` is resized to
// the base's count. phase 0 short-circuits to a plain base copy (the
// identity pose — the fast path draws the static mesh instead, but this
// keeps the function total).
void buildSwingFrame(const SwingModel& model, int phase,
                     std::vector<MeshVertex>& out);

} // namespace mc
