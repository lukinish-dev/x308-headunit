#pragma once

#include "x308/Models.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace x308 {

class IMediaPlayer {
public:
    virtual ~IMediaPlayer() = default;
    [[nodiscard]] virtual MediaStatus status() = 0;
    [[nodiscard]] virtual Result play() = 0;
    [[nodiscard]] virtual Result pause() = 0;
    [[nodiscard]] virtual Result stop() = 0;
    [[nodiscard]] virtual Result togglePause() = 0;
    [[nodiscard]] virtual Result next() = 0;
    [[nodiscard]] virtual Result previous() = 0;
    [[nodiscard]] virtual std::vector<Track> queue() = 0;
    [[nodiscard]] virtual Result clearQueue() = 0;
    [[nodiscard]] virtual std::vector<LibraryEntry> library(std::string_view path) = 0;
    [[nodiscard]] virtual Result add(std::string_view path) = 0;
    [[nodiscard]] virtual Result addFolder(std::string_view path) = 0;
    [[nodiscard]] virtual Result setRandom(bool enabled) = 0;
    [[nodiscard]] virtual Result setRepeat(bool enabled) = 0;
    [[nodiscard]] virtual Result update() = 0;
    [[nodiscard]] virtual Result activateAudio() = 0;
    [[nodiscard]] virtual Result releaseAudio() = 0;
    [[nodiscard]] virtual std::string lastError() const = 0;
};

class IBluetoothManager {
public:
    virtual ~IBluetoothManager() = default;
    [[nodiscard]] virtual BluetoothStatus status() = 0;
    [[nodiscard]] virtual Result setPower(bool enabled) = 0;
    [[nodiscard]] virtual Result startScan() = 0;
    [[nodiscard]] virtual Result stopScan() = 0;
    [[nodiscard]] virtual std::vector<BluetoothDevice> devices() = 0;
    [[nodiscard]] virtual Result pair(std::string_view mac) = 0;
    [[nodiscard]] virtual Result trust(std::string_view mac, bool trusted) = 0;
    [[nodiscard]] virtual Result connect(std::string_view mac) = 0;
    [[nodiscard]] virtual Result disconnect(std::string_view mac) = 0;
    [[nodiscard]] virtual Result remove(std::string_view mac) = 0;
    [[nodiscard]] virtual Result setPairingMode(bool enabled) = 0;
    [[nodiscard]] virtual Result autoConnect() = 0;
    [[nodiscard]] virtual Result activateAudio() = 0;
    [[nodiscard]] virtual Result releaseAudio() = 0;
    [[nodiscard]] virtual std::string lastError() const = 0;
};

class IAudioOutput {
public:
    virtual ~IAudioOutput() = default;
    [[nodiscard]] virtual Result selectSource(AudioSource source) = 0;
    [[nodiscard]] virtual std::string currentDevice() const = 0;
};

class IDspController {
public:
    virtual ~IDspController() = default;
    [[nodiscard]] virtual Result initialize() = 0;
};

class IInputController {
public:
    virtual ~IInputController() = default;
    [[nodiscard]] virtual std::optional<std::string> poll(
        std::chrono::milliseconds timeout) = 0;
};

}  // namespace x308
