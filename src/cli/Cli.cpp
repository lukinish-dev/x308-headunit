#include "x308/Cli.hpp"

#include "x308/Interfaces.hpp"
#include "x308/SourceManager.hpp"
#include "x308/SystemStatusPresenter.hpp"
#include "x308/SystemStatusService.hpp"

#include <ostream>
#include <iomanip>
#include <sstream>
#include <string_view>

namespace x308 {
namespace {

std::string_view stateName(const PlaybackState state) {
    switch (state) {
        case PlaybackState::playing: return "воспроизведение";
        case PlaybackState::paused: return "пауза";
        case PlaybackState::stopped: return "остановлено";
        case PlaybackState::unknown: return "неизвестно";
    }
    return "неизвестно";
}

void printTrack(const Track& track, std::ostream& output) {
    output << "URI: " << track.uri << '\n'
           << "Название: " << (track.title.empty() ? "—" : track.title) << '\n'
           << "Исполнитель: " << (track.artist.empty() ? "—" : track.artist) << '\n'
           << "Альбом: " << (track.album.empty() ? "—" : track.album) << '\n';
}

int printResult(const Result& result, std::ostream& output, std::ostream& error) {
    if (result.success) {
        output << "Готово: операция выполнена.\n";
        return 0;
    }
    error << "Ошибка: " << result.message << '\n';
    return 1;
}

bool parseSwitch(const std::string& value, bool& enabled) {
    if (value == "on") {
        enabled = true;
        return true;
    }
    if (value == "off") {
        enabled = false;
        return true;
    }
    return false;
}

void printBluetoothDevice(const BluetoothDevice& device, std::ostream& output) {
    output << "Имя: " << (device.name.empty() ? "—" : device.name) << '\n'
           << "MAC: " << device.mac << '\n'
           << "Сопряжено: " << (device.paired ? "да" : "нет") << '\n'
           << "Доверено: " << (device.trusted ? "да" : "нет") << '\n'
           << "Подключено: " << (device.connected ? "да" : "нет") << '\n'
           << "Доступно сейчас: " << (device.available ? "да" : "нет") << "\n\n";
}

std::string mediaTime(const std::uint64_t milliseconds) {
    const auto totalSeconds = milliseconds / 1000U;
    std::ostringstream rendered;
    rendered << totalSeconds / 60U << ':' << std::setw(2) << std::setfill('0')
             << totalSeconds % 60U;
    return rendered.str();
}

void printBluetoothMedia(const BluetoothMediaStatus& status, std::ostream& output) {
    output << "AVRCP-плеер: доступен\n"
           << "Устройство: " << (status.deviceName.empty() ? "—" : status.deviceName) << '\n'
           << "MAC: " << (status.deviceAddress.empty() ? "—" : status.deviceAddress) << '\n'
           << "Состояние: " << stateName(status.state) << '\n';
    if (status.currentTrack.has_value()) {
        const auto& track = *status.currentTrack;
        output << "Название: " << (track.title.empty() ? "—" : track.title) << '\n'
               << "Исполнитель: " << (track.artist.empty() ? "—" : track.artist) << '\n'
               << "Альбом: " << (track.album.empty() ? "—" : track.album) << '\n';
    } else {
        output << "Метаданные трека отсутствуют.\n";
    }
    if (status.durationMilliseconds.has_value()) {
        output << "Длительность: " << mediaTime(*status.durationMilliseconds) << '\n';
    }
    if (status.positionMilliseconds.has_value()) {
        output << "Позиция: " << mediaTime(*status.positionMilliseconds) << '\n';
    }
}

}  // namespace

CliArguments CliParser::parse(const int argc, const char* const* argv) {
    CliArguments result;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "--help" || argument == "-h") {
            result.help = true;
        } else if (argument == "--version") {
            result.version = true;
        } else if (argument == "--config") {
            if (++index >= argc) {
                throw CliError("Для --config требуется путь");
            }
            result.configPath = argv[index];
        } else if (argument.starts_with('-')) {
            throw CliError("Неизвестный параметр: " + std::string{argument});
        } else {
            result.command.emplace_back(argument);
        }
    }
    return result;
}

std::string CliParser::helpText() {
    return
        "Jaguar X308 Head Unit\n"
        "Использование: x308-headunit [--config <путь>] [команда]\n\n"
        "  --help, -h     Показать справку\n"
        "  --version      Показать версию\n"
        "  status         Показать состояние системы\n"
        "  source ...     Управление источником\n"
        "  mpd ...        Управление MPD\n"
        "  bluetooth ...  Устройства Bluetooth и AVRCP\n"
        "    current | play | pause | toggle | next | previous\n";
}

