#pragma once

#include <iosfwd>

namespace x308 {
class IMediaPlayer;
class IBluetoothManager;
class SourceManager;
class SystemStatusService;
}

namespace x308 {

class InteractiveMenu {
public:
    InteractiveMenu(IMediaPlayer* mediaPlayer = nullptr, IBluetoothManager* bluetooth = nullptr,
                    SourceManager* sourceManager = nullptr,
                    SystemStatusService* systemStatus = nullptr);
    [[nodiscard]] int run(std::istream& input, std::ostream& output) const;

private:
    [[nodiscard]] int runMpdMenu(std::istream& input, std::ostream& output) const;
    [[nodiscard]] int runBluetoothMenu(std::istream& input, std::ostream& output) const;
    [[nodiscard]] int runSourceMenu(std::istream& input, std::ostream& output) const;
    IMediaPlayer* mediaPlayer_;
    IBluetoothManager* bluetooth_;
    SourceManager* sourceManager_;
    SystemStatusService* systemStatus_;
};

}  // namespace x308
