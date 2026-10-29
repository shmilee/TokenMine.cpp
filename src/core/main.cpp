// TokenMine.cpp entry point: argument parsing, logging setup, game bootstrap.
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

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
                                 args.saveFullDiff)) {
        err = "save management runs without a game: drop the game-run "
              "flags (--save / --new / --frames / --screenshot / --time / "
              "--seed / --save-full-diff)";
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
