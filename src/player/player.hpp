// Player: the first-person actor.
//
// The position/AABB convention, the sub-stepped axis-separated collision
// model and the kill plane live in the Entity base (see entity.hpp). This
// class adds the player-specific parts: orientation (yaw/pitch) and the
// input-driven movement model.
//
// Movement model (per update):
//   1. mouse look (yaw/pitch, pitch clamped to +/- ~89 deg)
//   2. desired horizontal velocity from WASD relative to yaw (walk/sprint)
//   3. sub-stepped physics (Entity::moveInSubsteps): per sub-step gravity
//      (or swim physics while the AABB overlaps a fluid cell), a Space jump
//      while onGround (or swim-up in water), then the X/Z/Y collision moves
//   4. below the kill plane: respawn at spawn
//
// Swimming (Phase 9, while the AABB overlaps a fluid cell): reduced gravity
// and sink speed, reduced horizontal speed, and Space swims upward instead
// of jumping (also from the ground — a shallow pool floor does not jump, it
// surfaces).
//
// The World + BlockRegistry are the sole source of solid/air (via Entity):
// the player reads properties.solid and never switches on block identity.
#pragma once

#include <algorithm>

#include "core/input.hpp"
#include "core/math/vec3.hpp"
#include "entity/entity.hpp"
#include "world/world.hpp"

namespace mc {

class Player : public Entity {
public:
    struct Params {
        float width = 0.6f;
        float height = 1.8f;
        float eyeHeight = 1.62f;
        float walkSpeed = 4.3f;      // blocks / second
        float sprintSpeed = 5.6f;
        float jumpSpeed = 8.5f;      // initial vertical velocity (blocks/s)
        float gravity = 32.0f;       // downward acceleration (blocks/s^2)
        float terminalVelocity = 38.0f; // max fall speed (blocks/s)
        // Swimming (while the AABB overlaps a fluid cell): reduced gravity
        // and sink speed, a lower horizontal speed, and Space swims upward
        // instead of jumping (Minecraft-style values).
        float swimSpeed = 3.1f;         // horizontal speed in water (blocks/s)
        float swimUpSpeed = 4.5f;       // vertical velocity while swimming up
        float waterGravity = 12.0f;     // downward acceleration in water (blocks/s^2)
        float waterTerminalVelocity = 4.0f; // max sink speed in water (blocks/s)
        float mouseSensitivity = 0.0025f; // radians / pixel
        // Survival (Phase 16): health and fall damage. No fall damage for
        // the first `fallDamageOffset` blocks (Minecraft: 3 are safe), then
        // 1 HP per block.
        float maxHealth = 20.0f;
        float fallDamageOffset = 3.0f;
        // Crouch (LeftControl): the AABB shrinks to crouchHeight, movement
        // scales by crouchSpeedScale, and the player does not walk off a
        // 1-block ledge while on the ground.
        float crouchHeight = 1.6f;
        float crouchSpeedScale = 0.3f;
        // Regeneration (Minecraft's hunger regen, simplified): after
        // regenDelaySeconds without damage, +1 HP every regenIntervalSeconds.
        // Phase 23: the regen additionally requires hunger >=
        // regenHungerThreshold (Minecraft: below 6 hunger points there is
        // no natural regen).
        double regenDelaySeconds = 10.0;
        double regenIntervalSeconds = 4.0;
        // Hunger (Phase 23): maxHunger hunger points (10 drumstick icons,
        // 2 each); hungerDecaySeconds per -1 point (Minecraft's base
        // survival pace, no saturation buffer in v1); at ZERO hunger the
        // player takes 1 HP every starveDamageIntervalSeconds
        // (Minecraft's starvation); regenHungerThreshold gates the regen
        // above.
        float maxHunger = 20.0f;
        double hungerDecaySeconds = 4.0;
        double starveDamageIntervalSeconds = 4.0;
        float regenHungerThreshold = 6.0f;
    };

    // `world` and `blocks` must outlive the player. `spawnPos` is the
    // respawn point (bottom-center AABB position).
    Player(const World& world, const BlockRegistry& blocks, Vec3 spawnPos);

    // Advance the simulation by dt seconds (Entity::update virtual). The
    // controls (polled input + look state) must have been set by the
    // 3-argument overload below first.
    void update(double dt) override;

    // Advance the simulation by dt seconds using the polled input.
    // Mouse look is applied only when `lookEnabled` (the game passes the
    // window's pointer-captured state, so an unlocked cursor never rotates
    // the view).
    void update(double dt, const Input& input, bool lookEnabled);

    // Is the AABB (bottom-center + given dimensions) free of solid blocks?
    // Used for spawn point validation.
    static bool hasRoomAt(const World& world, const BlockRegistry& blocks,
                          Vec3 bottomCenter, float width, float height);

    // -- state ---------------------------------------------------------------
    // Orientation conventions (shared with Camera): yaw 0 = facing -Z,
    // positive turns left; pitch positive looks up (clamped in update()).
    float yaw = 0.0f;
    float pitch = 0.0f;

