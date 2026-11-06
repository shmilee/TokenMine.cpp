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
//                                 // (a flying mob reuses it as the cruise
//                                 //  heading re-roll cadence)
//     "flying": false,            // V2-14 optional bool: gravity off +
//                                 // the flyCruise AI (requires
//                                 // spawns_in_air — the parse rejects
//                                 // the pair by name)
//     "spawns_in_air": false,     // V2-14 optional bool: the spawn cell
//                                 // is the mob's air box at ANY Y (the
//                                 // floor band is skipped — the bat's
//                                 // cave semantics)
//     "ambient_sound": "squeak",  // V2-14 optional: the ambient sample
//                                 // (the wav's file stem; the init
//                                 // cross-checks the sound directory)
//     "ambient_interval": 3.0,    // V2-14 optional seconds > 0: the
//                                 // per-frame roll period (the default
//                                 // when the sound names no interval)
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
//       { "name": "head", "pos": [0, 1.55, 0], "size": [0.5, 0.5, 0.5],
//         "texture": "ph_face",
//         "faces": { "front": "ph_face", "back": "ph_hair",
//                    "top": "ph_hair", "left": "ph_side",
//                    "right": "ph_side", "bottom": "ph_hair" } },
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
// V2-2 per-face textures: a box may carry an optional `faces` object
// mapping face names to atlas tile names. The face names follow the
// yaw-0 = -Z convention (front = -Z, back = +Z, left = -X, right = +X,
// top = +Y, bottom = -Y — the box faces of the shared table). Partial
// keys are legal: only the named faces are overridden, the rest fall
// back to the box's `texture`. Absent `faces` = the pre-V2-2
// single-tile behavior (bit-identical geometry for every shipped mob).
// The tile names (default + overrides) are cross-checked against the
// atlas at game init (the model-texture pattern, named box + face).
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
#include <deque>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "block/block.hpp"
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

// V2-14: the fixed mob step's frame rate (the 1/60 s step — hard rule
// 8): the ambient roll's per-frame probability is 1 / (interval * this),
// and the flight bob's period is counted in these frames.
inline constexpr int kMobFramesPerSecond = 60;

// V2-14: the ambient roll's default period (seconds) when the data names
// a sound but not an interval (the bat's shipped value — the Minecraft
// ambience cadence class).
inline constexpr double kAmbientIntervalDefaultSeconds = 3.0;

