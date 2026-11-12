#include "core/demo.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <set>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include "ui/font.hpp" // V3-20: the title's font-safety's face
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
constexpr const char* kEventTypeNames[9] = {
    "key_down",       "key_up",         "mouse_down",   "mouse_up",
    "mouse_move",     "wheel",          "gamepad_button", "gamepad_axis",
    "panel_click"};

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
    static const std::vector<std::string> kPanel = {"panel", "slot",
                                                    "button"};
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
        case InputEvent::Kind::PanelClick:
            return kPanel;
    }
    return kKey; // unreachable (the switch covers the closed Kind)
}

InputEvent::Kind kindForType(const std::string& type) {
    for (int i = 0; i < 9; ++i) {
        if (type == kEventTypeNames[i]) {
            return static_cast<InputEvent::Kind>(i);
        }
    }
    return InputEvent::Kind::KeyDown; // unreachable (the caller checks)
}

// The world spec's strict parse (the demo file's and the author
// timeline's shared face): exactly {type, seed} (fresh) /
// {type, slot} (save) / {type, name} (scene, V3-16a — the committed
// guide asset) — the double key / missing key / the invalid name
// charset are named.
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
    } else if (type->asString() == "scene") {
        spec.type = DemoWorldSpec::Type::Scene;
        if (world.contains("seed") || world.contains("slot")) {
            return MC_FAIL("demo world double key: a scene start takes "
                           "'name', not 'seed'/'slot'");
        }
        const Json* name = world.find("name");
        if (!name || !name->isString()) {
            return MC_FAIL("demo world: a scene start is missing 'name'");
        }
        if (!isValidSlotName(name->asString())) {
            return MC_FAIL("demo world: 'name' is not a valid scene name "
                           "(letters, digits, '_' or '-')");
        }
        spec.slot = name->asString();
        if (world.memberCount() != 2) {
            return MC_FAIL("demo world (scene) must have exactly the "
                           "keys 'type' and 'name'");
        }
    } else {
        return MC_FAIL("demo world: unknown type '" + type->asString() +
                       "' (fresh|save|scene)");
    }
    return spec;
}

} // namespace

std::string demoWorldDescription(const DemoWorldSpec& spec) {
    switch (spec.type) {
        case DemoWorldSpec::Type::Fresh:
            return "fresh seed " + std::to_string(spec.seed);
        case DemoWorldSpec::Type::Save:
            return "save slot '" + spec.slot + "'";
        case DemoWorldSpec::Type::Scene:
            return "scene '" + spec.slot + "' (the committed guide asset)";
    }
    return "fresh seed 0"; // unreachable (the switch's exhaustiveness)
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
                           "mouse_move/wheel/gamepad_button/gamepad_axis/"
                           "panel_click)");
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
            case InputEvent::Kind::PanelClick: {
                const Json* k = e.find("panel");
                if (!k || !isIntValue(*k, v) || v < 0 || v > 3) {
                    return MC_FAIL("demo event " + std::to_string(i) +
                                   " ('panel_click'): 'panel' must be an "
                                   "integer 0..3 (inventory/table/"
                                   "furnace/creative)");
                }
                ev.u32 = static_cast<std::uint32_t>(v);
                k = e.find("slot");
                if (!k || !isIntValue(*k, v) || v < -32768 || v > 32767) {
                    return MC_FAIL("demo event " + std::to_string(i) +
                                   " ('panel_click'): 'slot' must be a "
                                   "Sint16-range integer id");
                }
                ev.code = static_cast<std::int32_t>(v);
                k = e.find("button");
                if (!k || !isIntValue(*k, v) || v < 0 || v > 2) {
                    return MC_FAIL("demo event " + std::to_string(i) +
                                   " ('panel_click'): 'button' must be an "
                                   "integer 0..2 (left/right/shift)");
                }
                ev.value = static_cast<std::int16_t>(v);
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
    if (file.world.type == DemoWorldSpec::Type::Fresh) {
        world["type"] = Json(std::string("fresh"));
        world["seed"] = Json(static_cast<long long>(file.world.seed));
    } else if (file.world.type == DemoWorldSpec::Type::Save) {
        world["type"] = Json(std::string("save"));
        world["slot"] = Json(file.world.slot);
    } else {
        // V3-16a: the scene (the committed guide asset — the name's
        // key, the save's slot's twin).
        world["type"] = Json(std::string("scene"));
        world["name"] = Json(file.world.slot);
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
            case InputEvent::Kind::PanelClick:
                e["type"] = Json(std::string(kEventTypeNames[
                    static_cast<int>(ev.kind)]));
                e["panel"] = Json(static_cast<long long>(ev.u32));
                e["slot"] = Json(ev.code);
                e["button"] = Json(static_cast<long long>(ev.value));
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

std::string DemoPlayer::injectForFrame(int frame, Input& target,
                                       const PanelClickResolver& panelClick) const {
    assert(frame >= 0 && frame < frameCount());
    const int begin = m_frameBegin[frame];
    const int end = m_frameBegin[frame + 1];
    std::string err; // the frame's first named problem (the rest's
                     // events keep injecting — the demo's break's loud
                     // line, the sim's never-stopping face)
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
            case InputEvent::Kind::PanelClick:
                if (panelClick == nullptr) {
                    if (err.empty()) {
                        err = "the demo's panel_click event " +
                              std::to_string(i) + " has no resolver "
                              "(the replay's loop's face — a caller's bug)";
                    }
                    break;
                }
                const std::string pe =
                    panelClick(ev.u32, ev.code, ev.value);
                if (!pe.empty() && err.empty()) {
                    err = pe;
                }
                break;
        }
    }
    return err;
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
                                                  "key"}
