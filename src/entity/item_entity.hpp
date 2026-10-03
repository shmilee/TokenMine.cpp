// ItemEntity: a dropped block (Phase 10).
//
// When the player breaks a block whose `properties.drops` names a block,
// the game spawns one here at the block's center with zero velocity. The
// zero-velocity spawn is deliberate: the rest position is then an exact
// integer boundary, which keeps headless runs byte-reproducible (Minecraft
// scatters drops with a random kick; that is not representable in a
// deterministic screenshot workflow).
//
// Physics: the same fall as the player (gravity 32 / terminal 38) plus an
// exponential horizontal friction that zero's out below kStopSpeed so a
// dropped item truly comes to rest.
//
// Merging: two same-block items within kMergeDistance of each other's
// bottom-center position merge into the earlier one (stacks cap at
// kMaxStack, like Minecraft's 64). The game performs the merge pass; the
// rules live here.
//
// Lifetime: kDespawnSeconds (Minecraft's 120 s) or below the kill plane.
// Pickup (into an inventory) is Phase 11; until then items sit where they
// land and age out.
#pragma once

#include "entity/entity.hpp"

namespace mc {

class ItemEntity : public Entity {
public:
    static constexpr float kSize = 0.25f; // collision + visual cube size
    static constexpr int kMaxStack = 64;  // max count in one entity
    static constexpr double kDespawnSeconds = 120.0; // Minecraft's item lifetime
    static constexpr float kMergeDistance = 0.75f; // bottom-center merge radius

    ItemEntity(const World& world, const BlockRegistry& blocks, BlockID block,
               Vec3 pos);

    void update(double dt) override;
    bool shouldDespawn() const override {
        return m_age >= kDespawnSeconds || belowKillPlane();
    }

    BlockID block() const { return m_block; }
    int count() const { return m_count; }
    double age() const { return m_age; }

    // Can `this` be absorbed into `other`? Same block, stack room in
    // `other`, and within the merge radius of each other.
    bool canMergeInto(const ItemEntity& other) const;
    // Add `other`'s count into this one (capped at kMaxStack). The absorbed
    // entity is left untouched — the game removes it.
    void absorb(const ItemEntity& other);

private:
    void integrate(float h) override;

    BlockID m_block;
    int m_count = 1;
    double m_age = 0.0;

    // Same fall physics as the player (blocks/s, blocks/s^2).
    static constexpr float kGravity = 32.0f;
    static constexpr float kTerminalVelocity = 38.0f;
    // Exponential decay of the horizontal velocity (1/s); below kStopSpeed
    // the residual is zeroed so the item truly stops (no sub-pixel creep).
    static constexpr float kFriction = 2.5f;
    static constexpr float kStopSpeed = 0.005f;
};

} // namespace mc
