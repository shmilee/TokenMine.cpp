// V3-9 (v3.11.0): the demo replay / record / author — the test family.
//
// The demo file's strict schema (the reject family, all named), the
// injection kernel's pure faces (the frame dispatch's file order, the
// dt provider, the 0.5x hold signal, the speed cycle), the recorder's
// ZERO-SIDE-EFFECT pin (tapped and untapped Inputs run the same event
// stream bit-identically), the author's expansion (the hold / the 1/60
// dts / the (frame, action, part) order — the hand-written pins), the
// gzip container's identity (the V3-7 discipline: the pinned header,
// the round-trip byte-identity, the double-run identity), and the
// audience's routing (the two-Input structural pin + the injection's
// zero-leak count).
#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <SDL3/SDL.h>

#include "core/demo.hpp"
#include "core/input.hpp"
#include "core/json.hpp"
#include "core/save.hpp" // kGzipHeader / gzipEncode / gzipDecode

using namespace mc;

namespace {

std::string readAll(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    EXPECT_TRUE(in) << path;
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void writeAll(const std::string& path, const std::string& data) {
    std::ofstream out(path, std::ios::binary);
    EXPECT_TRUE(out) << path;
    out << data;
}

// The test's scratch directory (the OS temp — the temp .dmo's home;
// created fresh at startup, removed at exit).
const std::filesystem::path& scratchDir() {
    static const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "tm_v3110_demo_test";
    return dir;
}

struct ScratchCleanup {
    ScratchCleanup() {
        std::error_code ec;
        std::filesystem::remove_all(scratchDir(), ec);
        std::filesystem::create_directories(scratchDir(), ec);
    }
};
const ScratchCleanup kScratchCleanup;

// The shared fixture demo (fresh world, 4 frames, 6 events across the
// vocabulary — the round-trip / the player / the recorder's reference).
DemoFile fixtureFile() {
    DemoFile f;
    f.sim = kDemoSimVersion;
    f.recordedWith = kDemoFileVersion;
    f.world.type = DemoWorldSpec::Type::Fresh;
    f.world.seed = 123456789;
    f.dts = {0.016666666666666666, 0.01, 0.0, 0.25};
    f.events.push_back({0, InputEvent {InputEvent::Kind::KeyDown, SDLK_W,
                                       0, 0, 0.0f, 0.0f, 0.0f, 0.0f, false}});
    f.events.push_back({0, InputEvent {InputEvent::Kind::MouseDown, 0,
                                       SDL_BUTTON_LEFT, 0, 0.0f, 0.0f, 0.0f,
                                       0.0f, false}});
    f.events.push_back({1, InputEvent {InputEvent::Kind::MouseMove, 0, 0, 0,
                                       12.5f, -4.0f, 632.0f, 352.0f, false}});
    f.events.push_back({2, InputEvent {InputEvent::Kind::Wheel, 0, -1, 0,
                                       0.0f, 0.0f, 0.0f, 0.0f, false}});
    f.events.push_back({2, InputEvent {InputEvent::Kind::GamepadButton, 7,
                                       SDL_GAMEPAD_BUTTON_SOUTH, 0, 0.0f, 0.0f,
                                       0.0f, 0.0f, true}});
    f.events.push_back({3, InputEvent {InputEvent::Kind::GamepadAxis, 7,
                                       SDL_GAMEPAD_AXIS_LEFTX, 16384, 0.0f,
                                       0.0f, 0.0f, 0.0f, false}});
    f.hasGamepadEvents = true;
    return f;
}

// A temp .dmo (the load-level tests' container).
std::filesystem::path tempDemo(const DemoFile& f) {
    const std::filesystem::path p = scratchDir() / "tmp.dmo";
    auto w = writeDemoFile(p, f);
    EXPECT_TRUE(w.ok()) << w.error().describe();
    return p;
}

// The Input's state snapshot (the recorder's zero-side-effect pin's
// comparison — every key / button level + edge, the mouse delta, the
// wheel).
std::vector<double> inputSnapshot(const Input& in) {
    std::vector<double> s;
    for (int i = 0; i < kKeyCount; ++i) {
        s.push_back(in.isDown(static_cast<GameKey>(i)) ? 1.0 : 0.0);
        s.push_back(in.wasPressed(static_cast<GameKey>(i)) ? 1.0 : 0.0);
    }
    for (int i = 0; i < kMouseButtonCount; ++i) {
        s.push_back(in.isDown(static_cast<MouseButton>(i)) ? 1.0 : 0.0);
        s.push_back(in.wasPressed(static_cast<MouseButton>(i)) ? 1.0 : 0.0);
        s.push_back(in.wasReleased(static_cast<MouseButton>(i)) ? 1.0 : 0.0);
    }
    s.push_back(in.mouseDelta().x);
    s.push_back(in.mouseDelta().y);
    s.push_back(static_cast<double>(in.wheelDelta()));
    return s;
}

} // namespace

// ---------------------------------------------------------------------------
// The schema's strict parse
// ---------------------------------------------------------------------------

TEST(V3110Demo, ParseRoundTrip) {
    const DemoFile in = fixtureFile();
    auto out = parseDemoJson(Json::parse(serializeDemoJson(in)).value());
    ASSERT_TRUE(out.ok()) << out.error().describe();
    EXPECT_EQ(out->sim, in.sim);
    EXPECT_EQ(out->recordedWith, in.recordedWith);
    EXPECT_EQ(out->world.type, in.world.type);
    EXPECT_EQ(out->world.seed, in.world.seed);
    ASSERT_EQ(out->dts.size(), in.dts.size());
    for (std::size_t i = 0; i < in.dts.size(); ++i) {
        EXPECT_DOUBLE_EQ(out->dts[i], in.dts[i]);
    }
    ASSERT_EQ(out->events.size(), in.events.size());
    for (std::size_t i = 0; i < in.events.size(); ++i) {
        EXPECT_EQ(out->events[i].frame, in.events[i].frame);
        EXPECT_EQ(static_cast<int>(out->events[i].event.kind),
                  static_cast<int>(in.events[i].event.kind));
        EXPECT_EQ(out->events[i].event.u32, in.events[i].event.u32);
        EXPECT_EQ(out->events[i].event.code, in.events[i].event.code);
        EXPECT_EQ(out->events[i].event.value, in.events[i].event.value);
        EXPECT_FLOAT_EQ(out->events[i].event.dx, in.events[i].event.dx);
        EXPECT_FLOAT_EQ(out->events[i].event.dy, in.events[i].event.dy);
        EXPECT_FLOAT_EQ(out->events[i].event.ax, in.events[i].event.ax);
        EXPECT_FLOAT_EQ(out->events[i].event.ay, in.events[i].event.ay);
        EXPECT_EQ(out->events[i].event.down, in.events[i].event.down);
    }
    EXPECT_EQ(out->hasGamepadEvents, true);
}

TEST(V3110Demo, ParseRoundTripSaveWorld) {
    DemoFile f = fixtureFile();
    f.world.type = DemoWorldSpec::Type::Save;
    f.world.slot = "my-save-1";
    auto out = parseDemoJson(Json::parse(serializeDemoJson(f)).value());
    ASSERT_TRUE(out.ok()) << out.error().describe();
    EXPECT_EQ(out->world.type, DemoWorldSpec::Type::Save);
    EXPECT_EQ(out->world.slot, "my-save-1");
}

TEST(V3110Demo, ParseRejectMissingKeys) {
    // sim / world / dts / events — each missing is named (the
    // hand-built variants, the strict top level's reject face; the
    // first is the CURRENT schema, the rest the legacy `version`
    // form).
    const std::vector<std::pair<std::string, std::string>> variants = {
        {"sim", R"({"world": {"type": "fresh", "seed": 1}, )"
                   R"("dts": [0.01], "events": []})"},
        {"world", R"({"version": "v", "dts": [0.01], "events": []})"},
        {"dts", R"({"version": "v", "world": {"type": "fresh", "seed": 1}, )"
               R"("events": []})"},
        {"events", R"({"version": "v", "world": {"type": "fresh", "seed": 1}, )"
                   R"("dts": [0.01]})"},
    };
    for (const auto& [name, text] : variants) {
        auto r = parseDemoJson(Json::parse(text).value());
        ASSERT_FALSE(r.ok()) << "missing '" << name << "' accepted";
        EXPECT_NE(r.error().message.find("missing"), std::string::npos)
            << r.error().message;
    }
}

TEST(V3110Demo, ParseRejectUnexpectedTopKey) {
    auto r = parseDemoJson(
        Json::parse(R"({"version": "v", "world": {"type": "fresh", "seed": 1}, )"
                    R"("dts": [0.01], "events": [], "foo": 1})")
            .value());
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("unexpected top-level key 'foo'"),
              std::string::npos);
}

TEST(V3110Demo, ParseRejectUnknownEventType) {
    auto r = parseDemoJson(Json::parse(R"({"version": "v", )"
                                       R"("world": {"type": "fresh", "seed": 1}, )"
                                       R"("dts": [0.01], )"
                                       R"("events": [{"frame": 0, "type": "fly"}]})")
                              .value());
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("unknown type 'fly'"),
              std::string::npos);
}

TEST(V3110Demo, ParseRejectOutOfOrderFrame) {
    auto r = parseDemoJson(Json::parse(R"({"version": "v", )"
                                       R"("world": {"type": "fresh", "seed": 1}, )"
                                       R"("dts": [0.01, 0.01, 0.01], )"
                                       R"("events": [{"frame": 1, "type": "wheel", "ticks": 1}, )"
                                       R"({"frame": 0, "type": "wheel", "ticks": 1}]})")
                              .value());
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("out of order"), std::string::npos);
}

TEST(V3110Demo, ParseRejectDanglingFrame) {
    auto r = parseDemoJson(Json::parse(R"({"version": "v", )"
                                       R"("world": {"type": "fresh", "seed": 1}, )"
                                       R"("dts": [0.01], )"
                                       R"("events": [{"frame": 1, "type": "wheel", "ticks": 1}]})")
                              .value());
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("out of range"), std::string::npos);
}

TEST(V3110Demo, ParseRejectNegativeDt) {
    auto r = parseDemoJson(Json::parse(R"({"version": "v", )"
                                       R"("world": {"type": "fresh", "seed": 1}, )"
                                       R"("dts": [-0.001], "events": []})")
                              .value());
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("negative"), std::string::npos);
}

