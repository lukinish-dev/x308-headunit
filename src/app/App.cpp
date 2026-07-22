#include "x308/App.hpp"

#include "x308/AppContext.hpp"
#include "x308/BluetoothCtlManager.hpp"
#include "x308/Cli.hpp"
#include "x308/Configuration.hpp"
#include "x308/HardwareStubs.hpp"
#include "x308/InteractiveMenu.hpp"
#include "x308/Logger.hpp"
#include "x308/MpdClient.hpp"
#include "x308/ProcessRunner.hpp"
#include "x308/SourceManager.hpp"
#include "x308/SystemStatusService.hpp"

#include <chrono>
#include <exception>
#include <iostream>
#include <memory>

namespace x308 {

Application::Application() : Application(std::cin, std::cout, std::cerr) {}

Application::Application(std::istream& input, std::ostream& output, std::ostream& error)
    : input_(input), output_(output), error_(error) {}

Application::~Application() { shutdown(); }

ApplicationState Application::state() const noexcept { return state_; }

int Application::run(const int argc, const char* const* argv) {
    if (state_ != ApplicationState::created) {
        error_ << "Ошибка: экземпляр Application уже был запущен\n";
        return 2;
    }

    state_ = ApplicationState::initialized;
    const auto applicationStartedAt = std::chrono::steady_clock::now();
    CliArguments arguments;
    try {
        arguments = CliParser::parse(argc, argv);
    } catch (const CliError& error) {
        error_ << "Ошибка: " << error.what() << '\n';
        return finish(2);
    }

    if (arguments.help) {
        state_ = ApplicationState::running;
        output_ << CliParser::helpText();
        return finish(0);
    }
    if (arguments.version) {
        state_ = ApplicationState::running;
        output_ << "x308-headunit " << X308_VERSION << '\n';
        return finish(0);
    }

    try {
        auto context = std::make_unique<AppContext>();
        context->configuration = std::make_unique<Configuration>(
            ConfigurationLoader::load(arguments.configPath));
        auto& configuration = *context->configuration;

        context->logger = std::make_unique<Logger>(configuration.logging.level);
        if (configuration.loadedFrom.has_value()) {
            context->logger->log(
                LogLevel::debug,
                "Configuration loaded from " + configuration.loadedFrom->string());
        } else {
            context->logger->log(
                LogLevel::warning, "No configuration file found; using defaults");
        }

        context->processRunner = std::make_shared<PosixProcessRunner>();
        const auto timeout = static_cast<unsigned>(
            configuration.application.startupTimeoutSeconds) * 1000U;
        context->mpd = std::make_unique<MpdClient>(configuration.mpd, timeout);
        context->bluetooth = std::make_unique<BluetoothCtlManager>(
            configuration.bluetooth, context->processRunner, context->logger.get());
        context->audioOutput = std::make_unique<NullAudioOutput>();

        const auto initialSource = configuration.audio.defaultSource == "bluetooth"
            ? AudioSource::bluetooth : AudioSource::mpd;
        context->sourceManager = std::make_unique<SourceManager>(
            *context->mpd, *context->bluetooth, *context->audioOutput, initialSource);
        context->systemStatus = std::make_unique<SystemStatusService>(
            *context->mpd, *context->bluetooth, *context->sourceManager,
            configuration.mpd.musicDirectory, X308_VERSION, X308_BUILD_TYPE,
            applicationStartedAt);
        context->cli = std::make_unique<Cli>(
            *context->mpd, *context->bluetooth, *context->sourceManager,
            *context->systemStatus, output_, error_);
        context->interactiveMenu = std::make_unique<InteractiveMenu>(
            context->mpd.get(), context->bluetooth.get(), context->sourceManager.get(),
            context->systemStatus.get());

        context_ = std::move(context);
        state_ = ApplicationState::running;
    } catch (const ConfigurationError& error) {
        error_ << "Ошибка конфигурации: " << error.what() << '\n';
        return finish(2);
    } catch (const std::exception& error) {
        error_ << "Ошибка инициализации: " << error.what() << '\n';
        return finish(1);
    }

    try {
        const int exitCode = arguments.command.empty()
            ? context_->interactiveMenu->run(input_, output_)
            : context_->cli->run(arguments.command);
        return finish(exitCode);
    } catch (const std::exception& error) {
        error_ << "Ошибка выполнения: " << error.what() << '\n';
        return finish(1);
    } catch (...) {
        error_ << "Ошибка выполнения: неизвестное исключение\n";
        return finish(1);
    }
}

void Application::shutdown() noexcept {
    if (state_ == ApplicationState::stopping || state_ == ApplicationState::stopped) return;
    state_ = ApplicationState::stopping;
    context_.reset();
    state_ = ApplicationState::stopped;
}

int Application::finish(const int exitCode) noexcept {
    shutdown();
    return exitCode;
}

}  // namespace x308
