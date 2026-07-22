#pragma once

#include "x308/Configuration.hpp"
#include "x308/Interfaces.hpp"
#include "x308/ProcessRunner.hpp"

#include <memory>
#include <optional>

namespace x308 {

class Logger;

class BluetoothCtlManager final : public IBluetoothManager {
public:
    BluetoothCtlManager(BluetoothConfig config, std::shared_ptr<IProcessRunner> processRunner,
                        Logger* logger = nullptr);
    ~BluetoothCtlManager() override;

    [[nodiscard]] BluetoothStatus status() override;
    [[nodiscard]] Result setPower(bool enabled) override;
    [[nodiscard]] Result startScan() override;
    [[nodiscard]] Result stopScan() override;
    [[nodiscard]] std::vector<BluetoothDevice> devices() override;
    [[nodiscard]] std::vector<BluetoothDevice> pairedDevices() override;
    [[nodiscard]] std::vector<BluetoothDevice> trustedDevices() override;
    [[nodiscard]] std::vector<BluetoothDevice> connectedDevices() override;
    [[nodiscard]] Result pair(std::string_view mac) override;
    [[nodiscard]] Result trust(std::string_view mac, bool trusted) override;
    [[nodiscard]] Result connect(std::string_view mac) override;
    [[nodiscard]] Result disconnect(std::string_view mac) override;
    [[nodiscard]] Result remove(std::string_view mac) override;
    [[nodiscard]] Result setPairingMode(bool enabled) override;
    [[nodiscard]] Result autoConnect() override;
    [[nodiscard]] Result activateAudio() override;
    [[nodiscard]] Result releaseAudio() override;
    [[nodiscard]] std::string lastError() const override;

    [[nodiscard]] static bool isValidMac(std::string_view mac);
    [[nodiscard]] static std::vector<BluetoothDevice> parseDeviceList(std::string_view output);
    [[nodiscard]] static BluetoothDevice parseDeviceInfo(std::string_view output,
                                                         BluetoothDevice device = {});
    [[nodiscard]] static BluetoothStatus parseControllerStatus(std::string_view output);
    [[nodiscard]] static std::optional<BluetoothDevice> firstAvailableTrusted(
        const std::vector<BluetoothDevice>& devices);

private:
    class AgentProcess;

    [[nodiscard]] ProcessResult execute(const std::vector<std::string>& command,
                                        std::chrono::milliseconds timeout = std::chrono::seconds{4},
                                        std::chrono::milliseconds processAllowance =
                                            std::chrono::seconds{1});
    [[nodiscard]] Result executeAction(const std::vector<std::string>& command,
                                       std::string_view action,
                                       std::chrono::seconds timeout = std::chrono::seconds{4});
    [[nodiscard]] std::vector<BluetoothDevice> listDevices(std::optional<std::string> filter);

    BluetoothConfig config_;
    std::shared_ptr<IProcessRunner> processRunner_;
    Logger* logger_;
    std::unique_ptr<AgentProcess> agent_;
    std::string lastError_;
};

}  // namespace x308
