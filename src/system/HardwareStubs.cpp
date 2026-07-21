#include "x308/HardwareStubs.hpp"

namespace x308 {

Result NullAudioOutput::selectSource(const AudioSource source) {
    if (source == AudioSource::carPlay) {
        return Result::error("CarPlay audio is unavailable");
    }
    return Result::ok();
}

std::string NullAudioOutput::currentDevice() const {
    return "not detected";
}

Result NullDspController::initialize() {
    return Result::ok("DSP hardware is not installed");
}

std::optional<std::string> NullInputController::poll(const std::chrono::milliseconds timeout) {
    static_cast<void>(timeout);
    return std::nullopt;
}

}  // namespace x308

