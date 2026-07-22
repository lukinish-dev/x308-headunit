#include "x308/Configuration.hpp"
#include "x308/MpdClient.hpp"
#include "x308/BluetoothCtlManager.hpp"
#include "x308/ProcessRunner.hpp"

#include <filesystem>
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
