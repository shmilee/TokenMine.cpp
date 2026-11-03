#include "core/fly.hpp"

namespace mc {

FlyToggleDecision flyToggleDecision(double now, double lastPressTime,
                                    bool flying) {
    FlyToggleDecision d;
    d.doubleTap =
        lastPressTime >= 0.0 &&
        (now - lastPressTime) <= kFlyDoubleTapSeconds;
    d.nowFlying = d.doubleTap ? !flying : flying;
    return d;
}

} // namespace mc
