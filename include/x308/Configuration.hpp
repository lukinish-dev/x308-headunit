#pragma once

#include <filesystem>
#include <istream>
#include <optional>
#include <stdexcept>
#include <string>

namespace x308 {

struct ApplicationConfig {
    std::string language{"ru"};
    int startupTimeoutSeconds{10};
};

struct MpdConfig {
    std::string host{"localhost"};
    unsigned port{6600};
    std::filesystem::path musicDirectory{"/mnt/music"};
};

struct BluetoothConfig {
    std::string adapter{"hci0"};
    std::string deviceName{"Jaguar XJR"};
    bool autoConnect{true};
    int discoverableTimeoutSeconds{120};
    bool autoAcceptPairing{true};
};

struct AudioConfig {
    std::string defaultSource{"mpd"};
};

struct LoggingConfig {
    std::string level{"info"};
};

struct Configuration {
    ApplicationConfig application;
    MpdConfig mpd;
    BluetoothConfig bluetooth;
    AudioConfig audio;
    LoggingConfig logging;
    std::optional<std::filesystem::path> loadedFrom;
};

class ConfigurationError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class ConfigurationLoader {
public:
    [[nodiscard]] static Configuration load(
        const std::optional<std::filesystem::path>& explicitPath = std::nullopt);
    [[nodiscard]] static Configuration parse(std::istream& input);
};

}  // namespace x308

