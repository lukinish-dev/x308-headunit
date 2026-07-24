#!/usr/bin/env bash
set -u

fail() {
    printf 'FAIL: %s\n\n' "$1"
    printf 'SYSTEM_SETUP_REQUIRED\n\nInstall the required Linux permissions:\n\n'
    printf 'sudo ./scripts/install-system-permissions.sh\n'
    exit 1
}

[ -x /usr/bin/systemctl ] || fail "missing /usr/bin/systemctl"
command -v sudo >/dev/null 2>&1 || fail "missing sudo"
/usr/bin/systemctl cat bluealsa-aplay.service >/dev/null 2>&1 || fail "bluealsa-aplay.service is unavailable"
[ -f /etc/sudoers.d/x308-audio ] || fail "missing /etc/sudoers.d/x308-audio"
command -v visudo >/dev/null 2>&1 || fail "missing visudo"
visudo -cf /etc/sudoers.d/x308-audio >/dev/null 2>&1 || fail "sudoers syntax is invalid"

for action in start stop restart; do
    sudo -n -l -- /usr/bin/systemctl "$action" bluealsa-aplay.service \
        >/dev/null 2>&1 || fail "application user is not allowed to run systemctl $action bluealsa-aplay.service through sudo -n"
done

printf 'PASS: application user can manage bluealsa-aplay.service through sudo -n\n'