TEST(V3110Demo, ParseRejectDtTooLarge) {
    auto r = parseDemoJson(Json::parse(R"({"version": "v", )"
                                       R"("world": {"type": "fresh", "seed": 1}, )"
                                       R"("dts": [0.2500001], "events": []})")
                              .value());
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("exceeds kDemoMaxDt"), std::string::npos);
}

TEST(V3110Demo, ParseAcceptDtBounds) {
    // 0.0 (the record's zero-cost frame) and exactly 0.25 (the loop's
    // clamp value) are INSIDE the strict window.
    auto ok0 = parseDemoJson(Json::parse(R"({"version": "v", )"
                                         R"("world": {"type": "fresh", "seed": 1}, )"
                                         R"("dts": [0.0], "events": []})")
                                  .value());
    EXPECT_TRUE(ok0.ok());
    auto okMax =
        parseDemoJson(Json::parse(R"({"version": "v", )"
                                  R"("world": {"type": "fresh", "seed": 1}, )"
                                  R"("dts": [0.25], "events": []})")
                          .value());
    EXPECT_TRUE(okMax.ok());
    EXPECT_DOUBLE_EQ(okMax->dts[0], kDemoMaxDt);
}

TEST(V3110Demo, ParseRejectWorldDoubleKey) {
    auto freshSlot = parseDemoJson(Json::parse(R"({"version": "v", )"
                                               R"("world": {"type": "fresh", "seed": 1, "slot": "x"}, )"
                                               R"("dts": [0.01], "events": []})")
                                        .value());
    ASSERT_FALSE(freshSlot.ok());
    EXPECT_NE(freshSlot.error().message.find("double key"),
              std::string::npos);
    auto saveSeed = parseDemoJson(Json::parse(R"({"version": "v", )"
                                              R"("world": {"type": "save", "slot": "x", "seed": 1}, )"
                                              R"("dts": [0.01], "events": []})")
                                       .value());
    ASSERT_FALSE(saveSeed.ok());
    EXPECT_NE(saveSeed.error().message.find("double key"),
              std::string::npos);
}

TEST(V3110Demo, ParseRejectWorldMissing) {
    auto noType = parseDemoJson(Json::parse(R"({"version": "v", )"
                                            R"("world": {"seed": 1}, )"
                                            R"("dts": [0.01], "events": []})")
                                    .value());
    ASSERT_FALSE(noType.ok());
    EXPECT_NE(noType.error().message.find("'type'"), std::string::npos);
    auto noSeed = parseDemoJson(Json::parse(R"({"version": "v", )"
                                            R"("world": {"type": "fresh"}, )"
                                            R"("dts": [0.01], "events": []})")
                                    .value());
    ASSERT_FALSE(noSeed.ok());
    EXPECT_NE(noSeed.error().message.find("'seed'"), std::string::npos);
    auto noSlot = parseDemoJson(Json::parse(R"({"version": "v", )"
                                            R"("world": {"type": "save"}, )"
                                            R"("dts": [0.01], "events": []})")
                                    .value());
    ASSERT_FALSE(noSlot.ok());
    EXPECT_NE(noSlot.error().message.find("'slot'"), std::string::npos);
    auto badType = parseDemoJson(Json::parse(R"({"version": "v", )"
                                             R"("world": {"type": "void", "seed": 1}, )"
                                             R"("dts": [0.01], "events": []})")
                                        .value());
    ASSERT_FALSE(badType.ok());
    EXPECT_NE(badType.error().message.find("unknown type 'void'"),
              std::string::npos);
}

TEST(V3110Demo, ParseRejectBadSlotCharset) {
    auto r = parseDemoJson(Json::parse(R"({"version": "v", )"
                                       R"("world": {"type": "save", "slot": "a/b"}, )"
                                       R"("dts": [0.01], "events": []})")
                              .value());
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("not a valid save slot name"),
              std::string::npos);
}

TEST(V3110Demo, ParseRejectEventFaces) {
    // The per-event strict faces: missing frame / missing type / the
    // unexpected key / the negative frame / the missing payload key /
    // the gamepad payload's type + range.
    const char* base = R"({"version": "v", )"
                       R"("world": {"type": "fresh", "seed": 1}, )"
                       R"("dts": [0.01, 0.01], )";
    auto noFrame = parseDemoJson(Json::parse(std::string(base) +
                                             R"("events": [{"type": "wheel", "ticks": 1}]})")
                                     .value());
    ASSERT_FALSE(noFrame.ok());
    EXPECT_NE(noFrame.error().message.find("'frame'"), std::string::npos);
    auto noType = parseDemoJson(Json::parse(std::string(base) +
                                            R"("events": [{"frame": 0, "ticks": 1}]})")
                                   .value());
    ASSERT_FALSE(noType.ok());
    EXPECT_NE(noType.error().message.find("'type'"), std::string::npos);
    auto unexpected = parseDemoJson(Json::parse(std::string(base) +
                                                R"("events": [{"frame": 0, "type": "key_down", "sdl_key": 119, "ticks": 1}]})")
                                       .value());
    ASSERT_FALSE(unexpected.ok());
    EXPECT_NE(unexpected.error().message.find("unexpected key 'ticks'"),
              std::string::npos);
    auto negativeFrame =
        parseDemoJson(Json::parse(std::string(base) +
                                  R"("events": [{"frame": -1, "type": "wheel", "ticks": 1}]})")
                           .value());
    ASSERT_FALSE(negativeFrame.ok());
    auto missingSdlKey = parseDemoJson(Json::parse(std::string(base) +
                                                   R"("events": [{"frame": 0, "type": "key_down"}]})")
                              .value());
    ASSERT_FALSE(missingSdlKey.ok());
    EXPECT_NE(missingSdlKey.error().message.find("'sdl_key'"),
              std::string::npos);
    auto axisRange = parseDemoJson(Json::parse(std::string(base) +
                                               R"("events": [{"frame": 0, "type": "gamepad_axis", "which": 1, "axis": 0, "value": 32768}]})")
                                       .value());
    ASSERT_FALSE(axisRange.ok());
    EXPECT_NE(axisRange.error().message.find("Sint16"), std::string::npos);
    auto gpadDownNotBool =
        parseDemoJson(Json::parse(std::string(base) +
                                  R"("events": [{"frame": 0, "type": "gamepad_button", "which": 1, "button": 0, "down": 1}]})")
                           .value());
    ASSERT_FALSE(gpadDownNotBool.ok());
    EXPECT_NE(gpadDownNotBool.error().message.find("boolean"),
              std::string::npos);
}

// ---------------------------------------------------------------------------
// The load-level family (the container + the sim version gate)
// ---------------------------------------------------------------------------

TEST(V3110Demo, LoadSimMismatch) {
    // v3.13.0: the gate is the SIM version, not the build string —
    // a demo recorded at a different simulation-semantics version is
    // rejected by name (the recorded build is informational in the
    // message).
    DemoFile f = fixtureFile();
    f.sim = kDemoSimVersion + 1;
    f.recordedWith = "3.0.0";
    const auto p = tempDemo(f);
    auto r = loadDemoFile(p);
    ASSERT_FALSE(r.ok());
    const std::string expectGate = "demo sim " +
                                   std::to_string(kDemoSimVersion + 1) +
                                   " != this build sim " +
                                   std::to_string(kDemoSimVersion);
    EXPECT_NE(r.error().message.find(expectGate), std::string::npos);
    EXPECT_NE(r.error().message.find("(recorded with 3.0.0)"),
              std::string::npos);
    EXPECT_NE(r.error().message.find("simulation semantics differ"),
              std::string::npos);
    // The --demo-force escape (bypass the gate only): the file loads,
    // the payload untouched.
    auto forced = loadDemoFile(p, true);
    ASSERT_TRUE(forced.ok()) << forced.error().describe();
    EXPECT_EQ(forced->sim, kDemoSimVersion + 1);
    EXPECT_EQ(forced->recordedWith, "3.0.0");
}

// ---------------------------------------------------------------------------
// v3.13.0: the SIM version family (the dual schema + the legacy
// mapping + the mixed-schema reject)
// ---------------------------------------------------------------------------

TEST(V3110Demo, SimPinnedToTwo) {
    // The demo family's simulation-semantics version: 1 = the V3-9..
    // V3-13 era, 2 = the V3-8b gamepad input-semantics change (B/EAST
    // sprint -> the dedicated flight toggle, L3 -> crouch, the RT
    // trigger -> sprint — a recorded gamepad event replays through the
    // CURRENT interpretation, hence the first real bump).
    EXPECT_EQ(kDemoSimVersion, 2);
}

TEST(V3110Demo, ParseLegacyMapping) {
    // The legacy (pre-v3.13.0) {version, ...} file maps to sim 1 +
    // recorded_with := version (zero migration — every recorded demo
    // stays replayable).
    auto r = parseDemoJson(Json::parse(R"({"version": "3.11.0", )"
                                       R"("world": {"type": "fresh", "seed": 1}, )"
                                       R"("dts": [0.01], "events": []})")
                               .value());
    ASSERT_TRUE(r.ok()) << r.error().describe();
    EXPECT_EQ(r->sim, 1);
    EXPECT_EQ(r->recordedWith, "3.11.0");
}

TEST(V3110Demo, ParseCurrentSchema) {
    // The current {sim, recorded_with, ...} file: the fields land
    // verbatim (a future sim 2 parses fine — the GATE is the load's
    // job, not the parse's).
    auto r = parseDemoJson(Json::parse(R"({"sim": 2, "recorded_with": "9.9.9", )"
                                       R"("world": {"type": "fresh", "seed": 1}, )"
                                       R"("dts": [0.01], "events": []})")
                               .value());
    ASSERT_TRUE(r.ok()) << r.error().describe();
    EXPECT_EQ(r->sim, 2);
    EXPECT_EQ(r->recordedWith, "9.9.9");
}

TEST(V3110Demo, ParseRejectMixedSchemas) {
    // `version` (legacy) AND `sim`/`recorded_with` (current) = the
    // named mixed-schema reject.
    auto vsim = parseDemoJson(Json::parse(R"({"version": "3.11.0", "sim": 1, )"
                                          R"("world": {"type": "fresh", "seed": 1}, )"
                                          R"("dts": [0.01], "events": []})")
                                            .value());
    ASSERT_FALSE(vsim.ok());
    EXPECT_NE(vsim.error().message.find("mixed schemas"), std::string::npos);
    auto vrec = parseDemoJson(
        Json::parse(R"({"version": "3.11.0", "recorded_with": "3.11.0", )"
                    R"("world": {"type": "fresh", "seed": 1}, )"
                    R"("dts": [0.01], "events": []})")
            .value());
    ASSERT_FALSE(vrec.ok());
    EXPECT_NE(vrec.error().message.find("mixed schemas"), std::string::npos);
}

