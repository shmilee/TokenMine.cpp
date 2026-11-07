 // MobEntity: a data-driven hostile mob (Phase 20).
 //
 // Physics: the shared Entity AABB physics (gravity / terminal velocity,
 // sub-stepped axis moves). Unlike dropped items there is NO ground
 // friction: the AI sets the horizontal velocity every frame (exact data
 // speed), and knockback decays with its own damping while the knockback
 // window is open. ENT-4 / FLUID-3: while the AABB overlaps a water OR
 // lava cell (or the body is at the fluid's surface band) the physics
 // switches to the shared fluid model (Entity::refreshFluidState +
 // integrateFluid — the input-free twin of the player's FLUID-2 swim:
 // the passive rise to the swim level (the body's mid-axis on the
 // surface — the Q2 depth gate lets a resting body rise in a pool >= 2
 // deep, a 1-deep wade still stands), the geometric swim-level float,
 // the sink / pool-floor stand), the horizontal speed is gated by
 // kUnderwaterSpeedFactor, and the step-up (ENT-3) runs in fluid too (the
 // FLUID-3 lift of the ENT-4 ban — a wading mob steps out of a 1-deep
 // pool; a swimming mob, onGround false, cannot step up: no swim-up shore
 // climb, the documented deviation). The AI state machine is untouched —
 // chase / wander steer as usual underwater. V3-3: the walks_on_lava
// mob (the strider — the data flag) is the EXCEPTED fluid case: the
// lava's surface plane is its FLOOR (the integrateLavaSurface branch —
// the feet-on-plane walk pace at full data speed, no 0.6x underwater
// gate, the splash-less landing, the passive rise back to the plane
// from depth), the lava ignition is exempt (the fire BLOCK still
// ignites — Minecraft's strider burns on fire too), and the spawn
// scan's lava-surface branch offers it the lava's tops (the floor scan
// never would). Every non-walker's fluid path is the pre-V3-3 one
// (the flag false in the data, the unit-pinned red line).
//
// AI (deterministic: the same seed + input sequence yields the same
// trajectory). The game steps mobs with a FIXED per-frame dt (the day
// cycle's convention, see Game::kMobStepPerFrame): a moving body's
// position at frame N is then a pure function of N — the real dt varies
// with machine load and would drift a moving mob's screenshot position
// by sub-pixels between runs (the resting entities of the earlier phases
// were immune to that).
//   Wander — the default state. Every `wander_interval` (data) a seeded
//            random decision: 1 in 3 stands still, otherwise walks at
//            speed * kWanderSpeedFactor in a seeded random heading.
//   Chase  — the player is within `detect_radius` (horizontal) AND within
//            kDetectVertical in height: walk at full data speed toward the
//            player's bottom-center, facing it.
//   Attack — within `attack_range` (horizontal) AND kAttackVertical in
//            height: stand still, face the player, and deal
//            `attack_damage` through the hit callback every
//            `attack_cooldown` (the game wires it to Player::damage).
//   RangedAttack (Phase 27) — for mobs whose data `shoot_range > 0`
//            (the skeleton): the player is within `shoot_range` (horizontal)
//            → face it and fire through the shoot callback every
//            `attackCooldown` (the game spawns an arrow at the player's eye
//            with the data shoot_speed / difficulty-scaled damage). While
//            the player is inside `retreat_distance` (0 = never) the mob
//            backs away at full data speed instead of standing. Beyond
//            shoot_range (but inside detect_radius) it chases like a melee
//            mob. A ranged mob's `attack_range` is its (unused) melee
//            range — the skeleton sets it to 0, so it never melee-attacks.
//   FlyCruise (V2-14) — for mobs whose data `flying` is true (the bat —
//            the project's first flying mob): the state machine's TOP
//            branch (the data flag gate, no species switch) — a flying
//            mob never reaches the chase / attack / pathfind branches
//            below (the bat's detect_radius is 0 in the data anyway).
//            Gravity is OFF (integrate's hover baseline: the vy is SET to
//            the frame-locked bob sine every sub-step, not accumulated)
//            and the mob cruises at its data speed along a heading that a
//            per-mob stream re-rolls every `wander_interval` (the data's
//            cruise turn cadence); a wall hit (a blocked sub-step clamps
//            the move axis to zero velocity) re-rolls the heading next
//            frame (the documented deviation — MC's bat has no wall
//            avoidance, this is the natural detour). The optional data
//            `ambient_sound` / `ambient_interval` pair adds the per-frame
//            ambient roll (probability 1 / (interval * 60) of the
//            per-mob stream, played spatialized at the mob through the
//            game-wired callback — the bat's squeak).
// A dead player is neither chased nor attacked (the mob wanders).
// Passive mobs (Phase 22): detect_radius 0 (and/or attack_range 0 /
// attack_damage 0) make the chase/attack conditions never hold, so the
// mob only ever wanders — no code path distinguishes them from hostile
// mobs, the data alone decides.
// Neutral mobs (V3-3, the nether's piglin / hoglin — the data flag
// `neutral`, strictly exclusive with `hostile` at parse): the chase
// condition gains ONE anger term — the single predicate is
// `playerAlive && (neutral ? angry : true) && detect tests`, so a calm
// neutral wanders through the player (the detect test holds only while
// angry) and an ANGRY one chases / attacks exactly like a hostile (the
// data attack_range / attack_damage / attack_cooldown drive the loop).
// ANY damage source enragues (the damage() choke point — the player's
// melee, the arrow, the fire tick, the fall, the poison: deviation 7,
// the Minecraft piglin's twin), arming the kNeutralAngryDurationSeconds
// (30 s) window — a SURVIVAL-CLASS real-dt countdown (hard rule 8:
// entity age = real dt, the update's dt — the game's fixed mob step in
// a session), refreshed by every enraging hit; the timer's expiry or
// the player's death de-enrages (the respawned player finds no stale
// grudge). Every non-neutral mob keeps angry false forever: the anger
// term is a constant true there, the pre-V3-3 expression bit-identical
// (the structural-immunity root, the unit-pinned red line).
//
// Combat (player side): damage() subtracts from health; a killing hit sets
// the dead state — the game removes the mob the same frame and spawns its
// data-defined drops (see Game::updateEntities). addKnockback() applies an
// impulse and opens a kKnockbackTime window during which the AI does not
// override the velocity.
//
// Spawning / despawning are the game's job (mob_spawning.hpp): spawn cells
// come from the deterministic scan, and mobs beyond the despawn distance
// (or below the kill plane, or dead) are removed. Mobs are NOT persisted
// in saves — spawned mobs are ephemeral, as in Minecraft; their drops are
// items and DO save.
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

#include "entity/entity.hpp"
#include "entity/mob_registry.hpp"
#include "entity/mob_swing.hpp"
#include "entity/pathfind.hpp"
#include "core/status_effect.hpp"

