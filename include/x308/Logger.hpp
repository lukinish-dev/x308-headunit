#pragma once

#include <mutex>
#include <string_view>

namespace x308 {

enum class LogLevel { debug, info, warning, error };

class Logger {
public:
    explicit Logger(std::string_view level = "info");

    void setLevel(std::string_view level);
    void log(LogLevel level, std::string_view message);

private:
    LogLevel minimumLevel_{LogLevel::info};
    std::mutex mutex_;
};

}  // namespace x308
