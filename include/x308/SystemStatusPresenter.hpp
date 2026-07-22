#pragma once

#include "x308/SystemStatusReport.hpp"

#include <iosfwd>

namespace x308 {

class SystemStatusPresenter {
public:
    static void print(const SystemStatusReport& report, std::ostream& output);
};

}  // namespace x308
