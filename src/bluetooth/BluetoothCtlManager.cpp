#include "x308/BluetoothCtlManager.hpp"

#include "x308/BluezDbusMediaController.hpp"
#include "x308/Logger.hpp"

#include <algorithm>
#include <chrono>
#include <cerrno>
#include <cctype>
#include <csignal>
#include <fcntl.h>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <utility>

namespace x308 {
namespace {

std::string trim(std::string value) {
    const auto nonSpace = [](const unsigned char character) { return std::isspace(character) == 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), nonSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), nonSpace).base(), value.end());
    return value;
}

bool property(std::string_view output, const std::string_view name) {
    const std::string expected = std::string{name} + ": yes";
    return output.find(expected) != std::string_view::npos;
}

std::string propertyValue(std::string_view output, const std::string_view name) {
    std::istringstream lines{std::string{output}};
    std::string line;
    const std::string prefix = std::string{name} + ':';
    while (std::getline(lines, line)) {
        line = trim(std::move(line));
        if (line.starts_with(prefix)) return trim(line.substr(prefix.size()));
    }
    return {};
}

bool commandFailed(const ProcessResult& result) {
    return result.exitCode != 0 || result.timedOut ||
           result.standardOutput.find("Failed") != std::string::npos ||
           result.standardError.find("Failed") != std::string::npos;
}

constexpr auto mediaProbeInterval = std::chrono::milliseconds{150};
constexpr auto initialMediaWait = std::chrono::milliseconds{1800};
constexpr auto reconnectCooldown = std::chrono::milliseconds{500};
constexpr auto mediaStatusLogInterval = std::chrono::seconds{1};
constexpr auto maximumConnectTimeout = std::chrono::seconds{5};
constexpr auto minimumConnectBudget = std::chrono::milliseconds{1000};

std::chrono::milliseconds remainingUntil(
    const std::chrono::steady_clock::time_point deadline) {
    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
        deadline - std::chrono::steady_clock::now());
    return std::max(remaining, std::chrono::milliseconds::zero());
}

std::string lowerCase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

}  // namespace

class BluetoothCtlManager::AgentProcess {
public:
    AgentProcess() = default;
    ~AgentProcess() { stop(); }

    bool start(const bool autoAccept, const int lifetimeSeconds) {
        if (pid_ > 0) return true;
        int inputPipe[2]{-1, -1};
        if (pipe2(inputPipe, O_CLOEXEC) != 0) return false;
        pid_ = fork();
        if (pid_ < 0) {
            close(inputPipe[0]); close(inputPipe[1]);
            return false;
        }
        if (pid_ == 0) {
            static_cast<void>(setpgid(0, 0));
            static_cast<void>(dup2(inputPipe[0], STDIN_FILENO));
            const int nullFd = open("/dev/null", O_WRONLY);
            if (nullFd >= 0) {
                static_cast<void>(dup2(nullFd, STDOUT_FILENO));
                static_cast<void>(dup2(nullFd, STDERR_FILENO));
                close(nullFd);
            }
            close(inputPipe[0]); close(inputPipe[1]);
            const char* capability = autoAccept ? "NoInputNoOutput" : "DisplayYesNo";
            const std::string lifetime = std::to_string(std::max(lifetimeSeconds, 1));
            execlp("timeout", "timeout", "--signal=TERM", "--kill-after=1s", lifetime.c_str(),
                   "bluetoothctl", "--agent", capability, nullptr);
            _exit(127);
        }
        static_cast<void>(setpgid(pid_, pid_));
        close(inputPipe[0]);
        inputFd_ = inputPipe[1];
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
        int status = 0;
        if (waitpid(pid_, &status, WNOHANG) == pid_) {
            close(inputFd_); inputFd_ = -1; pid_ = -1;
            return false;
        }
        return true;
    }

