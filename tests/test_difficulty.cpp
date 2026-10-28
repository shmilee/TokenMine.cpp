// Difficulty tests (Phase 25): the strict config parsing and the pure
// per-difficulty effects (spawn-protection radius, hostile damage scale,
// spawn-rate scale and the resulting attempt cadence) shared by the game
// (core/difficulty.hpp).
#include <gtest/gtest.h>

#include "core/difficulty.hpp"

using namespace mc;

// -- parsing (strict) ---------------------------------------------------------

TEST(DifficultyTest, ParsesTheFourSpellings) {
    // ENT-6/8 evolution: the vocabulary is easy / normal / hard /
    // peaceful (Minecraft's four).
    auto easy = parseDifficulty("easy");
    auto normal = parseDifficulty("normal");
    auto hard = parseDifficulty("hard");
    auto peaceful = parseDifficulty("peaceful");
    ASSERT_TRUE(easy.ok());
    ASSERT_TRUE(normal.ok());
    ASSERT_TRUE(hard.ok());
    ASSERT_TRUE(peaceful.ok());
    EXPECT_EQ(*easy, Difficulty::Easy);
    EXPECT_EQ(*normal, Difficulty::Normal);
    EXPECT_EQ(*hard, Difficulty::Hard);
    EXPECT_EQ(*peaceful, Difficulty::Peaceful);
    EXPECT_STREQ(difficultyName(Difficulty::Easy), "easy");
    EXPECT_STREQ(difficultyName(Difficulty::Normal), "normal");
    EXPECT_STREQ(difficultyName(Difficulty::Hard), "hard");
    EXPECT_STREQ(difficultyName(Difficulty::Peaceful), "peaceful");
}

TEST(DifficultyTest, RejectsOtherSpelling) {
    // Case-sensitive, non-empty, exact match (same discipline as the
    // other strict data fields).
    for (const char* bad : {"Easy", "EASY", "normal ", " hard", "Peaceful",
                            "peacful", "", "medium", "2"}) {
        auto r = parseDifficulty(bad);
        EXPECT_FALSE(r.ok()) << "accepted '" << bad << "'";
        if (!r.ok()) {
            EXPECT_NE(r.error().message.find("mobs.difficulty"),
                      std::string::npos);
        }
    }
}

// -- spawn protection (Minecraft radii) ---------------------------------------

TEST(DifficultyTest, ProtectionRadiusIsMinecrafts) {
    EXPECT_FLOAT_EQ(spawnProtectionRadius(Difficulty::Easy), 8.0f);
    EXPECT_FLOAT_EQ(spawnProtectionRadius(Difficulty::Normal), 12.0f);
    EXPECT_FLOAT_EQ(spawnProtectionRadius(Difficulty::Hard), 16.0f);
    // The constants and the accessor agree.
    EXPECT_FLOAT_EQ(kSpawnProtectionEasy, 8.0f);
    EXPECT_FLOAT_EQ(kSpawnProtectionNormal, 12.0f);
    EXPECT_FLOAT_EQ(kSpawnProtectionHard, 16.0f);
    // ENT-8: peaceful = the whole world is protected (every natural
    // hostile spawn is rejected; passives stay exempt).
    EXPECT_FLOAT_EQ(spawnProtectionRadius(Difficulty::Peaceful),
                    kSpawnProtectionPeaceful);
    EXPECT_FLOAT_EQ(kSpawnProtectionPeaceful, 1e6f);
    EXPECT_GT(kSpawnProtectionPeaceful, kSpawnProtectionHard);
}

// -- hostile damage scale ------------------------------------------------------

TEST(DifficultyTest, DamageScaleNormalIsDataBaseline) {
    // The mob data is the NORMAL baseline; the other difficulties scale
    // it (zombie 3 -> easy 1.5 / normal 3 / hard 4, mirroring Minecraft's
    // 2.5 / 3 / 4 relative shape).
    EXPECT_FLOAT_EQ(hostileDamageScale(Difficulty::Normal), 1.0f);
    EXPECT_NEAR(hostileDamageScale(Difficulty::Easy) * 3.0f, 1.5f, 1e-6f);
    EXPECT_NEAR(hostileDamageScale(Difficulty::Hard) * 3.0f, 4.0f, 1e-6f);
    EXPECT_LT(hostileDamageScale(Difficulty::Easy),
              hostileDamageScale(Difficulty::Normal));
    EXPECT_LT(hostileDamageScale(Difficulty::Normal),
              hostileDamageScale(Difficulty::Hard));
    // ENT-8: no hostile exists on peaceful — the scale reads as "no
    // damage" (defined for API consistency).
    EXPECT_FLOAT_EQ(hostileDamageScale(Difficulty::Peaceful), 0.0f);
}

// -- spawn rate + cadence ------------------------------------------------------

TEST(DifficultyTest, SpawnRateScales) {
    EXPECT_FLOAT_EQ(hostileSpawnRateScale(Difficulty::Easy), 0.5f);
    EXPECT_FLOAT_EQ(hostileSpawnRateScale(Difficulty::Normal), 1.0f);
    EXPECT_FLOAT_EQ(hostileSpawnRateScale(Difficulty::Hard), 1.5f);
    EXPECT_FLOAT_EQ(hostileSpawnRateScale(Difficulty::Peaceful), 1.0f);
}

TEST(DifficultyTest, CadenceFromBase30) {
    // The data cadence (30) divided by the rate scale, rounded:
    // easy 60, normal 30 (the baseline — unchanged), hard 20.
    EXPECT_EQ(checkEveryFramesFor(30, Difficulty::Easy), 60);
    EXPECT_EQ(checkEveryFramesFor(30, Difficulty::Normal), 30);
    EXPECT_EQ(checkEveryFramesFor(30, Difficulty::Hard), 20);
    // ENT-8: the peaceful cadence is unchanged (moot — the protection
    // rejects every hostile attempt before it runs).
    EXPECT_EQ(checkEveryFramesFor(30, Difficulty::Peaceful), 30);
    // The cadence never falls below 1 frame.
    EXPECT_EQ(checkEveryFramesFor(1, Difficulty::Hard), 1);
    EXPECT_EQ(checkEveryFramesFor(1, Difficulty::Easy), 2);
    // Rounding of a non-integer quotient (base 7, hard: 7/1.5 = 4.67).
    EXPECT_EQ(checkEveryFramesFor(7, Difficulty::Hard), 5);
}
