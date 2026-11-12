// TokenMine.cpp entry point: argument parsing, logging setup, game bootstrap.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

#include "core/demo.hpp"
#include "core/game.hpp"
#include "core/logger.hpp"
#include "core/save_slot.hpp"

using namespace mc;

namespace {

void printUsage() {
    std::printf(
        "TokenMine.cpp (Minecraft-like voxel sandbox, written by a local LLM)\n"
        "Usage: tokenmine [options]\n"
        "  --config <path>   Config file (default: config/config.json)\n"
        "  --seed <n>        Override world seed\n"
        "  --time <0..1>     Start time of day (0 = pre-dawn, 0.25 = noon,\n"
        "                    0.5 = sunset, 0.75 = midnight)\n"
        "  --weather <w>     Initial weather (V2-4): clear (the default) or\n"
        "                    rain (the opening segment runs the fixed kRainMin\n"
        "                    frames, then the world-seed stream takes over)\n"
        "  --explode <x y z> [r]\n"        "  --dragon-damage <n>  V3-10d: deterministic damage to the End dragon (gate seam)\n"
        "                    V3-14a: one deterministic blast at the run's\n"
        "                    start (the explosion's gate seam — the ghast's\n"
        "                    V3-14b fireball is its first in-game trigger;\n"
        "                    r absent = the TNT-class radius 3)\n"
        "  --frames <n>      Exit after N frames (0 = run until closed)\n"
        "  --screenshot <p>  Save a PNG screenshot of the last frame\n"
        "  --save <slot>     Save slot (letters/digits/_/-): load\n"
        "                    data/saves/<slot>.json.gz at startup when present\n"
        "                    (the legacy <slot>.json still loads — zero\n"
        "                    migration), write it on clean exit (opt-in;\n"
        "                    default = no save/load at all)\n"
        "  --new             Start a fresh world, ignoring an existing save;\n"
        "                    the slot is overwritten on exit (requires --save)\n"
        "  --save-full-diff  Force the legacy full-world save diff at exit\n"
        "                    (PIPE-2: the journal-path equivalence oracle /\n"
        "                    escape hammer; no config key, no format effect)\n"
        "  --list-saves      List the data/saves slots (name, format, seed,\n"
        "                    time of day, player position, health) and exit\n"
        "  --copy-save S D   Copy save slot S to a NEW slot D (branch a\n"
        "                    save; D must not exist) and exit\n"
        "  --rename-save S D Rename save slot S to D (D must not exist)\n"
        "                    and exit\n"
        "  --delete-save S   Delete save slot S and exit\n"
        "                    (slot names: letters/digits/_/-, the --save\n"
        "                    charset; management flags are standalone — one\n"
        "                    per run, no game-run flags)\n"
        "  --demo <file>     V3-9: replay the demo file (.dmo) — the world\n"
        "                    start (a fresh seed or a save slot) + the frame\n"
        "                    dt sequence + the input event stream are the\n"
        "                    ONLY time source (deterministic replay); the\n"
        "                    audience's mouse orbits the camera (render-)\n"
        "                    only, the wheel cycles the speed (0.5/1/2),\n"
        "                    C cycles the view, ESC = the pause menu (Q\n"
        "                    quits from it); composes with --frames\n"
        "                    (the truncated replay) and --save (the exit\n"
        "                    write of the replay's result)\n"
        "  --demo-force    v3.13.0: with --demo, bypass the SIM version\n"
        "                    gate (the demo's recorded simulation-\n"
        "                    semantics version != this build's): the\n"
        "                    replay runs anyway (a WARN is printed; the\n"
        "                    result may be wrong if the simulation\n"
        "                    differs) — the schema's strictness is kept\n"
        "  --demo-author <t> V3-9: expand the JSON action timeline <t> to a\n"
        "                    .dmo (the input's <stem>.dmo) and exit —\n"
        "                    STANDALONE headless (the save management's\n"
        "                    twin; the game never boots)\n"
        "  --demo-record [o] V3-9: record the session to a .dmo (the input\n"
        "                    events + the per-frame dts; o = the output\n"
        "                    path, default data/demos/rec_<ts>.dmo) and save\n"
        "                    it on exit (the window close / the ESC exit)\n"
        "  --list-demos      V3-16a: list the guide demo's index (the\n"
        "                    category/seq/mode/tags/file/title/length) +\n"
        "                    run the shipped content's cross-check —\n"
        "                    STANDALONE headless (the save management's\n"
        "                    twin)\n"
        "  --panorama <out>  V3-20: generate the menu's background strip\n"
        "                    (the config's seed's 360-degree view from the\n"
        "                    spawn's eye, 8 views stitched — the one-shot,\n"
        "                    no loop / no save)\n"
        "  --menu-open <p>   V3-21: open the menu at the page (title/login/\n"
        "                    multiplayer/settings/tutorial — the demo's\n"
        "                    return's deep-link)\n"
        "  --log-level <l>   trace|debug|info|warn|error\n"
        "  -h, --help        Show this help\n");
}

bool parseLogLevel(const std::string& s, LogLevel& out) {
    if (s == "trace") out = LogLevel::Trace;
    else if (s == "debug") out = LogLevel::Debug;
    else if (s == "info") out = LogLevel::Info;
    else if (s == "warn") out = LogLevel::Warn;
    else if (s == "error") out = LogLevel::Error;
    else return false;
    return true;
}

bool parseArgs(int argc, char** argv, GameArgs& args, std::string& err) {
    auto nextValue = [&](int& i, const char* name) -> const char* {
        if (i + 1 >= argc) {
            err = std::string("missing value for ") + name;
            return nullptr;
        }
        return argv[++i];
    };
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--config") {
            const char* v = nextValue(i, "--config");
            if (!v) return false;
            args.configPath = v;
        } else if (arg == "--seed") {
            const char* v = nextValue(i, "--seed");
            if (!v) return false;
            args.seed = std::atoll(v);
        } else if (arg == "--time") {
            const char* v = nextValue(i, "--time");
            if (!v) return false;
            args.time = std::atof(v);
            if (args.time < 0.0 || args.time >= 1.0) {
                err = "--time must be in [0, 1)";
                return false;
            }
        } else if (arg == "--weather") {
            const char* v = nextValue(i, "--weather");
            if (!v) return false;
            // V2-4: the closed word (the --log-level parse discipline —
            // the unknown value is named and rejected): "clear" / "rain".
            auto w = parseWeather(v);
            if (!w.ok()) {
                err = w.error().message;
                return false;
            }
            args.weather = *w;
        } else if (arg == "--explode") {
            // V3-14a: the blast's gate seam (--explode <x> <y> <z>
            // [radius]) — the strict finite-read discipline (a non-
            // finite / short argument is named, not silently zeroed).
            const char* vx = nextValue(i, "--explode");
            if (!vx) return false;
            const char* vy = nextValue(i, "--explode");
            if (!vy) return false;
            const char* vz = nextValue(i, "--explode");
            if (!vz) return false;
            char* ex = nullptr;
            char* ey = nullptr;
            char* ez = nullptr;
            const double x = std::strtod(vx, &ex);
            const double y = std::strtod(vy, &ey);
            const double z = std::strtod(vz, &ez);
            if (*ex != '\0' || *ey != '\0' || *ez != '\0' ||
                !std::isfinite(x) || !std::isfinite(y) ||
                !std::isfinite(z)) {
                err = "--explode takes three finite coordinates "
                      "(<x> <y> <z> [radius])";
                return false;
            }
            ExplosionSpec spec;
            spec.center = Vec3 {static_cast<float>(x),
                                static_cast<float>(y),
                                static_cast<float>(z)};
            // The optional radius (absent = the TNT-class default;
            // present = strict finite + > 0).
            if (i + 1 < argc && std::string(argv[i + 1]) != "--" &&
                argv[i + 1][0] != '-') {
                const char* vr = nextValue(i, "--explode");
                char* er = nullptr;
                const double r = std::strtod(vr, &er);
                if (*er != '\0' || !std::isfinite(r) || r <= 0.0) {
                    err = "--explode's radius must be a finite number "
                          "> 0";
                    return false;
                }
                spec.radius = r;
            }
            args.explode = spec;
        } else if (arg == "--dragon-damage") {
            // V3-10d: the boss's gate seam (--dragon-damage <n>) — the
            // strict finite-read discipline (a non-finite / short /
            // non-positive argument is named, not silently zeroed).
            const char* vn = nextValue(i, "--dragon-damage");
            if (!vn) {
                return false;
            }
            char* en = nullptr;
            const double n = std::strtod(vn, &en);
            if (*en != '\0' || !std::isfinite(n) || n <= 0.0) {
                err = "--dragon-damage takes a finite number > 0";
                return false;
            }
            args.dragonDamage = n;
        } else if (arg == "--frames") {
            const char* v = nextValue(i, "--frames");
            if (!v) return false;
            args.frames = std::atoi(v);
            if (args.frames < 0) {
                err = "--frames must be >= 0";
                return false;
            }
        } else if (arg == "--screenshot") {
            const char* v = nextValue(i, "--screenshot");
            if (!v) return false;
            args.screenshotPath = v;
        } else if (arg == "--save") {
            const char* v = nextValue(i, "--save");
            if (!v) return false;
            const std::string slot = v;
            if (slot.empty()) {
                err = "--save requires a slot name";
                return false;
            }
            for (const char c : slot) {
                const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                                (c >= '0' && c <= '9') || c == '_' || c == '-';
                if (!ok) {
                    err = "--save slot may only contain letters, digits, "
                          "'_' or '-' (got '" +
                          std::string(1, c) + "')";
                    return false;
                }
            }
            args.saveSlot = std::move(slot);
        } else if (arg == "--new") {
            args.newWorld = true;
        } else if (arg == "--save-full-diff") {
            args.saveFullDiff = true;
        } else if (arg == "--list-saves") {
            args.listSaves = true;
        } else if (arg == "--copy-save") {
            const char* src = nextValue(i, "--copy-save");
            if (!src) return false;
            const char* dst = nextValue(i, "--copy-save <dst>");
            if (!dst) return false;
            args.copySave = {src, dst};
        } else if (arg == "--rename-save") {
            const char* src = nextValue(i, "--rename-save");
            if (!src) return false;
            const char* dst = nextValue(i, "--rename-save <dst>");
            if (!dst) return false;
            args.renameSave = {src, dst};
        } else if (arg == "--delete-save") {
            const char* v = nextValue(i, "--delete-save");
            if (!v) return false;
            args.deleteSave = v;
        } else if (arg == "--demo") {
            const char* v = nextValue(i, "--demo");
            if (!v) return false;
            args.demo = v;
        } else if (arg == "--demo-force") {
            args.demoForce = true;
        } else if (arg == "--list-demos") {
            args.listDemos = true;
        } else if (arg == "--demo-author") {
            const char* v = nextValue(i, "--demo-author");
            if (!v) return false;
            args.demoAuthor = v;
        } else if (arg == "--demo-record") {
            // V3-9: the OPTIONAL value — the next argument that is not
            // another flag is the output path (the default = the
            // data/demos/rec_<ts>.dmo the epilogue names).
            args.demoRecord = true;
            if (i + 1 < argc && argv[i + 1][0] != '\0' &&
                argv[i + 1][0] != '-') {
                args.demoRecordOut = argv[++i];
            }
        } else if (arg == "--panorama") {
            // V3-20: the menu's background strip's generator (the
            // management's one-shot: the fresh world's 360-degree view,
            // the stitch, the write, the exit — no loop / no save).
            const char* v = nextValue(i, "--panorama");
            if (!v) return false;
            args.panoramaOut = v;
        } else if (arg == "--menu-open") {
            // V3-21: the menu's deep-link (the closed vocabulary — the
            // named reject's belt; the demo's return's handoff page).
            const char* v = nextValue(i, "--menu-open");
            if (!v) return false;
            if (!menuOpenPage(v).has_value()) {
                err = "invalid --menu-open: '" + std::string(v) +
                      "' (title/login/multiplayer/settings/tutorial)";
                return false;
            }
            args.menuOpen = v;
        } else if (arg == "--listen") {
            // V3-12b: the net HOST (the --port's fixed shape:
            // --listen <port>, the 12b's LAN loopback gate's form).
            const char* v = nextValue(i, "--listen");
            if (!v) {
                return false;
            }
            const long port = std::strtol(v, nullptr, 10);
            if (port < 1 || port > 65535) {
                err = "invalid --listen port: '" + std::string(v) +
                      "' (1-65535)";
                return false;
            }
            args.listenPort = static_cast<std::uint16_t>(port);
        } else if (arg == "--connect") {
            // V3-12b: the net CLIENT (--connect <host:port>).
            const char* v = nextValue(i, "--connect");
            if (!v) {
                return false;
            }
            const std::string spec(v);
            const auto colon = spec.rfind(':');
            if (colon == std::string::npos || colon == 0 ||
                colon + 1 >= spec.size()) {
                err = "invalid --connect: '" + spec +
                      "' (use <host:port>)";
                return false;
            }
            args.connectHost = spec.substr(0, colon);
            const long port = std::strtol(spec.c_str() + colon + 1,
                                          nullptr, 10);
            if (port < 1 || port > 65535) {
                err = "invalid --connect port: '" + spec + "' (1-65535)";
                return false;
            }
            args.connectPort = static_cast<std::uint16_t>(port);
        } else if (arg == "--script") {
            // V3-12b: the net's deterministic input source (the demo
            // timeline's fourth consumer — the gate arm's zero-
            // xdotool input).
            const char* v = nextValue(i, "--script");
            if (!v) {
                return false;
            }
            args.script = v;
        } else if (arg == "--log-level") {
            const char* v = nextValue(i, "--log-level");
            if (!v) return false;
            if (!parseLogLevel(v, args.logLevel)) {
                err = "invalid --log-level: '" + std::string(v) +
                      "' (use trace|debug|info|warn|error)";
                return false;
            }
        } else if (arg == "-h" || arg == "--help") {
            printUsage();
            std::exit(0);
        } else {
            err = "unknown argument: " + arg;
            return false;
        }
    }
    // V3-12b: the net host's --new is the world shape's flag (the
    // WELCOME's fresh spec — the save form is the 12c's face), so the
    // --new x --save requirement is the solo's (a --listen run's fresh
    // world needs no slot).
    if (args.newWorld && !args.saveSlot.has_value() &&
        args.listenPort == 0) {
        err = "--new requires --save <slot> (or the --listen's net "
              "fresh-world shape)";
        return false;
    }
    // SAVE-3: the save management flags are STANDALONE — at most one per
    // run, and never combined with a flag that starts the game.
    const int managementCount =
        (args.listSaves ? 1 : 0) + (args.copySave ? 1 : 0) +
        (args.renameSave ? 1 : 0) + (args.deleteSave ? 1 : 0);
    if (managementCount > 1) {
        err = "the save management flags (--list-saves / --copy-save / "
              "--rename-save / --delete-save) are standalone: one per run";
        return false;
    }
    if (managementCount == 1 && (args.saveSlot.has_value() ||
                                 args.newWorld || args.frames != 0 ||
                                 !args.screenshotPath.empty() ||
                                 args.time >= 0.0 || args.seed >= 0 ||
                                 args.weather.has_value() ||
                                 args.explode.has_value() ||
                                 args.dragonDamage.has_value() ||
                                 args.saveFullDiff)) {
        err = "save management runs without a game: drop the game-run "
              "flags (--save / --new / --frames / --screenshot / --time / "
              "--seed / --weather / --explode / --dragon-damage / --save-full-diff)";
        return false;
    }
    // V3-9: the demo's three flags are MUTUALLY EXCLUSIVE (one per
    // run — the replay / the author / the record are different entry
    // points into one injection kernel).
    const int demoCount = (args.demo.empty() ? 0 : 1) +
                          (args.demoAuthor.empty() ? 0 : 1) +
                          (args.demoRecord ? 1 : 0);
    if (demoCount > 1) {
        err = "--demo / --demo-author / --demo-record are mutually "
              "exclusive (one per run)";
        return false;
    }
    // V3-12b: the NET family's exclusions (the demo family's twin —
    // one entry point per run; the script is the net's input source,
    // never standalone; the client never writes a save, the R7's
    // named face; the 12b's world shape is the FRESH one — the
    // host's --save is the 12c's reconnect snapshot's reserved form).
    const int netCount = (args.listenPort != 0 ? 1 : 0) +
                         (args.connectHost.empty() ? 0 : 1);
    if (netCount > 1) {
        err = "--listen / --connect are mutually exclusive (one per "
              "run — the host OR the client, never both)";
        return false;
    }
    if (netCount == 1 && demoCount > 0) {
        err = "the net family (--listen / --connect) is mutually "
              "exclusive with the demo family (--demo / --demo-author "
              "/ --demo-record — the replay's audience Input is the "
              "net's relay's distinct consumer)";
        return false;
    }
    if (!args.script.empty() && netCount == 0) {
        err = "--script requires --listen or --connect (the net's "
              "deterministic input source — the solo's input is the "
              "live pump)";
        return false;
    }
    if (!args.connectHost.empty() && args.saveSlot.has_value()) {
        err = "--connect x --save is rejected (the R7's client-never-"
              "writes — the save is the host's world state; the "
              "client's exit is the disconnect, not the write)";
        return false;
    }
    // The host's --save composes (the R7's host single save — the
    // exit's world write; the SAVE-FORM WELCOME (joining a loaded
    // world) is the handshake's named reject, the 12c's reconnect
    // snapshot's reserved face).
    // v3.13.0: --demo-force is the sim-mismatch ESCAPE of the --demo
    // replay (it needs the replay entry point to be present).
    if (args.demoForce && args.demo.empty()) {
        err = "--demo-force requires --demo (it bypasses the replay's "
              "sim version gate only)";
        return false;
    }
    // V3-16a: --list-demos is STANDALONE headless (the index's parse +
    // the shipped content's cross-check — the save management's twin).
    if (args.listDemos &&
        (args.frames != 0 || args.saveSlot.has_value() ||
         !args.screenshotPath.empty() || args.newWorld ||
         args.time >= 0.0 || args.seed >= 0 ||
         args.weather.has_value() || args.explode.has_value() ||
         args.saveFullDiff || !args.demoAuthor.empty() ||
         !args.demo.empty() || args.demoRecord ||
         args.listenPort != 0 || !args.connectHost.empty() ||
         !args.script.empty())) {
        err = "--list-demos runs headless: drop the game-run flags";
        return false;
    }
    // V3-20: --panorama runs the world ONCE (the strip's render from
    // the config's seed — the menu's background generator). It is the
    // game's boot (window + renderer + scene) minus the loop, so every
    // game-run flag is rejected (the config is the world's authority).
    if (!args.panoramaOut.empty() &&
        (args.frames != 0 || args.saveSlot.has_value() ||
         !args.screenshotPath.empty() || args.newWorld ||
         args.time >= 0.0 || args.seed >= 0 ||
         args.weather.has_value() || args.explode.has_value() ||
         args.saveFullDiff || !args.demoAuthor.empty() ||
         !args.demo.empty() || args.demoRecord ||
         args.listenPort != 0 || !args.connectHost.empty() ||
         !args.script.empty() || args.listDemos)) {
        err = "--panorama runs the world once (the strip's render): "
              "drop the game-run flags";
        return false;
    }
    // V3-21: --menu-open is the MENU's deep-link — every flag that
    // BYPASSES the menu (the five forms) is its named reject (the
    // page would be dead state).
    if (!args.menuOpen.empty() &&
        (!args.demo.empty() || args.demoRecord || !args.demoAuthor.empty() ||
         args.listenPort != 0 || !args.connectHost.empty() ||
         !args.script.empty() || args.newWorld ||
         !args.panoramaOut.empty() || args.listDemos)) {
        err = "--menu-open opens the menu: drop the menu-bypass flags "
              "(--demo / --demo-record / --demo-author / --listen / "
              "--connect / --script / --new / --panorama / --list-demos)";
        return false;
    }
    // V3-9: --demo-author is STANDALONE headless (the timeline expands
    // to the .dmo, the game never boots — the save management's twin).
    if (!args.demoAuthor.empty() &&
        (args.frames != 0 || args.saveSlot.has_value() ||
         !args.screenshotPath.empty() || args.newWorld ||
         args.time >= 0.0 || args.seed >= 0 ||
         args.weather.has_value() || args.explode.has_value() ||
         args.saveFullDiff)) {
        err = "--demo-author runs headless: drop the game-run flags "
              "(--frames / --save / --screenshot / --new / --time / "
              "--seed / --weather / --explode / --save-full-diff)";
        return false;
    }
    // V3-9: the --demo replay's world spec is the start-point
    // AUTHORITY — the CLI's --seed / --new are redundant (the spec's
    // fresh seed is applied in main; the fresh spec ignores an
    // existing slot by construction). --save composes (the fresh
    // spec's EXIT write of the replay's result) and must match the
    // save spec's slot (checked in main, the spec is known there).
    if (!args.demo.empty() && (args.seed >= 0 || args.newWorld ||
                               args.explode.has_value())) {
        err = "--demo's world spec is the start-point authority: drop "
              "--seed / --new / --explode (the replay's input is the "
              "demo's — the blast's seam has no demo form)";
        return false;
    }
    return true;
}