    void stop() {
        if (pid_ <= 0) return;
        if (inputFd_ >= 0) {
            close(inputFd_);
            inputFd_ = -1;
        }
        static_cast<void>(kill(-pid_, SIGTERM));
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds{100};
        int status = 0;
        while (std::chrono::steady_clock::now() < deadline) {
            if (waitpid(pid_, &status, WNOHANG) == pid_) {
                pid_ = -1;
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds{5});
        }
        static_cast<void>(kill(-pid_, SIGKILL));
        while (waitpid(pid_, nullptr, 0) < 0 && errno == EINTR) {
        }
        pid_ = -1;
    }

private:
    pid_t pid_{-1};
    int inputFd_{-1};
};

struct BluetoothCtlManager::MediaWaitContext {
    explicit MediaWaitContext(const std::chrono::steady_clock::time_point now)
        : lastStatusLog(now) {}

    std::optional<BluetoothMediaState> lastKnownState;
    std::chrono::steady_clock::time_point lastStatusLog;
};

BluetoothCtlManager::BluetoothCtlManager(BluetoothConfig config,
                                         std::shared_ptr<IProcessRunner> processRunner,
                                         Logger* logger)
    : config_(std::move(config)), processRunner_(std::move(processRunner)), logger_(logger) {}

BluetoothCtlManager::~BluetoothCtlManager() = default;

ProcessResult BluetoothCtlManager::execute(const std::vector<std::string>& command,
                                           const std::chrono::milliseconds timeout,
                                           const std::chrono::milliseconds processAllowance) {
    auto result = processRunner_->run("bluetoothctl", command, timeout + processAllowance);
    if (commandFailed(result)) {
        if (result.timedOut) lastError_ = "bluetoothctl timed out";
        else if (!result.standardError.empty()) lastError_ = trim(result.standardError);
        else if (!result.standardOutput.empty()) lastError_ = trim(result.standardOutput);
        else lastError_ = "bluetoothctl failed with exit code " + std::to_string(result.exitCode);
    } else {
        lastError_.clear();
    }
    return result;
}

Result BluetoothCtlManager::executeAction(const std::vector<std::string>& command,
                                          const std::string_view action,
                                          const std::chrono::seconds timeout) {
    const auto result = execute(command, timeout);
    return commandFailed(result) ? Result::error(lastError_)
                                 : Result::ok(std::string{action} + " completed");
}

BluetoothStatus BluetoothCtlManager::status() {
    const auto result = execute(
        {"show"}, std::chrono::milliseconds{90}, std::chrono::milliseconds{10});
    BluetoothStatus status;
    status.serviceAvailable = result.exitCode != 127 && !result.timedOut;
    if (commandFailed(result)) {
        status.error = lastError_;
        return status;
    }
    status = parseControllerStatus(result.standardOutput);
    return status;
}

Result BluetoothCtlManager::setPower(const bool enabled) {
    return executeAction({"power", enabled ? "on" : "off"}, "Bluetooth power");
}

Result BluetoothCtlManager::startScan() { return executeAction({"scan", "on"}, "Scan start"); }
Result BluetoothCtlManager::stopScan() { return executeAction({"scan", "off"}, "Scan stop"); }

std::vector<BluetoothDevice> BluetoothCtlManager::listDevices(const std::optional<std::string> filter) {
    std::vector<std::string> command{"devices"};
    if (filter.has_value()) command.push_back(*filter);
    const auto result = execute(command);
    if (commandFailed(result)) return {};
    auto resultDevices = parseDeviceList(result.standardOutput);
    for (auto& device : resultDevices) {
        const auto info = execute({"info", device.mac});
        if (!commandFailed(info)) device = parseDeviceInfo(info.standardOutput, std::move(device));
    }
    return resultDevices;
}

std::vector<BluetoothDevice> BluetoothCtlManager::devices() { return listDevices(std::nullopt); }

std::vector<BluetoothDevice> BluetoothCtlManager::pairedDevices() {
    return listDevices("Paired");
}

std::vector<BluetoothDevice> BluetoothCtlManager::trustedDevices() {
    return listDevices("Trusted");
}

std::vector<BluetoothDevice> BluetoothCtlManager::connectedDevices() {
    return listDevices("Connected");
}

