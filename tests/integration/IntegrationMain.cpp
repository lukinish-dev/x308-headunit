#include "x308/Configuration.hpp"
#include "x308/MpdClient.hpp"
#include "x308/BluetoothCtlManager.hpp"
#include "x308/ProcessRunner.hpp"
#include "x308/HardwareStubs.hpp"
#include "x308/SourceManager.hpp"
#include "x308/SystemStatusService.hpp"

#include <filesystem>
#include <chrono>
#include <iostream>
#include <memory>
#include <string_view>

int main(const int argc, const char* const* argv) {
    if (argc != 2) {
        std::cerr << "Unknown integration test\n";
        return 2;
    }
    if (std::string_view{argv[1]} == "bluetooth") {
        const auto config = x308::ConfigurationLoader::load();
        auto runner = std::make_shared<x308::PosixProcessRunner>();
        x308::BluetoothCtlManager bluetooth{config.bluetooth, runner};
        const auto status = bluetooth.status();
        if (!status.adapterAvailable) {
            std::cerr << "Bluetooth unavailable: " << status.error << '\n';
            return 1;
        }
        std::cout << "Bluetooth adapter is available\n";
        return 0;
    }
    if (std::string_view{argv[1]} == "status") {
        const auto config = x308::ConfigurationLoader::load();
        constexpr auto mpdTimeout = std::chrono::milliseconds{180};
        constexpr auto bluetoothTimeout = std::chrono::milliseconds{100};
        x308::MpdClient mpd{config.mpd, static_cast<unsigned>(mpdTimeout.count())};
        auto runner = std::make_shared<x308::PosixProcessRunner>(
            bluetoothTimeout, std::chrono::milliseconds{10});
        x308::BluetoothCtlManager bluetooth{config.bluetooth, runner};
        x308::NullAudioOutput audioOutput;
        const auto initialSource = config.audio.defaultSource == "bluetooth"
            ? x308::AudioSource::bluetooth : x308::AudioSource::mpd;
        x308::SourceManager sourceManager{mpd, bluetooth, audioOutput, initialSource};
        x308::SystemStatusService service{
            mpd, bluetooth, sourceManager, config.mpd.musicDirectory, "integration", "Debug"};
        const auto report = service.collect();
        if (report.collectionDuration >= x308::SystemStatusService::collectionBudget) {
            std::cerr << "System status exceeded 200 ms: "
                      << report.collectionDuration.count() << " us\n";
            return 1;
        }
        if (!report.mpd.available || !report.bluetooth.adapterPresent ||
            !report.storage.accessible) {
            std::cerr << "System status is incomplete: mpd=" << report.mpd.available
                      << " bluetooth=" << report.bluetooth.adapterPresent
                      << " storage=" << report.storage.accessible
                      << " mpd_error=" << report.mpd.error
                      << " bluetooth_error=" << report.bluetooth.error << '\n';
            return 1;
        }
        std::cout << "System status collected in " << report.collectionDuration.count()
                  << " us without changing system state\n";
        return 0;
    }
    if (std::string_view{argv[1]} != "mpd") {
        std::cerr << "Unknown integration test\n";
        return 2;
    }
    const auto config = x308::ConfigurationLoader::load();
    x308::MpdClient client{config.mpd};
    const auto status = client.status();
    if (!status.available) {
        std::cerr << "MPD unavailable: " << status.error << '\n';
        return 1;
    }
    if (!std::filesystem::is_directory(config.mpd.musicDirectory)) {
        std::cerr << "Music directory unavailable\n";
        return 1;
    }
    std::cout << "MPD and music directory are available\n";
    return 0;
}
