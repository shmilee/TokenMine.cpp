// Renderer: the only component (besides Window, for the context) that calls
// GL APIs. Game logic submits declarative draw commands; it never touches
// GL state directly.
//
// Responsibilities in Phase 1:
//   - per-frame viewport + clear
//   - draw calls with MVP matrices
//   - frame statistics (draw calls, vertices) for the future F3 overlay
//
// Thread ownership: game thread with the GL context current (created by
// Window). Call init() after Window::init().
#pragma once

#include "core/error.hpp"
#include "core/math/mat4.hpp"
#include "core/math/vec4.hpp"
#include "renderer/mesh.hpp"
#include "renderer/shader.hpp"

namespace mc {

class Window;

class Renderer {
public:
    struct Stats {
        unsigned drawCalls = 0;
        unsigned long long vertices = 0;
    };

    Renderer() = default;
    ~Renderer() = default;
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // Enable depth test / back-face culling, verify a context exists.
    Result<void> init();
    bool valid() const { return m_valid; }

    void beginFrame(int width, int height);
    void endFrame();

    void clear(const Vec4& color);

    // Alpha blending for the transparent block pass (glass, leaves, water).
    // Call enableBlending() before the batch, disableBlending() after.
    void enableBlending();
    void disableBlending();

    // Phase 15 UI switches: the 2D overlay pass runs with the depth test
    // and back-face culling disabled (screen-space quads are drawn in an
    // y-down ortho projection where the world-space winding rules do not
    // hold). The world pass restores both before the next frame.
    void setDepthTest(bool enabled);
    void setCulling(bool enabled);
    // PRE-5 alpha cutout: glDepthMask in a line — the world pass runs
    // with the depth WRITE on (the GL default; the pre-PRE-5 behavior
    // everywhere) except the translucent-blend section, which turns it
    // off for its draws (a translucent surface must not occlude what is
    // behind it — the classic Minecraft pass discipline). The caller
    // restores it (setDepthMask(true)) after the section; a frame that
    // never turns it off leaves the GL default untouched.
    void setDepthMask(bool enabled);

    // Draw one indexed mesh. The program must declare u_model/u_view/u_proj.
    // Texture binding is the caller's job (bind the texture + set the
    // sampler uniform before this call).
    void draw(const Mesh& mesh, const ShaderProgram& program, const Mat4& view,
              const Mat4& projection, const Mat4& model);

    // Draw a sky-object mesh (sun/moon/star quads, unlit): no model
    // transform and no u_model uniform — the geometry is positioned in
    // world space by the shader (u_offset). The program must declare
    // u_view/u_proj.
    void drawSky(const Mesh& mesh, const ShaderProgram& program,
                 const Mat4& view, const Mat4& projection);

    // Stats for the current frame (reset by beginFrame).
    const Stats& stats() const { return m_stats; }

private:
    bool m_valid = false;
    Stats m_stats;
};

} // namespace mc
