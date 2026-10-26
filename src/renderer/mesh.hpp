// GPU mesh: VAO + VBO + IBO. RAII.
//
// One mesh = one draw call's worth of geometry. Vertex format is fixed
// (MeshVertex) so the VAO layout is set once at upload time.
//
// Vertex layout (14 floats, 56 bytes):
//   location 0: position   (vec3)
//   location 1: normal     (vec3)
//   location 2: uv         (vec2) — legacy producers and single-face
//                             chunk quads: atlas-space UV. Greedy-merged
//                             chunk quads (tiled = 1, Phase 18): the
//                             atlas TILE INDEX (integer col/row).
//   location 3: skyLight   (float, 0..1 — light value / 15)
//   location 4: blockLight (float, 0..1 — light value / 15)
//   location 5: uvLocal    (vec2) — tiled chunk quads: position in TILE
//                             UNITS (a run of W x H merged faces spans
//                             [0, W) x [1 - H, 1] around the seed cell;
//                             the fragment shader wraps the fractional
//                             part back into the tile). Water TOP faces
//                             (PRE-3) use it too, carrying the four
//                             tile-unit corners (0|1, 0|1). Zero
//                             otherwise.
//   location 6: tiled      (float, 0 or 1) — 1 selects the tiled UV
//                             path in the fragment shader (Phase 18
//                             greedy meshing). Zero for every legacy
//                             producer (item cube, particles, cracks,
//                             UI), whose atlas-space UVs are sampled
//                             exactly as before.
//   location 7: water      (float, 0 or 1) — PRE-3: 1 on WATER TOP-face
//                             vertices only (the chunk mesher's emitFluid
//                             marks them; every other producer keeps the
//                             default 0). Water top faces additionally use
//                             the TILED encoding (locations 2/5/6: tile
//                             index + tile units) so the vertex shader's
//                             UV-wander — applied by the fragment stage's
//                             tiled branch — stays tile-local; the wander
//                             branch runs only on marked vertices, so a
//                             0 field is inert for all pre-PRE-3 geometry
//                             and for the whole pipeline when
//                             render.water_ripples is off.
//
// Thread ownership: game thread with GL context current.
#pragma once

#include <cstdint>
#include <vector>

#include "core/error.hpp"
#include "core/math/vec2.hpp"
#include "core/math/vec3.hpp"

namespace mc {

struct MeshVertex {
    Vec3 position {0.0f, 0.0f, 0.0f};
    Vec3 normal {0.0f, 0.0f, 1.0f};
    Vec2 uv {0.0f, 0.0f};
    float skyLight = 0.0f;
    float blockLight = 0.0f;
    Vec2 uvLocal {0.0f, 0.0f};
    float tiled = 0.0f;
    // PRE-3 water ripples: 1.0 on water TOP-face vertices (emitFluid),
    // 0.0 everywhere else. The default keeps every pre-PRE-3 producer
    // (greedy quads, emitSlab/Stairs/Wall/Fence/GlassPane/Cross, item
    // cubes, particles, cracks, UI, mobs) byte-identical.
    float water = 0.0f;
};

// GL primitive for the draw. Triangles is indexed (the chunk meshes and
// the UI quad batches, which are non-indexed triangle strips flattened to
// two triangles per quad) or non-indexed; Lines is always non-indexed
// (the Phase 15 target-block outline: 24 vertices = 12 line segments).
enum class MeshMode {
    Triangles,
    Lines,
};

class Mesh {
public:
    Mesh() = default;
    ~Mesh();
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    // Upload geometry. For Triangles the indices reference vertices (may
    // be empty, in which case the vertices are drawn in order); for Lines
    // the indices must be empty.
    Result<void> upload(const std::vector<MeshVertex>& vertices,
                        const std::vector<std::uint32_t>& indices = {},
                        MeshMode mode = MeshMode::Triangles);

    // ENT-1: upload the same geometry as a DYNAMIC mesh — the VBO is
    // GL_DYNAMIC_DRAW because the CPU rewrites the WHOLE vertex buffer
    // every frame (the mob limb-swing: the swing parts' vertices rotate
    // about their pivots, the rest is copied verbatim). The indices are
    // static (the swing never changes the topology). Triangles mode only.
    Result<void> uploadDynamic(const std::vector<MeshVertex>& vertices,
                               const std::vector<std::uint32_t>& indices);

    // ENT-1: rewrite the whole dynamic vertex buffer (the frame's swing
    // pose). The count must equal the upload's (the swing remaps
    // vertices, it never adds or removes them) — a mismatch is a
    // programming error (assert), not a GL condition.
    void updateVertices(const std::vector<MeshVertex>& vertices);

    bool valid() const { return m_vao != 0; }
    std::size_t vertexCount() const { return m_vertexCount; }
    std::size_t indexCount() const { return m_indexCount; }
    MeshMode mode() const { return m_mode; }

    // glBindVertexArray — all vertex attributes come from the VAO.
    void bind() const;

private:
    void destroy();
    // The eight attribute pointers for the fixed MeshVertex layout —
    // called with the VAO and the VBO bound (shared by upload() and
    // uploadDynamic()).
    void setupAttributes();

    std::uint32_t m_vao = 0;
    std::uint32_t m_vbo = 0;
    std::uint32_t m_ibo = 0;
    std::size_t m_vertexCount = 0;
    std::size_t m_indexCount = 0;
    MeshMode m_mode = MeshMode::Triangles;
};

} // namespace mc