Cli::Cli(IMediaPlayer& mediaPlayer, IBluetoothManager& bluetooth,
         IBluetoothMediaController& bluetoothMedia,
         SourceManager& sourceManager, SystemStatusService& systemStatus,
         std::ostream& output, std::ostream& error)
    : mediaPlayer_(mediaPlayer),
      bluetooth_(bluetooth),
      bluetoothMedia_(bluetoothMedia),
      sourceManager_(sourceManager),
      systemStatus_(systemStatus),
      output_(output),
      error_(error) {}

int Cli::run(const std::vector<std::string>& command) const {
    if (command.empty()) {
        error_ << "Ошибка: команда не указана\n";
        return 2;
    }
    if (command.size() == 1 && command.front() == "status") {
        SystemStatusPresenter::print(systemStatus_.collect(), output_);
        return 0;
    }
    if (command.front() == "mpd") {
        if (command.size() < 2) {
            error_ << "Ошибка: укажите команду MPD\n";
            return 2;
        }
        const auto& action = command[1];
        if (action == "status" || action == "current") {
            const auto status = mediaPlayer_.status();
            if (!status.available) {
                error_ << "MPD недоступен: " << status.error << '\n';
                return 1;
            }
            if (action == "status") {
                output_ << "MPD: доступен\nСостояние: " << stateName(status.state)
                        << "\nRandom: " << (status.random ? "on" : "off")
                        << "\nRepeat: " << (status.repeat ? "on" : "off") << '\n';
            }
            if (status.currentTrack.has_value()) printTrack(*status.currentTrack, output_);
            else output_ << "Текущий трек отсутствует.\n";
            return 0;
        }
        if (action == "play") return printResult(mediaPlayer_.play(), output_, error_);
        if (action == "pause") return printResult(mediaPlayer_.pause(), output_, error_);
        if (action == "stop") return printResult(mediaPlayer_.stop(), output_, error_);
        if (action == "toggle") return printResult(mediaPlayer_.togglePause(), output_, error_);
        if (action == "next") return printResult(mediaPlayer_.next(), output_, error_);
        if (action == "previous") return printResult(mediaPlayer_.previous(), output_, error_);
        if (action == "queue" && command.size() == 3 && command[2] == "clear") {
            return printResult(mediaPlayer_.clearQueue(), output_, error_);
        }
        if (action == "queue" && command.size() == 2) {
            const auto queue = mediaPlayer_.queue();
            if (!mediaPlayer_.lastError().empty()) {
                error_ << "Ошибка: " << mediaPlayer_.lastError() << '\n';
                return 1;
            }
            if (queue.empty()) output_ << "Очередь пуста.\n";
            for (std::size_t index = 0; index < queue.size(); ++index) {
                output_ << index + 1 << ". "
                        << (queue[index].title.empty() ? queue[index].uri : queue[index].title)
                        << '\n';
            }
            return 0;
        }
        if (action == "library" && command.size() <= 3) {
            const std::string path = command.size() == 3 ? command[2] : "";
            const auto entries = mediaPlayer_.library(path);
            if (!mediaPlayer_.lastError().empty()) {
                error_ << "Ошибка: " << mediaPlayer_.lastError() << '\n';
                return 1;
            }
            if (entries.empty()) output_ << "Папка пуста.\n";
            for (const auto& entry : entries) {
                output_ << (entry.directory ? "[DIR] " : "      ") << entry.path << '\n';
            }
            return 0;
        }
        if ((action == "add" || action == "add-folder") && command.size() == 3) {
            const auto result = action == "add" ? mediaPlayer_.add(command[2])
                                                 : mediaPlayer_.addFolder(command[2]);
            return printResult(result, output_, error_);
        }
        if ((action == "random" || action == "repeat") && command.size() == 3) {
            bool enabled = false;
            if (!parseSwitch(command[2], enabled)) {
                error_ << "Ошибка: ожидается on или off\n";
                return 2;
            }
            const auto result = action == "random" ? mediaPlayer_.setRandom(enabled)
                                                    : mediaPlayer_.setRepeat(enabled);
            return printResult(result, output_, error_);
        }
        if (action == "update" && command.size() == 2) {
            return printResult(mediaPlayer_.update(), output_, error_);
        }
        error_ << "Неизвестная или неполная команда MPD\n" << CliParser::helpText();
        return 2;
    }
    if (command.front() == "bluetooth") {
        if (command.size() < 2) {
            error_ << "Ошибка: укажите команду Bluetooth\n";
            return 2;
        }
        const auto& action = command[1];
        if (action == "current" && command.size() == 2) {
            const auto media = bluetoothMedia_.status();
            if (!media.available) {
                error_ << "Bluetooth-медиаплеер недоступен: " << media.error << '\n';
                return 1;
            }
            printBluetoothMedia(media, output_);
            return 0;
        }
        if (command.size() == 2 && action == "play") {
            return printResult(bluetoothMedia_.play(), output_, error_);
        }
        if (command.size() == 2 && action == "pause") {
            return printResult(bluetoothMedia_.pause(), output_, error_);
        }
        if (command.size() == 2 && action == "toggle") {
            return printResult(bluetoothMedia_.togglePause(), output_, error_);
        }
        if (command.size() == 2 && action == "next") {
            return printResult(bluetoothMedia_.next(), output_, error_);
        }
        if (command.size() == 2 && action == "previous") {
            return printResult(bluetoothMedia_.previous(), output_, error_);
        }
        if (action == "status" && command.size() == 2) {
            const auto status = bluetooth_.status();
            output_ << "Служба Bluetooth: "
                    << (status.serviceAvailable ? "доступна" : "недоступна")
                    << "\nАдаптер: " << (status.adapterAvailable ? "доступен" : "недоступен")
                    << "\nПитание: " << (status.powered ? "включено" : "выключено")
                    << "\nСканирование: " << (status.discovering ? "идёт" : "выключено")
                    << "\nPairable: " << (status.pairable ? "да" : "нет")
                    << "\nDiscoverable: " << (status.discoverable ? "да" : "нет") << '\n';
            if (status.activeAudioDevice.has_value()) {
                output_ << "Активное Bluetooth-аудиоустройство:\n";
                printBluetoothDevice(*status.activeAudioDevice, output_);
            }
            if (!status.error.empty()) {
                error_ << "Ошибка: " << status.error << '\n';
                return 1;
            }
            return status.adapterAvailable ? 0 : 1;
        }
        if (action == "power" && command.size() == 3) {
            if (command[2] == "on") {
                return printResult(bluetooth_.setPower(true), output_, error_);
            }
            if (command[2] == "off") {
                return printResult(bluetooth_.setPower(false), output_, error_);
            }
        }
        if (action == "scan" && command.size() == 2) {
            return printResult(bluetooth_.startScan(), output_, error_);
        }
        if (action == "scan" && command.size() == 3 && command[2] == "stop") {
            return printResult(bluetooth_.stopScan(), output_, error_);
        }
        if ((action == "devices" || action == "paired" || action == "trusted") &&
            command.size() == 2) {
            const auto devices = action == "paired" ? bluetooth_.pairedDevices()
                : action == "trusted" ? bluetooth_.trustedDevices()
                                      : bluetooth_.devices();
            if (!bluetooth_.lastError().empty()) {
                error_ << "Ошибка: " << bluetooth_.lastError() << '\n';
                return 1;
            }
            bool shown = false;
            for (const auto& device : devices) {
                printBluetoothDevice(device, output_);
                shown = true;
            }
            if (!shown) output_ << "Устройства не найдены.\n";
            return 0;
        }
        if (action == "pairing-mode" && command.size() == 3) {
            if (command[2] == "on") {
                return printResult(bluetooth_.setPairingMode(true), output_, error_);
            }
            if (command[2] == "off") {
                return printResult(bluetooth_.setPairingMode(false), output_, error_);
            }
        }
        if (action == "auto-connect" && command.size() == 2) {
            return printResult(bluetooth_.autoConnect(), output_, error_);
        }
        if (command.size() == 3) {
            const auto& mac = command[2];
            if (action == "pair") return printResult(bluetooth_.pair(mac), output_, error_);
            if (action == "trust") return printResult(bluetooth_.trust(mac, true), output_, error_);
            if (action == "untrust") return printResult(bluetooth_.trust(mac, false), output_, error_);
            if (action == "connect") return printResult(bluetooth_.connect(mac), output_, error_);
            if (action == "disconnect") return printResult(bluetooth_.disconnect(mac), output_, error_);
            if (action == "remove") return printResult(bluetooth_.remove(mac), output_, error_);
        }
        error_ << "Неизвестная или неполная команда Bluetooth\n" << CliParser::helpText();
        return 2;
    }
    if (command.front() == "source") {
        if (command.size() == 2 && command[1] == "status") {
            output_ << "Активный источник: "
                    << SourceManager::name(sourceManager_.activeSource()) << '\n';
            return 0;
        }
        if (command.size() == 3 && command[1] == "set") {
            if (command[2] == "mpd") {
                return printResult(sourceManager_.setSource(AudioSource::mpd), output_, error_);
            }
            if (command[2] == "bluetooth") {
                return printResult(sourceManager_.setSource(AudioSource::bluetooth), output_, error_);
            }
        }
        error_ << "Неизвестная или неполная команда source\n" << CliParser::helpText();
        return 2;
    }

    error_ << "Команда пока не реализована: " << command.front() << '\n'
           << CliParser::helpText();
    return 2;
}

}  // namespace x308