// One render box of a mob model (see the file head for the layout).
struct MobBox {
    Vec3 center = Vec3::zero();  // box center, relative to bottom-center
    Vec3 size = Vec3::zero();    // full extent per axis
    std::string texture;         // atlas tile name (all six faces unless a
                                 // faces override names another)
    // ENT-1: the optional part NAME (empty = unnamed; unique among the
    // model's boxes). A name is required for a box a 'swings' entry
    // references; unnamed boxes simply never swing.
    std::string name;
    // V2-2: the optional per-face tile overrides (face name -> atlas tile
    // name). EMPTY = the pre-V2-2 single-tile behavior (every face uses
    // `texture` — the bit-identical root). Keys are the six face names
    // of the yaw-0 = -Z convention (front = -Z, back = +Z, left = -X,
    // right = +X, top = +Y, bottom = -Y); the strict parse accepts
    // nothing else (a partial map overrides only the named faces).
    std::map<std::string, std::string> faces;
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

// V2-15 (sheep): one entry of a mob's `wool_weights` list (the
// spawn-time wool color's weighted roll): the DYE COLOR NAME (the
// vocabulary is data — cross-checked against the dye items' colors at
// game init, the family-completeness pattern) + the roll weight (a
// strict integer 1..10000, parse-validated).
struct WoolWeight {
    std::string color;
    int weight = 0;
};

// V3-14c: a barter output entry (the piglin's trade — the weighted roll
// + the deterministic count range, the wool_weights / drops fusion).
struct BarterOutput {
    std::string item;
    int weight = 1;
    int countMin = 1;
    int countMax = 1;
};

// V2-15: the model box NAME of a wool_drops mob's wool body (the
// render-layer variant's anchor: the per-instance tile — the instance-
// color dye variant or the bare skin tile — is applied to this box's
// texture when the variant mesh is built). Shipped on the sheep's body
// box. An unnamed box is simply never a variant target.
inline constexpr std::string_view kWoolBoxName = "wool";

// V2-15: the BARE (sheared) wool mob's body tile (the new atlas tile
// "sheep_skin" — the pinkish skin under the wool, the generator's
// 0x5EE4 salt). The render derives it for the wool box when the
// instance is bare; a woolly instance takes the instance-color dye
// variant instead (dyeVariantName over the kWoolDyeFamily base — the
// wool family's EXISTING block tiles, same atlas: zero new texture
// for the 15 colors).
inline constexpr std::string_view kWoolBareTile = "sheep_skin";

// V2-15: the spawn-time weighted roll of a wool color: ONE uniform `u`
// in [0, 1) reduced into the total-weight domain and walked over the
// cumulative list in DATA ORDER (the attemptMobSpawn pick's paradigm —
// the same single-draw discipline; all weights 1 would pick exactly
// entry `u * n`). The parse guarantees total >= 1, so `u` always lands
// on an entry (the trailing return is the rounding safety for
// u -> 1).
inline std::string rollWoolColor(const std::vector<WoolWeight>& weights,
                                 double u) {
    int total = 0;
    for (const WoolWeight& w : weights) {
        total += w.weight;
    }
    const double target = u * static_cast<double>(total);
    double cum = 0.0;
    for (const WoolWeight& w : weights) {
        cum += static_cast<double>(w.weight);
        if (target < cum) {
            return w.color;
        }
    }
    return weights.back().color;
}

// V3-14c: the barter's item pick — the rollWoolColor twin (a single
// uniform01 draw over the cumulative weights, DATA order; the degenerate
// one-entry table still consumes the one draw, the roll discipline).
// Returns the chosen entry's index.
inline int rollBarterOutput(const std::vector<BarterOutput>& outputs,
                            double u) {
    int total = 0;
    for (const BarterOutput& o : outputs) {
        total += o.weight;
    }
    const double target = u * static_cast<double>(total);
    double cum = 0.0;
    for (std::size_t i = 0; i < outputs.size(); ++i) {
        cum += static_cast<double>(outputs[i].weight);
        if (target < cum) {
            return static_cast<int>(i);
        }
    }
    return static_cast<int>(outputs.size()) - 1;
}

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
    // V3-14b: the shot's sound (the data-driven shoot cue — the
    // skeleton's "shoot" default; the ghast's "ghast_roar" is its
    // pre-fireball warning). The init cross-checks the wav's existence
    // (the ambient_sound's twin).
    std::string shootSound = "shoot";
    // V3-14b: the exploding shooter (the ghast): `explodeRadius > 0`
    // turns the mob's shot from an arrow into a FIREBALL — damage 0
    // direct, it explodes on block impact / player contact through the
    // Game::explodeAt seam (V3-14a, the sourceName = the mob's id
    // names the death line). A fireball is consumed at impact (it never
    // sticks), so its pickup is unreachable and the ammo rule exempts
    // exploding shooters (a fireball returns nothing).
    float explodeRadius = 0.0f;
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
    // Bartering (V3-14c, piglin): `barterInput` is the item the player
    // must HOLD for a right-click ON the mob to trade (one held
    // consumed), and `barterOutputs` the weighted table the trade rolls
    // (item + weight + the count range, the drops' interval twin). Both
    // fields must be present TOGETHER (a lone field is a typo, rejected
    // — the milk pairing discipline); every item name is cross-checked
    // against the ItemRegistry at init (the checkWoolDrops pure
    // pattern). The trade NEVER damages the mob (a neutral barter does
    // not enrage — the milk non-enrage's structural twin: the
    // interaction path never calls damage()). Empty = not a bartering
    // mob (every pre-V3-14c mob).
    std::string barterInput;
    std::vector<BarterOutput> barterOutputs;
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
    // V3-3 (the nether mobs): the NEUTRAL class flag (data 'neutral',
    // strict bool, default false) — the data-driven MIDDLE class between
    // passive (detect_radius 0 — wanders, never chases) and hostile
    // (chases on sight): a neutral mob carries a live detect_radius but
    // CHASES ONLY WHILE ANGRY (the MobEntity's m_angry state — any
    // damage source enragues it for kNeutralAngryDurationSeconds real dt,
    // the player's death de-enragues, the design's single-predicate
    // gate: the chase condition ORs the existing detect test with
    // `neutral && angry` — no species switch). STRICTLY EXCLUSIVE with
    // `hostile` (both true = the parse rejects by name — a mob cannot be
    // both sight-chasing and anger-gated). false = the pre-V3-3
    // semantics (every shipped overworld mob is false = the bit-
    // identical structural root).
    bool neutral = false;
    // V3-3 (the strider): the WALKS-ON-LAVA flag (data 'walks_on_lava',
    // strict bool, default false) — the lava's surface plane is this
    // mob's FLOOR: the fire system's lava ignition is exempt (the fire
    // block still ignites — Minecraft's strider burns on fire too), the
    // fluid integrate runs the feet-on-surface walk pace (data speed,
    // no underwater gate — the integrateLavaSurface branch) instead of
    // the swim-level float, and the spawn scan's lava-surface branch
    // (the findMobSpawnCell walks the column top-down for a lava source
    // cell whose top is a free surface) offers it cells the floor scan
    // never would. false = the pre-V3-3 fluid semantics (every shipped
    // mob is false = the bit-identical structural root — the
    // pigs-in-water regression pins it).
    bool walksOnLava = false;
    // V3-14d: the rideable flag (the walks_on_lava's strict-bool twin,
    // default false): the player may MOUNT this mob (right-click, the
    // tryRide interaction). A rideable mob is a RIDE TARGET only — the
    // riding mechanics (the carry snap, the steer takeover, the
    // dismounts) live on the game's ride pass + the MobEntity's
    // m_ridden session state; the flag gates the INTERACTION, the
    // session state gates the BEHAVIOR. Every non-rideable mob's data
    // is unchanged (the pre-V3-14d structural red line).
    bool rideable = false;
    // V3-10c (the enderman): the EYE-CONTACT provocation range (data
    // 'eye_contact_range', a number >= 0, default 0 = OFF): the game's
    // per-frame pass raycasts the player's aim at the mob's FULL AABB
    // (occluded by the aim block) and, while the aim is inside this
    // range, feeds the MobEntity's integer frame counter — kEyeContact-
    // Frames consecutive looking frames provoke it (the neutral's m_angry
    // + the 30 s window, the damage-provocation's twin source). The
    // pass is zero-stream (pure geometry + frame counting — the
    // stream-discipline's clean side, the ambient roll's contrast).
    // 0 = no eye contact (every pre-V3-10c mob, the bit-identical
    // structural root).
    float eyeContactRange = 0.0f;
    // V3-10c (the enderman): the TELEPORT interval pair (data
    // 'teleport_interval_min' / 'teleport_interval_max', seconds, each
    // in (0, 60], min <= max, PRESENT TOGETHER — the milk pairing
    // discipline): the random teleport's re-roll window (lerp over one
    // per-mob-stream draw). The pair's PRESENCE is the teleport
    // capability's structural gate (0/0 = no teleport, every
    // pre-V3-10c mob); the HIT-triggered attempt (damage sets the
    // want-flag, the update's slot consumes it BEFORE the timer — the
    // stream order pinned) rides the same capability. Absent = the
    // pre-V3-10c bit root.
    double teleportIntervalMin = 0.0;
    double teleportIntervalMax = 0.0;
    // V3-10c (the enderman): the CARRY flag (data 'carries_blocks',
    // strict bool, default false): the game's carry pass may pick up a
    // 'carriable' block (the block-side data predicate — 10c-lite
    // ships endstone + chorus_plant) into the mob's session state and
    // drop it back (the 1/6000-frame place roll) — the death drops the
    // carried instance (100%). false = no carry (every pre-V3-10c mob,
    // the bit-identical structural root; the block-side carriable
    // defaults false, so BOTH ends must ship for the feature to run —
    // the two-phase asset discipline).
    bool carriesBlocks = false;
    // Drops (Phase 28): the item list the mob drops on death — one entry
    // per dropped item, spawned in list order. Empty = no drops.
    std::vector<MobDrop> drops;
    // V2-11 (XP): the experience points the mob's death drops as an XP
    // orb (one orb, this value — the despawn pass spawns it at the mob's
    // rest position, the drop-point twin). Optional data field `xp`, a
    // strict integer in [0, 9]: 0 (the default) = no orb; > 9 rejected
    // by name (the orb value cap — no split mechanism). Shipped = the
    // FIXED values of the modern Minecraft table (the wiki-verified
    // 11-03 set, the random intervals simplified to their mids — the
    // documented deviation): hostiles zombie / skeleton / spider 5
    // (Minecraft's fixed 5), passives pig / cow / sheep / chicken 2
    // (Minecraft's 1-3 mid).
    int xp = 0;
    // V2-14 (the bat — the project's first flying mob): the flight data
    // flags — the core reads flags, never species (the data-driven red
    // line). Every pre-V2-14 mob is false / empty = the bit-identical
    // structural root.
    // `flying` (data 'flying', strict bool, default false): the gravity
    // gate off + the flyCruise AI branch (the frame-locked hover-bob
    // baseline, the cruise steering, the wall-hit heading re-roll — see
    // MobEntity). The ONE hard pairing: a flying mob must also set
    // `spawnsInAir` (a ground-spawning flier is meaningless — the parse
    // rejects the pair by name).
    bool flying = false;
    // `spawnsInAir` (data 'spawns_in_air', strict bool, default false):
    // the spawn cell = the mob's air box at ANY Y (the findMobSpawnCell
    // in-air branch: the floor band is skipped entirely, the Minecraft
    // bat cave semantics).
    bool spawnsInAir = false;
    // The ambient sound (the bat's squeak — the GENERAL mechanism, not a
    // bat special case): `ambientSoundName` (data 'ambient_sound', a
    // non-empty sound name = the wav's file stem; the game init
    // cross-checks it against the sound directory — the
    // checkSoundMaterials file-based discipline, strict even with the
    // audio disabled) and `ambientInterval` (data 'ambient_interval',
    // seconds > 0, default kAmbientIntervalDefaultSeconds): the flyCruise
    // per-frame roll plays the sample spatialized at the mob with
    // probability 1 / (interval * kMobFramesPerSecond) of the per-mob
    // stream (frame-locked, hard rule 8 — a no-input run squeaks on the
    // same frames every time). Empty name = no ambient (every
    // pre-V2-14 mob — the stream draw never happens).
    std::string ambientSoundName;
    double ambientInterval = kAmbientIntervalDefaultSeconds;
    // V2-15 (sheep — the project's first per-instance-colored mob): the
    // WOOL data (the data-flag gates, no species switch — the core reads
    // flags, and every pre-V2-15 mob is false / empty = the bit-
    // identical structural root).
    // `woolDrops` (data 'wool_drops', strict bool, default false): the
    // mob's wool is per-INSTANCE state — it spawns with a weighted color
    // (the `wool_weights` roll, the per-mob stream's first draw), a
    // right-click with a `shears` item shears it (1-3 of the instance-
    // color wool drop, the mob goes bare for the regrow window), a right-
    // click with a DYE recolors it (Minecraft: a bare mob refuses the
    // dye), and on death it drops its instance-color wool — a bare death
    // drops none (the Minecraft sheared-sheep rule). A wool_drops mob
    // must also carry a drop entry named for the wool family + a
    // non-empty wool_weights (the strict pair — init / parse rejections
    // name it).
    bool woolDrops = false;
    // `woolWeights` (data 'wool_weights'): the spawn-time wool color's
    // weighted roll — a non-empty list of strict {color, weight}
    // entries (color = a dye color name, init-vocabulary-cross-checked
    // against the dye items; weight = an integer 1..10000; duplicate
    // colors rejected; the empty list rejected). The MobEntity
    // constructor consumes ONE per-mob-stream draw and walks the
    // cumulative weights (rollWoolColor — the attemptMobSpawn pick's
    // paradigm). MUST be paired with woolDrops (the milk pair's
    // discipline: a lone field is a dead-data typo).
    std::vector<WoolWeight> woolWeights;
};

