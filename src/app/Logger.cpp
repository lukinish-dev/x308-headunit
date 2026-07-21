#include "x308/Logger.hpp"

#include <array>
#include <iostream>
#include <mutex>
#include <string>

namespace x308 {
namespace {

LogLevel minimumLevel = LogLevel::info;
std::mutex logMutex;

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

void Logger::setLevel(const std::string_view level) {
    if (level == "debug") minimumLevel = LogLevel::debug;
    else if (level == "warning" || level == "warn") minimumLevel = LogLevel::warning;
    else if (level == "error") minimumLevel = LogLevel::error;
    else minimumLevel = LogLevel::info;
}

void Logger::log(const LogLevel level, const std::string_view message) {
    if (static_cast<int>(level) < static_cast<int>(minimumLevel)) {
        return;
    }
    const std::lock_guard lock(logMutex);
    std::clog << '[' << label(level) << "] " << message << '\n';
}

}  // namespace x308