TEST(V3110Demo, ParseRejectCurrentSchemaFieldShapes) {
    // The current schema's field shapes: sim = an integer >= 1,
    // recorded_with = a non-empty string (each reject named).
    const std::vector<std::array<std::string, 3>> variants = {
        {"sim: string",
         R"({"sim": "1", "recorded_with": "v", )"
         R"("world": {"type": "fresh", "seed": 1}, )"
         R"("dts": [0.01], "events": []})",
         "must be an integer >= 1"},
        {"sim: zero",
         R"({"sim": 0, "recorded_with": "v", )"
         R"("world": {"type": "fresh", "seed": 1}, )"
         R"("dts": [0.01], "events": []})",
         "must be an integer >= 1"},
        {"sim: negative",
         R"({"sim": -1, "recorded_with": "v", )"
         R"("world": {"type": "fresh", "seed": 1}, )"
         R"("dts": [0.01], "events": []})",
         "must be an integer >= 1"},
        {"recorded_with: missing",
         R"({"sim": 1, )"
         R"("world": {"type": "fresh", "seed": 1}, )"
         R"("dts": [0.01], "events": []})",
         "missing the 'recorded_with' key"},
        {"recorded_with: empty",
         R"({"sim": 1, "recorded_with": "", )"
         R"("world": {"type": "fresh", "seed": 1}, )"
         R"("dts": [0.01], "events": []})",
         "missing the 'recorded_with' key"},
    };
    for (const auto& v : variants) {
        auto r = parseDemoJson(Json::parse(v[1]).value());
        ASSERT_FALSE(r.ok()) << v[0];
        EXPECT_NE(r.error().message.find(v[2]), std::string::npos)
            << v[0] << ": " << r.error().message;
    }
}

TEST(V3110Demo, LoadLegacyFileSimMismatch) {
    // The load face of the legacy mapping AFTER the first sim bump
    // (v3.14.0, V3-8b): a hand-written pre-v3.13.0 container (the
    // `version`-only payload, the gzip demo family) maps to sim 1 —
    // the gate now names the mismatch (sim 1 != build sim 2) and the
    // --demo-force escape loads it (payload untouched).
    const std::string body =
        R"({"version": "3.11.0", "world": {"type": "fresh", "seed": 1}, )"
        R"("dts": [0.01], "events": []})";
    const auto p = scratchDir() / "legacy.dmo";
    writeAll(p.string(), gzipEncode(body).value());
    auto r = loadDemoFile(p);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("demo sim 1 != this build sim " +
                                     std::to_string(kDemoSimVersion)),
              std::string::npos);
    auto forced = loadDemoFile(p, true);
    ASSERT_TRUE(forced.ok()) << forced.error().describe();
    EXPECT_EQ(forced->sim, 1);
    EXPECT_EQ(forced->recordedWith, "3.11.0");
}

TEST(V3110Demo, LoadMissingFile) {
    auto r = loadDemoFile(scratchDir() / "nope.dmo");
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("cannot open demo file"),
              std::string::npos);
}

TEST(V3110Demo, LoadNotGzip) {
    const auto p = scratchDir() / "plain.dmo";
    writeAll(p.string(), "this is not a demo");
    auto r = loadDemoFile(p);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("not a gzip demo"), std::string::npos);
}

TEST(V3110Demo, LoadCorruptContainer) {
    // The header's pinned bytes (a system-gzip file's MTIME drift = the
    // named reject), the truncation, the trailer's CRC.
    DemoFile f = fixtureFile();
    const auto body = serializeDemoJson(f);
    const auto container = gzipEncode(body).value();
    {
        std::string bad = container;
        bad[4] = 0x5A; // the MTIME's first byte — the pinned-header reject
        const auto p = scratchDir() / "hdrcorrupt.dmo";
        writeAll(p.string(), bad);
        auto r = loadDemoFile(p);
        ASSERT_FALSE(r.ok());
        EXPECT_NE(r.error().message.find("unsupported gzip header"),
                  std::string::npos);
    }
    {
        const auto p = scratchDir() / "truncated.dmo";
        writeAll(p.string(), container.substr(0, 5));
        auto r = loadDemoFile(p);
        ASSERT_FALSE(r.ok());
        EXPECT_NE(r.error().message.find("truncated"), std::string::npos);
    }
    {
        std::string bad = container;
        bad.back() = static_cast<char>(bad.back() ^ 0xFF); // the ISIZE
        const auto p = scratchDir() / "crcbad.dmo";
        writeAll(p.string(), bad);
        auto r = loadDemoFile(p);
        ASSERT_FALSE(r.ok());
        EXPECT_NE(r.error().message.find("corrupt gzip trailer"),
                  std::string::npos);
    }
}

// ---------------------------------------------------------------------------
// The gzip container's identity (the V3-7 discipline, the demo face)
// ---------------------------------------------------------------------------

TEST(V3110Demo, ContainerHeaderPinned) {
    const auto p = tempDemo(fixtureFile());
    const std::string raw = readAll(p.string());
    ASSERT_GE(raw.size(), kGzipHeader.size());
    for (std::size_t i = 0; i < kGzipHeader.size(); ++i) {
        EXPECT_EQ(static_cast<std::uint8_t>(raw[i]), kGzipHeader[i])
            << "header byte " << i;
    }
}

TEST(V3110Demo, ContainerRoundTripByteIdentity) {
    const auto p = tempDemo(fixtureFile());
    const std::string raw = readAll(p.string());
    const std::string payload = gzipDecode(raw, p.string()).value();
    // The decoder's inverse re-encodes to the SAME bytes (the encoder's
    // determinism — a second run of the same session is byte-identical).
    EXPECT_EQ(gzipEncode(payload).value(), raw);
}

TEST(V3110Demo, ContainerDoubleRunIdentity) {
    const DemoFile f = fixtureFile();
    const auto a = scratchDir() / "a.dmo";
    const auto b = scratchDir() / "b.dmo";
    ASSERT_TRUE(writeDemoFile(a, f).ok());
    ASSERT_TRUE(writeDemoFile(b, f).ok());
    EXPECT_EQ(readAll(a.string()), readAll(b.string()));
}

// ---------------------------------------------------------------------------
// DemoPlayer (the injection kernel)
// ---------------------------------------------------------------------------

TEST(V3110Demo, PlayerInjectFileOrder) {
    const DemoFile f = fixtureFile();
    const DemoPlayer player(f);
    EXPECT_EQ(player.frameCount(), 4);
    EXPECT_TRUE(player.hasGamepadEvents());
    Input in;
    in.handleGamepadAdded(7); // the fixture's gamepad events' device
    // A tap counter (the zero-leak pin — every injected event reaches
    // the handler's closure).
    int tapped = 0;
    in.setEventTap([&tapped](const InputEvent&) { ++tapped; });
    in.frameBegin();
    player.injectForFrame(0, in);
    EXPECT_EQ(tapped, 2);
    EXPECT_TRUE(in.isDown(GameKey::W));
    EXPECT_TRUE(in.wasPressed(GameKey::W));
    EXPECT_TRUE(in.isDown(MouseButton::Left));
    EXPECT_TRUE(in.wasPressed(MouseButton::Left));
    in.frameBegin();
    player.injectForFrame(1, in);
    EXPECT_EQ(tapped, 3);
    EXPECT_FLOAT_EQ(in.mouseDelta().x, 12.5f);
    EXPECT_FLOAT_EQ(in.mouseDelta().y, -4.0f);
    in.frameBegin();
    player.injectForFrame(2, in);
    EXPECT_EQ(tapped, 5);
    EXPECT_EQ(in.wheelDelta(), -1);
    EXPECT_TRUE(in.isDown(GameKey::Space)); // the SOUTH's mapped key
    in.frameBegin();
    player.injectForFrame(3, in);
    EXPECT_EQ(tapped, 6);
    // A frame without events = zero calls (the structural no-op).
    const DemoFile empty;
    const DemoPlayer p0(empty);
    EXPECT_EQ(p0.frameCount(), 0);
}

TEST(V3110Demo, PlayerInjectOrderWithinFrame) {
    // Same-frame events land in FILE order (the recorder's capture
    // order — the simulation's faithfulness): the W's down AFTER the
    // E's down keeps both edges, in that order.
    DemoFile f;
    f.sim = kDemoSimVersion;
    f.recordedWith = kDemoFileVersion;
    f.world.type = DemoWorldSpec::Type::Fresh;
    f.world.seed = 1;
    f.dts = {0.01};
    f.events.push_back({0, InputEvent {InputEvent::Kind::KeyDown, SDLK_E,
                                       0, 0, 0.0f, 0.0f, 0.0f, 0.0f, false}});
    f.events.push_back({0, InputEvent {InputEvent::Kind::KeyDown, SDLK_W,
                                       0, 0, 0.0f, 0.0f, 0.0f, 0.0f, false}});
    const DemoPlayer player(f);
    std::vector<GameKey> order;
    Input in;
    in.setEventTap([&order](const InputEvent& ev) {
        if (ev.kind == InputEvent::Kind::KeyDown) {
            order.push_back(ev.u32 == SDLK_E ? GameKey::E : GameKey::W);
        }
    });
    in.frameBegin();
    player.injectForFrame(0, in);
    ASSERT_EQ(order.size(), 2);
    EXPECT_EQ(order[0], GameKey::E);
    EXPECT_EQ(order[1], GameKey::W);
    EXPECT_TRUE(in.isDown(GameKey::E));
    EXPECT_TRUE(in.isDown(GameKey::W));
}

TEST(V3110Demo, PlayerDtForFrame) {
    const DemoFile f = fixtureFile();
    const DemoPlayer player(f);
    EXPECT_DOUBLE_EQ(player.dtForFrame(0), 0.016666666666666666);
    EXPECT_DOUBLE_EQ(player.dtForFrame(1), 0.01);
    EXPECT_DOUBLE_EQ(player.dtForFrame(2), 0.0);
    EXPECT_DOUBLE_EQ(player.dtForFrame(3), 0.25);
}

