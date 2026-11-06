#include "core/demo.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>

#include <SDL3/SDL.h>

#include "core/save.hpp"     // gzipEncode / gzipDecode / looksLikeGzip / writeAtomicFile
#include "core/save_slot.hpp" // isValidSlotName (the demo's save-slot charset)

namespace mc {

namespace {

// A JSON number's exact integer read (the parse discipline: a number
// that is not an integer value is named, not truncated).
bool isIntValue(const Json& j, long long& out) {
    if (!j.isNumber()) {
        return false;
    }
    const double v = j.asDouble();
    if (!std::isfinite(v) || v != std::floor(v) ||
        v < -9.007199254740992e15 || v > 9.007199254740992e15) {
        return false;
    }
    out = static_cast<long long>(v);
    return true;
}

// A JSON number's finite read (the mouse_move's delta / the dts'
// values — non-finite numbers are named, not silently zeroed).
bool isFiniteNumber(const Json& j, double& out) {
    if (!j.isNumber()) {
        return false;
    }
    out = j.asDouble();
    return std::isfinite(out);
}

// The demo's event type names (the CLOSED file vocabulary — the
// strict parse's table; the order is the InputEvent::Kind order, the
// two gamepad kinds carry their press state in the payload).
constexpr const char* kEventTypeNames[8] = {
    "key_down",       "key_up",         "mouse_down",   "mouse_up",
    "mouse_move",     "wheel",          "gamepad_button", "gamepad_axis"};

// The event's per-type key sets (the unexpected-key reject's table —
// every key besides {frame, type} must belong to its type's set).
const std::vector<std::string>& eventKeys(const InputEvent::Kind kind) {
    static const std::vector<std::string> kKey = {"sdl_key"};
    static const std::vector<std::string> kMouse = {"button"};
    static const std::vector<std::string> kMove = {"dx", "dy", "x", "y"};
    static const std::vector<std::string> kWheel = {"ticks"};
    static const std::vector<std::string> kGButton = {"which", "button",
                                                      "down"};
    static const std::vector<std::string> kGAxis = {"which", "axis",
                                                    "value"};
    switch (kind) {
        case InputEvent::Kind::KeyDown:
        case InputEvent::Kind::KeyUp:
            return kKey;
        case InputEvent::Kind::MouseDown:
        case InputEvent::Kind::MouseUp:
            return kMouse;
        case InputEvent::Kind::MouseMove:
            return kMove;
        case InputEvent::Kind::Wheel:
            return kWheel;
        case InputEvent::Kind::GamepadButton:
            return kGButton;
        case InputEvent::Kind::GamepadAxis:
            return kGAxis;
    }
    return kKey; // unreachable (the switch covers the closed Kind)
}

InputEvent::Kind kindForType(const std::string& type) {
    for (int i = 0; i < 8; ++i) {
        if (type == kEventTypeNames[i]) {
            return static_cast<InputEvent::Kind>(i);
        }
    }
    return InputEvent::Kind::KeyDown; // unreachable (the caller checks)
}

// The world spec's strict parse (the demo file's and the author
// timeline's shared face): exactly {type, seed} (fresh) or
// {type, slot} (save) — the double key / missing key / the invalid
// slot charset are named.
Result<DemoWorldSpec> parseWorldSpec(const Json& world) {
    if (!world.isObject()) {
        return MC_FAIL("demo world must be an object");
    }
    const Json* type = world.find("type");
    if (!type || !type->isString()) {
        return MC_FAIL("demo world is missing the 'type' key (fresh|save)");
    }
    DemoWorldSpec spec;
    if (type->asString() == "fresh") {
        spec.type = DemoWorldSpec::Type::Fresh;
        if (world.contains("slot")) {
            return MC_FAIL("demo world double key: a fresh world takes "
                           "'seed', not 'slot'");
        }
        const Json* seed = world.find("seed");
        if (!seed) {
            return MC_FAIL("demo world: a fresh start is missing 'seed'");
        }
        long long v;
        if (!isIntValue(*seed, v)) {
            return MC_FAIL("demo world: 'seed' must be an integer");
        }
        spec.seed = v;
        if (world.memberCount() != 2) {
            return MC_FAIL("demo world (fresh) must have exactly the "
                           "keys 'type' and 'seed'");
        }
    } else if (type->asString() == "save") {
        spec.type = DemoWorldSpec::Type::Save;
        if (world.contains("seed")) {
            return MC_FAIL("demo world double key: a save start takes "
                           "'slot', not 'seed'");
        }
        const Json* slot = world.find("slot");
        if (!slot || !slot->isString()) {
            return MC_FAIL("demo world: a save start is missing 'slot'");
        }
        if (!isValidSlotName(slot->asString())) {
            return MC_FAIL("demo world: 'slot' is not a valid save slot "
                           "name (letters, digits, '_' or '-')");
        }
        spec.slot = slot->asString();
        if (world.memberCount() != 2) {
            return MC_FAIL("demo world (save) must have exactly the "
                           "keys 'type' and 'slot'");
        }
    } else {
        return MC_FAIL("demo world: unknown type '" + type->asString() +
                       "' (fresh|save)");
    }
    return spec;
}

} // namespace

std::string demoWorldDescription(const DemoWorldSpec& spec) {
    return spec.type == DemoWorldSpec::Type::Fresh
               ? "fresh seed " + std::to_string(spec.seed)
               : "save slot '" + spec.slot + "'";
}

// ---------------------------------------------------------------------------
// Strict parse
// ---------------------------------------------------------------------------

Result<DemoFile> parseDemoJson(const Json& root) {
    if (!root.isObject()) {
        return MC_FAIL("demo file must be a JSON object");
    }
    // v3.13.0: TWO schemas — the legacy file (pre-v3.13.0) carries
    // {version, world, dts, events}; the current file carries
    // {sim, recorded_with, world, dts, events}. The presence of
    // `version` selects the legacy schema; both `version` AND
    // `sim`/`recorded_with` = the named mixed-schema reject.
    const Json* version = root.find("version");
    const Json* sim = root.find("sim");
    const Json* recorded = root.find("recorded_with");
    const bool legacy = version != nullptr;
    if (legacy && (sim != nullptr || recorded != nullptr)) {
        return MC_FAIL("demo file: mixed schemas — 'version' (legacy) "
                       "and 'sim'/'recorded_with' (current) are "
                       "mutually exclusive");
    }
    // The top level is the CLOSED set of the file's schema (the
    // unexpected key is named).
    for (const auto& p : root.members()) {
        const std::string& key = p.first;
        if (legacy) {
            if (key != "version" && key != "world" && key != "dts" &&
                key != "events") {
                return MC_FAIL("demo file: unexpected top-level key '" +
                               key + "' (version/world/dts/events)");
            }
        } else if (key != "sim" && key != "recorded_with" &&
                   key != "world" && key != "dts" && key != "events") {
            return MC_FAIL("demo file: unexpected top-level key '" + key +
                           "' (sim/recorded_with/world/dts/events)");
        }
    }
    DemoFile file;
    if (legacy) {
        if (!version->isString() || version->asString().empty()) {
            return MC_FAIL("demo file's 'version' key (legacy schema) "
                           "must be a non-empty string");
        }
        // The legacy mapping (zero migration): the old strict gate
        // proved record-build == replay-build, hence every legacy
        // file's lineage is sim-identical = sim 1; the build string
        // becomes the informational recorded_with.
        file.sim = 1;
        file.recordedWith = version->asString();
    } else {
        if (!sim) {
            return MC_FAIL("demo file is missing the 'sim' key (an "
                           "integer >= 1)");
        }
        long long simValue = 0;
        if (!isIntValue(*sim, simValue) || simValue < 1) {
            return MC_FAIL("demo file's 'sim' key must be an integer "
                           ">= 1");
        }
        file.sim = static_cast<int>(simValue);
        if (!recorded || !recorded->isString() ||
            recorded->asString().empty()) {
            return MC_FAIL("demo file is missing the 'recorded_with' "
                           "key (a non-empty string)");
        }
        file.recordedWith = recorded->asString();
    }
    const Json* world = root.find("world");
    if (!world) {
        return MC_FAIL("demo file is missing the 'world' key");
    }
    auto worldSpec = parseWorldSpec(*world);
    MC_TRY(worldSpec);
    file.world = *worldSpec;
    const Json* dts = root.find("dts");
    if (!dts || !dts->isArray()) {
        return MC_FAIL("demo file is missing the 'dts' key (an array of "
                       "numbers)");
    }
    file.dts.reserve(dts->size());
    for (std::size_t i = 0; i < dts->size(); ++i) {
        double v;
        if (!isFiniteNumber(dts->at(i), v)) {
            return MC_FAIL("demo dts[" + std::to_string(i) +
                           "] must be a finite number");
        }
        if (v < 0.0) {
            return MC_FAIL("demo dts[" + std::to_string(i) +
                           "] is negative (" + std::to_string(v) + ")");
        }
        if (v > kDemoMaxDt) {
            return MC_FAIL("demo dts[" + std::to_string(i) +
                           "] exceeds kDemoMaxDt " +
                           std::to_string(kDemoMaxDt) + " (got " +
                           std::to_string(v) + ")");
        }
        file.dts.push_back(v);
    }
    const Json* events = root.find("events");
    if (!events || !events->isArray()) {
        return MC_FAIL("demo file is missing the 'events' key (an array)");
    }
    const int nFrames = static_cast<int>(file.dts.size());
    file.events.reserve(events->size());
    int lastFrame = -1;
    for (std::size_t i = 0; i < events->size(); ++i) {
        const Json& e = events->at(i);
        if (!e.isObject()) {
            return MC_FAIL("demo event " + std::to_string(i) +
                           " must be an object");
        }
        long long frameV;
        const Json* frameJ = e.find("frame");
        if (!frameJ || !isIntValue(*frameJ, frameV) || frameV < 0) {
            return MC_FAIL("demo event " + std::to_string(i) +
                           " is missing a non-negative integer 'frame'");
        }
        const int frameIdx = static_cast<int>(frameV);
        if (frameIdx > lastFrame) {
            lastFrame = frameIdx;
        } else if (frameIdx != lastFrame) {
            return MC_FAIL("demo event " + std::to_string(i) +
                           " frame " + std::to_string(frameIdx) +
                           " is out of order (the last was " +
                           std::to_string(lastFrame) + ")");
        }
        if (frameIdx >= nFrames) {
            return MC_FAIL("demo event " + std::to_string(i) +
                           " frame " + std::to_string(frameIdx) +
                           " is out of range (the demo has " +
                           std::to_string(nFrames) + " frames)");
        }
        const Json* type = e.find("type");
        if (!type || !type->isString()) {
            return MC_FAIL("demo event " + std::to_string(i) +
                           " is missing the 'type' key");
        }
        const std::string typeName = type->asString();
        bool known = false;
        for (const char* name : kEventTypeNames) {
            if (typeName == name) {
                known = true;
                break;
            }
        }
        if (!known) {
            return MC_FAIL("demo event " + std::to_string(i) +
                           ": unknown type '" + typeName +
                           "' (key_down/key_up/mouse_down/mouse_up/"
                           "mouse_move/wheel/gamepad_button/gamepad_axis)");
        }
        const InputEvent::Kind kind = kindForType(typeName);
        // The per-type key set (the unexpected key is named — the
        // closed payload).
        const auto& keys = eventKeys(kind);
        for (const auto& p : e.members()) {
            const std::string& key = p.first;
            if (key == "frame" || key == "type") {
                continue;
            }
            bool ok = false;
            for (const std::string& k : keys) {
                if (key == k) {
                    ok = true;
                    break;
                }
            }
            if (!ok) {
                return MC_FAIL("demo event " + std::to_string(i) +
                               " ('" + typeName + "'): unexpected key '" +
                               key + "'");
            }
        }
        InputEvent ev;
        ev.kind = kind;
        long long v = 0;
        double d = 0.0;
        switch (kind) {
            case InputEvent::Kind::KeyDown:
            case InputEvent::Kind::KeyUp: {
                const Json* k = e.find("sdl_key");
                if (!k || !isIntValue(*k, v) || v < 0 || v > 0xFFFFFFFFLL) {
                    return MC_FAIL("demo event " + std::to_string(i) +
                                   " ('" + typeName + "'): 'sdl_key' must "
                                   "be an integer keycode");
                }
                ev.u32 = static_cast<std::uint32_t>(v);
                break;
            }
            case InputEvent::Kind::MouseDown:
            case InputEvent::Kind::MouseUp: {
                const Json* k = e.find("button");
                if (!k || !isIntValue(*k, v)) {
                    return MC_FAIL("demo event " + std::to_string(i) +
                                   " ('" + typeName + "'): 'button' must "
                                   "be an integer");
                }
                ev.code = static_cast<std::int32_t>(v);
                break;
            }
            case InputEvent::Kind::MouseMove: {
                const Json* k = e.find("dx");
                if (!k || !isFiniteNumber(*k, d)) {
                    return MC_FAIL("demo event " + std::to_string(i) +
                                   " ('mouse_move'): 'dx' must be a "
                                   "finite number");
                }
                ev.dx = static_cast<float>(d);
                k = e.find("dy");
                if (!k || !isFiniteNumber(*k, d)) {
                    return MC_FAIL("demo event " + std::to_string(i) +
                                   " ('mouse_move'): 'dy' must be a "
                                   "finite number");
                }
                ev.dy = static_cast<float>(d);
                k = e.find("x");
                if (!k || !isFiniteNumber(*k, d)) {
                    return MC_FAIL("demo event " + std::to_string(i) +
                                   " ('mouse_move'): 'x' must be a finite "
                                   "number");
                }
                ev.ax = static_cast<float>(d);
                k = e.find("y");
                if (!k || !isFiniteNumber(*k, d)) {
                    return MC_FAIL("demo event " + std::to_string(i) +
                                   " ('mouse_move'): 'y' must be a finite "
                                   "number");
                }
                ev.ay = static_cast<float>(d);
                break;
            }
            case InputEvent::Kind::Wheel: {
                const Json* k = e.find("ticks");
                if (!k || !isIntValue(*k, v)) {
                    return MC_FAIL("demo event " + std::to_string(i) +
                                   " ('wheel'): 'ticks' must be an "
                                   "integer");
                }
                ev.code = static_cast<std::int32_t>(v);
                break;
            }
            case InputEvent::Kind::GamepadButton: {
                const Json* k = e.find("which");
                if (!k || !isIntValue(*k, v) || v < 0 || v > 0xFFFFFFFFLL) {
                    return MC_FAIL("demo event " + std::to_string(i) +
                                   " ('gamepad_button'): 'which' must be "
                                   "an integer");
                }
                ev.u32 = static_cast<std::uint32_t>(v);
                k = e.find("button");
                if (!k || !isIntValue(*k, v)) {
                    return MC_FAIL("demo event " + std::to_string(i) +
                                   " ('gamepad_button'): 'button' must "
                                   "be an integer");
                }
                ev.code = static_cast<std::int32_t>(v);
                k = e.find("down");
                if (!k || !k->isBool()) {
                    return MC_FAIL("demo event " + std::to_string(i) +
                                   " ('gamepad_button'): 'down' must be a "
                                   "boolean");
                }
                ev.down = k->asBool();
                file.hasGamepadEvents = true;
                break;
            }
            case InputEvent::Kind::GamepadAxis: {
                const Json* k = e.find("which");
                if (!k || !isIntValue(*k, v) || v < 0 || v > 0xFFFFFFFFLL) {
                    return MC_FAIL("demo event " + std::to_string(i) +
                                   " ('gamepad_axis'): 'which' must be an "
                                   "integer");
                }
                ev.u32 = static_cast<std::uint32_t>(v);
                k = e.find("axis");
                if (!k || !isIntValue(*k, v)) {
                    return MC_FAIL("demo event " + std::to_string(i) +
                                   " ('gamepad_axis'): 'axis' must be an "
                                   "integer");
                }
                ev.code = static_cast<std::int32_t>(v);
                k = e.find("value");
                if (!k || !isIntValue(*k, v) || v < -32768 || v > 32767) {
                    return MC_FAIL("demo event " + std::to_string(i) +
                                   " ('gamepad_axis'): 'value' must be a "
                                   "Sint16 (-32768..32767)");
                }
                ev.value = static_cast<std::int16_t>(v);
                file.hasGamepadEvents = true;
                break;
            }
        }
        file.events.push_back(DemoEvent {frameIdx, ev});
    }
    return file;
}

Result<DemoFile> loadDemoFile(const std::filesystem::path& path,
                              bool forceSimMismatch) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return MC_FAIL("cannot open demo file: '" + path.string() + "'");
    }
    std::string raw((std::istreambuf_iterator<char>(in)),
                    std::istreambuf_iterator<char>());
    // The demo family has no plaintext legacy (the V3-7 migration era
    // is the save's, not the demo's) — the gzip magic is required.
    if (!looksLikeGzip(raw)) {
        return MC_FAIL(path.string() + " is not a gzip demo file (the "
                       "demo family is the V3-7 deterministic container)");
    }
    auto payload = gzipDecode(raw, path.string());
    MC_TRY(payload);
    auto js = Json::parse(*payload);
    MC_TRY(js);
    auto file = parseDemoJson(*js);
    MC_TRY(file);
    // The SIM version gate (v3.13.0): the build string is
    // informational, the simulation-semantics version decides.
    if (file->sim != kDemoSimVersion && !forceSimMismatch) {
        return MC_FAIL("demo sim " + std::to_string(file->sim) +
                       " != this build sim " +
                       std::to_string(kDemoSimVersion) + " (recorded "
                       "with " + file->recordedWith +
                       ") - the simulation semantics differ (use "
                       "--demo-force to replay anyway)");
    }
    return file;
}

