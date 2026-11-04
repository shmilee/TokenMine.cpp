// V3-1 Phase A (v3.2.0): the nether portal frame — the pure function
// family the unit tests own (the campfire / bed pattern: the game is the
// thin adapter over these; the setBlock choke-point hook, the flint-and-
// steel ignition branch, the standing countdown and the load-time rescan
// live in the Game).
//
// Model (the Minecraft nether-portal semantics, the wiki-verified 11-04
// set — the design's scope decisions, the documented deviations noted
// per gate):
//   - the frame is a rectangular OBSIDIAN ring in a VERTICAL plane
//     (axis 0 = the long edge along X, the ring in the z = const plane;
//     axis 1 = the long edge along Z, the ring in the x = const plane —
//     horizontal rings do not exist, the End-portal distinction).
//   - the outer extent (the ring's bounding box): long edge in
//     [kPortalMinLongEdge, kPortalMaxLongEdge] = [4, 23], height in
//     [kPortalMinHeight, kPortalMaxHeight] = [5, 23] — the Minecraft
//     4×5 minimum / 23×23 maximum. The FOUR CORNERS ARE OPTIONAL (any
//     content — the player may save four obsidian; the game's own
//     generated portals always carry them).
//   - the INTERIOR (the strictly-internal cells) holds only air / fire
//     (the activation search) — a complete frame with a live fire in the
//     interior activates: the interior's air + fire cells become
//     nether_portal blocks in the frame's AXIS state (the fire cell is
//     replaced — Minecraft's "one of them replacing the fire").
//     DEVIATION (the design's ruling): the wiki's "the fire must be the
//     last placed block" quirk is NOT reproduced — both the obsidian and
//     the fire events check (a complete frame with interior fire is
//     activated regardless of placement order; the event-driven model
//     makes "last" meaningless, and the two-sided check is the stronger
//     determinism).
//   - deactivation: a NON-CORNER ring cell that stops being obsidian
//     (break / replace) deactivates the frame (interior portals -> air).
//     A CORNER cell may be removed WITHOUT destroying the portal
//     (Minecraft-exact). The portal is SELF-SUSTAINING: the interior
//     fire burning out (the real-dt countdown) or being put out never
//     deactivates (the fire is interior, not ring — the unit tests pin
//     it).
//   - a loaded portal block that does not sit strictly inside a complete
//     frame (a hand-edited save) is set to air at init (the Minecraft
//     update semantics + the PITS 383① two-way cross-check discipline).
//
// The scan is BOUNDED: from the event cell the ring's far edge is at
// most kPortalMaxLongEdge - 1 = 22 cells away on the long axis and
// kPortalMaxHeight - 1 = 22 on the height axis, so each vertical plane
// through the cell reads a 45×45 window (the design's "≤ 25×25 / 625
// cells" estimate covers only event cells within 12 of a large frame's
// far edge — the ±22 window is the arithmetic root; one event stays a
// bounded O(1) worst case, the sub-millisecond class). The prefix-sum
// grid makes every candidate frame an O(1) check.
//
// The dimension mapping (Phase B's input, computed here already — the
// pure function): overworld (x, y, z) -> nether (floorDiv(x, 8), y,
// floorDiv(z, 8)) (Minecraft's 8:1, Y unscaled; floorDiv = the
// negative-safe floor, the coordinates.hpp discipline).
#pragma once

#include <cstdint>
#include <numbers>
#include <optional>
#include <vector>

#include "block/block.hpp"
#include "block/block_registry.hpp"
#include "core/math/vec3.hpp"
#include "world/coordinates.hpp"

