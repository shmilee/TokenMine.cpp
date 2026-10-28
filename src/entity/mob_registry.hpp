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
//     "drops": [                  // what the mob drops on death (Phase 28:
//       { "item": "leather",      // a list — one entry per dropped item,
//                "count": 1 },    // spawned in list order, each count 1..64
//       { "item": "beef",         // (default 1); absent/[] = nothing)
//                "count": 1 } ]
//   },
//   "model": {
//     "width": 0.6,               // AABB width (x and z), > 0
//     "height": 1.8,              // AABB height, > 0
//     "boxes": [
//       { "pos": [0, 1.25, 0], "size": [0.5, 0.5, 0.5],
//         "texture": "zombie_head" },
//       { "name": "leg", "pos": [0, 0.25, 0], "size": [0.3, 0.5, 0.3],
//         "texture": "zombie_body" },
//       ...
//     ],
//     "swings": [                 // ENT-1: optional limb animation (absent
//       { "part": "leg",          // or [] = no swing parts — the static
//         "amplitude": 0.5,       //   v0.9.0 gait; amplitude in radians,
//         "group": 0 }            //   (0, 1]; group 0/1 = half-cycle
//     ]                           //   phase offset (see MobSwing))
//   }
// }
//
// The model is a box list (Minecraft's simple-skeleton style): each box
// renders with the shared six-face table (same corners/winding as blocks
// and item cubes), one atlas tile on all six faces, centered at `pos`
// relative to the mob's bottom-center position. Every box must lie inside
// the width x height AABB.
//
// ENT-1 limb swing: a box may carry an optional unique `name` (part
// name), and the model may carry a `swings` list whose entries reference
// part names — the referenced part rotates about its pivot (the box
// top-center, the local X axis) while the mob walks (see mob_swing.hpp
// for the integer/frame-locked math). A swing entry's `part` must name an
// existing box (rejected, named, otherwise) and no two entries may claim
// one part.
//
// Validation is strict (same discipline as the block/item registries):
// wrong types, out-of-range values, boxes outside the AABB, duplicate
// ids, unknown box/model/swing fields and dangling swing part references
// all produce descriptive errors naming the mob. Unknown TOP-LEVEL
// fields are warned about and ignored (forward compatibility).
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
    // ENT-1: the optional part NAME (empty = unnamed; unique among the
    // model's boxes). A name is required for a box a 'swings' entry
    // references; unnamed boxes simply never swing.
    std::string name;
};

// One limb-swing spec of a mob model (ENT-1, see mob_swing.hpp for the
// math): the named part rotates about its pivot (the box top-center, the
// local X axis) by ±amplitude radians (the data range (0, 1], the
// ~57-degree walk-stride cap) on a half-cycle phase offset: group 0
// swings with the mob's swing phase, group 1 with (phase + N/2) % N
// (the diagonal gait — chicken's opposite legs, quadruped diagonal leg
// pairs, spider even/odd leg rows). `boxIndex` is the resolved part
// (the existence cross-check — the grows_to discipline — runs at parse
// time: the part must name a box of THIS model, and no two swings may
// claim one part).
struct MobSwing {
    int boxIndex = -1;
    float amplitude = 0.0f; // radians, (0, 1]
    int group = 0;          // 0 or 1
};

struct MobModel {
    float width = 0.0f;   // AABB width (x and z)
    float height = 0.0f;  // AABB height
    std::vector<MobBox> boxes;
    std::vector<MobSwing> swings; // ENT-1: empty = no limb animation
};

// One entry of a mob's drop list (Phase 28; Phase 38 random range).
// `item` is the item NAME (resolved in the ItemRegistry at game init,
// like block drops). The count is either FIXED (countMin == countMax, from
// "count", 1..64 — the Phase 28 form) or a RANDOM RANGE [countMin,
// countMax] (from "count_min"/"count_max", 0..64, min <= max — Phase 38:
// rolled once per death with the mob's seeded stream; 0 = that item does
// not drop this time). Validated to fit the item's max stack at init.
struct MobDrop {
    std::string item;
    int countMin = 1;
    int countMax = 1;
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
    // Melee mobs leave all three at 0 (absent in data). `ammoItem`
    // (Phase 42): the item a stuck arrow shot by this mob returns to the
    // player's inventory on pickup (the game cross-checks the item's
    // existence at init). A ranged mob (shootRange > 0) must define it;
    // melee mobs leave it empty.
    float shootRange = 0.0f;
    float retreatDistance = 0.0f;
    float shootSpeed = 0.0f;
    std::string ammoItem;
    // Milking (Phase 43, cow): `milkWith` is the item the player must
    // hold and `milkGives` is the item a right-click ON the mob returns
    // (one held consumed, one given; the mob is NOT affected — no damage,
    // death, drop or cooldown in v1). Both fields must be present
    // together (a lone field is a typo, rejected — the dye_base /
    // dye_color pairing discipline); both item names are cross-checked
    // against the ItemRegistry at game init (the ammo pattern). Empty =
    // not milkable (every pre-Phase-43 mob).
    std::string milkWith;
    std::string milkGives;
    // Status effect on hit (Phase 46, spider): when this mob's landed
    // MELEE attack hits the player, the named status effect is applied
    // for `hitEffectDuration` seconds (data/effects/*.json; the game
    // cross-checks the effect's existence at init). Ranged attacks (the
    // skeleton's arrows) never apply it. Empty name = no effect (every
    // pre-Phase-46 mob).
    std::string hitEffectName;
    double hitEffectDuration = 0.0;
    // SURV-6: the tier the hit applies (0-based; init range-checked
    // against the effect's tier_max). 0 = the pre-SURV-6 value.
    int hitEffectTier = 0;
    // SURV-5: the status effect this mob's ARROW applies to the target
    // (Minecraft's wither skeleton: its arrows wither the player). The
    // strict {name, duration} pair, the on_hit_effect discipline; only a
    // RANGED mob (shoot_range > 0) may carry it — a melee mob's arrows
    // never fly, so the field would be dead (the ammo discipline). The
    // effect name is cross-checked against the EffectRegistry at game
    // init. Empty = no arrow effect (every pre-SURV-5 mob — the shipped
    // skeleton is the first carrier).
    std::string arrowEffectName;
    double arrowEffectDuration = 0.0;
    // SURV-6: the tier the arrow applies (0-based; init range-checked).
    int arrowEffectTier = 0;
    // SURV-5: the undead (Minecraft's undead set — the zombie / skeleton).
    // An effect flagged `undead_exempt` (the poison) is ignored when
    // applied to this mob. false = a normal mob (every pre-SURV-5 mob).
    bool undead = false;
    // Drops (Phase 28): the item list the mob drops on death — one entry
    // per dropped item, spawned in list order. Empty = no drops.
    std::vector<MobDrop> drops;
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
