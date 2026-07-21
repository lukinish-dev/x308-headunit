#include "x308/App.hpp"

#include "x308/Cli.hpp"
#include "x308/Configuration.hpp"
#include "x308/InteractiveMenu.hpp"
#include "x308/Logger.hpp"

#include <iostream>
#include <string>

namespace x308 {

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

    try {
        const Configuration config = ConfigurationLoader::load(arguments.configPath);
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

    if (arguments.command.empty()) {
        return InteractiveMenu{}.run(std::cin, std::cout);
    }

    std::cerr << "Команда пока не реализована: " << arguments.command.front() << '\n';
    return 2;
}

}  // namespace x308
