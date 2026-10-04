// GPU mesh: VAO + VBO + IBO. RAII.
//
// One mesh = one draw call's worth of geometry. Vertex format is fixed
// (MeshVertex) so the VAO layout is set once at upload time.
//
// Vertex layout (13 floats, 52 bytes):
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
//                             part back into the tile). Zero otherwise.
//   location 6: tiled      (float, 0 or 1) — 1 selects the tiled UV
//                             path in the fragment shader (Phase 18
//                             greedy meshing). Zero for every legacy
//                             producer (item cube, particles, cracks,
//                             UI), whose atlas-space UVs are sampled
//                             exactly as before.
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

    bool valid() const { return m_vao != 0; }
    std::size_t vertexCount() const { return m_vertexCount; }
    std::size_t indexCount() const { return m_indexCount; }
    MeshMode mode() const { return m_mode; }

    // glBindVertexArray — all vertex attributes come from the VAO.
    void bind() const;

private:
    void destroy();

    std::uint32_t m_vao = 0;
    std::uint32_t m_vbo = 0;
    std::uint32_t m_ibo = 0;
    std::size_t m_vertexCount = 0;
    std::size_t m_indexCount = 0;
    MeshMode m_mode = MeshMode::Triangles;
};

} // namespace mc