namespace mc {

class World;

// -- the frame size limits (Minecraft, the named-constant discipline) --
// The outer extent of the ring's bounding box: long edge (the horizontal
// axis of the ring's plane) and height (the y span). 4×5 is the minimum
// (Minecraft's smallest frame, corners included), 23×23 the maximum
// (13w37a onward).
inline constexpr int kPortalMinLongEdge = 4;
inline constexpr int kPortalMaxLongEdge = 23;
inline constexpr int kPortalMinHeight = 5;
inline constexpr int kPortalMaxHeight = 23;

// The 8:1 dimension mapping scale (Minecraft: overworld / 8 -> nether,
// Y unscaled; nether * 8 -> overworld is the exact inverse Phase B uses).
inline constexpr int kPortalMappingScale = 8;

// The player's standing countdown (the frame-locked class, hard rule 8):
// kPortalEntryFrames consecutive frames of the player's AABB overlapping
// a portal cell complete the entry (Minecraft's 80-tick survival delay
// is 4 s at 20 TPS; the project's 60 frames = 1 s sits in the wiki's
// 10-80 tick span and is the E2E-assertable frame-locked value — the
// design's ruling, the 240-frame alternative rejected).
inline constexpr int kPortalEntryFrames = 60;

// The Phase A travel gate: false = the countdown completion is the
// explicit NO-OP STUB (the "Nether ready at (x, y, z)" info log, zero
// displacement, the countdown cleared — re-entry re-counts, no cooldown).
// Phase B flips it to true and runs the dimension switch on the mapped
// coordinate (the static_assert in updatePortalEntry pins the Phase A
// stub: a flip without the pipeline is a build error, not a TODO).
// V3-1 Phase B (v3.4.0): FLIPPED — the travel pipeline (Game::
// travelPortal) consumes the completion; the static_assert now pins the
// OPPOSITE (a flip back to the Phase A stub without removing the
// pipeline is a build error).
inline constexpr bool kPortalTravelEnabled = true;

// V3-1 Phase B: the destination-portal search radius in the target
// dimension (Minecraft's 1.16.2 value: the nether search is 16 cells —
// the overworld's 128 collapsed by the 8:1 scale; the 32x32 full-height
// box). ±16 on X/Z (inclusive), any Y.
inline constexpr int kPortalSearchRadius = 16;

// V3-1 Phase B: the landing hitbox probe's upward reach (blocks): when
// the destination frame's bottom center has no room for the player's
// AABB, the probe steps UP y+1..y+3 and takes the first roomy level (the
// design's D6 step 2 "失败 = 上探 y+1..+3").
inline constexpr int kPortalLandingProbeUp = 3;

// V3-1 Phase B: the post-travel portal cooldown, in FRAMES (the
// frame-locked class — the Minecraft 15-second entity portal cooldown at
// the project's 60 frames/s: 900). While armed, the standing countdown
// never re-arms, so a player left INSIDE the destination portal (the
// travel lands at its bottom center) is not ping-ponged back on the next
// kPortalEntryFrames (Minecraft's "the portal does not re-trigger for
// 15 s" rule; the Phase A no-cooldown pin is superseded by Phase B — the
// destination-inside-portal geometry makes the cooldown mandatory).
inline constexpr int kPortalCooldownFrames = 900;

// V3-1 Phase B: the orthogonal-portal yaw turn (radians). Minecraft JE:
// the target portal perpendicular to the source rotates the entity's yaw
// (and velocity) 90° CLOCKWISE (viewed from above). The project's yaw
// convention (hard rule 5: 0 = -Z, positive = LEFT turn = counter-
// clockwise from above), so the clockwise turn is the NEGATIVE pi/2.
// Same-axis travel keeps the source yaw (the design's ruling — the wiki's
// double-rotation round-trip quirk rides on this: two orthogonal legs =
// a 180° total turn at unchanged coordinates, the known look quirk).
inline constexpr float kPortalOrthoYawTurn =
    -static_cast<float>(std::numbers::pi) / 2.0f;

// The portal ambience (the visible-portal per-frame roll, the bat
// squeak's cadence pattern): the expected interval in seconds (the
// per-frame probability is 1 / (interval × 60)) + the visibility radius
// (blocks, the player's feet to the portal cell's center — beyond it the
// portal is not "visible") + the dedicated salt (the kAmbienceSalt
// discipline: the stream never tracks any other deterministic roll).
inline constexpr double kPortalAmbientIntervalSeconds = 2.0;
inline constexpr float kPortalAmbientRadiusBlocks = 32.0f;
inline constexpr std::uint64_t kPortalAmbientSalt = 0x3D1A0C55ULL;

// One rectangular portal frame: the obsidian ring's outer extent (the
// bounding box, closed intervals) in a vertical plane.
//   axis  = 0: the long edge runs along X (the ring is in the plane
//             z = plane) — the portal blocks' state 0;
//   axis  = 1: the long edge runs along Z (the ring is in the plane
//             x = plane) — the portal blocks' state 1.
// lo/hi   = the long-edge coordinate range (x for axis 0, z for axis 1);
// y0/y1   = the height range; plane = the plane's fixed coordinate.
struct PortalFrame {
    int axis = 0;
    int lo = 0;
    int hi = 0;
    int y0 = 0;
    int y1 = 0;
    int plane = 0;

    int longEdge() const { return hi - lo + 1; }
    int height() const { return y1 - y0 + 1; }
    // The bounding box's area (the "largest frame" selection's measure).
    int area() const { return longEdge() * height(); }

