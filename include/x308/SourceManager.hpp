#pragma once

#include "x308/Interfaces.hpp"

#include <mutex>
#include <optional>
#include <string_view>

namespace x308 {

class Logger;

class SourceManager {
public:
    SourceManager(IMediaPlayer& mpd, IAudioOutput& audioOutput,
                  AudioSource initialSource = AudioSource::mpd,
                  IBluetoothMediaController* bluetoothMedia = nullptr,
                  Logger* logger = nullptr);

    [[nodiscard]] Result setSource(AudioSource source);
    [[nodiscard]] Result prepareForPlayback(AudioSource source, std::string_view reason);
    [[nodiscard]] Result cancelPreparedPlayback(AudioSource source);
    [[nodiscard]] Result onPlaybackStarted(AudioSource source, std::string_view reason);
    [[nodiscard]] AudioSource activeSource() const;
    [[nodiscard]] static std::string_view name(AudioSource source) noexcept;

private:
    [[nodiscard]] Result setSourceLocked(AudioSource source);
    [[nodiscard]] Result prepareMpdAudioLocked();

    IMediaPlayer& mpd_;
    IAudioOutput& audioOutput_;
    IBluetoothMediaController* bluetoothMedia_;
    Logger* logger_;
    mutable std::mutex mutex_;
    AudioSource activeSource_;
    std::optional<AudioSource> preparedSource_;
};

}  // namespace x308
