# x308-headunit

Консольное приложение головного устройства Jaguar X308 для Rock Pi 4B Plus.
Текущая версия управляет MPD, устройствами BlueZ через `bluetoothctl`, A2DP
receiver через BlueALSA, AVRCP через BlueZ D-Bus, выбором источника и read-only
системным статусом.

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
devices, а также ObjectManager media status. Они не запускают scan/pairing, не
подключают и не удаляют устройства, не вызывают AVRCP playback, не переключают
audio service, не очищают очередь и не обновляют базу MPD.

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

AVRCP status и управление подключённым телефоном:

```bash
./build/x308-headunit bluetooth current
./build/x308-headunit bluetooth play
./build/x308-headunit bluetooth pause
./build/x308-headunit bluetooth toggle
./build/x308-headunit bluetooth next
./build/x308-headunit bluetooth previous
```

При `bluetooth.auto_connect = true` приложение на старте выполняет bounded
ordered подключение trusted devices. Ошибка или отсутствующий телефон пишутся в
технический лог и не препятствуют запуску CLI/меню.

Реальное переключение выполняется командами `source set bluetooth` и
`source set mpd`. MPD output `ES8316` выключается/включается через libmpdclient.
При выборе MPD Bluetooth ставится на паузу через AVRCP, затем
`bluealsa-aplay.service` останавливается и освобождает ALSA PCM. Управление
service выполняется без интерактивного запроса через ограниченное sudoers-правило;
инструкция находится в [docs/BLUEALSA_APLAY_AUTHORIZATION.md](docs/BLUEALSA_APLAY_AUTHORIZATION.md).

Перед первым запуском на Rock Pi установите ограниченное право управления
сервисом и проверьте его:

```bash
sudo ./scripts/install-system-permissions.sh --user <USER>
./scripts/check-system-setup.sh
```

Разрешены только `start`, `stop` и `restart` для `bluealsa-aplay.service`.
Без этого права переключение завершается `SYSTEM_SETUP_REQUIRED`, а active
source сохраняется.

Фактический аудит и риски описаны в
[docs/BLUETOOTH_RUNTIME.md](docs/BLUETOOTH_RUNTIME.md), полный сценарий с
iPhone — в [docs/MANUAL_TESTS.md](docs/MANUAL_TESTS.md).

## Известные ограничения

- Реальные pairing/connect/A2DP/AVRCP и переключение источников требуют телефона,
  слышимого output и выполняются только по ручному сценарию.
- `MediaPlayer1` существует только при подключённом совместимом AVRCP source;
  без него `bluetooth current` корректно сообщает unavailable.
- Автоподключение делает не более трёх попыток и ограничено общим startup
  deadline; iOS может оставаться недоступной до действия пользователя.
- PipeWire/WirePlumber и BlueALSA работают одновременно. Выбор одного системного
  Bluetooth audio backend и исключение потенциальной конкуренции требуют
  отдельного согласованного изменения системы.
- Source switching требует ограниченного sudoers-права start/stop
  `bluealsa-aplay.service`. При отказе active source не меняется, но ошибка
  посередине может оставить MPD на паузе и будет явно отмечена как partial failure.
- Состояние `SourceManager` хранится в процессе; для последовательной проверки
  переключений используйте интерактивное меню.
