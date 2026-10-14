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

#include "core/config.hpp"
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
        // Swimming in LAVA (Phase 54, the Minecraft "slow + burn" pair):
        // while the AABB overlaps a lava cell the player is in the same
        // swim state with the lava pace — a slower horizontal speed, a
        // much weaker swim-up, and a lighter, slower sink. The lava/water
        // ratios mirror Minecraft's motion-factor ratio (0.014/0.0231 ~
        // 0.6 for the horizontal pace) and its much weaker lava buoyancy.
        float lavaSwimSpeed = 1.9f;          // horizontal speed in lava (blocks/s)
        float lavaSwimUpSpeed = 1.5f;        // vertical velocity swimming up in lava
        float lavaGravity = 4.0f;            // downward acceleration in lava (blocks/s^2)
        float lavaTerminalVelocity = 2.0f;   // max sink speed in lava (blocks/s)
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
        // Hunger (Phase 23, pace Phase 31): maxHunger hunger points (10
        // drumstick icons, 2 each); one decay tick per
        // hungerDecaySeconds (Minecraft's passive pace: 1 hunger point =
        // 720 ticks = 36 s); at ZERO hunger the player takes 1 HP every
        // starveDamageIntervalSeconds (Minecraft normal: 1 HP per 400 ticks
        // = 20 s); regenHungerThreshold gates the regen above. Phase 28:
        // each decay tick first drains the hidden saturation buffer
        // (capped at maxHunger, the food's satiety — see core/food.hpp),
        // else drops one hunger point; sprinting (LeftShift + moving, out
        // of water) runs the decay at sprintHungerDecayFactor pace
        // (Minecraft's 36 s -> 18 s relative shape).
        float maxHunger = 20.0f;
        double hungerDecaySeconds = 36.0;
        double starveDamageIntervalSeconds = 20.0;
        float regenHungerThreshold = 6.0f;
        double sprintHungerDecayFactor = 2.0;
        // Lava burn (Phase 54, Minecraft's 4 HP/s): while the AABB overlaps
        // a lava cell, take lavaDamage HP every lavaDamageIntervalSeconds
        // of real dt (a survival timer like starvation — a real-dt
        // accumulator in Player::update, through the normal damage path so
        // the hurt flash/sound and death handling all fire). Leaving the
        // lava resets the pending DIRECT-lava tick (the LINGERING burn
        // after leaving is the Phase 57 fire effect — a different system).
        double lavaDamageIntervalSeconds = 1.0;
        float lavaDamage = 4.0f; // must be <= maxHealth (fromConfig check)
        // Fire contact (Phase 70, Minecraft's 1 HP / 0.2 s): while the
        // AABB overlaps a FIRE block cell, take fireDamage HP every
        // fireDamageIntervalSeconds of real dt (a survival timer like
        // the lava burn — the accumulator in Player::update, through the
        // normal damage path so the hurt flash/sound and death handling
        // all fire). Leaving the fire resets the pending tick. 0 disables
        // the damage (the config off-switch, like mobs.enabled). Must be
        // <= maxHealth (fromConfig check).
        double fireDamageIntervalSeconds = 0.2;
        float fireDamage = 1.0f; // 0 = off; must be <= maxHealth (fromConfig check)
        // Fire duration (Phase 57, Minecraft's 8 s on Normal): the seconds
        // the ignites_on_lava effect is (re-)applied while the player is
        // in the lava. While in the lava it re-applies every frame (the
        // duration resets = continuous burn); after leaving, the effect
        // counts down this duration and burns (1 HP / 0.2 s) until it
        // expires or water extinguishes it.
        double fireDurationSeconds = 8.0; // (0, 100] (fromConfig check)

        // Build the parameters from the config's `player.*` keys (Phase 32):
        // a missing key keeps the struct default, a present key must be a
        // JSON number (the Config getters would silently fall back to the
        // default on a type mismatch, so this reads the raw values).
        // Rejected: non-numeric values, non-positive values (only
        // fall_damage_offset, regen_hunger_threshold and fire_damage —
        // 0 = the damage off-switch — may be 0), eye_height >= height,
        // crouch_height >= height, regen_hunger_threshold > max_hunger,
        // lava_damage > max_health, fire_damage > max_health, and
        // fire_duration_seconds > 100. Unknown player.* keys are
        // tolerated (config forward-compat, like the rest of the config).
        static Result<Params> fromConfig(const Config& config);

        friend bool operator==(const Params&, const Params&) = default;
    };

    // `world` and `blocks` must outlive the player. `spawnPos` is the
    // respawn point (bottom-center AABB position). The params overload
    // tunes the movement/survival model (the game builds it from config
    // `player.*`); the plain form uses the struct defaults.
    Player(const World& world, const BlockRegistry& blocks, Vec3 spawnPos);
    Player(const World& world, const BlockRegistry& blocks, Vec3 spawnPos,
           Params params);

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

    // Save restore (Phase 36): teleport and re-pin the fall tracking to
    // the restored Y. A player restored mid-air has no leave-the-ground
    // transition to record the fall start, so an unpinned m_fallStartY
    // would let the saved fall land damage-free. Harmless when the save
    // was taken at rest (the landing tick sees a zero net drop).
    void restorePosition(Vec3 pos) {
        teleportTo(pos);
        m_fallStartY = pos.y;
    }

    const Params& params() const { return m_params; }

    // -- survival (Phase 16) --------------------------------------------------
    float health() const { return m_health; }
    // The EFFECTIVE max health (Phase 56): the configured max plus the
    // active absorption layer. damage()/heal()/setHealth() clamp against
    // this value, and the HUD draws the layer as extra (red) hearts.
    float maxHealth() const { return m_params.maxHealth + m_absorptionBonus; }
    bool isDead() const { return m_dead; }

    // The absorption layer (Phase 56): `bonus` is the sum of `absorb_hp`
    // over the player's active effects (the game syncs this at every
    // effect change and on save load).
    //   - An INCREASE (a new/refreshed layer) RAISES the health by the
    //     added amount (clamped at the new max) when `healOnIncrease` —
    //     the layer appears full, like Minecraft's.
    //   - A DECREASE (expiry/clear/load) drops the excess SILENTLY
    //     (health is clamped down to the new max — no damage flash, no
    //     sound, no damage clock: the layer is not real HP).
    //   - `healOnIncrease = false` is the SAVE-LOAD mode: the saved
    //     health already includes the layer, so loading must not heal.
    void setAbsorptionBonus(float bonus, bool healOnIncrease);
    bool isCrouching() const { return m_crouching; }
    double timeSinceDamage() const { return m_timeSinceDamage; }
    // Fluid immersion (Phase 54 tick state, exposed in Phase 57 for the
    // game's fire system: lava ignites, water extinguishes): the AABB
    // overlaps at least one water / lava cell.
    bool inWater() const { return m_inWater; }
    bool inLava() const { return m_inLava; }
    // Fire immersion (Phase 70): the AABB overlaps a fire block cell (the
    // contact-damage source).
    bool inFire() const { return m_inFire; }

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

    // Saturation (Phase 28): the hidden satiety buffer, [0, max] (a full
    // meal = a full buffer). Each hunger decay tick drains 1 of it before
    // touching the hunger bar (see core/food.hpp).
    float saturation() const { return m_saturation; }

    // Restore `amount` saturation (eating): clamped to [0, max].
    void restoreSaturation(float amount) {
        m_saturation = std::clamp(m_saturation + amount, 0.0f,
                                  m_params.maxHunger);
    }

    // Set the saturation (save load / tests): clamped to [0, max].
    void setSaturation(float s) {
        m_saturation = std::clamp(s, 0.0f, m_params.maxHunger);
    }

    // Apply `amount` damage. Returns true when health actually decreased
    // (the game drives the hurt sound/flash from this). Lethal damage sets
    // the dead state; a dead player is frozen (update() no-ops).
    bool damage(float amount);

    // Back at the spawn point with full health and cleared fall state. The
    // game calls this on the R key. The inventory is untouched
    // (Minecraft-Java keep-inventory respawn).
    void respawn();

    // Restore health (save load): clamped to [0, max] (the EFFECTIVE max,
    // base + absorption layer — Phase 56); 0 sets the dead state (a dead
    // save reloads dead).
    void setHealth(float h) {
        m_health = std::clamp(h, 0.0f, maxHealth());
        m_dead = m_health <= 0.0f;
    }

    // Heal `amount` HP (Phase 46's regeneration effect; the natural
    // hunger-based regen calls this too). Clamped at the effective max
    // (Phase 56: regeneration can fill INTO the absorption layer).
    void heal(float amount) {
        m_health = std::clamp(m_health + amount, 0.0f, maxHealth());
    }

    // Entity separation (Phases 16/19): the player is immovable — entities
    // are pushed out of the player (Minecraft's behavior), never the other
    // way around.
    bool pushableByEntities() const override { return false; }

