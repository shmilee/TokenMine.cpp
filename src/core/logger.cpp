#include "core/logger.hpp"

#include <chrono>
#include <cstdio>
#include <ctime>

namespace mc {

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

const char* Logger::levelName(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO ";
        case LogLevel::Warn:  return "WARN ";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Fatal: return "FATAL";
    }
    return "?????";
}

const char* Logger::levelColor(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: return "\x1b[90m";
        case LogLevel::Debug: return "\x1b[36m";
        case LogLevel::Info:  return "\x1b[32m";
        case LogLevel::Warn:  return "\x1b[33m";
        case LogLevel::Error: return "\x1b[31m";
        case LogLevel::Fatal: return "\x1b[1;31m";
    }
    return "";
}

void Logger::init(const std::filesystem::path& logFile, LogLevel minLevel) {
    m_level.store(static_cast<int>(minLevel), std::memory_order_relaxed);
    std::lock_guard lock(m_mutex);
    if (m_file.is_open()) {
        m_file.close();
    }
    m_filePath = logFile;
    m_file.open(logFile, std::ios::out | std::ios::app);
    m_fileOpen = m_file.is_open();
    if (!m_fileOpen) {
        std::fprintf(stderr, "Logger: warning: cannot open log file '%s'\n",
                     logFile.string().c_str());
    }
}

void Logger::shutdown() {
    std::lock_guard lock(m_mutex);
    if (m_file.is_open()) {
        m_file.flush();
        m_file.close();
    }
    m_fileOpen = false;
}

void Logger::log(LogLevel level, std::string_view message, const char* file, int line) {
    if (!enabled(level)) {
        return;
    }

    // Timestamp (wall clock, milliseconds since second).
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    const long ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch())
            .count() %
        1000;
    std::tm tm {};
    localtime_r(&t, &tm);
    char stamp[32];
    std::snprintf(stamp, sizeof(stamp), "%02d:%02d:%02d.%03ld", tm.tm_hour, tm.tm_min,
                  tm.tm_sec, ms);

    // Source location: file basename:line
    std::string loc;
    if (file != nullptr) {
        loc = std::filesystem::path(file).filename().string();
        loc += ":";
        loc += std::to_string(line);
    }

    // Console (always).
    std::fprintf(stdout, "%s%s\x1b[0m %s %s%s%s\n", levelColor(level), levelName(level),
                 stamp, loc.empty() ? "" : loc.c_str(), loc.empty() ? "" : " ",
                 message.data());
    std::fflush(stdout);

    // File (if open).
    {
        std::lock_guard lock(m_mutex);
        if (m_fileOpen && m_file.is_open()) {
            m_file << levelName(level) << ' ' << stamp;
            if (!loc.empty()) {
                m_file << ' ' << loc;
            }
            m_file << ' ' << message << '\n';
        }
    }

    if (level == LogLevel::Fatal) {
        std::abort();
    }
}

} // namespace mc
