// Swing clip predicate (ENT-1b): the pure function that pins the
// "joint out of the body" contract of the shipped mob limb-swing
// geometry, so that ANY future JSON edit that re-buries a pivot or
// re-inflates an amplitude turns the test suite red (the data and the
// predicate are mutually pinned, the content-pin discipline).
//
// The predicate re-derives exactly what the renderer does per frame
// (buildSwingFrame): the swing part's box is rotated about its pivot
// (swingPivot = the box top-center) on the local X axis by the part's
// (sin, cos) — the same 16.16-derived floats the mesh upload uses —
// and is then checked against:
//
//   1. THE GROUND (y = 0): the leg tip (its bottom-CENTER point, the
//      point directly below the pivot) stays on/above the floor, and the
//      lowest rotated FOOT CORNER dips no more than kClipGroundSliver
//      below it. The corner sliver is unavoidable kinematics, not
//      tolerance for clipping: a box foot that swings about its
//      top-center pivot ALWAYS has its trailing bottom corner dip below
//      the floor for any non-zero angle (at rest the foot plane is
//      y = 0; rotating by t lifts the pivot side by ~len*(1-cos t) =
//      second order but drags the trailing corner down by
//      halfWidth*sin t = first order). The shipped maximum is the
//      chicken foot (half-width 0.095) at 0.4 rad: a 0.0126 dip.
//      kClipGroundSliver = 0.02 (1.6x that margin) still rejects any
//      foot that is below the floor at rest (a pivot shorter than the
//      leg, dip >= 0.05) or any significantly dug-in foot.
//   2. THE BODY (an axis-aligned box): the rotated leg ∩ body volume is
//      <= kClipJointOverlapFraction (25%) of the leg's own volume AND
//      the overlap inside the leg's BOTTOM two-thirds (away from the
//      pivot) is <= kClipForbiddenSliver — the overlap the user saw
//      (the old cow leg's 0.3-deep buried pivot sweeping its mid-shank
//      through the torso side). With the joint at the body bottom, only
//      the pivot-end corners can kiss the body underside, and that
//      sliver is a few percent of the leg volume.
//
// Geometry reduction: the rotation is about the X axis, so each box's x
// interval is invariant and the leg's YZ cross-section (a rectangle
// about the pivot) rotates in the YZ plane. Intersection volume =
// (x-interval overlap length) x (rotated-rect ∩ axis-aligned-rect area
// in YZ). The area is a convex polygon clip (Sutherland–Hodgman against
// the four body half-planes) + shoelace — exact for these inputs, a
// fixed order of float ops (deterministic), no runtime libm (s/c are
// the renderer's exact 16.16 ratios passed through).
#pragma once

#include "entity/mob_swing.hpp"

namespace mc {

// The corner sliver of rule 1 (see the file head for the derivation):
// the max dip of the lowest rotated foot corner below the floor.
constexpr float kClipGroundSliver = 0.02f;
// Rule 2, the joint tolerance as a FRACTION of the leg's own volume:
// the rotated leg ∩ body must be <= 25% of the leg (a full leg buried
// to the mid-shank in the body — the old cow — is ~40% and fails).
constexpr float kClipJointOverlapFraction = 0.25f;
// Rule 2, the FORBIDDEN zone (the leg's bottom two-thirds, away from
// the pivot): any overlap volume above this absolute sliver is a body
// clip. Float noise from the polygon clip is << 1e-6 cubic units; a
// real interpenetration (a buried pivot) is >= 1e-3.
constexpr float kClipForbiddenSliver = 1e-6f;

// One predicate evaluation: every measured quantity + the pass/fail of
// each rule (the tests pin the values, not just the booleans).
struct SwingClipResult {
    float tipCenterY = 0.0f;    // the leg's bottom-center point after rotation
    float cornerMinY = 0.0f;    // the lowest of the four rotated foot corners
    float overlapVolume = 0.0f; // rotated leg ∩ body (the full leg)
    float forbiddenVolume = 0.0f; // rotated leg's bottom two-thirds ∩ body
    bool groundOk = true;        // rule 1 (both tip measures within slivers)
    bool bodyOk = true;          // rule 2 (both volume bounds hold)

    bool ok() const { return groundOk && bodyOk; }
};

// Check the `leg` (a swing part) rotated about its pivot by the
// renderer's exact (s, c) against the axis-aligned `body`. Pure and
// total: any boxes, any (s, c); a zero-angle call (s = 0, c = 1)
// measures the resting pose (the face-touching joint reads 0 overlap).
SwingClipResult checkSwingClip(const MobBox& leg, const MobBox& body,
                               float s, float c);

} // namespace mc
