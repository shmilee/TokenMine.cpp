// V2-13 (v2.14.0): the bed sleep system — the pure gate family the unit
// tests own (the campfire pattern: the game is the thin adapter over
// these; the right-click channel + the respawn flow live in the Game).
//
// Model (the Minecraft bed semantics, the wiki-verified 11-04 set — the
// design's scope decisions, the documented deviations noted per gate):
//   - the bed is a single-cell SLAB block (the bottom half-cell, the
//     stone_slab precedent — the 1x2 two-cell pair is out of scope:
//     there is no multi-cell block in this project, documented
//     deviation 1).
//   - a right-click on a bed family block (the `dye_base` "bed"
//     property — the family check, the core switches on NO color id,
//     hard rule 3) attempts the sleep: three gates decide, in order —
//     (b) the player's feet are within kSleepMaxDistance of the bed
//     cell's center (Minecraft: 3 blocks — the right-click's own reach
//     is 4.0, this gate is the stricter one); (c) NO hostile mob sits
//     in the box around the bed (Minecraft's head 8x5 volume, adapted
//     to the single-cell slab = the whole cell as the head position,
//     documented deviation 4); (a) the time of day is in the night
//     window. Gates b and c failing = the whole action is refused
//     (NO spawn point is set — documented deviation 6: the bare
//     point-set applies only when the sleep is otherwise allowed).
//   - inside the window the sleep is IMMEDIATE (the documented
//     deviation 2: the Minecraft 101-tick entry animation + the
//     hurt/wake-up are out of scope — the right-click completes it):
//     the time jumps to the dawn edge (DayCycle::kDawnStart = the
//     06:00 clock), a raining weather resets to CLEAR (the V2-4
//     machine, the Minecraft "sleeping clears the rain" rule — a clear
//     sky is untouched), the bed cell's center becomes the respawn
//     point, and the "sleep" cue plays (spatialized at the bed).
//   - OUTSIDE the window (by day) the right-click sets the RESPAWN
//     POINT ONLY (the Minecraft day rule: a bed right-clicked by day
//     does not enter the bed, it does set the spawn) — the click is
//     consumed either way (no placement, no dye fall-through except a
//     HELD dye, which recolors the bed through the Phase-41 path —
//     the bed is a dye family like the wool).
//   - the respawn point is SAVED (format 28, player.respawn — the bed
//     cell's int triple, always written) and RE-VALIDATED at RESPAWN
//     TIME ONLY (the Minecraft rule: the coordinate is remembered
//     through the bed's break/recolor/reorientation; at the respawn,
//     the cell must still hold a bed family block with room for the
//     player's AABB — else the world spawn is the fallback, which a
//     broken bed therefore restores permanently).
#pragma once

#include <cmath>
#include <string_view>

#include "core/error.hpp"
#include "core/math/vec3.hpp"
#include "world/coordinates.hpp"
#include "world/day_cycle.hpp"

namespace mc {

class BlockRegistry;
class World;

// The dye family name of the bed blocks (Phase 41's dye_base property —
// every bed family member carries dye_base "bed" + its dye_color; the
// white member is the base name, the dyeVariantName convention). The
// core reads this property, never a color id (hard rule 3).
inline constexpr std::string_view kDyeBaseBed = "bed";

// -- the sleep gates (Minecraft values, the named-constant discipline) --
// Gate b: the max distance (in blocks, 3D) between the player's feet
// (the AABB bottom center) and the bed CELL'S CENTER. Minecraft: 3
// ("within 3 blocks of the bed" — the right-click's own reach 4.0
// admits the aim, this gate is the stricter rule).
inline constexpr float kSleepMaxDistance = 3.0f;
// Gate c: the hostile-exclusion box around the bed cell's center —
// horizontal radius 8 (the Minecraft head volume's 8x5 span, the
// single-cell slab's whole cell standing in for the head position,
// the documented deviation 4), vertical from -1 to +5 (the volume
// sits one cell below to five cells above the bed's cell).
inline constexpr float kSleepHostileRadius = 8.0f;
inline constexpr float kSleepHostileAbove = 5.0f;
inline constexpr float kSleepHostileBelow = 1.0f;

// Gate b: the player's feet within the bed's reach (the inclusive
// boundary: exactly kSleepMaxDistance still sleeps — the unit tests
// pin both sides).
bool sleepDistanceOk(Vec3 feet, Vec3 bedCenter);

// Gate c (per mob): the mob's position inside the hostile-exclusion
// box (the CLOSED boundary: a mob exactly on the box's face still
// prevents the sleep — the unit tests pin the corners).
bool inSleepHostileBox(Vec3 bedCenter, Vec3 mobPos);

// Gate a: the night window of the time of day t in [0, 1) — the
// project's night plateaus [kDuskEnd, 1) + [0, kDawnStart) (the day
// cycle's ramp edges — no new magic: the window is the DERIVED
// constant span. The Minecraft "sky light <= 11" rule maps to
// [0.586, 0.045] in this project's time coordinates — the project's
// window is slightly wider, the documented deviation 3).
inline bool sleepNightWindow(double timeOfDay) {
    return timeOfDay >= DayCycle::kDuskEnd ||
           timeOfDay < DayCycle::kDawnStart;
}

// The bed cell's respawn position: the cell's CENTER (the slab's top
// is the cell's middle, so the feet rest on it; the AABB bottom
// convention — position = bottom center).
inline Vec3 bedSpawnPosition(int x, int y, int z) {
    return Vec3 {static_cast<float>(x) + 0.5f,
                 static_cast<float>(y) + 0.5f,
                 static_cast<float>(z) + 0.5f};
}

// The bed cell of a remembered respawn position (bedSpawnPosition's
// inverse — the floor of each component; a cell-center position floors
// exactly, a world-spawn position's floor is the cell the spawn sits
// on, which never holds a bed = the fallback is exercised instead).
inline BlockPos bedSpawnCell(Vec3 pos) {
    return BlockPos {static_cast<int>(std::floor(pos.x)),
                     static_cast<int>(std::floor(pos.y)),
                     static_cast<int>(std::floor(pos.z))};
}

// The RESPAWN-TIME bed validation (the Minecraft rule — the saved
// coordinate is re-checked only when the player respawns): true when
// the cell of `pos` holds a bed FAMILY block (the kDyeBaseBed property
// — any color) AND the player's AABB (bottom-center `pos`, the given
// dimensions) fits — the support cell (the bed's own cell) is exempt
// from the solid test: its slab's solid region ends at the feet (the
// Entity::hasRoomAt height-aware overlap precedent, the slab's top
// 0.5 — standing on the slab's top is room). A cell that stops being a
// bed (broken, recolored into... the family is color-blind — only
// breaking or replacing removes it) or a blocked headroom is false =
// the world-spawn fallback.
bool bedRespawnValid(const World& world, const BlockRegistry& blocks,
                     Vec3 pos, float width, float height);

} // namespace mc
