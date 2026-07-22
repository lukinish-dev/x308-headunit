#include "x308/SystemStatusService.hpp"

#include <exception>
#include <system_error>
#include <thread>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <utility>

namespace x308 {
namespace {

void collectSystemDetails(SystemStatusReport& report) {
    utsname information{};
    if (uname(&information) == 0) {
        report.system.hostname = information.nodename;
        report.system.kernel = information.release;
    } else {
        report.warnings.emplace_back("Cannot read hostname and kernel information");
    }

    struct sysinfo systemInformation {};
    if (sysinfo(&systemInformation) == 0) {
        report.system.uptime = std::chrono::seconds{
            static_cast<std::chrono::seconds::rep>(systemInformation.uptime)};
    } else {
        report.warnings.emplace_back("Cannot read system uptime");
    }
}

void collectStorageDetails(SystemStatusReport& report) {
    std::error_code error;
    report.storage.present = std::filesystem::exists(report.storage.path, error);
    if (error) {
        report.warnings.emplace_back("Cannot check music directory existence: " + error.message());
        return;
    }
    report.storage.accessible = report.storage.present &&
        std::filesystem::is_directory(report.storage.path, error) &&
        access(report.storage.path.c_str(), R_OK | X_OK) == 0;
    if (error) {
        report.warnings.emplace_back("Cannot inspect music directory: " + error.message());
        return;
    }
    if (!report.storage.accessible) return;

    const auto space = std::filesystem::space(report.storage.path, error);
    if (error) {
        report.warnings.emplace_back("Cannot read free storage space: " + error.message());
        return;
    }
    report.storage.freeBytes = space.available;
}

}  // namespace

SystemStatusService::SystemStatusService(
    IMediaPlayer& mediaPlayer, IBluetoothManager& bluetooth,
    const SourceManager& sourceManager, std::filesystem::path musicDirectory,
    std::string applicationVersion, std::string buildType,
    const std::chrono::steady_clock::time_point applicationStartedAt)
    : mediaPlayer_(mediaPlayer),
      bluetooth_(bluetooth),
      sourceManager_(sourceManager),
      musicDirectory_(std::move(musicDirectory)),
      applicationVersion_(std::move(applicationVersion)),
      buildType_(std::move(buildType)),
      applicationStartedAt_(applicationStartedAt) {}

SystemStatusReport SystemStatusService::collect() {
    const auto collectionStartedAt = std::chrono::steady_clock::now();
    SystemStatusReport report;

    report.application.version = applicationVersion_;
    report.application.buildType = buildType_;
    report.application.uptime = std::chrono::duration_cast<std::chrono::milliseconds>(
        collectionStartedAt - applicationStartedAt_);

    collectSystemDetails(report);
    report.storage.path = musicDirectory_;
    collectStorageDetails(report);

    report.sourceManager.activeSource = sourceManager_.activeSource();
    report.mpd.currentSource = report.sourceManager.activeSource;

    MediaStatus mediaStatus;
    BluetoothStatus bluetoothStatus;
    {
        std::jthread mediaProbe{[this, &mediaStatus] {
            try {
                mediaStatus = mediaPlayer_.status();
            } catch (const std::exception& error) {
                mediaStatus.error = error.what();
            } catch (...) {
                mediaStatus.error = "Unexpected MPD status error";
            }
        }};
        std::jthread bluetoothProbe{[this, &bluetoothStatus] {
            try {
                bluetoothStatus = bluetooth_.status();
            } catch (const std::exception& error) {
                bluetoothStatus.error = error.what();
            } catch (...) {
                bluetoothStatus.error = "Unexpected Bluetooth status error";
            }
        }};
    }

    report.mpd.available = mediaStatus.available;
    report.mpd.state = mediaStatus.state;
    report.mpd.error = mediaStatus.error;
    if (mediaStatus.currentTrack.has_value()) {
        const auto& track = *mediaStatus.currentTrack;
        report.mpd.currentTrack = track.title.empty() ? track.uri : track.title;
        if (!track.artist.empty()) report.mpd.artist = track.artist;
    }

    report.bluetooth.serviceAvailable = bluetoothStatus.serviceAvailable;
    report.bluetooth.adapterPresent = bluetoothStatus.adapterAvailable;
    report.bluetooth.adapterPowered = bluetoothStatus.powered;
    report.bluetooth.discoverable = bluetoothStatus.discoverable;
    report.bluetooth.error = bluetoothStatus.error;
    if (bluetoothStatus.activeAudioDevice.has_value()) {
        const auto& device = *bluetoothStatus.activeAudioDevice;
        if (!device.mac.empty()) report.bluetooth.connectedDevice = device.mac;
        if (!device.name.empty()) report.bluetooth.connectedDeviceName = device.name;
    }

    report.collectionDuration = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - collectionStartedAt);
    if (report.collectionDuration >= collectionBudget) {
        report.warnings.emplace_back("System status collection exceeded the 200 ms budget");
    }
    return report;
}

}  // namespace x308
