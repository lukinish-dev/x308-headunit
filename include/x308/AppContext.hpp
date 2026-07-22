#pragma once

#include <memory>

namespace x308 {

class BluetoothCtlManager;
class Cli;
struct Configuration;
class InteractiveMenu;
class Logger;
class MpdClient;
class NullAudioOutput;
class PosixProcessRunner;
class SourceManager;
class SystemStatusService;

struct AppContext {
    AppContext() = default;
    ~AppContext();

    AppContext(const AppContext&) = delete;
    AppContext& operator=(const AppContext&) = delete;

    std::unique_ptr<Configuration> configuration;
    std::unique_ptr<Logger> logger;
    std::shared_ptr<PosixProcessRunner> processRunner;
    std::unique_ptr<MpdClient> mpd;
    std::unique_ptr<BluetoothCtlManager> bluetooth;
    std::unique_ptr<NullAudioOutput> audioOutput;
    std::unique_ptr<SourceManager> sourceManager;
    std::unique_ptr<SystemStatusService> systemStatus;
    std::unique_ptr<Cli> cli;
    std::unique_ptr<InteractiveMenu> interactiveMenu;
};

}  // namespace x308
