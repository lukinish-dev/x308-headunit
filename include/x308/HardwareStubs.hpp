#pragma once

#include "x308/Interfaces.hpp"

namespace x308 {

class NullAudioOutput final : public IAudioOutput {
public:
    [[nodiscard]] Result selectSource(AudioSource source) override;
    [[nodiscard]] std::string currentDevice() const override;
};

class NullDspController final : public IDspController {
public:
    [[nodiscard]] Result initialize() override;
};

class NullInputController final : public IInputController {
public:
    [[nodiscard]] std::optional<std::string> poll(std::chrono::milliseconds timeout) override;
};

}  // namespace x308

