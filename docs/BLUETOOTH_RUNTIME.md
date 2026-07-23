# Bluetooth audio runtime на Rock Pi 4B Plus

Дата аудита: 2026-07-22. Все команды аудита были read-only и выполнялись с
внешним timeout не более пяти секунд. Сервисы, конфигурация BlueZ/ALSA и
сопряжённые устройства во время аудита не изменялись.

## Фактический стек

| Компонент | Версия/состояние | Назначение |
|---|---|---|
| BlueZ | 5.66, `bluetooth.service` active | adapter, pairing, device connection, media objects |
| BlueALSA | 4.0.0, `bluealsa.service` active | A2DP Source и A2DP Sink endpoints |
| bluealsa-aplay | 4.0.0, service active | A2DP Sink stream → локальный ALSA PCM |
| MPD | service active | локальное воспроизведение через тот же ALSA PCM |
| PipeWire | 0.3.65, user service active | desktop audio graph/default ALSA plugin |
| WirePlumber | 0.4.13, user service active | PipeWire session manager и BlueZ monitor |

Фактическая команда receiver:

```text
/usr/bin/bluealsa-aplay --pcm=plughw:CARD=rockchipes8316,DEV=0
```

Она задана drop-in файлом
`/etc/systemd/system/bluealsa-aplay.service.d/override.conf`. MPD output
`ES8316` также использует:

```text
plughw:CARD=rockchipes8316,DEV=0
```

Таким образом, MPD и Bluetooth receiver конкурируют за один hardware PCM и
должны переключаться последовательно.

## Владелец A2DP

Наблюдаемый production receiver — BlueALSA:

- процесс `bluealsa` запущен с `-p a2dp-source -p a2dp-sink`;
- system D-Bus name `org.bluealsa` принадлежит процессу BlueALSA;
- его D-Bus tree содержит зарегистрированные A2DP SBC sink/source endpoints;
- `bluealsa-aplay` является процессом, который открывает ES8316 PCM.

PipeWire и WirePlumber одновременно активны. `wpctl status` во время аудита
показывал только встроенные ALSA devices и не показывал Bluetooth device/node.
Это означает, что фактический отключённый receiver path настроен через
BlueALSA, однако наличие WirePlumber BlueZ monitor создаёт потенциальную
конкуренцию при следующем подключении телефона. Подтвердить отсутствие
конфликта можно только при живом A2DP transport: должен появиться один ожидаемый
BlueALSA PCM, звук должен идти через ES8316, а в PipeWire не должен появиться
конкурирующий Bluetooth sink/source, перехватывающий профиль.

Приложение не отключает PipeWire/WirePlumber и не меняет их конфигурацию.
Отключение BlueZ monitor WirePlumber является системным изменением: риск —
потеря desktop Bluetooth audio; откат — удалить локальный WirePlumber override
и перезапустить user services. Такое изменение в этом этапе не выполнялось.

## Телефон и D-Bus

Во время аудита BlueZ знал устройство:

```text
B0:8C:75:9F:CA:EE  iPhone (Алексей)
Paired: yes
Bonded: yes
Trusted: yes
Connected: no
```

В ObjectManager были обнаружены:

- `org.bluez.Device1`;
- `org.bluez.MediaControl1` с `Connected=false`;
- `org.freedesktop.DBus.Properties`;
- `org.freedesktop.DBus.ObjectManager` на `/`.

`org.bluez.MediaPlayer1` отсутствовал, что ожидаемо для отключённого телефона.
Поэтому в этой сессии нельзя честно подтвердить A2DP connection, слышимый звук,
AVRCP команды и живые metadata. Эти проверки остаются ручными и описаны в
[MANUAL_TESTS.md](MANUAL_TESTS.md).

## Runtime приложения

`BluetoothCtlManager` по-прежнему отвечает только за adapter/device операции
через `bluetoothctl`: power/status, scan, pairing, trust, connect/disconnect,
списки устройств и bounded auto-connect. Он не управляет media player и ALSA.

`BluezDbusMediaController` является отдельным adapter. Через `busctl` и system
D-Bus он выполняет ObjectManager discovery, читает свойства `MediaControl1` и
`MediaPlayer1` и вызывает `Play`, `Pause`, `Next`, `Previous`. Executable и argv
передаются раздельно через `IProcessRunner`; каждый вызов имеет hard timeout.
Отсутствие player возвращается как обычный unavailable status.

`LinuxAudioOutputController` переключает receiver service:

- Bluetooth: `systemctl start bluealsa-aplay.service`, затем bounded
  `is-active` verification;
- MPD: AVRCP Pause, затем `systemctl stop bluealsa-aplay.service` и bounded
  verification освобождения PCM.

`MpdClient` отдельно включает/отключает настроенный MPD output `ES8316` через
libmpdclient. `SourceManager` сохраняет единоличное владение логическим active
source и меняет его только после полного успеха.

## Риски и откат переключения

Управление system service выполняется через `sudo -n systemctl` и требует
ограниченного sudoers-правила для пользователя приложения. Инструкция установки
находится в `docs/BLUEALSA_APLAY_AUTHORIZATION.md`. При отказе в доступе ошибка
возвращается вызывающему коду, а active source не меняется.

Риск остановки `bluealsa-aplay`: текущий A2DP stream перестанет звучать. Риск
запуска: если MPD output не освобождён или PipeWire занял PCM, service может
остаться active без готового звука. Безопасный ручной откат к исходному
состоянию Rock Pi:

```bash
sudo systemctl start bluealsa-aplay.service
mpc enable 1
```

Перед применением используйте `mpc outputs`, потому что numeric output id может
измениться. Для возврата PCM MPD вручную остановите `bluealsa-aplay`, включите
MPD output `ES8316` и проверьте `mpc outputs`.

## Результат аудита

- backend: BlueALSA + `bluealsa-aplay`;
- ALSA PCM: `plughw:CARD=rockchipes8316,DEV=0`;
- MPD output: `ES8316`, тот же PCM;
- PipeWire: активен параллельно, наблюдаемого Bluetooth node нет, потенциальный
  конфликт при connection требует ручной проверки;
- iPhone: paired/trusted, во время аудита disconnected;
- живые A2DP audio и AVRCP metadata: не проверены без участия телефона.