;
    static const std::vector<std::string> kHold = {"at_frame", "action",
                                                   "key", "until_frame"};
    static const std::vector<std::string> kMouse = {"at_frame", "action",
                                                    "button"};
    static const std::vector<std::string> kMove = {"at_frame", "action",
                                                   "dx", "dy", "x", "y"};
    static const std::vector<std::string> kWheel = {"at_frame", "action",
                                                    "ticks"};
    static const std::vector<std::string> kPanelClick = {"at_frame",
                                                         "action", "panel",
                                                         "slot", "button"};
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
        case TimelineAction::Type::PanelClick:
            return kPanelClick;
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
        } else if (an == "panel_click") {
            t.type = TimelineAction::Type::PanelClick;
        } else if (an == "gamepad_button" || an == "gamepad_axis") {
            return MC_FAIL(idx + "gamepad actions are the record's face "
                           "(the author v1 is keyboard/mouse)");
        } else {
            return MC_FAIL(idx + "unknown action '" + an +
                           "' (the vocabulary: key_down key_up hold "
                           "mouse_down mouse_up mouse_move wheel "
                           "panel_click)");
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
            case TimelineAction::Type::PanelClick: {
                const Json* k = a.find("panel");
                if (!k || !k->isString()) {
                    return fail("'panel' is required "
                                "(inventory/table/furnace/creative)");
                }
                t.panel = k->asString();
                auto pr = authorPanel(t.panel);
                MC_TRY(pr);
                const Json* k2 = a.find("slot");
                if (!k2 || !k2->isString()) {
                    return fail("'slot' is required (the panel's name)");
                }
                t.pslot = k2->asString();
                auto sr = authorPanelSlot(*pr, t.pslot);
                MC_TRY(sr);
                const Json* k3 = a.find("button");
                if (!k3 || !k3->isString()) {
                    return fail("'button' is required (left/right/shift)");
                }
                t.button = k3->asString();
                MC_TRY(authorPanelButton(t.button));
                break;
            }
        }
        out.push_back(t);
    }
    return out;
}

// ---------------------------------------------------------------------------
// V3-16a: the panel-click's closed vocabularies (the author's names'
// faces — the geometry-free addressing's single source)
// ---------------------------------------------------------------------------

std::string demoPanelName(std::uint32_t panel) {
    switch (panel) {
        case kDemoPanelInventory:
            return "inventory";
        case kDemoPanelTable:
            return "table";
        case kDemoPanelFurnace:
            return "furnace";
        case kDemoPanelCreative:
            return "creative";
    }
    return "panel-" + std::to_string(panel);
}

