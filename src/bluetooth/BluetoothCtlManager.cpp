#include "x308/BluetoothCtlManager.hpp"

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

BluetoothCtlManager::BluetoothCtlManager(BluetoothConfig config,
                                         std::shared_ptr<IProcessRunner> processRunner)
    : config_(std::move(config)), processRunner_(std::move(processRunner)) {}

BluetoothCtlManager::~BluetoothCtlManager() = default;

ProcessResult BluetoothCtlManager::execute(const std::vector<std::string>& command,
                                           const std::chrono::seconds timeout) {
    auto result = processRunner_->run("bluetoothctl", command, timeout + std::chrono::seconds{1});
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
    const auto result = execute({"show"});
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
    const auto candidate = firstAvailableTrusted(devices());
    if (!candidate.has_value()) return Result::error("No available trusted Bluetooth device");
    return connect(candidate->mac);
}

Result BluetoothCtlManager::activateAudio() {
    const auto current = status();
    if (current.activeAudioDevice.has_value()) return Result::ok("Bluetooth audio device is connected");
    return config_.autoConnect ? autoConnect() : Result::error("No Bluetooth audio device connected");
}

Result BluetoothCtlManager::releaseAudio() {
    const auto current = status();
    if (!current.activeAudioDevice.has_value()) return Result::ok("No Bluetooth stream is active");
    return disconnect(current.activeAudioDevice->mac);
}

std::string BluetoothCtlManager::lastError() const { return lastError_; }

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
        if (prefix == "Device" && isValidMac(mac)) devices.push_back({trim(name), mac, false, false, false, true});
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