    bool operator==(const PortalFrame& o) const {
        return axis == o.axis && lo == o.lo && hi == o.hi && y0 == o.y0 &&
               y1 == o.y1 && plane == o.plane;
    }
    // The lexicographic order (V3-1 Phase B: the travel destination's
    // distinct-frame dedupe + the nearest-frame tie-break — the
    // (axis, lo, y0, plane) order y1-irrelevant: y1 = y0 + height - 1 is
    // determined by the size, the tie-break's documented key).
    bool operator<(const PortalFrame& o) const {
        if (axis != o.axis) {
            return axis < o.axis;
        }
        if (lo != o.lo) {
            return lo < o.lo;
        }
        if (y0 != o.y0) {
            return y0 < o.y0;
        }
        return plane < o.plane;
    }
};

// The frame's size rule (Minecraft's 4×5..23×23 outer extent): the long
// edge in [4, 23] AND the height in [5, 23] (a 4×4 ring — both edges at
// the long-edge minimum — is NOT a frame: the height needs 5).
bool portalFrameSizeValid(const PortalFrame& frame);

// Maps a long-edge coordinate `u` (in [lo, hi]) + a height `y` (in
// [y0, y1]) to the frame plane's world cell.
BlockPos portalFrameCell(const PortalFrame& frame, int u, int y);

// True when `cell` is one of the ring's FOUR CORNERS (the optional
// ones: any content, including air, keeps the frame valid).
bool portalFrameIsCorner(const PortalFrame& frame, const BlockPos& cell);

// True when `cell` lies on the ring's PERIMETER (the bounding box's
// boundary — the four corners included; a cell strictly inside the
// interior does not qualify).
bool portalFrameOnRing(const PortalFrame& frame, const BlockPos& cell);

// True when `cell` is STRICTLY INSIDE the frame (the interior: the long-
// edge coordinate in (lo, hi), the height in (y0, y1), on the plane).
// The interior is the air->portal conversion domain (the activation plan)
// and the portal's own living space (a loaded portal cell must sit here
// to survive the rescan).
bool portalFrameStrictlyInterior(const PortalFrame& frame,
                                 const BlockPos& cell);

// The ring's perimeter cells (the four corners INCLUDED), deterministic
// order (the top row left->right, then the side columns top->bottom,
// then the bottom row left->right — one pass, no duplicates).
std::vector<BlockPos> portalFrameRing(const PortalFrame& frame);

// The frame's interior cells (the air->portal conversion domain),
// deterministic order (row-major: the long edge outermost, y innermost).
std::vector<BlockPos> portalFrameInterior(const PortalFrame& frame);

// The frame's BOTTOM CENTER (Phase B's landing anchor + the destination
// distance's representative point): the interior's bottom row (y = y0 + 1)
// centered on the long edge (the cell centers' mean, float-exact), on the
// frame's plane (plane + 0.5 on the fixed axis). The player's feet rest on
// the frame's bottom row's top (y0 + 1); the body (height <= 1.8) fits the
// 3-tall interior of a legal (height >= 5) frame. Pure.
Vec3 portalFrameBottomCenter(const PortalFrame& frame);

// The 8:1 overworld -> nether mapping (Minecraft, the wiki-verified set):
// the coordinates floor-divided by kPortalMappingScale (floorDiv = the
// negative-safe floor: -7 -> -1, 7 -> 0), Y unscaled. Pure; Phase A logs
// it, Phase B consumes it as the travel destination.
BlockPos overworldToNether(int x, int y, int z);

// The 8:1 nether -> overworld mapping (Phase B's return leg — the exact
// inverse shape of overworldToNether: the nether coordinates multiplied
// by the scale, Y unscaled; no floor — the integer product is exact, the
// negative values scale symmetrically (floorDiv(x, 8) * 8 <= x < ... and
// the nether -> overworld mapping is the wiki's (X*8, Y, Z*8), which
// lands in the source overworld cell's 8x8 block for every x). Pure.
BlockPos netherToOverworld(int x, int y, int z);

// -- the travel destination (Phase B, the design's D6 step 2/3) --------

// The pure travel-destination decision (the unit tests own it — the
// campfire / portalEntryStep pattern): given the player's 8:1 mapped
// cell in the TARGET dimension, the source frame's axis + yaw, and the
// target dimension's tracked active portal cells, decide WHERE the player
// lands and which way they face:
//   1. SEARCH: the distinct VALID frames (portalValidFrame — a cell whose
//      ring is broken is an orphan, never a destination) holding a portal
//      cell within kPortalSearchRadius of the mapped cell on X/Z (any Y —
//      the full-height box). Nearest = the frame's bottom-center's
//      Euclidean distance to the mapped cell (Y included — the wiki rule),
//      ties by (axis, lo, y0, plane) ascending.
//   2. GENERATE fallback: no qualifying frame -> the 4x5 corners-included
//      frame anchored on the mapped cell (the long edge along the SOURCE
//      frame's axis — the wiki "the long axis aligns with the source
//      portal"; the bottom row at mapped.y - 1, Y-clamped into the world,
//      so a mapped y of 0 still yields a legal frame), the plane through
//      the mapped cell. The plan's `generated` marks it (the game writes
//      the ring + the 2x3 bedrock platform + the interior through the
//      setBlock choke point — the editedColumns / journal protection
//      rides along, the activation semantics hold by construction).
//   3. LANDING: the destination frame's interior BOTTOM ROW center (the
//      feet on the frame's bottom row — the standing pose the countdown
//      itself used). Hitbox safety (Player::hasRoomAt with the body's
//      width/height): when the bottom center has no room, probe UP
//      y+1..y+3 and take the first roomy level (the design's D6; the
//      portal interior is non-solid, so a failure is the adjacent
//      geometry intruding — rare, deterministic).
//   4. YAW: the source yaw is kept (the design's ruling) — except when
//      the destination frame's axis is ORTHOGONAL to the source axis:
//      the Minecraft JE 90° clockwise turn (kPortalOrthoYawTurn).
// `portalCells` is the target dimension's tracked portal cells (the
// game's per-dimension tracking set, position-sorted — the set's
// deterministic order; the distinct-frame dedupe below is order-
// independent by the frame == frame equality).
struct PortalTravelQuery {
    BlockPos mapped; // the 8:1 mapped cell in the target dimension
    int sourceAxis = 0; // the source frame's axis (0 = x-long, 1 = z-long)
    float sourceYaw = 0.0f; // the player's yaw before the switch
    std::vector<BlockPos> portalCells; // the target's active portal cells
    const World& targetWorld;
    const BlockRegistry& blocks;
    BlockID obsidianId = BlockID::invalid();
    BlockID fireId = BlockID::invalid();
    BlockID portalId = BlockID::invalid();
    // The landing body's AABB (the player's; the probe's dimensions).
    float width = 0.6f;
    float height = 1.8f;
};
struct PortalTravelPlan {
    PortalFrame frame; // the destination frame (found or generated)
    bool generated = false; // true when the fallback anchored one
    Vec3 destination; // the feet position (the bottom center, probed)
    float yaw = 0.0f; // the post-travel yaw (kept or ortho-turned)
};
PortalTravelPlan portalTravelDestination(const PortalTravelQuery& q);

// The generated fallback's 2x3 BEDROCK platform cells (pure — the game
// writes them through the setBlock choke point): the 2 cells along the
// frame's long edge (the interior's two long-edge cells) x 3 cells across
// the frame's plane (plane - 1 .. plane + 1), one BELOW the frame's
// bottom row (y0 - 1; the wiki's forced-generation platform, the design's
// bedrock ruling). Empty when y0 - 1 < 0 (the frame bottomed on the world
// floor — the platform has nowhere to go). Deterministic order (long edge
// outer, plane inner).
std::vector<BlockPos> portalPlatformPlan(const PortalFrame& frame);

// The portal ambience roll (pure, frame-locked — the fire-particle-roll
// pattern: the state advances with the frame by the golden step before
// the splitmix64 finalizer (PITS #4: a pure finalizer on a constant input
// yields a constant stream), the per-cell hash3 decorrelation, the
// dedicated salt). True = the portal at `cell` emits its ambient sound on
// `frame`; over a fixed (seed, cell) stream the pass probability is
// 1 / (kPortalAmbientIntervalSeconds × 60) per frame.
bool portalAmbientRoll(std::uint64_t seed, std::uint64_t frame,
                       const BlockPos& cell);

// One scanned candidate frame + its ring / interior census (the bounded
// scan's output). The interior census is over the STRICTLY-interior cells
// (air / fire / portal — everything else disqualifies the candidate at
// scan time).
struct PortalFrameStatus {
    PortalFrame frame;
    // The CURRENT world: every non-corner perimeter cell is obsidian
    // (the corners are unchecked — any content).
    bool ringComplete = false;
    // The PRE-EVENT view: the same, but the event cell counted as
    // obsidian when it sits on the perimeter (a destroyed ring cell read
    // back the way the frame was just before the destruction — the
    // deactivation's predicate).
    bool ringCompletePreEvent = false;
    // The event cell's position relative to this frame's perimeter.
    bool eventOnRing = false;
    bool eventIsCorner = false;
    int interiorAir = 0;
    int interiorFire = 0;
    int interiorPortal = 0;
};

// The bounded frame scan (the whole V3-1 Phase A decision surface): every
// size-valid frame in EITHER vertical plane through `eventCell` whose
// bounding box contains it, whose non-corner perimeter cells are all
// obsidian (the event cell counted as obsidian when on the perimeter —
// the pre-event view), and whose interior holds only {air, fire, portal}
// (allowPortalInterior = false: only {air, fire} — the strict activation
// search; portal cells disqualify, which is what findFrameAt wants).
// Deterministic output order: (axis, lo, y0) ascending.
std::vector<PortalFrameStatus> scanPortalFrames(
    const World& world, const BlockPos& eventCell, BlockID obsidianId,
    BlockID fireId, BlockID portalId, bool allowPortalInterior = true);

// The largest COMPLETE strict frame (ringComplete, the interior all air /
// fire — no portal) whose bounding box contains `cell` (the "is this cell
// inside a finished, unlit frame" query — the flint-and-steel ignition's
// geometry). Selection: the largest area; a tie prefers the frame whose
// interior already holds a fire (the one an adjacent event would activate
// — the shared-obsidian frames' determinism), then (axis, lo, y0).
// nullopt = no such frame.
std::optional<PortalFrame> findFrameAt(const World& world,
                                       const BlockPos& cell,
                                       BlockID obsidianId, BlockID fireId);

// The frame a LOADED portal cell must sit in to survive the init rescan
// (the Minecraft update semantics): a complete ring + the interior all
// {air, fire, portal} (the live frame's own portals allowed — the strict
// findFrameAt would reject them) + the cell strictly interior. The
// largest per the findFrameAt selection. nullopt = orphan (hand-edited
// save) -> air.
std::optional<PortalFrame> portalValidFrame(
    const World& world, const BlockPos& portalCell, BlockID obsidianId,
    BlockID fireId, BlockID portalId);

// -- the transition plans (the pure cell lists the game applies) --------

// The ACTIVATION plan (pure): the frame's interior cells that hold air or
// fire (the conversion domain — the fire cell is replaced, Minecraft's
// "one of them replacing the fire"), deterministic frameInterior order.
// The game writes each to the portal block in the frame's axis state.
std::vector<BlockPos> portalActivationPlan(const World& world,
                                           const PortalFrame& frame,
                                           BlockID fireId);

// The DEACTIVATION plan (pure): the frame's interior cells that hold the
// portal block (the reverse conversion domain), deterministic order. The
// game writes each to air.
std::vector<BlockPos> portalDeactivationPlan(const World& world,
                                             const PortalFrame& frame,
                                             BlockID portalId);

// The state-transition DECISION over a scan (pure — the choke-point
// logic, the unit tests own it):
//   - deactivate: an OBSIDIAN DESTRUCTION event (oldId == obsidianId)
//     whose cell was a NON-CORNER ring cell of a frame that was complete
//     pre-event and is broken now, with portal blocks in the interior
//     (a corner destruction keeps the portal — Minecraft-exact);
//   - activate: a CURRENTLY-complete frame with a live fire in the
//     interior and no portal yet (the fire placement, the obsidian
//     placement completing the frame, or the shared-frame event — both
//     sides checked, the design's "last block" quirk deviation).
// The fire-extinguish event (oldId == fireId) deactivates nothing (the
// portal is self-sustaining) and activates only when another interior
// fire still lives (idempotent). Both lists in the scan's deterministic
// order.
struct PortalTransitionDecision {
    std::vector<PortalFrameStatus> activate;
    std::vector<PortalFrameStatus> deactivate;
};
PortalTransitionDecision portalTransitionDecision(
    const std::vector<PortalFrameStatus>& candidates, BlockID oldId,
    BlockID obsidianId);

// The standing countdown step (pure — the campfire-tick paradigm, the
// unit tests own it): `inPortal` = the player's AABB overlaps a portal
// cell this frame; `armed` = the frames remaining BEFORE this step
// (0 = unarmed). Returns the frames remaining AFTER the step + whether
// this frame is the COMPLETION frame (the kPortalEntryFrames-th
// consecutive in-portal frame — the Phase A stub's log frame, the Phase
// B travel frame). Leaving the portal (inPortal false) cancels
// immediately (0, no completion); a re-entry restarts from the full
// count (no cooldown — the Phase A semantics pin).
struct PortalEntryStep {
    int framesRemaining = 0;
    bool completed = false;
};
PortalEntryStep portalEntryStep(bool inPortal, int armed);

} // namespace mc
