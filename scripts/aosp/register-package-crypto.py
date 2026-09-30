#!/usr/bin/env python3
"""Permit static SHA-256 for the isolated AEGIS package worker, pinned to AOSP.

This grants one build-package visibility entry only. Runtime credentials still
use Android's shared FIPS libcrypto/KeyMint; this static dependency is solely
for package metadata digests after leaving the Android filesystem namespace.
"""
import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile

REVISION = 'ecc1358826150d6a1851c517c325b0e6c0e1b8be'
ORIGINAL_SHA256 = 'c64b09f64b9fb5a2ba7836c7b964895db74edeef404a8380202bc429f0738430'
ANCHOR = b'    name: "libcrypto_static",\n    visibility: [\n'
ENTRY = b'        "//packages/aegis/identity", // AEGIS package SHA-256 only; no credential crypto.\n'


def register(aosp, verify=False):
    aosp = Path(aosp).resolve(strict=True)
    repo = aosp / 'external/boringssl'
    path = repo / 'Android.bp'
    if path.resolve(strict=True) != path or not path.is_file():
        raise ValueError('Unexpected BoringSSL source path')
    revision = subprocess.check_output(['git', '-C', str(repo), 'rev-parse', 'HEAD'], text=True).strip()
    if revision != REVISION:
        raise ValueError('Review static digest integration for changed BoringSSL revision')
    original = subprocess.check_output(['git', '-C', str(repo), 'show', 'HEAD:Android.bp'])
    if hashlib.sha256(original).hexdigest() != ORIGINAL_SHA256 or original.count(ANCHOR) != 1:
        raise ValueError('Pinned BoringSSL build source differs')
    wanted = original.replace(ANCHOR, ANCHOR + ENTRY)
    current = path.read_bytes()
    if current == wanted:
        print('Static package digest dependency verified: ' + hashlib.sha256(wanted).hexdigest())
        return
    if verify or current != original:
        raise ValueError('BoringSSL source differs from the exact owned visibility adjustment')
    # Preserve the pinned original outside module discovery. Never overwrite any
    # unrelated AOSP edit; atomic replacement prevents a partial Android.bp.
    backup = aosp / 'out/aegis-package-crypto'
    if backup.exists() and (backup.is_symlink() or not backup.is_dir()):
        raise ValueError('Unexpected backup path')
    backup.mkdir(parents=True, exist_ok=True)
    saved = backup / (ORIGINAL_SHA256 + '.bp')
    if saved.exists():
        if saved.is_symlink() or saved.read_bytes() != original:
            raise ValueError('Original BoringSSL backup differs')
    else:
        with saved.open('xb') as stream:
            stream.write(original)
    fd, temporary = tempfile.mkstemp(prefix='.aegis-package-crypto-', dir=repo)
    try:
        with os.fdopen(fd, 'wb') as stream:
            stream.write(wanted)
            os.fchmod(stream.fileno(), 0o644)
            stream.flush()
            os.fsync(stream.fileno())
        if path.read_bytes() != original:
            raise ValueError('BoringSSL source changed during preparation')
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)
    print('Static package digest dependency registered: ' + hashlib.sha256(wanted).hexdigest())


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--aosp', required=True, type=Path)
    parser.add_argument('--verify', action='store_true')
    args = parser.parse_args()
    register(args.aosp, args.verify)
