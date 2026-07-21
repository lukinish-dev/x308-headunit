# Аудит исходной системы

Дата проверки: 2026-07-21.

- Платформа: Rock Pi 4B Plus, Debian 12 (bookworm), ARM64.
- Ядро: Linux 6.1.115-8-rk2501.
- Toolchain: GCC 12.2, CMake 3.25.1, Ninja 1.11.1.
- MPD: служба активна; библиотека `/mnt/music`; bind `localhost`; ALSA-выход
  `plughw:CARD=rockchipes8316,DEV=0` (ES8316).
- `/mnt/music`: смонтирован с `/dev/mmcblk1p1`, файловая система exFAT.
- Bluetooth: BlueZ 5.66, служба активна, адаптер включён, alias `Jaguar XJR`.
- Найдены `bluetoothctl`, `bluealsa`, `bluealsa-aplay` и ALSA PCM `bluealsa`.
- `libmpdclient2` установлен, но пакет разработчика `libmpdclient-dev` отсутствует.
- Не установлены dev-пакеты spdlog, CLI11, toml++ и Catch2.

Во время аудита конфигурация MPD, BlueZ, ALSA и systemd не изменялась.

