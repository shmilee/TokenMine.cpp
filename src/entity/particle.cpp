#include "entity/particle.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "world/noise.hpp"

namespace mc {

namespace {

// 53-bit uniform in [0, 1) from an advancing splitmix64 stream:
// state += gamma, then mix (the repo's splitmix64 is a pure finalizer —
// without the step every call in a burst returned the same value and all
// 16 particles overlapped into one visible point; fixed in Phase 29).
double uniform01(std::uint64_t& state) {
    state += 0x9E3779B97F4A7C15ULL;
    return static_cast<double>(splitmix64(state) >> 11) /
           static_cast<double>(1ull << 53);
}

} // namespace

void ParticleSystem::spawnBreak(int cx, int cy, int cz, BlockID id,
                                float skyLight, float blockLight,
                                std::uint64_t rngSeed) {
    std::uint64_t state = rngSeed;
    for (int i = 0; i < kBreakBurstCount; ++i) {
        Particle p;
        p.block = id;
        p.skyLight = skyLight;
        p.blockLight = blockLight;

        // Start near the block center (offset in [-0.35, 0.35] blocks).
        const float ox =
            static_cast<float>(uniform01(state) - 0.5) * 0.7f;
        const float oy =
            static_cast<float>(uniform01(state) - 0.5) * 0.7f;
        const float oz =
            static_cast<float>(uniform01(state) - 0.5) * 0.7f;
        p.position = {static_cast<float>(cx) + 0.5f + ox,
                      static_cast<float>(cy) + 0.5f + oy,
                      static_cast<float>(cz) + 0.5f + oz};

        // Velocity: a uniform sphere direction (u1 = polar cosine, u2 =
        // azimuth) with speed 1.2..3.5 plus an upward bias.
        const double u1 = uniform01(state);
        const double u2 = uniform01(state);
        const double u3 = uniform01(state);
        const double u4 = uniform01(state);
        const double theta = 2.0 * 3.14159265358979323846 * u2;
        const double cosPhi = 2.0 * u1 - 1.0;
        const double sinPhi = std::sqrt(std::max(0.0, 1.0 - cosPhi * cosPhi));
        const double speed = 1.2 + 2.3 * u3;
        p.velocity = {
            static_cast<float>(sinPhi * std::cos(theta) * speed),
            static_cast<float>(cosPhi * speed) +
                static_cast<float>(1.2 + 1.5 * u4),
            static_cast<float>(sinPhi * std::sin(theta) * speed)};

        p.life = static_cast<float>(kMinLife +
                                    (kMaxLife - kMinLife) * uniform01(state));

        if (m_particles.size() >= kMaxParticles) {
            m_particles.erase(m_particles.begin()); // drop the oldest
        }
        m_particles.push_back(std::move(p));
    }
}

void ParticleSystem::update(double dt) {
    if (m_particles.empty()) {
        return;
    }
    const float h = static_cast<float>(dt);
    for (Particle& p : m_particles) {
        p.age += h;
        p.velocity.y -= kGravity * h;
        p.position = p.position + p.velocity * h;
    }
    // Compact out the expired (order-preserving).
    std::vector<Particle> alive;
    alive.reserve(m_particles.size());
    for (auto& p : m_particles) {
        if (p.age < p.life) {
            alive.push_back(std::move(p));
        }
    }
    m_particles = std::move(alive);
}

} // namespace mc