TEST(V3110Demo, PlayerRendersPerFrame) {
    // 0.5x = the frame hold (the second render is render-only); 1x /
    // 2x = the 1:1 render (the 2x's interval is the pacing's halved
    // wall distance, not a second update).
    EXPECT_EQ(DemoPlayer::rendersPerFrame(kDemoSpeedMin), 2);
    EXPECT_EQ(DemoPlayer::rendersPerFrame(kDemoSpeedDefault), 1);
    EXPECT_EQ(DemoPlayer::rendersPerFrame(kDemoSpeedMax), 1);
}

TEST(V3110Demo, PlayerNextSpeedCycle) {
    EXPECT_DOUBLE_EQ(DemoPlayer::nextSpeed(kDemoSpeedMin),
                     kDemoSpeedDefault);
    EXPECT_DOUBLE_EQ(DemoPlayer::nextSpeed(kDemoSpeedDefault),
                     kDemoSpeedMax);
    EXPECT_DOUBLE_EQ(DemoPlayer::nextSpeed(kDemoSpeedMax), kDemoSpeedMin);
    // The off-ladder's forward snap (defensive — the game holds the
    // rungs).
    EXPECT_DOUBLE_EQ(DemoPlayer::nextSpeed(0.25), kDemoSpeedMin);
    EXPECT_DOUBLE_EQ(DemoPlayer::nextSpeed(0.75), kDemoSpeedDefault);
    EXPECT_DOUBLE_EQ(DemoPlayer::nextSpeed(1.5), kDemoSpeedMax);
    EXPECT_DOUBLE_EQ(DemoPlayer::nextSpeed(3.0), kDemoSpeedMin);
}

// ---------------------------------------------------------------------------
// DemoRecorder (the boundary tap)
// ---------------------------------------------------------------------------

TEST(V3110Demo, RecorderZeroSideEffect) {
    // The pin: the tapped Input (the record) and the untapped Input
    // (the normal run) run the SAME event stream — the states are
    // bit-identical after EVERY event (the record first, then inject:
    // the observer touches nothing).
    Input tapped, plain;
    DemoRecorder rec;
    tapped.setEventTap([&rec](const InputEvent& ev) {
        rec.tapEvent(ev, 0);
    });
    std::vector<std::function<void(Input&)>> stream = {
        [](Input& in) { in.handleKey(SDLK_W, true); },
        [](Input& in) { in.handleMouseMotion(1.5f, -2.0f, 100.0f, 50.0f); },
        [](Input& in) { in.handleMouseButton(SDL_BUTTON_LEFT, true); },
        [](Input& in) { in.handleMouseWheel(2); },
        [](Input& in) { in.handleKey(SDLK_W, false); },
        [](Input& in) { in.handleMouseButton(SDL_BUTTON_LEFT, false); },
        [](Input& in) { in.handleKey(SDLK_SPACE, true); },
    };
    tapped.handleGamepadAdded(3);
    plain.handleGamepadAdded(3);
    stream.push_back([](Input& in) {
        in.handleGamepadButton(SDL_GAMEPAD_BUTTON_SOUTH, true);
    });
    stream.push_back([](Input& in) {
        in.handleGamepadAxis(SDL_GAMEPAD_AXIS_LEFTX, 16384);
    });
    stream.push_back([](Input& in) {
        in.handleGamepadButton(SDL_GAMEPAD_BUTTON_SOUTH, false);
    });
    for (const auto& step : stream) {
        step(tapped);
        step(plain);
        EXPECT_EQ(inputSnapshot(tapped), inputSnapshot(plain));
    }
    // The record's contents (the raw arguments, the frame 0's).
    ASSERT_EQ(rec.file().events.size(), stream.size());
    EXPECT_EQ(rec.file().events[0].event.kind, InputEvent::Kind::KeyDown);
    EXPECT_EQ(rec.file().events[0].event.u32, static_cast<std::uint32_t>(SDLK_W));
    EXPECT_EQ(rec.file().events[7].event.kind, InputEvent::Kind::GamepadButton);
    EXPECT_EQ(rec.file().events[7].event.u32, 3u);
    EXPECT_EQ(rec.file().events[7].event.down, true);
    EXPECT_EQ(rec.file().events[8].event.kind, InputEvent::Kind::GamepadAxis);
    EXPECT_EQ(rec.file().events[8].event.value, 16384);
    EXPECT_TRUE(rec.file().hasGamepadEvents);
}

TEST(V3110Demo, RecorderFrameAttribution) {
    DemoRecorder rec;
    rec.tapEvent(InputEvent {InputEvent::Kind::KeyDown, SDLK_A, 0, 0, 0.0f,
                             0.0f, 0.0f, 0.0f, false},
                 0);
    rec.frame(0.01);
    rec.tapEvent(InputEvent {InputEvent::Kind::Wheel, 0, 1, 0, 0.0f, 0.0f,
                             0.0f, 0.0f, false},
                 1);
    rec.frame(0.02);
    ASSERT_EQ(rec.file().events.size(), 2);
    EXPECT_EQ(rec.file().events[0].frame, 0);
    EXPECT_EQ(rec.file().events[1].frame, 1);
    EXPECT_EQ(rec.frameCount(), 2);
    // The event's frame < dts.size() (the parser's invariant).
    EXPECT_LT(rec.file().events[1].frame, rec.frameCount());
}

TEST(V3110Demo, RecorderDiscardPendingFrame) {
    DemoRecorder rec;
    rec.tapEvent(InputEvent {InputEvent::Kind::KeyDown, SDLK_A, 0, 0, 0.0f,
                             0.0f, 0.0f, 0.0f, false},
                 0);
    rec.frame(0.01);
    rec.tapEvent(InputEvent {InputEvent::Kind::KeyUp, SDLK_A, 0, 0, 0.0f,
                             0.0f, 0.0f, 0.0f, false},
                 1);
    // Frame 1 was never simulated (the quit's pump ran) — the tail is
    // excluded; the simulated prefix stays.
    rec.discardPendingFrame();
    ASSERT_EQ(rec.file().events.size(), 1);
    EXPECT_EQ(rec.file().events[0].frame, 0);
    EXPECT_EQ(rec.frameCount(), 1);
    // The no-tail case (the frame-limit's exit) is a no-op.
    rec.discardPendingFrame();
    EXPECT_EQ(rec.file().events.size(), 1);
}

// ---------------------------------------------------------------------------
// The author (the timeline expansion)
// ---------------------------------------------------------------------------

TEST(V3110Demo, AuthorKeycodeTable) {
    EXPECT_EQ(*authorKeycode("w"), static_cast<std::uint32_t>(SDLK_W));
    EXPECT_EQ(*authorKeycode("a"), static_cast<std::uint32_t>(SDLK_A));
    EXPECT_EQ(*authorKeycode("s"), static_cast<std::uint32_t>(SDLK_S));
    EXPECT_EQ(*authorKeycode("d"), static_cast<std::uint32_t>(SDLK_D));
    EXPECT_EQ(*authorKeycode("q"), static_cast<std::uint32_t>(SDLK_Q));
    EXPECT_EQ(*authorKeycode("e"), static_cast<std::uint32_t>(SDLK_E));
    EXPECT_EQ(*authorKeycode("r"), static_cast<std::uint32_t>(SDLK_R));
    EXPECT_EQ(*authorKeycode("c"), static_cast<std::uint32_t>(SDLK_C));
    EXPECT_EQ(*authorKeycode("t"), static_cast<std::uint32_t>(SDLK_T));
    EXPECT_EQ(*authorKeycode("space"), static_cast<std::uint32_t>(SDLK_SPACE));
    EXPECT_EQ(*authorKeycode("shift"), static_cast<std::uint32_t>(SDLK_LSHIFT));
    EXPECT_EQ(*authorKeycode("escape"), static_cast<std::uint32_t>(SDLK_ESCAPE));
    EXPECT_EQ(*authorKeycode("tab"), static_cast<std::uint32_t>(SDLK_TAB));
    EXPECT_EQ(*authorKeycode("f3"), static_cast<std::uint32_t>(SDLK_F3));
    EXPECT_EQ(*authorKeycode("f5"), static_cast<std::uint32_t>(SDLK_F5));
    for (int d = 1; d <= 9; ++d) {
        EXPECT_EQ(*authorKeycode(std::to_string(d)),
                  static_cast<std::uint32_t>(SDLK_1 + (d - 1)));
    }
    EXPECT_FALSE(authorKeycode("x").ok());
    EXPECT_FALSE(authorKeycode("W").ok()); // the vocabulary is lowercase
    EXPECT_NE(authorKeycode("x").error().message.find("unknown timeline key"),
              std::string::npos);
}

TEST(V3110Demo, AuthorButtonTable) {
    EXPECT_EQ(*authorButton("left"), SDL_BUTTON_LEFT);
    EXPECT_EQ(*authorButton("right"), SDL_BUTTON_RIGHT);
    EXPECT_EQ(*authorButton("middle"), SDL_BUTTON_MIDDLE);
    EXPECT_FALSE(authorButton("up").ok());
}

TEST(V3110Demo, AuthorHoldExpansion) {
    auto r = parseTimelineActions(
        Json::parse(R"([{"at_frame": 10, "action": "hold", "key": "w", )"
                    R"("until_frame": 30}])")
            .value());
    ASSERT_TRUE(r.ok()) << r.error().describe();
    DemoWorldSpec world;
    world.type = DemoWorldSpec::Type::Fresh;
    world.seed = 42;
    const DemoFile f = authorDemoFile(world, *r);
    EXPECT_EQ(f.sim, kDemoSimVersion);
    EXPECT_EQ(f.recordedWith, kDemoFileVersion);
    EXPECT_EQ(f.dts.size(), 31u);
    for (const double d : f.dts) {
        EXPECT_DOUBLE_EQ(d, kDemoAuthorDt);
    }
    ASSERT_EQ(f.events.size(), 2u);
    EXPECT_EQ(f.events[0].frame, 10);
    EXPECT_EQ(f.events[0].event.kind, InputEvent::Kind::KeyDown);
    EXPECT_EQ(f.events[0].event.u32, static_cast<std::uint32_t>(SDLK_W));
    EXPECT_EQ(f.events[1].frame, 30);
    EXPECT_EQ(f.events[1].event.kind, InputEvent::Kind::KeyUp);
}

