#include "x308/PlaybackSourceMonitor.hpp"

#include "x308/Interfaces.hpp"
#include "x308/Logger.hpp"
#include "x308/SourceManager.hpp"

#include <chrono>

namespace x308 {
namespace {

constexpr auto bluetoothStatusPollInterval = std::chrono::milliseconds{300};

}  // namespace

PlaybackSourceMonitor::PlaybackSourceMonitor(IBluetoothMediaController& bluetoothMedia,
                                             SourceManager& sourceManager, Logger* logger,
                                             const bool startMonitoring)
    : bluetoothMedia_(bluetoothMedia), sourceManager_(sourceManager), logger_(logger) {
    if (startMonitoring) {
        worker_ = std::jthread([this](const std::stop_token stopToken) { monitor(stopToken); });
    }
}

PlaybackSourceMonitor::~PlaybackSourceMonitor() {
    worker_.request_stop();
    wake_.notify_all();
    if (worker_.joinable()) worker_.join();
}

void PlaybackSourceMonitor::pollOnce() {
    const auto status = bluetoothMedia_.status();
    const bool isPlaying = status.available && status.state == PlaybackState::playing;
    bool playbackStarted = false;
    {
        const std::lock_guard lock(wakeMutex_);
        playbackStarted = isPlaying && !bluetoothWasPlaying_;
        bluetoothWasPlaying_ = isPlaying;
    }
    if (playbackStarted) {
        const auto result = sourceManager_.onPlaybackStarted(
            AudioSource::bluetooth, "Bluetooth playback started");
        if (!result.success && logger_ != nullptr) {
            logger_->log(LogLevel::warning,
                         "Bluetooth playback source switch failed: " + result.message);
        }
    }
}

void PlaybackSourceMonitor::monitor(const std::stop_token stopToken) {
    std::unique_lock lock(wakeMutex_);
    while (!stopToken.stop_requested()) {
        wake_.wait_for(lock, bluetoothStatusPollInterval, [&stopToken] {
            return stopToken.stop_requested();
        });
        if (stopToken.stop_requested()) return;
        lock.unlock();
        pollOnce();
        lock.lock();
    }
}

}  // namespace x308
