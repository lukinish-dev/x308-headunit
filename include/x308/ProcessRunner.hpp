#pragma once

#include <chrono>
#include <string>
#include <string_view>
#include <vector>

namespace x308 {

struct ProcessResult {
    int exitCode{-1};
    bool timedOut{false};
    std::string standardOutput;
    std::string standardError;
};

class IProcessRunner {
public:
    virtual ~IProcessRunner() = default;
    [[nodiscard]] virtual ProcessResult run(
        std::string_view executable, const std::vector<std::string>& arguments,
        std::chrono::milliseconds timeout) = 0;
};

class PosixProcessRunner final : public IProcessRunner {
public:
    [[nodiscard]] ProcessResult run(
        std::string_view executable, const std::vector<std::string>& arguments,
        std::chrono::milliseconds timeout) override;
};

}  // namespace x308