    // Eye height: the standing value, lowered by the crouch delta (the eye
    // stays a fixed distance below the top of the head).
    float currentEyeHeight() const {
        return m_crouching
            ? m_params.eyeHeight -
                  (m_params.height - m_params.crouchHeight)
            : m_params.eyeHeight;
    }

    Vec3 eyePosition() const {
        return position() + Vec3{0.0f, currentEyeHeight(), 0.0f};
    }

    const Params& params() const { return m_params; }

    // -- survival (Phase 16) --------------------------------------------------
    float health() const { return m_health; }
    float maxHealth() const { return m_params.maxHealth; }
    bool isDead() const { return m_dead; }
    bool isCrouching() const { return m_crouching; }
    double timeSinceDamage() const { return m_timeSinceDamage; }

    // -- hunger (Phase 23) -----------------------------------------------------
    float hunger() const { return m_hunger; }
    float maxHunger() const { return m_params.maxHunger; }

    // Restore `amount` hunger points (eating): clamped to [0, max].
    void restoreHunger(float amount) {
        m_hunger = std::clamp(m_hunger + amount, 0.0f, m_params.maxHunger);
    }

    // Set the hunger (save load / tests): clamped to [0, max]. Zero hunger
    // does NOT kill — starvation damage in update() is the death path.
    void setHunger(float h) {
        m_hunger = std::clamp(h, 0.0f, m_params.maxHunger);
    }

    // Apply `amount` damage. Returns true when health actually decreased
    // (the game drives the hurt sound/flash from this). Lethal damage sets
    // the dead state; a dead player is frozen (update() no-ops).
    bool damage(float amount);

    // Back at the spawn point with full health and cleared fall state. The
    // game calls this on the R key. The inventory is untouched
    // (Minecraft-Java keep-inventory respawn).
    void respawn();

    // Restore health (save load): clamped to [0, max]; 0 sets the dead
    // state (a dead save reloads dead).
    void setHealth(float h) {
        m_health = std::clamp(h, 0.0f, m_params.maxHealth);
        m_dead = m_health <= 0.0f;
    }

    // Entity separation (Phases 16/19): the player is immovable — entities
    // are pushed out of the player (Minecraft's behavior), never the other
    // way around.
    bool pushableByEntities() const override { return false; }

private:
    // One physics sub-step (Entity::integrate): swim/air gravity + the
    // jump. `look`/`inWater` are tick state, consumed per sub-step.
    void integrate(float h) override;

    // The AABB overlaps at least one fluid cell (water).
    bool isInWater() const;

    // Crouch height state machine: shrink to crouchHeight while the key is
    // held (even mid-air); grow back only when the headroom fits the full
    // height. The feet never move (position is the bottom center).
    void updateCrouchHeight();

    // Crouching on the ground: hold the player back at a 1-block ledge
    // (Minecraft behavior). Per wish axis, the cell the AABB face would
    // reach by the end of this tick (predictive: the guard can only zero
    // the velocity, not rewind an overshoot) at support level must be
    // solid or the axis's velocity is zeroed.
    void applyCrouchEdgeGuard(const Vec3& dir, double dt);

    // Step up 1 block when flush against a 1-block step (not crouching, on
    // the ground, step cell solid, the two cells above it free). On success
    // the player is lifted and this tick's horizontal move is re-applied
    // along the stepped axis (x axis first, then z; one step per tick).
    void stepUp(const Vec3& wishDir, float distPerTick);

    // A world cell is solid (air and unregistered ids are not).
    bool isSolidCell(int x, int y, int z) const;

    Params m_params;
    Vec3 m_spawnPos {0.0f, 0.0f, 0.0f};
    // Tick state for update()/integrate(): the polled input, the look
    // state, and this tick's water state (computed once per update, not
    // per sub-step).
    const Input* m_input = nullptr;
    bool m_lookEnabled = false;
    bool m_inWater = false;
    // Crouch state this tick (set in update before the speed computation).
    bool m_crouching = false;
    // Survival: health, the damage clock (regen delay), the regen
    // accumulator (only runs after the delay), the fall tracking (net drop
    // since the last leave-the-ground), and the death state.
    float m_health = Params {}.maxHealth;
    double m_timeSinceDamage = 0.0;
    double m_regenAccum = 0.0;
    bool m_wasOnGround = false;
    float m_fallStartY = 0.0f;
    bool m_dead = false;
    // Hunger (Phase 23): the level, the decay accumulator (only runs while
    // hungry), and the starvation accumulator (only runs at zero).
    float m_hunger = Params {}.maxHunger;
    double m_hungerAccum = 0.0;
    double m_starveAccum = 0.0;

    static constexpr float kMaxPitch = 1.5533f; // ~89 deg
    // The crouch edge guard / step-up treat an AABB face as "flush" with a
    // block face within this tolerance (the collision clamp is exact, so
    // this only guards float round-trips).
    static constexpr float kFaceEps = 1e-4f;
};

} // namespace mc
