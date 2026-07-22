# x308-headunit

Консольное приложение головного устройства Jaguar X308 для Rock Pi 4B Plus.
Текущая версия управляет MPD, устройствами BlueZ через `bluetoothctl`, выбором
источника и read-only системным статусом.

## Системные зависимости

Сборка рассчитана на Debian 12 ARM64 и требует:

- CMake 3.25+, Ninja и компилятор с C++20;
- `libmpdclient-dev`;
- MPD и доступный каталог `/mnt/music`;
- BlueZ (`bluetoothd`, `bluetoothctl`);
- ALSA utilities для диагностики;
- BlueALSA/`bluealsa-aplay` для Bluetooth A2DP Sink.

На проверенной Rock Pi используется BlueALSA 4.0.0 с профилями
`a2dp-source`/`a2dp-sink`. `bluealsa-aplay` выводит звук в
`plughw:CARD=rockchipes8316,DEV=0`. В пользовательской сессии также запущены
PipeWire, PipeWire Pulse и WirePlumber; приложение не меняет и не отключает их.

## Сборка

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Безопасные host integration tests включаются отдельно:

```bash
cmake -S . -B build-integration -G Ninja \
  -DX308_ENABLE_INTEGRATION_TESTS=ON
cmake --build build-integration
ctest --test-dir build-integration --output-on-failure -L integration
```

Integration tests только читают MPD/Bluetooth status, queue/library и trusted
devices. Они не запускают scan/pairing, не подключают и не удаляют устройства,
не очищают очередь и не обновляют базу MPD.

## Запуск и статус

Без аргументов открывается интерактивное меню:

```bash
./build/x308-headunit
./build/x308-headunit status
./build/x308-headunit source status
```

Конфигурация по умолчанию находится в `config/config.toml`. Другой файл можно
передать через `--config <путь>`.

## Проверка MPD

Read-only команды:

```bash
./build/x308-headunit mpd status
./build/x308-headunit mpd current
./build/x308-headunit mpd queue
./build/x308-headunit mpd library
./build/x308-headunit mpd library "путь/в/библиотеке"
```

Управляющие команды `play`, `pause`, `toggle`, `stop`, `next`, `previous`,
`queue clear`, `add`, `add-folder`, `random`, `repeat` и `update` изменяют
состояние MPD. Перед ручной проверкой сохраните состояние пользовательской
очереди и следуйте [docs/MANUAL_TESTS.md](docs/MANUAL_TESTS.md).

## Проверка Bluetooth

Безопасные команды:

```bash
./build/x308-headunit bluetooth status
./build/x308-headunit bluetooth devices
./build/x308-headunit bluetooth paired
./build/x308-headunit bluetooth trusted
```

Режим сопряжения и ручное автоподключение:

```bash
./build/x308-headunit bluetooth pairing-mode on
./build/x308-headunit bluetooth pair AA:BB:CC:DD:EE:FF
./build/x308-headunit bluetooth trust AA:BB:CC:DD:EE:FF
./build/x308-headunit bluetooth auto-connect
./build/x308-headunit bluetooth pairing-mode off
```

Для входящего pairing рекомендуется интерактивное меню: оно сохраняет владельца
pairing agent до отключения режима или выхода из приложения. Все одноразовые
вызовы `bluetoothctl` имеют жёсткий timeout и не используют shell-команды.

## Известные ограничения

- Реальные pairing/connect/A2DP/AVRCP и переключение источников требуют телефона
  и выполняются только по ручному сценарию.
- Текущий `bluetoothctl` backend не умеет надёжно определить активный A2DP stream.
  Поэтому Bluetooth → MPD пока не гарантирует освобождение ALSA output.
- AVRCP Target объявлен BlueZ, а `MediaControl1` доступен через D-Bus, но
  bluetoothctl backend приложения не предоставляет AVRCP-команды или metadata.
  Для этого нужен отдельный ограниченный BlueZ D-Bus этап.
- Автоподключение не запускается автоматически при старте приложения. Команда
  `bluetooth auto-connect` делает не более трёх пятисекундных попыток в порядке
  списка trusted devices.
- PipeWire/WirePlumber и BlueALSA работают одновременно. Выбор одного системного
  Bluetooth audio backend требует отдельного согласованного изменения системы.
- Состояние `SourceManager` хранится в процессе; для последовательной проверки
  переключений используйте интерактивное меню.
