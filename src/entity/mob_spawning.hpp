// Mob spawning (Phase 20): the data-driven spawn cadence/config and the
// pure spawn-cell scan.
//
// The config is one file: assets/data/mob_spawning.json:
//
// {
//   "check_every_frames": 30,  // spawn attempt cadence (frames)
//   "first_spawn_frame": 60,   // no attempts before this frame
//   "max_mobs": 4,             // live mob cap
//   "min_radius": 4,           // horizontal spawn ring, from the player
//   "max_radius": 6,           //   column (Euclidean)
//   "despawn_distance": 40,    // mobs farther than this (3D) despawn
//   "mobs": [                  // Phase 39: {id, weight} entries (registry
//     {"id": "zombie",         //   cross-check at init; weight = relative
//      "weight": 10}           //   spawn probability, 1..10000, default 1)
//   ]
// }
//
// The scan is a pure function of (world, player anchor, config): the same
// inputs always return the same cell, which keeps headless runs
// byte-reproducible. It walks the cells of the square around the player's
// column in a fixed order (dz from -R..R, then dx from -R..R, R =
// ceil(max_radius)), keeps the Euclidean ring [min_radius, max_radius], and
// returns the FIRST cell that can host a standing mob:
//   - the cell itself is solid (the floor),
//   - the ceil(height - eps) cells above it are air with no fluid
//     (headroom for the mob's AABB),
//   - the floor top is within kSpawnFloorDelta of the player's floor
//     (no spawning onto far cliffs or into far valleys),
//   - the CURRENT light level at the spawn cell (see currentSpawnLight)
//     is <= the mob's spawnMaxLight — the Minecraft day/night rule:
//     hostile mobs (spawnMaxLight 0) only spawn in the dark,
//   - the spawn AABB does not overlap the player's AABB,
//   - no existing mob's bottom-center is within kSpawnClearance
//     (horizontal) of the spawn point (no spawning on top of each other),
//   - the spawn-protection circle (Phase 25): the cell center is at least
//     protectionRadius away (horizontal) from (protectionX, protectionZ)
//     — the WORLD spawn point. Hostile mobs are gated by the difficulty's
//     radius (core/difficulty.hpp); passive mobs pass protectionRadius 0
//     (exempt, exactly like Minecraft).

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "block/block_registry.hpp"
#include "core/error.hpp"
#include "core/json.hpp"
#include "core/math/vec3.hpp"
#include "world/coordinates.hpp"
#include "world/world.hpp"

namespace mc {

// One entry of the 'mobs' list (Phase 39): a registry name + its relative
// spawn weight. The deterministic type pick draws ONE stream value per
// attempt, reduces it into [0, totalWeight), and walks the cumulative
// weights in list order. With every weight 1 the walk is exactly the legacy
// pick % list.size() (Phases 22-27), so the shipped all-1 data changes no
// behavior.
struct SpawnMobEntry {
    std::string id;
    int weight = 1;
};

struct MobSpawningConfig {
    int checkEveryFrames = 30;
    int firstSpawnFrame = 60;
    int maxMobs = 4;
    float minRadius = 4.0f;
    float maxRadius = 6.0f;
    float despawnDistance = 40.0f;
    std::vector<SpawnMobEntry> mobs;

    // The sum of all entry weights — the pick's domain size.
    int totalWeight() const;

    // The pure weighted pick: v must be < totalWeight(). Returns the first
    // entry whose cumulative weight exceeds v.
    std::size_t pickIndex(std::uint64_t v) const;

    // Strict: wrong types / out-of-range values produce errors naming the
    // field; unknown fields are warned about and ignored.
    static Result<MobSpawningConfig> parse(const Json& def);
};

// The floor delta / clearance are spawn-policy constants (named, not
// magic): how far a spawn floor may sit from the player's floor, and how
// close two mobs may spawn to each other.
inline constexpr int kSpawnFloorDelta = 2;
inline constexpr float kSpawnClearance = 1.0f;

// The CURRENT effective light level (0..15) of a cell for spawn gating —
// the Minecraft semantics emulated with this engine's presentation-layer
// day/night (see day_cycle.hpp): the stored daytime (noon) sky light
// reduced by the LGT-3 affine retreat effectiveSkyLight(stored,
// dayFactor) = max(0, stored - floor((15 - kNightSkyLevel) *
// (1 - dayFactor))) — the same level the renderer shows, the seen light
// — plus block light (glowstone), which is unaffected by day/night.
// (At deep night the open-sky level is the kNightSkyLevel 7 moonlight
// floor — the shipped hostile threshold pairs to it, so dark-sky mobs
// spawn under open night sky but not in a torch circle; by day it is the
// stored sky level, so they may not.)
int currentSpawnLight(const World& world, int x, int y, int z,
                      float dayFactor);

std::optional<BlockPos> findMobSpawnCell(const World& world,
                                         const BlockRegistry& blocks,
                                         float minRadius, float maxRadius,
                                         int cx, int cz, int playerFloorY,
                                         float width, float height,
                                         float dayFactor, int spawnMaxLight,
                                         const Vec3& playerMin,
                                         const Vec3& playerMax,
                                         const std::vector<Vec3>& mobPositions,
                                         float protectionRadius = 0.0f,
                                         float protectionX = 0.0f,
                                         float protectionZ = 0.0f);

} // namespace mc