std::string demoPanelSlotName(std::uint32_t panel, std::int32_t slot) {
    // The shared player inventory (every panel's face): h0..h8 (the
    // hotbar's 0..8), b0..b26 (the storage's 9..35).
    if (slot >= 0 && slot <= 35) {
        if (slot < 9) {
            return "h" + std::to_string(slot);
        }
        return "b" + std::to_string(slot - 9);
    }
    switch (panel) {
        case kDemoPanelInventory:
            if (slot >= 100 && slot <= 103) {
                return "g" + std::to_string(slot - 100);
            }
            if (slot == 110) {
                return "out";
            }
            break;
        case kDemoPanelTable:
            if (slot >= 200 && slot <= 208) {
                return "g" + std::to_string(slot - 200);
            }
            if (slot == 110) {
                return "out";
            }
            break;
        case kDemoPanelFurnace:
            if (slot == 120) {
                return "in";
            }
            if (slot == 121) {
                return "fuel";
            }
            if (slot == 122) {
                return "out";
            }
            break;
        case kDemoPanelCreative:
            if (slot >= 300 && slot <= 323) {
                return "c" + std::to_string(slot - 300);
            }
            break;
    }
    return "slot-" + std::to_string(slot);
}

Result<std::uint32_t> authorPanel(const std::string& name) {
    if (name == "inventory") {
        return kDemoPanelInventory;
    }
    if (name == "table") {
        return kDemoPanelTable;
    }
    if (name == "furnace") {
        return kDemoPanelFurnace;
    }
    if (name == "creative") {
        return kDemoPanelCreative;
    }
    return MC_FAIL("unknown panel '" + name +
                   "' (inventory/table/furnace/creative)");
}

namespace {

// A digits-only read (the h0..h8 / b0..b26 / c0..c23's names).
bool isDigits(const std::string& t) {
    if (t.empty()) {
        return false;
    }
    for (const char c : t) {
        if (c < '0' || c > '9') {
            return false;
        }
    }
    return true;
}

} // namespace

Result<std::int32_t> authorPanelSlot(std::uint32_t panel,
                                     const std::string& name) {
    // The shared player inventory's names (every panel).
    if (name.size() >= 2 && isDigits(name.substr(1))) {
        const int n = std::atoi(name.substr(1).c_str());
        if (name[0] == 'h' && n >= 0 && n <= 8) {
            return n;
        }
        if (name[0] == 'b' && n >= 0 && n <= 26) {
            return n + 9;
        }
    }
    if (panel == kDemoPanelInventory) {
        if (name.size() == 2 && name[0] == 'g' && name[1] >= '0' &&
            name[1] <= '3') {
            return 100 + (name[1] - '0');
        }
        if (name == "out") {
            return 110;
        }
        return MC_FAIL("unknown inventory slot '" + name +
                       "' (h0..h8 / b0..b26 / g0..g3 / out)");
    }
    if (panel == kDemoPanelTable) {
        if (name.size() == 2 && name[0] == 'g' && name[1] >= '0' &&
            name[1] <= '8') {
            return 200 + (name[1] - '0');
        }
        if (name == "out") {
            return 110;
        }
        return MC_FAIL("unknown table slot '" + name +
                       "' (h0..h8 / b0..b26 / g0..g8 / out)");
    }
    if (panel == kDemoPanelFurnace) {
        if (name == "in") {
            return 120;
        }
        if (name == "fuel") {
            return 121;
        }
        if (name == "out") {
            return 122;
        }
        return MC_FAIL("unknown furnace slot '" + name +
                       "' (h0..h8 / b0..b26 / in / fuel / out)");
    }
    // kDemoPanelCreative
    if (name.size() >= 2 && name[0] == 'c' && isDigits(name.substr(1))) {
        const int n = std::atoi(name.substr(1).c_str());
        if (n >= 0 && n <= 23) {
            return 300 + n;
        }
    }
    return MC_FAIL("unknown creative slot '" + name +
                   "' (h0..h8 / b0..b26 / c0..c23)");
}

