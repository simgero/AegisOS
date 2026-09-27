#!/usr/bin/env python3
"""Run as aegis-build: package existing ARM64 host binaries and build helper init."""
import argparse
import hashlib
from pathlib import Path
import re
import shutil
import subprocess
import tarfile

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('output', type=Path)
args = p.parse_args()
aosp = Path('/srv/aegis/work/aosp')
host = aosp/'out/host/linux_musl-arm64'
root = args.output/'root'
root.mkdir(parents=True, exist_ok=False)
for folder in ['host/bin','host/lib64','lib','state/logs','state/internal','tmp','proc','sys','dev']:
    (root/folder).mkdir(parents=True, exist_ok=True)

# Follow only ELF DT_NEEDED dependencies, never execute the ARM64 programs here.
pending = [('bin/secure_env', host/'bin/secure_env')]
seen = set()
while pending:
    relative, source = pending.pop()
    if relative in seen:
        continue
    seen.add(relative)
    data = source.read_bytes()
    if data[:4] != b'\x7fELF' or int.from_bytes(data[18:20], 'little') != 183:
        raise ValueError(f'Expected AArch64 ELF: {source}')
    shutil.copyfile(source, root/'host'/relative)
    (root/'host'/relative).chmod(0o755)
    dynamic = subprocess.check_output(['readelf','-d',str(source)], text=True)
    for name in re.findall(r'\(NEEDED\).*?\[([^\]]+)\]', dynamic):
        if '/' in name:
            raise ValueError('Unexpected dependency path')
        pending.append(('lib64/'+name,host/'lib64'/name))

program = subprocess.check_output(['readelf','-l',str(host/'bin/secure_env')],text=True)
interpreter = re.search(r'Requesting program interpreter: ([^\]]+)',program)
if not interpreter:
    raise ValueError('Expected the AOSP musl dynamic executable')
loader = interpreter.group(1)
if loader != '/lib/ld-musl-aarch64.so.1':
    raise ValueError(f'Unexpected interpreter: {loader}')
shutil.copyfile(host/'lib64/libc_musl.so',root/'lib/ld-musl-aarch64.so.1')
(root/'lib/ld-musl-aarch64.so.1').chmod(0o755)

clang = sorted((aosp/'prebuilts/clang/host/linux-x86').glob('clang-r*/bin/clang'))[-1]
subprocess.run([str(clang),'--target=aarch64-linux-gnu','-fuse-ld=lld','-nostdlib',
                '-static','-ffreestanding','-fno-builtin','-fno-stack-protector',
                '-O2','-Wl,-e,_start',str(Path(__file__).with_name('init.c')),
                '-o',str(root/'init')],check=True)
(root/'state/cuttlefish_config.json').write_text(
    '{"root_dir":"/state","instances":{"1":{"instance_dir":"/state",'
    '"instance_uds_dir":"/state","run_as_daemon":false}}}\n')
archive = args.output/'secure-env-arm64.tar.gz'
with tarfile.open(archive,'w:gz') as tar:
    for item in sorted(root.rglob('*')):
        tar.add(item,arcname=item.relative_to(root),recursive=False)
if archive.stat().st_size >= 1900*1024**2:
    raise ValueError('Helper package exceeds single GitHub asset limit')
digest = hashlib.sha256(archive.read_bytes()).hexdigest()
(args.output/'SHA256SUMS').write_text(f'{digest}  {archive.name}\n')
print(f'Packaged {len(seen)} original ELF files plus helper init: {archive}',flush=True)
