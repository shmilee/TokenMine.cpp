#include "renderer/alpha_test.hpp"

namespace mc {

bool isPureCutoutImage(const std::uint8_t* rgba, int width, int height) {
    if (width <= 0 || height <= 0 || rgba == nullptr) {
        return false;
    }
    for (std::size_t i = 0; i < static_cast<std::size_t>(width) * height;
         ++i) {
        const std::uint8_t a = rgba[i * 4 + 3]; // the alpha channel (RGBA)
        if (a != 0 && a != 255) {
            return false; // an intermediate value: a translucent surface
        }
    }
    return true;
}

} // namespace mc
