#include "entity/mob_swing.hpp"

#include "entity/mob_mesh.hpp"

namespace mc {

SwingModel buildSwingModel(const MobModel& model, const AtlasLayout& layout) {
    SwingModel sm;
    // The base mesh = the static box-model geometry itself (the SAME
    // vertices/indices Game::mobMesh uploads to the per-type cache — the
    // phase-0 frame and every non-swing box are bit-identical to the
    // v0.9.0 render).
    const ItemMeshGeometry g = makeMobModelGeometry(model, layout);
    sm.base = g.vertices;
    sm.indices = g.indices;
    for (const MobSwing& s : model.swings) {
        SwingPart p;
        p.boxIndex = s.boxIndex;
        p.vertexStart = s.boxIndex * kBoxVertexCount;
        p.pivot = swingPivot(model.boxes[static_cast<std::size_t>(s.boxIndex)]);
        p.amplitudeFixed = swingFixedFromRadian(s.amplitude);
        p.group = s.group;
        // Precompute the per-effective-phase rotation: the limb angle is
        // amplitude * sin(phase) in 16.16, its (sin, cos) from the integer
        // Taylor — done ONCE per type (the build), never per frame.
        for (int eff = 0; eff < kSwingPhaseCount; ++eff) {
            const std::int32_t t = fmul(p.amplitudeFixed, kSwingSinTable[eff]);
            std::int32_t s16 = 0;
            std::int32_t c16 = 0;
            swingSinCosFixed(t, s16, c16);
            p.sinCos[2 * eff] =
                static_cast<float>(s16) / static_cast<float>(kSwingFixedScale);
            p.sinCos[2 * eff + 1] =
                static_cast<float>(c16) / static_cast<float>(kSwingFixedScale);
        }
        sm.parts.push_back(std::move(p));
    }
    return sm;
}

void buildSwingFrame(const SwingModel& model, int phase,
                     std::vector<MeshVertex>& out) {
    out = model.base;
    if (phase == 0) {
        return; // every part is at its identity pose (the table's 0)
    }
    for (const SwingPart& p : model.parts) {
        const int eff = (p.group == 1)
                            ? (phase + kSwingHalf) % kSwingPhaseCount
                            : phase;
        const float s = p.sinCos[2 * eff];
        const float c = p.sinCos[2 * eff + 1];
        if (s == 0.0f && c == 1.0f) {
            continue; // identity pose (e.g. a half-cycle crossing at N/2)
        }
        for (int i = 0; i < kBoxVertexCount; ++i) {
            MeshVertex& v = out[static_cast<std::size_t>(p.vertexStart + i)];
            v.position = swingRotateVertex(v.position, p.pivot, s, c);
            v.normal = swingRotateNormal(v.normal, s, c);
        }
    }
}

} // namespace mc
