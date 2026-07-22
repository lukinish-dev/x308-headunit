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

### Changed

- Обычные одноразовые вызовы `bluetoothctl` больше не используют задерживающий
  флаг `--timeout`; ограничение времени обеспечивает общий ProcessRunner.
- Bluetooth status сокращён до одного безопасного запроса `bluetoothctl show`.
- `PosixProcessRunner` поддерживает необязательный верхний предел запрошенного
  timeout и настраиваемый grace period для коротких диагностических probes.
- Независимые MPD и Bluetooth probes SystemStatus выполняются параллельно во
  владеемых потоках с обязательным join и отдельными жёсткими лимитами.

### Fixed

- Устранена возможность бесконечного ожидания внешнего процесса, который
  игнорирует `SIGTERM` или оставляет pipe открытым в дочернем процессе.
- Завершение внешнего процесса теперь распространяется на всю process group и
  не оставляет потомков после timeout.

[Unreleased]: https://github.com/lukinish-dev/x308-headunit/commits/main
