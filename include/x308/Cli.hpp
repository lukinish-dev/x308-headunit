#pragma once

#include <filesystem>
#include <iosfwd>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace x308 {

class IBluetoothManager;
class IMediaPlayer;
class SourceManager;
class SystemStatusService;

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

class Cli {
public:
    Cli(IMediaPlayer& mediaPlayer, IBluetoothManager& bluetooth,
        SourceManager& sourceManager, SystemStatusService& systemStatus,
        std::ostream& output, std::ostream& error);

    [[nodiscard]] int run(const std::vector<std::string>& command) const;

private:
    IMediaPlayer& mediaPlayer_;
    IBluetoothManager& bluetooth_;
    SourceManager& sourceManager_;
    SystemStatusService& systemStatus_;
    std::ostream& output_;
    std::ostream& error_;
};

}  // namespace x308