Result BluetoothCtlManager::pair(const std::string_view mac) {
    if (!isValidMac(mac)) return Result::error("Invalid Bluetooth MAC address");
    std::vector<std::string> args{"--agent", config_.autoAcceptPairing ? "NoInputNoOutput" : "DisplayYesNo",
                                  "--timeout", "30", "pair", std::string{mac}};
    auto result = processRunner_->run("bluetoothctl", args, std::chrono::seconds{31});
    if (commandFailed(result)) {
        lastError_ = result.timedOut ? "Bluetooth pairing timed out" :
                     trim(!result.standardError.empty() ? result.standardError : result.standardOutput);
        return Result::error(lastError_);
    }
    lastError_.clear();
    return Result::ok("Pairing completed");
}

Result BluetoothCtlManager::trust(const std::string_view mac, const bool trusted) {
    if (!isValidMac(mac)) return Result::error("Invalid Bluetooth MAC address");
    return executeAction({trusted ? "trust" : "untrust", std::string{mac}},
                         trusted ? "Trust" : "Untrust");
}

Result BluetoothCtlManager::connect(const std::string_view mac) {
    if (!isValidMac(mac)) return Result::error("Invalid Bluetooth MAC address");
    return executeAction({"connect", std::string{mac}}, "Connect", std::chrono::seconds{15});
}

Result BluetoothCtlManager::disconnect(const std::string_view mac) {
    if (!isValidMac(mac)) return Result::error("Invalid Bluetooth MAC address");
    return executeAction({"disconnect", std::string{mac}}, "Disconnect");
}

Result BluetoothCtlManager::remove(const std::string_view mac) {
    if (!isValidMac(mac)) return Result::error("Invalid Bluetooth MAC address");
    return executeAction({"remove", std::string{mac}}, "Remove");
}

Result BluetoothCtlManager::setPairingMode(const bool enabled) {
    if (!enabled) {
        const auto discoverable = executeAction({"discoverable", "off"}, "Discoverable off");
        const auto pairable = executeAction({"pairable", "off"}, "Pairable off");
        agent_.reset();
        return !discoverable.success ? discoverable : pairable;
    }
    if (const auto alias = executeAction({"system-alias", config_.deviceName}, "Set alias"); !alias.success) return alias;
    if (const auto timeout = executeAction({"discoverable-timeout", std::to_string(config_.discoverableTimeoutSeconds)}, "Set timeout"); !timeout.success) return timeout;
    if (const auto pairable = executeAction({"pairable", "on"}, "Pairable on"); !pairable.success) return pairable;
    if (const auto discoverable = executeAction({"discoverable", "on"}, "Discoverable on"); !discoverable.success) return discoverable;
    agent_ = std::make_unique<AgentProcess>();
    if (!agent_->start(config_.autoAcceptPairing, config_.discoverableTimeoutSeconds)) {
        agent_.reset();
        return Result::error("Cannot start Bluetooth pairing agent");
    }
    return Result::ok("Pairing mode enabled");
}

