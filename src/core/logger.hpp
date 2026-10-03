// Thread-safe leveled logger. Writes to stdout (colored) and, after init(),
// to a log file. Fatal-level messages abort the process after flushing so
// that errors are never silently swallowed.
#pragma once

#include <atomic>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>

namespace mc {

enum class LogLevel {
    Trace = 0,
    Debug,
    Info,
    Warn,
    Error,
    Fatal
};

class Logger {
public:
    static Logger& instance();

    // Open the log file (directory must exist). Safe to call once at startup.
    void init(const std::filesystem::path& logFile, LogLevel minLevel = LogLevel::Info);
    void shutdown();

    // Cheap, lock-free check so call sites can skip formatting work.
    bool enabled(LogLevel level) const {
        return static_cast<int>(level) >= m_level.load(std::memory_order_relaxed);
    }

    void setLevel(LogLevel level) {
        m_level.store(static_cast<int>(level), std::memory_order_relaxed);
    }
    LogLevel level() const { return static_cast<LogLevel>(m_level.load(std::memory_order_relaxed)); }

    void log(LogLevel level, std::string_view message, const char* file = nullptr, int line = 0);

private:
    Logger() = default;

    static const char* levelName(LogLevel level);
    static const char* levelColor(LogLevel level);

    std::atomic<int> m_level{static_cast<int>(LogLevel::Info)};
    mutable std::mutex m_mutex; // guards m_file only
    std::ofstream m_file;
    std::filesystem::path m_filePath;
    bool m_fileOpen = false;
};

} // namespace mc

#define MC_LOG_AT(level, msg)                                                    \
    do {                                                                         \
        if (mc::Logger::instance().enabled(mc::LogLevel::level)) {               \
            mc::Logger::instance().log(mc::LogLevel::level, (msg), __FILE__,     \
                                       __LINE__);                                \
        }                                                                        \
    } while (0)

#define MC_LOG_TRACE(msg) MC_LOG_AT(Trace, msg)
#define MC_LOG_DEBUG(msg) MC_LOG_AT(Debug, msg)
#define MC_LOG_INFO(msg) MC_LOG_AT(Info, msg)
#define MC_LOG_WARN(msg) MC_LOG_AT(Warn, msg)
#define MC_LOG_ERROR(msg) MC_LOG_AT(Error, msg)
#define MC_LOG_FATAL(msg) MC_LOG_AT(Fatal, msg)
