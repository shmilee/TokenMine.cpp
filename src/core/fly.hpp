// V2-19: the double-tap Space fly toggle — the pure input-state decision
// (the campfire pattern: the logic is a named pure function the unit
// tests own; the Game is the thin adapter over it).
//
// The double-tap window is REAL dt (the CRAFT-7 kDoubleClickSeconds
// pattern, hard rule 8: input-only state — never frame state, never
// saved: a no-input run never presses Space, so it is structurally
// immune to the toggle). Minecraft creative uses the same double-tap
// toggle; a single tap is an ordinary jump (the second tap of a
// double-tap swallows its would-be jump — the Game suppresses the
// player's jump grant for the toggle frame, see Player).
#pragma once

namespace mc {

// The double-tap window: two Space presses within this many real
// seconds (inclusive) toggle the fly. 0.3 s = the CRAFT-7
// kDoubleClickSeconds value — the "fast double tap" band: wide enough
// for xdotool's ~50 ms click jitter (the E2E protocol), narrow enough
// that a deliberate single tap never toggles.
constexpr double kFlyDoubleTapSeconds = 0.3;

// The decision for one Space press.
struct FlyToggleDecision {
    // True when this press is the SECOND of a double-tap (the window
    // hit). A double-tap FLIPS the fly state (creative's toggle: the
    // same gesture enters and leaves); a single tap keeps it.
    bool doubleTap = false;
    // The fly state AFTER this press (doubleTap ? !flying : flying).
    bool nowFlying = false;
};

// The decision for a Space press at `now` (real seconds) given the last
// press's time (`lastPressTime`, < 0 = no press yet) and the current fly
// state. The window is INCLUSIVE at kFlyDoubleTapSeconds (the CRAFT-7
// predicate's `<=`): a tap exactly on the boundary still pairs.
FlyToggleDecision flyToggleDecision(double now, double lastPressTime,
                                    bool flying);

} // namespace mc
