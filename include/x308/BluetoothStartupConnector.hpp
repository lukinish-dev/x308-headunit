#pragma once

#include "x308/Interfaces.hpp"

namespace x308 {

class BluetoothStartupConnector {
public:
    [[nodiscard]] static Result run(bool enabled, IBluetoothManager& bluetooth);
};

}  // namespace x308
