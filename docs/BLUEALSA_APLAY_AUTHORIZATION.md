# Неинтерактивное управление bluealsa-aplay

`bluealsa-aplay` и MPD используют один эксклюзивный ALSA PCM. Приложение
запускает или останавливает только `bluealsa-aplay.service` через
`sudo -n /usr/bin/systemctl`; интерактивный пароль не запрашивается.

Минимальное правило sudoers для пользователя приложения:

```text
<USER> ALL=(root) NOPASSWD: /usr/bin/systemctl start bluealsa-aplay.service, /usr/bin/systemctl stop bluealsa-aplay.service, /usr/bin/systemctl restart bluealsa-aplay.service
```

Установите правило для пользователя, под которым запускается приложение:

```bash
sudo ./scripts/install-system-permissions.sh --user <USER>
./scripts/check-system-setup.sh
```

Шаблон находится в `deploy/sudoers/x308-audio`. Скрипт рендерит имя
пользователя, проверяет `visudo` до установки и атомарно создаёт
`/etc/sudoers.d/x308-audio`.

Удаление правила:

```bash
sudo rm /etc/sudoers.d/x308-audio
```

Правило разрешает только перечисленные действия именно для
`bluealsa-aplay.service` и не даёт shell/root-доступ. Если правило отсутствует,
приложение возвращает `SYSTEM_SETUP_REQUIRED` и не меняет active source.
