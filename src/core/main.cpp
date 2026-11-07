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
        "                    data/saves/<slot>.json at startup when present,\n"
        "                    write it on clean exit (opt-in; default = no\n"
        "                    save/load at all)\n"
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
    if (args.newWorld && !args.saveSlot.has_value()) {
        err = "--new requires --save <slot>";
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
    // v3.13.0: --demo-force is the sim-mismatch ESCAPE of the --demo
    // replay (it needs the replay entry point to be present).
    if (args.demoForce && args.demo.empty()) {
        err = "--demo-force requires --demo (it bypasses the replay's "
              "sim version gate only)";
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
        } else {
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
        args.demoFile = std::move(demo).value();
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
