#include "x308/MpdClient.hpp"

#include "x308/Logger.hpp"

#include <mpd/client.h>
#include <mpd/database.h>
#include <mpd/output.h>
#include <mpd/player.h>

#include <algorithm>
#include <chrono>
#include <cerrno>
#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <utility>

namespace x308 {
namespace {

using ConnectionPtr = std::unique_ptr<mpd_connection, decltype(&mpd_connection_free)>;
using StatusPtr = std::unique_ptr<mpd_status, decltype(&mpd_status_free)>;
using SongPtr = std::unique_ptr<mpd_song, decltype(&mpd_song_free)>;
using EntityPtr = std::unique_ptr<mpd_entity, decltype(&mpd_entity_free)>;
using OutputPtr = std::unique_ptr<mpd_output, decltype(&mpd_output_free)>;

constexpr unsigned interactiveStatusTimeoutMilliseconds = 1500;
constexpr unsigned transitionTransactionTimeoutMilliseconds = 1500;
constexpr unsigned backgroundStatusTimeoutMilliseconds = 400;
constexpr auto transitionInitialDelay = std::chrono::milliseconds{150};
constexpr auto transitionRetryInterval = std::chrono::milliseconds{150};
constexpr auto transitionRetryWindow = std::chrono::seconds{3};
constexpr unsigned playbackStatusTimeoutMilliseconds = 500;

std::string_view playbackStateName(const PlaybackState state) {
    switch (state) {
        case PlaybackState::stopped: return "stop";
        case PlaybackState::paused: return "pause";
        case PlaybackState::playing: return "playing";
        case PlaybackState::unknown: return "unknown";
    }
    return "unknown";
}

std::string connectionError(const mpd_connection* connection,
                            const std::string_view operation) {
    const char* rawMessage = mpd_connection_get_error_message(connection);
    const std::string detail = rawMessage == nullptr ? std::string{} : rawMessage;
    const auto withDetail = [&detail](const std::string_view category) {
        return detail.empty() ? std::string{category}
                              : std::string{category} + ": " + detail;
    };
    switch (mpd_connection_get_error(connection)) {
        case MPD_ERROR_TIMEOUT:
            return withDetail(std::string{operation} + " timed out");
        case MPD_ERROR_SYSTEM:
            if (mpd_connection_get_system_error(connection) == ECONNREFUSED) {
                return withDetail(std::string{operation} + ": MPD connection refused");
            }
            return withDetail(std::string{operation} + ": MPD socket error");
        case MPD_ERROR_RESOLVER:
            return withDetail(std::string{operation} + ": MPD host resolution failed");
        case MPD_ERROR_MALFORMED:
            return withDetail(std::string{operation} + ": malformed MPD response");
        case MPD_ERROR_CLOSED:
            return withDetail(std::string{operation} + ": MPD socket closed");
        case MPD_ERROR_SERVER:
            return withDetail(std::string{operation} + ": MPD ACK error");
        case MPD_ERROR_ARGUMENT:
            return withDetail("Invalid MPD command argument");
        case MPD_ERROR_STATE:
            return withDetail("Invalid MPD connection state");
        case MPD_ERROR_OOM:
            return withDetail("MPD client allocation failed");
        case MPD_ERROR_SUCCESS:
            return std::string{operation} + " failed without an MPD error";
    }
    return withDetail("Unknown MPD error");
}

ConnectionPtr connect(const MpdConfig& config, const unsigned timeout, std::string& error) {
    ConnectionPtr connection{
        mpd_connection_new(config.host.c_str(), config.port, timeout), &mpd_connection_free};
    if (!connection) {
        error = "Cannot allocate MPD connection";
        return connection;
    }
    if (mpd_connection_get_error(connection.get()) != MPD_ERROR_SUCCESS) {
        error = connectionError(connection.get(), "Connect");
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
    error = connectionError(connection, action);
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

std::string trackName(const Track& track) {
    if (!track.title.empty()) return track.title;
    const auto separator = track.uri.rfind('/');
    return separator == std::string::npos ? track.uri : track.uri.substr(separator + 1);
}

std::string trackSummary(const Track& track) {
    const auto title = trackName(track);
    return track.artist.empty() ? title : track.artist + " — " + title;
}

std::string outputSummary(const std::vector<std::string>& outputs) {
    if (outputs.empty()) return "<none>";
    std::string result;
    for (std::size_t index = 0; index < outputs.size(); ++index) {
        if (index != 0) result += ", ";
        result += outputs[index];
    }
    return result;
}

MediaStatus readStatus(const MpdConfig& config, const unsigned timeout, std::string& error) {
    MediaStatus result;
    auto connection = connect(config, timeout, error);
    if (!connection) {
        result.error = error;
        return result;
    }
    StatusPtr status{mpd_run_status(connection.get()), &mpd_status_free};
    if (!status) {
        error = connectionError(connection.get(), "Read status");
        result.error = error;
        return result;
    }
    result.available = true;
    result.state = convertState(mpd_status_get_state(status.get()));
    result.random = mpd_status_get_random(status.get());
    result.repeat = mpd_status_get_repeat(status.get());
    result.elapsedMilliseconds = mpd_status_get_elapsed_ms(status.get());

    SongPtr song{mpd_run_current_song(connection.get()), &mpd_song_free};
    if (song) {
        result.currentTrack = convertSong(song.get());
        error.clear();
    } else if (mpd_connection_get_error(connection.get()) != MPD_ERROR_SUCCESS) {
        error = connectionError(connection.get(), "Read current song");
        result.error = error;
    } else {
        error.clear();
    }
    return result;
}

}  // namespace

MpdClient::MpdClient(MpdConfig config, const unsigned timeoutMilliseconds, Logger* logger)
    : config_(std::move(config)),
      timeoutMilliseconds_(timeoutMilliseconds),
      logger_(logger),
      playbackMonitor_([this](const std::stop_token stopToken) {
          monitorAutomaticPlayback(stopToken);
      }) {}

MpdClient::~MpdClient() {
    playbackMonitor_.request_stop();
    playbackWake_.notify_all();
    if (playbackMonitor_.joinable()) playbackMonitor_.join();
}

MediaStatus MpdClient::status() {
    const std::lock_guard lock(mutex_);
    return statusLocked(interactiveStatusTimeoutMilliseconds);
}

MediaStatus MpdClient::statusLocked(const unsigned requestedTimeoutMilliseconds) {
    const auto timeout = std::min(timeoutMilliseconds_, requestedTimeoutMilliseconds);
    return readStatus(config_, timeout, lastError_);
}

#define X308_MPD_COMMAND(methodName, expression, action) \
    Result MpdClient::methodName() { \
        const std::lock_guard lock(mutex_); \
        auto connection = connect(config_, timeoutMilliseconds_, lastError_); \
        if (!connection) return Result::error(lastError_); \
        return commandResult(connection.get(), (expression), lastError_, (action)); \
    }

X308_MPD_COMMAND(togglePause, mpd_run_toggle_pause(connection.get()), "Toggle pause")

#undef X308_MPD_COMMAND

Result MpdClient::play() {
    const std::lock_guard lock(mutex_);
    logInfo("MPD playback request started");
    if (const auto output = setAudioOutputEnabledLocked(true); !output.success) {
        return Result::error("Cannot start MPD playback: " + output.message);
    }
    const auto current = statusLocked(500);
    if (current.available && current.state == PlaybackState::paused) {
        auto connection = connect(config_, timeoutMilliseconds_, lastError_);
        if (!connection) return Result::error(lastError_);
        const auto result = commandResult(
            connection.get(), mpd_run_play(connection.get()), lastError_, "Resume");
        if (!result.success) return result;
        connection.reset();
        logInfo("MPD play command sent");
        return waitForTrackAfterCommandLocked(
            "Воспроизведение продолжено.", "Сейчас играет",
            PlaybackState::playing, {}, false, false, false,
            playbackStatusTimeoutMilliseconds);
    }

    if (!manualSelectionMade_) {
        automaticFolderPlayback_ = true;
        return startRandomFolderLocked();
    }

    auto connection = connect(config_, timeoutMilliseconds_, lastError_);
    if (!connection) return Result::error(lastError_);
    const auto result = commandResult(
        connection.get(), mpd_run_play(connection.get()), lastError_, "Play");
    if (!result.success) return result;
    connection.reset();
    return verifyPlaybackLocked();
}

Result MpdClient::pause() {
    const std::lock_guard lock(mutex_);
    auto connection = connect(config_, timeoutMilliseconds_, lastError_);
    if (!connection) return Result::error(lastError_);
    const auto command = commandResult(
        connection.get(), mpd_run_pause(connection.get(), true), lastError_, "Pause");
    if (!command.success) return command;
    connection.reset();
    return waitForTrackAfterCommandLocked(
        "Воспроизведение приостановлено.", "Текущий трек",
        PlaybackState::paused);
}

Result MpdClient::resume() {
    const std::lock_guard lock(mutex_);
    logInfo("MPD playback request started");
    if (const auto output = setAudioOutputEnabledLocked(true); !output.success) {
        return Result::error("Cannot start MPD playback: " + output.message);
    }
    auto connection = connect(config_, timeoutMilliseconds_, lastError_);
    if (!connection) return Result::error(lastError_);
    const auto result = commandResult(
        connection.get(), mpd_run_play(connection.get()), lastError_, "Resume");
    if (!result.success) return result;
    logInfo("MPD play command sent");
    connection.reset();
    return waitForTrackAfterCommandLocked(
        "Воспроизведение продолжено.", "Сейчас играет",
        PlaybackState::playing, {}, false, false, false,
        playbackStatusTimeoutMilliseconds);
}

Result MpdClient::stop() {
    const std::lock_guard lock(mutex_);
    automaticFolderPlayback_ = false;
    auto connection = connect(config_, timeoutMilliseconds_, lastError_);
    if (!connection) return Result::error(lastError_);
    const auto result = commandResult(
        connection.get(), mpd_run_stop(connection.get()), lastError_, "Stop");
    return result.success ? Result::ok("Воспроизведение остановлено.") : result;
}

Result MpdClient::next() {
    const std::lock_guard lock(mutex_);
    const auto before = statusLocked(transitionTransactionTimeoutMilliseconds);
    const auto previousUri = before.currentTrack.has_value()
        ? before.currentTrack->uri : std::string{};
    const auto expectedState =
        before.state == PlaybackState::playing || before.state == PlaybackState::paused
        ? std::optional{before.state} : std::nullopt;

    auto connection = connect(config_, timeoutMilliseconds_, lastError_);
    if (!connection) return Result::error(lastError_);
    const auto command = commandResult(
        connection.get(), mpd_run_next(connection.get()), lastError_, "Next");
    if (!command.success) return command;
    connection.reset();
    return waitForTrackAfterCommandLocked(
        "Выбран следующий трек.", "Сейчас играет", expectedState,
        previousUri, !previousUri.empty(), automaticFolderPlayback_, true);
}

Result MpdClient::previous() {
    const std::lock_guard lock(mutex_);
    const auto before = statusLocked(transitionTransactionTimeoutMilliseconds);
    const auto previousUri = before.currentTrack.has_value()
        ? before.currentTrack->uri : std::string{};
    const auto expectedState =
        before.state == PlaybackState::playing || before.state == PlaybackState::paused
        ? std::optional{before.state} : std::nullopt;

    auto connection = connect(config_, timeoutMilliseconds_, lastError_);
    if (!connection) return Result::error(lastError_);
    const auto command = commandResult(
        connection.get(), mpd_run_previous(connection.get()), lastError_, "Previous");
    if (!command.success) return command;
    connection.reset();
    return waitForTrackAfterCommandLocked(
        "Выбран предыдущий трек.", "Сейчас играет", expectedState,
        previousUri, !previousUri.empty(), false, true);
}

Result MpdClient::clearQueue() {
    const std::lock_guard lock(mutex_);
    automaticFolderPlayback_ = false;
    manualSelectionMade_ = false;
    auto connection = connect(config_, timeoutMilliseconds_, lastError_);
    if (!connection) return Result::error(lastError_);
    return commandResult(
        connection.get(), mpd_run_clear(connection.get()), lastError_, "Clear queue");
}

std::vector<Track> MpdClient::queue() {
    const std::lock_guard lock(mutex_);
    std::vector<Track> result;
    auto connection = connect(config_, timeoutMilliseconds_, lastError_);
    if (!connection) return result;
    if (!mpd_send_list_queue_meta(connection.get())) {
        lastError_ = connectionError(connection.get(), "List MPD queue");
        return result;
    }
    while (mpd_song* rawSong = mpd_recv_song(connection.get())) {
        SongPtr song{rawSong, &mpd_song_free};
        result.push_back(convertSong(song.get()));
    }
    if (!mpd_response_finish(connection.get())) {
        lastError_ = connectionError(connection.get(), "Finish MPD queue response");
        result.clear();
    } else {
        lastError_.clear();
    }
    return result;
}

std::vector<LibraryEntry> MpdClient::library(const std::string_view path) {
    const std::lock_guard lock(mutex_);
    return libraryLocked(path);
}

std::vector<LibraryEntry> MpdClient::libraryLocked(const std::string_view path) {
    std::vector<LibraryEntry> result;
    auto connection = connect(config_, timeoutMilliseconds_, lastError_);
    if (!connection) return result;
    const std::string pathString{path};
    if (!mpd_send_list_meta(connection.get(), pathString.empty() ? nullptr : pathString.c_str())) {
        lastError_ = connectionError(connection.get(), "List MPD library");
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
        lastError_ = connectionError(connection.get(), "Finish MPD library response");
        result.clear();
    } else {
        lastError_.clear();
    }
    return result;
}

Result MpdClient::playFolder(const std::string_view folder,
                             const std::string_view selectedTrack) {
    const std::lock_guard lock(mutex_);
    automaticFolderPlayback_ = false;
    manualSelectionMade_ = true;
    return startFolderLocked(folder, selectedTrack, false);
}

Result MpdClient::add(const std::string_view path) {
    const std::lock_guard lock(mutex_);
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
    const std::lock_guard lock(mutex_);
    auto connection = connect(config_, timeoutMilliseconds_, lastError_);
    if (!connection) return Result::error(lastError_);
    return commandResult(connection.get(), mpd_run_random(connection.get(), enabled),
                         lastError_, "Set random");
}

Result MpdClient::setRepeat(const bool enabled) {
    const std::lock_guard lock(mutex_);
    auto connection = connect(config_, timeoutMilliseconds_, lastError_);
    if (!connection) return Result::error(lastError_);
    return commandResult(connection.get(), mpd_run_repeat(connection.get(), enabled),
                         lastError_, "Set repeat");
}

Result MpdClient::update() {
    const std::lock_guard lock(mutex_);
    auto connection = connect(config_, timeoutMilliseconds_, lastError_);
    if (!connection) return Result::error(lastError_);
    const unsigned job = mpd_run_update(connection.get(), nullptr);
    if (job == 0) {
        lastError_ = connectionError(connection.get(), "Update MPD database");
        return Result::error(lastError_.empty() ? "MPD update request failed" : lastError_);
    }
    lastError_.clear();
    return Result::ok("MPD update job " + std::to_string(job) + " started");
}

Result MpdClient::activateAudio() {
    return setAudioOutputEnabled(true);
}

Result MpdClient::releaseAudio() {
    return setAudioOutputEnabled(false);
}

Result MpdClient::setAudioOutputEnabled(const bool enabled) {
    const std::lock_guard lock(mutex_);
    return setAudioOutputEnabledLocked(enabled);
}

Result MpdClient::setAudioOutputEnabledLocked(const bool enabled) {
    auto connection = connect(config_, timeoutMilliseconds_, lastError_);
    if (!connection) return Result::error(lastError_);
    if (!mpd_send_outputs(connection.get())) {
        lastError_ = connectionError(connection.get(), "List MPD outputs");
        return Result::error(lastError_);
    }

    std::optional<unsigned> selectedId;
    bool currentlyEnabled = false;
    std::vector<std::string> outputs;
    while (mpd_output* rawOutput = mpd_recv_output(connection.get())) {
        OutputPtr output{rawOutput, &mpd_output_free};
        const char* rawName = mpd_output_get_name(output.get());
        const std::string_view name = rawName == nullptr ? std::string_view{} : rawName;
        const auto id = mpd_output_get_id(output.get());
        outputs.emplace_back((name.empty() ? "output " + std::to_string(id)
                                           : std::string{name}) +
                             (mpd_output_get_enabled(output.get()) ? " [enabled]"
                                                                    : " [disabled]"));
        if (!selectedId.has_value() &&
            (config_.audioOutputName.empty() || name == config_.audioOutputName)) {
            selectedId = id;
            currentlyEnabled = mpd_output_get_enabled(output.get());
        }
    }
    if (!mpd_response_finish(connection.get())) {
        lastError_ = connectionError(connection.get(), "Finish MPD outputs response");
        return Result::error(lastError_);
    }
    if (!selectedId.has_value()) {
        lastError_ = "MPD audio output not found: " + config_.audioOutputName +
                     "; outputs: " + outputSummary(outputs);
        return Result::error(lastError_);
    }
    if (currentlyEnabled == enabled) {
        lastError_.clear();
        return Result::ok(enabled ? "MPD audio output is already enabled"
                                  : "MPD audio output is already disabled");
    }
    const bool changed = enabled
        ? mpd_run_enable_output(connection.get(), *selectedId)
        : mpd_run_disable_output(connection.get(), *selectedId);
    const auto result = commandResult(
        connection.get(), changed, lastError_,
        enabled ? "Enable MPD audio output" : "Disable MPD audio output");
    if (!result.success) {
        return Result::error(std::string{enabled ? "Cannot enable" : "Cannot disable"} +
                             " MPD audio output; outputs: " + outputSummary(outputs) +
                             "; " + result.message);
    }
    return result;
}

std::string MpdClient::lastError() const {
    const std::lock_guard lock(mutex_);
    return lastError_;
}

std::vector<std::string> MpdClient::playableFoldersFromUris(
    const std::vector<std::string>& uris) {
    std::set<std::string> foldersWithTracks;
    for (const auto& uri : uris) {
        const auto separator = uri.rfind('/');
        foldersWithTracks.insert(
            separator == std::string::npos ? std::string{} : uri.substr(0, separator));
    }
    std::vector<std::string> leafFolders;
    for (const auto& folder : foldersWithTracks) {
        const auto prefix = folder.empty() ? std::string{} : folder + '/';
        const auto hasPlayableChild = std::any_of(
            foldersWithTracks.begin(), foldersWithTracks.end(),
            [&folder, &prefix](const std::string& other) {
                if (other == folder) return false;
                return folder.empty() || other.starts_with(prefix);
            });
        if (!hasPlayableChild) leafFolders.push_back(folder);
    }
    return leafFolders;
}

std::vector<std::string> MpdClient::nextFolderCandidates(
    const std::vector<std::string>& folders, const std::string_view previousFolder) {
    if (folders.size() <= 1) return folders;
    std::vector<std::string> result;
    std::copy_if(folders.begin(), folders.end(), std::back_inserter(result),
                 [previousFolder](const std::string& folder) {
                     return folder != previousFolder;
                 });
    return result.empty() ? folders : result;
}

std::vector<std::string> MpdClient::playableFoldersLocked() {
    std::vector<std::string> uris;
    auto connection = connect(config_, timeoutMilliseconds_, lastError_);
    if (!connection) return {};
    if (!mpd_send_list_all(connection.get(), nullptr)) {
        lastError_ = connectionError(connection.get(), "List all MPD songs");
        return {};
    }
    while (mpd_entity* rawEntity = mpd_recv_entity(connection.get())) {
        EntityPtr entity{rawEntity, &mpd_entity_free};
        if (mpd_entity_get_type(entity.get()) == MPD_ENTITY_TYPE_SONG) {
            uris.emplace_back(mpd_song_get_uri(mpd_entity_get_song(entity.get())));
        }
    }
    if (!mpd_response_finish(connection.get())) {
        lastError_ = connectionError(connection.get(), "Finish MPD song list response");
        return {};
    }
    lastError_.clear();
    return playableFoldersFromUris(uris);
}

std::vector<std::string> MpdClient::folderTracksLocked(const std::string_view folder) {
    std::vector<std::string> tracks;
    for (const auto& entry : libraryLocked(folder)) {
        if (!entry.directory) tracks.push_back(entry.path);
    }
    std::sort(tracks.begin(), tracks.end());
    return tracks;
}

Result MpdClient::startFolderLocked(const std::string_view folder,
                                    const std::string_view selectedTrack,
                                    const bool automatic) {
    const auto tracks = folderTracksLocked(folder);
    if (tracks.empty()) {
        const auto folderName = folder.empty() ? std::string{"корень библиотеки"}
                                                : std::string{folder};
        lastError_ = "В папке нет доступных аудиотреков: " + folderName;
        logWarning("MPD folder has no playable tracks: " + std::string{folder});
        return Result::error(lastError_);
    }

    std::size_t selectedPosition = 0;
    if (!selectedTrack.empty()) {
        const auto selected = std::find(tracks.begin(), tracks.end(), selectedTrack);
        if (selected == tracks.end()) {
            lastError_ = "Выбранный трек отсутствует в папке MPD";
            return Result::error(lastError_);
        }
        selectedPosition = static_cast<std::size_t>(std::distance(tracks.begin(), selected));
    }

    if (const auto output = setAudioOutputEnabledLocked(true); !output.success) {
        return Result::error("Cannot start MPD playback: " + output.message);
    }

    auto connection = connect(config_, timeoutMilliseconds_, lastError_);
    if (!connection) return Result::error(lastError_);
    if (!mpd_run_clear(connection.get()) ||
        !mpd_run_random(connection.get(), false) ||
        !mpd_run_repeat(connection.get(), false)) {
        lastError_ = connectionError(connection.get(), "Prepare MPD queue");
        return Result::error(lastError_.empty() ? "Не удалось подготовить очередь MPD" : lastError_);
    }
    for (const auto& track : tracks) {
        if (!mpd_run_add(connection.get(), track.c_str())) {
            lastError_ = connectionError(connection.get(), "Add track to MPD queue");
            return Result::error(lastError_.empty() ? "Не удалось добавить трек в очередь MPD"
                                                    : lastError_);
        }
    }
    if (!mpd_run_play_pos(connection.get(), static_cast<unsigned>(selectedPosition))) {
        lastError_ = connectionError(connection.get(), "Start MPD folder playback");
        return Result::error(lastError_.empty() ? "MPD не начал воспроизведение" : lastError_);
    }
    connection.reset();

    const auto mode = automatic ? "automatic" : "manual";
    logInfo("MPD " + std::string{mode} + " folder playback started: " +
            std::string{folder} + ", track index " + std::to_string(selectedPosition));
    logInfo("MPD play command sent");
    return verifyPlaybackLocked();
}

Result MpdClient::startRandomFolderLocked() {
    const auto folders = playableFoldersLocked();
    if (folders.empty()) {
        lastError_ = lastError_.empty() ? "В библиотеке MPD нет папок с аудиотреками"
                                        : lastError_;
        automaticFolderPlayback_ = false;
        logWarning("MPD automatic folder playback cannot find playable folders");
        return Result::error(lastError_);
    }
    const auto candidates = nextFolderCandidates(folders, previousAutomaticFolder_);
    std::uniform_int_distribution<std::size_t> distribution{0, candidates.size() - 1};
    const auto& selectedFolder = candidates[distribution(randomEngine_)];
    previousAutomaticFolder_ = selectedFolder;
    automaticFolderPlayback_ = true;
    return startFolderLocked(selectedFolder, {}, true);
}

Result MpdClient::verifyPlaybackLocked() {
    return waitForTrackAfterCommandLocked(
        "Воспроизведение успешно запущено.", "Сейчас играет",
        PlaybackState::playing, {}, false, false, false,
        playbackStatusTimeoutMilliseconds);
}

Result MpdClient::waitForTrackAfterCommandLocked(
    const std::string_view completedMessage, const std::string_view trackLabel,
    const std::optional<PlaybackState> expectedState,
    const std::string_view previousUri, const bool requireTrackChange,
    const bool advanceAutomaticFolderOnStop,
    const bool metadataFailureIsWarning,
    const unsigned statusTimeoutMilliseconds) {
    const auto deadline = std::chrono::steady_clock::now() + transitionRetryWindow;
    const auto waitStarted = std::chrono::steady_clock::now();
    const bool confirmingPlayback = expectedState == PlaybackState::playing;
    MediaStatus lastStatus;
    std::optional<MediaStatus> lastAvailableStatus;
    std::this_thread::sleep_for(transitionInitialDelay);

    while (true) {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
        if (remaining <= std::chrono::milliseconds::zero()) break;
        const auto remainingMilliseconds = std::max(
            1U, static_cast<unsigned>(remaining.count()));
        lastStatus = statusLocked(std::min(statusTimeoutMilliseconds, remainingMilliseconds));
        if (confirmingPlayback && lastStatus.available) {
            logInfo("MPD state: " + std::string{playbackStateName(lastStatus.state)});
        }
        if (lastStatus.available) lastAvailableStatus = lastStatus;
        if (advanceAutomaticFolderOnStop && lastStatus.available &&
            lastStatus.state == PlaybackState::stopped) {
            return startRandomFolderLocked();
        }
        const bool stateMatches = !expectedState.has_value() ||
                                  lastStatus.state == *expectedState;
        const bool hasTrack = lastStatus.currentTrack.has_value();
        const bool trackMatches = hasTrack &&
            (!requireTrackChange || lastStatus.currentTrack->uri != previousUri);
        if (lastStatus.available && stateMatches && trackMatches) {
            lastError_.clear();
            if (confirmingPlayback) {
                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - waitStarted);
                logInfo("MPD playback confirmed after " + std::to_string(elapsed.count()) + " ms");
            }
            return Result::ok(std::string{completedMessage} + "\n" +
                              std::string{trackLabel} + ": " +
                              trackSummary(*lastStatus.currentTrack));
        }

        if (std::chrono::steady_clock::now() >= deadline) break;
        std::this_thread::sleep_for(transitionRetryInterval);
    }

    const auto warning = [&](std::string diagnostic) {
        if (!metadataFailureIsWarning) {
            return Result::error(std::string{completedMessage} + " " + diagnostic);
        }
        lastError_.clear();
        return Result::ok(std::string{completedMessage} +
                          "\nПредупреждение: трек переключён, но актуальные метаданные "
                          "не удалось получить за 3 с.\n" +
                          diagnostic + "\nВоспроизведение продолжается.");
    };

    if (!lastAvailableStatus.has_value()) {
        lastError_ = lastStatus.error.empty()
            ? "MPD status response was unavailable"
            : lastStatus.error;
        return warning("Последняя ошибка чтения: " + lastError_);
    }
    const auto& availableStatus = *lastAvailableStatus;
    if (availableStatus.state == PlaybackState::stopped &&
        !availableStatus.currentTrack.has_value()) {
        lastError_ = "MPD is stopped and has no current song";
        return warning("MPD остановлен, текущий трек отсутствует.");
    }
    if (!availableStatus.currentTrack.has_value()) {
        if (!availableStatus.error.empty()) {
            lastError_ = availableStatus.error;
            return warning("Последняя ошибка чтения: " + lastError_);
        }
        lastError_ = "MPD response has no current song metadata";
        return warning("MPD не вернул метаданные текущего трека.");
    }
    if (requireTrackChange && availableStatus.currentTrack->uri == previousUri) {
        lastError_ = "MPD current song did not change before the retry deadline";
        return warning("MPD продолжает возвращать метаданные предыдущего трека.");
    }

    lastError_ = "MPD playback state did not reach the expected value";
    if (confirmingPlayback) {
        logWarning("MPD playback timeout: last state=" +
                   std::string{lastStatus.available ? playbackStateName(lastStatus.state) : "unavailable"} +
                   ", play command sent=true, audio preparation completed=true");
    }
    return warning("Состояние воспроизведения не обновилось за 3 с.");
}

void MpdClient::monitorAutomaticPlayback(const std::stop_token stopToken) {
    using namespace std::chrono_literals;
    std::unique_lock lock(mutex_);
    while (!stopToken.stop_requested()) {
        playbackWake_.wait_for(lock, 500ms, [&stopToken] {
            return stopToken.stop_requested();
        });
        if (stopToken.stop_requested()) return;
        if (!automaticFolderPlayback_) continue;
        const auto current = statusLocked(backgroundStatusTimeoutMilliseconds);
        if (!current.available || current.state != PlaybackState::stopped) continue;

        const auto result = startRandomFolderLocked();
        if (!result.success) {
            logWarning("MPD automatic folder transition failed: " + result.message);
        }
    }
}

void MpdClient::logInfo(const std::string_view message) const {
    if (logger_ != nullptr) logger_->log(LogLevel::info, message);
}

void MpdClient::logWarning(const std::string_view message) const {
    if (logger_ != nullptr) logger_->log(LogLevel::warning, message);
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
