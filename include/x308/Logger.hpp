#pragma once

#include <string_view>

namespace x308 {

enum class LogLevel { debug, info, warning, error };

class Logger {
public:
    static void setLevel(std::string_view level);
    static void log(LogLevel level, std::string_view message);
};

}  // namespace x308

