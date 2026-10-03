#include "renderer/shader.hpp"

#include <GL/glew.h>

#include "core/logger.hpp"

namespace mc {

namespace {

// Compile a single shader stage; on failure the GLSL info log is returned
// in the error message so the problem is diagnosable from the log file.
Result<std::uint32_t> compileStage(GLenum type, const std::string& source,
                                   const std::string& stageName) {
    const std::uint32_t shader = static_cast<std::uint32_t>(glCreateShader(type));
    if (shader == 0) {
        return MC_FAIL("glCreateShader(" + stageName + ") failed");
    }
    const char* sourcePtr = source.c_str();
    glShaderSource(shader, 1, &sourcePtr, nullptr);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok == 0) {
        char log[4096];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        glDeleteShader(shader);
        return MC_FAIL("shader compilation failed (" + stageName +
                       "):\n" + log);
    }
    return shader;
}

} // namespace

ShaderProgram::~ShaderProgram() {
    if (m_id != 0) {
        glDeleteProgram(m_id);
    }
}

ShaderProgram::ShaderProgram(ShaderProgram&& other) noexcept
    : m_id(other.m_id), m_uniforms(std::move(other.m_uniforms)) {
    other.m_id = 0;
}

ShaderProgram& ShaderProgram::operator=(ShaderProgram&& other) noexcept {
    if (this != &other) {
        if (m_id != 0) {
            glDeleteProgram(m_id);
        }
        m_id = other.m_id;
        m_uniforms = std::move(other.m_uniforms);
        other.m_id = 0;
    }
    return *this;
}

Result<ShaderProgram> ShaderProgram::create(const std::string& vertexSource,
                                            const std::string& fragmentSource) {
    auto vs = compileStage(GL_VERTEX_SHADER, vertexSource, "vertex");
    MC_TRY(vs);
    auto fs = compileStage(GL_FRAGMENT_SHADER, fragmentSource, "fragment");
    MC_TRY(fs);

    const std::uint32_t program = static_cast<std::uint32_t>(glCreateProgram());
    if (program == 0) {
        glDeleteShader(*vs);
        glDeleteShader(*fs);
        return MC_FAIL("glCreateProgram failed");
    }
    glAttachShader(program, *vs);
    glAttachShader(program, *fs);
    glLinkProgram(program);

    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (ok == 0) {
        char log[4096];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        glDeleteProgram(program);
        glDeleteShader(*vs);
        glDeleteShader(*fs);
        return MC_FAIL(std::string("shader linking failed:\n") + log);
    }
    glDeleteShader(*vs);
    glDeleteShader(*fs);

    ShaderProgram result;
    result.m_id = program;
    MC_LOG_DEBUG("Shader program linked (id=" + std::to_string(program) + ")");
    return result;
}

void ShaderProgram::bind() const {
    glUseProgram(m_id);
}

void ShaderProgram::unbind() const {
    glUseProgram(0);
}

int ShaderProgram::location(const std::string& name) const {
    const auto it = m_uniforms.find(name);
    if (it != m_uniforms.end()) {
        return it->second;
    }
    const int loc =
        static_cast<int>(glGetUniformLocation(m_id, name.c_str()));
    m_uniforms.emplace(name, loc);
    if (loc < 0) {
        MC_LOG_WARN("Uniform '" + name +
                    "' not found in shader (optimized out or misspelled)");
    }
    return loc;
}

void ShaderProgram::setUniform1i(const std::string& name, int value) const {
    const int loc = location(name);
    if (loc >= 0) {
        glUniform1i(loc, value);
    }
}

void ShaderProgram::setUniform1f(const std::string& name, float value) const {
    const int loc = location(name);
    if (loc >= 0) {
        glUniform1f(loc, value);
    }
}

void ShaderProgram::setUniform2f(const std::string& name, float x,
                                 float y) const {
    const int loc = location(name);
    if (loc >= 0) {
        glUniform2f(loc, x, y);
    }
}

void ShaderProgram::setUniform3f(const std::string& name, float x, float y,
                                 float z) const {
    const int loc = location(name);
    if (loc >= 0) {
        glUniform3f(loc, x, y, z);
    }
}

void ShaderProgram::setUniform4f(const std::string& name, float x, float y,
                                 float z, float w) const {
    const int loc = location(name);
    if (loc >= 0) {
        glUniform4f(loc, x, y, z, w);
    }
}

void ShaderProgram::setUniformMat4(const std::string& name,
                                   const Mat4& value) const {
    const int loc = location(name);
    if (loc >= 0) {
        glUniformMatrix4fv(loc, 1, GL_FALSE, value.m);
    }
}

} // namespace mc
