// TokenMine.cpp entry point: argument parsing, logging setup, game bootstrap.
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

#include "core/game.hpp"
#include "core/logger.hpp"

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
