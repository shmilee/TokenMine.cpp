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

Result<double> uiFontScaleFromConfig(const Config& config) {
    // UX-8e: same strict-read discipline as uiScaleFromConfig (the
    // SAVE-1/UX-1 pattern): absent = the bit-exact default, present =
    // a finite number in (0, kFontScaleMax], everything else named.
    const std::string key = "ui.font_scale";
    const Json* j = config.at(key);
    if (j == nullptr) {
        return kFontScaleDefault; // absent: the v1.44.0 bit-exact baseline
    }
    if (!j->isNumber()) {
        return MC_FAIL("config " + key + " must be a number");
    }
    const double v = j->asDouble(0.0);
    if (!std::isfinite(v)) {
        return MC_FAIL("config " + key + " must be finite");
    }
    if (v <= 0.0 || v > kFontScaleMax) {
        return MC_FAIL("config " + key + " must be in (0, " +
                       std::to_string(kFontScaleMax) + "], got " +
                       std::to_string(v));
    }
    return v;
}

} // namespace mc
