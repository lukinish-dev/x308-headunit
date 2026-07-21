#include "x308/App.hpp"

#include <iostream>
#include <string_view>

namespace x308 {

int App::run(const int argc, const char* const* argv) const {
    if (argc > 1 && std::string_view{argv[1]} == "--version") {
        std::cout << "x308-headunit 0.1.0\n";
        return 0;
    }

    std::cout << "=== Jaguar X308 Head Unit ===\n";
    std::cout << "Консольное приложение готово к настройке.\n";
    return 0;
}

}  // namespace x308

