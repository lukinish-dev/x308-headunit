#pragma once

#include "x308/Interfaces.hpp"
#include "x308/ProcessRunner.hpp"

#include <chrono>
#include <memory>
#include <string_view>

namespace x308 {

class BluezDbusMediaController final : public IBluetoothMediaController {
public:
    explicit BluezDbusMediaController(
        std::shared_ptr<IProcessRunner> processRunner,
        std::chrono::milliseconds timeout = std::chrono::milliseconds{800});

    [[nodiscard]] BluetoothMediaStatus status() override;
    [[nodiscard]] Result play() override;
    [[nodiscard]] Result pause() override;
    [[nodiscard]] Result togglePause() override;
    [[nodiscard]] Result next() override;
    [[nodiscard]] Result previous() override;

    [[nodiscard]] static BluetoothMediaStatus parseManagedObjects(std::string_view json);

private:
    [[nodiscard]] Result invoke(std::string_view method);
    [[nodiscard]] Result invokeOnPlayer(std::string_view playerPath, std::string_view method);

    std::shared_ptr<IProcessRunner> processRunner_;
    std::chrono::milliseconds timeout_;
};

}  // namespace x308