// ---------------------------------------------------------------------------
// Serialize / write
// ---------------------------------------------------------------------------

std::string serializeDemoJson(const DemoFile& file) {
    Json root = Json::object();
    // v3.13.0: the current schema {sim, recorded_with, world, dts,
    // events} (the legacy `version`-only schema is accepted on read
    // with the sim-1 mapping, never written again).
    root["sim"] = Json(file.sim);
    root["recorded_with"] = Json(file.recordedWith);
    Json world = Json::object();
    world["type"] = Json(file.world.type == DemoWorldSpec::Type::Fresh
                             ? std::string("fresh")
                             : std::string("save"));
    if (file.world.type == DemoWorldSpec::Type::Fresh) {
        world["seed"] = Json(static_cast<long long>(file.world.seed));
    } else {
        world["slot"] = Json(file.world.slot);
    }
    root["world"] = std::move(world);
    Json dts = Json::array();
    for (const double d : file.dts) {
        dts.push_back(Json(d));
    }
    root["dts"] = std::move(dts);
    Json events = Json::array();
    for (const DemoEvent& de : file.events) {
        const InputEvent& ev = de.event;
        Json e = Json::object();
        e["frame"] = Json(de.frame);
        switch (ev.kind) {
            case InputEvent::Kind::KeyDown:
            case InputEvent::Kind::KeyUp:
                e["type"] = Json(std::string(kEventTypeNames[
                    static_cast<int>(ev.kind)]));
                e["sdl_key"] = Json(static_cast<long long>(ev.u32));
                break;
            case InputEvent::Kind::MouseDown:
            case InputEvent::Kind::MouseUp:
                e["type"] = Json(std::string(kEventTypeNames[
                    static_cast<int>(ev.kind)]));
                e["button"] = Json(ev.code);
                break;
            case InputEvent::Kind::MouseMove:
                e["type"] = Json(std::string(kEventTypeNames[
                    static_cast<int>(ev.kind)]));
                e["dx"] = Json(static_cast<double>(ev.dx));
                e["dy"] = Json(static_cast<double>(ev.dy));
                e["x"] = Json(static_cast<double>(ev.ax));
                e["y"] = Json(static_cast<double>(ev.ay));
                break;
            case InputEvent::Kind::Wheel:
                e["type"] = Json(std::string(kEventTypeNames[
                    static_cast<int>(ev.kind)]));
                e["ticks"] = Json(ev.code);
                break;
            case InputEvent::Kind::GamepadButton:
                e["type"] = Json(std::string(kEventTypeNames[
                    static_cast<int>(ev.kind)]));
                e["which"] = Json(static_cast<long long>(ev.u32));
                e["button"] = Json(ev.code);
                e["down"] = Json(ev.down);
                break;
            case InputEvent::Kind::GamepadAxis:
                e["type"] = Json(std::string(kEventTypeNames[
                    static_cast<int>(ev.kind)]));
                e["which"] = Json(static_cast<long long>(ev.u32));
                e["axis"] = Json(ev.code);
                e["value"] = Json(ev.value);
                break;
        }
        events.push_back(std::move(e));
    }
    root["events"] = std::move(events);
    return root.dump(-1);
}

