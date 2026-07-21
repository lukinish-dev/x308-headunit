#include "x308/Cli.hpp"

#include <string_view>

namespace x308 {

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
        "  bluetooth ...  Управление Bluetooth\n";
}

}  // namespace x308