// SAVE-3: run the (single) save-slot management operation headless on
// data/saves, print the result, and return true (exit 0) / false
// (exit 1, the naming on stderr). No world, no window — the game never
// boots for a management run.
bool runSaveManagement(const GameArgs& args) {
    const std::filesystem::path dir = "data/saves";
    if (args.listSaves) {
        auto names = listSaveSlots(dir);
        if (!names.ok()) {
            std::fprintf(stderr, "Save management error: %s\n",
                         names.error().message.c_str());
            return false;
        }
        std::printf("saves in %s:\n", dir.string().c_str());
        if (names->empty()) {
            std::printf("  (none)\n");
            return true;
        }
        for (const auto& name : *names) {
            auto summary = readSaveSlotSummary(dir, name);
            if (!summary.ok()) {
                std::fprintf(stderr, "Save management error: %s\n",
                             summary.error().message.c_str());
                return false;
            }
            const SaveSlotSummary& s = *summary;
            if (!s.readable) {
                std::printf("  %-16s UNREADABLE: %s\n", s.name.c_str(),
                            s.problem.c_str());
            } else {
                std::printf("  %-16s fmt %3d seed %11lld day %5.2f pos "
                            "(%4.0f,%3.0f,%4.0f) hp %5.1f\n",
                            s.name.c_str(), s.format,
                            static_cast<long long>(s.seed), s.time,
                            s.posX, s.posY, s.posZ, s.health);
            }
        }
        return true;
    }
    const bool isCopy = args.copySave.has_value();
    const std::pair<std::string, std::string>* pair =
        isCopy ? &*args.copySave
               : (args.renameSave ? &*args.renameSave : nullptr);
    std::string single;
    if (args.deleteSave) {
        single = *args.deleteSave;
    }
    if (pair) {
        auto r = isCopy ? copySaveSlot(dir, pair->first, pair->second)
                        : renameSaveSlot(dir, pair->first, pair->second);
        if (!r.ok()) {
            std::fprintf(stderr, "Save management error: %s\n",
                         r.error().message.c_str());
            return false;
        }
        std::printf("%s save slot '%s' -> '%s'\n",
                    isCopy ? "copied" : "renamed", pair->first.c_str(),
                    pair->second.c_str());
        return true;
    }
    auto r = deleteSaveSlot(dir, single);
    if (!r.ok()) {
        std::fprintf(stderr, "Save management error: %s\n",
                     r.error().message.c_str());
        return false;
    }
    std::printf("deleted save slot '%s'\n", single.c_str());
    return true;
}

} // namespace