private:
    // One physics sub-step (Entity::integrate): swim/air gravity + the
    // jump. `look`/`inWater` are tick state, consumed per sub-step.
    void integrate(float h) override;

    // The AABB overlaps at least one fluid cell (water / lava, Phase 54).
    bool isInWater() const;
    bool isInLava() const;
    // The AABB overlaps at least one fire block cell (Phase 70, the
    // contact damage). The fire id is resolved once in the constructor
    // (the registry is static after init); an unresolved id means "never
    // in fire" (a test fixture without the fire block — the fluid.cpp
    // fallback).
    bool isInFire() const;

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
    // This tick's lava state (Phase 54): the AABB overlaps a lava cell.
    // Lava wins over water when both are true (an AABB can straddle a
    // water cell and a lava cell — the burn pace applies).
    bool m_inLava = false;
    // This tick's fire state (Phase 70): the AABB overlaps a fire block
    // cell (the contact-damage source — blocks, not the fluid layer).
    bool m_inFire = false;
    // Crouch state this tick (set in update before the speed computation).
    bool m_crouching = false;
    // Survival: health, the damage clock (regen delay), the regen
    // accumulator (only runs after the delay), the fall tracking (net drop
    // since the last leave-the-ground), and the death state.
    float m_health = Params {}.maxHealth;
    // The absorption layer (Phase 56): temporary max-health points from
    // active effects (0 = none). The game owns the value (synced from the
    // StatusEffects); the player only clamps against it.
    float m_absorptionBonus = 0.0f;
    double m_timeSinceDamage = 0.0;
    double m_regenAccum = 0.0;
    bool m_wasOnGround = false;
    float m_fallStartY = 0.0f;
    bool m_dead = false;
    // Hunger (Phase 23): the level, the decay accumulator (runs always —
    // a dead player never reaches update), and the starvation accumulator
    // (only runs at zero). Phase 28: the hidden saturation buffer.
    float m_hunger = Params {}.maxHunger;
    float m_saturation = 0.0f;
    double m_hungerAccum = 0.0;
    double m_starveAccum = 0.0;
    // Lava burn (Phase 54): the real-dt accumulator for the pending burn
    // tick (only runs while the AABB overlaps a lava cell; leaving the
    // lava resets it — no lingering fire in v1).
    double m_lavaAccum = 0.0;
    // Fire contact (Phase 70): the real-dt accumulator for the pending
    // burn tick (only runs while the AABB overlaps a fire cell AND
    // fireDamage > 0; leaving the fire resets it).
    double m_fireAccum = 0.0;
    // The fire block id (Phase 70), resolved once in the constructor
    // (registry name "fire" — the fluid.cpp fallback pattern): invalid =
    // no fire block registered (a test fixture) = isInFire is false.
    BlockID m_fireBlockId = BlockID::invalid();

    static constexpr float kMaxPitch = 1.5533f; // ~89 deg
    // The crouch edge guard / step-up treat an AABB face as "flush" with a
    // block face within this tolerance (the collision clamp is exact, so
    // this only guards float round-trips).
    static constexpr float kFaceEps = 1e-4f;
};

} // namespace mc
