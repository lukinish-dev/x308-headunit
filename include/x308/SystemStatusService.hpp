#pragma once

#include "x308/Interfaces.hpp"
#include "x308/SourceManager.hpp"
#include "x308/SystemStatusReport.hpp"

#include <chrono>
#include <filesystem>
#include <string>

namespace x308 {

class SystemStatusService {
public:
    static constexpr auto collectionBudget = std::chrono::milliseconds{200};

    SystemStatusService(
        IMediaPlayer& mediaPlayer, IBluetoothManager& bluetooth,
        const SourceManager& sourceManager, std::filesystem::path musicDirectory,
        std::string applicationVersion, std::string buildType,
        std::chrono::steady_clock::time_point applicationStartedAt =
            std::chrono::steady_clock::now());

    [[nodiscard]] SystemStatusReport collect();

private:
    IMediaPlayer& mediaPlayer_;
    IBluetoothManager& bluetooth_;
    const SourceManager& sourceManager_;
    std::filesystem::path musicDirectory_;
    std::string applicationVersion_;
    std::string buildType_;
    std::chrono::steady_clock::time_point applicationStartedAt_;
};

}  // namespace x308
