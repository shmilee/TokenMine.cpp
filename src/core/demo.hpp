// V3-9 (v3.11.0): the demo replay / record / author kernel — the 3.x
// matrix's closing item.
//
// A demo file (.dmo) is {the TM_VERSION fingerprint + the world start
// (a fresh seed | a save-slot reference, NOT embedded) + the frame dt
// sequence + the input event stream}, gzip-packed in the V3-7
// DETERMINISTIC container (the save's exact container: kGzipHeader +
// raw deflate level kSaveGzipLevel + the CRC32 / ISIZE trailer — the
// load side is the same strict decode). The demo family is INDEPENDENT
// of the save family: zero format (the save's schema-30 is untouched),
// assets/demos/ (the shipped example, committed) + data/demos/ (the
// user's records / authors, git-ignored like data/saves/).
//
// The injection kernel is the Input's public event entry points
// (handle* — the same closure pump()'s SDL events and the unit tests
// use): the REPLAY injects a frame's events into the player's Input in
// file order (the demo's dts are the ONLY time source — the wall clock
// only paces the rendering); the RECORD taps at the Input boundary
// (record-first-then-inject, a zero-side-effect observer); the AUTHOR
// expands a JSON action timeline into the event stream + the fixed
// 1/60 dts (wall-clock-free = fully frame-locked determinism — the
// shipped example's first-play x2 byte-identity acceptance).
//
// The audience channel (the replay's SDL consumer, Game's m_
// audienceInput) is a separate Input instance: its events reach ONLY
// the camera + playback-control layer (the whitelist — orbit /
// speed / pause / view mode; the player's Input is demo-fed only).
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "core/error.hpp"
#include "core/input.hpp"
#include "core/json.hpp"

