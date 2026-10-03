// Player: the first-person actor. Owns position, orientation and velocity,
// and moves against the voxel world with AABB collision + sliding.
//
// Position convention (Minecraft-style): `position` is the CENTER of the
// BOTTOM face of the player's AABB, in world block units. The AABB is
//   min = {x - width/2, y, z - width/2}
//   max = {x + width/2, y + height, z + width/2}
// The camera eye sits at position + (0, eyeHeight, 0).
//
// Movement model (per update):
//   1. mouse look (yaw/pitch, pitch clamped to +/- ~89 deg)
//   2. desired horizontal velocity from WASD relative to yaw (walk/sprint)
//   3. gravity on the vertical velocity; Space jumps while onGround
//   4. integration per axis (X, Z, then Y) against solid blocks. Each axis
//      is resolved independently, which is what produces "sliding": the
//      blocked axis zeroes its velocity while the other keeps moving.
//   5. onGround = the Y resolution clamped downward this tick
//
// Collision details:
//   - A block cell [b, b+1) overlaps the AABB iff `min < b+1 && max > b`
//     (strict). Block boundaries are integers, exactly representable in
//     float, so resting exactly on a face is not an overlap and needs no
//     epsilon: gravity re-penetrates the face each tick and the Y resolve
//     clamps back to the exact integer boundary (no jitter).
//   - When an axis is blocked, the player is clamped flush against the
//     limiting block face and that axis's velocity is zeroed.
//   - Tunneling: the tick is sub-stepped so no axis moves more than
//     kMaxSubstepMove blocks per step (less than half a block).
//
// The World + BlockRegistry are the sole source of solid/air: the player
// reads properties.solid and never switches on block identity. Water is
// non-solid (the player can be inside it); when the AABB overlaps a fluid
// cell the player SWIMS: reduced gravity/terminal velocity, reduced
// horizontal speed, and Space swims upward (Minecraft-style, Phase 9).
#pragma once

#include "core/input.hpp"
#include "core/math/vec3.hpp"
#include "world/world.hpp"

namespace mc {

class BlockRegistry;

class Player {
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

    // Advance the simulation by dt seconds using the polled input.
    // Mouse look is applied only when `lookEnabled` (the game passes the
    // window's pointer-captured state, so an unlocked cursor never rotates
    // the view).
    void update(double dt, const Input& input, bool lookEnabled);

    // Move the player to `pos` (bottom-center) and zero its velocity.
    void teleportTo(Vec3 pos);

    // Is the AABB (bottom-center + given dimensions) free of solid blocks?
    // Used for spawn point validation.
    static bool hasRoomAt(const World& world, const BlockRegistry& blocks,
                          Vec3 bottomCenter, float width, float height);

    // -- state ---------------------------------------------------------------
    // Orientation conventions (shared with Camera): yaw 0 = facing -Z,
    // positive turns left; pitch positive looks up (clamped in update()).
    float yaw = 0.0f;
    float pitch = 0.0f;

    const Vec3& position() const { return m_position; }
    Vec3 eyePosition() const {
        return m_position + Vec3{0.0f, m_params.eyeHeight, 0.0f};
    }
    bool onGround() const { return m_onGround; }
    const Vec3& velocity() const { return m_velocity; }

    Vec3 aabbMin() const;
    Vec3 aabbMax() const;

    const Params& params() const { return m_params; }
    const World& world() const { return m_world; }

private:
    // One physics sub-step (look + desired velocity are applied per tick,
    // not per sub-step, in update()). `inWater` is the tick's water state
    // (swim physics instead of air physics while true).
    void step(double h, const Input& input, bool inWater);

    // Move along one axis by `delta` and resolve against solid blocks.
    // For the Y axis, a downward clamp sets m_onGround.
    void moveAxis(int axis, float delta);

    // Solid = registered block with properties.solid (air is never solid).
    bool isSolid(BlockID id) const;

    // The AABB overlaps at least one fluid cell (water).
    bool isInWater() const;

    const World& m_world;
    const BlockRegistry& m_blocks;

    Params m_params;
    Vec3 m_spawnPos {0.0f, 0.0f, 0.0f};

    Vec3 m_position {0.0f, 0.0f, 0.0f}; // bottom-center of the AABB
    Vec3 m_velocity {0.0f, 0.0f, 0.0f}; // blocks / second
    bool m_onGround = false;

    static constexpr float kMaxPitch = 1.5533f; // ~89 deg
    static constexpr float kMaxSubstepMove = 0.4f; // blocks per physics sub-step
    static constexpr int kMaxSubsteps = 64;
    static constexpr float kKillPlaneY = -32.0f; // fell out of the world -> respawn
};

} // namespace mc
