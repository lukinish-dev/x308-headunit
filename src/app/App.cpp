#include "x308/App.hpp"

#include "x308/Cli.hpp"
#include "x308/Configuration.hpp"
#include "x308/InteractiveMenu.hpp"
#include "x308/Logger.hpp"
#include "x308/MpdClient.hpp"

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

    if (arguments.command.empty()) {
        return InteractiveMenu{&mpd}.run(std::cin, std::cout);
    }

    if (arguments.command.front() == "mpd") return runMpdCommand(mpd, arguments.command);

    std::cerr << "Команда пока не реализована: " << arguments.command.front() << '\n';
    return 2;
}

}  // namespace x308
