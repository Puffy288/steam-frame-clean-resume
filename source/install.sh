#!/bin/bash
# Managed by frame-resume-dashboard
set -euo pipefail
if [[ $EUID == 0 ]]; then
    echo 'Run this as your normal Frame user, without sudo.' >&2
    exit 1
fi
package_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
install_dir="$HOME/.local/share/frame-resume-dashboard"
unit_dir="$HOME/.config/systemd/user"
unit_file="$unit_dir/frame-resume-dashboard.service"
test -x /opt/steamvr/bin/linuxarm64/vrcmd || { echo 'SteamVR for Frame was not found.' >&2; exit 1; }
/usr/bin/python3 -c 'import dbus; from gi.repository import Gio, GLib'
if [[ -e "$install_dir" && ! -f "$install_dir/.managed-by-frame-resume-dashboard" ]]; then
    echo "Refusing to overwrite an unrelated folder: $install_dir" >&2
    exit 1
fi
if [[ -e "$unit_file" || -L "$unit_file" ]]; then
    if ! grep -qFx '# Managed by frame-resume-dashboard' "$unit_file"; then
        echo "Refusing to overwrite an unrelated service: $unit_file" >&2
        exit 1
    fi
fi
# Preserve the last installed helper on upgrades; no Steam settings are edited.
if [[ -f "$install_dir/resume-dashboard.py" ]]; then
    backup_dir="$HOME/.local/state/frame-resume-dashboard/previous-install"
    mkdir -p -- "$backup_dir"
    cp -p -- "$install_dir/resume-dashboard.py" "$backup_dir/"
    if [[ -f "$unit_file" ]]; then cp -p -- "$unit_file" "$backup_dir/"; fi
    systemctl --user stop frame-resume-dashboard.service
fi
mkdir -p -- "$install_dir" "$unit_dir"
install -m 644 -- "$package_dir/resume-dashboard.py" "$install_dir/resume-dashboard.py"
install -m 755 -- "$package_dir/uninstall.sh" "$install_dir/uninstall.sh"
touch -- "$install_dir/.managed-by-frame-resume-dashboard"
install -m 644 -- "$package_dir/frame-resume-dashboard.service" "$unit_file"
systemctl --user daemon-reload
systemctl --user enable frame-resume-dashboard.service
systemctl --user restart frame-resume-dashboard.service
systemctl --user is-active --quiet frame-resume-dashboard.service
echo 'Installed. Leave your VR environment running, then suspend and wake to test.'
