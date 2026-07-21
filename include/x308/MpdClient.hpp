#pragma once

#include "x308/Configuration.hpp"
#include "x308/Interfaces.hpp"

#include <string>

struct mpd_song;

namespace x308 {

class MpdClient final : public IMediaPlayer {
public:
    explicit MpdClient(MpdConfig config, unsigned timeoutMilliseconds = 3000);

    [[nodiscard]] MediaStatus status() override;
    [[nodiscard]] Result play() override;
    [[nodiscard]] Result pause() override;
    [[nodiscard]] Result stop() override;
    [[nodiscard]] Result togglePause() override;
    [[nodiscard]] Result next() override;
    [[nodiscard]] Result previous() override;
    [[nodiscard]] std::vector<Track> queue() override;
    [[nodiscard]] Result clearQueue() override;
    [[nodiscard]] std::vector<LibraryEntry> library(std::string_view path) override;
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

private:
    MpdConfig config_;
    unsigned timeoutMilliseconds_;
    std::string lastError_;
};

}  // namespace x308

