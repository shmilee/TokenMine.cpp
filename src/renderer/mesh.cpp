#include "renderer/mesh.hpp"

#include <GL/glew.h>

#include <cstddef>

#include "core/logger.hpp"

namespace mc {

namespace {
constexpr std::size_t kVertexSize = sizeof(MeshVertex); // 52 bytes, packed
}

Mesh::~Mesh() {
    destroy();
}

void Mesh::destroy() {
    if (m_vao != 0) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
    if (m_vbo != 0) {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }
    if (m_ibo != 0) {
        glDeleteBuffers(1, &m_ibo);
        m_ibo = 0;
    }
    m_vertexCount = 0;
    m_indexCount = 0;
    m_mode = MeshMode::Triangles;
}

Mesh::Mesh(Mesh&& other) noexcept {
    m_vao = other.m_vao;
    m_vbo = other.m_vbo;
    m_ibo = other.m_ibo;
    m_vertexCount = other.m_vertexCount;
    m_indexCount = other.m_indexCount;
    m_mode = other.m_mode;
    other.m_vao = other.m_vbo = other.m_ibo = 0;
    other.m_vertexCount = other.m_indexCount = 0;
    other.m_mode = MeshMode::Triangles;
}

Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this != &other) {
        destroy();
        m_vao = other.m_vao;
        m_vbo = other.m_vbo;
        m_ibo = other.m_ibo;
        m_vertexCount = other.m_vertexCount;
        m_indexCount = other.m_indexCount;
        m_mode = other.m_mode;
        other.m_vao = other.m_vbo = other.m_ibo = 0;
        other.m_vertexCount = other.m_indexCount = 0;
        other.m_mode = MeshMode::Triangles;
    }
    return *this;
}

Result<void> Mesh::upload(const std::vector<MeshVertex>& vertices,
                          const std::vector<std::uint32_t>& indices,
                          MeshMode mode) {
    if (vertices.empty()) {
        return MC_FAIL("mesh upload with empty vertex data");
    }
    if (mode == MeshMode::Lines) {
        if (!indices.empty()) {
            return MC_FAIL("line meshes are non-indexed");
        }
        if (vertices.size() % 2 != 0) {
            return MC_FAIL("line meshes need an even vertex count, got " +
                           std::to_string(vertices.size()));
        }
    } else {
        for (const std::uint32_t idx : indices) {
            if (idx >= vertices.size()) {
                return MC_FAIL("mesh index " + std::to_string(idx) +
                               " out of range (" +
                               std::to_string(vertices.size()) + " vertices)");
            }
        }
    }

    destroy();
    m_mode = mode;
    const bool indexed = mode == MeshMode::Triangles && !indices.empty();

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    if (indexed) {
        glGenBuffers(1, &m_ibo);
    }
    if (m_vao == 0 || m_vbo == 0 || (indexed && m_ibo == 0)) {
        destroy();
        return MC_FAIL("GL buffer/VAO generation failed");
    }

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * kVertexSize),
                 vertices.data(), GL_STATIC_DRAW);

    // location 0: position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(kVertexSize),
                          reinterpret_cast<const void*>(offsetof(MeshVertex, position)));
    // location 1: normal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(kVertexSize),
                          reinterpret_cast<const void*>(offsetof(MeshVertex, normal)));
    // location 2: uv
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(kVertexSize),
                          reinterpret_cast<const void*>(offsetof(MeshVertex, uv)));
    // location 3: sky light
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(kVertexSize),
                          reinterpret_cast<const void*>(offsetof(MeshVertex, skyLight)));
    // location 4: block light
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(kVertexSize),
                          reinterpret_cast<const void*>(offsetof(MeshVertex, blockLight)));
    // location 5: local UV (tiled chunk quads, Phase 18; zero elsewhere)
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 2, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(kVertexSize),
                          reinterpret_cast<const void*>(offsetof(MeshVertex, uvLocal)));
    // location 6: tiled flag (1 = greedy-merged chunk quad, Phase 18)
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(kVertexSize),
                          reinterpret_cast<const void*>(offsetof(MeshVertex, tiled)));

    if (indexed) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(indices.size() *
                                             sizeof(std::uint32_t)),
                     indices.data(), GL_STATIC_DRAW);
    }

    glBindVertexArray(0);

    m_vertexCount = vertices.size();
    m_indexCount = indexed ? indices.size() : 0;

    const GLenum glErr = glGetError();
    if (glErr != GL_NO_ERROR) {
        destroy();
        return MC_FAIL("GL error during mesh upload: " +
                       std::to_string(static_cast<unsigned>(glErr)));
    }
    return {};
}

void Mesh::bind() const {
    glBindVertexArray(m_vao);
}

} // namespace mc