Result<void> writeDemoFile(const std::filesystem::path& path,
                           const DemoFile& file) {
    // The V3-7 deterministic container (the save's exact encoder — the
    // pinned 10-byte header + the raw deflate + the CRC32/ISIZE
    // trailer) + the POSIX atomic write (the save's twin).
    auto body = gzipEncode(serializeDemoJson(file));
    MC_TRY(body);
    return writeAtomicFile(path, *body);
}

std::string demoFileName(const std::string& path) {
    return std::filesystem::path(path).filename().string();
}

std::string demoSpeedLabel(double speed) {
    if (speed == kDemoSpeedMin) {
        return "x0.5";
    }
    if (speed == kDemoSpeedDefault) {
        return "x1";
    }
    if (speed == kDemoSpeedMax) {
        return "x2";
    }
    // Off-ladder (defensive — the game only holds the rungs): the
    // shortest round-trip keeps the label honest.
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%g", speed);
    return std::string("x") + buf;
}

// ---------------------------------------------------------------------------
// DemoPlayer (the replay's injection kernel)
// ---------------------------------------------------------------------------

DemoPlayer::DemoPlayer(const DemoFile& file) : m_file(file) {
    const int n = frameCount();
    m_frameBegin.assign(n + 1, 0);
    for (const DemoEvent& de : file.events) {
        assert(de.frame >= 0 && de.frame < n);
        ++m_frameBegin[static_cast<int>(de.frame) + 1];
    }
    for (int f = 1; f <= n; ++f) {
        m_frameBegin[f] += m_frameBegin[f - 1];
    }
}

