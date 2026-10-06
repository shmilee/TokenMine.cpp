// Phase 15 tests: the break particle system (deterministic spawn bursts,
// gravity integration, lifetime culling, the particle cap).
#include <gtest/gtest.h>

#include <set>
#include <vector>

#include "block/block.hpp"
#include "entity/particle.hpp"

using namespace mc;

TEST(ParticleTest, SpawnIsDeterministicForTheSameSeed) {
    ParticleSystem a, b;
    a.spawnBreak(1, 2, 3, BlockID {1}, 0.8f, 0.1f, 0x1234);
    b.spawnBreak(1, 2, 3, BlockID {1}, 0.8f, 0.1f, 0x1234);
    ASSERT_EQ(a.size(), static_cast<std::size_t>(ParticleSystem::kBreakBurstCount));
    ASSERT_EQ(a.size(), b.size());
    const auto& pa = a.particles();
    const auto& pb = b.particles();
    for (std::size_t i = 0; i < pa.size(); ++i) {
        EXPECT_FLOAT_EQ(pa[i].position.x, pb[i].position.x);
        EXPECT_FLOAT_EQ(pa[i].position.y, pb[i].position.y);
        EXPECT_FLOAT_EQ(pa[i].position.z, pb[i].position.z);
        EXPECT_FLOAT_EQ(pa[i].velocity.x, pb[i].velocity.x);
        EXPECT_FLOAT_EQ(pa[i].velocity.y, pb[i].velocity.y);
        EXPECT_FLOAT_EQ(pa[i].velocity.z, pb[i].velocity.z);
        EXPECT_FLOAT_EQ(pa[i].life, pb[i].life);
    }
}

TEST(ParticleTest, DifferentSeedsGiveDifferentBursts) {
    ParticleSystem a, b;
    a.spawnBreak(1, 2, 3, BlockID {1}, 1.0f, 0.0f, 0x1234);
    b.spawnBreak(1, 2, 3, BlockID {1}, 1.0f, 0.0f, 0x5678);
    const auto& pa = a.particles();
    const auto& pb = b.particles();
    bool anyDifferent = false;
    for (std::size_t i = 0; i < pa.size(); ++i) {
        if (pa[i].position.x != pb[i].position.x ||
            pa[i].velocity.y != pb[i].velocity.y) {
            anyDifferent = true;
            break;
        }
    }
    EXPECT_TRUE(anyDifferent);
}

TEST(ParticleTest, BurstParticlesAreDistinctWithinOneBurst) {
    // Phase 29 regression: uniform01 must advance the splitmix64 state —
    // before the fix every call in a burst returned the same value, so all
    // 16 particles shared one offset/velocity/life (one visible point).
    ParticleSystem p;
    p.spawnBreak(1, 2, 3, BlockID {1}, 0.8f, 0.1f, 0x1234);
    const auto& parts = p.particles();
    ASSERT_EQ(parts.size(),
              static_cast<std::size_t>(ParticleSystem::kBreakBurstCount));
    std::set<float> distinctX;
    std::set<float> distinctLife;
    for (const Particle& part : parts) {
        distinctX.insert(part.position.x);
        distinctLife.insert(part.life);
    }
    EXPECT_EQ(distinctX.size(), parts.size());
    EXPECT_EQ(distinctLife.size(), parts.size());
}

TEST(ParticleTest, BurstValuesMatchTheAdvancingStream) {
    // Pins the exact Phase 29 stream: state += gamma before each mix, one
    // advance per uniform01 call, call order ox/oy/oz/u1..u4/life per
    // particle. Position/life involve no libm calls, so the pins hold on
    // any IEEE-754 platform.
    ParticleSystem p;
    p.spawnBreak(1, 2, 3, BlockID {1}, 0.8f, 0.1f, 0x1234);
    const auto& parts = p.particles();
    ASSERT_EQ(parts.size(),
              static_cast<std::size_t>(ParticleSystem::kBreakBurstCount));
    EXPECT_FLOAT_EQ(parts[0].position.x, 1.4108357429504395f);
    EXPECT_FLOAT_EQ(parts[0].position.y, 2.396921157836914f);
    EXPECT_FLOAT_EQ(parts[0].position.z, 3.5287749767303467f);
    EXPECT_FLOAT_EQ(parts[0].life, 0.6028023362159729f);
    EXPECT_FLOAT_EQ(parts[1].position.x, 1.187707781791687f);
    EXPECT_FLOAT_EQ(parts[1].position.y, 2.493948221206665f);
    EXPECT_FLOAT_EQ(parts[1].position.z, 3.238013982772827f);
    EXPECT_FLOAT_EQ(parts[1].life, 0.5545276999473572f);
}

TEST(ParticleTest, ParticlesStartNearTheBrokenBlock) {
    ParticleSystem p;
    p.spawnBreak(10, 20, 30, BlockID {2}, 1.0f, 0.0f, 42);
    for (const Particle& part : p.particles()) {
        EXPECT_NEAR(part.position.x, 10.5, 0.36);
        EXPECT_NEAR(part.position.y, 20.5, 0.36);
        EXPECT_NEAR(part.position.z, 30.5, 0.36);
        EXPECT_GE(part.life, ParticleSystem::kMinLife - 1e-6);
        EXPECT_LE(part.life, ParticleSystem::kMaxLife + 1e-6);
        EXPECT_EQ(part.block, BlockID {2});
    }
}

TEST(ParticleTest, UpdateAppliesGravityAndCullsExpired) {
    ParticleSystem p;
    p.spawnBreak(0, 0, 0, BlockID {1}, 1.0f, 0.0f, 42);
    ASSERT_GT(p.size(), 0u);
    // A one-second step: every particle (life <= kMaxLife) is gone.
    p.update(1.0);
    EXPECT_TRUE(p.empty());
}

TEST(ParticleTest, UpdateIntegratesVelocityAndAge) {
    ParticleSystem p;
    p.spawnBreak(0, 0, 0, BlockID {1}, 1.0f, 0.0f, 42);
    const Particle before = p.particles().front();
    const double dt = 0.1;
    p.update(dt);
    // Every particle lives at least 0.45 s, so all survive a 0.1 s step.
    ASSERT_EQ(p.size(), ParticleSystem::kBreakBurstCount);
    const auto& parts = p.particles();
    const Particle& after = parts.front();
    EXPECT_FLOAT_EQ(after.age, 0.1f);
    // Semi-implicit Euler: the velocity is updated first, then the
    // position integrates the NEW velocity.
    EXPECT_NEAR(after.velocity.y, before.velocity.y -
                                       ParticleSystem::kGravity * 0.1f,
                1e-4);
    EXPECT_NEAR(after.position.y,
                before.position.y +
                    (before.velocity.y - ParticleSystem::kGravity * 0.1f) *
                        0.1f,
                1e-4);
    if (before.velocity.y < 0.0f) {
        EXPECT_LT(after.position.y, before.position.y); // falling
    }
}

TEST(ParticleTest, CapDropsTheOldest) {
    ParticleSystem p;
    // 20 bursts x 16 = 320 > kMaxParticles.
    for (int i = 0; i < 20; ++i) {
        p.spawnBreak(0, 0, 0, BlockID {1}, 1.0f, 0.0f,
                     static_cast<std::uint64_t>(i));
    }
    EXPECT_EQ(p.size(), ParticleSystem::kMaxParticles);
}

TEST(ParticleTest, ClearEmpties) {
    ParticleSystem p;
    p.spawnBreak(0, 0, 0, BlockID {1}, 1.0f, 0.0f, 1);
    p.clear();
    EXPECT_TRUE(p.empty());
}
