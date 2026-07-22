#include "x308/Logger.hpp"

#include <array>
#include <iostream>
#include <mutex>
#include <string>

namespace x308 {
namespace {

constexpr std::string_view label(const LogLevel level) {
    switch (level) {
        case LogLevel::debug: return "DEBUG";
        case LogLevel::info: return "INFO";
        case LogLevel::warning: return "WARN";
        case LogLevel::error: return "ERROR";
    }
    return "UNKNOWN";
}

}  // namespace

Logger::Logger(const std::string_view level) { setLevel(level); }

void Logger::setLevel(const std::string_view level) {
    const std::lock_guard lock(mutex_);
    if (level == "debug") minimumLevel_ = LogLevel::debug;
    else if (level == "warning" || level == "warn") minimumLevel_ = LogLevel::warning;
    else if (level == "error") minimumLevel_ = LogLevel::error;
    else minimumLevel_ = LogLevel::info;
}

void Logger::log(const LogLevel level, const std::string_view message) {
    const std::lock_guard lock(mutex_);
    if (static_cast<int>(level) < static_cast<int>(minimumLevel_)) {
        return;
    }
    std::clog << '[' << label(level) << "] " << message << '\n';
}

}  // namespace x308