Result BluetoothCtlManager::autoConnect() {
    const auto startedAt = std::chrono::steady_clock::now();
    const auto deadline = startedAt +
                          std::chrono::seconds{config_.autoConnectTimeoutSeconds};
    const auto listTimeout = std::min(
        remainingUntil(deadline), std::chrono::milliseconds{1000});
    if (listTimeout <= std::chrono::milliseconds::zero()) {
        return Result::error("Bluetooth auto-connect timeout before trusted device lookup");
    }
    const auto list = execute(
        {"devices", "Trusted"}, listTimeout, std::chrono::milliseconds::zero());
    if (commandFailed(list)) return Result::error(lastError_);
    const auto candidates = parseDeviceList(list.standardOutput);
    if (candidates.empty()) return Result::error("No trusted Bluetooth device");

    constexpr std::size_t maximumAttempts = 3;
    const auto attempts = std::min(candidates.size(), maximumAttempts);
    std::string lastCandidateMac;
    std::optional<BluetoothMediaState> lastKnownState;
    for (std::size_t index = 0; index < attempts; ++index) {
        const auto& candidate = candidates[index];
        lastCandidateMac = candidate.mac;
        lastKnownState.reset();
        log(LogLevel::info, "Bluetooth auto-connect attempt for " + candidate.mac);
        const auto remaining = remainingUntil(deadline);
        if (remaining <= std::chrono::milliseconds::zero()) {
            break;
        }

        const auto remainingCandidates = attempts - index;
        const auto candidateBudget = remaining /
            static_cast<std::chrono::milliseconds::rep>(remainingCandidates);
        const auto candidateDeadline = index + 1 == attempts
            ? deadline
            : std::chrono::steady_clock::now() + candidateBudget;
        const auto processTimeout = std::min(
            remainingUntil(candidateDeadline),
            std::chrono::duration_cast<std::chrono::milliseconds>(maximumConnectTimeout));
        if (processTimeout <= std::chrono::milliseconds::zero()) continue;
        const auto process = execute({"connect", candidate.mac}, processTimeout,
                                     std::chrono::milliseconds::zero());
        const bool initialInProgress = isConnectionInProgress(process);
        if (commandFailed(process) && !initialInProgress) {
            log(LogLevel::warning,
                "Bluetooth auto-connect failed for " + candidate.mac + ": " + lastError_);
            continue;
        }

        if (initialInProgress) {
            log(LogLevel::info,
                "Bluetooth connection still in progress: " + candidate.mac);
        } else {
            log(LogLevel::info,
                "Bluetooth base connection command succeeded: " + candidate.mac);
        }

        MediaWaitContext waitContext{std::chrono::steady_clock::now()};
        log(LogLevel::info, "Bluetooth waiting for media profiles: " + candidate.mac);
        const auto firstWaitDeadline = std::min(
            candidateDeadline, std::chrono::steady_clock::now() + initialMediaWait);
        if (const auto ready = waitForMediaProfiles(
                candidate.mac, firstWaitDeadline, deadline, startedAt, waitContext)) {
            lastError_.clear();
            logMediaReady(candidate.mac, *ready, startedAt);
            return Result::ok("A2DP ready for " +
                              (candidate.name.empty() ? candidate.mac : candidate.name));
        }
        lastKnownState = waitContext.lastKnownState;

        if (std::chrono::steady_clock::now() >= candidateDeadline) {
            lastError_ = "A2DP media profile did not become ready for " + candidate.mac + ": " +
                         formatMediaState(lastKnownState.value_or(BluetoothMediaState{}));
            continue;
        }
        log(LogLevel::info, "Bluetooth initial media wait expired: " + candidate.mac);

        bool permanentFailure = false;
        while (std::chrono::steady_clock::now() < candidateDeadline) {
            const auto retryRemaining = remainingUntil(candidateDeadline);
            if (retryRemaining < minimumConnectBudget) {
                if (const auto ready = waitForMediaProfiles(
                        candidate.mac, candidateDeadline, deadline, startedAt, waitContext)) {
                    lastError_.clear();
                    logMediaReady(candidate.mac, *ready, startedAt);
                    return Result::ok("A2DP ready for " +
                                      (candidate.name.empty() ? candidate.mac : candidate.name));
                }
                break;
            }

            log(LogLevel::info,
                "Bluetooth retrying media-profile connection: " + candidate.mac);
            const auto retryTimeout = std::min(
                retryRemaining,
                std::chrono::duration_cast<std::chrono::milliseconds>(maximumConnectTimeout));
            const auto retry = execute({"connect", candidate.mac}, retryTimeout,
                                       std::chrono::milliseconds::zero());
            if (commandFailed(retry)) {
                if (isConnectionInProgress(retry)) {
                    log(LogLevel::info,
                        "Bluetooth media-profile connection still in progress: " +
                            candidate.mac);
                    log(LogLevel::debug,
                        "Bluetooth waiting after InProgress: " + candidate.mac +
                            ", remaining=" + std::to_string(remainingUntil(deadline).count()) +
                            "ms");
                    const auto cooldownDeadline = std::min(
                        candidateDeadline,
                        std::chrono::steady_clock::now() + reconnectCooldown);
                    if (const auto ready = waitForMediaProfiles(
                            candidate.mac, cooldownDeadline, deadline, startedAt, waitContext)) {
                        lastError_.clear();
                        logMediaReady(candidate.mac, *ready, startedAt);
                        return Result::ok("A2DP ready for " +
                                          (candidate.name.empty() ? candidate.mac
                                                                  : candidate.name));
                    }
                    lastKnownState = waitContext.lastKnownState;
                    continue;
                }
                log(LogLevel::warning,
                    "Bluetooth media-profile retry failed for " + candidate.mac + ": " +
                        lastError_);
                permanentFailure = true;
                break;
            }

            log(LogLevel::info,
                "Bluetooth media-profile connection command succeeded: " + candidate.mac);
            const auto successfulRetryDeadline = std::min(
                candidateDeadline, std::chrono::steady_clock::now() + initialMediaWait);
            if (const auto ready = waitForMediaProfiles(
                    candidate.mac, successfulRetryDeadline, deadline, startedAt, waitContext)) {
                lastError_.clear();
                logMediaReady(candidate.mac, *ready, startedAt);
                return Result::ok("A2DP ready for " +
                                  (candidate.name.empty() ? candidate.mac : candidate.name));
            }
            lastKnownState = waitContext.lastKnownState;
        }
        if (!permanentFailure) {
            lastError_ = "A2DP media profile did not become ready for " + candidate.mac + ": " +
                         formatMediaState(lastKnownState.value_or(BluetoothMediaState{}));
        }
    }

    if (std::chrono::steady_clock::now() >= deadline) {
        lastError_ = "Bluetooth auto-connect timeout for " + lastCandidateMac + ": " +
                     formatMediaState(lastKnownState.value_or(BluetoothMediaState{}));
        log(LogLevel::warning, lastError_);
    }
    return Result::error("No trusted Bluetooth device could be connected: " + lastError_);
}

