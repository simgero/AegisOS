#!/usr/bin/env python3
"""Run as aegis-build: package existing ARM64 host binaries and build helper init."""
import argparse
import hashlib
from pathlib import Path
import re
import os
import struct
import shutil
import subprocess
import tarfile

def validate_program(data, dependencies):
    """Accept musl PT_INTERP or AOSP's embedded relinterp startup."""
    if (len(data) < 64 or data[:7] != b'\x7fELF\x02\x01\x01'
            or struct.unpack_from('<H', data, 18)[0] != 183):
        raise ValueError('Expected little-endian ELF64 AArch64 program')
    offset = struct.unpack_from('<Q', data, 32)[0]
    size, count = struct.unpack_from('<HH', data, 54)
    if size != 56 or not count or offset + size * count > len(data):
        raise ValueError('Invalid ELF program headers')
    interpreters = []
    for i in range(count):
        kind, _, start, _, _, length, _, _ = struct.unpack_from('<IIQQQQQQ', data, offset + i * size)
        if kind == 3:  # PT_INTERP
            if not length or start + length > len(data) or data[start + length - 1] != 0:
                raise ValueError('Invalid ELF interpreter')
            interpreters.append(data[start:start + length - 1])
    if interpreters:
        if interpreters != [b'/lib/ld-musl-aarch64.so.1']:
            raise ValueError(f'Unexpected interpreter: {interpreters!r}')
        return 'musl PT_INTERP'
    # AOSP android/relinterp.c is linked into dynamic host executables.
    # It searches LD_LIBRARY_PATH and RUNPATH for libc_musl.so itself.
    # Absence of PT_INTERP alone is not evidence of a static executable.
    if 'libc_musl.so' not in dependencies or b'relinterp: ' not in data:
        raise ValueError('Missing AOSP musl relinterp startup or libc dependency')
    return 'AOSP embedded relinterp'


def main():
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
        dynamic = subprocess.check_output(['readelf','-d',str(source)], text=True,
                                          env={**os.environ, 'LC_ALL': 'C'})
        for name in re.findall(r'\(NEEDED\).*?\[([^\]]+)\]', dynamic):
            if '/' in name:
                raise ValueError('Unexpected dependency path')
            pending.append(('lib64/'+name,host/'lib64'/name))

    layout = validate_program((host/'bin/secure_env').read_bytes(),
                              {Path(name).name for name in seen})
    print(f'Validated startup: {layout}', flush=True)
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


if __name__ == '__main__':
    main()
