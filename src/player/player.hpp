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
// Swimming (Phase 9, while the AABB overlaps a fluid cell): a low sink cap,
// reduced horizontal speed, and Space swims upward instead of jumping (also
// from the ground — a shallow pool floor does not jump, it surfaces).
// FLUID-2 (the swim overhaul): with no input the player RISES passively
// (buoyancy, capped at waterRiseSpeed) and FLOATS on the water surface —
// the geometric plane one above the topmost full (level-8) water cell of
// the feet's center column, clamped exactly (fps-independent terminal).
// Crouch DIVES (crouch-sink at waterSinkSpeed). Space at the surface is a
// normal jump (the player has breached, Minecraft-style). A pure flow film
// (no level-8 support) gives no surface and no rise — the Phase-9 sink.
//
// The World + BlockRegistry are the sole source of solid/air (via Entity):
// the player reads properties.solid and never switches on block identity.
#pragma once

#include <algorithm>
#include <array>

#include "core/config.hpp"
#include "core/input.hpp"
#include "core/math/vec3.hpp"
#include "entity/entity.hpp"
#include "item/item.hpp"
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
        // Swimming (while the AABB overlaps a fluid cell): a lower
        // horizontal speed and Space swims upward instead of jumping
        // (Minecraft-style values). FLUID-2: the vertical model is
        // buoyancy + surface float + crouch-dive (see the header comment);
        // waterBuoyancy is the rise/sink acceleration (the Phase-9
        // water_gravity value, the role flipped), waterRiseSpeed the
        // passive-rise cap, waterSinkSpeed the crouch-dive cap. The
        // retired player.water_gravity key is no longer read (tolerated in
        // old configs, the Phase-74 fire_damage precedent).
        float swimSpeed = 3.1f;         // horizontal speed in water (blocks/s)
        float swimUpSpeed = 4.5f;       // vertical velocity while swimming up
        float waterBuoyancy = 12.0f;    // rise/sink acceleration in water (blocks/s^2)
        float waterRiseSpeed = 1.5f;    // passive-rise speed cap (blocks/s)
        float waterSinkSpeed = 1.5f;    // crouch-dive speed cap (blocks/s)
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
        // Fire duration (Phase 57, Minecraft's 8 s on Normal): the seconds
        // the ignites_on_lava effect is (re-)applied while the player is
        // in the lava OR overlapping a fire block (Phase 74: the fire
        // block ignites the player the way the lava does — the P70
        // direct contact damage was replaced by the ignition). While in
        // the fire source it re-applies every frame (the duration resets
        // = continuous burn); after leaving, the effect counts down this
        // duration and burns (1 HP / 0.2 s) until it expires or water
        // extinguishes it.
        double fireDurationSeconds = 8.0; // (0, 100] (fromConfig check)

        // Build the parameters from the config's `player.*` keys (Phase 32):
        // a missing key keeps the struct default, a present key must be a
        // JSON number (the Config getters would silently fall back to the
        // default on a type mismatch, so this reads the raw values).
        // Rejected: non-numeric values, non-positive values (only
        // fall_damage_offset and regen_hunger_threshold may be 0),
        // eye_height >= height, crouch_height >= height,
        // regen_hunger_threshold > max_hunger, lava_damage > max_health,
        // and fire_duration_seconds > 100. Unknown player.* keys are
        // tolerated (config forward-compat, like the rest of the config —
        // a pre-Phase-74 config with the retired player.fire_damage*
        // keys, or a pre-FLUID-2 config with the retired
        // player.water_gravity key, still boots).
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
    // The BASE max health (Phase 72: the absorption pool is a SEPARATE
    // value — see absorption()). damage()/heal()/setHealth() clamp the
    // health against this value only.
    float maxHealth() const { return m_params.maxHealth; }
    bool isDead() const { return m_dead; }

    // The absorption pool (Phase 72, the two-component model): HP points
    // ABOVE the health that damage drains FIRST before any health is
    // touched (Minecraft). The game sets this to the sum of `absorb_hp`
    // over the player's active effects at every effect change (a
    // refreshed layer restores the pool to the full sum — Minecraft's
    // attribute-recalculation semantics); healing NEVER fills it (heal()
    // clamps at the base max). Clamped to [0, kAbsorptionMax].
    static constexpr float kAbsorptionMax = 64.0f;
    float absorption() const { return m_absorption; }
    void setAbsorption(float a) {
        m_absorption = std::clamp(a, 0.0f, kAbsorptionMax);
    }
    // Phase 84 (the beacon's speed / resistance effects): the move-speed
    // multiplier + the incoming-damage reduction the game pushes each frame
    // from the active effects (identity 1.0 / 0.0 when none are active).
    void setSpeedMultiplier(float m) { m_speedMultiplier = m; }
    void setDamageReduction(float r) {
        m_damageReduction = std::clamp(r, 0.0f, 1.0f);
    }
    bool isCrouching() const { return m_crouching; }
    double timeSinceDamage() const { return m_timeSinceDamage; }
    // Fluid immersion (Phase 54 tick state, exposed in Phase 57 for the
    // game's fire system: lava ignites, water extinguishes): the AABB
    // overlaps at least one water / lava cell.
    bool inWater() const { return m_inWater; }
    bool inLava() const { return m_inLava; }
    // The AABB overlaps at least one fire block cell (Phase 70; the
    // ignition source since Phase 74 — Game::updateEffects re-applies
    // the fire effect while this is true, the lava pattern). The fire id
    // is resolved once in the constructor (the registry is static after
    // init); an unresolved id means "never in fire" (a test fixture
    // without the fire block — the fluid.cpp fallback).
    bool isInFire() const;
    // Sitting (Phase 71): crouching on the ground while the AABB overlaps
    // a campfire block cell — movement and jumping are locked (the crouch
    // height is the sitting pose — documented v1 deviation: no separate
    // sit pose). Recomputed every tick from the crouch key + world, so it
    // is not saved.
    bool isSitting() const { return m_sitting; }

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

    // -- armor (SURV-1) ------------------------------------------------------
    // The four equipment slots (the kArmorSlotNames order: 0 = head,
    // 1 = chest, 2 = legs, 3 = feet). A slot holds one piece (count 1 —
    // the armor items' max_stack) or is empty. The game owns every
    // change (the right-click channel + the save load); the player
    // stores the pieces and derives the reduction. Death and respawn
    // keep the armor (the inventory is kept — the project's death
    // semantics, the keep-inventory respawn).
    const ItemStack& armorSlot(int slot) const;
    ItemStack& armorSlot(int slot);
    // Equip / swap / unequip one slot: the piece (count 1, or the empty
    // stack) + its data armor_points (0 when empty). The game passes
    // both, so the player holds no ItemRegistry (the setAbsorption
    // "the game owns the value" pattern).
    void setArmorSlot(int slot, const ItemStack& piece, int points);
    // The total equipped points, capped at kArmorMaxPoints (the damage
    // formula's input; 0 = no armor = the pre-SURV-1 bit-identical
    // path).
    int armorPoints() const;

    // Apply `amount` damage. Returns true when health actually decreased
    // (the game drives the hurt sound/flash from this). Lethal damage sets
    // the dead state; a dead player is frozen (update() no-ops).
    //
    // SURV-1 (armor, the Minecraft integer-floor rule): the equipped
    // points reduce the damage BEFORE the resistance effect and the
    // absorption pool (the pipeline order armor -> resistance -> pool ->
    // health; the unit tests pin it).
    //
    // SURV-2 (drowning, the Minecraft exemption — the wiki damage table
    // reads Drowning as "Armor: No effect / Resistance: Applies"):
    // bypassArmor=true skips the armor floor step ONLY — the resistance
    // reduction and the absorption pool drain as for any other damage.
    // The default false keeps every pre-SURV-2 call site bit-identical
    // (the existing damage tests are untouched).
    bool damage(float amount, bool bypassArmor = false);

    // Back at the spawn point with full health and cleared fall state. The
    // game calls this on the R key. The inventory is untouched
    // (Minecraft-Java keep-inventory respawn).
    void respawn();

    // Restore health (save load): clamped to [0, base max] (Phase 72: the
    // absorption pool is SEPARATE — it is restored via setAbsorption, the
    // two pools of the format-12 save); 0 sets the dead state (a dead save
    // reloads dead).
    void setHealth(float h) {
        m_health = std::clamp(h, 0.0f, maxHealth());
        m_dead = m_health <= 0.0f;
    }

    // Heal `amount` HP (Phase 46's regeneration effect; the natural
    // hunger-based regen calls this too). Clamped at the BASE max —
    // Phase 72: healing NEVER fills the absorption pool (Minecraft:
    // the pool is set only by the active `absorb_hp` effects).
    void heal(float amount) {
        m_health = std::clamp(m_health + amount, 0.0f, maxHealth());
    }

    // Entity separation (Phases 16/19): the player is immovable — entities
    // are pushed out of the player (Minecraft's behavior), never the other
    // way around.
    bool pushableByEntities() const override { return false; }

