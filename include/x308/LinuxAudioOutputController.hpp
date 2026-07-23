#pragma once

#include "x308/Configuration.hpp"
#include "x308/Interfaces.hpp"
#include "x308/ProcessRunner.hpp"

#include <chrono>
#include <memory>
#include <string>

namespace x308 {

class Logger;

class LinuxAudioOutputController final : public IAudioOutput {
public:
    LinuxAudioOutputController(AudioConfig config,
                               std::shared_ptr<IProcessRunner> processRunner,
                               Logger* logger = nullptr);

    [[nodiscard]] Result selectSource(AudioSource source) override;
    [[nodiscard]] std::string currentDevice() const override;

private:
    [[nodiscard]] ProcessResult serviceState() const;
    [[nodiscard]] Result waitForServiceState(bool active) const;
    [[nodiscard]] Result setBluealsaAplayActive(bool active);

    AudioConfig config_;
    std::shared_ptr<IProcessRunner> processRunner_;
    Logger* logger_;
};

}  // namespace x308
