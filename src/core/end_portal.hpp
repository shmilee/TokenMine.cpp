// The end portal's pure function family (V3-10a, the nether portal's
// second consumption — the 5x5 horizontal ring, the A-lite ruling: a
// dedicated frame block, no orientation, the data predicates
// end_frame / end_frame_eyed). The decision lives here (pure over the
// World + BlockRegistry, unit-tested headless), the game's init /
// interaction / travel are thin adapters (the scanPortalFrames
// pattern).
//
// The geometry (the body-level ring — the player standing on the floor
// under the center has the portal cell around its lower body, the
// entry's AABB overlap): a 5x5 square at level L (the player's FEET
// level) centered on (cx, cz) — the 12 ring cells (the four corners
// missing) hold end_portal_frame / end_portal_frame_eyed blocks; the
// 3x3 center holds end_portal blocks once ACTIVE. Activation = all 12
// ring cells EYED + the 3x3 center all air (the eye placement's scan).
#pragma once

#include <cstdint>
#include <vector>

#include "block/block.hpp"
#include "block/block_registry.hpp"
#include "world/coordinates.hpp"
#include "world/world.hpp"

namespace mc {

// The 12 ring cells of the 5x5 (the four corners missing), in the
// deterministic (dz, dx) nested order — the scan / the rescan / the
// save's states cross-check all read this order.
std::vector<BlockPos> endPortalRingCells(int cx, int level, int cz);

// The ring center CANDIDATES of a ring cell (the embed interaction's
// reverse map): a ring cell sits at one of the EIGHT ring offsets from
// its center — (±2, 0), (0, ±2), (±1, ±2) (the four corners missing) —
// so the candidate centers are the cell minus each offset. A genuine
// ring cell of a single square has exactly ONE valid candidate (the
// others scan incomplete); a hand-crafted cluster can complete several
// (the caller activates the FIRST in deterministic order). The
// deterministic order: (-2,0), (2,0), (0,-2), (0,2), (-1,-2), (1,-2),
// (-1,2), (1,2).
std::vector<BlockPos> endRingCenterCandidates(const BlockPos& ringCell);

// The 3x3 center cells (the portal area), in the same (dz, dx) order.
std::vector<BlockPos> endPortalCenterCells(int cx, int level, int cz);

// The ring's scan result (the pure predicate over the world): the
// candidate center (cx, level, cz) is a VALID end portal ring when
// ringComplete — the activation predicate (allEyed && centerAir) is
// the game's write decision, kept separate (the nether portal's
// activation-decision twin: the scan reports, the adapter decides).
struct EndPortalScan {
    int cx = 0;
    int level = 0;
    int cz = 0;
    // True when all 12 ring cells hold an end_frame block (eyed or
    // not); the ring's geometry is then (cx, level, cz).
    bool ringComplete = false;
    // The ring cells' eyed count (0..12; meaningful when ringComplete).
    int eyedCount = 0;
    // The 3x3 center's state: all air (activatable) / all end_portal
    // (already active) — neither = an invalid (mixed) center.
    bool centerAir = false;
    bool centerPortal = false;
};

// Scans the 5x5 ring centered on (cx, level, cz) — the pure read over
// the world (the data predicates end_frame / end_frame_eyed /
// end_portal; no block id ever hardcoded — hard rule 3).
EndPortalScan scanEndPortalRing(const World& world,
                                const BlockRegistry& blocks, int cx,
                                int level, int cz);

// The rescan's orphan predicate (the portalValidFrame twin): an
// end_portal CELL is valid when SOME center of the 3x3 it could belong
// to (the ring is horizontal, same level) scans as a COMPLETE,
// ALL-EYED ring with a FULL portal center — the activated read the
// frames' no-deactivation path guarantees is monotone. The 9 candidate
// centers are the cell's (dz, dx) offset neighborhood (deterministic
// order); a hand-edited save's partial ring orphans its portal cells
// (the load sets them to air, the PITS 383 two-way cross-check).
bool endPortalCellValid(const World& world, const BlockRegistry& blocks,
                        const BlockPos& cell);

// The activation plan: the 3x3 center cells to become end_portal
// (the deterministic order — the setBlock sequence the game applies).
// Valid only on an activatable scan (allEyed + centerAir).
std::vector<BlockPos> endPortalActivationPlan(int cx, int level, int cz);

// The entry's SPAWN platform (the travel pipeline's per-entry write —
// the nether's portalPlatformPlan twin, mirrored): the 5x5 obsidian
// square at level kEndEntryPlatformLevel (= 52, the island's top layer)
// centered on (kEndEntryX, kEndEntryZ) = (40, 0) — the player's feet
// land on its top (53), the walk to the origin exit is 40 blocks west.
// The write also clears the 3 layers ABOVE each platform cell (the
// MC's landing-safety ruling; the air is the island's open sky).
inline constexpr int kEndEntryPlatformLevel = 52;
inline constexpr int kEndEntryX = 40;
inline constexpr int kEndEntryZ = 0;
inline constexpr int kEndEntryPlatformClearAbove = 3;
struct EndEntryPlatformPlan {
    // The 25 obsidian cells (deterministic (dz, dx) order).
    std::vector<BlockPos> setObsidian;
    // The clear-above cells (the 25 columns x 3 layers above the
    // platform top, deterministic order).
    std::vector<BlockPos> clearAir;
};
EndEntryPlatformPlan endEntryPlatformPlan();

// The landing point on the platform (the travel's restorePosition
// target — the platform's top center, the feet) + the landing yaw
// (west = +pi/2, the camera convention: yaw 0 = -Z, positive turns
// left — the recon's MC-exact ruling).
Vec3 endEntryPosition();
float endEntryYaw();

// V3-10d (the ender dragon): the boss's FIXED end-origin anchors (the
// endEntryPosition's named-constant shape — the exit portal's arena,
// the world origin, D1's always-live exit). The dragon PERCHES over the
// exit (the MC perched-on-the-exit read), the death EGG lands on the
// exit's center block, and the first spawn is the orbit's top point.
// All frame-locked pure constants (zero world reads — the worldgen and
// the game's dragon pass share them, the PITS-406 single-source shape).
// The perch: the exit's 2.5-block hover (the egg's top + 1.5 — the
// dragon perches over its own arena's core).
inline constexpr Vec3 kEndPerchPos {0.5f, 56.5f, 0.5f};
// The egg: ONE above the exit's center body (the portal's y53 body's
// top cell — D17's non-solid egg; the player's head cell over the
// exit, the walk-through door).
inline constexpr Vec3 kEndEggPos {0.5f, 54.5f, 0.5f};
// The orbit's center + the first spawn (the perch's 1.5-block-up point
// — the dragon appears high and settles into the orbit).
inline constexpr Vec3 kDragonOrbitCenter {0.5f, 58.0f, 0.5f};
inline constexpr Vec3 kDragonSpawnPos {0.5f, 58.0f, 0.5f};
// The six fixed CRYSTAL slots' CELL CENTERS (the 4 tower tops + the 2
// exit crystals, D3's project scale over MC's 10 — the towers' 5x5
// obsidian columns rise y53..y58 at the plateau's (±50, ±50) with the
// crystal on top (y59), the exit pair on the frame ring's mid-edge
// TOPS ((±2, y54, 0), the frame itself untouched). The game's crystal
// pass / the worldgen pass 4 / the arrow shatter sweep / the heal
// source all read THIS vocabulary (the single source, zero per-world
// state; the fire sits at cell +1y above a live crystal).
inline constexpr Vec3 kEndCrystalSlots[6] = {
    {50.5f, 59.5f, 50.5f},
    {-50.5f, 59.5f, 50.5f},
    {50.5f, 59.5f, -50.5f},
    {-50.5f, 59.5f, -50.5f},
    {2.5f, 54.5f, 0.5f},
    {-2.5f, 54.5f, 0.5f},
};
// The four TOWER footprints' centers (the 5x5 obsidian columns'
// (±50, ±50) — the worldgen pass 4's writer vocabulary, the slot
// array's tower quartet shared).
inline constexpr Vec3 kEndTowerSlots[4] = {
    {50.5f, 0.0f, 50.5f},
    {-50.5f, 0.0f, 50.5f},
    {50.5f, 0.0f, -50.5f},
    {-50.5f, 0.0f, -50.5f},
};

} // namespace mc
