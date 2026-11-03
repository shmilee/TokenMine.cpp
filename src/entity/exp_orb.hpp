// ExpOrbEntity: a dropped experience orb (V2-11, the XP system).
//
// Physics = the ItemEntity's discipline (the "other entities" real-dt
// rest class of hard rule 8): the same gravity / terminal-velocity /
// ground-friction integration through the shared Entity sub-stepped
// collision (every axis <= 0.4 blocks per sub-step), so a resting orb's
// position is an exact block boundary regardless of the dt history and
// headless runs stay byte-reproducible. The pickup delay (0.5 s, REAL dt
// — the item's twin timer) keeps an orb spawned at the player's feet
// (the smelt / break grant points) from self-picking up on the next
// frame. The lifetime is the item's 120 s (kDespawnSeconds).
//
// VALUE: 1..9 (kMinValue..kMaxValue — the orb value cap: the modern
// Minecraft split point; the constructor CLAMPS into the range and the
// save format rejects out-of-range values by name). NO MERGE, NO SPLIT
// (one grant = one orb; the item's canMergeInto/absorb have no orb twin
// — the merge pass dynamic_casts ItemEntity and never sees an orb, the
// structural no-merge guarantee). The documented simplification of
// Minecraft's "orbs worth 9+ split".
//
// Magnet (V2-6): the pickup magnet's frame-locked pull applies to READY
// orbs exactly like ready items (the game's magnet pass adds the
// dynamic_cast branch; swirl + falloff + room-check untouched).
//
// Pickup: the game's pickup pass collects a READY orb within 1.0 of the
// player's bottom-center (the item's radius) — the player's pool gains
// the orb's value, the orb is removed, the "pickup" timbre plays
// (reused — the 8-event sound set, no new wav).
//
// Render: a small SOLID-CUBE ball (kSize 0.2, the six-face table of the
// shared cube convention — the 0.2 cube option over the billboard disc,
// the documented choice: it takes the basic shader's solid-color path
// like the arrows, zero new texture, zero new atlas item) in the fixed
// Minecraft green (kExpOrbColor — the (0,1,0)-class value, named). NO
// light emission (the Minecraft orb lights nothing — the orb's render
// samples its own cell's light through the PRE-6 per-draw override, the
// itemDropLight pattern: bright by day, moonlit by night, dark in a cave).
// A no-input run has zero orbs (the grant points need a furnace tick with
// a product / a mob death / a break — all input- or world-event-driven,
// the quiet gate config is mob-free) = zero draws = structural immunity
// (the HUD bar is the documented exception — it always renders).
#pragma once

#include "entity/entity.hpp"
#include "entity/item_mesh.hpp" // ItemMeshGeometry (the orb cube)

namespace mc {

class ExpOrbEntity : public Entity {
public:
    // The collision + visual cube size (blocks) — the small-ball look
    // (the item's kSize 0.25 is the precedent; the orb is a touch
    // tighter).
    static constexpr float kSize = 0.2f;
    // The orb value range (the modern Minecraft split point — a value
    // above 9 would split into several orbs; the project simplification
    // is no split, so the data discipline caps the value here).
    static constexpr int kMinValue = 1;
    static constexpr int kMaxValue = 9;
    // The item's lifetime (Minecraft's 120 s despawn — the "same lifetime"
    // of the V2-11 design).
    static constexpr double kDespawnSeconds = 120.0;
    // The item's pickup delay (0.5 s, REAL dt — the two-clock discipline:
    // the physics rests on real dt, the delay counts real dt too, so a
    // save/load round trip continues both timers exactly).
    static constexpr double kPickupDelaySeconds = 0.5;

    // The orb's Minecraft green (the (0,1,0)-class value the design
    // names — the render's solid color; the HUD bar / level number keep
    // their own yellow-green on the game_ui side).
    static constexpr float kColorR = 0.0f;
    static constexpr float kColorG = 1.0f;
    static constexpr float kColorB = 0.0f;

    // `value` is the experience points the orb carries (clamped into
    // [kMinValue, kMaxValue] — the grant points pass data-validated
    // values, the clamp is the belt). The orb spawns at rest (zero
    // velocity — the item convention: gravity + friction settle it at
    // its spawn, headless-deterministic).
    ExpOrbEntity(const World& world, const BlockRegistry& blocks, int value,
                 Vec3 pos);

    void update(double dt) override;
    bool shouldDespawn() const override {
        return m_age >= kDespawnSeconds || belowKillPlane();
    }

    int value() const { return m_value; }
    double age() const { return m_age; }
    bool pickupReady() const { return m_pickupDelay <= 0.0; }
    // Remaining pickup delay (seconds; 0 = pickable now) — the save
    // restores the timer with setPickupDelay (the item's twin).
    double pickupDelayRemaining() const { return m_pickupDelay; }
    void setAge(double a) { m_age = a; }
    void setPickupDelay(double remaining) { m_pickupDelay = remaining; }

    // No canMergeInto / absorb (the structural no-merge guarantee — see
    // the file head).

private:
    void integrate(float h) override;

    int m_value;
    double m_age = 0.0;
    double m_pickupDelay = kPickupDelaySeconds;

    // The item's rest physics (blocks/s, blocks/s^2) — the shared
    // fall model, named here like ItemEntity's.
    static constexpr float kGravity = 32.0f;
    static constexpr float kTerminalVelocity = 38.0f;
    // The item's exponential ground friction + stop epsilon (a resting
    // orb is exactly still — no sub-pixel creep).
    static constexpr float kFriction = 2.5f;
    static constexpr float kStopSpeed = 0.005f;
};

// The orb's solid-cube geometry (kSize 0.2, centered on the ORIGIN — the
// game translates by the orb's bottom-center + kSize/2 at draw time, the
// item cube's convention): the shared six-face table (kCubeFaces — the
// counter-clockwise-from-outside winding, the FaceWindingIs... guard's
// geometry) with all faces, full sky light baked (the per-draw override
// replaces it at render — the vertices are upload-once static).
ItemMeshGeometry makeExpOrbGeometry();

} // namespace mc
