#include <gtest/gtest.h>

#include <cmath>

#include "world/noise.hpp"

using namespace mc;

TEST(NoiseTest, HashIsDeterministicAndSeedSensitive) {
    EXPECT_EQ(hash2(3, 5, 7), hash2(3, 5, 7));
    EXPECT_NE(hash2(3, 5, 7), hash2(3, 5, 8)); // different seed
    EXPECT_NE(hash2(3, 5, 7), hash2(4, 5, 7)); // different input
    EXPECT_EQ(hash3(1, 2, 3, 4), hash3(1, 2, 3, 4));
    EXPECT_NE(hash3(1, 2, 3, 4), hash3(1, 2, 3, 5));
    EXPECT_NE(hash3(1, 2, 3, 4), hash3(1, 2, 4, 4));
}

TEST(NoiseTest, Value2DInRange) {
    for (int x = -16; x <= 16; ++x) {
        for (int y = -16; y <= 16; ++y) {
            const float v = valueNoise2(x * 0.37f, y * -0.21f, 42);
            EXPECT_GE(v, 0.0f);
            EXPECT_LT(v, 1.0f);
        }
    }
}

TEST(NoiseTest, Value3DInRange) {
    for (int x = -8; x <= 8; ++x) {
        for (int y = -8; y <= 8; ++y) {
            for (int z = -8; z <= 8; ++z) {
                const float v = valueNoise3(x * 0.5f, y * 0.5f, z * 0.5f, 999);
                EXPECT_GE(v, 0.0f);
                EXPECT_LT(v, 1.0f);
            }
        }
    }
}

TEST(NoiseTest, DeterministicForSameSeedAndCoords) {
    EXPECT_FLOAT_EQ(valueNoise2(3.7f, -1.2f, 123), valueNoise2(3.7f, -1.2f, 123));
    EXPECT_FLOAT_EQ(valueNoise3(1.5f, 2.5f, -3.5f, 999),
                    valueNoise3(1.5f, 2.5f, -3.5f, 999));
}

TEST(NoiseTest, DifferentSeedsGiveDifferentFields) {
    int equal = 0;
    for (int i = 0; i < 128; ++i) {
        const float a = valueNoise2(i * 0.37f, (i * 7 % 13) * 0.29f, 1);
        const float b = valueNoise2(i * 0.37f, (i * 7 % 13) * 0.29f, 2);
        if (a == b) {
            ++equal;
        }
    }
    // Occasional coincidental equality is possible; a correlated field is not.
    EXPECT_LT(equal, 8);
}

TEST(NoiseTest, SmoothNearbySamples) {
    // Nearby points must interpolate smoothly (no lattice discontinuities).
    const float a = valueNoise2(10.0f, 10.0f, 5);
    const float b = valueNoise2(10.02f, 10.02f, 5);
    EXPECT_NEAR(a, b, 0.05f);
}

TEST(NoiseTest, FbmInRangeAndDeterministic) {
    for (int i = 0; i < 32; ++i) {
        const float v = fbm2(i * 0.53f, -i * 0.31f, 777, 4);
        EXPECT_GE(v, 0.0f);
        EXPECT_LT(v, 1.0f);
        EXPECT_FLOAT_EQ(v, fbm2(i * 0.53f, -i * 0.31f, 777, 4));
    }
}

TEST(NoiseTest, FbmOctaveCountChangesResult) {
    EXPECT_NE(fbm2(1.3f, 2.1f, 42, 2), fbm2(1.3f, 2.1f, 42, 4));
}

TEST(NoiseTest, FbmOctavesClamped) {
    // 0 octaves falls back to 1; results stay in range.
    const float v = fbm2(0.5f, 0.5f, 42, 0);
    EXPECT_GE(v, 0.0f);
    EXPECT_LT(v, 1.0f);
}
