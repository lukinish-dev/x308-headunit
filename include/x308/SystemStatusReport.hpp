#pragma once

#include "x308/Models.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace x308 {

struct ApplicationStatusReport {
    std::string version;
    std::string buildType;
    std::chrono::milliseconds uptime{0};
};

struct SystemStatusDetails {
    std::string hostname;
    std::string kernel;
    std::chrono::seconds uptime{0};
};

struct StorageStatusReport {
    std::filesystem::path path;
    bool present{false};
    bool accessible{false};
    std::optional<std::uintmax_t> freeBytes;
};

struct MpdStatusReport {
    bool available{false};
    PlaybackState state{PlaybackState::unknown};
    AudioSource currentSource{AudioSource::mpd};
    std::optional<std::string> currentTrack;
    std::optional<std::string> artist;
    std::string error;
};

struct BluetoothStatusReport {
    bool serviceAvailable{false};
    bool adapterPresent{false};
    bool adapterPowered{false};
    bool discoverable{false};
    std::optional<std::string> connectedDevice;
    std::optional<std::string> connectedDeviceName;
    std::string error;
};

struct SourceManagerStatusReport {
    AudioSource activeSource{AudioSource::mpd};
};

struct SystemStatusReport {
    ApplicationStatusReport application;
    SystemStatusDetails system;
    StorageStatusReport storage;
    MpdStatusReport mpd;
    BluetoothStatusReport bluetooth;
    SourceManagerStatusReport sourceManager;
    std::chrono::microseconds collectionDuration{0};
    std::vector<std::string> warnings;
};

}  // namespace x308
