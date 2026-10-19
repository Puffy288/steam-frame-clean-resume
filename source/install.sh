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
[[ $(uname -m) == aarch64 ]] || { echo 'This installer is for Steam Frame (ARM64).' >&2; exit 1; }
[[ -x /opt/steamvr/bin/linuxarm64/vrcmd ]] || { echo 'SteamVR for Frame was not found.' >&2; exit 1; }
"$package_dir/resume-dashboard" --version

# Repeating this exact installation only re-enables the same service.
# A different or unrelated installation is never overwritten.
if [[ -e "$install_dir" || -L "$install_dir" || -e "$unit_file" || -L "$unit_file" ]]; then
    identical=true
    [[ ! -L "$install_dir" && ! -L "$unit_file" && -f "$install_dir/.managed-by-frame-resume-dashboard" ]] || identical=false
    for name in resume-dashboard resume-dashboard.c uninstall.sh; do
        cmp -s -- "$package_dir/$name" "$install_dir/$name" || identical=false
    done
    cmp -s -- "$package_dir/frame-resume-dashboard.service" "$unit_file" || identical=false
    if [[ $identical != true ]]; then
        echo 'An existing installation differs. Uninstall it first, then run this installer again.' >&2
        exit 1
    fi
    systemctl --user daemon-reload
    systemctl --user enable --now frame-resume-dashboard.service
    systemctl --user is-active --quiet frame-resume-dashboard.service
    echo 'Installed. The same native helper is enabled; no duplicate was created.'
    exit 0
fi

cleanup_failed_install() {
    trap - ERR
    systemctl --user disable --now frame-resume-dashboard.service 2>/dev/null || true
    rm -f -- "$unit_file" "$install_dir/resume-dashboard" "$install_dir/resume-dashboard.c" "$install_dir/uninstall.sh" "$install_dir/.managed-by-frame-resume-dashboard"
    rmdir -- "$install_dir" 2>/dev/null || true
    systemctl --user daemon-reload
    echo 'Setup did not complete; its new files were removed.' >&2
    exit 1
}
mkdir -p -- "$install_dir" "$unit_dir"
trap cleanup_failed_install ERR
install -m 755 -- "$package_dir/resume-dashboard" "$install_dir/resume-dashboard"
install -m 644 -- "$package_dir/resume-dashboard.c" "$install_dir/resume-dashboard.c"
install -m 755 -- "$package_dir/uninstall.sh" "$install_dir/uninstall.sh"
touch -- "$install_dir/.managed-by-frame-resume-dashboard"
install -m 644 -- "$package_dir/frame-resume-dashboard.service" "$unit_file"
systemctl --user daemon-reload
systemctl --user enable --now frame-resume-dashboard.service
systemctl --user is-active --quiet frame-resume-dashboard.service
trap - ERR
echo 'Installed. Leave your VR environment running, then suspend and wake to test.'
