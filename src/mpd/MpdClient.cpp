#include "x308/MpdClient.hpp"

#include <mpd/client.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

namespace x308 {
namespace {

using ConnectionPtr = std::unique_ptr<mpd_connection, decltype(&mpd_connection_free)>;
using StatusPtr = std::unique_ptr<mpd_status, decltype(&mpd_status_free)>;
using SongPtr = std::unique_ptr<mpd_song, decltype(&mpd_song_free)>;
using EntityPtr = std::unique_ptr<mpd_entity, decltype(&mpd_entity_free)>;

ConnectionPtr connect(const MpdConfig& config, const unsigned timeout, std::string& error) {
    ConnectionPtr connection{
        mpd_connection_new(config.host.c_str(), config.port, timeout), &mpd_connection_free};
    if (!connection) {
        error = "Cannot allocate MPD connection";
        return connection;
    }
    if (mpd_connection_get_error(connection.get()) != MPD_ERROR_SUCCESS) {
        error = mpd_connection_get_error_message(connection.get());
        connection.reset();
    } else {
        error.clear();
    }
    return connection;
}

Result commandResult(mpd_connection* connection, const bool success, std::string& error,
                     const std::string_view action) {
    if (success) {
        error.clear();
        return Result::ok(std::string{action} + " completed");
    }
    error = mpd_connection_get_error_message(connection);
    return Result::error(error.empty() ? std::string{action} + " failed" : error);
}

Track convertSong(const mpd_song* song) {
    return MpdClient::trackFromMetadata(
        mpd_song_get_uri(song), mpd_song_get_tag(song, MPD_TAG_TITLE, 0),
        mpd_song_get_tag(song, MPD_TAG_ARTIST, 0), mpd_song_get_tag(song, MPD_TAG_ALBUM, 0));
}

PlaybackState convertState(const mpd_state state) {
    switch (state) {
        case MPD_STATE_STOP: return PlaybackState::stopped;
        case MPD_STATE_PLAY: return PlaybackState::playing;
        case MPD_STATE_PAUSE: return PlaybackState::paused;
        case MPD_STATE_UNKNOWN: return PlaybackState::unknown;
    }
    return PlaybackState::unknown;
}

}  // namespace

MpdClient::MpdClient(MpdConfig config, const unsigned timeoutMilliseconds)
    : config_(std::move(config)), timeoutMilliseconds_(timeoutMilliseconds) {}

MediaStatus MpdClient::status() {
    MediaStatus result;
    constexpr unsigned statusTimeoutMilliseconds = 180;
    auto connection = connect(
        config_, std::min(timeoutMilliseconds_, statusTimeoutMilliseconds), lastError_);
    if (!connection) {
        result.error = lastError_;
        return result;
    }
    StatusPtr status{mpd_run_status(connection.get()), &mpd_status_free};
    if (!status) {
        lastError_ = mpd_connection_get_error_message(connection.get());
        result.error = lastError_;
        return result;
    }
    result.available = true;
    result.state = convertState(mpd_status_get_state(status.get()));
    result.random = mpd_status_get_random(status.get());
    result.repeat = mpd_status_get_repeat(status.get());

    SongPtr song{mpd_run_current_song(connection.get()), &mpd_song_free};
    if (song) {
        result.currentTrack = convertSong(song.get());
    } else if (mpd_connection_get_error(connection.get()) != MPD_ERROR_SUCCESS) {
        lastError_ = mpd_connection_get_error_message(connection.get());
        result.error = lastError_;
    } else {
        lastError_.clear();
    }
    return result;
}

#define X308_MPD_COMMAND(methodName, expression, action) \
    Result MpdClient::methodName() { \
        auto connection = connect(config_, timeoutMilliseconds_, lastError_); \
        if (!connection) return Result::error(lastError_); \
        return commandResult(connection.get(), (expression), lastError_, (action)); \
    }

X308_MPD_COMMAND(play, mpd_run_play(connection.get()), "Play")
X308_MPD_COMMAND(pause, mpd_run_pause(connection.get(), true), "Pause")
X308_MPD_COMMAND(stop, mpd_run_stop(connection.get()), "Stop")
X308_MPD_COMMAND(togglePause, mpd_run_toggle_pause(connection.get()), "Toggle pause")
X308_MPD_COMMAND(next, mpd_run_next(connection.get()), "Next")
X308_MPD_COMMAND(previous, mpd_run_previous(connection.get()), "Previous")
X308_MPD_COMMAND(clearQueue, mpd_run_clear(connection.get()), "Clear queue")

#undef X308_MPD_COMMAND

std::vector<Track> MpdClient::queue() {
    std::vector<Track> result;
    auto connection = connect(config_, timeoutMilliseconds_, lastError_);
    if (!connection) return result;
    if (!mpd_send_list_queue_meta(connection.get())) {
        lastError_ = mpd_connection_get_error_message(connection.get());
        return result;
    }
    while (mpd_song* rawSong = mpd_recv_song(connection.get())) {
        SongPtr song{rawSong, &mpd_song_free};
        result.push_back(convertSong(song.get()));
    }
    if (!mpd_response_finish(connection.get())) {
        lastError_ = mpd_connection_get_error_message(connection.get());
        result.clear();
    } else {
        lastError_.clear();
    }
    return result;
}

std::vector<LibraryEntry> MpdClient::library(const std::string_view path) {
    std::vector<LibraryEntry> result;
    auto connection = connect(config_, timeoutMilliseconds_, lastError_);
    if (!connection) return result;
    const std::string pathString{path};
    if (!mpd_send_list_meta(connection.get(), pathString.empty() ? nullptr : pathString.c_str())) {
        lastError_ = mpd_connection_get_error_message(connection.get());
        return result;
    }
    while (mpd_entity* rawEntity = mpd_recv_entity(connection.get())) {
        EntityPtr entity{rawEntity, &mpd_entity_free};
        const auto type = mpd_entity_get_type(entity.get());
        if (type == MPD_ENTITY_TYPE_DIRECTORY) {
            result.push_back({mpd_directory_get_path(mpd_entity_get_directory(entity.get())), true});
        } else if (type == MPD_ENTITY_TYPE_SONG) {
            result.push_back({mpd_song_get_uri(mpd_entity_get_song(entity.get())), false});
        }
    }
    if (!mpd_response_finish(connection.get())) {
        lastError_ = mpd_connection_get_error_message(connection.get());
        result.clear();
    } else {
        lastError_.clear();
    }
    return result;
}

Result MpdClient::add(const std::string_view path) {
    auto connection = connect(config_, timeoutMilliseconds_, lastError_);
    if (!connection) return Result::error(lastError_);
    const std::string value{path};
    return commandResult(connection.get(), mpd_run_add(connection.get(), value.c_str()),
                         lastError_, "Add");
}

Result MpdClient::addFolder(const std::string_view path) {
    return add(path);
}

Result MpdClient::setRandom(const bool enabled) {
    auto connection = connect(config_, timeoutMilliseconds_, lastError_);
    if (!connection) return Result::error(lastError_);
    return commandResult(connection.get(), mpd_run_random(connection.get(), enabled),
                         lastError_, "Set random");
}

Result MpdClient::setRepeat(const bool enabled) {
    auto connection = connect(config_, timeoutMilliseconds_, lastError_);
    if (!connection) return Result::error(lastError_);
    return commandResult(connection.get(), mpd_run_repeat(connection.get(), enabled),
                         lastError_, "Set repeat");
}

Result MpdClient::update() {
    auto connection = connect(config_, timeoutMilliseconds_, lastError_);
    if (!connection) return Result::error(lastError_);
    const unsigned job = mpd_run_update(connection.get(), nullptr);
    if (job == 0) {
        lastError_ = mpd_connection_get_error_message(connection.get());
        return Result::error(lastError_.empty() ? "MPD update request failed" : lastError_);
    }
    lastError_.clear();
    return Result::ok("MPD update job " + std::to_string(job) + " started");
}

Result MpdClient::activateAudio() {
    const auto currentStatus = status();
    return currentStatus.available ? Result::ok("MPD audio is available")
                                   : Result::error(currentStatus.error);
}

Result MpdClient::releaseAudio() {
    return stop();
}

std::string MpdClient::lastError() const {
    return lastError_;
}

Track MpdClient::trackFromMetadata(std::string uri, const char* title, const char* artist,
                                   const char* album) {
    Track track;
    track.uri = std::move(uri);
    track.title = title == nullptr ? "" : title;
    track.artist = artist == nullptr ? "" : artist;
    track.album = album == nullptr ? "" : album;
    return track;
}

}  // namespace x308