void DemoPlayer::injectForFrame(int frame, Input& target) const {
    assert(frame >= 0 && frame < frameCount());
    const int begin = m_frameBegin[frame];
    const int end = m_frameBegin[frame + 1];
    for (int i = begin; i < end; ++i) {
        const InputEvent& ev = m_file.events[i].event;
        switch (ev.kind) {
            case InputEvent::Kind::KeyDown:
                target.handleKey(ev.u32, true);
                break;
            case InputEvent::Kind::KeyUp:
                target.handleKey(ev.u32, false);
                break;
            case InputEvent::Kind::MouseDown:
                target.handleMouseButton(ev.code, true);
                break;
            case InputEvent::Kind::MouseUp:
                target.handleMouseButton(ev.code, false);
                break;
            case InputEvent::Kind::MouseMove:
                target.handleMouseMotion(ev.dx, ev.dy, ev.ax, ev.ay);
                break;
            case InputEvent::Kind::Wheel:
                target.handleMouseWheel(ev.code);
                break;
            case InputEvent::Kind::GamepadButton:
                target.handleGamepadButton(ev.code, ev.down);
                break;
            case InputEvent::Kind::GamepadAxis:
                target.handleGamepadAxis(ev.code, ev.value);
                break;
        }
    }
}

double DemoPlayer::dtForFrame(int frame) const {
    assert(frame >= 0 && frame < frameCount());
    return m_file.dts[frame];
}