Result<std::int16_t> authorPanelButton(const std::string& name) {
    if (name == "left") {
        return kDemoPanelButtonLeft;
    }
    if (name == "right") {
        return kDemoPanelButtonRight;
    }
    if (name == "shift") {
        return kDemoPanelButtonShift;
    }
    return MC_FAIL("unknown panel button '" + name +
                   "' (left/right/shift)");
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
            case TimelineAction::Type::PanelClick:
                // The parse's validation already succeeded (the author's
                // faces) — the re-reads are infallible here.
                evs.push_back(Ev {a.atFrame, i, 0,
                                  InputEvent {InputEvent::Kind::PanelClick,
                                              *authorPanel(a.panel),
                                              *authorPanelSlot(
                                                  *authorPanel(a.panel),
                                                  a.pslot),
                                              *authorPanelButton(a.button),
                                              0.0f, 0.0f, 0.0f, 0.0f,
                                              false}});
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

Result<DemoFile> parseScriptTimeline(const std::filesystem::path& path) {
    // The authorDemoFromFile's no-write twin (the V3-12b --script's
    // parse — the net session's deterministic input source, the demo
    // kernel's fourth consumer). The world key is optional (the net's
    // WELCOME is the world spec's authority).
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return MC_FAIL("cannot open script timeline: '" + path.string() +
                       "'");
    }
    std::string text((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
    auto js = Json::parse(text);
    MC_TRY(js);
    const Json& root = *js;
    if (!root.isObject()) {
        return MC_FAIL("script timeline root must be an object");
    }
    for (const auto& p : root.members()) {
        const std::string& key = p.first;
        if (key != "world" && key != "actions") {
            return MC_FAIL("script timeline: unexpected top-level key '" +
                           key + "' (world/actions)");
        }
    }
    const Json* world = root.find("world");
    DemoWorldSpec spec;
    if (world != nullptr) {
        auto specR = parseWorldSpec(*world);
        MC_TRY(specR);
        spec = *specR;
    } // absent: the default fresh-seed-0 (the net ignores it)
    const Json* actions = root.find("actions");
    if (actions == nullptr) {
        return MC_FAIL("script timeline is missing the 'actions' key");
    }
    auto list = parseTimelineActions(*actions);
    MC_TRY(list);
    // V3-16a: the net's --script has no panel_click (the solo's guide's
    // face — the net's FrameState's 40B carries no panel id, the
    // lockstep's input's documented bound; the coop's guide's later
    // face).
    for (const TimelineAction& a : *list) {
        if (a.type == TimelineAction::Type::PanelClick) {
            return MC_FAIL("script: the 'panel_click' is the solo's "
                           "guide's face (the net's lockstep's input "
                           "carries no panel id)");
        }
    }
    return authorDemoFile(spec, *list);
}

// ---------------------------------------------------------------------------
// V3-16a: the guide demo's index data layer (the 11-07 ruling's parse +
// the shipped content's init cross-check)
// ---------------------------------------------------------------------------

std::filesystem::path demoScenePath(const std::string& name) {
    return std::filesystem::path("assets/demos/scenes") / (name + ".json.gz");
}

std::filesystem::path demoSceneReadPath(
    const std::string& name, const std::filesystem::path& dir) {
    // V3-18: the two-stage find (the committed .json.gz first, the
    // legacy .json after — a checkout mid-rename still boots).
    return slotReadPath(dir, name);
}

namespace {

// A tag's charset face: [a-z0-9_]+ (the closed, the traversal guard's
// twin — the tags are data-layer only in v1, the render's face is
// the tutorial page's later).
bool isTagString(const std::string& t) {
    if (t.empty()) {
        return false;
    }
    for (const char c : t) {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
              c == '_')) {
            return false;
        }
    }
    return true;
}

// A file name's charset face: [a-z0-9_]+ with the .dmo's extension.
bool isDemoFileName(const std::string& f) {
    if (f.size() < 5 || f.compare(f.size() - 4, 4, ".dmo") != 0) {
        return false;
    }
    for (const char c : f) {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
              c == '_' || c == '.')) {
            return false;
        }
    }
    return true;
}

} // namespace

