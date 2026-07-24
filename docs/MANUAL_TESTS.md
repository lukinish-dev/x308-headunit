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
5. Проверьте `mpc outputs`: output `ES8316` должен существовать, а
   `audio.alsa_pcm` должен совпадать с PCM в `bluealsa-aplay.service`.
6. Установите право запуска/остановки `bluealsa-aplay.service` командой
   `sudo ./scripts/install-system-permissions.sh --user <USER>` и проверьте его
   через `./scripts/check-system-setup.sh`. Приложение использует только
   неинтерактивный `sudo -n`.

## Полный обязательный сценарий iPhone

Выполните весь сценарий в указанном порядке и запишите результат каждого шага:

1. Сопрягите iPhone с `Jaguar XJR` через интерактивный pairing mode.
2. Выполните `bluetooth trust <MAC>` и убедитесь, что устройство trusted.
3. Убедитесь, что `bluetooth.auto_connect = true`, закройте и снова запустите
   приложение. Startup не должен зависнуть; iPhone должен подключиться
   автоматически либо должна появиться bounded warning при недоступном телефоне.
4. Запустите музыку на iPhone и выберите `source set bluetooth` в одном
   долгоживущем интерактивном процессе.
5. Убедитесь на слух, что A2DP audio идёт через ES8316, и выполните
   `bluetooth current`: title/artist/album/status должны соответствовать iPhone.
6. По очереди проверьте `bluetooth pause`, `play`, `next`, `previous` и
   `toggle`; после каждого шага сравните состояние и трек на iPhone.
7. В том же процессе выберите MPD. Убедитесь, что `bluealsa-aplay.service`
   освобождён, MPD output `ES8316` включён и MPD звучит.
8. Проверьте MPD `play`, `pause`, `next`, `previous`.
9. Переключитесь обратно на Bluetooth. Убедитесь, что MPD поставлен на паузу,
   его output отключён, `bluealsa-aplay.service` active, звук iPhone и metadata
   восстановились.
10. Если любой шаг неуспешен, зафиксируйте сообщение partial failure,
    `source status`, `mpc outputs`, `systemctl status bluealsa-aplay` и
    `wpctl status`. Не считайте сценарий пройденным при конкуренции двух backend.

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
с жёстким timeout до пяти секунд на попытку, общим startup deadline и подключает
первое доступное. Команда не повторяется бесконечно. При
`bluetooth.auto_connect = true` тот же bounded сценарий выполняется на старте;
его ошибка не должна препятствовать открытию CLI/меню.

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

Проверьте, появился ли `org.bluez.MediaPlayer1`, затем выполните
`bluetooth current`, `play`, `pause`, `toggle`, `next`, `previous`. Ожидается:
приложение показывает `Status`, `Track`, duration/position при наличии и iPhone
реагирует на каждый метод. При отсутствии player приложение должно вернуть
понятное сообщение, а не зависнуть.

## 7. Переключение MPD → Bluetooth

1. Выполняйте проверку в одном интерактивном процессе приложения.
2. Запустите MPD track и зафиксируйте `mpc status`.
3. Подключите телефон и выберите источник Bluetooth.
4. Проверьте, что MPD перешёл в pause/stop и телефон слышен через ES8316.

Ожидается: MPD поставлен на паузу, output `ES8316` отключён, затем
`bluealsa-aplay.service` запущен и active source изменяется только после
успешной readiness-проверки. При ошибке MPD output восстанавливается либо
возвращается явный partial failure.

## 8. Переключение Bluetooth → MPD

1. При активном A2DP stream выберите MPD в том же интерактивном процессе.
2. Проверьте `mpc status`, `bluealsa-aplay --list-pcms` и ALSA/journal errors.

Ожидается: iPhone поставлен на паузу через AVRCP, `bluealsa-aplay.service`
остановлен и освободил PCM, затем MPD output `ES8316` включён до команды Play.
При ошибке возвращается явный partial failure. Не считайте тест пройденным,
если MPD не открыл ES8316 или оба backend конкурируют за устройство.

## 9. Проверка после reboot

1. Перезагрузите Rock Pi вручную.
2. Проверьте services `mpd`, `bluetooth`, `bluealsa`, `bluealsa-aplay`.
3. Проверьте paired/trusted list.
4. Запустите приложение и проверьте автоматический bounded connect и A2DP test.

Ожидается: pairing/trust сохраняются, приложение автоматически подключает
trusted iPhone, а отсутствующий телефон не препятствует запуску.

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
