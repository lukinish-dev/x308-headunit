#include "x308/App.hpp"

#include "x308/Cli.hpp"
#include "x308/Configuration.hpp"
#include "x308/InteractiveMenu.hpp"
#include "x308/Logger.hpp"
#include "x308/MpdClient.hpp"
#include "x308/BluetoothCtlManager.hpp"
#include "x308/HardwareStubs.hpp"
#include "x308/ProcessRunner.hpp"
#include "x308/SourceManager.hpp"

#include <iostream>
#include <string>

namespace x308 {
namespace {

std::string_view stateName(const PlaybackState state) {
    switch (state) {
        case PlaybackState::playing: return "playing";
        case PlaybackState::paused: return "paused";
        case PlaybackState::stopped: return "stopped";
        case PlaybackState::unknown: return "unknown";
    }
    return "unknown";
}

void printTrack(const Track& track) {
    std::cout << "URI: " << track.uri << '\n'
              << "Название: " << (track.title.empty() ? "—" : track.title) << '\n'
              << "Исполнитель: " << (track.artist.empty() ? "—" : track.artist) << '\n'
              << "Альбом: " << (track.album.empty() ? "—" : track.album) << '\n';
}

int printResult(const Result& result) {
    (result.success ? std::cout : std::cerr) << (result.success ? "Готово: " : "Ошибка: ")
                                             << result.message << '\n';
    return result.success ? 0 : 1;
}

bool parseSwitch(const std::string& value, bool& enabled) {
    if (value == "on") { enabled = true; return true; }
    if (value == "off") { enabled = false; return true; }
    return false;
}

int runMpdCommand(MpdClient& client, const std::vector<std::string>& command) {
    if (command.size() < 2) {
        std::cerr << "Ошибка: укажите команду MPD\n";
        return 2;
    }
    const auto& action = command[1];
    if (action == "status" || action == "current") {
        const auto status = client.status();
        if (!status.available) {
            std::cerr << "MPD недоступен: " << status.error << '\n';
            return 1;
        }
        if (action == "status") {
            std::cout << "MPD: доступен\nСостояние: " << stateName(status.state)
                      << "\nRandom: " << (status.random ? "on" : "off")
                      << "\nRepeat: " << (status.repeat ? "on" : "off") << '\n';
        }
        if (status.currentTrack.has_value()) printTrack(*status.currentTrack);
        else std::cout << "Текущий трек отсутствует.\n";
        return 0;
    }
    if (action == "play") return printResult(client.play());
    if (action == "pause") return printResult(client.pause());
    if (action == "stop") return printResult(client.stop());
    if (action == "toggle") return printResult(client.togglePause());
    if (action == "next") return printResult(client.next());
    if (action == "previous") return printResult(client.previous());
    if (action == "queue" && command.size() == 3 && command[2] == "clear") return printResult(client.clearQueue());
    if (action == "queue" && command.size() == 2) {
        const auto queue = client.queue();
        if (!client.lastError().empty()) { std::cerr << "Ошибка: " << client.lastError() << '\n'; return 1; }
        if (queue.empty()) std::cout << "Очередь пуста.\n";
        for (std::size_t index = 0; index < queue.size(); ++index) {
            std::cout << index + 1 << ". " << (queue[index].title.empty() ? queue[index].uri : queue[index].title) << '\n';
        }
        return 0;
    }
    if (action == "library" && command.size() <= 3) {
        const std::string path = command.size() == 3 ? command[2] : "";
        const auto entries = client.library(path);
        if (!client.lastError().empty()) { std::cerr << "Ошибка: " << client.lastError() << '\n'; return 1; }
        if (entries.empty()) std::cout << "Папка пуста.\n";
        for (const auto& entry : entries) std::cout << (entry.directory ? "[DIR] " : "      ") << entry.path << '\n';
        return 0;
    }
    if ((action == "add" || action == "add-folder") && command.size() == 3) {
        return printResult(action == "add" ? client.add(command[2]) : client.addFolder(command[2]));
    }
    if ((action == "random" || action == "repeat") && command.size() == 3) {
        bool enabled = false;
        if (!parseSwitch(command[2], enabled)) { std::cerr << "Ошибка: ожидается on или off\n"; return 2; }
        return printResult(action == "random" ? client.setRandom(enabled) : client.setRepeat(enabled));
    }
    if (action == "update" && command.size() == 2) return printResult(client.update());
    std::cerr << "Неизвестная или неполная команда MPD\n";
    return 2;
}

void printBluetoothDevice(const BluetoothDevice& device) {
    std::cout << "Имя: " << (device.name.empty() ? "—" : device.name) << '\n'
              << "MAC: " << device.mac << '\n'
              << "Paired: " << (device.paired ? "yes" : "no") << '\n'
              << "Trusted: " << (device.trusted ? "yes" : "no") << '\n'
              << "Connected: " << (device.connected ? "yes" : "no") << '\n'
              << "Available: " << (device.available ? "yes" : "no") << "\n\n";
}

int runBluetoothCommand(BluetoothCtlManager& bluetooth, const std::vector<std::string>& command) {
    if (command.size() < 2) { std::cerr << "Ошибка: укажите команду Bluetooth\n"; return 2; }
    const auto& action = command[1];
    if (action == "status" && command.size() == 2) {
        const auto status = bluetooth.status();
        std::cout << "Bluetooth service: " << (status.serviceAvailable ? "available" : "unavailable")
                  << "\nAdapter: " << (status.adapterAvailable ? "available" : "unavailable")
                  << "\nPowered: " << (status.powered ? "yes" : "no")
                  << "\nDiscovering: " << (status.discovering ? "yes" : "no")
                  << "\nPairable: " << (status.pairable ? "yes" : "no")
                  << "\nDiscoverable: " << (status.discoverable ? "yes" : "no") << '\n';
        if (status.activeAudioDevice.has_value()) {
            std::cout << "Активное Bluetooth-аудиоустройство:\n";
            printBluetoothDevice(*status.activeAudioDevice);
        }
        if (!status.error.empty()) { std::cerr << "Ошибка: " << status.error << '\n'; return 1; }
        return status.adapterAvailable ? 0 : 1;
    }
    if (action == "power" && command.size() == 3) {
        if (command[2] == "on") return printResult(bluetooth.setPower(true));
        if (command[2] == "off") return printResult(bluetooth.setPower(false));
    }
    if (action == "scan" && command.size() == 2) return printResult(bluetooth.startScan());
    if (action == "scan" && command.size() == 3 && command[2] == "stop") return printResult(bluetooth.stopScan());
    if ((action == "devices" || action == "paired" || action == "trusted") && command.size() == 2) {
        const auto devices = bluetooth.devices();
        if (!bluetooth.lastError().empty()) { std::cerr << "Ошибка: " << bluetooth.lastError() << '\n'; return 1; }
        bool shown = false;
        for (const auto& device : devices) {
            if ((action == "paired" && !device.paired) || (action == "trusted" && !device.trusted)) continue;
            printBluetoothDevice(device);
            shown = true;
        }
        if (!shown) std::cout << "Устройства не найдены.\n";
        return 0;
    }
    if (action == "pairing-mode" && command.size() == 3) {
        if (command[2] == "on") return printResult(bluetooth.setPairingMode(true));
        if (command[2] == "off") return printResult(bluetooth.setPairingMode(false));
    }
    if (action == "auto-connect" && command.size() == 2) return printResult(bluetooth.autoConnect());
    if (command.size() == 3) {
        const auto& mac = command[2];
        if (action == "pair") return printResult(bluetooth.pair(mac));
        if (action == "trust") return printResult(bluetooth.trust(mac, true));
        if (action == "untrust") return printResult(bluetooth.trust(mac, false));
        if (action == "connect") return printResult(bluetooth.connect(mac));
        if (action == "disconnect") return printResult(bluetooth.disconnect(mac));
        if (action == "remove") return printResult(bluetooth.remove(mac));
    }
    std::cerr << "Неизвестная или неполная команда Bluetooth\n";
    return 2;
}

int runSourceCommand(SourceManager& manager, const std::vector<std::string>& command) {
    if (command.size() == 2 && command[1] == "status") {
        std::cout << "Активный источник: " << SourceManager::name(manager.activeSource()) << '\n';
        return 0;
    }
    if (command.size() == 3 && command[1] == "set") {
        if (command[2] == "mpd") return printResult(manager.setSource(AudioSource::mpd));
        if (command[2] == "bluetooth") return printResult(manager.setSource(AudioSource::bluetooth));
    }
    std::cerr << "Неизвестная или неполная команда source\n";
    return 2;
}

}  // namespace

int App::run(const int argc, const char* const* argv) const {
    CliArguments arguments;
    try {
        arguments = CliParser::parse(argc, argv);
    } catch (const CliError& error) {
        std::cerr << "Ошибка: " << error.what() << '\n';
        return 2;
    }

    if (arguments.help) {
        std::cout << CliParser::helpText();
        return 0;
    }
    if (arguments.version) {
        std::cout << "x308-headunit 0.1.0\n";
        return 0;
    }

    Configuration config;
    try {
        config = ConfigurationLoader::load(arguments.configPath);
        Logger::setLevel(config.logging.level);
        if (config.loadedFrom.has_value()) {
            Logger::log(LogLevel::debug, "Configuration loaded from " + config.loadedFrom->string());
        } else {
            Logger::log(LogLevel::warning, "No configuration file found; using defaults");
        }
    } catch (const ConfigurationError& error) {
        std::cerr << "Ошибка конфигурации: " << error.what() << '\n';
        return 2;
    }

    const unsigned timeout = static_cast<unsigned>(config.application.startupTimeoutSeconds) * 1000U;
    MpdClient mpd{config.mpd, timeout};
    auto processRunner = std::make_shared<PosixProcessRunner>();
    BluetoothCtlManager bluetooth{config.bluetooth, processRunner};
    NullAudioOutput audioOutput;
    const AudioSource initialSource = config.audio.defaultSource == "bluetooth"
                                          ? AudioSource::bluetooth : AudioSource::mpd;
    SourceManager sourceManager{mpd, bluetooth, audioOutput, initialSource};

    if (arguments.command.empty()) {
        return InteractiveMenu{&mpd, &bluetooth, &sourceManager}.run(std::cin, std::cout);
    }

    if (arguments.command.front() == "mpd") return runMpdCommand(mpd, arguments.command);
    if (arguments.command.front() == "bluetooth") return runBluetoothCommand(bluetooth, arguments.command);
    if (arguments.command.front() == "source") return runSourceCommand(sourceManager, arguments.command);

    std::cerr << "Команда пока не реализована: " << arguments.command.front() << '\n';
    return 2;
}

}  // namespace x308