namespace mc {

// The recorded build's string (TM_VERSION at record / author time) —
// INFORMATIONAL only (the replay log line / the end panel's
// "recorded with …"): the replay-admissibility gate is the SIM
// version below, not the build string (v3.13.0 user ruling: the
// build-string fingerprint was a proxy for "the simulation is
// identical" and any core bump — even a render-constant-only release
// like v3.12.0's shadow scale — killed every recorded demo).
inline constexpr const char* kDemoFileVersion = TM_VERSION;

// The demo's SIMULATION-semantics version (the replay-admissibility
// gate — the demo family's own number, independent of TM_VERSION):
// BUMP ONLY when a frame-locked SIMULATION semantics changes (the
// update loop / physics / AI / fluid / spawn / input routing / the
// demo pacing). Pure render / UI / audio / content-data (JSON) /
// docs releases do NOT bump = the demo stays replayable across them
// (the "compatible when it should be" half). The "reject when it
// should be" half is enforced by the CANARY: the shipped demos are
// replayed in every gate (savenorm EXACT vs the reference tree) — a
// core change that alters the simulation breaks the canary = bump
// here + re-author the timelines (byte-reproducible, PITS 393).
// Legacy files (the pre-v3.13.0 `version`-only schema) map to sim 1
// (sound: the old strict gate proved record-build == replay-build,
// hence sim-identical lineage).
//
// Bump history: 1 = the V3-9..V3-13 era (the sim gate's introduction).
// 2 (v3.14.0, V3-8b) = the gamepad's INPUT-SEMANTICS change: B/EAST
// sprint -> the dedicated flight toggle, L3 -> crouch, the RT trigger
// -> sprint — a recorded gamepad event replays through the CURRENT
// interpretation, so a sim-1 demo carrying L3/RT/B events would
// silently behave differently = the bump (the discipline's "input
// routing" clause, first real instance).
inline constexpr int kDemoSimVersion = 2;

// The demo dt's bound: the main loop's kMaxDt clamp (the same value —
// a recorded wall dt is already clamped by the loop, an author's 1/60
// is far below; the parse side's strict window is [0, 0.25]).
inline constexpr double kDemoMaxDt = 0.25;

// The author timeline's fixed dt (documented deviation 4: the author
// is wall-clock-free = fully frame-locked; 1/60 = the frame-locked
// systems' step, the run loop's nominal pace).
inline constexpr double kDemoAuthorDt = 1.0 / 60.0;

// The replay's speed ladder (the audience's wheel cycles 0.5 -> 1 ->
// 2; documented deviation 1: no 4x). 0.5x = frame HOLD (the demo frame
// renders twice, the second render-only — a frame-locked system must
// never be double-pushed, update(0) would advance it); 2x = the pacing
// interval halves (render frames stay 1:1 with demo frames). The
// speed NEVER touches the simulation (the dts are the time source).
inline constexpr double kDemoSpeedMin = 0.5;
inline constexpr double kDemoSpeedDefault = 1.0;
inline constexpr double kDemoSpeedMax = 2.0;

// The replay's SYNTHETIC gamepad device's which (SDL ids are 1-based):
// a demo's gamepad events reach the player's Input through the
// handleGamepad* entry points — the exact closure the pump() uses;
// the player's Input has no real device on the replay machine, so the
// replay's start "connects" this one (the handlers' not-connected
// guard opens, the source-bit semantics are the record side's). The
// audience's Input is a separate instance (its real device, if any,
// never collides — the single-gamepad model is per-instance).
inline constexpr std::uint32_t kDemoSyntheticGamepadWhich = 1;

// The audience orbit's look speed (radians / pixel, the player's
// default mouse_sensitivity — the audience's mouse + the gamepad's
// RIGHT stick drive the orbit on the same pixels-equivalent scale the
// player's look uses).
inline constexpr float kAudienceOrbitSensitivity = 0.0025f;
// The audience orbit's pitch clamp (the player's range, ~89 deg —
// Player::kMaxPitch is private; the render-only offset must never flip
// the view over).
inline constexpr float kAudienceOrbitMaxPitch = 1.5533f;
// The audience orbit's gamepad LEFT-stick step (radians / frame while
// a digitized direction holds — the Input exposes the digitized state,
// not the analog deflection; the v1 coarse step, the documented
// limitation).
inline constexpr float kAudienceStickOrbitStep = 0.03f;
// The replay's paused / finished-observation render pace (~60 fps —
// the wall-clock-only pacing; the simulation is frozen there).
inline constexpr int kDemoIdleRenderDelayMs = 16;
// The pacing's time unit (the demo's µs scale — the int64 drift guard's
// divisor and the dt->µs conversion).
inline constexpr double kDemoUsPerSecond = 1000000.0;
// The pacing's MINIMUM wait (seconds): below this the sub-2ms delay is
// busy-spin noise (the wall clock's jitter exceeds it), so the wait is
// skipped — the simulation is never affected (the pacing only paces the
// render).
inline constexpr double kDemoMinPacingWaitSeconds = 0.002;

// The demo file's world start point (documented deviation 6: a
// REFERENCE, not an embed — a fresh seed = the --new --seed S worldgen
// (the same deterministic generation); a save slot = the slot's state
// is the start point (a deleted slot = the named reject "save slot
// not found"; a slot MODIFIED between record and replay = the start
// point shifts with it — the documented boundary)).
struct DemoWorldSpec {
    enum class Type { Fresh, Save } type = Type::Fresh;
    std::int64_t seed = 0; // Fresh only
    std::string slot;      // Save only (the --save charset)
};

// One recorded / authored input event: the demo frame index (0-based,
// the SIMULATED frame the event feeds) + the raw InputEvent (the
// handler's arguments, the replay's faithfulness).
struct DemoEvent {
    int frame = 0;
    InputEvent event;
};

struct DemoFile {
    std::string recordedWith; // the record-time build string
                              // (informational; the gate is `sim`)
    int sim = 1;              // the record-time kDemoSimVersion (the
                              // replay-admissibility gate)
    DemoWorldSpec world;
    std::vector<double> dts;         // per demo frame (0-based)
    std::vector<DemoEvent> events;   // frame non-decreasing (enforced)
    bool hasGamepadEvents = false;   // the replay's synthetic-connect
                                     // decision (a parse-time scan)
};

// -- strict parse (the save discipline) ------------------------------------
//
// The payload JSON's parse. v3.13.0: TWO schemas — the new file
// {sim, recorded_with, world, dts, events} (sim = the record-time
// kDemoSimVersion, the replay-admissibility gate; recorded_with = the
// record-time build string, informational only) and the LEGACY file
// {version, world, dts, events} (pre-v3.13.0), which maps to sim := 1
// + recorded_with := version (the old strict gate proved record-build
// == replay-build, hence the lineage is sim-identical). Both
// `version` AND `sim` or `recorded_with` present = the named
// mixed-schema reject.
//
// The reject family (all named): missing key / unexpected key (the
// closed vocabulary — the top level is exactly {sim, recorded_with,
// world, dts, events} (new) or {version, world, dts, events}
// (legacy); the world exactly {type, seed} or {type, slot}; every
// event type its exact key set) / unknown event type / out-of-order
// frame / event frame out of range (>= dts.size(), the dangling frame)
// / negative dt / dt > kDemoMaxDt / non-integer frame / world double
// key (a fresh with a slot or vice versa) / fresh missing seed / save
// missing slot / invalid slot charset (the --save charset, the
// traversal guard).
Result<DemoFile> parseDemoJson(const Json& root);

// A .dmo's load: the bytes read (a missing file is named) + the gzip
// MAGIC PROBE (the demo family has no plaintext legacy — a non-gzip
// file is named "not a gzip demo") + the container's strict decode
// (the V3-7 gzipDecode: the pinned 10-byte header / the raw inflate /
// the CRC32+ISIZE trailer) + the payload's strict parse + the SIM
// version gate (a mismatch = "demo sim X != this build sim Y
// (recorded with Z) - the simulation semantics differ"; the
// `forceSimMismatch` escape — main's `--demo-force` — bypasses the
// gate only, the schema's strictness stays).
Result<DemoFile> loadDemoFile(const std::filesystem::path& path,
                              bool forceSimMismatch = false);

// The payload's serialize (the deterministic compact JSON: the
// alphabetical key order, the doubles' shortest round-trip, the
// integers' exact values — a re-parse of the output is field-identical
// to the input, the unit tests pin it).
std::string serializeDemoJson(const DemoFile& file);

// A .dmo's write (the V3-7 deterministic container — the same
// gzipEncode the save uses, the write-side single choke point — + the
// POSIX atomic write, the save's writeAtomicFile twin; the parent
// directory is created when missing, data/demos/ for records).
Result<void> writeDemoFile(const std::filesystem::path& path,
                           const DemoFile& file);

// The demo file NAME (the pause panel's line — the path's filename
// only; a pure function, the unit tests pin it).
std::string demoFileName(const std::string& path);

// The speed label (the "DEMO x1" badge + the panel's line — the
// ASCII 'x' stands for the multiplication sign: the font's glyph set
// carries no U+00D7, the documented substitution).
std::string demoSpeedLabel(double speed);

// The world spec's description (the log lines — "fresh seed 123456789"
// / "save slot 'x'" — a pure function).
std::string demoWorldDescription(const DemoWorldSpec& spec);

// -- the injection kernel ---------------------------------------------------
//
// The REPLAY side: a frame's events -> the target Input's handle* (in
// FILE ORDER — the recorder captured them in that order, the
// simulation's faithfulness); the dt provider (the demo's time
// source); the pure playback decisions (the render count per demo
// frame at a speed, the wheel's speed cycle).
class DemoPlayer {
public:
    explicit DemoPlayer(const DemoFile& file);
    int frameCount() const { return static_cast<int>(m_file.dts.size()); }
    bool hasGamepadEvents() const { return m_file.hasGamepadEvents; }
    // The frame's events, injected in file order. A frame without
    // events = zero calls (the no-op is structural). frame outside
    // [0, frameCount) is a caller bug (asserts).
    void injectForFrame(int frame, Input& target) const;
    // The frame's dt (the simulation's step — the frame-locked systems
    // advance per call, the real-dt systems per this value).
    double dtForFrame(int frame) const;
    // The renders per demo frame at the speed (0.5 -> 2: the second
    // render is render-only — no update, no m_frameCount; 1 / 2 -> 1:
    // the 2x's interval is the pacing's halved wall-clock distance,
    // the render frames stay 1:1 with demo frames).
    static int rendersPerFrame(double speed);
    // The wheel's speed cycle (0.5 -> 1 -> 2 -> 0.5; an off-ladder
    // value snaps FORWARD to the next rung — below the min = the min,
    // between rungs = the higher rung, above the max = the wrap; the
    // audience's switch is always a rung).
    static double nextSpeed(double current);

private:
    const DemoFile& m_file;
    // The frame -> event range [begin, end) (the parse-enforced
    // non-decreasing frames; the O(n) prefix build, the per-frame
    // inject is an O(events-of-frame) slice in file order).
    std::vector<int> m_frameBegin;
};

// The RECORD side: the tap observer (the Input's setEventTap closure)
// + the per-frame dt append. O(1) per frame (vector push_back,
// amortized — dt is 1 double per frame, the events are sparse). A
// record NEVER mutates the simulation (the tap fires before the
// state change; the recorder holds no Input state). The frame
// attribution is the main loop's m_frameCount at the tap moment (the
// SIMULATED frame the event feeds).
class DemoRecorder {
public:
    DemoRecorder(); // the version fingerprint at construction
    void setWorld(const DemoWorldSpec& world) { m_file.world = world; }
    // One tapped event for the frame currently being simulated.
    void tapEvent(const InputEvent& ev, int frame);
    // The just-simulated frame's dt (the main loop's clamped value —
    // the replay's dts carry it verbatim).
    void frame(double dt) { m_file.dts.push_back(dt); }
    // The record's QUIT path (the window close / the ESC exit): the
    // frame's pump ran (events tapped) but the frame was never
    // simulated (no dt appended) — those events are the un-simulated
    // tail, EXCLUDED from the recording (the replay reproduces the
    // simulated prefix exactly; the documented deviation: a quit
    // frame's input is not part of the demo). A no-op when no tail
    // exists (the normal frame-limit exit).
    void discardPendingFrame();
    int frameCount() const { return static_cast<int>(m_file.dts.size()); }
    bool hasEvents() const { return !m_file.events.empty(); }
    // The assembled file (world + dts + events + the version
    // fingerprint + the gamepad scan) — the epilogue's write.
    const DemoFile& file() const;

private:
    DemoFile m_file; // the version fingerprint is set at construction
};

// -- the author (the timeline entry) ----------------------------------------
//
// A JSON action timeline {world, actions} expands into the demo file:
// the actions' primitives (the key / mouse / wheel vocabulary — the
// gamepad is the RECORD's face, the author's v1 rejects it, the
// documented deviation) + the dts = kDemoAuthorDt x frames (the fixed
// step — no wall clock = fully frame-locked determinism; the first-
// play x2 byte-identity is the acceptance).
struct TimelineAction {
    enum class Type {
        KeyDown,
        KeyUp,
        Hold, // key_down at atFrame, key_up at untilFrame (untilFrame
              // = -1: held to the demo's end, no release event)
        MouseDown,
        MouseUp,
        MouseMove,
        Wheel,
    } type = Type::KeyDown;
    int atFrame = 0;
    std::string key;    // Key*: the author's key name (the closed set)
    int untilFrame = -1; // Hold only
    std::string button; // Mouse*: left / right / middle
    double dx = 0.0;    // MouseMove: the delta pixels
    double dy = 0.0;
    int ticks = 0;      // Wheel: the ticks
};

// A key name -> the SDL virtual keycode (the author's closed
// vocabulary — the physical keys the Input's mapKey knows; "shift" =
// the left shift, mapKey's quick-move convention; the numpad digits
// stay unmapped, the mapKey twin). Unknown = named reject.
Result<std::uint32_t> authorKeycode(const std::string& name);

// A button name -> the SDL button (left / right / middle). Unknown =
// named reject.
Result<std::int32_t> authorButton(const std::string& name);

// The actions' parse (strict: the closed action / key / button
// vocabularies, at_frame an integer >= 0, the hold's until_frame an
// integer > at_frame, the per-action key sets exact, a gamepad action
// = named reject (the record's face)).
Result<std::vector<TimelineAction>> parseTimelineActions(const Json& actions);

// The expansion (the pure function — the unit tests pin it): the
// dts = kDemoAuthorDt x frames, frames = max(all at_frame + 1, all
// until_frame + 1) (no action = 0 frames, the empty demo); the event
// order = (frame, action index, the hold's release = the same
// action's second half) — the deterministic total order the
// simulation's faithfulness needs.
DemoFile authorDemoFile(const DemoWorldSpec& world,
                        const std::vector<TimelineAction>& actions);

// The --demo-author headless path: the timeline JSON's read (strict
// top level: exactly {world, actions}) + the parse + the expansion +
// the .dmo's write. The output = the input path's extension swapped
// to .dmo (a.json -> a.dmo; no extension -> appended). The world spec
// is the timeline's (the file's start point — the CLI's --seed /
// --save / --new stay out of the author, main enforces it).
Result<std::filesystem::path> authorDemoFromFile(
    const std::filesystem::path& timelinePath);

} // namespace mc
