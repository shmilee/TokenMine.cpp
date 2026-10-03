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

    Vec3 eyePosition() const {
        return position() + Vec3{0.0f, m_params.eyeHeight, 0.0f};
    }

    const Params& params() const { return m_params; }

private:
    // One physics sub-step (Entity::integrate): swim/air gravity + the
    // jump. `look`/`inWater` are tick state, consumed per sub-step.
    void integrate(float h) override;

    // The AABB overlaps at least one fluid cell (water).
    bool isInWater() const;

    Params m_params;
    Vec3 m_spawnPos {0.0f, 0.0f, 0.0f};
    // Tick state for update()/integrate(): the polled input, the look
    // state, and this tick's water state (computed once per update, not
    // per sub-step).
    const Input* m_input = nullptr;
    bool m_lookEnabled = false;
    bool m_inWater = false;

    static constexpr float kMaxPitch = 1.5533f; // ~89 deg
};

} // namespace mc
