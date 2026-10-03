// GPU 2D texture (RGBA8). RAII.
//
// Two creation paths:
//   - createFromPixels: from an in-memory RGBA buffer (generated textures)
//   - loadPng: from a .png file via the internal Png decoder
//
// Filtered NEAREST by default (blocky game look). Call setRepeat(true) for
// tiling (ground). Thread ownership: game thread with GL context current.
#pragma once

#include <cstdint>
#include <filesystem>

#include "core/error.hpp"

namespace mc {

class Texture {
public:
    Texture() = default;
    ~Texture();
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    // rgba must be width*height*4 bytes, top-to-bottom.
    static Result<Texture> createFromPixels(int width, int height,
                                            const std::uint8_t* rgba);

    static Result<Texture> loadPng(const std::filesystem::path& file);

    bool valid() const { return m_id != 0; }
    int width() const { return m_width; }
    int height() const { return m_height; }
    std::uint32_t id() const { return m_id; }

    // glBindTexture(GL_TEXTURE_2D, id) — bind to a sampler unit of your
    // choice before calling (glActiveTexture).
    void bind() const;
    void setRepeat(bool on) const;

private:
    std::uint32_t m_id = 0;
    int m_width = 0;
    int m_height = 0;
};

} // namespace mc
