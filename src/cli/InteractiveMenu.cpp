#include "x308/InteractiveMenu.hpp"

#include "x308/Interfaces.hpp"
#include "x308/SourceManager.hpp"

#include <istream>
#include <ostream>
#include <string>

namespace x308 {

namespace {

std::string_view playbackName(const PlaybackState state) {
    switch (state) {
        case PlaybackState::playing: return "воспроизведение";
        case PlaybackState::paused: return "пауза";
        case PlaybackState::stopped: return "остановлено";
        case PlaybackState::unknown: return "неизвестно";
    }
    return "неизвестно";
}

void showResult(std::ostream& output, const Result& result) {
    output << (result.success ? "Готово" : "Ошибка") << ": " << result.message << '\n';
}

}  // namespace

InteractiveMenu::InteractiveMenu(IMediaPlayer* mediaPlayer, IBluetoothManager* bluetooth,
                                 SourceManager* sourceManager)
    : mediaPlayer_(mediaPlayer), bluetooth_(bluetooth), sourceManager_(sourceManager) {}

int InteractiveMenu::run(std::istream& input, std::ostream& output) const {
    std::string selection;
    while (true) {
        output << "\n=== Jaguar X308 Head Unit ===\n\n"
               << "Активный источник: "
               << (sourceManager_ == nullptr ? "MPD" : SourceManager::name(sourceManager_->activeSource()))
               << "\n\n"
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
        if (selection == "1" && bluetooth_ != nullptr) {
            static_cast<void>(runBluetoothMenu(input, output));
        } else if (selection == "2" && mediaPlayer_ != nullptr) {
            static_cast<void>(runMpdMenu(input, output));
        } else if (selection == "3" && sourceManager_ != nullptr) {
            static_cast<void>(runSourceMenu(input, output));
        } else if (selection == "1" || selection == "2" || selection == "3" || selection == "4") {
            output << "Этот модуль будет доступен на следующем этапе.\n";
        } else {
            output << "Неверный пункт меню. Попробуйте снова.\n";
        }
    }
}

int InteractiveMenu::runBluetoothMenu(std::istream& input, std::ostream& output) const {
    std::string selection;
    const auto promptMac = [&]() -> std::optional<std::string> {
        output << "MAC-адрес: " << std::flush;
        std::string mac;
        if (!std::getline(input, mac)) return std::nullopt;
        return mac;
    };
    while (true) {
        output << "\n--- Bluetooth ---\n"
               << "1. Состояние\n2. Включить адаптер\n3. Выключить адаптер\n"
               << "4. Начать сканирование\n5. Остановить сканирование\n6. Устройства\n"
               << "7. Сопрячь\n8. Доверять\n9. Не доверять\n10. Подключить\n"
               << "11. Отключить\n12. Удалить\n13. Режим сопряжения: вкл\n"
               << "14. Режим сопряжения: выкл\n15. Автоподключение\n0. Назад\n\n> " << std::flush;
        if (!std::getline(input, selection) || selection == "0") return 0;
        if (selection == "1") {
            const auto status = bluetooth_->status();
            output << "Служба: " << (status.serviceAvailable ? "доступна" : "недоступна")
                   << "\nАдаптер: " << (status.adapterAvailable ? "найден" : "не найден")
                   << "\nПитание: " << (status.powered ? "включено" : "выключено")
                   << "\nВидимость: " << (status.discoverable ? "да" : "нет") << '\n';
            if (!status.error.empty()) output << "Ошибка: " << status.error << '\n';
        } else if (selection == "2") showResult(output, bluetooth_->setPower(true));
        else if (selection == "3") showResult(output, bluetooth_->setPower(false));
        else if (selection == "4") showResult(output, bluetooth_->startScan());
        else if (selection == "5") showResult(output, bluetooth_->stopScan());
        else if (selection == "6") {
            const auto devices = bluetooth_->devices();
            if (devices.empty()) output << (bluetooth_->lastError().empty() ? "Устройства не найдены.\n" : "Ошибка: " + bluetooth_->lastError() + "\n");
            for (const auto& device : devices) output << device.mac << "  " << device.name
                << " [paired=" << (device.paired ? "yes" : "no") << ", trusted="
                << (device.trusted ? "yes" : "no") << ", connected="
                << (device.connected ? "yes" : "no") << "]\n";
        } else if (selection == "13") showResult(output, bluetooth_->setPairingMode(true));
        else if (selection == "14") showResult(output, bluetooth_->setPairingMode(false));
        else if (selection == "15") showResult(output, bluetooth_->autoConnect());
        else if (selection == "7" || selection == "8" || selection == "9" ||
                 selection == "10" || selection == "11" || selection == "12") {
            const auto mac = promptMac();
            if (!mac.has_value()) return 0;
            if (selection == "7") showResult(output, bluetooth_->pair(*mac));
            else if (selection == "8") showResult(output, bluetooth_->trust(*mac, true));
            else if (selection == "9") showResult(output, bluetooth_->trust(*mac, false));
            else if (selection == "10") showResult(output, bluetooth_->connect(*mac));
            else if (selection == "11") showResult(output, bluetooth_->disconnect(*mac));
            else showResult(output, bluetooth_->remove(*mac));
        } else output << "Неверный пункт меню.\n";
    }
}

int InteractiveMenu::runSourceMenu(std::istream& input, std::ostream& output) const {
    output << "\nАктивный источник: " << SourceManager::name(sourceManager_->activeSource())
           << "\n1. MPD\n2. Bluetooth\n0. Назад\n\n> " << std::flush;
    std::string selection;
    if (!std::getline(input, selection) || selection == "0") return 0;
    if (selection == "1") showResult(output, sourceManager_->setSource(AudioSource::mpd));
    else if (selection == "2") showResult(output, sourceManager_->setSource(AudioSource::bluetooth));
    else output << "Неверный пункт меню.\n";
    return 0;
}

int InteractiveMenu::runMpdMenu(std::istream& input, std::ostream& output) const {
    std::string selection;
    while (true) {
        output << "\n--- MPD ---\n"
               << "1. Состояние и текущий трек\n2. Воспроизвести\n3. Пауза\n"
               << "4. Стоп\n5. Следующий трек\n6. Предыдущий трек\n"
               << "7. Очередь\n8. Очистить очередь\n9. Библиотека\n"
               << "10. Добавить путь\n11. Обновить базу\n0. Назад\n\n> " << std::flush;
        if (!std::getline(input, selection) || selection == "0") return 0;
        if (selection == "1") {
            const auto status = mediaPlayer_->status();
            if (!status.available) {
                output << "MPD недоступен: " << status.error << '\n';
            } else {
                output << "Состояние: " << playbackName(status.state) << '\n';
                if (status.currentTrack.has_value()) {
                    const auto& track = *status.currentTrack;
                    output << "Трек: " << (track.title.empty() ? track.uri : track.title) << '\n'
                           << "Исполнитель: " << (track.artist.empty() ? "—" : track.artist) << '\n'
                           << "Альбом: " << (track.album.empty() ? "—" : track.album) << '\n';
                } else {
                    output << "Текущий трек отсутствует.\n";
                }
            }
        } else if (selection == "2") showResult(output, mediaPlayer_->play());
        else if (selection == "3") showResult(output, mediaPlayer_->pause());
        else if (selection == "4") showResult(output, mediaPlayer_->stop());
        else if (selection == "5") showResult(output, mediaPlayer_->next());
        else if (selection == "6") showResult(output, mediaPlayer_->previous());
        else if (selection == "7") {
            const auto queue = mediaPlayer_->queue();
            if (queue.empty()) output << (mediaPlayer_->lastError().empty() ? "Очередь пуста.\n" : "Ошибка: " + mediaPlayer_->lastError() + "\n");
            for (const auto& track : queue) output << "- " << (track.title.empty() ? track.uri : track.title) << '\n';
        } else if (selection == "8") showResult(output, mediaPlayer_->clearQueue());
        else if (selection == "9") {
            output << "Путь (пусто — корень): " << std::flush;
            std::string path;
            if (!std::getline(input, path)) return 0;
            const auto entries = mediaPlayer_->library(path);
            if (entries.empty()) output << (mediaPlayer_->lastError().empty() ? "Папка пуста.\n" : "Ошибка: " + mediaPlayer_->lastError() + "\n");
            for (const auto& entry : entries) output << (entry.directory ? "[DIR] " : "      ") << entry.path << '\n';
        } else if (selection == "10") {
            output << "Путь в библиотеке MPD: " << std::flush;
            std::string path;
            if (!std::getline(input, path)) return 0;
            showResult(output, mediaPlayer_->add(path));
        } else if (selection == "11") showResult(output, mediaPlayer_->update());
        else output << "Неверный пункт меню.\n";
    }
}

}  // namespace x308
