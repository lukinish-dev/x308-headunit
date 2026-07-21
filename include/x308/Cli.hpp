#pragma once

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace x308 {

struct CliArguments {
    bool help{false};
    bool version{false};
    std::optional<std::filesystem::path> configPath;
    std::vector<std::string> command;
};

class CliError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class CliParser {
public:
    [[nodiscard]] static CliArguments parse(int argc, const char* const* argv);
    [[nodiscard]] static std::string helpText();
};

}  // namespace x308