TEST(V3110Demo, AuthorHoldToEnd) {
    auto r = parseTimelineActions(
        Json::parse(R"([{"at_frame": 10, "action": "hold", "key": "w"}])")
            .value());
    ASSERT_TRUE(r.ok()) << r.error().describe();
    DemoWorldSpec world;
    world.type = DemoWorldSpec::Type::Fresh;
    world.seed = 42;
    const DemoFile f = authorDemoFile(world, *r);
    EXPECT_EQ(f.dts.size(), 11u); // at_frame + 1 (no until)
    ASSERT_EQ(f.events.size(), 1u);
    EXPECT_EQ(f.events[0].frame, 10);
    EXPECT_EQ(f.events[0].event.kind, InputEvent::Kind::KeyDown);
    // The key stays down at the demo's end (the no-release event).
}

TEST(V3110Demo, AuthorOrderPin) {
    // The (frame, action index, the hold's release = the second
    // half) total order: the same-frame ties keep the timeline's
    // order; the hold's release at frame 10 lands BEFORE the next
    // action's frame-10 event (the action index 0 < 1).
    auto r = parseTimelineActions(Json::parse(R"([
        {"at_frame": 0, "action": "hold", "key": "w", "until_frame": 10},
        {"at_frame": 5, "action": "mouse_down", "button": "left"},
        {"at_frame": 5, "action": "key_down", "key": "e"},
        {"at_frame": 10, "action": "key_down", "key": "a"},
        {"at_frame": 12, "action": "mouse_move", "dx": 1.5, "dy": -2},
        {"at_frame": 14, "action": "wheel", "ticks": -2},
        {"at_frame": 16, "action": "mouse_up", "button": "left"}
    ])")
                                   .value());
    ASSERT_TRUE(r.ok()) << r.error().describe();
    DemoWorldSpec world;
    world.type = DemoWorldSpec::Type::Save;
    world.slot = "s1";
    const DemoFile f = authorDemoFile(world, *r);
    EXPECT_EQ(f.dts.size(), 17u); // the max at_frame 16 + 1
    ASSERT_EQ(f.events.size(), 8u);
    EXPECT_EQ(f.events[0].frame, 0);
    EXPECT_EQ(f.events[0].event.kind, InputEvent::Kind::KeyDown);
    EXPECT_EQ(f.events[0].event.u32, static_cast<std::uint32_t>(SDLK_W));
    EXPECT_EQ(f.events[1].frame, 5);
    EXPECT_EQ(f.events[1].event.kind, InputEvent::Kind::MouseDown);
    EXPECT_EQ(f.events[1].event.code, SDL_BUTTON_LEFT);
    EXPECT_EQ(f.events[2].frame, 5);
    EXPECT_EQ(f.events[2].event.kind, InputEvent::Kind::KeyDown);
    EXPECT_EQ(f.events[2].event.u32, static_cast<std::uint32_t>(SDLK_E));
    EXPECT_EQ(f.events[3].frame, 10);
    EXPECT_EQ(f.events[3].event.kind, InputEvent::Kind::KeyUp); // the
                                                                 // hold's
                                                                 // release
    EXPECT_EQ(f.events[4].frame, 10);
    EXPECT_EQ(f.events[4].event.kind, InputEvent::Kind::KeyDown);
    EXPECT_EQ(f.events[4].event.u32, static_cast<std::uint32_t>(SDLK_A));
    EXPECT_EQ(f.events[5].frame, 12);
    EXPECT_EQ(f.events[5].event.kind, InputEvent::Kind::MouseMove);
    EXPECT_FLOAT_EQ(f.events[5].event.dx, 1.5f);
    EXPECT_FLOAT_EQ(f.events[5].event.dy, -2.0f);
    EXPECT_EQ(f.events[6].frame, 14);
    EXPECT_EQ(f.events[6].event.kind, InputEvent::Kind::Wheel);
    EXPECT_EQ(f.events[6].event.code, -2);
    EXPECT_EQ(f.events[7].frame, 16);
    EXPECT_EQ(f.events[7].event.kind, InputEvent::Kind::MouseUp);
}

TEST(V3110Demo, AuthorRejectFaces) {
    const auto check = [](const std::string& actions, const std::string& needle) {
        auto r = parseTimelineActions(Json::parse(actions).value());
        EXPECT_FALSE(r.ok()) << actions;
        EXPECT_NE(r.error().message.find(needle), std::string::npos)
            << r.error().message << " !~ " << needle;
    };
    check(R"([{"at_frame": 0, "action": "fly", "key": "w"}])",
          "unknown action 'fly'");
    check(R"([{"at_frame": 0, "action": "key_down", "key": "x"}])",
          "unknown timeline key 'x'");
    check(R"([{"at_frame": 0, "action": "mouse_down", "button": "up"}])",
          "unknown timeline button 'up'");
    check(R"([{"at_frame": 5, "action": "hold", "key": "w", "until_frame": 5}])",
          "'until_frame' must be > 'at_frame'");
    check(R"([{"at_frame": 5, "action": "hold", "key": "w", "until_frame": 3}])",
          "'until_frame' must be > 'at_frame'");
    check(R"([{"action": "key_down", "key": "w"}])", "missing 'at_frame'");
    check(R"([{"at_frame": 1.5, "action": "key_down", "key": "w"}])",
          "'at_frame' must be an integer");
    check(R"([{"at_frame": -1, "action": "key_down", "key": "w"}])",
          "'at_frame' must be an integer");
    check(R"([{"at_frame": 0, "action": "key_down"}])", "'key' is required");
    check(R"([{"at_frame": 0, "action": "mouse_move", "dx": 1}])",
          "'dy' is required");
    check(R"([{"at_frame": 0, "action": "wheel"}])", "'ticks' is required");
    check(R"([{"at_frame": 0, "action": "key_down", "key": "w", "foo": 1}])",
          "unexpected key 'foo'");
    check(R"([{"at_frame": 0, "action": "gamepad_button", "button": 0, "down": true}])",
          "the record's face");
    check(R"([{"at_frame": 0, "action": "gamepad_axis", "axis": 0, "value": 0}])",
          "the record's face");
}

TEST(V3110Demo, AuthorEmptyActions) {
    auto r = parseTimelineActions(Json::parse("[]").value());
    ASSERT_TRUE(r.ok());
    DemoWorldSpec world;
    world.type = DemoWorldSpec::Type::Fresh;
    world.seed = 1;
    const DemoFile f = authorDemoFile(world, *r);
    EXPECT_EQ(f.dts.size(), 0u);
    EXPECT_EQ(f.events.size(), 0u);
}

TEST(V3110Demo, AuthorEndToEnd) {
    const std::filesystem::path timeline = scratchDir() / "t.json";
    writeAll(timeline.string(), R"({
        "world": {"type": "fresh", "seed": 7},
        "actions": [
            {"at_frame": 0, "action": "hold", "key": "w", "until_frame": 59}
        ]
    })");
    auto out = authorDemoFromFile(timeline);
    ASSERT_TRUE(out.ok()) << out.error().describe();
    EXPECT_EQ(out->string(), (scratchDir() / "t.dmo").string());
    auto r = loadDemoFile(*out);
    ASSERT_TRUE(r.ok()) << r.error().describe();
    EXPECT_EQ(r->world.type, DemoWorldSpec::Type::Fresh);
    EXPECT_EQ(r->world.seed, 7);
    EXPECT_EQ(r->dts.size(), 60u);
    ASSERT_EQ(r->events.size(), 2u);
    EXPECT_EQ(r->events[0].event.kind, InputEvent::Kind::KeyDown);
    EXPECT_EQ(r->events[1].event.kind, InputEvent::Kind::KeyUp);
    // The author's idempotence: the second run is byte-identical.
    const std::string first = readAll(out->string());
    auto out2 = authorDemoFromFile(timeline);
    ASSERT_TRUE(out2.ok());
    EXPECT_EQ(readAll(out2->string()), first);
}

TEST(V3110Demo, AuthorRejectTimelineRoot) {
    const std::filesystem::path p = scratchDir() / "badroot.json";
    writeAll(p.string(), R"({"world": {"type": "fresh", "seed": 1}, )"
                        R"("actions": [], "extra": 1})");
    auto r = authorDemoFromFile(p);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("unexpected top-level key 'extra'"),
              std::string::npos);
    const std::filesystem::path p2 = scratchDir() / "badworld.json";
    writeAll(p2.string(), R"({"world": {"type": "void", "seed": 1}, )"
                         R"("actions": []})");
    auto r2 = authorDemoFromFile(p2);
    ASSERT_FALSE(r2.ok());
    EXPECT_NE(r2.error().message.find("unknown type 'void'"),
              std::string::npos);
}

// ---------------------------------------------------------------------------
// The audience's routing (the structural pin)
// ---------------------------------------------------------------------------

TEST(V3110Demo, AudienceChannelIsolation) {
    // The two-Input architecture's pin: the audience's events never
    // reach the player's state (the isDown / the wasPressed / the
    // mouseDelta's zero — a future refactor that merges the channels
    // breaks this).
    Input player, audience;
    audience.handleKey(SDLK_W, true);
    audience.handleMouseMotion(50.0f, 50.0f, 300.0f, 200.0f);
    audience.handleMouseButton(SDL_BUTTON_LEFT, true);
    audience.handleMouseWheel(3);
    EXPECT_FALSE(player.isDown(GameKey::W));
    EXPECT_FALSE(player.wasPressed(GameKey::W));
    EXPECT_FALSE(player.isDown(MouseButton::Left));
    EXPECT_DOUBLE_EQ(player.mouseDelta().x, 0.0);
    EXPECT_DOUBLE_EQ(player.mouseDelta().y, 0.0);
    EXPECT_EQ(player.wheelDelta(), 0);
    // The inverse (the player's channel is demo-fed only).
    player.handleKey(SDLK_A, true);
    EXPECT_TRUE(player.isDown(GameKey::A));
    EXPECT_FALSE(audience.isDown(GameKey::A));
}

TEST(V3110Demo, AudienceInjectionZeroLeak) {
    // The demo's events zero-leak the player's channel: the injected
    // count (the handler's closure tap) = the file's event count.
    const DemoFile f = fixtureFile();
    const DemoPlayer player(f);
    Input in;
    in.handleGamepadAdded(7);
    int tapped = 0;
    in.setEventTap([&tapped](const InputEvent&) { ++tapped; });
    for (int frame = 0; frame < player.frameCount(); ++frame) {
        in.frameBegin();
        player.injectForFrame(frame, in);
    }
    EXPECT_EQ(tapped, static_cast<int>(f.events.size()));
}

