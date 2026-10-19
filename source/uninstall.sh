#!/bin/bash
# Managed by frame-resume-dashboard
set -euo pipefail
install_dir="$HOME/.local/share/frame-resume-dashboard"
unit_file="$HOME/.config/systemd/user/frame-resume-dashboard.service"
if [[ ! -f "$install_dir/.managed-by-frame-resume-dashboard" ]]; then
    echo 'No managed installation found.'
    exit 0
fi
systemctl --user disable --now frame-resume-dashboard.service
rm -f -- "$unit_file" "$install_dir/resume-dashboard.py" "$install_dir/uninstall.sh" "$install_dir/.managed-by-frame-resume-dashboard"
rmdir -- "$install_dir" 2>/dev/null || true
systemctl --user daemon-reload
systemctl --user reset-failed frame-resume-dashboard.service 2>/dev/null || true
echo 'Uninstalled. Normal dashboard-on-resume behaviour is restored.'
