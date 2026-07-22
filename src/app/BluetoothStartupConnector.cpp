#include "x308/BluetoothStartupConnector.hpp"

namespace x308 {

Result BluetoothStartupConnector::run(const bool enabled, IBluetoothManager& bluetooth) {
    if (!enabled) return Result::ok("Bluetooth startup auto-connect is disabled");
    return bluetooth.autoConnect();
}

}  // namespace x308