namespace mc {

// SURV-10: the milking cooldown (Minecraft: a milked cow cannot be
// milked again for 5 minutes). Expressed in the fixed mob step's frames
// (18000 x 1/60 s = 300 s) — the frame-locked discipline (hard rule 8).
inline constexpr int kMilkingCooldownSeconds = 300;
inline constexpr int kMilkingCooldownFrames = kMilkingCooldownSeconds * 60;

// V2-15 (sheep): the sheared wool's regrow duration in the fixed mob
// step's frames (7200 x 1/60 s = 120 s) — the frame-locked discipline
// (hard rule 8), the kMilkingCooldownFrames twin. DOCUMENTED DEVIATION 3
// (the v2.16.0 design): Minecraft regrows the wool on a RANDOM grazing
// gate (1/500 tick, ~12 min expected); the project has no grass
// interaction, so the duration is a FIXED frame-locked value (the
// determinism > real-dt rule, E2E-assertable).
inline constexpr int kSheepRegrowSeconds = 120;
inline constexpr int kSheepRegrowFrames = kSheepRegrowSeconds * 60;

// V3-3 (the nether's neutral class — the piglin / hoglin): the ANGER
// duration in SECONDS (the design's value: the Minecraft piglin's
// ~enraged window class). This is a SURVIVAL-CLASS TIMER (hard rule 8:
// entity age = real dt — like the player's hunger / damage flash), so
// the countdown runs on the update's REAL dt (the game's fixed mob step
// IS that dt in a session; a test may pass any dt) — NOT a frame count.
// Any damage source enragues (the piglin's Minecraft twin, deviation 7);
// the timer's expiry or the player's death de-enragues.
inline constexpr double kNeutralAngryDurationSeconds = 30.0;

// The mob's AI state (observable for tests and the debug overlay).
enum class MobState {
    Wander, // no player in range: seeded random decisions
    Chase,  // player in range: walk toward it
    Attack, // in attack range: stand still and swing on the cooldown
    // Ranged attack (Phase 27): fire on the cooldown, back away inside
    // retreat_distance.
    RangedAttack,
    // ENT-6: following the BFS chase path (the stuck-triggered route
    // around terrain the straight chase cannot cross).
    Pathfind,
    // V3-14d: the ridden state — the AI is taken over by the rider's
    // steer (the game pushes it each frame), no player interaction.
    // Appended at the TAIL (no serialization consumer — mobs are not
    // persisted, the PITS 396 kKeyCount tail-pin discipline twin).
    Ridden,
};

// ENT-5: the safe-fall offset for mobs (Minecraft: 3 blocks — the same
// value as the player's Phase-16 rule; a fixed constant rather than a
// per-mob field: no Minecraft mob varies it, YAGNI).
inline constexpr float kMobFallDamageOffset = 3.0f;

// V2-14 (the bat — the project's first flying mob): the flight-AI named
// values. The behavior is data-flag gated (MobProperties::flying — the
// core never switches on a species); these constants are the frame-
// locked math's figures (hard rule 8) and the Minecraft-verified
// reference the bat.json data pins to (the content test asserts the
// data equals them).
// The bat's cruise speed (blocks/s) — bat.json's `speed` field is the
// data source of truth (flyCruise reads it); this is the MC-verified
// value the content pin holds it to.
inline constexpr float kBatCruiseSpeed = 1.0f;
// The hover bob: the flying mob's vy baseline is
// kBatBobAmplitude * sin(2*pi * frame / kBatBobPeriodFrames + the
// spawn-stream phase) — the amplitude in blocks/s...
inline constexpr float kBatBobAmplitude = 0.2f;
// ...and the period in the fixed mob step's frames (60 = one second at
// the 1/60 step — the frame-locked discipline: the bob at frame N is a
// pure function of N, the per-mob local frame count and the phase).
inline constexpr int kBatBobPeriodFrames = 60;

// V3-10d (the ender dragon): the dragonFlight AI's named values (the bat
// constants' shape — the data pins the breath/fireball/crystal parameters
// + the speed, the code owns the boss geometry + the frame-locked math's
// figures; the capability gate is the data dragonFlight flag, every
// pre-V3-10d mob runs none of this — the pure-new-branch red line).
// The orbit's radius (the project scale over the MC's ~40-block end
// arena: the 120 plateau fits the circle, the fog's 48 end keeps the
// dragon readable at every point — D9's 8x4 box read).
inline constexpr float kDragonOrbitRadius = 30.0f;
// The orbit's angular speed scale: the DATA speed (0.7) x this = the
// arc speed in b/s (7 b/s -> a ~36 s lap — the MC orbit's ~10 b/s
// project read, the named constant the speed data multiplies).
inline constexpr float kDragonOrbitSpeedScale = 10.0f;
// The lap-roll's STRAFE probability (the orbit lap's decision draw u1 —
// the MC's crystal-break-guaranteed strafe is the crystalDelta flag's
// job; the idle lap's strafe is the project's cadence read).
inline constexpr double kDragonStrafeLapRollP = 0.25;
// The strafe's DURATION in seconds (the fixed 3.5 s — the MC's 3-4 s
// read's mid, the code constant the kSheepRegrowFrames fixed-duration
// precedent; the data family stays the recon's 8-group surface).
inline constexpr double kDragonStrafeSeconds = 3.5;
// The perched GROWL lead-in (the MC 1.25 s audio telegraph, D12's
// roar-as-telegraph read — the fixed frame-locked duration).
inline constexpr double kDragonGrowlSeconds = 1.25;
// The altitude approach's proportional gain + climb cap (the integrate's
// dragon branch: vy = clamp((target - y) * gain) + the bob sine — a
// pure function of the frame position, the frame grid's determinism).
inline constexpr float kDragonAltitudeGain = 2.5f;
inline constexpr float kDragonMaxClimb = 8.0f;
// The perched cumulative-damage TAKEOFF threshold (the MC's 50 read —
// the perched hit-accumulator's escape trigger, D12's escape rule).
inline constexpr float kDragonPerchDamageTakeoff = 50.0f;
// The breath's player-range gate (the MC's 20-block read).
inline constexpr float kDragonBreathRange = 20.0f;
// The perch's no-player takeoff range (the MC's 150-block read).
inline constexpr float kDragonPerchPlayerRange = 150.0f;
// The consecutive-breath TAKEOFF count (the MC's 4-read).
inline constexpr int kDragonBreathTakeoffCount = 4;
// The contact damage's session cooldown (seconds — the wing sweep's
// D13 read: the MC's 0.5-s hit-window simplified to a flat 1 s, the
// no-knockback project precedent's twin).
inline constexpr double kContactDamageCooldownSeconds = 1.0;

// V3-10c (the enderman): the TELEPORT math's named values (the bat
// constants' shape — the data pins the interval pair, the code owns the
// box geometry + the stream contract's figures; the capability gate is
// the data pair's presence, every pre-V3-10c mob runs none of this).
// The eye-contact provocation's frame counter (the Minecraft 5-tick =
// 0.25 s read, the frame-locked class, hard rule 8 — the code constant
// the kNeutralAngryDurationSeconds precedent): 5 CONSECUTIVE looking
// frames provoke (an un-occluded aim ray inside the data
// eye_contact_range, the game pass feeds the per-frame bool).
inline constexpr int kEyeContactFrames = 5;
// The attempt's FIXED candidate count (the stream contract's K — the
// Minecraft 64-attempt read scaled to the project's ±16 box; a variable
// count would step the per-mob stream by the world's answer, the
// PITS 389 discipline).
inline constexpr int kTeleportAttempts = 8;
// The landing probe's downward depth (the Minecraft "probe down to the
// ground" read, bounded — the pearl's upward 0..3's mirror extension):
// from the candidate's feet, the first of these levels that holds the
// body (hasRoomAt, the strict inequality) AND no fluid cell is the
// landing. A fixed bound (not a stream draw — the contract's 3 draws
// per candidate are the two offsets + the y slice, see below).
inline constexpr int kTeleportProbeDown = 16;
// The candidate box's half-ranges (the Minecraft 64³ read's project
// scale — the fog's kEndFogEnd 48 sees a ±16 jump, the island's r 5..12
// scale: CALM = ±16 x/z, ±8 y; ENRAGED = toward the player's 12-block
// point ±8 x/z, ±4 y, the recon's project-scale read, the deviations
// recorded).
inline constexpr float kTeleportCalmH = 16.0f;
inline constexpr float kTeleportCalmV = 8.0f;
inline constexpr float kTeleportAngryH = 8.0f;
inline constexpr float kTeleportAngryV = 4.0f;
inline constexpr float kTeleportAngryAnchor = 12.0f;
// The landing cue's sound name (the GENERIC teleport action's — not a
// species switch: any teleport-capable mob plays it, the game init
// cross-checks its wav against the data capability, the S6 gate's face).
inline constexpr const char* kTeleportSound = "enderman_teleport";

// V3-10c: one TELEPORT CANDIDATE (the roll's output, the game-side
// probe's input): the candidate's feet position (bottom-center, the
// Entity convention). The roll is a PURE function of (the per-mob
// stream state, the mob's position, the player's position, the enrage
// flag) — 3 draws per candidate (u1 the x offset, u2 the z offset, n
// the y slice — the PITS 4 lattice-bit discipline: the modulus BEFORE
// any int conversion), kTeleportAttempts candidates per ATTEMPT, so an
// attempt ALWAYS consumes exactly 3 * kTeleportAttempts stream values
// whether or not the game's probe lands one (the swallowed-attempt's
// stream alignment, the savenorm arms' catch).
struct TeleportCandidate {
    Vec3 pos;
};

std::array<TeleportCandidate, kTeleportAttempts> rollTeleportCandidates(
    std::uint64_t& state, Vec3 pos, Vec3 playerPos, bool enraged);

// V3-10d: the dragon's LAP-CROSSING decision (the rollTeleportCandidates'
// pure-function twin — the stream contract's apex): on the frame the
// orbit's angle crosses a full lap, EXACTLY 2 draws are consumed,
// whether or not the decision changes the state (the swallowed-roll
// discipline, the 10c's twin): u1 the strafe roll (kDragonStrafeLapRollP)
// and u2 the perch roll (1 / (3 + live crystals — the MC's
// 1/(3+crystals) read, the crystal census's -1 never fed reads 0)). A
// strafe decision STILL consumes u2 (the fixed 2-draw contract — the
// inline else-if's 1-draw-on-strafe shape is the bug this refactor
// closes). The decision is a pure function of (the stream state, the
// crystal census) — the unit tests pin both the contract and the
// outcomes over known states.
enum class DragonLapDecision { Orbit, Strafe, Perch };
DragonLapDecision rollDragonLapDecision(std::uint64_t& state,
                                        int liveCrystals);

class MobEntity : public Entity {
public:
    // `def` is a registry entry owned by the game (which outlives every
    // entity). `rngSeed` is a per-mob deterministic seed (the game derives
    // it from the world seed + the spawn counter).
    MobEntity(const World& world, const BlockRegistry& blocks, const Mob& def,
              Vec3 pos, std::uint64_t rngSeed);

