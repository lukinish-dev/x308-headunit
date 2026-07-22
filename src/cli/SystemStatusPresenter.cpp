#include "x308/SystemStatusPresenter.hpp"

#include "x308/SourceManager.hpp"

#include <iomanip>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>

namespace x308 {
namespace {

std::string_view yesNo(const bool value) { return value ? "да" : "нет"; }

std::string_view playbackStateName(const PlaybackState state) {
    switch (state) {
        case PlaybackState::playing: return "воспроизведение";
        case PlaybackState::paused: return "пауза";
        case PlaybackState::stopped: return "остановлено";
        case PlaybackState::unknown: return "неизвестно";
    }
    return "неизвестно";
}

std::string formatDuration(const std::chrono::seconds value) {
    const auto total = value.count();
    const auto days = total / 86400;
    const auto hours = (total % 86400) / 3600;
    const auto minutes = (total % 3600) / 60;
    const auto seconds = total % 60;
    std::ostringstream output;
    if (days > 0) output << days << "д ";
    output << std::setfill('0') << std::setw(2) << hours << ':'
           << std::setw(2) << minutes << ':' << std::setw(2) << seconds;
    return output.str();
}

std::string formatBytes(const std::uintmax_t bytes) {
    constexpr double gibibyte = 1024.0 * 1024.0 * 1024.0;
    constexpr double mebibyte = 1024.0 * 1024.0;
    std::ostringstream output;
    output << std::fixed << std::setprecision(1);
    if (static_cast<double>(bytes) >= gibibyte) output << static_cast<double>(bytes) / gibibyte << " GiB";
    else output << static_cast<double>(bytes) / mebibyte << " MiB";
    return output.str();
}

std::string optionalValue(const std::optional<std::string>& value) {
    return value.has_value() ? *value : "—";
}

}  // namespace

void SystemStatusPresenter::print(const SystemStatusReport& report, std::ostream& output) {
    output << "=== Состояние системы ===\n\n"
           << "Application\n"
           << "  Версия: " << report.application.version << '\n'
           << "  Тип сборки: " << report.application.buildType << '\n'
           << "  Uptime приложения: "
           << formatDuration(std::chrono::duration_cast<std::chrono::seconds>(report.application.uptime)) << "\n\n"
           << "System\n"
           << "  Hostname: " << (report.system.hostname.empty() ? "—" : report.system.hostname) << '\n'
           << "  Kernel: " << (report.system.kernel.empty() ? "—" : report.system.kernel) << '\n'
           << "  Uptime системы: " << formatDuration(report.system.uptime) << "\n\n"
           << "Storage\n"
           << "  Каталог: " << report.storage.path.string() << '\n'
           << "  Присутствует: " << yesNo(report.storage.present) << '\n'
           << "  Доступен: " << yesNo(report.storage.accessible) << '\n'
           << "  Свободно: "
           << (report.storage.freeBytes.has_value() ? formatBytes(*report.storage.freeBytes) : "—") << "\n\n"
           << "MPD\n"
           << "  Доступен: " << yesNo(report.mpd.available) << '\n'
           << "  Состояние: " << playbackStateName(report.mpd.state) << '\n'
           << "  Текущий источник: " << SourceManager::name(report.mpd.currentSource) << '\n'
           << "  Трек: " << optionalValue(report.mpd.currentTrack) << '\n'
           << "  Исполнитель: " << optionalValue(report.mpd.artist) << "\n\n"
           << "Bluetooth\n"
           << "  Служба: " << (report.bluetooth.serviceAvailable ? "доступна" : "недоступна") << '\n'
           << "  Адаптер: " << (report.bluetooth.adapterPresent ? "найден" : "не найден") << '\n'
           << "  Питание: " << (report.bluetooth.adapterPowered ? "включено" : "выключено") << '\n'
           << "  Discoverable: " << yesNo(report.bluetooth.discoverable) << '\n'
           << "  Подключённое устройство: " << optionalValue(report.bluetooth.connectedDevice) << '\n'
           << "  Имя устройства: " << optionalValue(report.bluetooth.connectedDeviceName) << "\n\n"
           << "SourceManager\n"
           << "  Активный источник: " << SourceManager::name(report.sourceManager.activeSource) << "\n\n"
           << "Получено за: " << std::fixed << std::setprecision(1)
           << static_cast<double>(report.collectionDuration.count()) / 1000.0 << " мс\n";

    if (!report.mpd.error.empty()) output << "Предупреждение MPD: " << report.mpd.error << '\n';
    if (!report.bluetooth.error.empty()) output << "Предупреждение Bluetooth: " << report.bluetooth.error << '\n';
    for (const auto& warning : report.warnings) output << "Предупреждение: " << warning << '\n';
}

}  // namespace x308
