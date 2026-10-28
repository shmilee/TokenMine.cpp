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
}

void MobEntity::update(double dt) {
    if (m_dead || dt <= 0.0) {
        return;
    }
    // SURV-10: the milking cooldown counts down one frame per update
    // (the fixed 1/60 mob step — the integer-frame discipline).
    if (m_milkCooldownFrames > 0) {
        --m_milkCooldownFrames;
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
    if (m_inFluid || m_atSurface) {
        m_velocity.x *= kUnderwaterSpeedFactor;
        m_velocity.z *= kUnderwaterSpeedFactor;
    }
    // Peak-speed estimate for the sub-step count: in fluid (or at its
    // surface) the vertical motion is bounded by the rise/sink caps (the
    // player's FLUID-2 peak set, input-free — a peak SET, not one gravity
    // step); in air, this tick's post-gravity vertical velocity (UNCHANGED
    // — the on-land trajectory stays bit-identical to the pre-ENT-4
    // model; the horizontal one only decays or is reset).
    const float vyAfter = m_inFluid || m_atSurface
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
    // mob swimming at the surface, not stuck).
    if (m_onGround && lengthSq(preMove) > 0.0f) {
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
    // (zero burn damage in water; water beats both, the P57 rule). The
    // countdown + tick run after leaving (the lingering burn), with the
    // Phase 46 effect semantics (the expiry step does not tick).
    if (m_fireArmed) {
        const bool inLava = overlapsFluid(FluidKind::Lava);
        // Fire v3 (Phase 73): the block twin of the lava ignition — the
        // strict AABB overlap of a fire-block cell (the cross-model fire,
        // never a fluid; the invalid id = never, resolved at
        // construction).
        const bool inFire = overlapsBlock(m_fireBlockId);
        const bool inWater = overlapsFluid(FluidKind::Water);
        if (inLava || inFire) {
            m_fireRemaining = m_fireDuration;
        }
        if (inWater) {
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
    if (m_inFluid || m_atSurface) {
        // ENT-4 / FLUID-3: the fluid force model (the player's FLUID-2
        // integrate, the input branches removed — a mob has no Space /
        // crouch: it rises or sinks, it does not dive or swim-up). No
        // gravity in fluid; the at-surface band keeps a body's vy (the
        // dry band above the surface: the full-speed pass + the splash
        // fall-back) and the swim-level snap keeps the float terminal a
        // fixed point.
        integrateFluid(h);
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

void MobEntity::runAi(double dt) {
    const MobProperties& p = m_def->properties;
    const Vec3 pos = position();
    const float dx = m_playerPos.x - pos.x;
    const float dz = m_playerPos.z - pos.z;
    const float distH = std::hypot(dx, dz);
    const float dy = m_playerPos.y - pos.y;
    const bool seesPlayer = m_playerAlive && distH <= p.detectRadius &&
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
            r = pathfindBFS(m_world, m_blocks, from, target, bodyCells(),
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
        pathfindBFS(m_world, m_blocks, from, target, bodyCells(),
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

bool MobEntity::damage(float amount) {
    if (m_dead || amount <= 0.0f) {
        return m_dead;
    }
    m_health -= amount;
    if (m_health <= 0.0f) {
        m_health = 0.0f;
        m_dead = true;
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
