#pragma once

#include <chrono>
#include <optional>
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
    explicit PosixProcessRunner(
        std::optional<std::chrono::milliseconds> maximumTimeout = std::nullopt,
        std::chrono::milliseconds terminationGrace = std::chrono::milliseconds{100});

    [[nodiscard]] ProcessResult run(
        std::string_view executable, const std::vector<std::string>& arguments,
        std::chrono::milliseconds timeout) override;

private:
    std::optional<std::chrono::milliseconds> maximumTimeout_;
    std::chrono::milliseconds terminationGrace_;
};

}  // namespace x308
