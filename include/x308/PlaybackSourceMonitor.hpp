#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>

namespace x308 {

class IBluetoothMediaController;
class Logger;
class SourceManager;

class PlaybackSourceMonitor final {
public:
    PlaybackSourceMonitor(IBluetoothMediaController& bluetoothMedia,
                          SourceManager& sourceManager, Logger* logger = nullptr,
                          bool startMonitoring = true);
    ~PlaybackSourceMonitor();

    PlaybackSourceMonitor(const PlaybackSourceMonitor&) = delete;
    PlaybackSourceMonitor& operator=(const PlaybackSourceMonitor&) = delete;

    void pollOnce();

private:
    void monitor(std::stop_token stopToken);

    IBluetoothMediaController& bluetoothMedia_;
    SourceManager& sourceManager_;
    Logger* logger_;
    bool bluetoothWasPlaying_{false};
    std::mutex wakeMutex_;
    std::condition_variable wake_;
    std::jthread worker_;
};

}  // namespace x308
