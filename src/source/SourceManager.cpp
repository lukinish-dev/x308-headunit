#include "x308/SourceManager.hpp"

namespace x308 {
namespace {

Result withContext(const Result& result, const std::string_view context) {
    if (result.success) {
        return result;
    }
    return Result::error(std::string{context} + ": " + result.message);
}

}  // namespace

SourceManager::SourceManager(IMediaPlayer& mpd, IBluetoothManager& bluetooth,
                             IAudioOutput& audioOutput, const AudioSource initialSource)
    : mpd_(mpd), bluetooth_(bluetooth), audioOutput_(audioOutput), activeSource_(initialSource) {}

Result SourceManager::setSource(const AudioSource source) {
    if (source == AudioSource::carPlay) {
        return Result::error("CarPlay is not implemented");
    }
    if (source == activeSource_) {
        return Result::ok("Source is already active");
    }

    if (source == AudioSource::mpd) {
        if (const auto result = bluetooth_.releaseAudio(); !result.success) {
            return withContext(result, "Cannot release Bluetooth audio");
        }
        if (const auto result = audioOutput_.selectSource(source); !result.success) {
            return withContext(result, "Cannot select MPD output");
        }
        if (const auto result = mpd_.activateAudio(); !result.success) {
            return withContext(result, "Cannot activate MPD audio");
        }
    } else {
        if (const auto result = mpd_.pause(); !result.success) {
            return withContext(result, "Cannot pause MPD");
        }
        if (const auto result = mpd_.releaseAudio(); !result.success) {
            return withContext(result, "Cannot release MPD audio");
        }
        if (const auto result = audioOutput_.selectSource(source); !result.success) {
            return withContext(result, "Cannot select Bluetooth output");
        }
        if (const auto result = bluetooth_.activateAudio(); !result.success) {
            return withContext(result, "Cannot activate Bluetooth audio");
        }
    }

    activeSource_ = source;
    return Result::ok("Source changed to " + std::string{name(source)});
}

AudioSource SourceManager::activeSource() const noexcept {
    return activeSource_;
}

std::string_view SourceManager::name(const AudioSource source) noexcept {
    switch (source) {
        case AudioSource::mpd: return "MPD";
        case AudioSource::bluetooth: return "Bluetooth";
        case AudioSource::carPlay: return "CarPlay";
    }
    return "Unknown";
}

}  // namespace x308

