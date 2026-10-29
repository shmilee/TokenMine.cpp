// PRE-8: the burning-mob flames — the shared entity-center-cell pure
// function (entityCenterCell, the PRE-6 convention now factored out of
// itemDropLight: AABB center = position + height/2, per-axis floor
// including negative coordinates) and the render.mob_flames config
// semantics (code default true, the render.furnace_flames getBool
// precedent — config.json is untouched). The emission loop itself
// (gate + per-burning-mob spawnFire + self-lit 1.0 block term) is E2E-
// pinned at gate 144 (the live-particle class: the OFF side is the byte
// authority, the ON side's cluster bounds).

#include <gtest/gtest.h>

#include "core/config.hpp"
#include "entity/item_light.hpp"
#include "world/coordinates.hpp"

using namespace mc;

namespace {

// The entity at (22.5, 45.0, 0.5) with height 1.9 (a zombie) has its
// center at (22.5, 45.95, 0.5) -> cell (22, 45, 0): the BODY cell, not
// the feet cell, not a blend.
TEST(EntityCenterCellTest, BodyCellOfATallEntity) {
    const BlockPos cell = entityCenterCell({22.5f, 45.0f, 0.5f}, 1.9f);
    EXPECT_EQ(cell.x, 22);
    EXPECT_EQ(cell.y, 45);
    EXPECT_EQ(cell.z, 0);
}

// Negative coordinates FLOOR (hard rule 1): the entity at (-0.2, -1.3,
// 0.9) with height 1.9 has its center at (-0.2, -0.35, 0.9) -> cell
// (-1, -1, 0). Truncation would give (0, 0, 0).
TEST(EntityCenterCellTest, NegativeCoordinatesUseFloor) {
    const BlockPos cell = entityCenterCell({-0.2f, -1.3f, 0.9f}, 1.9f);
    EXPECT_EQ(cell.x, -1);
    EXPECT_EQ(cell.y, -1);
    EXPECT_EQ(cell.z, 0);
}

// A center exactly on a cell TOP boundary keeps the lower cell (floor
// semantics): position (5.0, 3.0, 5.0) + height 4.0 -> center
// (5.0, 5.0, 5.0) -> cell (5, 5, 5), not (5, 6, 5).
TEST(EntityCenterCellTest, ExactCenterOnBoundaryFloorsDown) {
    const BlockPos cell = entityCenterCell({5.0f, 3.0f, 5.0f}, 4.0f);
    EXPECT_EQ(cell.x, 5);
    EXPECT_EQ(cell.y, 5);
    EXPECT_EQ(cell.z, 5);
}

// A short entity (an item, height 0.25) whose center crosses a Y
// boundary: position (1.1, 0.875, 1.1) -> center (1.1, 1.0, 1.1) ->
// cell (1, 1, 1) (the boundary floor keeps the lower X/Z, and the
// center's exact 1.0 takes cell 1).
TEST(EntityCenterCellTest, ShortEntityCenterCrossing) {
    const BlockPos cell = entityCenterCell({1.1f, 0.875f, 1.1f}, 0.25f);
    EXPECT_EQ(cell.x, 1);
    EXPECT_EQ(cell.y, 1);
    EXPECT_EQ(cell.z, 1);
}

// render.mob_flames: the key is absent from config.json by design — the
// code default (true, the render.furnace_flames / render.drop_light
// getBool precedent) applies.
TEST(MobFlameConfigTest, MissingKeyDefaultsToOn) {
    Config cfg;
    EXPECT_TRUE(cfg.getBool("render.mob_flames", true));
}

// An explicit false turns the mob flames off (the A/B "off" control =
// zero emissions = bit-identical to v1.28.0).
TEST(MobFlameConfigTest, ExplicitFalseTurnsOff) {
    Config cfg;
    cfg.set("render.mob_flames", Json(false));
    EXPECT_FALSE(cfg.getBool("render.mob_flames", true));
}

} // namespace