int DemoPlayer::rendersPerFrame(double speed) {
    // 0.5x = the frame hold (the second render is render-only — the
    // frame-locked systems advance once per demo frame, never twice).
    return speed == kDemoSpeedMin ? 2 : 1;
}

double DemoPlayer::nextSpeed(double current) {
    // The wheel's cycle (0.5 -> 1 -> 2 -> 0.5); an off-ladder value
    // snaps FORWARD to the next rung (defensive — the game only holds
    // the rungs: below the min = the min, between rungs = the higher
    // rung, above the max = the wrap to the min).
    if (current < kDemoSpeedMin) {
        return kDemoSpeedMin;
    }
    if (current < kDemoSpeedDefault) {
        return kDemoSpeedDefault;
    }
    if (current < kDemoSpeedMax) {
        return kDemoSpeedMax;
    }
    return kDemoSpeedMin;
}

// ---------------------------------------------------------------------------
// DemoRecorder (the record's boundary tap)
// ---------------------------------------------------------------------------

DemoRecorder::DemoRecorder() {
    m_file.sim = kDemoSimVersion;
    m_file.recordedWith = kDemoFileVersion;
}

void DemoRecorder::tapEvent(const InputEvent& ev, int frame) {
    assert(frame >= 0);
    m_file.events.push_back(DemoEvent {frame, ev});
    if (ev.kind == InputEvent::Kind::GamepadButton ||
        ev.kind == InputEvent::Kind::GamepadAxis) {
        m_file.hasGamepadEvents = true;
    }
}