    void update(double dt) override;
    bool shouldDespawn() const override {
        return m_dead || belowKillPlane();
    }

    const Mob& def() const { return *m_def; }
    MobID typeId() const { return m_def->id; }
    float health() const { return m_health; }
    bool dead() const { return m_dead; }
    // SURV-10: the Minecraft milking cooldown (5 minutes) — after being
    // milked, the same mob cannot be milked again until it expires.
    // Frame-locked on the fixed mob step (hard rule 8: deterministic);
    // mobs are ephemeral, so it is never saved.
    bool milkReady() const { return m_milkCooldownFrames <= 0; }
    void startMilkCooldown() { m_milkCooldownFrames = kMilkingCooldownFrames; }
    // V2-15 (sheep): the WOOL instance state (the data-flag gated —
    // MobProperties::woolDrops; every non-wool mob keeps the default
    // values forever = the bit-level red line, the bat's m_flyFrame
    // twin). The state is session-only (mobs are ephemeral — zero
    // format, the milkCooldown precedent).
    // `woolColor()`: the instance's dye color name — rolled ONCE from
    // the per-mob stream's first draw at spawn (the def's wool_weights,
    // rollWoolColor), re-set by a successful dye (dyeWool). A non-wool
    // mob's stream is untouched (the constructor consumes nothing).
    bool woolDrops() const { return m_def->properties.woolDrops; }
    const std::string& woolColor() const { return m_woolColor; }
    // `isBare()`: sheared (the wool is off; the regrow window is
    // counting). `woolRegrowFrames()`: the remaining regrow countdown
    // (the fixed 1/60 step's frames, the milkCooldown's integer
    // discipline).
    bool isBare() const { return m_bare; }
    int woolRegrowFrames() const { return m_woolRegrowFrames; }
    // The render variant tile of the wool box (the per-entity variant
    // mesh's key, see Game::drawMobs): bare = the skin tile, woolly =
    // the instance-color dye variant (the wool family's EXISTING block
    // tiles, the 15-color reuse — zero new texture).
    std::string woolVariantTile() const {
        return m_bare ? std::string(kWoolBareTile)
                      : dyeVariantName(kWoolDyeFamily, m_woolColor);
    }
    // Shear (the game calls it on a successful right-click with a
    // `shears` item on a woolly wool-drops mob): rolls the wool COUNT
    // from the mob's own wool drop entry (the per-mob stream — the
    // rollDropCount twin, so the same run yields the same drops) and
    // sets the bare state + the full regrow window (kSheepRegrowFrames,
    // deviation 3's fixed duration). Returns the count (1..3 on the
    // shipped sheep); 0 = not eligible (already bare / no wool drop
    // entry — defensive; the state is UNCHANGED on a 0).
    int shearWool();
    // V3-14c (piglin bartering): the trade roll — the game calls it on a
    // successful right-click with the data `barter_input` item held.
    // STATELESS (the mob is untouched: no damage, no cooldown — a neutral
    // barter never enrages, the milk non-enrage's structural twin; the
    // game owns the gold consumption + the ground drop + the sound).
    // Draws the OUTPUT from the weighted table (rollBarterOutput, one
    // uniform01) + the COUNT from the entry's range (rollDropCount, one
    // nextMix; the fixed-count entries draw nothing — the degenerate
    // discipline). Returns (item name, count); an empty item name = not
    // a bartering mob (defensive; init rejects the pairing, the mob data
    // gate in tryBarter keeps this unreachable) with NO stream draw.
    std::pair<std::string, int> barterItem();
    // Dye (the game calls it on a successful right-click with a dye on
    // a woolly wool-drops mob): sets the instance color (the held dye's
    // color name — the DATA, so NO stream draw: the recolor is a pure
    // function of the input, the block recolor's twin). Refused when
    // bare (Minecraft: a sheared mob cannot be dyed) or non-wool (the
    // flag gate).
    void dyeWool(const std::string& color);
    MobState state() const { return m_state; }
    // ENT-6: the active chase path (test / debug observability).
    bool pathActive() const { return !m_path.empty(); }
    // V3-10d: the orbit's angle re-anchor from the current position
    // (the strafe/perch takeoff's pure geometry — the circle point's
    // angle over the orbit center, zero stream, [0, 2pi)); the test /
    // debug observability's state()'s twin.
    float orbitAngleFromPosition() const;
    // Facing in the camera yaw convention (0 = -Z); the render model
    // rotates by this.
    float facingYaw() const { return m_yaw; }

