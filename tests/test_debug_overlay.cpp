// Phase 15 UI tests: the F3 debug overlay line formatting (a pure
// function of a state snapshot).
#include <gtest/gtest.h>

#include "ui/debug_overlay.hpp"

using namespace mc;

TEST(DebugOverlayTest, LinesFormat) {
    DebugInfo info;
    info.fps = 60;
    info.frame = 1234;
    info.drawCalls = 45;
    info.vertices = 987654;
    info.chunks = 2312;
    info.posX = 0.5;
    info.posY = 54.0;
    info.posZ = -34.7;
    info.yaw = 0.0;
    info.pitch = -0.25;
    info.columnX = 0;
    info.columnZ = -3;
    info.seed = 123456789;
    info.timeOfDay = 0.333;
    info.dayFactor = 1.0;
    info.pointerLocked = true;
    info.streamRadius = 8;
    info.particles = 7;
    info.entities = 3;
    info.health = 17.0;
    info.maxHealth = 20.0;
    info.absorption = 4.0;
    info.armorPoints = 10;
    info.oxygen = 132;
    info.hunger = 14.0;
    info.maxHunger = 20.0;
    info.difficulty = "hard";
    const auto lines = debugOverlayLines(info);
    ASSERT_EQ(lines.size(), 15u);
    // Pinned literal (deliberately NOT the TM_VERSION macro): a version
    // bump must force a conscious update of this pin.
    EXPECT_EQ(lines[0], "TokenMine.cpp v1.35.0");
    EXPECT_EQ(lines[1], "60 FPS | frame 1234");
    EXPECT_EQ(lines[2], "chunks 2312 | draws 45 | verts 987654");
    EXPECT_EQ(lines[3], "pos 0.50, 54.00, -34.70  yaw 0.00  pitch -0.25");
    EXPECT_EQ(lines[4], "col (0, -3)  seed 123456789");
    EXPECT_EQ(lines[5], "time 0.333  day 1.00  locked");
    EXPECT_EQ(lines[6], "streaming idle  pending 0  radius 8");
    EXPECT_EQ(lines[7], "inventory closed  particles 7  entities 3");
    EXPECT_EQ(lines[8], "health 17.0 / 20.0  absorption 4.0  armor 10");
    // SURV-2: the oxygen line (the air units left).
    EXPECT_EQ(lines[9], "oxygen 132");
    EXPECT_EQ(lines[10], "hunger 14 / 20");
    // SURV-3: the held item line (the damage-less baseline state).
    EXPECT_EQ(lines[11], "held (none)");
    EXPECT_EQ(lines[12], "difficulty hard");
    // Phase 46: no active effects -> the baseline "(none)" line.
    EXPECT_EQ(lines[13], "effects (none)");
    // PRE-12 (the SURV-7 display boundary): no stats -> the baseline
    // "(none)" line.
    EXPECT_EQ(lines[14], "stats (none)");
}

TEST(DebugOverlayTest, HeldItemLineWithDurability) {
    // SURV-3: the held item carries its durability readout (d/max) —
    // the game fills the name + value; an empty hand / unset renders
    // "(none)".
    DebugInfo info;
    info.heldItem = "iron_pickaxe 238.0/250";
    const auto lines = debugOverlayLines(info);
    EXPECT_EQ(lines[11], "held iron_pickaxe 238.0/250");
    DebugInfo empty;
    EXPECT_EQ(debugOverlayLines(empty)[11], "held (none)");
}

TEST(DebugOverlayTest, ActiveEffectsLine) {
    // Phase 46: the active status effects render as "name 12.3s; ..."
    // (application order, one decimal place). PRE-12 (the SURV-6 display
    // boundary): the game fills in the amplifier roman (tier 0 = "I").
    DebugInfo info;
    info.effects = "poison I 12.3s; resistance II 4.5s";
    const auto lines = debugOverlayLines(info);
    EXPECT_EQ(lines[13], "effects poison I 12.3s; resistance II 4.5s");
    EXPECT_EQ(lines.back(), "stats (none)");
}

TEST(DebugOverlayTest, StatsLine) {
    // PRE-12 (the SURV-7 display boundary): the stats render in key
    // order, "name N", joined by " | ".
    DebugInfo info;
    info.stats = {
        {"deathCount", 1},
        {"kill.chicken", 3},
        {"kill.sheep", 2},
    };
    const auto lines = debugOverlayLines(info);
    EXPECT_EQ(lines.back(),
              "stats deathCount 1 | kill.chicken 3 | kill.sheep 2");
    // A single entry has no separator.
    DebugInfo one;
    one.stats = {{"kill.spider", 7}};
    EXPECT_EQ(debugOverlayLines(one).back(), "stats kill.spider 7");
}

TEST(DebugOverlayTest, EffectTierRoman) {
    // PRE-12: tier 0 = "I" (the MC amplifier convention), 1 = "II",
    // 2 = "III"; out-of-range clamps (defensive, saves are validated).
    EXPECT_STREQ(effectTierRoman(0), "I");
    EXPECT_STREQ(effectTierRoman(1), "II");
    EXPECT_STREQ(effectTierRoman(2), "III");
    EXPECT_STREQ(effectTierRoman(-1), "I");
    EXPECT_STREQ(effectTierRoman(5), "III");
}

TEST(DebugOverlayTest, EngagedStreamingAndOpenInventory) {
    DebugInfo info;
    info.streamingEngaged = true;
    info.streamPending = 3;
    info.inventoryOpen = true;
    info.pointerLocked = false;
    info.particles = 256;
    info.entities = 2;
    const auto lines = debugOverlayLines(info);
    EXPECT_EQ(lines[5], "time 0.000  day 0.00  free");
    EXPECT_EQ(lines[6], "streaming engaged  pending 3  radius 0");
    EXPECT_EQ(lines[7], "inventory open  particles 256  entities 2");
}