// ---------------------------------------------------------------------------
// The pure labels (the badge + the panel's lines)
// ---------------------------------------------------------------------------

TEST(V3110Demo, SpeedLabel) {
    EXPECT_EQ(demoSpeedLabel(kDemoSpeedMin), "x0.5");
    EXPECT_EQ(demoSpeedLabel(kDemoSpeedDefault), "x1");
    EXPECT_EQ(demoSpeedLabel(kDemoSpeedMax), "x2");
    EXPECT_EQ(demoSpeedLabel(1.5), "x1.5");
}

TEST(V3110Demo, FileName) {
    EXPECT_EQ(demoFileName("assets/demos/example.dmo"), "example.dmo");
    EXPECT_EQ(demoFileName("/a/b/c.dmo"), "c.dmo");
    EXPECT_EQ(demoFileName("c"), "c");
}

TEST(V3110Demo, WorldDescription) {
    DemoWorldSpec fresh;
    fresh.type = DemoWorldSpec::Type::Fresh;
    fresh.seed = 123456789;
    EXPECT_EQ(demoWorldDescription(fresh), "fresh seed 123456789");
    DemoWorldSpec save;
    save.type = DemoWorldSpec::Type::Save;
    save.slot = "g199";
    EXPECT_EQ(demoWorldDescription(save), "save slot 'g199'");
}

// ---------------------------------------------------------------------------
// V3-16a: the panel-click (the geometry-free slot addressing) + the
// guide index's data layer + the scene world's spec
// ---------------------------------------------------------------------------

TEST(V316aDemo, PanelClickVocabTables) {
    // The panel's names' round trip.
    EXPECT_EQ(*authorPanel("inventory"), kDemoPanelInventory);
    EXPECT_EQ(*authorPanel("table"), kDemoPanelTable);
    EXPECT_EQ(*authorPanel("furnace"), kDemoPanelFurnace);
    EXPECT_EQ(*authorPanel("creative"), kDemoPanelCreative);
    EXPECT_FALSE(authorPanel("enchant").ok());
    EXPECT_EQ(demoPanelName(1), "table");

    // The shared player inventory's names (every panel).
    EXPECT_EQ(*authorPanelSlot(kDemoPanelInventory, "h0"), 0);
    EXPECT_EQ(*authorPanelSlot(kDemoPanelTable, "h8"), 8);
    EXPECT_EQ(*authorPanelSlot(kDemoPanelFurnace, "b0"), 9);
    EXPECT_EQ(*authorPanelSlot(kDemoPanelFurnace, "b26"), 35);
    // The per-panel's names.
    EXPECT_EQ(*authorPanelSlot(kDemoPanelInventory, "g0"), 100);
    EXPECT_EQ(*authorPanelSlot(kDemoPanelInventory, "g3"), 103);
    EXPECT_EQ(*authorPanelSlot(kDemoPanelInventory, "out"), 110);
    EXPECT_EQ(*authorPanelSlot(kDemoPanelTable, "g0"), 200);
    EXPECT_EQ(*authorPanelSlot(kDemoPanelTable, "g8"), 208);
    EXPECT_EQ(*authorPanelSlot(kDemoPanelFurnace, "in"), 120);
    EXPECT_EQ(*authorPanelSlot(kDemoPanelFurnace, "fuel"), 121);
    EXPECT_EQ(*authorPanelSlot(kDemoPanelFurnace, "out"), 122);
    EXPECT_EQ(*authorPanelSlot(kDemoPanelCreative, "c0"), 300);
    EXPECT_EQ(*authorPanelSlot(kDemoPanelCreative, "c23"), 323);
    // The cross-panel's rejects (a furnace has no g0, the inventory's
    // grid is 2x2, the table's is 3x3, the creative's cells cap 23).
    EXPECT_FALSE(authorPanelSlot(kDemoPanelFurnace, "g0").ok());
    EXPECT_FALSE(authorPanelSlot(kDemoPanelInventory, "g4").ok());
    EXPECT_FALSE(authorPanelSlot(kDemoPanelTable, "g9").ok());
    EXPECT_FALSE(authorPanelSlot(kDemoPanelCreative, "c24").ok());
    EXPECT_FALSE(authorPanelSlot(kDemoPanelInventory, "h9").ok());
    EXPECT_FALSE(authorPanelSlot(kDemoPanelInventory, "b27").ok());
    // The display's inverse (the error line's twin).
    EXPECT_EQ(demoPanelSlotName(kDemoPanelTable, 5), "h5");
    EXPECT_EQ(demoPanelSlotName(kDemoPanelTable, 35), "b26");
    EXPECT_EQ(demoPanelSlotName(kDemoPanelTable, 208), "g8");
    EXPECT_EQ(demoPanelSlotName(kDemoPanelFurnace, 121), "fuel");
    EXPECT_EQ(demoPanelSlotName(kDemoPanelCreative, 303), "c3");
    // The button's names.
    EXPECT_EQ(*authorPanelButton("left"), kDemoPanelButtonLeft);
    EXPECT_EQ(*authorPanelButton("right"), kDemoPanelButtonRight);
    EXPECT_EQ(*authorPanelButton("shift"), kDemoPanelButtonShift);
    EXPECT_FALSE(authorPanelButton("middle").ok());
}

TEST(V316aDemo, PanelClickAuthorExpansion) {
    // The timeline's panel_click's expansion (the event's face + the
    // frame's bound + the (frame, action)'s order).
    Json actions = Json::array();
    Json a0 = Json::object();
    a0["at_frame"] = 10;
    a0["action"] = "panel_click";
    a0["panel"] = "table";
    a0["slot"] = "out";
    a0["button"] = "left";
    actions.push_back(a0);
    Json a1 = Json::object();
    a1["at_frame"] = 12;
    a1["action"] = "panel_click";
    a1["panel"] = "furnace";
    a1["slot"] = "in";
    a1["button"] = "shift";
    actions.push_back(a1);
    auto list = parseTimelineActions(actions);
    ASSERT_TRUE(list.ok()) << list.error().describe();
    DemoWorldSpec spec;
    spec.type = DemoWorldSpec::Type::Scene;
    spec.slot = "pilot_mine";
    DemoFile f = authorDemoFile(spec, *list);
    EXPECT_EQ(f.world.type, DemoWorldSpec::Type::Scene);
    EXPECT_EQ(f.world.slot, "pilot_mine");
    ASSERT_EQ(f.events.size(), 2u);
    EXPECT_EQ(f.events[0].frame, 10);
    EXPECT_EQ(f.events[0].event.kind, InputEvent::Kind::PanelClick);
    EXPECT_EQ(f.events[0].event.u32, kDemoPanelTable);
    EXPECT_EQ(f.events[0].event.code, 110);
    EXPECT_EQ(f.events[0].event.value, kDemoPanelButtonLeft);
    EXPECT_EQ(f.events[1].frame, 12);
    EXPECT_EQ(f.events[1].event.u32, kDemoPanelFurnace);
    EXPECT_EQ(f.events[1].event.code, 120);
    EXPECT_EQ(f.events[1].event.value, kDemoPanelButtonShift);
    EXPECT_EQ(f.dts.size(), 13u);
}

TEST(V316aDemo, PanelClickCodecRoundTrip) {
    DemoFile f = fixtureFile();
    f.events.push_back({3, InputEvent {InputEvent::Kind::PanelClick,
                                       kDemoPanelInventory, 110,
                                       kDemoPanelButtonLeft, 0.0f, 0.0f,
                                       0.0f, 0.0f, false}});
    const std::string body = serializeDemoJson(f);
    auto js = Json::parse(body);
    ASSERT_TRUE(js.ok());
    auto back = parseDemoJson(*js);
    ASSERT_TRUE(back.ok()) << back.error().describe();
    ASSERT_EQ(back->events.size(), f.events.size());
    const InputEvent& e = back->events.back().event;
    EXPECT_EQ(e.kind, InputEvent::Kind::PanelClick);
    EXPECT_EQ(e.u32, kDemoPanelInventory);
    EXPECT_EQ(e.code, 110);
    EXPECT_EQ(e.value, kDemoPanelButtonLeft);
    // The scene's world's file face.
    f.world.type = DemoWorldSpec::Type::Scene;
    f.world.slot = "abc";
    const std::string body2 = serializeDemoJson(f);
    auto js2 = Json::parse(body2);
    ASSERT_TRUE(js2.ok());
    const Json* w2 = js2->find("world");
    ASSERT_NE(w2, nullptr);
    EXPECT_EQ(w2->find("type")->asString(), "scene");
    EXPECT_EQ(w2->find("name")->asString(), "abc");
    auto back2 = parseDemoJson(*js2);
    ASSERT_TRUE(back2.ok()) << back2.error().describe();
    EXPECT_EQ(back2->world.type, DemoWorldSpec::Type::Scene);
    EXPECT_EQ(back2->world.slot, "abc");
    EXPECT_EQ(demoWorldDescription(back2->world),
              "scene 'abc' (the committed guide asset)");
}

TEST(V316aDemo, PanelClickCodecRejects) {
    DemoFile f = fixtureFile();
    f.events.push_back({3, InputEvent {InputEvent::Kind::PanelClick,
                                       kDemoPanelInventory, 110,
                                       kDemoPanelButtonLeft, 0.0f, 0.0f,
                                       0.0f, 0.0f, false}});
    auto base = Json::parse(serializeDemoJson(f));
    ASSERT_TRUE(base.ok());
    const Json* pe = base->find("events");
    ASSERT_NE(pe, nullptr);
    // The panel's range (0..3).
    {
        Json r = *base;
        r["events"].at(6)["panel"] = Json(5);
        auto out = parseDemoJson(r);
        ASSERT_FALSE(out.ok());
        EXPECT_NE(out.error().message.find("0..3"), std::string::npos);
    }
    // The button's range (0..2).
    {
        Json r = *base;
        r["events"].at(6)["button"] = Json(3);
        auto out = parseDemoJson(r);
        ASSERT_FALSE(out.ok());
        EXPECT_NE(out.error().message.find("0..2"), std::string::npos);
    }
    // The slot's type (an integer id, not a name).
    {
        Json r = *base;
        r["events"].at(6)["slot"] = Json("out");
        auto out = parseDemoJson(r);
        ASSERT_FALSE(out.ok());
        EXPECT_NE(out.error().message.find("slot"), std::string::npos);
    }
    // The unexpected key (the closed payload).
    {
        Json r = *base;
        r["events"].at(6)["extra"] = Json(1);
        auto out = parseDemoJson(r);
        ASSERT_FALSE(out.ok());
        EXPECT_NE(out.error().message.find("unexpected key"),
                  std::string::npos);
    }
}