    // ENT-1: the limb-swing phase (0..kSwingPhaseCount-1) — a pure
    // render-layer cosmetic state (session-local, NEVER saved: mobs are
    // ephemeral, see the file head). Advanced once per update() frame
    // (frame-locked, hard rule 8): kSwingStep while the mob has strict
    // horizontal displacement this frame, kSwingDecay toward 0 (the
    // identity pose) while standing. Types without swing data never
    // advance (structural: their render path stays the static mesh).
    int swingPhase() const { return m_swingPhase; }

    // ENT-4: this tick's fluid state (the strict AABB overlap of at least
    // one water OR lava cell — the player's inWater()/inLava() union; the
    // fire system's checks below stay FRESH per the P57 pattern, a
    // different read). Exposed for tests and the debug overlay.
    bool inFluid() const { return m_inFluid; }
    // V3-3: the NEUTRAL class state (the data flag
    // MobProperties::neutral — every non-neutral mob keeps false / 0
    // forever = the bit-level red line, the bat's m_flyFrame twin):
    // `neutral()` the class flag itself; `angry()` the enrage state
    // (any damage source sets it for kNeutralAngryDurationSeconds real
    // dt — the chase condition's anger gate, the design's single
    // predicate; the player's death de-enragues instantly).
    bool neutral() const { return m_neutral; }
    bool angry() const { return m_angry; }
    // V3-3: the WALKS-ON-LAVA state (the data flag
    // MobProperties::walksOnLava — every other mob is false = the
    // pre-V3-3 fluid semantics bit-identically): `onLavaSurface()` is
    // this tick's judgment that the lava's surface plane is this body's
    // FLOOR (the flag + the at-surface band + the immersion kind LAVA +
    // a live plane) — the fire system's ignition exemption, the
    // underwater speed gate's exemption and the integrate's walk-pace
    // branch all read this one predicate (the single source, the
    // refreshFluidState pattern).
    bool onLavaSurface() const;

    // Subtract damage; returns true when THIS hit killed the mob.
    bool damage(float amount);
    // V3-10d: ADD healing (the end crystal's 1 HP / 30-frames cadence —
    // the damage() twin with the sign flipped): the amount is clamped
    // at the data health cap (the crystal cannot over-heal the boss,
    // the MC 200-cap read). A dead mob's heal is a no-op (the damage's
    // m_dead guard's twin — the corpse cannot be revived). Returns
    // true when any health was actually added (the heal beam's render
    // gate — a full-health dragon's crystals glow but don't beam).
    bool heal(float amount);
    // Knockback impulse; suppresses AI steering for kKnockbackTime.
    void addKnockback(Vec3 impulse);

    // Fire system (Phase 58, v2: the fire reaches mobs — Minecraft's lava
    // ignites every entity; Phase 73, v3: the FIRE BLOCK does too — the
    // block twin of the lava ignition). The game arms the mob at spawn
    // from the SAME fire definition + config duration as the player (the
    // ignites_on_lava effect + player.fire_duration_seconds); an unarmed
    // mob — an asset set without the fire effect — never burns
    // (structural, the P57 pattern). While in the lava OR strictly
    // overlapping a fire-block cell (the constructor-resolved id, the
    // P70 player pattern — no fire block registered = structurally
    // inert) the burn re-ignites (the duration resets every step = a
    // continuous burn); after leaving, the remaining time counts down
    // (the lingering burn). Water extinguishes instantly, BEFORE the tick
    // (zero burn damage in water; water beats both, the P57 rule); V2-4:
    // the rain extinguishes the same way (the inWater ∨ raining
    // extension — setRaining, no roof-shade check = the documented
    // deviation). The burn ticks run on the fixed mob step
    // (deterministic — hard rule 8:
    // mobs tick at the fixed 1/60 frame step) with the Phase 46 effect
    // semantics (the expiry step does not tick). Because the step is the
    // exact 1/60 frame grid, the tick cadence is kept in INTEGER frame
    // units (tick k lands on burning frame k * interval*60 — no float
    // phase drift; the game init validates the ignites effect's interval
    // is a whole number of frames so this is exact).
    void armFire(double tickInterval, float tickAmount, double duration);
    bool burning() const { return m_fireRemaining > 0.0; }

    // SURV-5: the mob's active status effects (the shared pure
    // StatusEffects class — the arrow's effect lands here). Apply gate:
    // the effect must be mob-accepting (`affects_mobs`) AND not undead-
    // exempted against an undead mob (Minecraft: poison does not affect
    // the undead; the wither kills them). Both gates are defensive at
    // apply time (the data combinations are cross-checked at game init
    // where they make sense). A dead mob refuses the apply (it despawns
    // this frame anyway).
    // SURV-6: `tier` (0-based; the grant's tier — 0 = the pre-SURV-6
    // value, the trailing default keeps every legacy call site
    // bit-identical).
    void applyEffect(int effect, double duration, const EffectRegistry& effects,
                     int tier = 0);
    // SURV-5: advance the mob's active effects by `dt` (the game calls
    // this once per fixed mob step, right after update() — hard rule 8:
    // frame-locked mob time). Damage ticks subtract through damage()
    // with the MOB floor (`min_health_mob`, defaulting to the effect's
    // player `min_health` — poison ships 0: a mob CAN be poisoned to
    // death, the player keeps its 1-HP floor); a killing tick leaves the
    // mob dead (the existing damage/despawn/drop path, unchanged). Heal
    // ticks are a NO-OP (MobEntity has no regeneration — the documented
    // deviation: the project has no mob-heal need). count == 0 fast-
    // exits (a mob without effects costs nothing — the structural-
    // immunity basis).
    void updateStatusEffects(double dt, const EffectRegistry& effects);
    bool hasEffects() const { return m_effects.count() > 0; }
    const StatusEffects& statusEffects() const { return m_effects; }

