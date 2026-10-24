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
    info.hunger = 14.0;
    info.maxHunger = 20.0;
    info.difficulty = "hard";
    const auto lines = debugOverlayLines(info);
    ASSERT_EQ(lines.size(), 12u);
    // Pinned literal (deliberately NOT the TM_VERSION macro): a version
    // bump must force a conscious update of this pin.
    EXPECT_EQ(lines[0], "TokenMine.cpp v0.13.0");
    EXPECT_EQ(lines[1], "60 FPS | frame 1234");
    EXPECT_EQ(lines[2], "chunks 2312 | draws 45 | verts 987654");
    EXPECT_EQ(lines[3], "pos 0.50, 54.00, -34.70  yaw 0.00  pitch -0.25");
    EXPECT_EQ(lines[4], "col (0, -3)  seed 123456789");
    EXPECT_EQ(lines[5], "time 0.333  day 1.00  locked");
    EXPECT_EQ(lines[6], "streaming idle  pending 0  radius 8");
    EXPECT_EQ(lines[7], "inventory closed  particles 7  entities 3");
    EXPECT_EQ(lines[8], "health 17.0 / 20.0  absorption 4.0  armor 10");
    EXPECT_EQ(lines[9], "hunger 14 / 20");
    EXPECT_EQ(lines[10], "difficulty hard");
    // Phase 46: no active effects -> the baseline "(none)" line.
    EXPECT_EQ(lines[11], "effects (none)");
}

TEST(DebugOverlayTest, ActiveEffectsLine) {
    // Phase 46: the active status effects render as "name 12.3s; ..."
    // (application order, one decimal place).
    DebugInfo info;
    info.effects = "poison 12.3s; regeneration 4.5s";
    const auto lines = debugOverlayLines(info);
    EXPECT_EQ(lines.back(), "effects poison 12.3s; regeneration 4.5s");
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
