#!/usr/bin/env bash
set -u

die() {
    printf 'FAIL: %s\n' "$1" >&2
    exit 1
}

if [ "$(id -u)" -ne 0 ]; then
    die "must be run as root (use sudo)"
fi

app_user=''
while [ "$#" -gt 0 ]; do
    case "$1" in
        --user)
            [ "$#" -ge 2 ] || die "--user requires a value"
            app_user=$2
            shift 2
            ;;
        --)
            shift
            [ "$#" -eq 0 ] || die "unexpected arguments"
            ;;
        *)
            die "unknown argument: $1"
            ;;
    esac
done

if [ -z "$app_user" ] && [ -n "${SUDO_USER:-}" ]; then
    app_user=$SUDO_USER
fi
[ -n "$app_user" ] || die "application user is required (--user <name>)"
printf '%s\n' "$app_user" | grep -Eq '^[a-z_][a-z0-9_-]*$' || die "unsafe user name"
id "$app_user" >/dev/null 2>&1 || die "user does not exist: $app_user"
[ -x /usr/bin/systemctl ] || die "missing /usr/bin/systemctl"
command -v visudo >/dev/null 2>&1 || die "missing visudo"

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
template="$script_dir/../deploy/sudoers/x308-audio"
[ -r "$template" ] || die "missing sudoers template: $template"

target=/etc/sudoers.d/x308-audio
tmp=$(mktemp /etc/sudoers.d/.x308-audio.XXXXXX) || die "cannot create temporary sudoers file"
cleanup() { rm -f -- "$tmp"; }
trap cleanup EXIT HUP INT TERM

sed "s/@APP_USER@/$app_user/g" "$template" >"$tmp" || die "cannot render sudoers template"
chmod 0440 "$tmp"
chown root:root "$tmp"
visudo -cf "$tmp" >/dev/null || die "sudoers syntax is invalid"
mv -f -- "$tmp" "$target" || die "cannot install $target"
trap - EXIT HUP INT TERM

printf 'PASS: installed %s for user %s\n' "$target" "$app_user"
printf 'PASS: sudoers syntax is valid\n'
