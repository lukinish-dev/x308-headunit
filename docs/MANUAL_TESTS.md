# Ручные проверки MPD и Bluetooth

Эти проверки изменяют состояние MPD, Bluetooth или аудиовыхода и поэтому не
запускаются автоматически. Выполняйте их на Rock Pi с доступом к телефону и
аналоговому выходу 3,5 мм.

## Перед началом

1. Соберите приложение и убедитесь, что unit/integration tests проходят.
2. Сохраните вывод `mpc status`, `mpc playlist -f '%file%'` и
   `bluetoothctl show` для сравнения и возможного восстановления.
3. Не очищайте очередь и не удаляйте Bluetooth-устройство, если это отдельно не
   является целью проверки.
4. Все диагностические команды ограничивайте, например:
   `timeout --signal=TERM --kill-after=1s 5s <команда>`.

## 1. Pairing телефона

1. Запустите `./build/x308-headunit` и откройте Bluetooth menu.
2. Выберите «Режим сопряжения: вкл» и не закрывайте приложение: pairing agent
   принадлежит текущему процессу.
3. На телефоне выберите устройство `Jaguar XJR`.
4. При необходимости в меню выберите «Сопрячь» и введите MAC телефона.
5. После проверки выключите pairing mode.

Ожидается: телефон показывает успешное сопряжение; `bluetooth paired` выводит
его MAC и имя; discoverable/pairable выключаются после завершения сценария.

## 2. Trust телефона

```bash
./build/x308-headunit bluetooth trust AA:BB:CC:DD:EE:FF
./build/x308-headunit bluetooth trusted
```

Ожидается: устройство присутствует в trusted list с `Доверено: да`. Для отката:
`bluetooth untrust <MAC>`.

## 3. Connect телефона

```bash
./build/x308-headunit bluetooth connect AA:BB:CC:DD:EE:FF
timeout --signal=TERM --kill-after=1s 5s bluetoothctl info AA:BB:CC:DD:EE:FF
```

Ожидается: `Connected: yes`. Для отката выполните
`./build/x308-headunit bluetooth disconnect <MAC>`.

## 4. Auto-connect

1. Отключите телефон, не удаляя pairing/trust.
2. Держите Bluetooth телефона включённым и доступным.
3. Выполните `./build/x308-headunit bluetooth auto-connect`.

Ожидается: приложение пробует trusted devices в порядке списка, не более трёх,
с жёстким timeout пять секунд на попытку и подключает первое доступное. Команда
не повторяется бесконечно. Автоматический вызов при старте приложения сейчас не
включён.

## 5. A2DP audio

1. Подключите телефон и запустите на нём музыку.
2. Проверьте `bluealsa-aplay --list-devices` и `bluealsa-aplay --list-pcms`.
3. Проверьте `systemctl status bluealsa bluealsa-aplay`.
4. Убедитесь на слух, что звук идёт через аналоговый ES8316 output.

Ожидается: BlueALSA показывает A2DP PCM телефона, а
`bluealsa-aplay --pcm=plughw:CARD=rockchipes8316,DEV=0` выводит звук. Если звука
нет, зафиксируйте journal `bluealsa`, `bluealsa-aplay`, `bluetooth` и вывод
`wpctl status`; не отключайте PipeWire и не меняйте units без отдельного плана.

## 6. AVRCP

При подключённом телефоне найдите player object:

```bash
busctl tree org.bluez
busctl introspect org.bluez /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF
```

Проверьте, появился ли `org.bluez.MediaPlayer1`, его `Status` и `Track`, а также
реакцию телефона на Play/Pause/Next/Previous доступным системным D-Bus client.
Текущее приложение эти методы не вызывает: полноценная проверка управления и
metadata является следующим BlueZ D-Bus этапом, а не возможностью bluetoothctl
backend.

## 7. Переключение MPD → Bluetooth

1. Выполняйте проверку в одном интерактивном процессе приложения.
2. Запустите MPD track и зафиксируйте `mpc status`.
3. Подключите телефон и выберите источник Bluetooth.
4. Проверьте, что MPD перешёл в pause/stop и телефон слышен через ES8316.

Ожидается: active source изменяется только после успешных шагов. Если Bluetooth
не подключается, active source остаётся MPD.

## 8. Переключение Bluetooth → MPD

1. При активном A2DP stream выберите MPD в том же интерактивном процессе.
2. Проверьте `mpc status`, `bluealsa-aplay --list-pcms` и ALSA/journal errors.

Текущее ожидаемое ограничение: bluetoothctl status не определяет активный A2DP
stream, поэтому приложение пока не может гарантировать освобождение BlueALSA
PCM. Не считайте тест пройденным, если MPD не открыл ES8316 или оба backend
конкурируют за устройство.

## 9. Проверка после reboot

1. Перезагрузите Rock Pi вручную.
2. Проверьте services `mpd`, `bluetooth`, `bluealsa`, `bluealsa-aplay`.
3. Проверьте paired/trusted list.
4. Выполните ручной `bluetooth auto-connect` и A2DP test.

Ожидается: pairing/trust сохраняются. Само приложение пока не вызывает
auto-connect при старте; возможное системное автоподключение BlueZ фиксируйте
отдельно.

## 10. Итог и диагностика

Для каждого шага запишите: команду, exit code, фактический status, слышимый
результат и время выполнения. Полезные read-only команды:

```bash
./build/x308-headunit status
./build/x308-headunit mpd status
./build/x308-headunit bluetooth status
./build/x308-headunit bluetooth paired
./build/x308-headunit bluetooth trusted
mpc status
bluealsa-aplay --list-pcms
wpctl status
journalctl -u mpd -u bluetooth -u bluealsa -u bluealsa-aplay --since today
```

Системные изменения, установка пакетов, отключение PipeWire или изменение
systemd units не входят в эти проверки и требуют отдельного согласования с
описанием риска и отката.