    // Per-frame player state; the game sets it right before update()
    // (bottom-center position + whether the player can be attacked).
    void setPlayer(Vec3 pos, bool alive) {
        m_playerPos = pos;
        m_playerAlive = alive;
    }
    // V2-4: the per-frame weather flag (the setPlayer pattern — the game
    // sets it right before update()): rain extinguishes the burning mob
    // the way water does (the inWater ∨ raining extension of the P57
    // instant-extinguish path; the re-ignition runs first, so a mob on a
    // fire block in the rain stays out — the water-beats-fire order. No
    // roof-shade check = the documented deviation; default false = the
    // pre-V2-4 water-only semantics bit-identical).
    void setRaining(bool raining) { m_raining = raining; }
    // V3-14d: the ridden session state (the player's m_carried twin —
    // SESSION state, never persisted: mobs are not saved, so the ride
    // cannot outlive the session, the portal-cooldown / flight
    // precedent). The game sets it on mount/dismount; while ridden the
    // AI is the riderSteer takeover (the wander is suspended) and the
    // separation pass skips this mob (the rider's box overlaps the
    // mount by design — the carry snap owns the relative pose).
    void setRidden(bool ridden) { m_ridden = ridden; }
    bool ridden() const { return m_ridden; }
    // V3-10d (the ender dragon): the CRYSTAL FEED (the setPlayer pattern
    // — the game pushes it each frame BEFORE the mobs' update): the
    // live crystal count (the 10 fixed crystal slots' census — the
    // dragon's perched probability / the heal-source read) + the
    // changed-this-frame flag (a crystal shattered this frame — the MC
    // guaranteed-strafe trigger, the orbit lap's crystalDelta gate). A
    // non-dragon mob's feed is inert (the dragonFlight branch never
    // runs — the data gate, the bit root).
    void setCrystalsAlive(int count, bool changed) {
        m_crystalsAlive = count;
        m_crystalDelta = changed;
    }
    int crystalsAlive() const { return m_crystalsAlive; }
    // V3-10d: the ANCHOR FEED (the setPlayer pattern over geometry —
    // the game pushes the end-origin's perch + orbit-center constants
    // when it spawns the dragon, the entity layer never reads the core
    // layer's end_portal.hpp, the architecture's dependency direction).
    // Zero defaults: an unfed dragon's orbit degenerates to the origin
    // (structurally unreachable — the game feeds at spawn, the belt).
    void setDragonAnchors(Vec3 perch, Vec3 orbitCenter) {
        m_dragonPerch = perch;
        m_dragonOrbitCenter = orbitCenter;
    }
    // V3-10d: the CONTACT cooldown's frame step + hit latch (the game's
    // overlap pass calls tickContactCooldown each frame for a
    // contact_damage > 0 mob, and latchContactDamage on the hit frame —
    // a contact_damage-0 mob's field never moves, the bit root).
    void tickContactCooldown(double dt) {
        if (m_contactCooldown > 0.0) {
            m_contactCooldown -= dt;
        }
    }
    bool contactDamageReady() const { return m_contactCooldown <= 0.0; }
    void latchContactDamage() {
        m_contactCooldown = kContactDamageCooldownSeconds;
    }
    // The rider's steer (the setPlayer pattern — the game pushes it
    // each frame BEFORE update(): `dir` = the rider's desired heading
    // (a unit vector, or zero when the rider pushes no direction key)
    // and `forward` = whether the rider pushes forward (W held).
    // Zeroed by the game on dismount.
    void setRiderSteer(Vec3 dir, bool forward) {
        m_riderDir = dir;
        m_riderForward = forward;
    }
    // The separation pass's opt-out (the projectile's twin): a ridden
    // mount is pushed neither by the rider nor by other mobs.
    bool collidesWithEntities() const override { return !m_ridden; }
    void setHitCallback(std::function<void(float)> cb) {
        m_hitPlayer = std::move(cb);
    }
    // Ranged mobs (Phase 27): the AI invokes this to fire; the game
    // spawns the arrow (data shoot_speed, difficulty-scaled damage) aimed
    // at the player's eye. The callback reads the mob through the pointer
    // the game captured at spawn, so the mob must outlive its callback.
    void setShootCallback(std::function<void()> cb) {
        m_shootPlayer = std::move(cb);
    }
    // V2-14: the ambient sound (the bat's squeak — the data-driven general
    // mechanism, no species special-casing): the game wires this at spawn
    // for a mob with a data `ambient_sound` (the setShootCallback pattern);
    // the flyCruise per-frame roll invokes it with the mob's CURRENT
    // position (the game plays the data sample spatialized there — the
    // existing 16-block cutoff). A mob without the data field (or the
    // unwired callback) never invokes it (structural: the pre-V2-14
    // mobs' streams are untouched).
    void setAmbientSoundCallback(std::function<void(Vec3)> cb) {
        m_ambientSound = std::move(cb);
    }
    // V3-10d (the ender dragon): the GROWL callback (the breath's 1.25 s
    // audio telegraph — the setAmbientSoundCallback's seam shape; the
    // game plays the data `dragon_growl` spatialized at the mob). Wired
    // for a dragon_flight mob only (the data gate — the bit root).
    void setGrowlCallback(std::function<void(Vec3)> cb) {
        m_growlSound = std::move(cb);
    }
    // V3-10d: the BREATH callback (the perched breath's cloud trigger —
    // the game spawns the BreathCloud at the PLAYER's feet with the data
    // breath_duration / breath_radius; the mob owns only the timing).
    void setBreathCallback(std::function<void()> cb) {
        m_breath = std::move(cb);
    }
    // V3-10d: the ASCENSION state (the death ritual's session counters —
    // the game's dragon-death pass drives the corpse: -1 = alive,
    // >= 0 = the frames since the m_dead frame. The egg/XP bookkeeping
    // ran at the m_dead frame (the game side); this is the render/
    // removal timer's witness (the despawn pass's delay gate reads it).
    int ascensionFrames() const { return m_ascensionFrames; }
    void setAscensionFrames(int f) { m_ascensionFrames = f; }
    // V3-10c (the enderman): the eye-contact FEED (the game pass calls it
    // once per frame, BEFORE the mob's own update — the fixed slot:
    // `looking` = the aim ray hits this mob's full AABB inside the data
    // eye_contact_range unoccluded by the aim block). The integer frame
    // counter (the frame-locked class, hard rule 8): 5 consecutive true
    // frames provoke (m_angry + the full 30 s window, the damage-
    // provocation's state twin); a single false frame RESETS it (the
    // line-of-sight break's de-provocation — the provocation does not
    // persist across a blink, the Minecraft read). Returns true on the
    // PROVOKING frame (the game's DEBUG log / cue); false otherwise
    // (already angry, not yet, or a non-eye-contact mob — the flag gate
    // reads the DATA, the counter of a range-0 mob never moves). ZERO
    // stream draws (pure geometry + counting — the ambient roll's
    // contrast: the provocation's cadence is the PLAYER's aim, not the
    // mob's stream).
    bool eyeContactFeed(bool looking);
    // V3-10c: the TELEPORT capability gate (the data pair's presence —
    // the eyeContactRange's numeric-gate shape): a mob with no interval
    // pair runs none of the teleport state (the bit-identical root).
    bool canTeleport() const {
        return m_def->properties.teleportIntervalMin > 0.0;
    }
    // V3-10c: the game wires the TELEPORT EXECUTION at spawn (the
    // setShootCallback's seam — the mob rolls the candidates through
    // its own stream, the game probes + lands them: the catch-up disk,
    // the downward hasRoomAt / fluid probe, the setPosition + fall
    // re-anchor, the landing's sound + DEBUG line). Returns true when a
    // candidate landed (the mob is MOVED — the stream tail re-roll
    // still runs, the contract's fixedness). Unwired = the attempt
    // rolls and swallows (structurally unreachable: the game wires
    // every canTeleport mob, the belt).
    void setTeleportCallback(
        std::function<bool(const std::array<TeleportCandidate,
                                             kTeleportAttempts>&)> cb) {
        m_teleport = std::move(cb);
    }
    // V3-10c: the post-landing mob-side RESET (the game calls it after a
    // successful probe): the fall tracking re-anchors to the landing
    // (the pearl's fall-cancel twin — the drop from the candidate's air
    // is NOT damage) + the stuck window / path drop (a teleported chaser
    // re-measures its progress from the landing — the ENT-6 anchor's
    // stale-state guard).
    void finishTeleport();
    // V3-10c: the CARRY session state (the data flag carriesBlocks's
    // gate; every non-carry mob keeps invalid forever = the bit root).
    // The carried block's id (the ITEM is the block's own — the death
    // drops the block item, the instance state, the wool color's
    // precedent class: session-only, never saved).
    bool carriesBlocks() const { return m_def->properties.carriesBlocks; }
    BlockID carried() const { return m_carried; }
    void setCarried(BlockID id) { m_carried = id; }
    // Head height for ranged shots (80% of the model height): the arrow
    // spawn point (the game aims from here at the player's eye).
    Vec3 eyePosition() const {
        return position() +
               Vec3 {0.0f, m_def->model.height * kEyeHeightFraction, 0.0f};
    }

