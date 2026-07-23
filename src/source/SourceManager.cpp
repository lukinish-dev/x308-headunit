#include "x308/SourceManager.hpp"

#include "x308/Logger.hpp"

namespace x308 {
namespace {

Result withContext(const Result& result, const std::string_view context) {
    if (result.success) {
        return result;
    }
    return Result::error(std::string{context} + ": " + result.message);
}

}  // namespace

SourceManager::SourceManager(IMediaPlayer& mpd, IAudioOutput& audioOutput,
                             const AudioSource initialSource,
                             IBluetoothMediaController* bluetoothMedia, Logger* logger)
    : mpd_(mpd),
      audioOutput_(audioOutput),
      bluetoothMedia_(bluetoothMedia),
      logger_(logger),
      activeSource_(initialSource) {}

Result SourceManager::setSource(const AudioSource source) {
    const std::lock_guard lock(mutex_);
    return setSourceLocked(source);
}

Result SourceManager::prepareForPlayback(const AudioSource source,
                                         const std::string_view reason) {
    const std::lock_guard lock(mutex_);
    if (source != AudioSource::mpd) {
        return Result::error("Only MPD playback can be prepared before it starts");
    }
    if (source == activeSource_ || preparedSource_ == source) {
        return Result::ok("MPD audio is already ready");
    }
    if (activeSource_ != AudioSource::bluetooth) {
        return Result::error("Cannot prepare MPD from the current source");
    }
    if (bluetoothMedia_ != nullptr) {
        const auto pause = bluetoothMedia_->pause();
        if (!pause.success && logger_ != nullptr) {
            logger_->log(LogLevel::warning,
                         "Bluetooth AVRCP pause failed before MPD playback: " + pause.message);
        }
    }
    const auto result = prepareMpdAudioLocked();
    if (!result.success) return result;
    preparedSource_ = source;
    if (logger_ != nullptr) {
        logger_->log(LogLevel::info,
                     "MPD audio prepared before playback; reason: " + std::string{reason});
    }
    return Result::ok("MPD audio prepared");
}

Result SourceManager::cancelPreparedPlayback(const AudioSource source) {
    const std::lock_guard lock(mutex_);
    if (preparedSource_ != source) return Result::ok();
    if (const auto release = mpd_.releaseAudio(); !release.success) {
        return withContext(release, "Cannot release prepared MPD audio");
    }
    if (const auto restore = audioOutput_.selectSource(activeSource_); !restore.success) {
        return withContext(restore, "Cannot restore previous audio source");
    }
    preparedSource_.reset();
    return Result::ok("Prepared playback cancelled");
}

Result SourceManager::onPlaybackStarted(const AudioSource source, const std::string_view reason) {
    const std::lock_guard lock(mutex_);
    if (source == AudioSource::carPlay) {
        return Result::error("CarPlay is not implemented");
    }
    if (source == activeSource_) {
        if (logger_ != nullptr) {
            logger_->log(LogLevel::debug,
                         "Playback source unchanged: " + std::string{name(source)} +
                             "; reason: " + std::string{reason});
        }
        return Result::ok("Playback source is already active");
    }

    const auto previous = activeSource_;
    if (preparedSource_ == source) {
        preparedSource_.reset();
        activeSource_ = source;
        if (logger_ != nullptr) {
            logger_->log(LogLevel::info,
                         "Playback source changed: " + std::string{name(previous)} + " -> " +
                             std::string{name(source)} + "; reason: " + std::string{reason});
        }
        return Result::ok("Source changed to " + std::string{name(source)});
    }
    if (source == AudioSource::mpd && previous == AudioSource::bluetooth &&
        bluetoothMedia_ != nullptr) {
        const auto pause = bluetoothMedia_->pause();
        if (!pause.success && logger_ != nullptr) {
            logger_->log(LogLevel::warning,
                         "Bluetooth AVRCP pause failed during source switch: " + pause.message);
        }
    }

    const auto result = setSourceLocked(source);
    if (result.success && logger_ != nullptr) {
        logger_->log(LogLevel::info,
                     "Playback source changed: " + std::string{name(previous)} + " -> " +
                         std::string{name(source)} + "; reason: " + std::string{reason});
    }
    return result;
}

Result SourceManager::setSourceLocked(const AudioSource source) {
    if (source == AudioSource::carPlay) {
        return Result::error("CarPlay is not implemented");
    }
    if (source == activeSource_) {
        return Result::ok("Source is already active");
    }

    switch (source) {
        case AudioSource::mpd:
            if (const auto result = prepareMpdAudioLocked(); !result.success) return result;
            break;
        case AudioSource::bluetooth:
            if (const auto result = mpd_.pause(); !result.success) {
                return withContext(result, "Cannot pause MPD");
            }
            if (const auto result = mpd_.releaseAudio(); !result.success) {
                return withContext(result, "Cannot release MPD audio");
            }
            if (const auto result = audioOutput_.selectSource(source); !result.success) {
                const auto rollback = mpd_.activateAudio();
                if (!rollback.success) {
                    return Result::error("Partial source switch failure: Bluetooth audio activation failed: " +
                                         result.message + "; MPD output rollback failed: " +
                                         rollback.message);
                }
                return withContext(result, "Cannot activate Bluetooth audio; MPD output restored");
            }
            break;
        case AudioSource::carPlay:
            return Result::error("CarPlay is not implemented");
    }

    activeSource_ = source;
    preparedSource_.reset();
    return Result::ok("Source changed to " + std::string{name(source)});
}

Result SourceManager::prepareMpdAudioLocked() {
    if (const auto result = audioOutput_.selectSource(AudioSource::mpd); !result.success) {
        return withContext(result, "Cannot release Bluetooth audio receiver");
    }
    if (const auto result = mpd_.activateAudio(); !result.success) {
        const auto rollback = audioOutput_.selectSource(AudioSource::bluetooth);
        if (!rollback.success) {
            return Result::error("Partial source switch failure: MPD output activation failed: " +
                                 result.message + "; Bluetooth audio rollback failed: " +
                                 rollback.message);
        }
        return withContext(result, "Cannot activate MPD audio; Bluetooth audio restored");
    }
    return Result::ok();
}

AudioSource SourceManager::activeSource() const {
    const std::lock_guard lock(mutex_);
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