Result<DemoIndex> parseDemoIndex(const Json& root) {
    if (!root.isObject()) {
        return MC_FAIL("demo index root must be a JSON object");
    }
    for (const auto& p : root.members()) {
        if (p.first != "demos") {
            return MC_FAIL("demo index: unexpected top-level key '" +
                           p.first + "' (demos)");
        }
    }
    const Json* demos = root.find("demos");
    if (demos == nullptr) {
        return MC_FAIL("demo index is missing the 'demos' key");
    }
    if (!demos->isArray()) {
        return MC_FAIL("demo index: 'demos' must be an array");
    }
    DemoIndex index;
    std::map<std::string, std::set<int>> seqs; // category -> the seqs' set
    std::set<std::string> files;
    for (std::size_t i = 0; i < demos->size(); ++i) {
        const Json& e = demos->at(i);
        const std::string ctx = "demo index entry " + std::to_string(i);
        if (!e.isObject()) {
            return MC_FAIL(ctx + ": must be an object");
        }
        for (const auto& p : e.members()) {
            if (p.first != "category" && p.first != "seq" &&
                p.first != "mode" && p.first != "tags" &&
                p.first != "file" && p.first != "length_frames" &&
                p.first != "title") {
                return MC_FAIL(ctx + ": unexpected key '" + p.first +
                               "' (category/seq/mode/tags/file/"
                               "length_frames/title)");
            }
        }
        const Json* jc = e.find("category");
        if (jc == nullptr) {
            return MC_FAIL(ctx + " is missing the 'category' key");
        }
        if (!jc->isString()) {
            return MC_FAIL(ctx + ": 'category' must be a string");
        }
        const std::string& cat = jc->asString();
        bool catOk = false;
        for (const auto& c : kDemoCategories) {
            if (c.id == cat) {
                catOk = true;
                break;
            }
        }
        if (!catOk) {
            return MC_FAIL(ctx + ": unknown category '" + cat +
                           "' (day1/build/dimension/survival/coop)");
        }
        const Json* js = e.find("seq");
        if (js == nullptr) {
            return MC_FAIL(ctx + " is missing the 'seq' key");
        }
        long long seqv = 0;
        if (!isIntValue(*js, seqv) || seqv < 1) {
            return MC_FAIL(ctx + ": 'seq' must be an integer >= 1");
        }
        if (!seqs[cat].insert(static_cast<int>(seqv)).second) {
            return MC_FAIL(ctx + ": duplicate seq " +
                           std::to_string(seqv) + " in category '" +
                           cat + "'");
        }
        const Json* jm = e.find("mode");
        if (jm == nullptr) {
            return MC_FAIL(ctx + " is missing the 'mode' key");
        }
        if (!jm->isString() ||
            (jm->asString() != "solo" && jm->asString() != "coop")) {
            return MC_FAIL(ctx + ": 'mode' must be \"solo\" or \"coop\"");
        }
        const Json* jt = e.find("tags");
        if (jt == nullptr) {
            return MC_FAIL(ctx + " is missing the 'tags' key");
        }
        if (!jt->isArray()) {
            return MC_FAIL(ctx + ": 'tags' must be an array");
        }
        DemoIndexEntry en;
        en.category = cat;
        en.seq = static_cast<int>(seqv);
        en.mode = jm->asString();
        std::set<std::string> tagSeen;
        for (std::size_t k = 0; k < jt->size(); ++k) {
            const Json& t = jt->at(k);
            if (!t.isString()) {
                return MC_FAIL(ctx + ": tag #" + std::to_string(k) +
                               " must be a string");
            }
            const std::string& ts = t.asString();
            if (!isTagString(ts)) {
                return MC_FAIL(ctx + ": tag '" + ts +
                               "' must be a non-empty [a-z0-9_]+ name");
            }
            if (!tagSeen.insert(ts).second) {
                return MC_FAIL(ctx + ": duplicate tag '" + ts + "'");
            }
            en.tags.push_back(ts);
        }
        const Json* jf = e.find("file");
        if (jf == nullptr) {
            return MC_FAIL(ctx + " is missing the 'file' key");
        }
        if (!jf->isString()) {
            return MC_FAIL(ctx + ": 'file' must be a string");
        }
        const std::string& fn = jf->asString();
        if (!isDemoFileName(fn)) {
            return MC_FAIL(ctx + ": 'file' must be a [a-z0-9_]+ name "
                                 "with the .dmo extension, got '" + fn +
                           "'");
        }
        if (!files.insert(fn).second) {
            return MC_FAIL(ctx + ": duplicate file '" + fn + "'");
        }
        en.file = fn;
        // V3-20: the optional display's title (the menu's tier-2's +
        // the --list-demos's name face; absent = the file's name's
        // fallback). The FONT SAFETY's fail-fast (the 17d's 戏's
        // on-machine's '?' catch: a missing glyph would render the
        // menu's silent '?' — the init's named reject instead).
        const Json* jti = e.find("title");
        if (jti != nullptr) {
            if (!jti->isString()) {
                return MC_FAIL(ctx + ": 'title' must be a string");
            }
            en.title = jti->asString();
            if (en.title.empty()) {
                return MC_FAIL(ctx + ": 'title' must be non-empty");
            }
            std::size_t pos = 0;
            while (pos < en.title.size()) {
                const auto cp = utf8NextCodepoint(en.title, pos);
                if (!cp.has_value()) {
                    return MC_FAIL(ctx + ": 'title' has invalid UTF-8");
                }
                const uint32_t c = *cp;
                const bool ascii = c >= 0x20 && c <= 0x7E;
                const auto it = std::lower_bound(
                    std::begin(kCjkGlyphs), std::end(kCjkGlyphs), c,
                    [](const CjkGlyph& g, uint32_t v) { return g.cp < v; });
                const bool cjk = it != std::end(kCjkGlyphs) && it->cp == c;
                if (!ascii && !cjk) {
                    char hexbuf[16];
                    std::snprintf(hexbuf, sizeof(hexbuf), "U+%04X", c);
                    return MC_FAIL(ctx + ": 'title' has the font's "
                                   "missing glyph '" + std::string(hexbuf) +
                                   "' (the menu's '?' would render)");
                }
            }
        }
        const Json* jl = e.find("length_frames");
        if (jl == nullptr) {
            return MC_FAIL(ctx + " is missing the 'length_frames' key");
        }
        long long lv = 0;
        if (!isIntValue(*jl, lv) || lv < 1) {
            return MC_FAIL(ctx + ": 'length_frames' must be an integer >= 1");
        }
        en.lengthFrames = static_cast<int>(lv);
        index.demos.push_back(std::move(en));
    }
    // The within-category seq's CONTIGUITY (the learning order is 1..n
    // exactly — a gap is named, the menu's later).
    for (const auto& kv : seqs) {
        const std::size_t n = kv.second.size();
        for (std::size_t i = 1; i <= n; ++i) {
            if (!kv.second.count(static_cast<int>(i))) {
                return MC_FAIL("demo index: category '" + kv.first +
                               "' has a seq gap (the seqs must fill "
                               "1.." + std::to_string(n) + " exactly)");
            }
        }
    }
    return index;
}