TEST(V316aDemo, AuthorRejectsPanelClickFaces) {
    auto oneEntry = [](const char* panel, const char* slot,
                       const char* button) -> Json {
        Json actions = Json::array();
        Json a0 = Json::object();
        a0["at_frame"] = 1;
        a0["action"] = "panel_click";
        if (panel) {
            a0["panel"] = Json(std::string(panel));
        }
        if (slot) {
            a0["slot"] = Json(std::string(slot));
        }
        if (button) {
            a0["button"] = Json(std::string(button));
        }
        actions.push_back(a0);
        return actions;
    };
    {
        auto r = parseTimelineActions(oneEntry("enchant", "out", "left"));
        ASSERT_FALSE(r.ok());
        EXPECT_NE(r.error().message.find("unknown panel"),
                  std::string::npos);
    }
    {
        auto r = parseTimelineActions(oneEntry("inventory", "g4", "left"));
        ASSERT_FALSE(r.ok());
        EXPECT_NE(r.error().message.find("g0..g3"), std::string::npos);
    }
    {
        auto r = parseTimelineActions(oneEntry("inventory", "out",
                                               "middle"));
        ASSERT_FALSE(r.ok());
        EXPECT_NE(r.error().message.find("left/right/shift"),
                  std::string::npos);
    }
    {
        auto r = parseTimelineActions(oneEntry("inventory", nullptr,
                                               "left"));
        ASSERT_FALSE(r.ok());
        EXPECT_NE(r.error().message.find("'slot' is required"),
                  std::string::npos);
    }
    // The clean face (the control).
    {
        auto r = parseTimelineActions(oneEntry("inventory", "out", "left"));
        ASSERT_TRUE(r.ok()) << r.error().describe();
    }
}

TEST(V316aDemo, ScriptRejectsPanelClick) {
    // The net's --script has no panel_click (the solo's guide's face —
    // the lockstep's 40B's input carries no panel id, the bound).
    Json root = Json::object();
    Json actions = Json::array();
    Json a0 = Json::object();
    a0["at_frame"] = 1;
    a0["action"] = "panel_click";
    a0["panel"] = "inventory";
    a0["slot"] = "out";
    a0["button"] = "left";
    actions.push_back(a0);
    root["actions"] = actions;
    const auto p = scratchDir() / "pc_script.json";
    writeAll(p.string(), root.dump(-1));
    auto r = parseScriptTimeline(p);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("panel_click"), std::string::npos);
    // The control (the keys' face parses).
    Json root2 = Json::object();
    Json actions2 = Json::array();
    Json a1 = Json::object();
    a1["at_frame"] = 1;
    a1["action"] = "key_down";
    a1["key"] = "w";
    actions2.push_back(a1);
    root2["actions"] = actions2;
    const auto p2 = scratchDir() / "pc_script_ok.json";
    writeAll(p2.string(), root2.dump(-1));
    EXPECT_TRUE(parseScriptTimeline(p2).ok());
}

TEST(V316aDemo, InjectPanelClickResolverFace) {
    DemoFile f = fixtureFile();
    f.events.push_back({3, InputEvent {InputEvent::Kind::PanelClick,
                                       kDemoPanelTable, 110,
                                       kDemoPanelButtonLeft, 0.0f, 0.0f,
                                       0.0f, 0.0f, false}});
    DemoPlayer p(f);
    Input in;
    // The resolver's absent face (the named problem).
    const std::string err = p.injectForFrame(3, in);
    EXPECT_NE(err.find("no resolver"), std::string::npos);
    // The resolver's failure face (its name passes through).
    const std::string err2 =
        p.injectForFrame(3, in, [](std::uint32_t, std::int32_t,
                                   std::int16_t) {
            return "the table's panel is not open";
        });
    EXPECT_EQ(err2, "the table's panel is not open");
    // The resolver's success face (the ids' pass-through).
    std::uint32_t seenPanel = 99;
    std::int32_t seenSlot = -1;
    const std::string err3 =
        p.injectForFrame(3, in, [&](std::uint32_t panel, std::int32_t slot,
                                    std::int16_t) {
            seenPanel = panel;
            seenSlot = slot;
            return "";
        });
    EXPECT_TRUE(err3.empty());
    EXPECT_EQ(seenPanel, kDemoPanelTable);
    EXPECT_EQ(seenSlot, 110);
}

TEST(V316aDemo, SceneWorldSpecRejects) {
    // The hand-built json's reject faces (the parse's strictness).
    auto sceneJson = []() {
        DemoFile f = fixtureFile();
        f.world.type = DemoWorldSpec::Type::Scene;
        f.world.slot = "abc";
        return Json::parse(serializeDemoJson(f));
    };
    {
        auto js = sceneJson();
        ASSERT_TRUE(js.ok());
        Json r = *js;
        r["world"] = Json::object();
        r["world"]["type"] = Json(std::string("scene"));
        auto out = parseDemoJson(r);
        ASSERT_FALSE(out.ok());
        EXPECT_NE(out.error().message.find("'name'"), std::string::npos);
    }
    {
        auto js = sceneJson();
        ASSERT_TRUE(js.ok());
        Json r = *js;
        r["world"]["name"] = Json(std::string("a/b"));
        auto out = parseDemoJson(r);
        ASSERT_FALSE(out.ok());
        EXPECT_NE(out.error().message.find("scene name"),
                  std::string::npos);
    }
    {
        auto js = sceneJson();
        ASSERT_TRUE(js.ok());
        Json r = *js;
        r["world"]["seed"] = Json(1);
        auto out = parseDemoJson(r);
        ASSERT_FALSE(out.ok());
        EXPECT_NE(out.error().message.find("double key"),
                  std::string::npos);
    }
    {
        auto js = sceneJson();
        ASSERT_TRUE(js.ok());
        Json r = *js;
        r["world"]["type"] = Json(std::string("scenex"));
        auto out = parseDemoJson(r);
        ASSERT_FALSE(out.ok());
        EXPECT_NE(out.error().message.find("fresh|save|scene"),
                  std::string::npos);
    }
    // The scene's path's face (V3-18: the .json.gz's extension — the
    // save family's rename).
    EXPECT_EQ(demoScenePath("abc_1").string(),
              "assets/demos/scenes/abc_1.json.gz");
}

TEST(V316aDemo, ScenePathTwoStageFind) {
    // V3-18: the scene asset rides the save family's rename — the
    // committed form is .json.gz, the legacy .json still reads (zero
    // migration, the mid-rename checkout's boot).
    const auto dir = scratchDir() / "scenefind";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    // Neither = the empty path (the cross-check's naming's input).
    EXPECT_TRUE(demoSceneReadPath("ghost", dir).empty());
    // The legacy .json alone is found.
    writeAll((dir / "legacy.json").string(), "x");
    EXPECT_EQ(demoSceneReadPath("legacy", dir), dir / "legacy.json");
    // The current .json.gz alone is found.
    writeAll((dir / "modern.json.gz").string(), "x");
    EXPECT_EQ(demoSceneReadPath("modern", dir), dir / "modern.json.gz");
    // BOTH forms = the .json.gz wins (the current form's read).
    writeAll((dir / "both.json.gz").string(), "gz");
    writeAll((dir / "both.json").string(), "legacy");
    EXPECT_EQ(demoSceneReadPath("both", dir), dir / "both.json.gz");
    std::filesystem::remove_all(dir, ec);
}

TEST(V316aDemo, IndexParseValid) {
    const std::string text =
        R"({
          "demos": [
            {"category":"day1","seq":1,"mode":"solo","tags":["mining","crafting"],"file":"guide_mining.dmo","length_frames":3600},
            {"category":"day1","seq":2,"mode":"solo","tags":[],"file":"guide_food.dmo","length_frames":2400},
            {"category":"coop","seq":1,"mode":"coop","tags":["net"],"file":"guide_coop.dmo","length_frames":1800}
          ]
        })";
    auto js = Json::parse(text);
    ASSERT_TRUE(js.ok());
    auto r = parseDemoIndex(*js);
    ASSERT_TRUE(r.ok()) << r.error().describe();
    ASSERT_EQ(r->demos.size(), 3u);
    EXPECT_EQ(r->demos[0].category, "day1");
    EXPECT_EQ(r->demos[0].seq, 1);
    EXPECT_EQ(r->demos[0].mode, "solo");
    ASSERT_EQ(r->demos[0].tags.size(), 2u);
    EXPECT_EQ(r->demos[0].tags[0], "mining");
    EXPECT_EQ(r->demos[0].tags[1], "crafting");
    EXPECT_EQ(r->demos[0].file, "guide_mining.dmo");
    EXPECT_EQ(r->demos[0].lengthFrames, 3600);
    EXPECT_EQ(r->demos[2].mode, "coop");
    // The empty index's face (a chapter's not-yet-filled).
    auto js2 = Json::parse(R"({"demos":[]})");
    ASSERT_TRUE(js2.ok());
    EXPECT_TRUE(parseDemoIndex(*js2).ok());
}

