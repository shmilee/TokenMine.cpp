#include "entity/mob_entity.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "world/noise.hpp"

namespace mc {

namespace {

// Advances the splitmix64 stream and returns the next mixed value (the
// standard stream: state += gamma, result = mix(state)).
std::uint64_t nextMix(std::uint64_t& state) {
    state += 0x9E3779B97F4A7C15ULL;
    return splitmix64(state);
}

// 53-bit uniform in [0, 1) from the next stream value.
double uniform01(std::uint64_t& state) {
    return static_cast<double>(nextMix(state) >> 11) /
           static_cast<double>(1ull << 53);
}

} // namespace

MobEntity::MobEntity(const World& world, const BlockRegistry& blocks,
                     const Mob& def, Vec3 pos, std::uint64_t rngSeed)
    : Entity(world, blocks, pos, def.model.width, def.model.height),
      m_def(&def),
      m_health(def.properties.health),
      m_rng(rngSeed),
      // Fire v3 (Phase 73): the fire-block contact source, resolved once
      // (invalid = no fire block registered = structurally inert, the
      // P70 player pattern).
      m_fireBlockId(blocks.idForName("fire")) {
    // ENT-5: pin the fall tracking to the spawn height (a mob spawned
    // mid-air would otherwise credit the drop to a stale start).
    m_fallStartY = pos.y;
    // V2-14: the hover's initial phase (the bat — the per-mob stream's
    // FIRST draw, the PITS-30 advance discipline). A non-flying mob
    // consumes nothing here: its stream stays the pre-V2-14 one (the
    // bit-identical red line).
    if (def.properties.flying) {
        m_bobPhase =
            static_cast<float>(uniform01(m_rng) * 2.0 * std::numbers::pi);
    }
    // V2-15: the spawn-time WOOL COLOR (the sheep — the per-mob stream's
    // FIRST draw for a wool_drops mob, the PITS-30 advance discipline,
    // the bat's bobPhase twin): one uniform reduced over the def's
    // wool_weights (rollWoolColor — the data-order cumulative walk). A
    // non-wool mob consumes NOTHING here: its stream stays the
    // pre-V2-15 one (the bit-identical red line — the first draw of a
    // wandering mob's stream is still the wander decision, as before).
    if (def.properties.woolDrops) {
        m_woolColor =
            rollWoolColor(def.properties.woolWeights, uniform01(m_rng));
    }
    // V3-3: the neutral class flag (resolved once — the m_fireBlockId
    // pattern; a non-neutral mob keeps false / 0 forever = the bit-
    // level red line). The enrage state is session-only (mobs are
    // ephemeral — zero format, the wool state's twin).
    m_neutral = def.properties.neutral;
}

void MobEntity::update(double dt) {
    if (m_dead || dt <= 0.0) {
        return;
    }
    // V2-14: the hover frame count (flying mobs only — the bob sine's
    // local time base on the fixed 1/60 step, hard rule 8; a non-flying
    // mob's field stays 0 forever, the structural bit root). It advances
    // BEFORE the physics so every sub-step of this frame reads the same
    // frame index (the vy baseline is per-FRAME, not per sub-step).
    if (m_def->properties.flying) {
        ++m_flyFrame;
    }
    // SURV-10: the milking cooldown counts down one frame per update
    // (the fixed 1/60 mob step — the integer-frame discipline).
    if (m_milkCooldownFrames > 0) {
        --m_milkCooldownFrames;
    }
    // V2-15: the wool regrow countdown (the same frame discipline —
    // one frame per fixed step): a sheared mob regrows its wool after
    // kSheepRegrowFrames (deviation 3: the FIXED duration, not
    // Minecraft's random grazing gate). The regrown wool keeps the
    // CURRENT instance color (the dyed-sheep semantics — the color
    // state is untouched by the bare window). Structurally inert for a
    // non-wool mob (the counter stays 0 = the pre-V2-15 frame).
    if (m_woolRegrowFrames > 0) {
        --m_woolRegrowFrames;
        if (m_woolRegrowFrames == 0) {
            m_bare = false;
        }
    }
    // V3-3: the NEUTRAL's enrage window (the survival-class real-dt
    // countdown — hard rule 8: entity age = real dt, the player's
    // hunger-timer class, NOT a frame count): a damaged neutral mob
    // stays angry for kNeutralAngryDurationSeconds; the timer's expiry
    // de-enrages it (the wander resumes — the chase predicate's anger
    // term falls out), and the PLAYER'S DEATH de-enrages it instantly
    // (the design's ruling: a dead player is no threat — the existing
    // "dead player -> wander" path's twin, made state-explicit so a
    // respawned player does not find a 29-second-old grudge). A
    // non-neutral mob never enters the branch (m_neutral false = the
    // pre-V3-3 frame, the structural bit root).
    if (m_neutral && m_angry) {
        m_angryTimer -= dt;
        if (m_angryTimer <= 0.0 || !m_playerAlive) {
            m_angry = false;
            m_angryTimer = 0.0;
        }
    }
    // ENT-1: captured BEFORE the AI + physics so the "moving" decision
    // is the strict frame-boundary horizontal displacement (see below).
    const float prevX = position().x;
    const float prevZ = position().z;
    if (m_attackCooldown > 0.0) {
        m_attackCooldown = std::max(0.0, m_attackCooldown - dt);
    }
    if (m_knockbackTime > 0.0) {
        m_knockbackTime = std::max(0.0, m_knockbackTime - dt);
    }
    // The knockback window suppresses AI steering: the impulse carries the
    // mob (decayed by integrate) and the AI resumes after.
    if (m_knockbackTime <= 0.0) {
        runAi(dt);
    }
    // ENT-4: the fluid tick state (once per update, the player's FLUID-2
    // pattern — consumed per sub-step by integrate) + the underwater
    // horizontal gate (×kUnderwaterSpeedFactor on whatever drives the
    // move this tick — the AI steering AND the knockback slide). The AI
    // state machine is untouched: chase / wander steer as usual, only the
    // physics changes.
    refreshFluidState();
    // V3-3: the lava-surface walker (the strider) is EXEMPT from the
    // underwater pace gate — it WALKS on the lava's surface plane at
    // full data speed (the design's walk pace; the gate would slow it
    // to 0.6x, the swim pace the walk pace replaces). The predicate is
    // the flag + the at-surface band + the LAVA kind (a strider in
    // WATER still swims at the gated pace — the nether's water
    // evaporation makes that unreachable in practice, the documented
    // boundary). A non-walker's condition is exactly the pre-V3-3 one
    // (onLavaSurface false = the gate runs, the bit root).
    if ((m_inFluid || m_atSurface) && !onLavaSurface()) {
        m_velocity.x *= kUnderwaterSpeedFactor;
        m_velocity.z *= kUnderwaterSpeedFactor;
    }
    // Peak-speed estimate for the sub-step count: in fluid (or at its
    // surface) the vertical motion is bounded by the rise/sink caps (the
    // player's FLUID-2 peak set, input-free — a peak SET, not one gravity
    // step); in air, this tick's post-gravity vertical velocity (UNCHANGED
    // — the on-land trajectory stays bit-identical to the pre-ENT-4
    // model; the horizontal one only decays or is reset). V3-3: the
    // lava-surface walker is DRY at its floor (the plane's fixed point
    // holds vy 0 — the dry estimate applies: the walk pace is the data
    // speed, no rise/sink cap); a non-walker's branch is the pre-V3-3
    // one (onLavaSurface false = the fluid estimate, the bit root).
    const bool walkerDry = onLavaSurface();
    const float vyAfter =
        (m_inFluid || m_atSurface) && !walkerDry
            ? std::max(std::abs(m_velocity.y), kWaterTerminalVelocity)
            : std::max(m_velocity.y - kGravity * static_cast<float>(dt),
                       -kTerminalVelocity);
    const float maxSpeed =
        std::max({std::abs(m_velocity.x), std::abs(vyAfter),
                  std::abs(m_velocity.z)});
    // ENT-3: the pre-substep horizontal motion (a clamped axis zeroes its
    // velocity inside the sub-steps — the step-up needs the pre-move value,
    // the player's pattern). It covers chase / wander / retreat steering
    // AND a knockback slide: whatever is driving the horizontal move, a
    // 1-block step in its way is crossed, not climbed.
    const Vec3 preMove {m_velocity.x, 0.0f, m_velocity.z};
    const float preMoveSpeed =
        std::max(std::abs(m_velocity.x), std::abs(m_velocity.z));
    moveInSubsteps(dt, maxSpeed);

    // ENT-3: step up 1 block when flush against a step (on the ground,
    // moving horizontally) — the shared Entity::stepUp, the Player's
    // geometry and the Phase-99 TRUE-1-block judgment (solid-region top
    // <= 1.0: a slab / full block / a glass pane — NOT the wall / fence
    // 1.5 barrier). This is the fix for the chase getting permanently
    // stuck on 1-block terrain steps. Pure geometry: frame-locked, no real
    // dt, no randomness (hard rule 8). FLUID-3: no fluid gate (the ENT-4
    // `!m_inFluid` ban is lifted — the player never had one): a wading mob
    // (on the 1-deep pool floor) steps out of the water over a 1-block
    // shore (the "walked in, can't get out" report, A1); a swimming mob
    // (at the swim level, onGround false) still cannot step up — no
    // swim-up shore climb (the documented deviation, the user's look is a
    // mob swimming at the surface, not stuck). V2-14: a FLYING mob never
    // steps up (the cruise carries no terrain intelligence — the wall hit
    // is a heading re-roll, deviation 3; a bob-pressed bat on a step face
    // would otherwise climb it). Structurally inert for every
    // non-flying mob (the flag is false in the data).
    if (m_onGround && lengthSq(preMove) > 0.0f &&
        !m_def->properties.flying) {
        stepUp(normalized(preMove), preMoveSpeed * static_cast<float>(dt));
    }

    // ENT-5: fall damage on landing — the Player's Phase-16 rule
    // mirrored onto mobs (Minecraft: mobs take fall damage — 1 HP per
    // block above the safe offset, kMobFallDamageOffset = 3, the
    // player's MC value). Landing in ANY fluid cancels the fall (the
    // player's P54 rule: the burn / swim replaces the fall damage —
    // a FRESH overlapsFluid scan of the FINAL position, like the
    // player: m_inFluid was sampled at the start of this tick, so a mob
    // landing in a pool one tick after its last airborne sample would
    // miss it). A
    // killing hit goes through the EXISTING damage() death path (the
    // game spawns the drops). Frame-locked (hard rule 8): m_onGround is
    // this tick's physics result; no real dt, no randomness.
    if (m_onGround && !m_wasOnGround) {
        const float fall = m_fallStartY - position().y;
        if (fall > kMobFallDamageOffset &&
            !(overlapsFluid(FluidKind::Water) || overlapsFluid(FluidKind::Lava))) {
            damage(fall - kMobFallDamageOffset);
        }
        m_fallStartY = position().y;
    }
    if (!m_onGround && m_wasOnGround) {
        m_fallStartY = position().y;
    }
    m_wasOnGround = m_onGround;

    // ENT-1: advance the limb-swing phase (render-only, never saved —
    // mobs are ephemeral). "Moving" = the STRICT frame-boundary
    // horizontal displacement (hard rule 8: frame-locked, no AI intent
    // and no real dt read — a knocked-back or falling-straight-down mob
    // swings or rests exactly as its position says). Only types with
    // swing data advance (structural gate: a swing-less model's phase
    // stays 0 forever = its render fast path is the static mesh).
    if (!m_def->model.swings.empty()) {
        const Vec3 pos = position();
        const bool moving = pos.x != prevX || pos.z != prevZ;
        m_swingPhase = advanceSwingPhase(m_swingPhase, moving);
    }

    // Fire system (Phase 58, v3 in Phase 73): the lava AND the fire
    // block ignite the mob with the player's fire (same effect
    // definition + config duration — the game arms it at spawn; an
    // unarmed mob never burns). The checks use the FINAL position of
    // this step (the AI + physics ran above). While in the lava or
    // overlapping a fire block the burn re-ignites (the duration resets
    // = continuous burn); water extinguishes instantly BEFORE the tick
    // (zero burn damage in water; water beats both, the P57 rule). V2-4:
    // the rain (m_raining, the setRaining per-frame feed) extinguishes
    // the same way — the inWater ∨ raining extension, no roof-shade
    // check (the documented deviation); the re-ignition runs first, so a
    // mob on a fire block in the rain stays out (the water-beats-fire
    // order). The countdown + tick run after leaving (the lingering burn), with the
    // Phase 46 effect semantics (the expiry step does not tick).
    if (m_fireArmed) {
        // V3-3: the lava-surface walker (the strider) is EXEMPT from the
        // LAVA ignition — it walks on the lava, the design's M2 ruling;
        // the FIRE BLOCK contact still ignites it (Minecraft's strider
        // burns on fire too — the inFire term is untouched). A non-
        // walker's term is exactly the pre-V3-3 one (the data flag
        // false = the bit root; the strider is the only carrier).
        const bool inLava =
            overlapsFluid(FluidKind::Lava) &&
            !m_def->properties.walksOnLava;
        // Fire v3 (Phase 73): the block twin of the lava ignition — the
        // strict AABB overlap of a fire-block cell (the cross-model fire,
        // never a fluid; the invalid id = never, resolved at
        // construction).
        const bool inFire = overlapsBlock(m_fireBlockId);
        const bool inWater = overlapsFluid(FluidKind::Water);
        if (inLava || inFire) {
            m_fireRemaining = m_fireDuration;
        }
        if (inWater || m_raining) {
            m_fireRemaining = 0.0;
            m_fireFrames = 0;
            m_fireDueFrames = m_fireIntervalFrames;
        }
        if (m_fireRemaining > 0.0) {
            m_fireRemaining -= dt;
            if (m_fireRemaining > 0.0) {
                // Tick on the integer frame grid: tick k lands on burning
                // frame k * intervalFrames (exact — no float phase drift).
                ++m_fireFrames;
                while (m_fireFrames >= m_fireDueFrames) {
                    m_fireDueFrames += m_fireIntervalFrames;
                    if (damage(m_fireAmount)) {
                        break;
                    }
                }
            } else {
                // Expired: clean the tick phase for the next ignition.
                m_fireFrames = 0;
                m_fireDueFrames = m_fireIntervalFrames;
            }
        }
    }
}

void MobEntity::integrate(float h) {
    if (m_def->properties.flying) {
        // V2-14: the hover baseline (the bat — the data flag gate, no
        // species switch): gravity is OFF (the vy is SET to the frame-
        // locked bob sine every sub-step — the baseline, not an
        // accumulation; the terminal cap never binds) and the fluid
        // model is bypassed (a flying body hovers through water too —
        // the documented simplification: the spawn cell is air by
        // construction and the hover is the flight's whole vertical
        // story). The knockback damping below still runs (the arrow's
        // impulse slides the bat, the cruise resumes after the window).
        m_velocity.y = bobVelocity();
    } else if (m_inFluid || m_atSurface) {
        // ENT-4 / FLUID-3: the fluid force model (the player's FLUID-2
        // integrate, the input branches removed — a mob has no Space /
        // crouch: it rises or sinks, it does not dive or swim-up). No
        // gravity in fluid; the at-surface band keeps a body's vy (the
        // dry band above the surface: the full-speed pass + the splash
        // fall-back) and the swim-level snap keeps the float terminal a
        // fixed point.
        if (onLavaSurface()) {
            // V3-3: the lava-surface walker (the strider — the data-flag
            // branch, the flyCruise's pure-new-branch twin): the lava's
            // surface plane is its FLOOR — the feet-on-surface walk pace
            // (the swim-level float's surface twin; the design's M2
            // ruling: the strider WALKS on the lava, it does not swim).
            integrateLavaSurface(h);
        } else {
            integrateFluid(h);
        }
    } else {
        // Gravity, capped at terminal velocity.
        m_velocity.y -= kGravity * h;
        if (m_velocity.y < -kTerminalVelocity) {
            m_velocity.y = -kTerminalVelocity;
        }
    }
    // No ground friction: the AI sets the exact data speed every frame (a
    // friction decay would make the walking speed a function of the
    // sub-step count). During the knockback window the impulse decays
    // exponentially instead (in fluid too — the water damps the slide as
    // the gate damps the steer).
    if (m_knockbackTime > 0.0) {
        const float f = std::exp(-kKnockbackDamping * h);
        m_velocity.x *= f;
        m_velocity.z *= f;
    }
}

bool MobEntity::onLavaSurface() const {
    // V3-3: the single judgment the walker's three exemptions read (the
    // fire ignition / the pace gate / the integrate's walk branch — the
    // refreshFluidState per-tick state, consumed the same way): the data
    // flag + a live LAVA plane (the immersion kind LAVA — a strider over
    // WATER is not a walker: the nether's evaporation makes that
    // unreachable in practice, the documented boundary) + the body AT the
    // plane's float band (m_atSurface — the feet within kMaxSubstepMove
    // of the plane, not resting on a solid: the walker's floor case) or
    // INSIDE the column (m_inFluid — the deep-recovery case: a body that
    // reached the column's depth below the band rises back to its floor,
    // the integrateLavaSurface's rise branch). The (atSurface || inFluid)
    // union is exactly the integrate's fluid-case gate, so the walker's
    // walk pace owns the fluid case whole; a strider on SOLID ground (no
    // band, no immersion) takes the gravity branch as any other mob.
    return m_def->properties.walksOnLava && m_fluidSurfaceY > 0.0f &&
           m_surfaceKind == FluidKind::Lava &&
           (m_atSurface || m_inFluid);
}

void MobEntity::integrateLavaSurface(float h) {
    // V3-3: the lava-surface walk pace (the strider — the data-flag
    // branch of integrate's fluid case). The lava's surface plane is
    // this body's FLOOR — the integrateFluid twin with the swim level
    // replaced by the plane (the design's M2 ruling: the strider WALKS
    // on the lava, it does not swim). The branch order:
    //   1. in the float band (the feet within kMaxSubstepMove of the
    //      plane — the at-surface case, on the plane OR a dry body above
    //      it landing with entry momentum): the FEET snap onto the plane
    //      and vy zeroes. The terminal (y = plane, vy = 0) is a fixed
    //      point: the snap re-fires every sub-step as a no-op, so it is
    //      fps-independent (the swim-level snap's twin). The strider sets
    //      down on the lava and walks — the splash-less landing, it never
    //      dives (the Minecraft behavior; the sub-step cap
    //      kMaxSubstepMove bounds the per-step drop, so a crossing body
    //      always lands in the band).
    //   2. below the band, rising or still (the deep-recovery case — a
    //      body that reached the column's depth): the passive rise (the
    //      integrateFluid rise's twin — the same buoyancy / rise-speed
    //      constants: the engine's lava is the water's in density), the
    //      swim level replaced by the PLANE: the recovery is feet-on,
    //      not half-submerged.
    //   3. (else — below the band, falling with entry momentum: keep its
    //      vy, the integrateFluid's "keep vy" twin: the sub-step cap
    //      bounds the drop to the column floor (the onGround clamp), and
    //      the still frame takes the rise — the self-correcting dive.)
    // NO GRAVITY in the branch (the integrateFluid's "no gravity in
    // fluid" twin — the plane's fixed point owns the vertical story).
    const float surface = m_fluidSurfaceY;
    if (m_position.y >= surface - kMaxSubstepMove) {
        m_position.y = surface;
        m_velocity.y = 0.0f;
    } else if (m_velocity.y >= 0.0f) {
        m_velocity.y = std::min(m_velocity.y + kWaterBuoyancy * h,
                                kWaterRiseSpeed);
    }
}

void MobEntity::runAi(double dt) {
    const MobProperties& p = m_def->properties;
    // V2-14: the flight AI (the bat — the data flag branch at the TOP of
    // the state machine, no species switch): a flying mob NEVER reaches
    // the chase / attack / pathfind / wander branches below (the bat's
    // detect_radius is 0 in the data anyway — the branch is the
    // structural guarantee). A non-flying mob takes the pre-V2-14 path
    // bit-identically (the pure-new-branch red line).
    if (p.flying) {
        // V3-14b: the flying ranged attacker (the ghast) takes the
        // flyShoot branch (the hover-hold + the cooldown fire fused
        // with the bat's cruise); a flying mob WITHOUT a shoot range
        // (the bat — shootRange 0 in the data) runs flyCruise
        // bit-identically (the pure-new-branch red line).
        if (p.shootRange > 0.0f) {
            flyShoot(p, dt);
        } else {
            flyCruise(p, dt);
        }
        return;
    }
    // V3-14d: the ridden takeover (the SESSION-state branch at the TOP
    // of the ground path — the flyCruise pure-new-branch twin): while
    // a player sits on this mob, the wander is suspended and the rider's
    // steer owns the heading / velocity. Gated on m_ridden (the game's
    // mount/dismount), NOT on a data flag — every non-ridden mob takes
    // the pre-V3-14d path bit-identically (the red line). The AMBIENT
    // roll is duplicated inside (the per-mob stream contract — the
    // hiss plays while ridden and the stream stays aligned, PITS 403⑤).
    if (m_ridden) {
        riderSteer(p);
        return;
    }
    // V3-3: the GROUND-mob ambient roll (the hoglin / piglin / strider —
    // the flyCruise squeak's ground twin, the data-driven general
    // mechanism, no species special-casing): once per frame, probability
    // 1 / (interval * kMobFramesPerSecond) of the per-mob stream
    // (frame-locked, hard rule 8 — a no-input run grunts on the same
    // frames every time), played spatialized at the mob's CURRENT
    // position (the game's callback). A mob without the data field never
    // rolls: the stream draw is an ambient mob's only new per-frame
    // consumption (every shipped overworld ground mob is ambient-free =
    // the bit-identical structural root; the bat returns above, its
    // roll stays in flyCruise).
    if (!p.ambientSoundName.empty() && m_ambientSound) {
        const double pAmbient = 1.0 /
            (p.ambientInterval * kMobFramesPerSecond);
        if (uniform01(m_rng) < pAmbient) {
            m_ambientSound(position());
        }
    }
    const Vec3 pos = position();
    const float dx = m_playerPos.x - pos.x;
    const float dz = m_playerPos.z - pos.z;
    const float distH = std::hypot(dx, dz);
    const float dy = m_playerPos.y - pos.y;
    // V3-3: the NEUTRAL class's chase gate (the design's single
    // predicate — no species switch): the existing detect test stands
    // for hostile / passive mobs (p.neutral false = the term is a
    // constant true = the pre-V3-3 expression bit-identically), while a
    // neutral mob's detect test holds ONLY WHILE ANGRY (m_angry — any
    // damage source enragues for kNeutralAngryDurationSeconds real dt,
    // the damage() hook + the update's de-enrage; a non-enraged neutral
    // mob wanders through the player, the Minecraft piglin's twin).
    const bool seesPlayer = m_playerAlive &&
                            (p.neutral ? m_angry : true) &&
                            distH <= p.detectRadius &&
                            std::abs(dy) <= kDetectVertical;

    // ENT-6: leaving the chase context (the player out of range or dead)
    // drops the active path — the wander below takes over. The window
    // resets too: the next chase starts a fresh progress measurement.
    if (!seesPlayer && !m_path.empty()) {
        m_path.clear();
        m_pathIdx = 0;
        m_stuckWindowT = 0.0;
        m_stuckAnchor = pos;
    }

    if (seesPlayer && p.shootRange > 0.0f && distH <= p.shootRange) {
        // Ranged attack (Phase 27, the skeleton): face the player and fire
        // on the cooldown. While the player is inside retreat_distance the
        // mob backs away at full data speed (Minecraft's kite); otherwise
        // it stands and shoots. It never closes in to melee — a ranged
        // mob's data attack_range is its (unused) melee range, 0 for the
        // skeleton, so the melee branch below can never hold for it.
        // ENT-6: a ranged mob never pathfinds (the branch precedes the
        // chase) — drop any stale path so a later chase starts clean.
        m_path.clear();
        m_pathIdx = 0;
        m_state = MobState::RangedAttack;
        if (distH > 1e-4f) {
            // Camera yaw convention: forward(yaw) = (-sin, 0, -cos).
            m_yaw = std::atan2(-dx, -dz);
        }
        if (distH <= p.retreatDistance) {
            const float inv = 1.0f / std::max(distH, 1e-4f);
            m_velocity.x = -dx * inv * p.speed;
            m_velocity.z = -dz * inv * p.speed;
        } else {
            m_velocity.x = 0.0f;
            m_velocity.z = 0.0f;
        }
        if (m_attackCooldown <= 0.0) {
            m_attackCooldown = p.attackCooldown;
            if (m_shootPlayer) {
                m_shootPlayer();
            }
        }
        return;
    }

    if (seesPlayer && distH <= p.attackRange &&
        std::abs(dy) <= kAttackVertical) {
        // ENT-6: the path's target zone is within 1 of the player, so a
        // pathing mob that gets here is done: the path is dropped, the
        // attack branch owns it from here on.
        m_path.clear();
        m_pathIdx = 0;
        // Attack: stand still, face the player, swing on the cooldown.
        m_state = MobState::Attack;
        m_velocity.x = 0.0f;
        m_velocity.z = 0.0f;
        if (distH > 1e-4f) {
            // Camera yaw convention: forward(yaw) = (-sin, 0, -cos).
            m_yaw = std::atan2(-dx, -dz);
        }
        if (m_attackCooldown <= 0.0) {
            m_attackCooldown = p.attackCooldown;
            if (m_hitPlayer) {
                m_hitPlayer(p.attackDamage);
            }
        }
        return;
    }

    if (seesPlayer) {
        // ENT-6: the lost-cooldown clock (throttles the pathfinding
        // retries after a failed route search).
        m_lostCooldown = std::max(0.0, m_lostCooldown - dt);
        if (!m_path.empty()) {
            // An active path overrides the straight chase until it is
            // consumed / replanned away / lost.
            m_state = MobState::Pathfind;
            stepAlongPath(p, pos, dt);
            return;
        }
        // Re-entering the chase from the wander: restart the stuck
        // window (the anchor is stale from before).
        if (m_state == MobState::Wander) {
            m_stuckWindowT = 0.0;
            m_stuckAnchor = pos;
        }
        // ENT-6: the stuck window — progress below a quarter of the
        // expected over the window = blocked by terrain (the chase
        // velocity is set every frame, but the wall keeps the position
        // put). Only then is the BFS attempted (structural dormancy:
        // an unblocked chaser never pathfinds, bit-identical steering).
        m_stuckWindowT += dt;
        if (m_stuckWindowT >= kStuckWindowSeconds) {
            const float expected =
                p.speed * static_cast<float>(kStuckWindowSeconds);
            const float progress = std::hypot(pos.x - m_stuckAnchor.x,
                                              pos.z - m_stuckAnchor.z);
            m_stuckWindowT = 0.0;
            m_stuckAnchor = pos;
            if (progress < kStuckProgressFactor * expected &&
                m_lostCooldown <= 0.0) {
                tryPathfind(pos);
            }
        }
        // Chase: full data speed toward the player's bottom-center.
        m_state = MobState::Chase;
        const float inv = 1.0f / std::max(distH, 1e-4f);
        m_velocity.x = dx * inv * p.speed;
        m_velocity.z = dz * inv * p.speed;
        m_yaw = std::atan2(-dx, -dz);
        return;
    }

    // Wander: a seeded decision every wander_interval (the first update
    // decides immediately: the timer starts at 0).
    m_state = MobState::Wander;
    m_wanderTimer -= dt;
    if (m_wanderTimer > 0.0) {
        return;  // keep the current heading / stand-still decision
    }
    m_wanderTimer = p.wanderInterval;
    if (nextMix(m_rng) % 3 == 0) {
        m_velocity.x = 0.0f;
        m_velocity.z = 0.0f;
        return;
    }
    const float angle =
        static_cast<float>(uniform01(m_rng) * 2.0 * std::numbers::pi);
    m_yaw = angle;
    const float s = p.speed * kWanderSpeedFactor;
    m_velocity.x = -std::sin(angle) * s;
    m_velocity.z = -std::cos(angle) * s;
}

// -- V3-14d: the ridden AI (the rider's steer takeover) -----------------

void MobEntity::riderSteer(const MobProperties& p) {
    m_state = MobState::Ridden;
    // The ambient roll (the per-mob stream contract — the ground
    // branch's verbatim twin, kept so the ridden hiss frame + the
    // stream alignment survive the takeover; PITS 403⑤).
    if (!p.ambientSoundName.empty() && m_ambientSound) {
        const double pAmbient = 1.0 /
            (p.ambientInterval * kMobFramesPerSecond);
        if (uniform01(m_rng) < pAmbient) {
            m_ambientSound(position());
        }
    }
    if (m_riderForward && lengthSq(m_riderDir) > 1e-8f) {
        // Forward: face the rider's heading (the camera yaw convention:
        // forward(yaw) = (-sin, -cos) — the chase steering's twin) and
        // walk at the DATA speed along it (the AI set-velocity
        // convention — the wander's `m_velocity = dir * speed` twin).
        const float inv = 1.0f / std::sqrt(lengthSq(m_riderDir));
        const float dx = m_riderDir.x * inv;
        const float dz = m_riderDir.z * inv;
        m_yaw = std::atan2(-dx, -dz);
        m_velocity.x = dx * p.speed;
        m_velocity.z = dz * p.speed;
        return;
    }
    // Released (no forward wish / no direction key): the kRideDecel
    // ease-out to a stand (the frame-locked twin of the player's ENT-7
    // decay — the fixed 1/60 step keeps the stop frame exact; the
    // stop-epsilon zeroes the residual, the player's :382-387 twin).
    m_velocity.x *= kRideDecel;
    m_velocity.z *= kRideDecel;
    if (std::abs(m_velocity.x) < kRideStopEps &&
        std::abs(m_velocity.z) < kRideStopEps) {
        m_velocity.x = 0.0f;
        m_velocity.z = 0.0f;
    }
}

// -- V2-14: the flight AI (the bat — the data-flag-gated cruise) --------

void MobEntity::flyCruise(const MobProperties& p, double dt) {
    // The cruise IS the bat's wander (the design: MobState is unchanged —
    // the enum keeps the ground mob's vocabulary, a flying mob reports
    // Wander).
    m_state = MobState::Wander;
    // Heading: a per-mob-stream roll every wander_interval (the data's
    // cruise turn cadence — bat.json reuses wander_interval 2.0). The
    // first update decides immediately (the timer starts at 0, the
    // wander branch's convention). A wall hit (afterSubstep) resets the
    // timer to 0 = the next frame re-rolls (deviation 3: no pathfinding,
    // the natural detour).
    m_wanderTimer -= dt;
    if (m_wanderTimer <= 0.0) {
        m_wanderTimer = p.wanderInterval;
        const float angle =
            static_cast<float>(uniform01(m_rng) * 2.0 * std::numbers::pi);
        m_yaw = angle;
    }
    // Cruise at the DATA speed along the heading (the camera yaw
    // convention: forward(yaw) = (-sin, 0, -cos) — the wander / chase
    // steering's twin; bat.json's speed is kBatCruiseSpeed, the content
    // pin holds it). The hover baseline (the vy sine) is integrate's job
    // (the gravity gate) — it is set every sub-step, not here.
    m_velocity.x = -std::sin(m_yaw) * p.speed;
    m_velocity.z = -std::cos(m_yaw) * p.speed;
    // The ambient roll (the bat's squeak — the data-driven general
    // mechanism, no species special-casing): once per frame, probability
    // 1 / (interval * kMobFramesPerSecond) of the per-mob stream
    // (frame-locked, hard rule 8 — a no-input run squeaks on the same
    // frames every time, the ambience's precedent). The play is
    // spatialized at the mob's CURRENT position (the game's callback).
    // A mob without the data field never rolls: the stream draw is the
    // flying ambient mob's only new per-frame consumption.
    if (!p.ambientSoundName.empty() && m_ambientSound) {
        const double pSqueak = 1.0 /
            (p.ambientInterval * kMobFramesPerSecond);
        if (uniform01(m_rng) < pSqueak) {
            m_ambientSound(position());
        }
    }
}

void MobEntity::flyShoot(const MobProperties& p, double dt) {
    // V3-14b: the ghast's AI — the bat's hover cruise fused with the
    // skeleton's ranged branch (the flight's vertical handling stays
    // integrate's job: the bob baseline runs under both branches, so
    // the "hold" below is the hover, not a freeze).
    const Vec3 pos = position();
    const float dx = m_playerPos.x - pos.x;
    const float dz = m_playerPos.z - pos.z;
    const float distH = std::hypot(dx, dz);
    const float dy = m_playerPos.y - pos.y;
    const bool seesPlayer = m_playerAlive && distH <= p.detectRadius &&
                            std::abs(dy) <= kDetectVertical &&
                            distH <= p.shootRange;
    if (!seesPlayer) {
        // Out of the shot's context: the bat's cruise verbatim (the
        // wander + the ambient roll live there — the ghast's screech
        // cruises exactly like the bat's squeak).
        flyCruise(p, dt);
        return;
    }
    // The shot's context (the skeleton branch's flying twin): face the
    // player, hold the hover position (zero horizontal velocity), and
    // fire on the cooldown (the LOS gate + the aim + the fireball's
    // launch live in the game's shoot callback, the skeleton's seam).
    m_state = MobState::RangedAttack;
    if (distH > 1e-4f) {
        // Camera yaw convention: forward(yaw) = (-sin, 0, -cos).
        m_yaw = std::atan2(-dx, -dz);
    }
    m_velocity.x = 0.0f;
    m_velocity.z = 0.0f;
    if (m_attackCooldown <= 0.0) {
        m_attackCooldown = p.attackCooldown;
        if (m_shootPlayer) {
            m_shootPlayer();
        }
    }
}

float MobEntity::bobVelocity() const {
    // The frame-locked hover sine (a pure function of the local frame
    // count + the spawn-stream phase — std::sin on the physics layer,
    // the integrate's knockback std::exp precedent: the frame grid makes
    // the trajectory a pure function of N, hard rule 8).
    const double arg =
        (2.0 * std::numbers::pi * static_cast<double>(m_flyFrame)) /
            kBatBobPeriodFrames +
        static_cast<double>(m_bobPhase);
    return static_cast<float>(kBatBobAmplitude * std::sin(arg));
}

void MobEntity::afterSubstep(bool blocked) {
    // V2-14: the flight wall hit (deviation 3's natural detour): a
    // blocked sub-step of a flying mob that clamped a HORIZONTAL axis to
    // zero velocity = a wall. The AI sets both horizontal components to
    // the nonzero cruise vector every frame, so a zeroed one can only
    // come from the collision resolve (the clamp is exact zero). A
    // ceiling / floor press (the y clamp) leaves the horizontal pair
    // untouched: no re-roll. The reset timer makes the NEXT frame's
    // flyCruise re-roll the heading.
    if (blocked && m_def->properties.flying &&
        (m_velocity.x == 0.0f || m_velocity.z == 0.0f)) {
        m_wanderTimer = 0.0;
    }
}

// -- ENT-6: the chase path (stuck-triggered BFS, frame-locked) -----------

void MobEntity::stepAlongPath(const MobProperties& p, const Vec3& pos,
                              double dt) {
    // A node not reached in time = the model was wrong (the shared
    // step-up's +X/+Z single-cell quirk makes a planned step infeasible,
    // the documented deviation): the path is aborted, the straight
    // chase resumes, throttled.
    m_nodeT += dt;
    // Replan on the rhythm: the target may have moved, and the from node
    // changed as the mob walked. A failed replan = lost (the straight
    // chase resumes, throttled by the cooldown).
    m_repathT += dt;
    if (m_repathT >= kRepathSeconds) {
        m_repathT = 0.0;
        const PathNode from = currentStandableNode(pos);
        const PathNode target = playerTargetNode();
        Result<std::vector<PathNode>> r = MC_FAIL(
            "pathfinding: the route replan is from an off-lattice cell");
        if (from.valid) {
            r = pathfindBFS(*m_world, m_blocks, from, target, bodyCells(),
                            kPathMaxNodes);
        }
        if (r.ok() && !r.value().empty()) {
            m_path = std::move(r.value());
            m_pathIdx = 0;
            m_nodeT = 0.0;
        } else {
            m_path.clear();
            m_pathIdx = 0;
            m_lostCooldown = kLostCooldownSeconds;
            return;
        }
    }
    // Advance through any nodes already reached, then steer to the next.
    while (m_pathIdx < m_path.size()) {
        const PathNode& n = m_path[m_pathIdx];
        const float nx = n.x + 0.5f - pos.x;
        const float nz = n.z + 0.5f - pos.z;
        if (std::hypot(nx, nz) < 0.5f && std::abs(pos.y - n.y) < 0.5f) {
            ++m_pathIdx;
            m_nodeT = 0.0;
            continue;
        }
        const float d = std::hypot(nx, nz);
        const float inv = 1.0f / std::max(d, 1e-4f);
        m_velocity.x = nx * inv * p.speed;
        m_velocity.z = nz * inv * p.speed;
        m_yaw = std::atan2(-nx, -nz);
        if (m_nodeT >= kNodeTimeoutSeconds) {
            // The model was wrong (the quirk geometry): give up the
            // route, the straight chase resumes on the cooldown.
            m_path.clear();
            m_pathIdx = 0;
            m_lostCooldown = kLostCooldownSeconds;
            return;
        }
        return;
    }
    // Path consumed (the target zone is reached): the straight chase —
    // or the attack branch — takes over next frame.
    m_path.clear();
    m_pathIdx = 0;
}

void MobEntity::tryPathfind(const Vec3& pos) {
    const PathNode from = currentStandableNode(pos);
    if (!from.valid) {
        // Off the integer lattice (stairs/slabs/mid-air): no route from
        // here — the straight chase continues, throttled.
        m_lostCooldown = kLostCooldownSeconds;
        return;
    }
    const PathNode target = playerTargetNode();
    const Result<std::vector<PathNode>> r =
        pathfindBFS(*m_world, m_blocks, from, target, bodyCells(),
                    kPathMaxNodes);
    if (r.ok() && !r.value().empty()) {
        m_path = std::move(r.value());
        m_pathIdx = 0;
        m_repathT = 0.0;
        m_nodeT = 0.0;
    } else {
        // No route (sealed) or already in the target zone (stuck for a
        // non-terrain reason): throttle the retry.
        m_lostCooldown = kLostCooldownSeconds;
    }
}

PathNode MobEntity::currentStandableNode(const Vec3& pos) const {
    // Only integer feet (resting on a FullCube floor plane) are nodes.
    const int iy = static_cast<int>(std::lround(pos.y));
    if (std::abs(pos.y - iy) > 1e-4f) {
        return PathNode{};
    }
    return PathNode{static_cast<int>(std::floor(pos.x)), iy,
                    static_cast<int>(std::floor(pos.z)), true};
}

PathNode MobEntity::playerTargetNode() const {
    // The player's feet rounded to the lattice: the target zone (within
    // 1) absorbs a mid-jump / sloped feet height.
    return PathNode{static_cast<int>(std::floor(m_playerPos.x)),
                    static_cast<int>(std::lround(m_playerPos.y)),
                    static_cast<int>(std::floor(m_playerPos.z)), true};
}

int MobEntity::bodyCells() const {
    // The body's vertical cell count (the 1.8 mob = 2, the 0.7 chicken
    // = 1) from the data model height.
    return std::max(1, static_cast<int>(std::ceil(
                           m_def->model.height - 1e-6f)));
}

int MobEntity::rollDropCount(int minCount, int maxCount) {
    if (minCount >= maxCount) {
        return minCount;
    }
    // Uniform over the (max - min + 1) integer outcomes; the stream
    // advances exactly once (the wander-decision convention).
    const std::uint64_t span =
        static_cast<std::uint64_t>(maxCount - minCount + 1);
    return minCount + static_cast<int>(nextMix(m_rng) % span);
}

int MobEntity::shearWool() {
    // V2-15: the successful shear — the state change ONLY (the game owns
    // the drop spawn / the shears wear / the sound, the tryMilk adapter
    // pattern). Not eligible (a non-wool mob or an already-bare one) =
    // 0 with the state UNTOUCHED (no stream draw, no regrow reset — the
    // bare re-shear no-op, the Minecraft twin).
    if (m_bare || !m_def->properties.woolDrops) {
        return 0;
    }
    const MobDrop* wool = woolDropEntry(&m_def->properties);
    if (wool == nullptr) {
        return 0; // defensive (init rejects a wool_drops mob without a
                  // wool drop entry); no state change
    }
    // The wool count: the mob's OWN wool entry's range, rolled with the
    // per-mob stream (the rollDropCount twin — a deterministic death /
    // shear frame yields a reproducible roll; the degenerate range
    // consumes nothing, as there).
    const int n = rollDropCount(wool->countMin, wool->countMax);
    m_bare = true;
    m_woolRegrowFrames = kSheepRegrowFrames; // deviation 3: fixed 120 s
    return n;
}

std::pair<std::string, int> MobEntity::barterItem() {
    // V3-14c: the stateless trade roll (shearWool's pattern minus the
    // state — the mob is NOT touched, so a neutral barter cannot enrage;
    // the damage() single-choke-point is the only enrage source and this
    // path never calls it). The item draw is FIRST (the roll order is
    // the stream contract: one uniform01 for the weighted pick, then the
    // count's nextMix when the range is non-degenerate — a fixed-count
    // entry draws nothing, the rollDropCount discipline).
    const MobProperties& p = m_def->properties;
    if (p.barterOutputs.empty() || p.barterInput.empty()) {
        return {std::string {}, 0}; // not a bartering mob: NO stream draw
    }
    const int idx = rollBarterOutput(p.barterOutputs, uniform01(m_rng));
    const BarterOutput& entry = p.barterOutputs.at(idx);
    const int n = rollDropCount(entry.countMin, entry.countMax);
    return {entry.item, n};
}

void MobEntity::dyeWool(const std::string& color) {
    // V2-15: the successful dye — the instance color re-set (the input
    // is DATA, so NO stream draw — the recolor is a pure function of
    // the held dye, the block recolor's twin). Refused when bare
    // (Minecraft: a sheared mob cannot be dyed), non-wool (the flag
    // gate) or given an empty color (the belt).
    if (m_bare || color.empty() || !m_def->properties.woolDrops) {
        return;
    }
    m_woolColor = color;
}

bool MobEntity::damage(float amount) {
    if (m_dead || amount <= 0.0f) {
        return m_dead;
    }
    m_health -= amount;
    if (m_health <= 0.0f) {
        m_health = 0.0f;
        m_dead = true;
    } else if (m_neutral) {
        // V3-3: the NEUTRAL's enrage hook (the design's M1 ruling —
        // ANY damage source enragues: the player's melee / the arrow's
        // swept hit / the fire tick / the fall landing / the poison
        // tick all flow through this one choke point, the Minecraft
        // piglin's deviation-7 twin). A killing hit leaves the mob dead
        // (it despawns this frame — the enrage state would be dead data);
        // a surviving hit sets the window to the full
        // kNeutralAngryDurationSeconds (a repeat hit REFRESHES it —
        // the burning neutral's continuous enrage, the MC behavior). A
        // non-neutral mob never enters the branch (the bit root).
        m_angry = true;
        m_angryTimer = kNeutralAngryDurationSeconds;
    }
    return m_dead;
}

void MobEntity::addKnockback(Vec3 impulse) {
    m_velocity += impulse;
    m_knockbackTime = kKnockbackTime;
}

void MobEntity::armFire(double tickInterval, float tickAmount,
                        double duration) {
    m_fireArmed = true;
    m_fireAmount = tickAmount;
    m_fireDuration = duration;
    // The mob steps on the fixed 1/60 frame grid (hard rule 8), so the
    // cadence is exact in integer frame units: interval * 60 (the game
    // init validates the ignites effect's interval is a whole number of
    // frames — for the shipped fire, 0.2 s = 12 frames — so the rounding
    // is exact and tick k lands on burning frame k * intervalFrames).
    m_fireIntervalFrames = static_cast<int>(std::llround(tickInterval * 60.0));
    m_fireDueFrames = m_fireIntervalFrames;
}

void MobEntity::applyEffect(int effect, double duration,
                            const EffectRegistry& effects, int tier) {
    if (m_dead || effect < 0 || duration <= 0.0) {
        return;
    }
    const Effect& def = effects.at(effect);
    if (!def.affectsMobs) {
        return; // the effect is not mob-accepting (defensive; a bow's
                // arrow_effect is init-cross-checked to carry the flag)
    }
    // Minecraft: the poison does not affect the undead (the zombie /
    // skeleton are immune); the wither has NO undead_exempt and is the
    // effect that kills them. A pure combination gate (both data).
    if (def.undeadExempt && m_def->properties.undead) {
        return;
    }
    m_effects.apply(effect, duration, tier);
}

void MobEntity::updateStatusEffects(double dt, const EffectRegistry& effects) {
    // count == 0 fast exit: a mob without active effects costs nothing
    // (the structural-immunity basis — every pre-SURV-5 mob's frame is
    // bit-identical to this check alone).
    if (m_dead || dt <= 0.0 || m_effects.count() == 0) {
        return;
    }
    m_effects.update(dt, effects, [this](const Effect& def) {
        if (def.tick != Effect::Tick::Damage) {
            // Heal ticks are a no-op on mobs (MobEntity has no
            // regeneration — the documented deviation); a modifier
            // effect has no ticks at all (its None tick never fires).
            return;
        }
        // The MOB floor: min_health_mob when the effect sets it, else the
        // effect's player min_health (the default-follow rule). Poison
        // ships 0: the damage tick may kill the mob (the player's 1-HP
        // floor is untouched — the dual-target rule).
        const float floor =
            def.minHealthMob >= 0 ? def.minHealthMob : def.minHealth;
        if (m_health - def.tickAmount >= floor) {
            // The killing tick runs through the EXISTING damage() path:
            // the dead flag + the game's despawn pass spawn the data
            // drops (unchanged). Later ticks this frame see m_dead and
            // no-op (damage()'s guard).
            damage(def.tickAmount);
        }
    });
}

} // namespace mc
