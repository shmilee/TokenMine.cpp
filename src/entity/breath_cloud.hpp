// BreathCloudEntity: the dragon's lingering breath cloud (V3-10d, the
// ender dragon boss).
//
// A TRANSIENT effect zone: the perched dragon's breath (the player's
// feet, the dragon_breath effect's carrier) and the dragon fireball's
// impact point (the MC cloud-on-impact read). It has NO physics (the
// cloud hovers and SLOWS-EXPANDS — a pure function of the frame age,
// r(t) = radius0 + kExpansion * age, the named constants below) and NO
// stream (zero rolls — the damage lives in the game's cloud pass, the
// player-overlap's 30-frame effect grant, the potion's twin path).
//
// Save exclusion: the cloud is a pure transient (the arrow / pearl's
// "arrows are not saved" discipline, projectile.hpp:42-43): the game's
// save write-side whitelists (the dynamic_cast family) never match it
// and the read side never restores one — a loaded world has no clouds
// (the dragon's perched state re-arms them on the next breath).
//
// Render: a flat violet disc (the blend pass's translucent quad stack —
// the spawnSteam's blend-pass twin, the portal steam's 0.12-alpha
// precedent class; the lazy mesh is the arrow_mesh's single-quad
// pattern, tinted the dragon breath's purple).
#pragma once

#include "entity/entity.hpp"

namespace mc {

class BreathCloudEntity : public Entity {
public:
    // The cloud's initial radius (blocks) — the game passes the data
    // breath_radius / fireball cloud's fixed radius here.
    // The expansion rate (blocks/second of frame age — the MC
    // "slowly expands to dissipate" read's project scale).
    static constexpr float kExpansionRate = 0.15f;
    // The disc's thickness (the render stack's depth — a visual
    // constant, the collision is the game pass's horizontal radius).
    static constexpr float kThickness = 0.5f;

    BreathCloudEntity(const World& world, const BlockRegistry& blocks,
                      Vec3 pos, double duration, float radius);
    // The cloud's current radius (the pure frame-age function, hard
    // rule 8 — the age is the fixed 1/60 step's accumulator).
    float radius() const {
        return m_radius0 +
            kExpansionRate * static_cast<float>(m_age);
    }
    double age() const { return m_age; }
    double duration() const { return m_duration; }
    // The despawn: the duration's expiry (the MC dissipate) or the
    // kill plane (the void's belt, the entity's convention).
    bool shouldDespawn() const override {
        return m_age >= m_duration || belowKillPlane();
    }

    void update(double dt) override {
        m_age += dt; // the fixed 1/60 step's accumulator (zero stream)
    }

    // The no-physics entity (the cloud hovers — the gravity / the
    // sub-step collision are the ItemEntity's discipline, never the
    // cloud's): the base pure virtual's no-op (the Particle's
    // render-only class, the collision-free entity).
    void integrate(float h) override { (void)h; }

private:
    double m_duration;
    float m_radius0;
    double m_age = 0.0;
};

} // namespace mc
