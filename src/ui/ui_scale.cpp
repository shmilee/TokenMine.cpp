#include "ui/ui_scale.hpp"

#include <cmath>
#include <string>

namespace mc {

Result<float> uiScaleFromConfig(const Config& config) {
    const std::string key = "ui.scale";
    const Json* j = config.at(key);
    if (j == nullptr) {
        return kUiScaleMin; // absent: the pixel-exact baseline
    }
    if (!j->isNumber()) {
        return MC_FAIL("config " + key + " must be a number");
    }
    const double v = j->asDouble(0.0);
    if (!std::isfinite(v) || v < kUiScaleMin || v > kUiScaleMax) {
        return MC_FAIL("config " + key + " must be in [" +
                       std::to_string(kUiScaleMin) + ", " +
                       std::to_string(kUiScaleMax) + "], got " +
                       std::to_string(v));
    }
    return static_cast<float>(v);
}

} // namespace mc
