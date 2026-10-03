// GLSL program: compile + link + cached uniform locations. RAII.
//
// The only place in the codebase that compiles shaders. Source text comes
// from the AssetManager ("shaders/basic.vert"), never hardcoded here.
// Compile/link failures return a Result whose message contains the full
// GLSL info log.
//
// Thread ownership: must be created, used and destroyed on the game thread
// with the GL context current.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/error.hpp"
#include "core/math/mat4.hpp"

namespace mc {

class ShaderProgram {
public:
    ShaderProgram() = default;
    ~ShaderProgram();
    ShaderProgram(ShaderProgram&& other) noexcept;
    ShaderProgram& operator=(ShaderProgram&& other) noexcept;
    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;

    // Compile and link a vertex + fragment shader pair.
    static Result<ShaderProgram> create(const std::string& vertexSource,
                                        const std::string& fragmentSource);

    bool valid() const { return m_id != 0; }
    std::uint32_t id() const { return m_id; }

    // glUseProgram.
    void bind() const;
    void unbind() const;

    // Uniform setters. No-op with a warning-free log when the uniform does
    // not exist (it may have been optimized out).
    void setUniform1i(const std::string& name, int value) const;
    void setUniform1f(const std::string& name, float value) const;
    void setUniform2f(const std::string& name, float x, float y) const;
    void setUniform3f(const std::string& name, float x, float y, float z) const;
    void setUniform4f(const std::string& name, float x, float y, float z,
                      float w) const;
    void setUniformMat4(const std::string& name, const Mat4& value) const;

private:
    // int (not GLint) so this header does not require a GL header.
    int location(const std::string& name) const;

    std::uint32_t m_id = 0;
    mutable std::unordered_map<std::string, int> m_uniforms;
};

} // namespace mc
