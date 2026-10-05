// Mob definitions (Phase 20): data-driven mob types.
//
// One JSON file per mob under assets/data/mobs/:
//
// {
//   "id": "zombie",
//   "properties": {
//     "health": 20,               // > 0
//     "speed": 0.8,               // blocks/s while chasing, > 0
//     "detect_radius": 16,        // >= 0; horizontal distance that starts a
//                                 // chase (0 = never: a passive mob)
//     "attack_range": 1.6,        // >= 0; horizontal distance that allows
//                                 // attacks (0 = never attacks)
//     "attack_damage": 3,         // >= 0; damage per hit (0 = harmless)
//     "attack_cooldown": 1.0,     // seconds between hits
//     "wander_interval": 2.5,     // seconds between wander decisions
//     "spawn_max_light": 0,       // 0..15; only spawn where the current
//                                 // effective light level is <= this
//                                 // (Minecraft hostile-mob convention:
//                                 // dark only, default 0; see the spawn
//                                 // light definition in mob_spawning.hpp)
//     "hostile": true,            // optional bool, default false (Phase 25):
//                                 // subject to the difficulty system
//                                 // (spawn protection + damage scale; see
//                                 // core/difficulty.hpp)
//     "drops": "rotten_flesh",    // item name; "" = nothing (optional)
//     "drops_count": 1            // 1..64 (optional, default 1)
//   },
//   "model": {
//     "width": 0.6,               // AABB width (x and z), > 0
//     "height": 1.8,              // AABB height, > 0
//     "boxes": [
//       { "pos": [0, 1.25, 0], "size": [0.5, 0.5, 0.5],
//         "texture": "zombie_head" },
//       ...
//     ]
//   }
// }
//
// The model is a box list (Minecraft's simple-skeleton style): each box
// renders with the shared six-face table (same corners/winding as blocks
// and item cubes), one atlas tile on all six faces, centered at `pos`
// relative to the mob's bottom-center position. Every box must lie inside
// the width x height AABB.
//
// Validation is strict (same discipline as the block/item registries):
// wrong types, out-of-range values, boxes outside the AABB and duplicate
// ids all produce descriptive errors naming the mob. Unknown fields are
// warned about and ignored (forward compatibility).
//
// Cross-checks against the other systems (model textures exist in the
// atlas, drops resolve in the ItemRegistry, the spawning list resolves
// here) happen at game init — the same pattern as block drops.
#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/asset_manager.hpp"
#include "core/error.hpp"
#include "core/json.hpp"
#include "core/math/vec3.hpp"

namespace mc {

// Stable handle into a MobRegistry. Invalid = "no mob type".
struct MobID {
    int value = -1;

    constexpr bool valid() const { return value >= 0; }
    constexpr bool operator==(MobID o) const { return value == o.value; }
    constexpr bool operator!=(MobID o) const { return value != o.value; }
    static constexpr MobID invalid() { return {-1}; }
};

struct MobIDHash {
    std::size_t operator()(MobID id) const {
        return std::hash<int>{}(id.value);
    }
};

// One render box of a mob model (see the file head for the layout).
struct MobBox {
    Vec3 center = Vec3::zero();  // box center, relative to bottom-center
    Vec3 size = Vec3::zero();    // full extent per axis
    std::string texture;         // atlas tile name (all six faces)
};

struct MobModel {
    float width = 0.0f;   // AABB width (x and z)
    float height = 0.0f;  // AABB height
    std::vector<MobBox> boxes;
};

// Behavior parameters of a mob type (all from data; see the file head).
struct MobProperties {
    float health = 0.0f;
    float speed = 0.0f;
    float detectRadius = 0.0f;
    float attackRange = 0.0f;
    float attackDamage = 0.0f;
    float attackCooldown = 0.0f;
    float wanderInterval = 0.0f;
    // Spawning light gate (0..15): the mob may only spawn where the
    // cell's CURRENT effective light level (sky light scaled by the
    // day/night factor + block light, see currentSpawnLight in
    // mob_spawning.hpp) is <= this value. 0 = darkness only (the
    // Minecraft hostile-mob convention; a 15 would allow any time of day,
    // for future passive mobs).
    int spawnMaxLight = 0;
    // Hostile (Phase 25): the mob is a hostile for the difficulty system
    // (core/difficulty.hpp) — the spawn-protection circle applies to its
    // natural spawns and its attack_damage is scaled by the difficulty.
    // Passive mobs (false, the default) are exempt from both. The flag is
    // data, not derived from behavior values (attack_damage 0 could still
    // be a hostile that never hits in v1, and vice versa).
    bool hostile = false;
    // Ranged attack (Phase 27, skeleton): `shootRange > 0` makes the mob a
    // ranged attacker — within `shootRange` (horizontal) of the player it
    // fires arrows (one per `attackCooldown`) instead of closing in, and
    // backs away at full speed while the player is inside `retreatDistance`
    // (0 = never back away). `shootSpeed` is the arrow launch speed.
    // Melee mobs leave all three at 0 (absent in data).
    float shootRange = 0.0f;
    float retreatDistance = 0.0f;
    float shootSpeed = 0.0f;
    std::string drops;   // item name; empty = no drops
    int dropsCount = 1;  // 1..64
};

struct Mob {
    MobID id = MobID::invalid();
    std::string name;
    MobProperties properties;
    MobModel model;
};

class MobRegistry {
public:
    MobRegistry() = default;

    // Parse and register one mob. Assigns the next free id.
    Result<MobID> registerMob(const Json& def);

    // Load and register every *.json file in `relativeDir` under the asset
    // root (sorted for determinism). Non-const: text loading updates the
    // AssetManager cache.
    Result<void> loadDirectory(AssetManager& assets,
                               const std::string& relativeDir =
                                   "data/mobs");

    // -- lookups --------------------------------------
    const Mob& at(MobID id) const;      // asserts valid id
    const Mob* find(MobID id) const;
    const Mob* findByName(const std::string& name) const;
    MobID idForName(const std::string& name) const;  // invalid() when absent
    std::size_t count() const { return m_mobs.size(); }
    std::vector<MobID> allIds() const;

    void clear();

    // Pure parse helpers (unit-testable without a registry).
    static Result<void> parseProperties(const Json& def, MobProperties& out);
    static Result<void> parseModel(const Json& def, MobModel& out);

private:
    std::vector<Mob> m_mobs;
    std::unordered_map<std::string, int> m_names;
};

} // namespace mc
