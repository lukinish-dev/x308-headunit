# Changelog

Все заметные изменения проекта документируются в этом файле.

Формат основан на [Keep a Changelog](https://keepachangelog.com/ru/1.1.0/),
а версии проекта должны следовать [Semantic Versioning](https://semver.org/lang/ru/).
Git-теги и опубликованные версии пока не создавались, поэтому текущая история
находится в разделе `Unreleased`.

## [Unreleased]

### Added

- Создан C++20/CMake-каркас единого консольного приложения `x308-headunit` со
  строгими предупреждениями, Ninja-сборкой, CTest и compile commands.
- Добавлены типизированная конфигурация, заданный порядок поиска TOML-файла,
  значения по умолчанию и пример конфигурации.
- Добавлены терминальное логирование, разбор CLI и интерактивное русскоязычное
  меню с безопасной обработкой неверного ввода и EOF.
- Введены `IMediaPlayer`, `IBluetoothManager`, `IAudioOutput`, `IDspController`,
  `IInputController`, собственные модели и минимальные hardware-заглушки.
- Добавлен базовый `SourceManager` для переключения между MPD и Bluetooth с
  зарезервированным источником CarPlay.
- Реализован `MpdClient` через libmpdclient: status/current, playback control,
  queue, library, add, random, repeat и явно запускаемый database update.
- Добавлены MPD-команды CLI, MPD-подменю, unit-тесты преобразования моделей и
  безопасная opt-in integration-проверка реального MPD и `/mnt/music`.
- Реализован `BluetoothCtlManager`: status, power, scan, devices, pair,
  trust/untrust, connect/disconnect, remove, pairing mode и выбор первого
  доступного trusted device для auto-connect.
- Добавлены валидация MAC-адресов, парсинг сохранённого вывода `bluetoothctl`,
  Bluetooth CLI/подменю, fake-based unit-тесты и безопасная integration-проверка
  `bluetoothctl show`.
- Добавлен `PosixProcessRunner` с отдельными argv/stdout/stderr, жёстким
  timeout, process group, `SIGTERM`, grace period, `SIGKILL` и закрытием pipe.
- Добавлены unit-тесты завершения зависшего процесса вместе с потомками и
  унаследованными файловыми дескрипторами.
- Добавлены аудит платформы и архитектурная документация модульного монолита с
  границами ответственности, правилами зависимостей, тестовой стратегией и
  известными отклонениями текущего кода.
- Добавлены Roadmap и этот Changelog.
- Добавлены `SystemStatusReport` и read-only `SystemStatusService`, агрегирующие
  версию и build type, uptime приложения и системы, hostname/kernel, состояние
  `/mnt/music`, MPD, Bluetooth и активный источник.
- Добавлен общий `SystemStatusPresenter`, используемый командой
  `x308-headunit status` и пунктом «Состояние системы» интерактивного меню.
- Добавлены unit-тесты агрегации, отсутствующего storage, общего presenter и
  отсутствия изменяющих вызовов, а также безопасный integration-тест с бюджетом
  получения статуса менее 200 мс.
- Добавлены владеющий `AppContext` и простой жизненный цикл `Application`:
  Created, Initialized, Running, Stopping и Stopped с единым shutdown.
- Добавлен инжектируемый `Cli`, который содержит command dispatch и presentation,
  ранее находившиеся в composition root.
- Добавлены отдельные paired/trusted/connected queries, bounded ordered
  auto-connect с техническим логированием и unit-тесты fallback/timeout.
- Добавлены read-only integration-проверки MPD queue/library/current и trusted
  Bluetooth devices, а также руководство `docs/MANUAL_TESTS.md`.
- Добавлен README со сборкой, runtime-командами, обнаруженным BlueALSA backend и
  честными ограничениями A2DP/AVRCP/SourceManager.
- Добавлены `BluetoothMediaStatus`, `IBluetoothMediaController` и отдельный
  `BluezDbusMediaController` для bounded ObjectManager discovery, свойств
  `MediaControl1`/`MediaPlayer1`, AVRCP playback и track metadata.
- Добавлены команды `bluetooth current`, `play`, `pause`, `toggle`, `next` и
  `previous` с русскоязычным выводом и соответствующие пункты меню.
- Добавлен `LinuxAudioOutputController`, управляющий готовностью
  `bluealsa-aplay.service` и фактическим ALSA PCM без shell-команд.
- Добавлен bounded startup auto-connect trusted Bluetooth devices; его ошибка
  логируется и не блокирует запуск приложения.
- Добавлены unit-тесты D-Bus property/metadata parsing, отсутствующего player,
  AVRCP dispatch, startup auto-connect, source ordering/rollback и timeout, а
  также безопасная read-only D-Bus integration-проверка.
- Добавлен фактический аудит BlueZ/BlueALSA/PipeWire/ALSA в
  `docs/BLUETOOTH_RUNTIME.md` и полный ручной iPhone A2DP/AVRCP сценарий.

### Changed

- Обычные одноразовые вызовы `bluetoothctl` больше не используют задерживающий
  флаг `--timeout`; ограничение времени обеспечивает общий ProcessRunner.
- Bluetooth status сокращён до одного безопасного запроса `bluetoothctl show`.
- `PosixProcessRunner` поддерживает необязательный верхний предел запрошенного
  timeout и настраиваемый grace period для коротких диагностических probes.
- Независимые MPD и Bluetooth probes SystemStatus выполняются параллельно во
  владеемых потоках с обязательным join и отдельными жёсткими лимитами.
- `Application` стал единственной production-точкой создания и связывания
  Configuration, Logger, ProcessRunner, MPD, Bluetooth, SourceManager,
  SystemStatus, CLI и InteractiveMenu; дублирующие status adapters удалены.
- Logger получил явного владельца и больше не хранит глобальное изменяемое
  состояние.
- Интерактивные MPD/Bluetooth menus теперь покрывают toggle, random, repeat,
  add-folder и отдельные paired/trusted lists через существующие сервисы.
- Пользовательские CLI statuses и успешные результаты выводятся на русском.
- `MpdClient::activateAudio()` и `releaseAudio()` теперь включают и отключают
  настроенный MPD output через libmpdclient вместо stop/status-заглушек.
- `SourceManager` больше не использует device-management Bluetooth interface
  для управления аудио; production переключение координирует MPD output и
  Linux BlueALSA receiver с компенсацией доступных шагов.
- `BluetoothCtlManager` ограничивает auto-connect одновременно числом попыток,
  timeout каждой команды и общим startup deadline.

### Fixed

- Устранена возможность бесконечного ожидания внешнего процесса, который
  игнорирует `SIGTERM` или оставляет pipe открытым в дочернем процессе.
- Завершение внешнего процесса теперь распространяется на всю process group и
  не оставляет потомков после timeout.
- Cached disconnected Bluetooth devices больше не помечаются available; признак
  требует connected state или актуального RSSI.
- Неизвестная или неполная module command теперь возвращает nonzero и показывает
  CLI help.
- Read-only MPD status восстанавливается после подтверждённого немедленного
  transient timeout одной ограниченной повторной попыткой, не повторяя
  полноценный timeout недоступного сервера.
- Ошибка переключения источника больше не меняет логического владельца; rollback
  восстанавливает предыдущий output, а невозможность восстановления явно
  возвращается как partial failure.

[Unreleased]: https://github.com/lukinish-dev/x308-headunit/commits/main
