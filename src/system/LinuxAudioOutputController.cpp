#include "x308/LinuxAudioOutputController.hpp"

#include <algorithm>
#include <cctype>
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

std::string processFailure(const ProcessResult& result, const std::string_view action) {
    if (result.timedOut) return std::string{action} + " timed out";
    if (!result.standardError.empty()) return result.standardError;
    if (!result.standardOutput.empty()) return result.standardOutput;
    return std::string{action} + " failed with exit code " + std::to_string(result.exitCode);
}

}  // namespace

LinuxAudioOutputController::LinuxAudioOutputController(
    AudioConfig config, std::shared_ptr<IProcessRunner> processRunner)
    : config_(std::move(config)),
      processRunner_(std::move(processRunner)),
      timeout_(config_.commandTimeoutMilliseconds) {
    if (processRunner_ == nullptr) throw std::invalid_argument("process runner is required");
    if (config_.bluetoothBackend != "bluealsa") {
        throw std::invalid_argument("unsupported Bluetooth audio backend: " +
                                    config_.bluetoothBackend);
    }
    if (!validServiceName(config_.bluealsaAplayService)) {
        throw std::invalid_argument("invalid bluealsa-aplay service name");
    }
    if (timeout_ <= std::chrono::milliseconds::zero()) {
        throw std::invalid_argument("audio command timeout must be positive");
    }
}

Result LinuxAudioOutputController::setBluealsaAplayActive(const bool active) {
    const std::string action = active ? "start" : "stop";
    const auto change = processRunner_->run(
        "systemctl", {action, config_.bluealsaAplayService}, timeout_);
    if (change.exitCode != 0 || change.timedOut) {
        return Result::error(processFailure(change, "systemctl " + action));
    }

    const auto verification = processRunner_->run(
        "systemctl", {"is-active", "--quiet", config_.bluealsaAplayService}, timeout_);
    if (verification.timedOut) {
        return Result::error("bluealsa-aplay readiness check timed out");
    }
    const bool stateMatches = active ? verification.exitCode == 0 : verification.exitCode != 0;
    if (!stateMatches) {
        return Result::error(active ? "bluealsa-aplay is not active after start"
                                    : "bluealsa-aplay is still active after stop");
    }
    return Result::ok(active ? "Bluetooth audio receiver is ready"
                             : "Bluetooth audio receiver released the ALSA PCM");
}

Result LinuxAudioOutputController::selectSource(const AudioSource source) {
    if (source == AudioSource::carPlay) return Result::error("CarPlay audio is unavailable");
    return setBluealsaAplayActive(source == AudioSource::bluetooth);
}

std::string LinuxAudioOutputController::currentDevice() const { return config_.alsaPcm; }

}  // namespace x308
