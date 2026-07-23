#pragma once

#include "x308/Configuration.hpp"
#include "x308/Interfaces.hpp"

#include <condition_variable>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <thread>

struct mpd_song;

namespace x308 {

class Logger;

class MpdClient final : public IMediaPlayer {
public:
    explicit MpdClient(MpdConfig config, unsigned timeoutMilliseconds = 3000,
                       Logger* logger = nullptr);
    ~MpdClient() override;

    [[nodiscard]] MediaStatus status() override;
    [[nodiscard]] Result play() override;
    [[nodiscard]] Result pause() override;
    [[nodiscard]] Result resume() override;
    [[nodiscard]] Result stop() override;
    [[nodiscard]] Result togglePause() override;
    [[nodiscard]] Result next() override;
    [[nodiscard]] Result previous() override;
    [[nodiscard]] std::vector<Track> queue() override;
    [[nodiscard]] Result clearQueue() override;
    [[nodiscard]] std::vector<LibraryEntry> library(std::string_view path) override;
    [[nodiscard]] Result playFolder(std::string_view folder,
                                    std::string_view selectedTrack) override;
    [[nodiscard]] Result add(std::string_view path) override;
    [[nodiscard]] Result addFolder(std::string_view path) override;
    [[nodiscard]] Result setRandom(bool enabled) override;
    [[nodiscard]] Result setRepeat(bool enabled) override;
    [[nodiscard]] Result update() override;
    [[nodiscard]] Result activateAudio() override;
    [[nodiscard]] Result releaseAudio() override;
    [[nodiscard]] std::string lastError() const override;

    [[nodiscard]] static Track trackFromMetadata(std::string uri, const char* title,
                                                 const char* artist, const char* album);
    [[nodiscard]] static std::vector<std::string> playableFoldersFromUris(
        const std::vector<std::string>& uris);
    [[nodiscard]] static std::vector<std::string> nextFolderCandidates(
        const std::vector<std::string>& folders, std::string_view previousFolder);

private:
    [[nodiscard]] MediaStatus statusLocked(unsigned timeoutMilliseconds);
    [[nodiscard]] std::vector<LibraryEntry> libraryLocked(std::string_view path);
    [[nodiscard]] std::vector<std::string> playableFoldersLocked();
    [[nodiscard]] std::vector<std::string> folderTracksLocked(std::string_view folder);
    [[nodiscard]] Result startFolderLocked(std::string_view folder,
                                           std::string_view selectedTrack,
                                           bool automatic);
    [[nodiscard]] Result startRandomFolderLocked();
    [[nodiscard]] Result verifyPlaybackLocked();
    [[nodiscard]] Result waitForTrackAfterCommandLocked(
        std::string_view completedMessage, std::string_view trackLabel,
        std::optional<PlaybackState> expectedState = std::nullopt,
        std::string_view previousUri = {}, bool requireTrackChange = false,
        bool advanceAutomaticFolderOnStop = false,
        bool metadataFailureIsWarning = false,
        unsigned statusTimeoutMilliseconds = 1500);
    void monitorAutomaticPlayback(std::stop_token stopToken);
    void logInfo(std::string_view message) const;
    void logWarning(std::string_view message) const;
    [[nodiscard]] Result setAudioOutputEnabled(bool enabled);

    MpdConfig config_;
    unsigned timeoutMilliseconds_;
    Logger* logger_;
    mutable std::mutex mutex_;
    std::string lastError_;
    bool manualSelectionMade_{false};
    bool automaticFolderPlayback_{false};
    std::string previousAutomaticFolder_;
    std::mt19937 randomEngine_{std::random_device{}()};
    std::condition_variable playbackWake_;
    std::jthread playbackMonitor_;
};

}  // namespace x308