int main(int argc, char** argv) {
    GameArgs args;
    std::string err;
    if (!parseArgs(argc, argv, args, err)) {
        std::fprintf(stderr, "Argument error: %s\n", err.c_str());
        printUsage();
        return 2;
    }

    // Runtime directories (logs, saves).
    std::error_code ec;
    std::filesystem::create_directories("logs", ec);
    std::filesystem::create_directories("data", ec);

    Logger::instance().init("logs/tokenmine.log", args.logLevel);
    MC_LOG_INFO("=== TokenMine.cpp v" TM_VERSION " ===");

    // SAVE-3: a save management run (--list-saves / --copy-save /
    // --rename-save / --delete-save) operates on data/saves headless and
    // exits here — the game never boots.
    if (args.listSaves || args.copySave || args.renameSave ||
        args.deleteSave) {
        Logger::instance().shutdown();
        return runSaveManagement(args) ? 0 : 1;
    }

    // V3-9: the --demo-author STANDALONE headless run (the timeline
    // expands to the .dmo — the save management's twin, the game never
    // boots; the author uses no SDL / GL, the zlib container only).
    if (!args.demoAuthor.empty()) {
        auto out = authorDemoFromFile(args.demoAuthor);
        if (!out.ok()) {
            std::fprintf(stderr, "Demo author error: %s\n",
                         out.error().message.c_str());
            return 1;
        }
        std::printf("wrote demo '%s' (from timeline '%s')\n",
                    out->string().c_str(),
                    args.demoAuthor.string().c_str());
        Logger::instance().shutdown();
        return 0;
    }

    // V3-16a: the --list-demos STANDALONE headless run (the index's
    // parse + the cross-check + the table — the game never boots).
    if (args.listDemos) {
        auto index = loadDemoIndexChecked();
        if (!index.ok()) {
            std::fprintf(stderr, "Demo index error: %s\n",
                         index.error().message.c_str());
            return 1;
        }
        const auto& demos = index->demos;
        std::printf("demo index (assets/demos/index.json) — %zu entr%s"
                    " (sim %d):\n",
                    demos.size(), demos.size() == 1 ? "y" : "ies",
                    kDemoSimVersion);
        for (const auto& cat : kDemoCategories) {
            std::vector<const DemoIndexEntry*> rows;
            for (const auto& e : demos) {
                if (e.category == cat.id) {
                    rows.push_back(&e);
                }
            }
            std::sort(rows.begin(), rows.end(),
                      [](const DemoIndexEntry* a, const DemoIndexEntry* b) {
                          return a->seq < b->seq;
                      });
            std::printf("  [%s %s]\n", cat.id, cat.title);
            if (rows.empty()) {
                std::printf("    (empty — the later face)\n");
                continue;
            }
            for (const auto* e : rows) {
                const int total = e->lengthFrames;
                const int mm = total / 3600;
                const int ss = (total % 3600) / 60;
                char timebuf[16];
                std::snprintf(timebuf, sizeof(timebuf), "%02d:%02d", mm, ss);
                std::string tags;
                for (const auto& t : e->tags) {
                    if (!tags.empty()) {
                        tags += ",";
                    }
                    tags += t;
                }
                std::printf("    #%-2d %-24s %-20s %6df (%s)  %-4s  tags: %s\n",
                            e->seq, e->file.c_str(),
                            e->title.empty() ? "-" : e->title.c_str(),
                            e->lengthFrames,
                            timebuf, e->mode.c_str(),
                            tags.empty() ? "-" : tags.c_str());
            }
        }
        std::printf("cross-check: OK (the files' loads + the SIM's gate "
                    "+ the lengths' match + the scenes' existence)\n");
        Logger::instance().shutdown();
        return 0;
    }

    // V3-9: the --demo replay (the file LOADS + PARSES headless — the
    // strict schema + the version fingerprint reject before the window
    // boots; the world spec is the start-point authority).
    if (!args.demo.empty()) {
        auto demo = loadDemoFile(args.demo, args.demoForce);
        if (!demo.ok()) {
            std::fprintf(stderr, "Demo error: %s\n",
                         demo.error().message.c_str());
            return 1;
        }
        if (args.demoForce && demo->sim != kDemoSimVersion) {
            std::fprintf(stderr,
                         "Demo warning: --demo-force: demo sim %d != "
                         "this build sim %d (recorded with %s) — the "
                         "simulation semantics differ, the replay "
                         "result may be wrong\n",
                         demo->sim, kDemoSimVersion,
                         demo->recordedWith.c_str());
        }
        const DemoFile& d = *demo;
        if (d.world.type == DemoWorldSpec::Type::Fresh) {
            args.seed = d.world.seed;
            // The fresh spec IGNORES an existing slot at start (the
            // start-point authority — the PITS 15 second-run load is
            // out by construction; the --save, when given, stays the
            // EXIT write of the replay's result).
            args.newWorld = true;
        } else if (d.world.type == DemoWorldSpec::Type::Save) {
            if (args.saveSlot.has_value() &&
                *args.saveSlot != d.world.slot) {
                std::fprintf(stderr,
                             "Argument error: --demo's world references "
                             "save slot '%s' (drop --save or match it)\n",
                             d.world.slot.c_str());
                return 2;
            }
            args.saveSlot = d.world.slot;
        }
        // The SCENE spec (V3-16a): the scene's name is NOT a save
        // slot (the committed asset, the read-only) — the CLI's
        // --save composes like the fresh spec (the EXIT write of
        // the replay's result).

        args.demoFile = std::move(demo).value();
    }

    // V3-12b: the --script's headless parse (the --demo's twin — the
    // timeline's validation BEFORE the game boots; the net's WELCOME
    // is the world spec's authority, the script's world key rides
    // along ignored).
    if (!args.script.empty()) {
        auto script = parseScriptTimeline(args.script);
        if (!script.ok()) {
            std::fprintf(stderr, "Script error: %s\n",
                         script.error().message.c_str());
            return 1;
        }
        args.scriptFile = std::move(script).value();
        std::fprintf(stderr,
                     "Script timeline: '%s' (%d frames, %zu events)\n",
                     args.script.string().c_str(),
                     static_cast<int>(args.scriptFile->dts.size()),
                     args.scriptFile->events.size());
    }

    Game game;
    auto st = game.init(args);
    if (!st.ok()) {
        MC_LOG_ERROR("Initialization failed: " + st.error().describe());
        return 1;
    }

    const int rc = game.run();
    Logger::instance().shutdown();
    return rc;
}
