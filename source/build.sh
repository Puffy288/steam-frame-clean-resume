#!/bin/bash
# Developer build on ARM64 Linux with the systemd development headers installed.
# Ordinary Frame owners use the packaged installer and do not need a compiler.
set -euo pipefail
source_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
gcc -std=c11 -Os -Wall -Wextra -Werror -Wformat=2 -Wconversion \
    -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fPIE -pie \
    -Wl,-z,relro,-z,now -s "$source_dir/resume-dashboard.c" \
    -lsystemd -o "$source_dir/resume-dashboard"
"$source_dir/resume-dashboard" --version
