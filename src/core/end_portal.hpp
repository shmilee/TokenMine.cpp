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

} // namespace mc