// V2-15: a wool_drops mob's WOOL drop entry — the drop named for the
// wool dye family (the item's base name, block.hpp's kWoolDyeFamily):
// the death pass recolors it per instance (dyeVariantName) and skips it
// when the mob is bare, and shearing rolls its count range. nullptr =
// absent (a wool_drops mob without one is rejected at game init — the
// init cross-check makes this the belt).
inline const MobDrop* woolDropEntry(const MobProperties* p) {
    if (p == nullptr || !p->woolDrops) {
        return nullptr;
    }
    for (const MobDrop& d : p->drops) {
        if (d.item == kWoolDyeFamily) {
            return &d;
        }
    }
    return nullptr;
}

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
    // `label` prefixes the error messages ("mob model" for the mob files,
    // "player body model" for the UX-3 player body — the default keeps
    // every pre-UX-3 message bit-identical).
    static Result<void> parseModel(const Json& def, MobModel& out,
                                   const char* label = "mob model");

private:
    // deque: STABLE references on push (the BlockRegistry / ItemRegistry
    // pattern — a vector's reallocation would dangle every earlier
    // `at()` reference). MobEntity holds a const Mob* into this storage
    // for its whole life, so the stability is load-bearing: a test (or
    // the game) that registers several mobs and keeps early ones alive
    // reads through pointers the late pushes must not move.
    std::deque<Mob> m_mobs;
    std::unordered_map<std::string, int> m_names;
};

} // namespace mc
