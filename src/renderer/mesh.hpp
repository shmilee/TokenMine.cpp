// GPU mesh: VAO + VBO + IBO. RAII.
//
// One mesh = one draw call's worth of geometry. Vertex format is fixed
// (MeshVertex) so the VAO layout is set once at upload time.
//
// Vertex layout (9 floats, 36 bytes):
//   location 0: position (vec3)
//   location 1: normal   (vec3)
//   location 2: uv       (vec2)
//   location 3: light    (float, 0..1 — per-vertex light for Phase 7)
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
    float light = 1.0f;
};

class Mesh {
public:
    Mesh() = default;
    ~Mesh();
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    // Upload geometry. Indices reference vertices.
    Result<void> upload(const std::vector<MeshVertex>& vertices,
                        const std::vector<std::uint32_t>& indices);

    bool valid() const { return m_vao != 0; }
    std::size_t vertexCount() const { return m_vertexCount; }
    std::size_t indexCount() const { return m_indexCount; }

    // glBindVertexArray — all vertex attributes come from the VAO.
    void bind() const;

private:
    void destroy();

    std::uint32_t m_vao = 0;
    std::uint32_t m_vbo = 0;
    std::uint32_t m_ibo = 0;
    std::size_t m_vertexCount = 0;
    std::size_t m_indexCount = 0;
};

} // namespace mc
