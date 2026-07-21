#include "x308/InteractiveMenu.hpp"

#include <istream>
#include <ostream>
#include <string>

namespace x308 {

int InteractiveMenu::run(std::istream& input, std::ostream& output) const {
    std::string selection;
    while (true) {
        output << "\n=== Jaguar X308 Head Unit ===\n\n"
               << "Активный источник: MPD\n\n"
               << "1. Bluetooth\n"
               << "2. MPD\n"
               << "3. Источник воспроизведения\n"
               << "4. Состояние системы\n"
               << "0. Выход\n\n"
               << "> " << std::flush;
        if (!std::getline(input, selection)) {
            output << "\nЗавершение работы.\n";
            return 0;
        }
        if (selection == "0") {
            output << "До свидания.\n";
            return 0;
        }
        if (selection == "1" || selection == "2" || selection == "3" || selection == "4") {
            output << "Этот модуль будет доступен на следующем этапе.\n";
        } else {
            output << "Неверный пункт меню. Попробуйте снова.\n";
        }
    }
}

}  // namespace x308

