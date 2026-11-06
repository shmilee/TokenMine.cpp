
#include "player/player.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>

#include "block/block.hpp"
#include "block/block_registry.hpp"
#include "core/math/vec2.hpp"
#include "entity/mob_swing.hpp"
#include "item/durability.hpp"

namespace mc {

Player::Player(const World& world, const BlockRegistry& blocks, Vec3 spawnPos)
    : Player(world, blocks, spawnPos, Params {}) {
}

Player::Player(const World& world, const BlockRegistry& blocks, Vec3 spawnPos,
               Params params)
    : Entity(world, blocks, spawnPos, params.width, params.height),
      m_params(params),
      m_spawnPos(spawnPos),
      // Phase 70 (the ignition source since Phase 74): resolve the fire
      // block id once (the registry is static after init). An unknown
      // name leaves the id invalid = isInFire is always false (a test
      // fixture without the fire block — the fluid.cpp fallback
      // pattern).
      m_fireBlockId(blocks.idForName("fire")),
      // Phase 71: the campfire id the same way (unknown = never sitting).
      // Phase 75: the extinguished twin joins the seat.
      m_campfireBlockId(blocks.idForName("campfire")),
      m_campfireExtId(blocks.idForName("campfire_extinguished")) {
    teleportTo(spawnPos);
}

Result<Player::Params> Player::Params::fromConfig(const Config& config) {
    Params p;
    // One reader per field: a missing key keeps the struct default, a
    // present key must be a finite JSON number (the Config getters would
    // silently fall back to the default on a type mismatch, so this reads
    // the raw values).
    auto read = [&config](const char* name, auto& out,
                          bool allowZero) -> Result<void> {
        const std::string key = std::string("player.") + name;
        const Json* j = config.at(key);
        if (j == nullptr) {
            return {}; // absent: keep the current (default) value
        }
        if (!j->isNumber()) {
            return MC_FAIL("config " + key + " must be a number");
        }
        const double v = j->asDouble(0.0);
        if (!std::isfinite(v) || (allowZero ? v < 0.0 : v <= 0.0)) {
            return MC_FAIL("config " + key + " must be " +
                           (allowZero ? "non-negative" : "positive") +
                           ", got " + std::to_string(v));
        }
        out = static_cast<std::remove_reference_t<decltype(out)>>(v);
        return {};
    };
    MC_TRY(read("width", p.width, false));
    MC_TRY(read("height", p.height, false));
    MC_TRY(read("eye_height", p.eyeHeight, false));
    MC_TRY(read("walk_speed", p.walkSpeed, false));
    MC_TRY(read("sprint_speed", p.sprintSpeed, false));
    MC_TRY(read("jump_speed", p.jumpSpeed, false));
    MC_TRY(read("gravity", p.gravity, false));
    MC_TRY(read("terminal_velocity", p.terminalVelocity, false));
    MC_TRY(read("swim_speed", p.swimSpeed, false));
    MC_TRY(read("swim_up_speed", p.swimUpSpeed, false));
    // FLUID-2: the retired player.water_gravity key is no longer read — the
    // role flipped to buoyancy (the value 12.0 carries over as
    // water_buoyancy). A pre-FLUID-2 config carrying it still boots (the
    // Phase-74 fire_damage precedent: unknown player.* keys are tolerated).
    MC_TRY(read("water_buoyancy", p.waterBuoyancy, false));
    MC_TRY(read("water_rise_speed", p.waterRiseSpeed, false));
    MC_TRY(read("water_sink_speed", p.waterSinkSpeed, false));
    MC_TRY(read("water_terminal_velocity", p.waterTerminalVelocity, false));
    MC_TRY(read("lava_swim_speed", p.lavaSwimSpeed, false));
    MC_TRY(read("lava_swim_up_speed", p.lavaSwimUpSpeed, false));
    MC_TRY(read("lava_gravity", p.lavaGravity, false));
    MC_TRY(read("lava_terminal_velocity", p.lavaTerminalVelocity, false));
    MC_TRY(read("mouse_sensitivity", p.mouseSensitivity, false));
    MC_TRY(read("max_health", p.maxHealth, false));
    MC_TRY(read("fall_damage_offset", p.fallDamageOffset, true));
    MC_TRY(read("crouch_height", p.crouchHeight, false));
    MC_TRY(read("crouch_speed_scale", p.crouchSpeedScale, false));
    MC_TRY(read("regen_delay_seconds", p.regenDelaySeconds, false));
    MC_TRY(read("regen_interval_seconds", p.regenIntervalSeconds, false));
    MC_TRY(read("max_hunger", p.maxHunger, false));
    MC_TRY(read("hunger_decay_seconds", p.hungerDecaySeconds, false));
    MC_TRY(read("starve_damage_interval_seconds",
                p.starveDamageIntervalSeconds, false));
    MC_TRY(read("regen_hunger_threshold", p.regenHungerThreshold, true));
    MC_TRY(read("sprint_hunger_decay_factor", p.sprintHungerDecayFactor,
                false));
    MC_TRY(read("lava_damage_interval_seconds", p.lavaDamageIntervalSeconds,
                false));
    MC_TRY(read("lava_damage", p.lavaDamage, false));
    // Phase 74: the retired player.fire_damage /
    // fire_damage_interval_seconds keys are no longer read — the fire
    // block ignites the player (the P57 effect path) instead of doing
    // direct contact damage. A pre-Phase-74 config carrying them still
    // boots (unknown player.* keys are tolerated).
    MC_TRY(read("fire_duration_seconds", p.fireDurationSeconds, false));
    // Cross-field sanity (the defaults satisfy all four).
    if (p.eyeHeight >= p.height) {
        return MC_FAIL("config player.eye_height must be < player.height");
    }
    if (p.crouchHeight >= p.height) {
        return MC_FAIL("config player.crouch_height must be < player.height");
    }
    if (p.regenHungerThreshold > p.maxHunger) {
        return MC_FAIL("config player.regen_hunger_threshold must be <= "
                       "player.max_hunger");
    }
    if (p.lavaDamage > p.maxHealth) {
        return MC_FAIL("config player.lava_damage must be <= "
                       "player.max_health");
    }
    if (p.fireDurationSeconds > 100.0) {
        return MC_FAIL("config player.fire_duration_seconds must be <= 100");
    }
    return p;
}

bool Player::hasRoomAt(const World& world, const BlockRegistry& blocks,
                       Vec3 bottomCenter, float width, float height) {
    const float half = width * 0.5f;
    const Vec3 mn {bottomCenter.x - half, bottomCenter.y,
                   bottomCenter.z - half};
    const Vec3 mx {bottomCenter.x + half, bottomCenter.y + height,
                   bottomCenter.z + half};

    const int x0 = static_cast<int>(std::floorf(mn.x));
    const int x1 = static_cast<int>(std::floorf(mx.x));
    const int y0 = static_cast<int>(std::floorf(mn.y));
    const int y1 = static_cast<int>(std::floorf(mx.y));
    const int z0 = static_cast<int>(std::floorf(mn.z));
    const int z1 = static_cast<int>(std::floorf(mx.z));
    for (int x = x0; x <= x1; ++x) {
        for (int y = y0; y <= y1; ++y) {
            for (int z = z0; z <= z1; ++z) {
                if (!(mn.x < x + 1 && mx.x > x)) {
                    continue;
                }
                if (!(mn.y < y + 1 && mx.y > y)) {
                    continue;
                }
                if (!(mn.z < z + 1 && mx.z > z)) {
                    continue;
                }
                const BlockID id = world.getBlock(x, y, z);
                if (id == BlockID::air() || !id.valid()) {
                    continue;
                }
                if (blocks.at(id).properties.solid) {
                    return false;
                }
            }
        }
    }
    return true;
}

void Player::update(double dt, const Input& input, bool lookEnabled) {
    m_input = &input;
    m_lookEnabled = lookEnabled;
    update(dt);
}

void Player::update(double dt) {
    if (dt <= 0.0 || m_dead) {
        return; // a dead player is frozen; the game re-enters via respawn()
    }
    m_updateStartPos = m_position; // V3-13: the distance stat's delta

    // 1. Mouse look (only while the pointer is captured).
    if (m_lookEnabled) {
        const Vec2 mouse = m_input->mouseDelta();
        yaw -= mouse.x * m_params.mouseSensitivity;
        pitch -= mouse.y * m_params.mouseSensitivity;
        pitch = std::clamp(pitch, -kMaxPitch, kMaxPitch);
    }

    // 2. Desired horizontal velocity (instantaneous, no inertia).
    const float cp = std::cos(pitch);
    const Vec3 flatFwd {std::sin(yaw) * -cp, 0.0f, std::cos(yaw) * -cp};
    const Vec3 fwd = normalized(flatFwd);
    const Vec3 right {fwd.z * -1.0f, 0.0f, fwd.x};

    // 2a. Fluid state: while the AABB overlaps a fluid cell the player
    // swims (reduced sink, reduced speed, Space swims up). Lava (Phase 54)
    // is the same swim state with the lava pace + the burn timer; when the
    // AABB straddles both kinds, lava wins.
    m_inWater = isInWater();
    m_inLava = isInLava();
    // V2-19: the fly's fluid gate — in water / lava the fly auto-switches
    // OFF (the documented deviation: the project's fluid buoyancy model
    // does not stack the flight, the minimal surface — the Minecraft
    // creative flies underwater, this one does not). Runs before the
    // speed computation and the sub-steps, so the SAME frame is already
    // on the swim pace + the normal gravity.
    if (m_flying && (m_inWater || m_inLava)) {
        m_flying = false;
    }
    // 2a-fluid2. The water surface plane W' of the feet's center column:
    // one above the topmost full (level-8) water cell at or below the feet
    // (scanned down kSurfaceScanDepth cells), or -1 when none. In a
    // continuous column W' tracks the feet one cell above (a transient
    // local plane) and resolves to the true surface at the topmost cell —
    // so the float only settles where the column ends. m_atSurface = the
    // feet within the float band (kMaxSubstepMove: the max per-sub-step
    // vertical displacement, so a sub-step that crosses the plane is
    // always seen) and the player not grounded (a pool-edge stand at the
    // surface keeps the ground pace — the onGround exclusion).
    {
        const int cx = static_cast<int>(std::floorf(m_position.x));
        const int cz = static_cast<int>(std::floorf(m_position.z));
        const int top = static_cast<int>(std::floorf(m_position.y));
        m_surfaceY = -1.0f;
        for (int y = top; y > top - kSurfaceScanDepth; --y) {
            if (m_world->getFluidOfKind(cx, y, cz, FluidKind::Water) ==
                kMaxFluidLevel) {
                m_surfaceY = static_cast<float>(y) + 1.0f;
                break;
            }
        }
        m_atSurface =
            m_surfaceY > 0.0f &&
            std::abs(m_position.y - m_surfaceY) <= kMaxSubstepMove &&
            !isRestingOnSolid();
    }

    // 2b. Crouch (LeftControl): the height state machine runs before the
    // speed computation (crouch scales the speed) and before the edge
    // guard (crouching on the ground holds at ledges).
    m_crouching = m_input->isDown(GameKey::LeftControl);
    updateCrouchHeight();

    // Sitting (Phase 71): crouching ON THE GROUND while the AABB overlaps
    // a campfire cell (the campfire is a non-solid cross block — the
    // player stands in it, like the fire contact above). Sitting locks
    // the movement (the wish below is zeroed) and the jump (integrate);
    // the crouch height IS the sitting pose. m_onGround is the previous
    // sub-step's result (the edge-guard convention) — a player mid-air
    // next to the fire does not sit.
    m_sitting =
        m_crouching && m_onGround &&
        (overlapsBlock(m_campfireBlockId) ||
         overlapsBlock(m_campfireExtId)); // both invalid-safe (P70)

    Vec3 wish {0.0f, 0.0f, 0.0f};
    if (m_input->isDown(GameKey::W)) {
        wish += fwd;
    }
    if (m_input->isDown(GameKey::S)) {
        wish -= fwd;
    }
    if (m_input->isDown(GameKey::D)) {
        wish += right;
    }
    if (m_input->isDown(GameKey::A)) {
        wish -= right;
    }
    // Sitting (Phase 71): the movement is locked (the sprint condition
    // below also falls out — it requires a non-zero wish).
    if (m_sitting) {
        wish = Vec3 {0.0f, 0.0f, 0.0f};
    }
    // Sprinting (Phase 28): the exact condition that selects sprintSpeed
    // (Shift + moving, out of water) — it also doubles the hunger decay
    // pace further down (Minecraft's 40 s -> 20 s relative shape).
    // No sprint in any fluid (lava, Phase 54) or on the water surface
    // (FLUID-2: the surface pace is the swim pace).
    const bool sprinting =
        m_input->isDown(GameKey::LeftShift) && lengthSq(wish) > 0.0f &&
        !m_inWater && !m_atSurface && !m_inLava;
    Vec3 dir {0.0f, 0.0f, 0.0f};
    // ENT-7: the fluid tiers (FLUID-2 water / surface / lava) keep the
    // instant set — the fluid integration is separately tuned (the
    // documented deviation); only the ground/air path gets the inertia.
    const bool inFluidTier = m_inWater || m_atSurface || m_inLava;
    // T5 (v3.7.0): the walk factor of the two feet cells (the soul sand
    // slowdown — the block's data-driven walk_speed_factor, the min =
    // the slowest surface). The FEET ROW is floor(position.y -
    // kFaceEps): grounded the feet rest EXACTLY on the integer support
    // plane, so this row is the SUPPORT (the crouch-edge-guard's "one
    // cell below the feet air" convention); mid-air it is the feet' own
    // row, which a solid block cannot occupy — one uniform formula for
    // both states (the fire-contact scan, restricted to the feet row).
    // The cells are the AABB's X span (the two feet at the feet' center
    // Z — the m_surfaceY column convention), strict-overlap gated (the
    // hard-rule-6 convention). A factor of 1.0 (no feet cell carries
    // the property — the overworld's content) keeps the walk pace
    // bit-identical (x * 1.0f == x, IEEE 754).
    float walkFactor = 1.0f;
    {
        const Vec3 mn = aabbMin();
        const Vec3 mx = aabbMax();
        const int y0 = static_cast<int>(std::floorf(m_position.y - kFaceEps));
        const int z0 = static_cast<int>(std::floorf(m_position.z));
        const int xLast = static_cast<int>(std::floorf(mx.x));
        for (int x = static_cast<int>(std::floorf(mn.x)); x <= xLast; ++x) {
            if (!(mn.x < x + 1 && mx.x > x)) {
                continue; // no strict X overlap (the hard rule 6)
            }
            const BlockID id = m_world->getBlock(x, y0, z0);
            if (id.valid() && id != BlockID::air()) {
                walkFactor = feetWalkFactor(
                    walkFactor, m_blocks.at(id).properties.walkSpeedFactor);
            }
        }
    }
    if (lengthSq(wish) > 0.0f) {
        // V2-19: the fly's horizontal pace — the WALK speed x
        // kFlySpeedMultiplier (the user's "faster than walking": 6.88
        // b/s, above the sprint 5.6). The Shift is the fly's DESCEND key,
        // not the sprint, so the fly never takes the sprint branch (the
        // fluid tiers are unreachable while flying — the auto switch-off
        // gate above); crouch + the beacon speed multiplier scale it
        // exactly like the walk (the crouch mechanism runs as usual).
        float speed = m_flying
                          ? m_params.walkSpeed * kFlySpeedMultiplier
                          : m_inLava
                          ? m_params.lavaSwimSpeed
                          : ((m_inWater || m_atSurface)
                                  ? m_params.swimSpeed // surface swim, FLUID-2
                                  : (m_input->isDown(GameKey::LeftShift)
                                          ? m_params.sprintSpeed
                                          : m_params.walkSpeed));
        if (m_crouching && !m_inWater && !m_atSurface && !m_inLava) {
            speed *= m_params.crouchSpeedScale;
        }
        // T5 (v3.7.0): the walk-only slowdown (the soul sand): the GROUND
        // walk/sprint pace is scaled by the feet' factor (the min over
        // the two feet cells — the slowest surface). Flight / any fluid
        // tier / crouch are EXEMPT (the walking-only rule — the recorded
        // deviation 4); a factor of 1.0 (no feet cell carries the
        // property — the overworld's content) is a bit-identical no-op
        // (the structural-immunity root).
        speed = applyWalkSpeedFactor(
            speed, walkFactor,
            !m_flying && !m_inWater && !m_atSurface && !m_inLava &&
                !m_crouching);
        // Phase 84: the beacon's speed effect (the game pushes the
        // effect-derived multiplier each frame; 1.0 when none is active).
        speed *= m_speedMultiplier;
        dir = normalized(wish);
        if (inFluidTier) {
            m_velocity.x = dir.x * speed;
            m_velocity.z = dir.z * speed;
        } else {
            // ENT-7: the exponential approach to dir*speed (fps-
            // independent on the real dt): the ground tier starts up
            // snappy, the air tier steers weakly.
            const float k = m_onGround ? kGroundAccelRate : kAirSteerRate;
            const float f = 1.0f - std::exp(-k * static_cast<float>(dt));
            m_velocity.x += (dir.x * speed - m_velocity.x) * f;
            m_velocity.z += (dir.z * speed - m_velocity.z) * f;
        }
        // Crouching on the ground: do not walk off a 1-block ledge.
        if (m_crouching && m_onGround) {
            applyCrouchEdgeGuard(dir, dt);
        }
    } else {
        if (inFluidTier) {
            m_velocity.x = 0.0f;
            m_velocity.z = 0.0f;
        } else {
            // ENT-7: releasing = the exponential decay (the slide), not
            // the instant stop: the ground stops fast (the MC 0.5448/
            // tick friction class), the air keeps gliding (the 0.91/
            // tick drag class). Below the stop epsilon the velocity is
            // zero exactly (the deterministic rest state).
            const float k = m_onGround ? kGroundDecelRate : kAirDecelRate;
            const float f = std::exp(-k * static_cast<float>(dt));
            m_velocity.x *= f;
            m_velocity.z *= f;
            if (std::abs(m_velocity.x) < kVelocityStopEps) {
                m_velocity.x = 0.0f;
            }
            if (std::abs(m_velocity.z) < kVelocityStopEps) {
                m_velocity.z = 0.0f;
            }
        }
    }

    // 3. Sub-step the tick (see Entity::moveInSubsteps); the peak-speed
    //    estimate uses this tick's post-gravity vertical velocity.
    // FLUID-2: the water/surface tiers are PEAK sets (the per-sub-step
    // vertical motion is bounded by the caps, not by one gravity step), so
    // the sub-step split stays tunnel-safe for the rise/sink/dive/jump
    // speeds at any frame rate. The lava and air formulas are UNCHANGED
    // (the resting air tier must not gain a sub-step — the P99 frame
    // budget).
    // UX-3: captured BEFORE the physics so the "moving" decision is the
    // strict frame-boundary horizontal displacement (the ENT-1 rule: the
    // position says, not the intent — a knocked-back / sliding / falling
    // player swings or rests exactly as its position says).
    const float prevX = m_position.x;
    const float prevZ = m_position.z;
    const float vyAfter =
        m_inLava
            ? std::max(m_velocity.y - m_params.lavaGravity *
                                      static_cast<float>(dt),
                       -m_params.lavaTerminalVelocity)
            : (m_inWater
                   ? std::max({std::abs(m_velocity.y),
                               m_params.waterTerminalVelocity,
                               m_params.waterRiseSpeed,
                               m_params.waterSinkSpeed,
                               m_input->isDown(GameKey::Space)
                                   ? m_params.swimUpSpeed
                                   : 0.0f})
                   : (m_atSurface
                          ? std::max(std::abs(m_velocity.y),
                                     m_input->isDown(GameKey::Space)
                                         ? m_params.jumpSpeed
                                         : 0.0f)
                          : std::max(m_velocity.y - m_params.gravity *
                                        static_cast<float>(dt),
                                     -m_params.terminalVelocity)));
    float maxSpeed =
        std::max({std::abs(m_velocity.x), std::abs(vyAfter),
                  std::abs(m_velocity.z)});
    // Phase 99: a jump impulse happens INSIDE the sub-steps (integrate), so
    // the vyAfter estimate above (pre-impulse) under-counts it: on a long
    // frame (a streaming hiccup, dt ~0.08 s) the tick would take a SINGLE
    // sub-step and displace the full frame at jump speed (0.73 blocks > the
    // 0.4 sub-step cap) — the jump apex balloons to ~1.65 and CLEARS the
    // 1.5 wall/fence barrier (frame-rate-dependent physics; the Phase 99
    // E2E wall-contrast exposed it). Account for the pending impulse so the
    // sub-step split stays tunnel-safe and the jump height is dt-robust.
    if (m_onGround && !m_sitting && !m_inWater && !m_inLava &&
        m_input->isDown(GameKey::Space)) {
        maxSpeed = std::max(maxSpeed, m_params.jumpSpeed);
    }
    // The tick's horizontal speed before the sub-steps (a clamped axis
    // zeroes its velocity inside the sub-steps; stepUp needs the value).
    const float preMoveSpeed =
        std::max(std::abs(m_velocity.x), std::abs(m_velocity.z));
    moveInSubsteps(dt, maxSpeed);
    // V2-19: the toggle press's jump swallow is a ONE-frame flag — the
    // sub-steps above had their jump grants gated on it; clear it for the
    // next update (a Space STILL held on later frames jumps normally, the
    // pre-V2-19 hold convention).
    m_jumpGrantSuppressed = false;

    // 3b. Step up 1 block when flush against a step (not crouching, on
    // the ground).
    if (!m_crouching && m_onGround && lengthSq(wish) > 0.0f) {
        stepUp(dir, preMoveSpeed * static_cast<float>(dt));
    }

    // UX-3: advance the limb-swing phase (render-only — see m_swingPhase).
    // Unconditional (an int op: zero cost, and no input = no displacement
    // = the phase stays 0 forever = the structural-immunity root). The
    // gate for the RENDER is the camera mode, not a config flag.
    {
        const Vec3 pos = position();
        const bool moving = pos.x != prevX || pos.z != prevZ;
        m_swingPhase = advanceSwingPhase(m_swingPhase, moving);
    }

    // 4. Survival (Phase 16): fall damage on landing, regeneration, and
    // the kill plane is now DEATH (the old respawn-teleport is gone).
    // V2-19: this frame's fly state at the update's start (the landing
    // path's fall-damage gate below: a landing WHILE flying is a
    // controlled descent, not a fall — the creative's no-damage landing;
    // the switch-OFF frame reads flying = false, so the free fall after
    // it counts normally from the re-pinned start).
    const bool flying = m_flying;
    if (m_onGround && !m_wasOnGround) {
        // Just landed: the net drop since leaving the ground. No damage
        // for the first fallDamageOffset blocks, 1 HP per block above
        // (Minecraft's rule); landing in water cancels the fall damage.
        // The water check is FRESH (not the tick's m_inWater): a falling
        // player enters a 1-deep pool one tick after the last airborne
        // sample, so the tick state would miss the landing-in-water.
        const float fall = m_fallStartY - m_position.y;
        // ANY fluid cancels the fall (Phase 54: lava like water — the
        // burn replaces the fall damage). V2-19: so does a fly landing
        // (see `flying` above).
        if (fall > m_params.fallDamageOffset && !isInWater() && !isInLava() &&
            !flying) {
            damage(fall - m_params.fallDamageOffset, false,
                   {DamageSource::Fall, {}});
        }
        m_fallStartY = m_position.y;
    }
    if (!m_onGround && m_wasOnGround) {
        m_fallStartY = m_position.y; // just left the ground
    }
    // V2-19: the fly's fall bookkeeping — while flying, the fall-distance
    // accumulator re-pins to this frame's (post-physics) position EVERY
    // frame: no distance accrues in the hover (the gravity skip), and the
    // switch-off frame's free fall counts from the switch height (the
    // last pin) — no burst damage at the switch instant, the normal fall
    // damage after (the landing path's `fall = m_fallStartY -
    // m_position.y`, unchanged).
    if (flying) {
        m_fallStartY = m_position.y;
    }
    m_wasOnGround = m_onGround;

    // Regeneration is GATED by hunger (Phase 23, Minecraft's rule: below
    // the threshold there is no natural regen). The accumulator pauses
    // rather than resets while the gate is closed, so a full meal makes
    // the pending progress count.
    if (m_timeSinceDamage >= m_params.regenDelaySeconds &&
        m_hunger >= m_params.regenHungerThreshold) {
        m_regenAccum += dt;
        while (m_regenAccum >= m_params.regenIntervalSeconds) {
            m_regenAccum -= m_params.regenIntervalSeconds;
            if (m_health < m_params.maxHealth) {
                m_health = std::min(m_params.maxHealth, m_health + 1.0f);
                // SURV-9: the regenerated half heart COSTS one hunger
                // point (Minecraft) — through the standard drain (the
                // hidden saturation buffer first, else the bar). At
                // full health the interval elapses free (no regen, no
                // cost — the pre-SURV-9 clamp was a no-op either way).
                drainOneHungerPoint();
            }
        }
    }
    m_timeSinceDamage += dt;

    // Hunger (Phase 23/28): a decay tick every hungerDecaySeconds of real
    // dt (sprinting doubles the pace — the factor above); each tick first
    // drains 1 of the hidden saturation buffer (the food's satiety, Phase
    // 28), else drops one hunger point. At zero the player starves (1 HP
    // per interval, through the normal damage path so the game's hurt
    // flash/sound and death handling all fire).
    if (!m_creative) {
        // V3-11a: the creative's static bar — the decay AND the
        // starvation tick never run (the damage gate alone would leave
        // the bar draining; the full bar is the creative's read). The
        // wrap (not an early return) keeps the lava/void sections below
        // running (their damage is gated by the creative anyway).
        m_hungerAccum +=
            dt * (sprinting ? m_params.sprintHungerDecayFactor : 1.0);
        if (m_hunger > 0.0f) {
            m_starveAccum = 0.0; // a meal resets the pending starvation tick
        }
        while (m_hungerAccum >= m_params.hungerDecaySeconds) {
            m_hungerAccum -= m_params.hungerDecaySeconds;
            // SURV-9: the shared one-point drain (the buffer first,
            // else the bar — the Phase 28 primitive, now also the
            // regen's cost).
            drainOneHungerPoint();
            if (m_hunger <= 0.0f) {
                m_hungerAccum = 0.0;
                break;
            }
        }
        if (m_hunger <= 0.0f && !m_dead) {
            m_starveAccum += dt;
            while (m_starveAccum >= m_params.starveDamageIntervalSeconds) {
                m_starveAccum -= m_params.starveDamageIntervalSeconds;
                damage(1.0f, false, {DamageSource::Starve, {}});
                if (m_dead) {
                    break;
                }
            }
        }
    }

    // Lava burn (Phase 54, Minecraft's 4 HP/s): while the AABB overlaps a
    // lava cell, take lavaDamage HP every lavaDamageIntervalSeconds of
    // real dt — a survival timer like starvation, through the normal
    // damage path so the hurt flash/sound and death handling all fire.
    // Leaving the lava resets the pending tick (no lingering fire in v1,
    // see KNOWN_ISSUES).
    if (m_inLava && !m_dead) {
        if (hasFireResistantArmor()) {
            // V3-15 (netherite): the fire immunity — the lava's burn
            // tick never lands; the accumulator resets (no lingering
            // damage after the piece comes off).
            m_lavaAccum = 0.0;
        } else {
            m_lavaAccum += dt;
            while (m_lavaAccum >= m_params.lavaDamageIntervalSeconds) {
                m_lavaAccum -= m_params.lavaDamageIntervalSeconds;
                damage(m_params.lavaDamage, false,
                       {DamageSource::Lava, {}});
                if (m_dead) {
                    break;
                }
            }
        }
    } else {
        m_lavaAccum = 0.0;
    }

    // Fire contact (Phase 70) was REPLACED in Phase 74: the fire block
    // now IGNITES the player through the Phase 57 effect path
    // (Game::updateEffects re-applies the fire effect while
    // Player::isInFire — the lingering 8 s burn after leaving, water
    // extinguishes). No direct damage here.

    // Fell out of the world (the world's edge is void): death.
    if (belowKillPlane()) {
        damage(m_params.maxHealth * 1000.0f, false,
               {DamageSource::Void, {}}); // instant, always lethal
    }

    // V3-13: the session's distance stat (the horizontal movement only
    // — the death screen's "行走 N 格"). A teleport / respawn jump
    // exceeds any single-frame motion (the terminal-velocity bound), so
    // the 1.0 threshold skips it without a flag.
    {
        const float dx = std::fabsf(m_position.x - m_updateStartPos.x);
        const float dz = std::fabsf(m_position.z - m_updateStartPos.z);
        if (dx + dz < 1.0f) {
            m_statsDistance += dx + dz;
        }
    }
}

const ItemStack& Player::armorSlot(int slot) const {
    assert(slot >= 0 && slot < kArmorSlotCount);
    return m_armor[static_cast<std::size_t>(slot)];
}

ItemStack& Player::armorSlot(int slot) {
    assert(slot >= 0 && slot < kArmorSlotCount);
    return m_armor[static_cast<std::size_t>(slot)];
}

void Player::setArmorSlot(int slot, const ItemStack& piece, int points,
                          float durability, bool fireResistant) {
    assert(slot >= 0 && slot < kArmorSlotCount);
    m_armor[static_cast<std::size_t>(slot)] = piece;
    // The four per-slot data stay in sync through this single setter
    // (the empty piece forces 0 / false — a corrupted caller cannot
    // leave a piece without its points / durability / effects).
    m_armorPoints[static_cast<std::size_t>(slot)] =
        piece.empty() ? 0 : points;
    m_armorDurability[static_cast<std::size_t>(slot)] =
        piece.empty() ? 0.0f : durability;
    m_armorFireResistant[static_cast<std::size_t>(slot)] =
        piece.empty() ? false : fireResistant;
    // V3-15: the derived equipped immunity (recomputed over all slots —
    // the setter is the single choke point; ANY piece suffices).
    m_fireResistant = false;
    for (std::size_t i = 0; i < m_armorFireResistant.size(); ++i) {
        m_fireResistant = m_fireResistant || m_armorFireResistant[i];
    }
}

int Player::armorPoints() const {
    int sum = 0;
    for (const int p : m_armorPoints) {
        sum += p;
    }
    return std::min(sum, kArmorMaxPoints);
}

bool Player::hasFireResistantArmor() const {
    return m_fireResistant;
}

void Player::setCreative(bool on) {
    m_creative = on;
    if (on) {
        // The creative's full static bar (the entry fills it; the
        // decay gate below keeps it there).
        m_hunger = m_params.maxHunger;
        m_saturation = m_params.maxHunger;
        m_hungerAccum = 0.0;
        m_starveAccum = 0.0;
    }
}

bool Player::damage(float amount, bool bypassArmor) {
    return damage(amount, bypassArmor, DamageInfo {});
}

bool Player::damage(float amount, bool bypassArmor,
                    const DamageInfo& info) {
    if (m_dead || amount <= 0.0f) {
        return false;
    }
    // V3-11a (creative): no damage of any source (the immunity is the
    // mode itself — the lava/fire/fall/mob/arrow/starve/void paths all
    // funnel through this one choke point).
    if (m_creative) {
        return false;
    }
    // SURV-3: the incoming PRE-reduction amount (the armor-wear formula's
    // input — "1 durability for every 4 HP of incoming damage" counts the
    // damage the event STARTED with, not what survived the pipeline).
    const float incoming = amount;
    // SURV-1 (armor, the Minecraft integer-floor rule): the equipped
    // points reduce the incoming damage BEFORE the resistance effect and
    // the absorption pool (the pipeline order armor -> resistance -> pool
    // -> health; the unit tests pin it). pts = the capped sum;
    // amount = floor(amount × (100 − 4×pts)) / 100 — the floor lands on
    // the percent-scaled damage (10.0 × 10 pts = floor(600)/100 = 6.0;
    // 1.0 × 2 pts = floor(92)/100 = 0.92). pts 0 skips the step = the
    // value is bit-identical (every pre-SURV-1 damage test untouched).
    // SURV-2 (drowning, the Minecraft exemption — the wiki damage table
    // "Drowning: Armor: No effect / Resistance: Applies"): bypassArmor
    // skips THIS armor step only — the resistance multiply and the pool
    // drain below run unchanged (drowning calls damage(2.0f, true)).
    // The default false keeps every other call site bit-identical.
    // Deviation from P84: the resistance is a float multiply, the armor
    // is the floor rule (Minecraft's integer-percentage semantics).
    // V2-12 (protection): the effective points = the equipped points + the
    // protection bonus, capped at kArmorMaxPoints (the SHARED 20-point cap).
    // No protection worn = m_protBonus 0 = pts = armorPoints() = the
    // pre-V2-12 bit-identical value (the red line the damage tests pin).
    const int pts =
        std::min(kArmorMaxPoints, armorPoints() + m_protBonus);
    if (pts > 0 && !bypassArmor) {
        amount = std::floor(
                     amount * (100.0f - kArmorReductionPerPoint *
                                          static_cast<float>(pts))) /
                 100.0f;
    }
    // Phase 84: the beacon's resistance effect reduces the incoming damage
    // BEFORE the absorption pool drains it (the game pushes the effect-
    // derived reduction each frame; 0.0 when none is active).
    amount *= 1.0f - m_damageReduction;
    if (amount <= 0.0f) {
        return false;
    }
    // SURV-3 (armor wear, the Minecraft float rule): this event is one the
    // armor ACTUALLY reduced (not bypassed, points equipped, and the
    // post-reduction amount still lands — the "armor-reduction-effective
    // event"): every worn breakable piece takes max(1, incoming / 4)
    // (10 -> 2.5 exact, 1 -> 1). A piece whose accumulated damage reaches
    // its data durability BREAKS in place: the slot clears through the
    // single setter and its points drop out of the total (the damage
    // already settled — the action is unaffected, the item is gone). The
    // wear follows the block rule (the project's alignment of the wiki's
    // no-wear-source list with the bypassArmor exemption): a source the
    // armor does not reduce (bypass — today: drowning) never wears it.
    if (pts > 0 && !bypassArmor) {
        const float wear = armorWearPerPiece(incoming);
        for (int slot = 0; slot < kArmorSlotCount; ++slot) {
            if (m_armor[static_cast<std::size_t>(slot)].empty()) {
                continue;
            }
            // V2-12 (unbreaking): the armor piece's unbreaking level (0 =
            // none, the pre-V2-12 bit-identical wear) rolls the per-point
            // skip (the state from the world seed + the piece's damage
            // BEFORE this event + its item id — no frame dependence).
            const int L =
                m_armorUnbreaking[static_cast<std::size_t>(slot)];
            const float applied =
                unbreakingWear(L, /*isArmor=*/true, wear,
                               unbreakingState(
                                   m_worldSeed,
                                   m_armor[static_cast<std::size_t>(slot)]
                                       .damage,
                                   m_armor[static_cast<std::size_t>(slot)]
                                       .item.value));
            if (applied > 0.0f &&
                applyDurability(
                    m_armor[static_cast<std::size_t>(slot)], applied,
                    m_armorDurability[static_cast<std::size_t>(slot)])) {
                setArmorSlot(slot, ItemStack {}, 0); // the BREAK: clear +
                                                     // the points resync
            }
        }
    }
    // Phase 72 (the two-component model, Minecraft): the absorption pool
    // drains FIRST — the health is touched only by the remainder. The pool
    // alone can never kill (the death comes from the health).
    const float absorbed = std::min(m_absorption, amount);
    m_absorption -= absorbed;
    amount -= absorbed;
    if (amount > 0.0f) {
        m_health -= amount;
    }
    m_timeSinceDamage = 0.0;
    m_regenAccum = 0.0;
    if (m_health <= 0.0f) {
        m_health = 0.0f;
        m_dead = true;
        // V3-13: the KILLING blow's source owns the death cause (the
        // heal's None default never overwrites). V3-15: a fire-effect
        // killing blow while in the lava reads as the LAVA (the
        // ignition's ticks do the killing, but Minecraft's death
        // message names the element — the inLava state is the
        // discriminator).
        if (info.source != DamageSource::None) {
            m_deathInfo = info;
            if (m_deathInfo.source == DamageSource::Effect &&
                m_inLava) {
                m_deathInfo.source = DamageSource::Lava;
            }
        }
    }
    return true;
}

std::string Player::deathCauseLine(const DamageInfo& info) {
    // V3-13: the font-safe cause strings (every CJK glyph in the 430-
    // char font set — the 摔/掘/外-missing lesson checked per char;
    // V3-14a's 爆/炸 were the +2 that grew the set 428 -> 430).
    switch (info.source) {
        case DamageSource::Fall:
            return "死于高处掉落";
        case DamageSource::Void:
            return "死于世界之外";
        case DamageSource::Starve:
            return "饿死";
        case DamageSource::Lava:
            return "死于熔岩";
        case DamageSource::Drown:
            return "死于落水";
        case DamageSource::Arrow:
            return "被箭射死";
        case DamageSource::Mob:
            return "被 " + info.name + " 杀死";
        case DamageSource::Explosion:
            // The named blast (the ghast's, V3-14b) reads like the
            // Mob line's twin; the unattributed one (the --explode
            // seam) is the bare cause.
            if (info.name.empty()) {
                return "死于爆炸";
            }
            return "被 " + info.name + " 的爆炸杀死";
        case DamageSource::Effect:
            if (info.name == "fire") {
                return "死于燃烧";
            }
            if (info.name == "poison") {
                return "死于毒";
            }
            return "死于药水";
        case DamageSource::None:
        default:
            return "死了";
    }
}

void Player::respawn() {
    m_dead = false;
    m_deathInfo = {}; // V3-13: the cause dies with the corpse
    // Respawn starts clean (Phase 72): the pool dies with the player
    // (the game clears the effects on death), so the absorption is 0 and
    // the health refills to the base max.
    m_absorption = 0.0f;
    m_health = m_params.maxHealth;
    m_timeSinceDamage = 0.0;
    m_regenAccum = 0.0;
    // Respawn also refills the hunger (Minecraft: a fresh start is full).
    m_hunger = m_params.maxHunger;
    // A full meal: the full bar plus the full hidden buffer (Phase 28).
    m_saturation = m_params.maxHunger;
    m_hungerAccum = 0.0;
    m_starveAccum = 0.0;
    m_lavaAccum = 0.0; // respawn out of the lava (spawn is dry land)
    m_wasOnGround = false;
    m_fallStartY = m_spawnPos.y;
    // V2-19: the death / respawn resets the fly (session state — the
    // player respawns on the ground, no creative carry-over).
    m_flying = false;
    m_jumpGrantSuppressed = false;
    teleportTo(m_spawnPos);
}

void Player::updateCrouchHeight() {
    if (m_crouching) {
        m_height = m_params.crouchHeight;
    } else if (m_height < m_params.height) {
        // Stand back up only when the headroom fits the full height
        // (trapped under a low ceiling: stay crouched).
        if (hasRoomAt(*m_world, m_blocks, m_position, m_params.width,
                      m_params.height)) {
            m_height = m_params.height;
        }
    }
}

void Player::applyCrouchEdgeGuard(const Vec3& dir, double dt) {
    const float half = m_width * 0.5f;
    // The SUPPORT cell under the front face: on the ground the feet sit
    // exactly on an integer boundary, so the support block is one cell
    // BELOW floor(position.y) (floor(position.y) is the air the feet are
    // in). The guard only runs while onGround, so the subtraction is safe.
    const int fy = static_cast<int>(std::floorf(m_position.y - kFaceEps));
    // PREDICTIVE: test the cell the AABB face would reach at the END of
    // this tick (face + this tick's remaining move, velocity * dt). The
    // guard can only zero the velocity (it cannot rewind an overshoot), so
    // looking one tick ahead keeps the face from ever crossing the edge.
    if (std::abs(dir.x) > 0.01f) {
        const float reach = m_position.x + dir.x * (half + kFaceEps) +
                            m_velocity.x * static_cast<float>(dt);
        const int cx = static_cast<int>(std::floorf(reach));
        if (!isSolidCell(cx, fy,
                         static_cast<int>(std::floorf(m_position.z)))) {
            m_velocity.x = 0.0f;
        }
    }
    if (std::abs(dir.z) > 0.01f) {
        const float reach = m_position.z + dir.z * (half + kFaceEps) +
                            m_velocity.z * static_cast<float>(dt);
        const int cz = static_cast<int>(std::floorf(reach));
        if (!isSolidCell(static_cast<int>(std::floorf(m_position.x)), fy,
                         cz)) {
            m_velocity.z = 0.0f;
        }
    }
}

bool Player::isSolidCell(int x, int y, int z) const {
    const BlockID id = m_world->getBlock(x, y, z);
    if (!id.valid() || id == BlockID::air()) {
        return false;
    }
    return m_blocks.at(id).properties.solid;
}

void Player::integrate(float h) {
    if (m_inLava) {
        // Swim in lava (Phase 54): the same swim state, the lava pace
        // (lighter, slower sink; a much weaker swim-up).
        m_velocity.y -= m_params.lavaGravity * h;
        if (m_velocity.y < -m_params.lavaTerminalVelocity) {
            m_velocity.y = -m_params.lavaTerminalVelocity;
        }
        if (m_input->isDown(GameKey::Space)) {
            m_velocity.y = m_params.lavaSwimUpSpeed;
        }
    } else if (m_inWater || m_atSurface) {
        // Swim (Phase 9) + the FLUID-2 surface model. Priority:
        // Space > crouch > float snap > passive rise > floor stand.
        if (m_input->isDown(GameKey::Space)) {
            // Submerged: keep surfacing at swimUpSpeed (the Phase-9
            // behavior, also from the ground — a shallow pool floor does
            // not jump, it surfaces). At the surface (no water overlap):
            // a normal jump (the player has breached — the jump height
            // ~1.13 clears a 1-block rim, the Phase-99 barrier
            // arithmetic).
            m_velocity.y = m_inWater
                ? m_params.swimUpSpeed
                : m_params.jumpSpeed;
        } else if (m_crouching) {
            // Crouch-dive (FLUID-2, Minecraft): sink at waterSinkSpeed.
            m_velocity.y =
                std::max(m_velocity.y - m_params.waterBuoyancy * h,
                         -m_params.waterSinkSpeed);
        } else if (m_atSurface && !m_onGround && m_velocity.y >= 0.0f &&
                   (m_position.y >= m_surfaceY - kFaceEps ||
                    m_position.y + m_velocity.y * h >= m_surfaceY - kFaceEps) &&
                   (m_position.y <= m_surfaceY + kFaceEps ||
                    m_velocity.y <= m_params.waterRiseSpeed + kFaceEps)) {
            // Float snap (FLUID-2): the geometric clamp to the surface
            // plane. The crossing re-check: a sub-step's displacement (up
            // to kMaxSubstepMove) skips the kFaceEps band, so the snap
            // fires when the sub-step reaches the plane from below (or
            // the feet are already at/above it); the rise-speed gate
            // keeps the surface jump (vy up to jumpSpeed) from being
            // pulled back. The terminal (y = W', vy = 0, onGround false)
            // is a fixed point — no input re-snaps it every tick, so it
            // is fps-independent.
            m_position.y = m_surfaceY;
            m_velocity.y = 0.0f;
        } else if (m_inWater && !isRestingOnSolid() && m_surfaceY > 0.0f &&
                   m_velocity.y >= 0.0f) {
            // Passive rise (FLUID-2, Minecraft: a submerged body at rest
            // or moving up surfaces slowly): buoyancy, capped at
            // waterRiseSpeed. Gated on vy >= 0 — a FALLING body (the
            // water entry with momentum, a jump returning) keeps sinking:
            // the entry-momentum clamp to -waterTerminalVelocity is kept
            // in the Phase-9 line below, bit-identical (the MC "dive"
            // semantics: a falling body sinks to the bottom, a still
            // body floats up). Also gated on resting-on-a-solid (a body
            // on the pool bottom stands — the FRESH check, the tick
            // m_onGround is the previous sub-step's result) and on a
            // water surface in the column (a pure flow film gives no
            // buoyancy).
            m_velocity.y = std::min(m_velocity.y + m_params.waterBuoyancy * h,
                                    m_params.waterRiseSpeed);
        } else if (m_inWater) {
            // The Phase-9 sink, bit-identical: grounded in water (pool
            // floor, wading — the "stand on the bottom" case; the small
            // downward drift is what the Y resolve re-clamps against the
            // floor every sub-step, re-establishing onGround — a zeroed
            // vy would never re-penetrate the face, the resting-air
            // convention), falling (the entry momentum, clamped at the
            // terminal), or in a column with no water surface (a pure
            // flow film).
            m_velocity.y -= m_params.waterBuoyancy * h;
            if (m_velocity.y < -m_params.waterTerminalVelocity) {
                m_velocity.y = -m_params.waterTerminalVelocity;
            }
        } else {
            // Surface band, descending (diving) or in a jump above the
            // plane: keep vy; the next tick's state re-resolves it.
        }
    } else {
        // V2-19: the fly (the creative-style double-tap Space toggle) —
        // the gravity skip (the vy baseline 0, no acceleration) + the
        // fixed-pace vertical control: Space = ascend, LeftShift =
        // descend (the crouch mechanism runs as usual — the height shrink
        // + the step-up prohibition are the existing paths, the Shift's
        // dual meaning is the creative's), neither = hover. The
        // horizontal move, the collision (the moveInSubsteps AABB, the
        // creative's wall bump) and the tunneling sub-steps are UNCHANGED.
        if (m_flying) {
            m_velocity.y =
                m_input->isDown(GameKey::Space)
                    ? kFlyVerticalSpeed
                    : (m_input->isDown(GameKey::LeftShift)
                           ? -kFlyVerticalSpeed
                           : 0.0f);
            return;
        }
        // Gravity, capped at terminal velocity.
        m_velocity.y -= m_params.gravity * h;
        if (m_velocity.y < -m_params.terminalVelocity) {
            m_velocity.y = -m_params.terminalVelocity;
        }
        // Jump: only from the ground (m_onGround is the previous
        // sub-step's result, so one press cannot re-trigger mid-air) and
        // never while sitting (Phase 71 — standing up is uncrouching, not
        // jumping). V2-19: never on the toggle press's frame either —
        // the double-tap's second tap SWALLOWS the jump (the creative's
        // toggle consumes it: on the toggle-ON frame the fly branch
        // above owns Space, on the toggle-OFF frame this gate does).
        if (m_input->isDown(GameKey::Space) && m_onGround && !m_sitting &&
            !m_jumpGrantSuppressed) {
            m_velocity.y = m_params.jumpSpeed;
        }
    }
}

bool Player::isInWater() const {
    return overlapsFluid(FluidKind::Water);
}

bool Player::isInLava() const {
    // LAVA (Phase 54): the swim state with the lava pace + the burn timer
    // run while this is true (Minecraft's "slow + burn").
    return overlapsFluid(FluidKind::Lava);
}

bool Player::isInFire() const {
    // FIRE (Phase 70): the contact-damage source. The fire is a BLOCK
    // (the cross-model fire, never a fluid), so this scans block cells
    // for the constructor-resolved fire id; an invalid id (no fire block
    // registered) means "never in fire".
    return overlapsBlock(m_fireBlockId);
}

} // namespace mc