const DemoFile& DemoRecorder::file() const {
    return m_file;
}

void DemoRecorder::discardPendingFrame() {
    // The quit frame's events (tapped, never simulated) are the
    // un-simulated tail — excluded (see the header's documented
    // deviation). The frames are non-decreasing, so the tail is the
    // back.
    while (!m_file.events.empty() &&
           m_file.events.back().frame >=
               static_cast<int>(m_file.dts.size())) {
        m_file.events.pop_back();
    }
}

// ---------------------------------------------------------------------------
// The author (the timeline expansion)
// ---------------------------------------------------------------------------

Result<std::uint32_t> authorKeycode(const std::string& name) {
    // The author's closed key vocabulary -> the SDL virtual keycode
    // (the Input's mapKey face — the physical keys the game knows;
    // "shift" = the left shift, the mapKey's quick-move convention;
    // the numpad digits stay unmapped, the mapKey's twin).
    if (name == "w") return std::uint32_t(SDLK_W);
    if (name == "a") return std::uint32_t(SDLK_A);
    if (name == "s") return std::uint32_t(SDLK_S);
    if (name == "d") return std::uint32_t(SDLK_D);
    if (name == "q") return std::uint32_t(SDLK_Q);
    if (name == "e") return std::uint32_t(SDLK_E);
    if (name == "r") return std::uint32_t(SDLK_R);
    if (name == "c") return std::uint32_t(SDLK_C);
    if (name == "t") return std::uint32_t(SDLK_T);
    if (name == "g") return std::uint32_t(SDLK_G); // V3-11a: creative
    if (name == "space") return std::uint32_t(SDLK_SPACE);
    if (name == "shift") return std::uint32_t(SDLK_LSHIFT);
    if (name == "escape") return std::uint32_t(SDLK_ESCAPE);
    if (name == "tab") return std::uint32_t(SDLK_TAB);
    if (name == "f3") return std::uint32_t(SDLK_F3);
    if (name == "f5") return std::uint32_t(SDLK_F5);
    if (name == "1") return std::uint32_t(SDLK_1);
    if (name == "2") return std::uint32_t(SDLK_2);
    if (name == "3") return std::uint32_t(SDLK_3);
    if (name == "4") return std::uint32_t(SDLK_4);
    if (name == "5") return std::uint32_t(SDLK_5);
    if (name == "6") return std::uint32_t(SDLK_6);
    if (name == "7") return std::uint32_t(SDLK_7);
    if (name == "8") return std::uint32_t(SDLK_8);
    if (name == "9") return std::uint32_t(SDLK_9);
    return MC_FAIL("unknown timeline key '" + name +
                   "' (the vocabulary: w a s d q e r c t g space shift "
                   "escape tab f3 f5 1..9)");
}

Result<std::int32_t> authorButton(const std::string& name) {
    if (name == "left") return std::int32_t(SDL_BUTTON_LEFT);
    if (name == "right") return std::int32_t(SDL_BUTTON_RIGHT);
    if (name == "middle") return std::int32_t(SDL_BUTTON_MIDDLE);
    return MC_FAIL("unknown timeline button '" + name +
                   "' (the vocabulary: left right middle)");
}

namespace {

// The per-action allowed key sets (the unexpected-key reject's table;
// "at_frame" and "action" are in every set; the hold's until_frame is
// its optional extra).
const std::vector<std::string>& actionKeys(TimelineAction::Type type) {
    static const std::vector<std::string> kKey = {"at_frame", "action",
                                                  "key"};
    static const std::vector<std::string> kHold = {"at_frame", "action",
                                                   "key", "until_frame"};
    static const std::vector<std::string> kMouse = {"at_frame", "action",
                                                    "button"};
    static const std::vector<std::string> kMove = {"at_frame", "action",
                                                   "dx", "dy", "x", "y"};
    static const std::vector<std::string> kWheel = {"at_frame", "action",
                                                    "ticks"};
    switch (type) {
        case TimelineAction::Type::KeyDown:
        case TimelineAction::Type::KeyUp:
            return kKey;
        case TimelineAction::Type::Hold:
            return kHold;
        case TimelineAction::Type::MouseDown:
        case TimelineAction::Type::MouseUp:
            return kMouse;
        case TimelineAction::Type::MouseMove:
            return kMove;
        case TimelineAction::Type::Wheel:
            return kWheel;
    }
    return kKey; // unreachable (the switch covers the closed Type)
}

} // namespace

