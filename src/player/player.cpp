#include "player/player.hpp"

#include <algorithm>
#include <cmath>

#include "block/block.hpp"
#include "block/block_registry.hpp"
#include "core/math/vec2.hpp"

namespace mc {

Player::Player(const World& world, const BlockRegistry& blocks, Vec3 spawnPos)
    : Player(world, blocks, spawnPos, Params {}) {
}

Player::Player(const World& world, const BlockRegistry& blocks, Vec3 spawnPos,
               Params params)
    : Entity(world, blocks, spawnPos, params.width, params.height),
      m_params(params),
      m_spawnPos(spawnPos),
      // Phase 70: resolve the fire block id once (the registry is static
      // after init). An unknown name leaves the id invalid = isInFire is
      // always false (a test fixture without the fire block — the
      // fluid.cpp fallback pattern).
      m_fireBlockId(blocks.idForName("fire")),
      // Phase 71: the campfire id the same way (unknown = never sitting).
      m_campfireBlockId(blocks.idForName("campfire")) {
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
    MC_TRY(read("water_gravity", p.waterGravity, false));
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
    MC_TRY(read("fire_damage_interval_seconds",
                p.fireDamageIntervalSeconds, false));
    MC_TRY(read("fire_damage", p.fireDamage, true)); // 0 = off (allowed)
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
    if (p.fireDamage > p.maxHealth) {
        return MC_FAIL("config player.fire_damage must be <= "
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
    // swims (reduced gravity/sink, reduced speed, Space swims up). Lava
    // (Phase 54) is the same swim state with the lava pace + the burn
    // timer; when the AABB straddles both kinds, lava wins.
    m_inWater = isInWater();
    m_inLava = isInLava();
    m_inFire = isInFire(); // Phase 70: the fire block (contact damage)

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
        (m_campfireBlockId.valid()
             ? overlapsBlock(m_campfireBlockId)
             : false);

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
    const bool sprinting =
        m_input->isDown(GameKey::LeftShift) && lengthSq(wish) > 0.0f &&
        !m_inWater && !m_inLava; // no sprint in any fluid (lava, Phase 54)
    Vec3 dir {0.0f, 0.0f, 0.0f};
    if (lengthSq(wish) > 0.0f) {
        float speed = m_inLava
                          ? m_params.lavaSwimSpeed
                          : (m_inWater
                                  ? m_params.swimSpeed
                                  : (m_input->isDown(GameKey::LeftShift)
                                          ? m_params.sprintSpeed
                                          : m_params.walkSpeed));
        if (m_crouching && !m_inWater && !m_inLava) {
            speed *= m_params.crouchSpeedScale;
        }
        dir = normalized(wish);
        m_velocity.x = dir.x * speed;
        m_velocity.z = dir.z * speed;
        // Crouching on the ground: do not walk off a 1-block ledge.
        if (m_crouching && m_onGround) {
            applyCrouchEdgeGuard(dir, dt);
        }
    } else {
        m_velocity.x = 0.0f;
        m_velocity.z = 0.0f;
    }

    // 3. Sub-step the tick (see Entity::moveInSubsteps); the peak-speed
    //    estimate uses this tick's post-gravity vertical velocity.
    const float vyAfter =
        m_inLava
            ? std::max(m_velocity.y - m_params.lavaGravity *
                                      static_cast<float>(dt),
                       -m_params.lavaTerminalVelocity)
            : (m_inWater
                   ? std::max(m_velocity.y - m_params.waterGravity *
                                     static_cast<float>(dt),
                              -m_params.waterTerminalVelocity)
                   : std::max(m_velocity.y - m_params.gravity *
                                  static_cast<float>(dt),
                              -m_params.terminalVelocity));
    const float maxSpeed =
        std::max({std::abs(m_velocity.x), std::abs(vyAfter),
                  std::abs(m_velocity.z)});
    // The tick's horizontal speed before the sub-steps (a clamped axis
    // zeroes its velocity inside the sub-steps; stepUp needs the value).
    const float preMoveSpeed =
        std::max(std::abs(m_velocity.x), std::abs(m_velocity.z));
    moveInSubsteps(dt, maxSpeed);

    // 3b. Step up 1 block when flush against a step (not crouching, on
    // the ground).
    if (!m_crouching && m_onGround && lengthSq(wish) > 0.0f) {
        stepUp(dir, preMoveSpeed * static_cast<float>(dt));
    }

    // 4. Survival (Phase 16): fall damage on landing, regeneration, and
    // the kill plane is now DEATH (the old respawn-teleport is gone).
    if (m_onGround && !m_wasOnGround) {
        // Just landed: the net drop since leaving the ground. No damage
        // for the first fallDamageOffset blocks, 1 HP per block above
        // (Minecraft's rule); landing in water cancels the fall damage.
        // The water check is FRESH (not the tick's m_inWater): a falling
        // player enters a 1-deep pool one tick after the last airborne
        // sample, so the tick state would miss the landing-in-water.
        const float fall = m_fallStartY - m_position.y;
        // ANY fluid cancels the fall (Phase 54: lava like water — the
        // burn replaces the fall damage).
        if (fall > m_params.fallDamageOffset && !isInWater() && !isInLava()) {
            damage(fall - m_params.fallDamageOffset);
        }
        m_fallStartY = m_position.y;
    }
    if (!m_onGround && m_wasOnGround) {
        m_fallStartY = m_position.y; // just left the ground
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
            m_health = std::min(m_params.maxHealth, m_health + 1.0f);
        }
    }
    m_timeSinceDamage += dt;

    // Hunger (Phase 23/28): a decay tick every hungerDecaySeconds of real
    // dt (sprinting doubles the pace — the factor above); each tick first
    // drains 1 of the hidden saturation buffer (the food's satiety, Phase
    // 28), else drops one hunger point. At zero the player starves (1 HP
    // per interval, through the normal damage path so the game's hurt
    // flash/sound and death handling all fire).
    m_hungerAccum +=
        dt * (sprinting ? m_params.sprintHungerDecayFactor : 1.0);
    if (m_hunger > 0.0f) {
        m_starveAccum = 0.0; // a meal resets the pending starvation tick
    }
    while (m_hungerAccum >= m_params.hungerDecaySeconds) {
        m_hungerAccum -= m_params.hungerDecaySeconds;
        if (m_saturation > 0.0f) {
            m_saturation = std::max(0.0f, m_saturation - 1.0f);
        } else {
            m_hunger = std::max(0.0f, m_hunger - 1.0f);
            if (m_hunger <= 0.0f) {
                m_hungerAccum = 0.0;
                break;
            }
        }
    }
    if (m_hunger <= 0.0f && !m_dead) {
        m_starveAccum += dt;
        while (m_starveAccum >= m_params.starveDamageIntervalSeconds) {
            m_starveAccum -= m_params.starveDamageIntervalSeconds;
            damage(1.0f);
            if (m_dead) {
                break;
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
        m_lavaAccum += dt;
        while (m_lavaAccum >= m_params.lavaDamageIntervalSeconds) {
            m_lavaAccum -= m_params.lavaDamageIntervalSeconds;
            damage(m_params.lavaDamage);
            if (m_dead) {
                break;
            }
        }
    } else {
        m_lavaAccum = 0.0;
    }

    // Fire contact (Phase 70, Minecraft's 1 HP / 0.2 s): while the AABB
    // overlaps a FIRE BLOCK cell, take fireDamage HP every
    // fireDamageIntervalSeconds of real dt — the lava-burn pattern,
    // through the normal damage path (hurt flash/sound/death all fire).
    // Leaving the fire resets the pending tick. fireDamage 0 disables the
    // damage entirely (the config off-switch).
    if (m_inFire && !m_dead && m_params.fireDamage > 0.0f) {
        m_fireAccum += dt;
        while (m_fireAccum >= m_params.fireDamageIntervalSeconds) {
            m_fireAccum -= m_params.fireDamageIntervalSeconds;
            damage(m_params.fireDamage);
            if (m_dead) {
                break;
            }
        }
    } else {
        m_fireAccum = 0.0;
    }

    // Fell out of the world (the world's edge is void): death.
    if (belowKillPlane()) {
        damage(m_params.maxHealth * 1000.0f); // instant, always lethal
    }
}

bool Player::damage(float amount) {
    if (m_dead || amount <= 0.0f) {
        return false;
    }
    m_health -= amount;
    m_timeSinceDamage = 0.0;
    m_regenAccum = 0.0;
    if (m_health <= 0.0f) {
        m_health = 0.0f;
        m_dead = true;
    }
    return true;
}

void Player::setAbsorptionBonus(float bonus, bool healOnIncrease) {
    const float oldBonus = m_absorptionBonus;
    m_absorptionBonus = bonus;
    if (healOnIncrease && bonus > oldBonus) {
        // The layer appears full: raise the health by the added amount
        // (clamped at the new effective max).
        m_health = std::clamp(m_health + (bonus - oldBonus), 0.0f,
                              maxHealth());
    }
    // A shrunk/vanished layer drops the excess SILENTLY (no damage
    // flash/sound, no damage-clock reset: the layer is not real HP).
    if (m_health > maxHealth()) {
        m_health = maxHealth();
    }
}

void Player::respawn() {
    m_dead = false;
    // Respawn starts clean (Phase 56): the layer dies with the player
    // (the game clears the effects on death), so the bonus is 0 and the
    // health refills to the BASE max.
    m_absorptionBonus = 0.0f;
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
    teleportTo(m_spawnPos);
}

void Player::updateCrouchHeight() {
    if (m_crouching) {
        m_height = m_params.crouchHeight;
    } else if (m_height < m_params.height) {
        // Stand back up only when the headroom fits the full height
        // (trapped under a low ceiling: stay crouched).
        if (hasRoomAt(m_world, m_blocks, m_position, m_params.width,
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

void Player::stepUp(const Vec3& wishDir, float distPerTick) {
    const float half = m_width * 0.5f;
    const int fx0 = static_cast<int>(std::floorf(m_position.x));
    const int fy = static_cast<int>(std::floorf(m_position.y));
    const int fz0 = static_cast<int>(std::floorf(m_position.z));
    // x axis first, then z (fixed order); at most one step per tick.
    for (int axis = 0; axis < 2; ++axis) {
        const float w = axis == 0 ? wishDir.x : wishDir.z;
        if (std::abs(w) <= 0.01f) {
            continue;
        }
        // Only a player flush against a block face can be blocked by a
        // step; the face position is exact after the collision clamp.
        const float face = axis == 0
            ? m_position.x + (w > 0.0f ? 1.0f : -1.0f) * half
            : m_position.z + (w > 0.0f ? 1.0f : -1.0f) * half;
        const float snapped = std::roundf(face);
        if (std::abs(face - snapped) > kFaceEps) {
            continue;
        }
        const int front = static_cast<int>(snapped) + (w > 0.0f ? 1 : -1);
        // axis 0 is X, axis 1 is Z (the loop index, NOT the moveAxis
        // numbering 0=x, 1=y, 2=z — that mapping is applied at the
        // moveAxis call below).
        const int cx = axis == 0 ? front : fx0;
        const int cz = axis == 1 ? front : fz0;
        // The step exists (solid at foot level) and the lifted player
        // fits (the two cells above the step are free).
        if (!isSolidCell(cx, fy, cz) || isSolidCell(cx, fy + 1, cz) ||
            isSolidCell(cx, fy + 2, cz)) {
            continue;
        }
        m_position.y += 1.0f;
        // Re-apply this tick's horizontal move along the stepped axis: the
        // sub-steps consumed it, clamped against the step face. The
        // per-tick distance (speed*dt, max 5.6/60 ~ 0.093 blocks) is well
        // under kMaxSubstepMove, so one moveAxis call is tunnel-safe.
        moveAxis(axis == 0 ? 0 : 2, w * distPerTick);
        return;
    }
}

bool Player::isSolidCell(int x, int y, int z) const {
    const BlockID id = m_world.getBlock(x, y, z);
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
    } else if (m_inWater) {
        // Swim: reduced gravity, a low sink cap, and Space raises the
        // player (also from the ground — a shallow pool floor does not
        // jump, it surfaces).
        m_velocity.y -= m_params.waterGravity * h;
        if (m_velocity.y < -m_params.waterTerminalVelocity) {
            m_velocity.y = -m_params.waterTerminalVelocity;
        }
        if (m_input->isDown(GameKey::Space)) {
            m_velocity.y = m_params.swimUpSpeed;
        }
    } else {
        // Gravity, capped at terminal velocity.
        m_velocity.y -= m_params.gravity * h;
        if (m_velocity.y < -m_params.terminalVelocity) {
            m_velocity.y = -m_params.terminalVelocity;
        }
        // Jump: only from the ground (m_onGround is the previous
        // sub-step's result, so one press cannot re-trigger mid-air) and
        // never while sitting (Phase 71 — standing up is uncrouching, not
        // jumping).
        if (m_input->isDown(GameKey::Space) && m_onGround && !m_sitting) {
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
