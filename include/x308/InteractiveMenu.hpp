#pragma once

#include <iosfwd>

namespace x308 {

class InteractiveMenu {
public:
    [[nodiscard]] int run(std::istream& input, std::ostream& output) const;
};

}  // namespace x308