Result<std::vector<TimelineAction>> parseTimelineActions(const Json& actions) {
    if (!actions.isArray()) {
        return MC_FAIL("timeline 'actions' must be an array");
    }
    std::vector<TimelineAction> out;
    out.reserve(actions.size());
    for (std::size_t i = 0; i < actions.size(); ++i) {
        const Json& a = actions.at(i);
        const std::string idx = "timeline action " + std::to_string(i) + " ";
        if (!a.isObject()) {
            return MC_FAIL(idx + "must be an object");
        }
        const Json* af = a.find("at_frame");
        if (!af) {
            return MC_FAIL(idx + "is missing 'at_frame'");
        }
        long long v = 0;
        if (!isIntValue(*af, v) || v < 0) {
            return MC_FAIL(idx + "'at_frame' must be an integer >= 0");
        }
        TimelineAction t;
        t.atFrame = static_cast<int>(v);
        const Json* act = a.find("action");
        if (!act || !act->isString()) {
            return MC_FAIL(idx + "is missing 'action'");
        }
        const std::string an = act->asString();
        if (an == "key_down") {
            t.type = TimelineAction::Type::KeyDown;
        } else if (an == "key_up") {
            t.type = TimelineAction::Type::KeyUp;
        } else if (an == "hold") {
            t.type = TimelineAction::Type::Hold;
        } else if (an == "mouse_down") {
            t.type = TimelineAction::Type::MouseDown;
        } else if (an == "mouse_up") {
            t.type = TimelineAction::Type::MouseUp;
        } else if (an == "mouse_move") {
            t.type = TimelineAction::Type::MouseMove;
        } else if (an == "wheel") {
            t.type = TimelineAction::Type::Wheel;
        } else if (an == "gamepad_button" || an == "gamepad_axis") {
            return MC_FAIL(idx + "gamepad actions are the record's face "
                           "(the author v1 is keyboard/mouse)");
        } else {
            return MC_FAIL(idx + "unknown action '" + an +
                           "' (the vocabulary: key_down key_up hold "
                           "mouse_down mouse_up mouse_move wheel)");
        }
        // The unexpected key (the per-action closed set).
        const auto& keys = actionKeys(t.type);
        for (const auto& p : a.members()) {
            const std::string& key = p.first;
            bool ok = false;
            for (const std::string& k : keys) {
                if (key == k) {
                    ok = true;
                    break;
                }
            }
            if (!ok) {
                return MC_FAIL(idx + "('" + an + "'): unexpected key '" +
                               key + "'");
            }
        }
        const auto fail = [&](const std::string& msg) -> Error {
            return MC_FAIL(idx + "('" + an + "'): " + msg);
        };
        switch (t.type) {
            case TimelineAction::Type::KeyDown:
            case TimelineAction::Type::KeyUp:
            case TimelineAction::Type::Hold: {
                const Json* k = a.find("key");
                if (!k || !k->isString()) {
                    return fail("'key' is required (a name)");
                }
                t.key = k->asString();
                {
                    auto kc = authorKeycode(t.key);
                    MC_TRY(kc);
                }
                if (t.type == TimelineAction::Type::Hold) {
                    const Json* uf = a.find("until_frame");
                    if (uf) {
                        long long uv = 0;
                        if (!isIntValue(*uf, uv)) {
                            return fail("'until_frame' must be an integer");
                        }
                        if (uv <= t.atFrame) {
                            return fail("'until_frame' must be > "
                                        "'at_frame' (" +
                                        std::to_string(t.atFrame) + ")");
                        }
                        t.untilFrame = static_cast<int>(uv);
                    }
                }
                break;
            }
            case TimelineAction::Type::MouseDown:
            case TimelineAction::Type::MouseUp: {
                const Json* b = a.find("button");
                if (!b || !b->isString()) {
                    return fail("'button' is required (left/right/middle)");
                }
                t.button = b->asString();
                MC_TRY(authorButton(t.button));
                break;
            }
            case TimelineAction::Type::MouseMove: {
                const Json* k = a.find("dx");
                if (!k || !isFiniteNumber(*k, t.dx)) {
                    return fail("'dx' is required (a finite number)");
                }
                // The optional absolute position (the file's mouse_move's
                // x/y — the panels' click coordinates; absent = 0, the
                // legacy delta-only shape's bit-identity).
                if (const Json* kx = a.find("x"); kx) {
                    if (!isFiniteNumber(*kx, t.x)) {
                        return fail("'x' must be a finite number");
                    }
                }
                if (const Json* ky = a.find("y"); ky) {
                    if (!isFiniteNumber(*ky, t.y)) {
                        return fail("'y' must be a finite number");
                    }
                }
                k = a.find("dy");
                if (!k || !isFiniteNumber(*k, t.dy)) {
                    return fail("'dy' is required (a finite number)");
                }
                break;
            }
            case TimelineAction::Type::Wheel: {
                const Json* k = a.find("ticks");
                if (!k || !isIntValue(*k, v)) {
                    return fail("'ticks' is required (an integer)");
                }
                t.ticks = static_cast<int>(v);
                break;
            }
        }
        out.push_back(t);
    }
    return out;
}

