#!/usr/bin/env python3
"""Developer packaging only; Python is not used by the installed helper."""
from pathlib import Path
import base64
import gzip
import hashlib
import shutil
import zipfile

root = Path(__file__).resolve().parent.parent
source = root / 'source'
dist = root / 'dist'
dist.mkdir(exist_ok=True)
installer_name = 'Steam-Frame-Resume-Install.run'
payload_names = ['resume-dashboard', 'resume-dashboard.c', 'frame-resume-dashboard.service', 'install.sh', 'uninstall.sh']
binary = (source / 'resume-dashboard').read_bytes()
assert binary[:4] == b'\x7fELF' and int.from_bytes(binary[18:20], 'little') == 183, 'Build the ARM64 helper first.'

parts = ['''#!/bin/bash
# Steam Frame Clean Resume 0.2.0: fresh, user-local native installation.
# Includes the ARM64 executable and readable C source. No download or sudo.
# Install: bash ~/Downloads/Steam-Frame-Resume-Install.run
# Remove: bash ~/.local/share/frame-resume-dashboard/uninstall.sh
set -euo pipefail
if [[ $EUID == 0 ]]; then
    echo 'Run this as your normal Frame user, without sudo.' >&2
    exit 1
fi
umask 077
stage_dir=$(mktemp -d /tmp/frame-resume-dashboard.XXXXXXXX)
trap 'rm -rf -- "$stage_dir"' EXIT
base64 -d <<'FRAME_RESUME_BINARY_END' | gzip -dc > "$stage_dir/resume-dashboard"
''']
encoded = base64.b64encode(gzip.compress(binary, compresslevel=9, mtime=0)).decode()
parts.extend(encoded[i:i+76]+'\n' for i in range(0,len(encoded),76))
parts.append('FRAME_RESUME_BINARY_END\nchmod 755 "$stage_dir/resume-dashboard"\n')
for index,name in enumerate(payload_names[1:]):
    content = (source / name).read_text()
    delimiter = f'FRAME_RESUME_TEXT_{index}_END'
    assert delimiter not in content
    parts.append(f'cat > "$stage_dir/{name}" <<\'{delimiter}\'\n{content.rstrip()}\n{delimiter}\n')
parts.append('cat > "$stage_dir/SHA256SUMS.txt" <<\'FRAME_RESUME_CHECKSUMS_END\'\n')
for name in payload_names:
    parts.append(f'{hashlib.sha256((source/name).read_bytes()).hexdigest()}  {name}\n')
parts.append('''FRAME_RESUME_CHECKSUMS_END
(cd -- "$stage_dir" && sha256sum --check --status SHA256SUMS.txt)
bash "$stage_dir/install.sh"
''')
installer = root / installer_name
installer.write_text(''.join(parts))
installer.chmod(0o644)
shutil.copyfile(installer, dist / installer_name)

repo_files = sorted([p for p in root.iterdir() if p.is_file() and p.name not in ('SHA256SUMS.txt',)] +
                    [p for folder in ('source','docs','tools') for p in (root/folder).rglob('*') if p.is_file()])
checksums = ''.join(f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.relative_to(root).as_posix()}\n' for p in repo_files)
(root / 'SHA256SUMS.txt').write_text(checksums)

def archive(path, entries):
    with zipfile.ZipFile(path, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for file, name in entries:
            info = zipfile.ZipInfo(name, date_time=(2026,10,19,0,0,0))
            info.external_attr = (0o100755 if file.name == 'resume-dashboard' or file.suffix == '.sh' else 0o100644) << 16
            info.compress_type = zipfile.ZIP_DEFLATED
            z.writestr(info, file.read_bytes(), compresslevel=9)

archive(dist / 'steam-frame-clean-resume-guide.zip', [(root/'steam-frame-resume-dashboard.html','steam-frame-resume-dashboard.html'),(installer,installer_name)])
archive(dist / 'steam-frame-clean-resume-source.zip', [(p,'steam-frame-clean-resume/'+p.relative_to(root).as_posix()) for p in repo_files+[root/'SHA256SUMS.txt']])
release_files = [dist/installer_name,dist/'steam-frame-clean-resume-guide.zip',dist/'steam-frame-clean-resume-source.zip']
(dist / 'SHA256SUMS.txt').write_text(''.join(f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n' for p in release_files))
print(f'Built installer: {installer.stat().st_size:,} bytes; all release assets are in dist/.')
