// Phase 15: block-break particles.
//
// Lightweight physics-only particles (no collision, no ground bounce):
// small cubes that fly out of a broken block under gravity and die after
// a short life. Rendered as a per-frame dynamic mesh (one batch per
// opaque/transparent pass), scaled from the broken block's own faces.
//
// Spawn bursts are DETERMINISTIC: given the same rngSeed the same
// particles come out, so an identical input sequence produces an
// identical burst (the game seeds from the world seed + a per-break
// counter). Motion itself integrates the real dt like the entities, so
// particle positions are only reproducible at frame boundaries — the E2E
// screenshots are taken after the burst has died (life <= kMaxLife).
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "block/block.hpp"
#include "core/math/vec3.hpp"

namespace mc {

// The shared default for break debris (a named constant — spawnBreak and
// anything without a reason to differ).
inline constexpr float kDefaultGravity = 16.0f;

struct Particle {
    Vec3 position {0.0f, 0.0f, 0.0f};
    Vec3 velocity {0.0f, 0.0f, 0.0f};
    float age = 0.0f;
    float life = 0.0f;
    BlockID block; // texture source (the broken block's faces)
    // Baked at spawn time by the game (the render pass must not read the
    // world light arrays — the worker lighting jobs may be running).
    float skyLight = 1.0f;   // 0..1
    float blockLight = 0.0f; // 0..1
    // Per-particle gravity (Phase 66): break debris falls under
    // kDefaultGravity; fire embers carry a NEGATIVE gravity (they rise,
    // like Minecraft's fire particles).
    float gravity = kDefaultGravity;
};

class ParticleSystem {
public:
    static constexpr std::size_t kMaxParticles = 256;
    static constexpr int kBreakBurstCount = 16;
    static constexpr float kGravity = kDefaultGravity; // blocks/s^2 (lighter than the player)
    static constexpr float kMinLife = 0.45f; // seconds
    static constexpr float kMaxLife = 0.75f;
    // Fire embers (Phase 66): a small per-tick emission per live fire
    // cell — few particles, short life, and a NEGATIVE gravity so they
    // rise like Minecraft's fire particles instead of falling.
    static constexpr int kFireEmissionCount = 2;
    static constexpr float kFireGravity = -2.0f; // blocks/s^2 (rising)
    static constexpr float kFireMinLife = 0.3f;  // seconds
    static constexpr float kFireMaxLife = 0.5f;  // seconds

    // Spawns a deterministic burst around the center of cell (cx, cy,
    // cz). rngSeed must differ per burst (the game passes the world seed
    // mixed with an increasing per-break counter).
    void spawnBreak(int cx, int cy, int cz, BlockID id, float skyLight,
                    float blockLight, std::uint64_t rngSeed);

    // Spawns a deterministic fire-ember emission around the center of
    // cell (cx, cy, cz) (kFireEmissionCount particles, rising). Same
    // seed contract as spawnBreak: the game mixes the world seed with a
    // position hash and the frame, so an identical run emits identical
    // particles.
    void spawnFire(int cx, int cy, int cz, BlockID id, float skyLight,
                   float blockLight, std::uint64_t rngSeed);

    // Integrates gravity + motion and culls expired particles.
    void update(double dt);

    const std::vector<Particle>& particles() const { return m_particles; }
    std::size_t size() const { return m_particles.size(); }
    bool empty() const { return m_particles.empty(); }
    void clear() { m_particles.clear(); }

private:
    std::vector<Particle> m_particles;
};

} // namespace mc
