# x308-headunit Roadmap

> Последнее обновление: 2026-07-22. Обновляется при начале или завершении milestone.

Архитектура проекта описана в [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md), а
история выполненных изменений — в [CHANGELOG.md](CHANGELOG.md).

---

## Статус проекта

Текущий этап:

✅ **Runtime Stage — MPD and Bluetooth Integration**

Общее состояние:

```text
█████████░░░░░░░░░ 45%
```

---

## Completed

### Milestone 0 — Project Foundation ✅

- Git repository
- CMake
- C++20
- Build system
- Unit test framework
- Configuration loader
- Logging
- Interactive console
- CLI
- Project architecture
- Documentation

### Milestone 1 — MPD Core ✅

- libmpdclient
- Playback control
- Queue
- Library
- Random
- Repeat
- Database update
- Integration tests

### Milestone 2 — Bluetooth Core ✅

- ProcessRunner
- BluetoothCtlManager
- Device discovery
- Pairing
- Trust
- Connect and disconnect
- Auto-connect
- Timeouts
- Safe integration tests

### Milestone 5 — System Status ✅

- SystemStatusService и SystemStatusReport
- Application и system uptime
- Hostname и kernel
- MPD и Bluetooth status
- Storage status
- CLI и интерактивное меню
- Бюджет менее 200 мс

### Architecture Stage — Core Application ✅

- AppContext с явным владением
- Единый composition root и shutdown
- Инжектируемые CLI и InteractiveMenu
- Линейный application lifecycle

---

## Latest completed stage

### Runtime Stage — MPD and Bluetooth Integration ✅

**Цель:** проверить консольные MPD/Bluetooth сценарии на Rock Pi и подготовить
hardware-changing операции к контролируемому ручному тесту.

Состав:

- Реальная read-only проверка MPD, Bluetooth и storage
- Полные MPD/Bluetooth CLI и интерактивные menus
- Bounded ordered Bluetooth auto-connect
- Определён BlueALSA A2DP Sink backend и ALSA output
- Исследованы доступные AVRCP D-Bus interfaces
- Safe integration tests и manual hardware test plan

Результат: **проверяемая консольная версия без автоматических destructive tests**.

---

## Planned milestones

### Milestone 3 — Bluetooth Audio

- BlueALSA A2DP Sink runtime control
- BlueZ D-Bus AVRCP и metadata
- Release audio device
- Source switching
- Metadata

### Milestone 4 — Source Manager

Расширение текущего базового SourceManager:

- Full source arbitration
- Rollback
- Transaction model
- Audio ownership

### Milestone 6 — DSP

- Helix DSP
- Digital potentiometers
- Presets
- Volume
- Subwoofer

### Milestone 7 — CarPlay

- Backend
- Source integration
- Audio routing

### Milestone 8 — GUI Backend

- Application services
- Event model
- View models
- API

### Milestone 9 — User Interface

- Main screen
- MPD
- Bluetooth
- CarPlay
- Settings

### Milestone 10 — Hardware

- Steering wheel buttons
- Rotary encoder
- GPIO
- Display
- Boot splash

### Milestone 11 — Production

- systemd
- Packaging
- Installation
- Performance tuning
- Boot optimization

---

## Future ideas

После выпуска первой стабильной версии:

- D-Bus Bluetooth backend
- OTA updates
- Wi-Fi management
- Remote diagnostics
- CAN integration
- GPS
- OBD-II

## Правила обновления Roadmap

Roadmap изменяется только при завершении текущего или начале нового milestone.
Обычные коммиты, исправления ошибок и рефакторинг Roadmap не изменяют.
