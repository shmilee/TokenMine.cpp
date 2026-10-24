// ItemEntity: a dropped item (Phases 10/11).
//
// When the player breaks a block whose `properties.drops` names an item,
// the game spawns one here at the block's center with zero velocity. The
// zero-velocity spawn is deliberate: the rest position is then an exact
// integer boundary, which keeps headless runs byte-reproducible (Minecraft
// scatters drops with a random kick; that is not representable in a
// deterministic screenshot workflow). Q-drops spawn the same way, at the
// player's feet.
//
// The entity holds the item's NAME (the game resolves it through the
// ItemRegistry) plus the item's data-defined max stack: merging respects
// it (a tool with max_stack 1 never merges into a stack of 2), and kMaxStack
// is the hard entity-level cap on top.
//
// Physics: the same fall as the player (gravity 32 / terminal 38) plus an
// exponential horizontal friction that zero's out below kStopSpeed so a
// dropped item truly comes to rest. ENT-4: while the AABB overlaps a
// water OR lava cell (or the item rests at the fluid's surface) the
// physics switches to the shared fluid model (Entity::refreshFluidState +
// integrateFluid — the input-free twin of the player's FLUID-2 swim):
// passive rise (buoyancy 12, capped 1.5), the geometric W' float (the
// terminal is a STATIONARY item — the friction zero's the residual, no
// surface drift, the documented MC simplification), and the sink /
// pool-floor stand for entry momentum. LAVA floats symmetrically (the
// documented extension: the player's lava has the weaker pace, items take
// the water model in both kinds).
//
// Merging: two same-item entities within kMergeDistance of each other's
// bottom-center position merge into the earlier one. The game performs the
// merge pass; the rules live here.
//
// Lifetime: kDespawnSeconds (Minecraft's 120 s) or below the kill plane.
// Pickup (Phase 11): the player collects an item when it is within
// kPickupDistance of the player's bottom-center and past its pickup delay
// (kPickupDelaySeconds — Minecraft's 10-tick delay, which is what keeps a
// Q-drop at the player's feet from being re-picked up on the next frame).
#pragma once

#include <string>

#include "entity/entity.hpp"

namespace mc {

class ItemEntity : public Entity {
public:
    static constexpr float kSize = 0.25f; // collision + visual cube size
    static constexpr int kMaxStack = 64;  // hard cap on an entity's count
    static constexpr double kDespawnSeconds = 120.0; // Minecraft's item lifetime
    static constexpr float kMergeDistance = 0.75f; // bottom-center merge radius
    // Not pickable right after spawning (see the class comment).
    static constexpr double kPickupDelaySeconds = 0.5;

    // `maxStack` is the item's data-defined stack cap (the game passes the
    // ItemRegistry value); the effective cap is min(maxStack, kMaxStack).
    // `count` is the starting amount (block drops pass
    // BlockProperties::dropsCount; Q-drops pass 1).
    ItemEntity(const World& world, const BlockRegistry& blocks,
               const std::string& item, int maxStack, int count, Vec3 pos);

    void update(double dt) override;
    bool shouldDespawn() const override {
        return m_age >= kDespawnSeconds || belowKillPlane();
    }

    const std::string& itemName() const { return m_item; }
    int count() const { return m_count; }
    double age() const { return m_age; }
    bool pickupReady() const { return m_pickupDelay <= 0.0; }
    // Remaining pickup delay (seconds; 0 = pickable now). Save/load
    // (Phase 13) restores the timers with setAge/setPickupDelay.
    double pickupDelayRemaining() const { return m_pickupDelay; }
    // ENT-4: this tick's fluid state (the strict AABB overlap of at least
    // one water OR lava cell — the player's inWater()/inLava() union).
    // Exposed for tests.
    bool inFluid() const { return m_inFluid; }
    void setAge(double a) { m_age = a; }
    void setPickupDelay(double remaining) { m_pickupDelay = remaining; }

    // Can `this` be absorbed into `other`? Same item, stack room in
    // `other`, and within the merge radius of each other.
    bool canMergeInto(const ItemEntity& other) const;
    // Add `other`'s count into this one (capped at the item's max stack).
    // The absorbed entity is left untouched — the game removes it.
    void absorb(const ItemEntity& other);

    // Entity separation (Phase 16 player-vs-item push, generalized in
    // Phase 19) lives in the Entity base: separateFrom (one-sided) and
    // separateEntityPair/separateEntityList (the per-frame pass).

private:
    void integrate(float h) override;

    std::string m_item;
    int m_maxStack;
    int m_count; // set in the constructor (>= 1)
    double m_age = 0.0;
    double m_pickupDelay = kPickupDelaySeconds;

    // Same fall physics as the player (blocks/s, blocks/s^2).
    static constexpr float kGravity = 32.0f;
    static constexpr float kTerminalVelocity = 38.0f;
    // Exponential decay of the horizontal velocity (1/s); below kStopSpeed
    // the residual is zeroed so the item truly stops (no sub-pixel creep).
    static constexpr float kFriction = 2.5f;
    static constexpr float kStopSpeed = 0.005f;
};

} // namespace mc
