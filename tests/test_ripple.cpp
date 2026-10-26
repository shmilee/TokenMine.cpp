// PRE-3: the water-ripple pipeline's CPU-side contracts — the MeshVertex
// water flag (appended tail field, default 0 = every pre-PRE-3 producer is
// untouched, stride pinned so the GL layout can never silently drift) and
// the render.water_ripples config key semantics (code default true, the
// render.fog / fire.particles getBool precedent — config.json is
// untouched). The GPU half (the vertex-shader UV wander) is exercised by
// the gate's three-way A/B; the frame-locked phase is a pure function of
// m_frameCount (hard rule 8), so same-frame-count runs are byte-identical
// without a separate unit seam.

#include <gtest/gtest.h>

#include <cstddef>

#include "core/config.hpp"
#include "renderer/mesh.hpp"

using mc::Config;
using mc::Json;
using mc::MeshVertex;

// The water flag sits at offset 52: the first 52 bytes are the pre-PRE-3
// layout, and PRE-3 appended one float (stride 48 -> 52 -> 56). PRE-5
// then appended the cutout mark as the new TAIL field (56 -> 60). Pinning
// the offsets and the size keeps the VAO binding (offsetof) and the GL
// stride honest if the struct is ever reordered.
TEST(MeshVertexWaterTest, WaterFlagAtOffset52AndTailCutoutAtOffset56) {
    EXPECT_EQ(offsetof(MeshVertex, water), 52u);
    EXPECT_EQ(offsetof(MeshVertex, cutout), 56u); // PRE-5 new tail
    EXPECT_EQ(sizeof(MeshVertex), 60u); // 56 -> 60 (PRE-5)
}

// Default construction = 0: every producer that never sets the field
// (greedy quads, slab/stairs/wall/fence/glass pane/cross, item cubes,
// particles, cracks, UI, mobs) is structurally inert — the vertex shader
// branch (a_water > 0.5) cannot fire.
TEST(MeshVertexWaterTest, DefaultConstructionIsZero) {
    const MeshVertex v;
    EXPECT_FLOAT_EQ(v.water, 0.0f);
}

// render.water_ripples: the key is absent from config.json by design —
// the code default (true) applies (the render.fog precedent).
TEST(RippleConfigTest, MissingKeyDefaultsToOn) {
    Config cfg;
    EXPECT_TRUE(cfg.getBool("render.water_ripples", true));
}

// An explicit false turns the ripples off (the gate's "new-off" control:
// the v0.26.0 pipeline, bit-identical by construction).
TEST(RippleConfigTest, ExplicitFalseTurnsOff) {
    Config cfg;
    cfg.set("render.water_ripples", Json(false));
    EXPECT_FALSE(cfg.getBool("render.water_ripples", true));
}