DemoFile authorDemoFile(const DemoWorldSpec& world,
                        const std::vector<TimelineAction>& actions) {
    DemoFile file;
    file.sim = kDemoSimVersion;
    file.recordedWith = kDemoFileVersion;
    file.world = world;
    // The frames: every action's events land inside [0, frames) — a
    // hold's release extends the demo to its until_frame (a held-to-
    // the-end hold keeps its at_frame + 1 bound, the key stays down at
    // the demo's end).
    int frames = 0;
    for (const TimelineAction& a : actions) {
        frames = std::max(frames, a.atFrame + 1);
        if (a.type == TimelineAction::Type::Hold && a.untilFrame >= 0) {
            frames = std::max(frames, a.untilFrame + 1);
        }
    }
    file.dts.assign(static_cast<std::size_t>(frames), kDemoAuthorDt);
    // The expansion (the event's file order = (frame, action index,
    // the hold's release = the same action's second half) — the
    // deterministic total order; the input's timeline order is kept
    // for the same-frame ties by the stable sort's action index).
    struct Ev {
        int frame = 0;
        int action = 0;
        int part = 0;
        InputEvent ev;
    };
    std::vector<Ev> evs;
    evs.reserve(actions.size() * 2);
    for (int i = 0; i < static_cast<int>(actions.size()); ++i) {
        const TimelineAction& a = actions[i];
        // The actions are parse-validated (authorKeycode / authorButton
        // succeeded in parseTimelineActions) — the re-reads below are
        // infallible here.
        std::uint32_t kc = 0;
        {
            auto r = authorKeycode(a.key);
            if (r.ok()) {
                kc = *r;
            }
        }
        std::int32_t mb = 0;
        {
            auto r = authorButton(a.button);
            if (r.ok()) {
                mb = *r;
            }
        }
        auto keyEv = [kc](bool down) {
            return InputEvent {down ? InputEvent::Kind::KeyDown
                                    : InputEvent::Kind::KeyUp,
                               kc, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, false};
        };
        auto mouseEv = [mb](InputEvent::Kind kind) {
            return InputEvent {kind, 0, mb, 0, 0.0f, 0.0f, 0.0f, 0.0f,
                               false};
        };
        switch (a.type) {
            case TimelineAction::Type::KeyDown:
                evs.push_back(Ev {a.atFrame, i, 0, keyEv(true)});
                break;
            case TimelineAction::Type::KeyUp:
                evs.push_back(Ev {a.atFrame, i, 0, keyEv(false)});
                break;
            case TimelineAction::Type::Hold:
                evs.push_back(Ev {a.atFrame, i, 0, keyEv(true)});
                if (a.untilFrame >= 0) {
                    evs.push_back(Ev {a.untilFrame, i, 1, keyEv(false)});
                }
                break;
            case TimelineAction::Type::MouseDown:
                evs.push_back(Ev {a.atFrame, i, 0,
                                  mouseEv(InputEvent::Kind::MouseDown)});
                break;
            case TimelineAction::Type::MouseUp:
                evs.push_back(Ev {a.atFrame, i, 0,
                                  mouseEv(InputEvent::Kind::MouseUp)});
                break;
            case TimelineAction::Type::MouseMove:
                // The absolute position (the panels' click coordinates)
                // rides the event's x/y (the file's mouse_move face —
                // 0.0f when the timeline omitted them, the legacy
                // delta-only shape).
                evs.push_back(Ev {a.atFrame, i, 0,
                                  InputEvent {InputEvent::Kind::MouseMove,
                                              0, 0, 0,
                                              static_cast<float>(a.dx),
                                              static_cast<float>(a.dy),
                                              static_cast<float>(a.x),
                                              static_cast<float>(a.y),
                                              false}});
                break;
            case TimelineAction::Type::Wheel:
                evs.push_back(Ev {a.atFrame, i, 0,
                                  InputEvent {InputEvent::Kind::Wheel, 0,
                                              a.ticks, 0, 0.0f, 0.0f, 0.0f,
                                              0.0f, false}});
                break;
        }
    }
    std::stable_sort(evs.begin(), evs.end(),
                     [](const Ev& x, const Ev& y) {
                         if (x.frame != y.frame) {
                             return x.frame < y.frame;
                         }
                         if (x.action != y.action) {
                             return x.action < y.action;
                         }
                         return x.part < y.part;
                     });
    for (const Ev& e : evs) {
        file.events.push_back(DemoEvent {e.frame, e.ev});
    }
    // The author's v1 has no gamepad actions (the record's face) —
    // the flag is false by the DemoFile default.
    return file;
}

Result<std::filesystem::path> authorDemoFromFile(
    const std::filesystem::path& timelinePath) {
    std::ifstream in(timelinePath, std::ios::binary);
    if (!in) {
        return MC_FAIL("cannot open timeline file: '" + timelinePath.string() +
                       "'");
    }
    std::string text((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
    auto js = Json::parse(text);
    MC_TRY(js);
    const Json& root = *js;
    if (!root.isObject()) {
        return MC_FAIL("timeline root must be an object");
    }
    for (const auto& p : root.members()) {
        const std::string& key = p.first;
        if (key != "world" && key != "actions") {
            return MC_FAIL("timeline: unexpected top-level key '" + key +
                           "' (world/actions)");
        }
    }
    const Json* world = root.find("world");
    if (!world) {
        return MC_FAIL("timeline is missing the 'world' key");
    }
    auto spec = parseWorldSpec(*world);
    MC_TRY(spec);
    const Json* actions = root.find("actions");
    if (!actions) {
        return MC_FAIL("timeline is missing the 'actions' key");
    }
    auto list = parseTimelineActions(*actions);
    MC_TRY(list);
    DemoFile file = authorDemoFile(*spec, *list);
    std::filesystem::path out = timelinePath;
    out.replace_extension(".dmo");
    MC_TRY(writeDemoFile(out, file));
    return out;
}

} // namespace mc
