#include "entity/mob_swing_clip.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

namespace mc {
namespace {

// One point of the YZ plane (a = y, b = z).
struct Pt {
    float a = 0.0f;
    float b = 0.0f;
};

// Rotate a YZ point about the pivot on the X axis by (s, c) — the SAME
// formula as swingRotateVertex (its y/z components only, the rotation
// leaves x alone).
Pt rotateYz(const Pt& p, const Pt& pivot, float s, float c) {
    const float dy = p.a - pivot.a;
    const float dz = p.b - pivot.b;
    return Pt {pivot.a + dy * c - dz * s, pivot.b + dy * s + dz * c};
}

float absf(float v) { return v < 0.0f ? -v : v; }

// Sutherland–Hodgman: clip the convex polygon against one half-plane of
// the body rectangle (axis 0 = y, 1 = z; low keeps v >= limit, high
// keeps v <= limit). Fixed op order per edge (deterministic). The
// intersection weight divides (limit - vp) by (vc - vp): the endpoints
// straddle the plane, so the limit lies strictly between vp and vc and
// the denominator is never 0 (both equal would mean both on the same
// side, handled by the inside/outside branches).
void clipHalfPlane(std::vector<Pt>& poly, int axis, bool low, float limit) {
    if (poly.empty()) {
        return;
    }
    std::vector<Pt> out;
    out.reserve(poly.size() + 1);
    auto coord = [axis](const Pt& p) { return (axis == 0) ? p.a : p.b; };
    auto inside = [coord, low, limit](const Pt& p) {
        return low ? coord(p) >= limit : coord(p) <= limit;
    };
    for (std::size_t i = 0; i < poly.size(); ++i) {
        const Pt& cur = poly[i];
        const Pt& prev = poly[(i + poly.size() - 1) % poly.size()];
        const bool cin = inside(cur);
        const bool pin = inside(prev);
        if (cin) {
            if (!pin) {
                const float t =
                    (limit - coord(prev)) / (coord(cur) - coord(prev));
                out.push_back(Pt {prev.a + t * (cur.a - prev.a),
                                   prev.b + t * (cur.b - prev.b)});
            }
            out.push_back(cur);
        } else if (pin) {
            const float t =
                (limit - coord(prev)) / (coord(cur) - coord(prev));
            out.push_back(Pt {prev.a + t * (cur.a - prev.a),
                               prev.b + t * (cur.b - prev.b)});
        }
    }
    poly = std::move(out);
}

// Clip against the body's four YZ half-planes (the x interval is
// handled by the caller as a length factor — the X-axis rotation leaves
// every box's x interval invariant).
void clipToBodyYz(std::vector<Pt>& poly, const MobBox& body) {
    clipHalfPlane(poly, 0, true, body.center.y - body.size.y * 0.5f);
    clipHalfPlane(poly, 0, false, body.center.y + body.size.y * 0.5f);
    clipHalfPlane(poly, 1, true, body.center.z - body.size.z * 0.5f);
    clipHalfPlane(poly, 1, false, body.center.z + body.size.z * 0.5f);
}

// Shoelace area of a (convex) YZ polygon; 0 for empty/degenerate.
float polygonAreaYz(const std::vector<Pt>& poly) {
    if (poly.size() < 3) {
        return 0.0f;
    }
    float twice = 0.0f;
    for (std::size_t i = 0; i < poly.size(); ++i) {
        const Pt& p = poly[i];
        const Pt& q = poly[(i + 1) % poly.size()];
        twice += p.a * q.b - q.a * p.b;
    }
    return absf(twice) * 0.5f;
}

// The rotated YZ rectangle of the leg region [u0, u1] (u measured DOWN
// from the pivot along the leg axis) x [-halfWz, +halfWz] (v = the leg's
// z offset about its center): the four region corners, rotated about the
// pivot by the renderer's (s, c). Counter-clockwise order preserved
// (the clip + shoelace do not care, but the fixed order keeps the op
// sequence deterministic).
std::vector<Pt> rotatedLegRegion(const MobBox& leg, const Pt& pivot,
                                 float u0, float u1, float s, float c) {
    const float halfWz = leg.size.z * 0.5f;
    const Pt local[4] = {
        {pivot.a - u0, pivot.b - halfWz},
        {pivot.a - u0, pivot.b + halfWz},
        {pivot.a - u1, pivot.b + halfWz},
        {pivot.a - u1, pivot.b - halfWz},
    };
    std::vector<Pt> out;
    out.reserve(4);
    for (const Pt& p : local) {
        out.push_back(rotateYz(p, pivot, s, c));
    }
    return out;
}

} // namespace

SwingClipResult checkSwingClip(const MobBox& leg, const MobBox& body,
                               float s, float c) {
    SwingClipResult r;
    const Pt pivot {leg.center.y + leg.size.y * 0.5f, leg.center.z};
    const float len = leg.size.y;
    const float legVolume = leg.size.x * leg.size.y * leg.size.z;

    // The full rotated leg cross-section + its bottom two-thirds (the
    // forbidden zone starts len/3 down from the pivot).
    const std::vector<Pt> full = rotatedLegRegion(leg, pivot, 0.0f, len, s, c);
    const std::vector<Pt> bottom =
        rotatedLegRegion(leg, pivot, len / 3.0f, len, s, c);

    // Rule 1, ground: the tip (bottom-center point, u = len, v = 0) and
    // the lowest rotated corner of the full foot.
    r.tipCenterY = rotateYz({pivot.a - len, pivot.b}, pivot, s, c).a;
    r.cornerMinY = full[0].a;
    for (std::size_t i = 1; i < full.size(); ++i) {
        if (full[i].a < r.cornerMinY) {
            r.cornerMinY = full[i].a;
        }
    }
    r.groundOk = r.tipCenterY >= -kClipGroundSliver &&
                 r.cornerMinY >= -kClipGroundSliver;

    // Rule 2, body: the x-interval overlap length (>= 0) times the
    // rotated-rect ∩ body-rect YZ areas (convex clips + shoelace).
    const float legX0 = leg.center.x - leg.size.x * 0.5f;
    const float legX1 = leg.center.x + leg.size.x * 0.5f;
    const float bodyX0 = body.center.x - body.size.x * 0.5f;
    const float bodyX1 = body.center.x + body.size.x * 0.5f;
    const float xOverlap =
        (legX1 > bodyX0 && bodyX1 > legX0) ? (std::min(legX1, bodyX1) -
                                              std::max(legX0, bodyX0))
                                           : 0.0f;

    std::vector<Pt> clipFull = full;
    clipToBodyYz(clipFull, body);
    r.overlapVolume = polygonAreaYz(clipFull) * xOverlap;

    std::vector<Pt> clipBottom = bottom;
    clipToBodyYz(clipBottom, body);
    r.forbiddenVolume = polygonAreaYz(clipBottom) * xOverlap;

    r.bodyOk = r.overlapVolume <= kClipJointOverlapFraction * legVolume &&
               r.forbiddenVolume <= kClipForbiddenSliver;
    return r;
}

} // namespace mc