    // Phase 38: roll one death-drop entry's count in [minCount, maxCount]
    // (both inclusive) with the mob's seeded splitmix64 stream — the same
    // state-advancing flow as the wander decisions. Deaths happen on a
    // deterministic frame (fixed step), so the roll is reproducible
    // run-to-run for the same world seed + input sequence. A degenerate
    // range (min == max) returns the constant without advancing the
    // stream, so fixed-count drops keep the pre-Phase-38 stream exactly.
    int rollDropCount(int minCount, int maxCount);

private:
    void integrate(float h) override;
    // V2-14: the flight wall-hit hook (the base no-op for the other
    // entities' sub-steps): a blocked sub-step of a FLYING mob that
    // clamped a horizontal axis to zero velocity = a wall (the AI sets
    // both horizontal components to the nonzero cruise vector every
    // frame, and only the collision resolve zeroes one) — the next
    // frame re-rolls the heading (the documented deviation 3: no
    // pathfinding, the natural detour). A ceiling / floor press (the y
    // clamp) leaves the horizontal pair untouched: no re-roll.
    void afterSubstep(bool blocked) override;
    void runAi(double dt);
    // V3-3: the lava-surface walk pace (the strider — the data-flag
    // branch of integrate's fluid case, the flyCruise's pure-new-branch
    // twin): the lava's surface plane is this body's FLOOR — the feet
    // snap onto it (the swim-level snap's surface twin, the fixed point),
    // a body below the plane rises to it (the passive-rise twin, the
    // surface instead of the swim level), a body falling onto it from
    // above LANDS (the feet plane, the splash-less landing). NO GRAVITY
    // in the branch (the integrateFluid's "no gravity in fluid" twin).
    // Every non-walksOnLava mob's integrate path is bit-identical (the
    // gate reads the data flag).
    void integrateLavaSurface(float h);
    // V2-14: the flyCruise branch (the design: runAi's TOP, gated on the
    // data flag def.flying — a pure new branch, every non-flying mob
    // runs the pre-V2-14 state machine bit-identically). The cruise
    // steering (data speed along the per-mob-stream heading, re-rolled
    // every wander_interval) + the ambient roll (the per-frame 1 /
    // (interval * 60) draw, played spatialized at the mob). The hover
    // baseline itself is integrate's job (the gravity gate).
    void flyCruise(const MobProperties& p, double dt);
    // V3-14b: the flying RANGED attacker's branch (the ghast — the
    // bat's hover cruise fused with the skeleton's ranged branch):
    // inside the shot's context (player alive + visible + within the
    // shoot range) it holds the hover position (zero horizontal
    // velocity — the integrate's bob baseline keeps the vertical) and
    // fires on the cooldown through the game's shoot callback (the
    // skeleton's seam — the LOS gate + the launch live there); outside
    // it, the bat's cruise verbatim. Gated on the data (p.flying &&
    // p.shootRange > 0) — the bat (shootRange 0) runs flyCruise
    // bit-identically, the pre-V3-14b red line.
    void flyShoot(const MobProperties& p, double dt);
    // V3-10d: the DRAGON's boss AI (the ender dragon — the runAi's
    // TOP-OF-THREE flying branch, gated on p.dragonFlight BEFORE
    // flyShoot/flyCruise — the pure-new-branch red line, the
    // flyShoot's twin): the three-state cycle over the existing
    // MobState vocabulary (zero new state — 10c R12's discipline):
    //   Wander = the ORBIT (a circle of kDragonOrbitRadius around the
    //   pulled end-origin — the player-far offset keeps the fight
    //   engaged; the lap-crossing frame consumes the fixed 2-draw
    //   decision — u1 the strafe roll, u2 the perched roll 1/(3 +
    //   live crystals), the stream contract);
    //   RangedAttack = the STRAFE (face the player, the lateral
    //   drift, the fireball on the data cooldown through the shoot
    //   callback — the dragon_shoot's launcher, the game's seam);
    //   Attack = the PERCH (straight-in at the end-exit anchor, the
    //   growl telegraph -> the breath cloud callback at the player's
    //   feet; the takeoff's three conditions — 4 consecutive breaths /
    //   no player in 150 / the perched cumulative damage > 50 — the
    //   MC Take-off/Escape reads). The AMBIENT roll (1 draw, every
    //   frame) keeps the stream aligned under all three states.
    //   Vertical: m_targetY is the AI's altitude setpoint; integrate's
    //   dragon branch approaches it proportionally (the bat's bob
    //   baseline is added on — the hover feel). Gated on the data
    //   (p.flying && p.dragonFlight) — the ghast (shootRange > 0) runs
    //   flyShoot bit-identically, the bat flyCruise, every ground mob
    //   its state machine (the pre-V3-10d red line).
    void dragonFlight(const MobProperties& p, double dt);
    // V3-14d: the ridden mob's AI takeover (the rider's steer replaces
    // the wander — the runAi's TOP branch, gated on the SESSION state
    // m_ridden, the flyCruise pure-new-branch twin): forward = face
    // the rider's heading + walk at the data speed along it; released
    // = the kRideDecel ease-out to the kRideStopEps stand. The AMBIENT
    // roll is kept (the per-mob stream contract — PITS 403⑤: the
    // hiss plays while ridden, the stream stays aligned).
    void riderSteer(const MobProperties& p);
    // V2-14: the frame-locked bob (a pure function of the local frame
    // count + the spawn-stream phase — the vy the flying integrate SETS,
    // the hover's baseline, not an accumulation).
    float bobVelocity() const;
    // V3-10c: the teleport interval's re-roll (ONE per-mob-stream draw:
    // lerp over the data pair — the spawn's first draw + every
    // post-attempt tail draw; the stream contract's fixed 25-draw
    // attempt = 8 candidates x 3 + this 1).
    double rollTeleportInterval();
    // ENT-6: the chase-path helpers (all private, all frame-locked).
    void stepAlongPath(const MobProperties& p, const Vec3& pos, double dt);
    void tryPathfind(const Vec3& pos);
    // The node the mob is standing on (invalid off the integer lattice:
    // stairs/slab floors, mid-air).
    PathNode currentStandableNode(const Vec3& pos) const;
    PathNode playerTargetNode() const;
    int bodyCells() const;