std::optional<BluetoothMediaState> BluetoothCtlManager::probeMediaState(
    const std::string_view mac, const std::chrono::steady_clock::time_point deadline) {
    const auto remaining = remainingUntil(deadline);
    if (remaining <= std::chrono::milliseconds::zero()) return std::nullopt;
    const auto configuredTimeout =
        std::chrono::milliseconds{config_.mediaDbusTimeoutMilliseconds};
    const auto timeout = std::min(remaining, configuredTimeout);
    const auto result = processRunner_->run(
        "busctl",
        {"--system", "--json=short", "call", "org.bluez", "/",
         "org.freedesktop.DBus.ObjectManager", "GetManagedObjects"},
        timeout);
    if (result.exitCode != 0 || result.timedOut) {
        return std::nullopt;
    }
    return BluezDbusMediaController::parseMediaState(result.standardOutput, mac);
}

std::optional<BluetoothMediaState> BluetoothCtlManager::waitForMediaProfiles(
    const std::string_view mac, const std::chrono::steady_clock::time_point windowDeadline,
    const std::chrono::steady_clock::time_point overallDeadline,
    const std::chrono::steady_clock::time_point startedAt, MediaWaitContext& context) {
    while (std::chrono::steady_clock::now() < windowDeadline) {
        if (const auto state = probeMediaState(mac, windowDeadline)) {
            if (!context.lastKnownState.has_value() || *context.lastKnownState != *state) {
                log(LogLevel::debug,
                    "Bluetooth media state for " + std::string{mac} + ": " +
                        formatMediaState(*state));
                context.lastKnownState = *state;
            }
            if (state->ready()) return state;
        }

        const auto now = std::chrono::steady_clock::now();
        if (now >= windowDeadline) break;
        if (now - context.lastStatusLog >= mediaStatusLogInterval) {
            log(LogLevel::info,
                "Bluetooth waiting for media profiles: " + std::string{mac} +
                    ", elapsed=" +
                    std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                                       now - startedAt).count()) +
                    "ms, remaining=" + std::to_string(remainingUntil(overallDeadline).count()) +
                    "ms");
            context.lastStatusLog = now;
        }
        std::this_thread::sleep_until(std::min(windowDeadline, now + mediaProbeInterval));
    }
    return std::nullopt;
}

