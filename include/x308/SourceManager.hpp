#pragma once

#include "x308/Interfaces.hpp"

namespace x308 {

class SourceManager {
public:
    SourceManager(IMediaPlayer& mpd, IBluetoothManager& bluetooth, IAudioOutput& audioOutput,
                  AudioSource initialSource = AudioSource::mpd);

    [[nodiscard]] Result setSource(AudioSource source);
    [[nodiscard]] AudioSource activeSource() const noexcept;
    [[nodiscard]] static std::string_view name(AudioSource source) noexcept;

private:
    IMediaPlayer& mpd_;
    IBluetoothManager& bluetooth_;
    IAudioOutput& audioOutput_;
    AudioSource activeSource_;
};

}  // namespace x308

