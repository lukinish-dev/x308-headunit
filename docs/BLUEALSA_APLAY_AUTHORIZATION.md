# Неинтерактивное управление bluealsa-aplay

`bluealsa-aplay` и MPD используют один эксклюзивный ALSA PCM. Приложение
запускает или останавливает только `bluealsa-aplay.service` через
`sudo -n systemctl`; интерактивный пароль не запрашивается.

Минимальное правило sudoers для пользователя приложения:

```text
<USER> ALL=(root) NOPASSWD: /usr/bin/systemctl start bluealsa-aplay.service, /usr/bin/systemctl stop bluealsa-aplay.service, /usr/bin/systemctl restart bluealsa-aplay.service
```

Замените `<USER>` на фактического пользователя и установите правило:

```bash
sudo install -D -m 0440 config/sudoers.d/x308-bluealsa-aplay /etc/sudoers.d/x308-bluealsa-aplay
sudo visudo -cf /etc/sudoers.d/x308-bluealsa-aplay
```

Правило разрешает только перечисленные действия именно для
`bluealsa-aplay.service` и не даёт shell/root-доступ.
