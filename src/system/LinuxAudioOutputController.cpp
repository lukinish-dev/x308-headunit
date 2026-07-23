#include "x308/LinuxAudioOutputController.hpp"

#include "x308/Logger.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <thread>
#include <stdexcept>
#include <utility>

namespace x308 {
namespace {

bool validServiceName(const std::string& value) {
    if (value.empty() || value.front() == '-') return false;
    return std::all_of(value.begin(), value.end(), [](const unsigned char character) {
        return std::isalnum(character) != 0 || character == '.' || character == '_' ||
               character == '-' || character == '@';
    });
}

constexpr auto serviceStateTimeout = std::chrono::seconds{1};
constexpr auto serviceStartTimeout = std::chrono::seconds{2};
constexpr auto serviceReadinessTimeout = std::chrono::seconds{3};
constexpr auto serviceReadinessPollInterval = std::chrono::milliseconds{100};

std::string processFailure(const ProcessResult& result, const std::string_view action) {
    if (result.timedOut) return std::string{action} + " timed out";
    if (!result.standardError.empty()) {
        return std::string{action} + ": " + result.standardError;
    }
    if (!result.standardOutput.empty()) {
        return std::string{action} + ": " + result.standardOutput;
    }
    return std::string{action} + " failed with exit code " + std::to_string(result.exitCode);
}

}  // namespace

LinuxAudioOutputController::LinuxAudioOutputController(
    AudioConfig config, std::shared_ptr<IProcessRunner> processRunner, Logger* logger)
    : config_(std::move(config)),
      processRunner_(std::move(processRunner)),
      logger_(logger) {
    if (processRunner_ == nullptr) throw std::invalid_argument("process runner is required");
    if (config_.bluetoothBackend != "bluealsa") {
        throw std::invalid_argument("unsupported Bluetooth audio backend: " +
                                    config_.bluetoothBackend);
    }
    if (!validServiceName(config_.bluealsaAplayService)) {
        throw std::invalid_argument("invalid bluealsa-aplay service name");
    }
    if (config_.commandTimeoutMilliseconds <= 0) {
        throw std::invalid_argument("audio command timeout must be positive");
    }
}

ProcessResult LinuxAudioOutputController::serviceState() const {
    return processRunner_->run(
        "systemctl", {"--no-ask-password", "is-active", "--quiet",
                       config_.bluealsaAplayService}, serviceStateTimeout);
}

Result LinuxAudioOutputController::waitForServiceState(const bool active) const {
    const auto deadline = std::chrono::steady_clock::now() + serviceReadinessTimeout;
    while (true) {
        const auto state = serviceState();
        if (state.timedOut) {
            return Result::error("bluealsa-aplay service state query timed out");
        }
        const bool matches = active ? state.exitCode == 0 : state.exitCode != 0;
        if (matches) {
            return Result::ok(active ? "Bluetooth audio service is active"
                                     : "Bluetooth audio service is inactive");
        }
        if (std::chrono::steady_clock::now() >= deadline) break;
        std::this_thread::sleep_for(serviceReadinessPollInterval);
    }
    return Result::error(active ? "bluealsa-aplay service remains inactive"
                                : "bluealsa-aplay service remains active");
}

Result LinuxAudioOutputController::setBluealsaAplayActive(const bool active) {
    const auto current = serviceState();
    if (current.timedOut) {
        return Result::error("bluealsa-aplay service state query timed out");
    }
    if (current.exitCode != 0 && !current.standardError.empty()) {
        return Result::error(processFailure(current, "bluealsa-aplay service state query"));
    }
    const bool alreadyActive = current.exitCode == 0;

    if (active && alreadyActive) {
        if (logger_ != nullptr) {
            logger_->log(LogLevel::info, "Bluetooth audio service is already active");
        }
        return Result::ok("Bluetooth audio service is already active.");
    }
    if (!active && !alreadyActive) {
        if (logger_ != nullptr) {
            logger_->log(LogLevel::info, "Bluetooth audio service is already inactive");
        }
        return Result::ok("Bluetooth audio service is already inactive.");
    }

    const std::string action = active ? "start" : "stop";
    const auto change = processRunner_->run(
        "systemctl", {"--no-ask-password", action, config_.bluealsaAplayService},
        serviceStartTimeout);
    if (change.exitCode != 0 || change.timedOut) {
        const auto afterFailure = serviceState();
        if (!afterFailure.timedOut &&
            (active ? afterFailure.exitCode == 0 : afterFailure.exitCode != 0)) {
            if (logger_ != nullptr) {
                logger_->log(LogLevel::warning,
                             "systemctl " + action + " failed, but Bluetooth audio service " +
                                 (active ? "became active" : "is inactive"));
            }
            return Result::ok("Предупреждение: systemctl " + action +
                              " завершился с ошибкой, но состояние Bluetooth audio готово.");
        }
        if (change.timedOut) {
            return Result::error("systemctl " + action +
                                 " timed out and Bluetooth audio service remains " +
                                 (active ? "inactive" : "active"));
        }
        return Result::error(processFailure(
            change, "systemctl " + action + " (non-interactive authorization unavailable)"));
    }

    const auto readiness = waitForServiceState(active);
    if (!readiness.success) {
        return Result::error("Bluetooth audio service " + action + " succeeded, but " +
                             readiness.message);
    }
    if (logger_ != nullptr) {
        logger_->log(LogLevel::info, active ? "Bluetooth audio service started"
                                            : "Bluetooth audio service stopped");
    }
    return Result::ok(active ? "Bluetooth audio service started."
                             : "Bluetooth audio service stopped.");
}

Result LinuxAudioOutputController::selectSource(const AudioSource source) {
    if (source == AudioSource::carPlay) return Result::error("CarPlay audio is unavailable");
    return setBluealsaAplayActive(source == AudioSource::bluetooth);
}

std::string LinuxAudioOutputController::currentDevice() const { return config_.alsaPcm; }

}  // namespace x308