    const Mob* m_def;
    float m_health = 0.0f;
    bool m_dead = false;
    // SURV-10: the remaining milking cooldown in FRAMES (the fixed mob
    // step — integer counting, the Phase 58 off-by-one discipline).
    int m_milkCooldownFrames = 0;
    // V2-15: the wool instance state (all gated on
    // def.properties.woolDrops — a non-wool mob keeps "" / false / 0
    // forever and consumes NO stream draw: the constructor's first draw
    // is the wool roll only, the bat's m_bobPhase first-draw twin).
    // `m_woolColor`: the dye color name (the spawn roll; a dye re-sets
    // it). `m_bare`: sheared (the wool off until the regrow expires).
    // `m_woolRegrowFrames`: the remaining regrow countdown (frames).
    std::string m_woolColor;
    bool m_bare = false;
    int m_woolRegrowFrames = 0;
    // V3-3: the NEUTRAL class state (all gated on
    // def.properties.neutral — a non-neutral mob keeps false / 0
    // forever and the chase predicate's anger term stays false: the
    // bit-level red line, the m_bare twin). `m_neutral`: the class flag
    // (resolved once — the m_fireBlockId pattern). `m_angry`: enraged
    // (any damage source — the damage() hook, the design's deviation 7:
    // the player / fire / fall alike, the Minecraft piglin's twin).
    // `m_angryTimer`: the remaining enrage window in SECONDS, a
    // SURVIVAL-CLASS real-dt countdown (hard rule 8: entity age = real
    // dt — the update's dt, the game's fixed mob step in a session),
    // reset to kNeutralAngryDurationSeconds on every enraging hit.
    bool m_neutral = false;
    bool m_angry = false;
    double m_angryTimer = 0.0;
    // V3-10c (the enderman): the eye-contact frame counter (the
    // kEyeContactFrames provocation count — 0 for a range-0 mob, the
    // bit root; reset by a single un-looking frame, the blink read).
    int m_eyeContactFrames = 0;
    // V3-10c: the TELEPORT state (all gated on canTeleport() — the data
    // pair's presence; a non-teleport mob keeps false / 0 forever and
    // consumes NO stream draw: the constructor's first interval roll is
    // a canTeleport mob's only new spawn draw, the bat's bobPhase twin).
    // `m_wantTeleport`: the HIT-requested attempt (damage() sets it, the
    // update's slot consumes it BEFORE the timer — the stream order
    // pinned; it never touches the stream itself, the call-site
    // frame-ordering's reason). `m_teleportTimer`: the random interval's
    // remaining SECONDS (the real-dt countdown, the wanderTimer's
    // domain — the fixed 1/60 step makes it frame-exact); a re-roll
    // consumes ONE per-mob-stream draw (lerp over the data pair).
    // `m_carried`: the carried block (carriesBlocks gate, the wool
    // color's instance-state class).
    bool m_wantTeleport = false;
    double m_teleportTimer = 0.0;
    std::function<bool(const std::array<TeleportCandidate,
                                         kTeleportAttempts>&)> m_teleport;
    BlockID m_carried = BlockID::invalid();
    // ENT-1: the limb-swing phase (see swingPhase()). Spawn = 0 (the
    // identity pose); render-only, never saved.
    int m_swingPhase = 0;
    // ENT-5: fall-damage tracking (the Player's Phase-16 rule mirrored).
    // Mobs are ephemeral (never saved) — m_fallStartY is pinned once in
    // the constructor at spawn; there is no save restore to re-pin from.
    float m_fallStartY = 0.0f;
    bool m_wasOnGround = false;
    Vec3 m_playerPos = Vec3::zero();
    bool m_playerAlive = true;
    // V3-14d: the ridden session state + the rider's steer (the game
    // pushes both — the ride cannot outlive the session, mobs are not
    // persisted).
    bool m_ridden = false;
    Vec3 m_riderDir = Vec3::zero();
    bool m_riderForward = false;
    // V2-4: the weather flag (the setRaining per-frame feed — rain
    // extinguishes the burn, see the setter). Default false = the
    // pre-V2-4 water-only semantics.
    bool m_raining = false;
    MobState m_state = MobState::Wander;
    float m_yaw = 0.0f;
    double m_wanderTimer = 0.0;   // <= 0: decide this frame
    double m_attackCooldown = 0.0;  // seconds until the next hit
    double m_knockbackTime = 0.0;   // seconds of AI suppression left
    std::uint64_t m_rng = 0;        // splitmix64 state (per-mob)
    std::function<void(float)> m_hitPlayer;
    std::function<void()> m_shootPlayer;  // ranged fire (Phase 27)
    // V2-14: the ambient sound (the game-wired play — the
    // setShootCallback pattern; empty = no ambient, the pre-V2-14
    // structural root).
    std::function<void(Vec3)> m_ambientSound;
    // V3-10d: the dragon's two extra cues (the setAmbientSoundCallback's
    // twin seams — empty = the pre-V3-10d bit root for every other mob).
    std::function<void(Vec3)> m_growlSound;
    std::function<void()> m_breath;
    // V2-14: the flight state (all gated on def.flying — a non-flying
    // mob keeps m_flyFrame 0 forever and never reads the phase).
    // `m_flyFrame`: the per-mob local frame count (incremented once per
    // update — the bob's time base on the fixed 1/60 step, hard rule 8).
    // `m_bobPhase`: the bob's initial phase (radians), rolled ONCE from
    // the per-mob stream in the constructor (the stream's first draw —
    // the PITS-30 advance discipline; a non-flying mob's stream is
    // untouched = the pre-V2-14 trajectories bit-identical).
    int m_flyFrame = 0;
    float m_bobPhase = 0.0f;
    // V3-10d: the DRAGON's session state (all gated on
    // def.dragonFlight — a non-dragon mob keeps every field at its
    // default forever and the dragonFlight branch never reads them:
    // the bit root). `m_crystalsAlive` / `m_crystalDelta`: the game's
    // per-frame feed (setCrystalsAlive — -1 = never fed, the orbit's
    // perched roll reads max(0, ...) over it). `m_targetY`: the
    // altitude setpoint integrate's dragon branch approaches.
    // `m_orbitAngle` / `m_orbitLaps`: the circle's phase + the last
    // crossed lap (the lap-frame's fixed 2-draw decision trigger —
    // pure geometry, zero stream). `m_strafeTimer`: the strafe's
    // remaining seconds. `m_perchArrived`: the perch's arrival latch.
    // `m_breathState` (0 = waiting / 1 = growling / 2 = breathing) +
    // `m_breathTimer` (the state's remaining seconds) + `m_breathCount`
    // (the consecutive-breath takeoff counter). `m_perchDamage`: the
    // perched cumulative damage (the takeoff's 50-read — damage()
    // accumulates it for a dragon mob, every state). `m_ascension-
    // Frames`: the death ritual's timer (-1 = alive).
    int m_crystalsAlive = -1;
    bool m_crystalDelta = false;
    Vec3 m_dragonPerch = Vec3::zero();
    Vec3 m_dragonOrbitCenter = Vec3::zero();
    float m_targetY = 0.0f;
    double m_orbitAngle = 0.0;
    int m_orbitLaps = 0;
    double m_strafeTimer = 0.0;
    bool m_perchArrived = false;
    int m_breathState = 0;
    double m_breathTimer = 0.0;
    int m_breathCount = 0;
    float m_perchDamage = 0.0f;
    int m_ascensionFrames = -1;
    // V3-10d: the CONTACT damage's 1-second session cooldown (the game's
    // overlap pass decrements it per frame and the hit zeroes it — the
    // wing sweep's D13 read; a contact_damage-0 mob's field never moves,
    // the bit root).
    double m_contactCooldown = 0.0;