TEST(V316aDemo, IndexTitleParse) {
    // V3-20: the display's title (the menu's tier-2's + the --list-
    // demos's name face; absent = the file's name's fallback).
    auto one = [](const std::string& entry) -> Result<DemoIndex> {
        const std::string text = std::string("{\"demos\":[") +
                                 entry + "]}";
        auto js = Json::parse(text);
        EXPECT_TRUE(js.ok());
        if (!js.ok()) {
            return MC_FAIL("the test's json is broken");
        }
        return parseDemoIndex(*js);
    };
    // Absent = empty (the fallback's face).
    {
        auto r = one(R"({"category":"day1","seq":1,"mode":"solo","tags":[],"file":"x.dmo","length_frames":10})");
        ASSERT_TRUE(r.ok()) << r.error().describe();
        EXPECT_TRUE(r->demos[0].title.empty());
    }
    // Present = the round-trip (the CJK's + the ASCII's mixed).
    {
        auto r = one(R"({"category":"day1","seq":1,"mode":"solo","tags":[],"file":"x.dmo","length_frames":10,"title":"挖矿 v2"})");
        ASSERT_TRUE(r.ok()) << r.error().describe();
        EXPECT_EQ(r->demos[0].title, "挖矿 v2");
    }
    // The non-string's reject.
    {
        auto r = one(R"({"category":"day1","seq":1,"mode":"solo","tags":[],"file":"x.dmo","length_frames":10,"title":7})");
        ASSERT_FALSE(r.ok());
        EXPECT_NE(r.error().message.find("'title' must be a string"),
                  std::string::npos);
    }
    // The empty's reject.
    {
        auto r = one(R"({"category":"day1","seq":1,"mode":"solo","tags":[],"file":"x.dmo","length_frames":10,"title":""})");
        ASSERT_FALSE(r.ok());
        EXPECT_NE(r.error().message.find("non-empty"), std::string::npos);
    }
    // The FONT SAFETY's reject (the 17d's 戏's on-machine catch's
    // fail-fast: a missing glyph would render the menu's '?'). 龘
    // U+9F98 is not in the charset.
    {
        auto r = one(R"({"category":"day1","seq":1,"mode":"solo","tags":[],"file":"x.dmo","length_frames":10,"title":"于龘"})");
        ASSERT_FALSE(r.ok());
        EXPECT_NE(r.error().message.find("missing glyph"),
                  std::string::npos);
        EXPECT_NE(r.error().message.find("U+9F98"), std::string::npos);
    }
    // The invalid UTF-8's reject (the title's raw bytes — the decoder's
    // named reject).
    {
        const std::string title = std::string("\xE4\x80"); // truncated's 3-byte
        const std::string text =
            std::string("{\"demos\":[{\"category\":\"day1\",\"seq\":1,"
                        "\"mode\":\"solo\",\"tags\":[],\"file\":\"x.dmo\","
                        "\"length_frames\":10,\"title\":\"") +
            title + "\"}]}";
        auto js = Json::parse(text);
        ASSERT_TRUE(js.ok());
        auto r = parseDemoIndex(*js);
        ASSERT_FALSE(r.ok());
        EXPECT_NE(r.error().message.find("invalid UTF-8"),
                  std::string::npos);
    }
}

TEST(V316aDemo, IndexParseRejects) {
    const std::string good = R"({"category":"day1","seq":1,"mode":"solo","tags":["a"],"file":"x.dmo","length_frames":10})";
    auto one = [](const std::string& entry) -> Result<DemoIndex> {
        const std::string text = std::string("{\"demos\":[") +
                                 entry + "]}";
        auto js = Json::parse(text);
        EXPECT_TRUE(js.ok());
        if (!js.ok()) {
            return MC_FAIL("the test's json is broken");
        }
        return parseDemoIndex(*js);
    };
    auto two = [](const std::string& e1,
                 const std::string& e2) -> Result<DemoIndex> {
        const std::string text =
            std::string("{\"demos\":[") + e1 + "," + e2 + "]}";
        auto js = Json::parse(text);
        EXPECT_TRUE(js.ok());
        if (!js.ok()) {
            return MC_FAIL("the test's json is broken");
        }
        return parseDemoIndex(*js);
    };
    const std::string good2 =
        R"({"category":"build","seq":1,"mode":"solo","tags":[],"file":"y.dmo","length_frames":10})";
    // The category's vocabulary.
    {
        auto r = one(R"({"category":"bogus","seq":1,"mode":"solo","tags":[],"file":"x.dmo","length_frames":10})");
        ASSERT_FALSE(r.ok());
        EXPECT_NE(r.error().message.find("unknown category"),
                  std::string::npos);
    }
    // The seq's lower bound.
    {
        auto r = one(R"({"category":"day1","seq":0,"mode":"solo","tags":[],"file":"x.dmo","length_frames":10})");
        ASSERT_FALSE(r.ok());
        EXPECT_NE(r.error().message.find("integer >= 1"), std::string::npos);
    }
    // The (category, seq)'s duplicate.
    {
        auto r = two(good, R"({"category":"day1","seq":1,"mode":"solo","tags":[],"file":"y.dmo","length_frames":10})");
        ASSERT_FALSE(r.ok());
        EXPECT_NE(r.error().message.find("duplicate seq"),
                  std::string::npos);
    }
    // The seq's gap (the contiguity's 1..n).
    {
        auto r = one(R"({"category":"day1","seq":2,"mode":"solo","tags":[],"file":"x.dmo","length_frames":10})");
        ASSERT_FALSE(r.ok());
        EXPECT_NE(r.error().message.find("seq gap"), std::string::npos);
    }
    // The mode's vocabulary.
    {
        auto r = one(R"({"category":"day1","seq":1,"mode":"duo","tags":[],"file":"x.dmo","length_frames":10})");
        ASSERT_FALSE(r.ok());
        EXPECT_NE(r.error().message.find("\"solo\" or \"coop\""),
                  std::string::npos);
    }
    // The file's duplicate.
    {
        auto r = two(good, R"({"category":"build","seq":1,"mode":"solo","tags":[],"file":"x.dmo","length_frames":10})");
        ASSERT_FALSE(r.ok());
        EXPECT_NE(r.error().message.find("duplicate file"),
                  std::string::npos);
    }
    // The file's extension's charset.
    {
        auto r = one(R"({"category":"day1","seq":1,"mode":"solo","tags":[],"file":"x.txt","length_frames":10})");
        ASSERT_FALSE(r.ok());
        EXPECT_NE(r.error().message.find(".dmo"), std::string::npos);
    }
    // The tag's charset (upper-case is out).
    {
        auto r = one(R"({"category":"day1","seq":1,"mode":"solo","tags":["A"],"file":"x.dmo","length_frames":10})");
        ASSERT_FALSE(r.ok());
        EXPECT_NE(r.error().message.find("[a-z0-9_]"), std::string::npos);
    }
    // The tag's duplicate.
    {
        auto r = one(R"({"category":"day1","seq":1,"mode":"solo","tags":["a","a"],"file":"x.dmo","length_frames":10})");
        ASSERT_FALSE(r.ok());
        EXPECT_NE(r.error().message.find("duplicate tag"),
                  std::string::npos);
    }
    // The length's lower bound.
    {
        auto r = one(R"({"category":"day1","seq":1,"mode":"solo","tags":[],"file":"x.dmo","length_frames":0})");
        ASSERT_FALSE(r.ok());
        EXPECT_NE(r.error().message.find("length_frames"),
                  std::string::npos);
    }
    // The entry's extra key.
    {
        auto r = one(R"({"category":"day1","seq":1,"mode":"solo","tags":["a"],"file":"x.dmo","length_frames":10,"extra":1})");
        ASSERT_FALSE(r.ok());
        EXPECT_NE(r.error().message.find("unexpected key"),
                  std::string::npos);
    }
    // The entry's missing key.
    {
        auto r = one(R"({"category":"day1","seq":1,"mode":"solo","tags":[],"file":"x.dmo"})");
        ASSERT_FALSE(r.ok());
        EXPECT_NE(r.error().message.find("length_frames"),
                  std::string::npos);
    }
    // The top-level's faces.
    {
        auto js = Json::parse(R"({"demos":[],"extra":1})");
        ASSERT_TRUE(js.ok());
        EXPECT_FALSE(parseDemoIndex(*js).ok());
    }
    {
        auto js = Json::parse(R"({"demoss":[]})");
        ASSERT_TRUE(js.ok());
        auto r = parseDemoIndex(*js);
        ASSERT_FALSE(r.ok());
        EXPECT_NE(r.error().message.find("unexpected top-level key"),
                  std::string::npos);
    }
    (void)good2;
}

TEST(V316aDemo, IndexCheckCrossFace) {
    const auto demos = scratchDir() / "demos";
    const auto scenes = scratchDir() / "scenes";
    std::error_code ec;
    std::filesystem::create_directories(demos, ec);
    std::filesystem::create_directories(scenes, ec);
    DemoIndex index;
    DemoIndexEntry e;
    e.category = "day1";
    e.seq = 1;
    e.mode = "solo";
    e.file = "guide_mining.dmo";
    e.lengthFrames = 4; // the fixture's 4 dts
    index.demos.push_back(e);
    // (a) the valid face.
    ASSERT_TRUE(writeDemoFile(demos / "guide_mining.dmo", fixtureFile()).ok());
    {
        auto r = checkDemoIndexDir(index, demos, scenes);
        ASSERT_TRUE(r.ok()) << r.error().describe();
    }
    // (b) the file's missing face.
    {
        DemoIndex i2 = index;
        i2.demos[0].file = "nope.dmo";
        auto r = checkDemoIndexDir(i2, demos, scenes);
        ASSERT_FALSE(r.ok());
        EXPECT_NE(r.error().message.find("not in"), std::string::npos);
    }
    // (c) the length's mismatch face.
    {
        DemoIndex i2 = index;
        i2.demos[0].lengthFrames = 5;
        auto r = checkDemoIndexDir(i2, demos, scenes);
        ASSERT_FALSE(r.ok());
        EXPECT_NE(r.error().message.find("length_frames"),
                  std::string::npos);
    }
    // (d) the SIM's mismatch face (a stale build's guide).
    {
        DemoFile f = fixtureFile();
        f.sim = 99;
        ASSERT_TRUE(writeDemoFile(demos / "sim.dmo", f).ok());
        DemoIndex i2 = index;
        i2.demos[0].file = "sim.dmo";
        auto r = checkDemoIndexDir(i2, demos, scenes);
        ASSERT_FALSE(r.ok());
        EXPECT_NE(r.error().message.find("sim"), std::string::npos);
    }
    // (e) the scene's missing face + (f) the scene's present face.
    DemoFile sf = fixtureFile();
    sf.world.type = DemoWorldSpec::Type::Scene;
    sf.world.slot = "scn";
    ASSERT_TRUE(writeDemoFile(demos / "scn.dmo", sf).ok());
    {
        DemoIndex i2 = index;
        i2.demos[0].file = "scn.dmo";
        auto r = checkDemoIndexDir(i2, demos, scenes);
        ASSERT_FALSE(r.ok());
        EXPECT_NE(r.error().message.find("scene"), std::string::npos);
    }
    {
        const auto sp = scenes / "scn.json";
        writeAll(sp.string(), "{}");
        DemoIndex i2 = index;
        i2.demos[0].file = "scn.dmo";
        auto r = checkDemoIndexDir(i2, demos, scenes);
        ASSERT_TRUE(r.ok()) << r.error().describe();
        std::filesystem::remove(sp, ec);
    }
}
