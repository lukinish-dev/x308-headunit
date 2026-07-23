# Неинтерактивное управление bluealsa-aplay

`bluealsa-aplay` и MPD используют один эксклюзивный ALSA PCM. Приложение
запускает или останавливает только `bluealsa-aplay.service` через
`systemctl --no-ask-password`; других privileged действий оно не выполняет.

Для пользователя `radxa` установите подготовленное ограниченное polkit-правило:

```bash
sudo install -D -m 0644 config/polkit-1/rules.d/49-x308-bluealsa-aplay.rules /etc/polkit-1/rules.d/49-x308-bluealsa-aplay.rules
```

Правило разрешает только `start` и `stop` именно для
`bluealsa-aplay.service`. Оно не разрешает управление другими systemd unit и
не даёт shell/root-доступ. Если приложение запускается от другого пользователя,
замените `radxa` в исходном файле правила до его установки.