Result<void> checkDemoIndexDir(const DemoIndex& index,
                               const std::filesystem::path& demosDir,
                               const std::filesystem::path& scenesDir) {
    std::error_code ec;
    for (std::size_t i = 0; i < index.demos.size(); ++i) {
        const DemoIndexEntry& e = index.demos[i];
        const std::string ctx = "demo index entry " + std::to_string(i) +
                                " (" + e.file + ")";
        const std::filesystem::path dpath = demosDir / e.file;
        if (!std::filesystem::exists(dpath, ec)) {
            return MC_FAIL(ctx + ": the file is not in '" +
                           demosDir.string() + "'");
        }
        // The full strict load (the container + the payload + the SIM
        // gate — a stale-sim guide's named reject, the canary's twin
        // at init).
        auto r = loadDemoFile(dpath);
        if (!r.ok()) {
            return MC_FAIL(ctx + ": " + r.error().message);
        }
        if (static_cast<int>(r->dts.size()) != e.lengthFrames) {
            return MC_FAIL(ctx + ": length_frames " +
                           std::to_string(e.lengthFrames) + " != the "
                           "file's " + std::to_string(r->dts.size()) +
                           " dts (re-pin the index)");
        }
        if (r->world.type == DemoWorldSpec::Type::Scene) {
            // V3-18: the two-stage scene find (.json.gz first, the
            // legacy .json after) — the committed asset's rename rides
            // the save family's discipline.
            if (demoSceneReadPath(r->world.slot, scenesDir).empty()) {
                return MC_FAIL(ctx + ": the scene '" + r->world.slot +
                               "' is not in '" + scenesDir.string() +
                               "'");
            }
        }
    }
    return {};
}

Result<DemoIndex> loadDemoIndexChecked() {
    const std::filesystem::path dir("assets/demos");
    const std::filesystem::path ipath = dir / "index.json";
    std::ifstream in(ipath, std::ios::binary);
    if (!in) {
        return MC_FAIL("cannot open the demo index: '" + ipath.string() +
                       "'");
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    auto js = Json::parse(ss.str());
    MC_TRY(js);
    auto index = parseDemoIndex(*js);
    MC_TRY(index);
    MC_TRY(checkDemoIndexDir(*index, dir, dir / "scenes"));
    return index;
}

} // namespace mc
