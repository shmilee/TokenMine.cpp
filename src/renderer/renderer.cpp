#include "renderer/renderer.hpp"

#include <GL/glew.h>

#include <cstdio>

#include "core/logger.hpp"

namespace mc {

Result<void> Renderer::init() {
    // A valid context must be current (Window::init made one).
    const char* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    if (vendor == nullptr) {
        return MC_FAIL("no current GL context (call Window::init first)");
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    const GLenum glErr = glGetError();
    if (glErr != GL_NO_ERROR) {
        return MC_FAIL("GL error during renderer init: " +
                       std::to_string(static_cast<unsigned>(glErr)));
    }
    m_valid = true;
    MC_LOG_INFO("Renderer initialized (depth test + back-face culling on)");
    return {};
}

void Renderer::beginFrame(int width, int height) {
    glViewport(0, 0, width, height);
    m_stats = Stats {};
}

void Renderer::endFrame() {
    // Swap is done by Window. Nothing to flush in Phase 1.
}

void Renderer::clear(const Vec4& color) {
    glClearColor(color.x, color.y, color.z, color.w);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::enableBlending() {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Renderer::disableBlending() {
    glDisable(GL_BLEND);
}

void Renderer::draw(const Mesh& mesh, const ShaderProgram& program,
                    const Mat4& view, const Mat4& projection,
                    const Mat4& model) {
    if (!mesh.valid() || !program.valid()) {
        MC_LOG_WARN("draw() skipped: invalid mesh/program");
        return;
    }
    program.bind();
    program.setUniformMat4("u_model", model);
    program.setUniformMat4("u_view", view);
    program.setUniformMat4("u_proj", projection);
    mesh.bind();
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh.indexCount()),
                   GL_UNSIGNED_INT, nullptr);
    program.unbind();
    const GLenum glErr = glGetError();
    if (glErr != GL_NO_ERROR) {
        char hex[16];
        std::snprintf(hex, sizeof(hex), "0x%x",
                      static_cast<unsigned>(glErr));
        MC_LOG_ERROR(std::string("GL error after draw call: ") + hex);
    }

    m_stats.drawCalls++;
    m_stats.vertices += mesh.indexCount();
}

void Renderer::drawSky(const Mesh& mesh, const ShaderProgram& program,
                       const Mat4& view, const Mat4& projection) {
    if (!mesh.valid() || !program.valid()) {
        MC_LOG_WARN("drawSky() skipped: invalid mesh/program");
        return;
    }
    program.bind();
    program.setUniformMat4("u_view", view);
    program.setUniformMat4("u_proj", projection);
    mesh.bind();
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh.indexCount()),
                   GL_UNSIGNED_INT, nullptr);
    program.unbind();
    const GLenum glErr = glGetError();
    if (glErr != GL_NO_ERROR) {
        char hex[16];
        std::snprintf(hex, sizeof(hex), "0x%x",
                      static_cast<unsigned>(glErr));
        MC_LOG_ERROR(std::string("GL error after draw call: ") + hex);
    }

    m_stats.drawCalls++;
    m_stats.vertices += mesh.indexCount();
}

} // namespace mc
