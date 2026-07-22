#pragma once

#include "x308/Configuration.hpp"
#include "x308/Interfaces.hpp"
#include "x308/ProcessRunner.hpp"

#include <chrono>
#include <memory>
#include <string>

namespace x308 {

class LinuxAudioOutputController final : public IAudioOutput {
public:
    LinuxAudioOutputController(AudioConfig config,
                               std::shared_ptr<IProcessRunner> processRunner);

    [[nodiscard]] Result selectSource(AudioSource source) override;
    [[nodiscard]] std::string currentDevice() const override;

private:
    [[nodiscard]] Result setBluealsaAplayActive(bool active);

    AudioConfig config_;
    std::shared_ptr<IProcessRunner> processRunner_;
    std::chrono::milliseconds timeout_;
};

}  // namespace x308
