#include "renderer/texture.hpp"

#include <GL/glew.h>

#include <fstream>

#include "core/logger.hpp"
#include "core/png.hpp"

namespace mc {

Texture::~Texture() {
    if (m_id != 0) {
        glDeleteTextures(1, &m_id);
    }
}

Texture::Texture(Texture&& other) noexcept
    : m_id(other.m_id), m_width(other.m_width), m_height(other.m_height) {
    other.m_id = 0;
    other.m_width = 0;
    other.m_height = 0;
}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        if (m_id != 0) {
            glDeleteTextures(1, &m_id);
        }
        m_id = other.m_id;
        m_width = other.m_width;
        m_height = other.m_height;
        other.m_id = 0;
        other.m_width = 0;
        other.m_height = 0;
    }
    return *this;
}

Result<Texture> Texture::createFromPixels(int width, int height,
                                          const std::uint8_t* rgba) {
    if (width <= 0 || height <= 0 || rgba == nullptr) {
        return MC_FAIL("invalid texture dimensions/pointer");
    }

    Texture tex;
    glGenTextures(1, &tex.m_id);
    if (tex.m_id == 0) {
        return MC_FAIL("glGenTextures failed");
    }
    tex.m_width = width;
    tex.m_height = height;

    glBindTexture(GL_TEXTURE_2D, tex.m_id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, rgba);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    const GLenum glErr = glGetError();
    if (glErr != GL_NO_ERROR) {
        tex.m_id = 0;
        return MC_FAIL("GL error during texture upload: " +
                       std::to_string(static_cast<unsigned>(glErr)));
    }
    return tex;
}

Result<Texture> Texture::loadPng(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        return MC_FAIL("cannot open texture file: '" + file.string() + "'");
    }
    std::string bytes((std::istreambuf_iterator<char>(in)),
                      std::istreambuf_iterator<char>());

    auto image = Png::decode(bytes);
    MC_TRY(image);

    MC_LOG_DEBUG("Loaded texture '" + file.string() + "' (" +
                 std::to_string(image->width) + "x" +
                 std::to_string(image->height) + ")");
    return createFromPixels(image->width, image->height, image->rgba.data());
}

void Texture::bind() const {
    glBindTexture(GL_TEXTURE_2D, m_id);
}

void Texture::setRepeat(bool on) const {
    glBindTexture(GL_TEXTURE_2D, m_id);
    const GLenum mode = on ? GL_REPEAT : GL_CLAMP_TO_EDGE;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, mode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, mode);
    glBindTexture(GL_TEXTURE_2D, 0);
}

} // namespace mc
