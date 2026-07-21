#pragma once

#include <optional>
#include <string>
#include <vector>

namespace x308 {

struct Result {
    bool success{true};
    std::string message;

    [[nodiscard]] static Result ok(std::string message = {}) {
        return {true, std::move(message)};
    }
    [[nodiscard]] static Result error(std::string message) {
        return {false, std::move(message)};
    }
};

enum class AudioSource { mpd, bluetooth, carPlay };

enum class PlaybackState { stopped, playing, paused, unknown };

struct Track {
    std::string uri;
    std::string title;
    std::string artist;
    std::string album;
};

struct MediaStatus {
    bool available{false};
    PlaybackState state{PlaybackState::unknown};
    bool random{false};
    bool repeat{false};
    std::optional<Track> currentTrack;
    std::string error;
};

struct LibraryEntry {
    std::string path;
    bool directory{false};
};

struct BluetoothDevice {
    std::string name;
    std::string mac;
    bool paired{false};
    bool trusted{false};
    bool connected{false};
    bool available{false};
};

struct BluetoothStatus {
    bool serviceAvailable{false};
    bool adapterAvailable{false};
    bool powered{false};
    bool discovering{false};
    bool pairable{false};
    bool discoverable{false};
    std::optional<BluetoothDevice> activeAudioDevice;
    std::string error;
};

}  // namespace x308