private:
    // One physics sub-step (Entity::integrate): the lava / water-surface /
    // air force model (FLUID-2: the water branch is the swim state
    // machine — Space > crouch-dive > float snap > passive rise > floor
    // stand) + the jump. `look`/`inWater`/`atSurface` are tick state,
    // consumed per sub-step.
    void integrate(float h) override;

    // The AABB overlaps at least one fluid cell (water / lava, Phase 54).
    bool isInWater() const;
    bool isInLava() const;
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

    // A world cell is solid (air and unregistered ids are not).
    bool isSolidCell(int x, int y, int z) const;

    // FLUID-2 (the rise gate, the FRESH grounded check): a solid that the
    // Y resolve would clamp against right now (a solid-region top at the
    // feet, within kFaceEps) — ENT-4: lifted to the Entity base (see
    // entity.hpp) so the mob + item share it; the player inherits it,
    // unchanged.

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
    // FLUID-2 (the surface float): the water surface plane of the feet's
    // center column — one above the topmost full (level-8) water cell at or
    // below the feet (scanned down kSurfaceScanDepth cells from the feet),
    // or -1 when none (no float plane). Tick state like m_inWater: computed
    // once per update, consumed per sub-step. Only WATER counts (a lava
    // column never floats — the Phase-54 semantics), and only level-8 cells
    // (a pure flow film supports no surface — the Phase-47 bucket source
    // settles to level 8 per FLUID-1, a flowing film does not).
    float m_surfaceY = -1.0f;
    // FLUID-2: this tick's surface state — the feet are within the float
    // band (kMaxSubstepMove, the max per-sub-step vertical displacement, so
    // a sub-step that crosses the plane is always seen) of m_surfaceY and
    // the player is NOT resting on a solid (a player standing on a
    // pool-edge block at the surface keeps the ground pace — the
    // isRestingOnSolid exclusion).
    bool m_atSurface = false;
    // Crouch state this tick (set in update before the speed computation).
    bool m_crouching = false;
    // Survival: health, the damage clock (regen delay), the regen
    // accumulator (only runs after the delay), the fall tracking (net drop
    // since the last leave-the-ground), and the death state.
    float m_health = Params {}.maxHealth;
    // The absorption pool (Phase 72): HP points ABOVE the health that
    // damage drains first (0 = none). The game owns the value (set from
    // the StatusEffects at every effect change, or from the format-12
    // save); the player only clamps it to [0, kAbsorptionMax].
    float m_absorption = 0.0f;
    // Phase 84: the effect-derived move-speed multiplier + damage reduction
    // (set by the game each frame from the active effects).
    float m_speedMultiplier = 1.0f;
    float m_damageReduction = 0.0f;
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
    // The fire block id (Phase 70; the ignition source since Phase 74),
    // resolved once in the constructor (registry name "fire" — the
    // fluid.cpp fallback pattern): invalid = no fire block registered
    // (a test fixture) = isInFire is false.
    BlockID m_fireBlockId = BlockID::invalid();
    // Phase 71: the campfire block id, resolved once in the constructor
    // (the registry is static after init). An unknown name leaves the id
    // invalid = the player can never sit (a test fixture without the
    // campfire block — the m_fireBlockId pattern). Phase 75: the
    // extinguished twin joins the seat (Minecraft: the unlit campfire is
    // still a seat — the logs are unchanged).
    BlockID m_campfireBlockId = BlockID::invalid();
    BlockID m_campfireExtId = BlockID::invalid();
    // Phase 71: the sitting state this tick (see isSitting()).
    bool m_sitting = false;
    // SURV-1 (armor): the equipped pieces (count 1 or empty) + their
    // per-slot data points (kept in sync by setArmorSlot — the player
    // holds no ItemRegistry, the game passes the points). Index = the
    // kArmorSlotNames slot order (head/chest/legs/feet).
    std::array<ItemStack, kArmorSlotCount> m_armor;
    std::array<int, kArmorSlotCount> m_armorPoints {};

    static constexpr float kMaxPitch = 1.5533f; // ~89 deg
    // FLUID-2: how many cells the surface scan (m_surfaceY) descends from
    // the feet. 32 covers the shipped water columns (ocean depth is
    // sea_level 34 - base_height 40 + amplitude 24 >= 16 => <= 18 cells;
    // the lava lakes are <= 4 deep) — a column deeper than this gives no
    // float plane (documented: a diver far below a 32+-deep column keeps
    // rising until within 32 of the surface).
    static constexpr int kSurfaceScanDepth = 32;
};

} // namespace mc