    // ENT-6: the active chase path (empty = none — the straight chase).
    // Ephemeral like the mob (never saved — zero format). The stuck
    // window measures horizontal progress over kStuckWindowSeconds;
    // below the factor of the expected it triggers a pathfinding attempt
    // (throttled by the lost cooldown after a failed one).
    std::vector<PathNode> m_path;
    std::size_t m_pathIdx = 0;
    double m_repathT = 0.0;
    double m_nodeT = 0.0;
    double m_stuckWindowT = 0.0;
    Vec3 m_stuckAnchor = Vec3::zero();
    double m_lostCooldown = 0.0;

    // Fire system (Phase 58) state: armed by the game at spawn (the fire
    // effect's tick parameters + the config duration). `remaining` counts
    // down the lingering burn; the tick phase is on the integer frame grid
    // (`frames` = burning steps so far, `due` = the next tick's frame,
    // `intervalFrames` = frames per tick — exact, init-validated).
    // `m_fireBlockId` (Phase 73): the fire block, constructor-resolved
    // (BlockID::invalid when the asset set has none — the contact check
    // is then structurally inert, the P70 player pattern).
    BlockID m_fireBlockId = BlockID::invalid();
    bool m_fireArmed = false;
    float m_fireAmount = 0.0f;
    double m_fireDuration = 0.0;
    double m_fireRemaining = 0.0;
    int m_fireIntervalFrames = 0;
    int m_fireFrames = 0;
    int m_fireDueFrames = 0;

    // SURV-5: the mob's active status effects (the shared pure class,
    // the player's m_playerEffects twin). Ephemeral like the mob itself
    // (never saved — zero format); count 0 = the update fast exit.
    StatusEffects m_effects;

    // Shared fall (same numbers as dropped items / player).
    static constexpr float kGravity = 32.0f;
    static constexpr float kTerminalVelocity = 38.0f;
    // Wander walks at half the chase speed (named, not magic).
    static constexpr float kWanderSpeedFactor = 0.5f;
    // Chase/attack only at comparable height (vertical band, blocks).
    static constexpr float kDetectVertical = 2.0f;
    // V3-14d: the ride's release decay — the strider eases to a stop
    // after the rider lets go of W (the player's ENT-7 exponential
    // decay's frame-locked twin: per fixed 1/60 step, the speed
    // factor — a release at any frame decays identically every run).
    static constexpr float kRideDecel = 0.90f;
    // V3-14d: the ride's stop epsilon (the player's :382-387 twin —
    // below it the velocity is zeroed, the strider stands).
    static constexpr float kRideStopEps = 0.02f;
    static constexpr float kAttackVertical = 1.0f;
    // Knockback window: the impulse decays (kKnockbackDamping per second)
    // while the AI holds off; then steering resumes.
    static constexpr double kKnockbackTime = 0.3;
    static constexpr float kKnockbackDamping = 6.0f;
    // The ranged-shot origin as a fraction of the model height (the head).
    static constexpr float kEyeHeightFraction = 0.8f;
    // ENT-4: the underwater horizontal speed gate — the AI steering AND
    // the knockback slide are multiplied by this while in fluid (or at
    // its surface, the player's m_atSurface swim-pace analog). A named
    // constant, not a per-mob data field (documented YAGNI deviation:
    // every mob swims at the same pace, like the player's single
    // swimSpeed).
    static constexpr float kUnderwaterSpeedFactor = 0.6f;
    // ENT-6: stuck-triggered pathfinding tuning (the chase state only —
    // the wander / attack / ranged steering is untouched, the documented
    // deviation). All frame-locked seconds / factors (hard rule 8).
    static constexpr double kStuckWindowSeconds = 0.5;
    static constexpr float kStuckProgressFactor = 0.25f;
    static constexpr double kLostCooldownSeconds = 1.0;
    static constexpr double kRepathSeconds = 0.5;
    // The model can be wrong (the shared step-up's +X/+Z single-cell
    // quirk makes a planned step infeasible): a node not reached within
    // this time aborts the path (the straight chase resumes, throttled).
    static constexpr double kNodeTimeoutSeconds = 2.0;
};

} // namespace mc
