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
#include <optional>
#include <vector>

#include "block/block.hpp"
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
inline constexpr bool kPortalTravelEnabled = false;

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

// The 8:1 overworld -> nether mapping (Minecraft, the wiki-verified set):
// the coordinates floor-divided by kPortalMappingScale (floorDiv = the
// negative-safe floor: -7 -> -1, 7 -> 0), Y unscaled. Pure; Phase A logs
// it, Phase B consumes it as the travel destination.
BlockPos overworldToNether(int x, int y, int z);

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