std::string BluetoothCtlManager::formatMediaState(const BluetoothMediaState& state) {
    const auto boolean = [](const bool value) { return value ? "true" : "false"; };
    return "deviceConnected=" + std::string{boolean(state.deviceConnected)} +
           ", mediaControlPresent=" + std::string{boolean(state.mediaControlPresent)} +
           ", mediaControlConnected=" + std::string{boolean(state.mediaControlConnected)} +
           ", mediaTransportPresent=" + std::string{boolean(state.mediaTransportPresent)};
}

void BluetoothCtlManager::logMediaReady(
    const std::string_view mac, const BluetoothMediaState& state,
    const std::chrono::steady_clock::time_point startedAt) const {
    std::string reason;
    if (state.mediaTransportPresent && state.mediaControlConnected) {
        reason = "Bluetooth A2DP ready";
    } else if (state.mediaTransportPresent) {
        reason = "Bluetooth media profile ready via MediaTransport1";
    } else {
        reason = "Bluetooth media profile ready via MediaControl1.Connected";
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startedAt);
    log(LogLevel::info,
        reason + ": " + std::string{mac} + ", " + formatMediaState(state) +
            ", elapsed=" + std::to_string(elapsed.count()) + "ms");
}

void BluetoothCtlManager::log(const LogLevel level, const std::string_view message) const {
    if (logger_ != nullptr) logger_->log(level, message);
}

std::string BluetoothCtlManager::lastError() const { return lastError_; }

bool BluetoothCtlManager::isConnectionInProgress(const ProcessResult& result) {
    const auto output = lowerCase(result.standardOutput + '\n' + result.standardError);
    return output.find("org.bluez.error.inprogress") != std::string::npos ||
           output.find("br-connection-busy") != std::string::npos;
}

bool BluetoothCtlManager::isValidMac(const std::string_view mac) {
    if (mac.size() != 17) return false;
    for (std::size_t index = 0; index < mac.size(); ++index) {
        if ((index + 1) % 3 == 0) {
            if (mac[index] != ':') return false;
        } else if (std::isxdigit(static_cast<unsigned char>(mac[index])) == 0) return false;
    }
    return true;
}

std::vector<BluetoothDevice> BluetoothCtlManager::parseDeviceList(const std::string_view output) {
    std::vector<BluetoothDevice> devices;
    std::istringstream lines{std::string{output}};
    std::string prefix;
    std::string mac;
    std::string name;
    while (lines >> prefix >> mac) {
        std::getline(lines, name);
        if (prefix == "Device" && isValidMac(mac)) {
            devices.push_back({trim(name), mac, false, false, false, false});
        }
    }
    return devices;
}

BluetoothDevice BluetoothCtlManager::parseDeviceInfo(const std::string_view output,
                                                       BluetoothDevice device) {
    const std::string mac = propertyValue(output, "Device");
    if (device.mac.empty() && isValidMac(mac)) device.mac = mac;
    const auto name = propertyValue(output, "Name");
    if (!name.empty()) device.name = name;
    device.paired = property(output, "Paired");
    device.trusted = property(output, "Trusted");
    device.connected = property(output, "Connected");
    device.available = device.connected || !propertyValue(output, "RSSI").empty();
    return device;
}

BluetoothStatus BluetoothCtlManager::parseControllerStatus(const std::string_view output) {
    BluetoothStatus status;
    status.serviceAvailable = true;
    status.adapterAvailable = output.find("Controller ") != std::string_view::npos;
    status.powered = property(output, "Powered");
    status.discovering = property(output, "Discovering");
    status.pairable = property(output, "Pairable");
    status.discoverable = property(output, "Discoverable");
    return status;
}

std::optional<BluetoothDevice> BluetoothCtlManager::firstAvailableTrusted(
    const std::vector<BluetoothDevice>& devices) {
    const auto match = std::find_if(devices.begin(), devices.end(), [](const BluetoothDevice& device) {
        return device.available && device.trusted;
    });
    return match == devices.end() ? std::nullopt : std::optional<BluetoothDevice>{*match};
}

}  // namespace x308
