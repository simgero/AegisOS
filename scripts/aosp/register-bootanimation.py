#!/usr/bin/env python3
"""Join the boot animation worker before main destroys process-wide state."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

COMMIT = '99b01a65cc4c104933788b3143285ab6bae65827'
RELATIVE = 'cmds/bootanimation/bootanimation_main.cpp'
ORIGINAL_SHA256 = '162c4e47a817a91d2579d4afbbd9e45d81d070cec32889026945232e156b173c'


def patched(original):
    if hashlib.sha256(original).hexdigest() != ORIGINAL_SHA256:
        raise ValueError('Unrecognized pinned boot animation source')
    old = b'        IPCThreadState::self()->joinThreadPool();'
    if original.count(old) != 1:
        raise ValueError('Boot animation shutdown anchor changed')
    return original.replace(old, old + b'''
        // stopProcess() wakes the main Binder loop before the animation
        // thread has returned. Keep process state alive through its teardown.
        boot->join();''', 1)


def install(root, original, verify=False):
    root = Path(root)
    path = root / RELATIVE
    for item in (root, *path.relative_to(root).parents):
        check = item if item == root else root / item
        if check.is_symlink():
            raise ValueError('Refuse linked boot animation source')
    if path.is_symlink():
        raise ValueError('Refuse linked boot animation source')
    wanted = patched(original)
    current = path.read_bytes()
    if current not in (original, wanted):
        raise ValueError('Unmanaged boot animation edits; preserving them')
    if verify and current != wanted:
        raise ValueError('Boot animation shutdown fix is not installed')
    if not verify and current != wanted:
        temporary = path.with_suffix('.aegis-tmp')
        with temporary.open('xb') as stream:
            stream.write(wanted)
        temporary.replace(path)
    return {'status': 'BOOTANIMATION_SOURCE_VERIFIED_NOT_BOOT_TESTED',
            'upstream_commit': COMMIT, 'path': RELATIVE,
            'original_sha256': ORIGINAL_SHA256,
            'output_sha256': hashlib.sha256(wanted).hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--aosp', required=True, type=Path)
    parser.add_argument('--receipt', required=True, type=Path)
    parser.add_argument('--verify', action='store_true')
    args = parser.parse_args()
    root = args.aosp / 'frameworks/base'
    commit = subprocess.check_output(['git', '-C', str(root), 'rev-parse', 'HEAD'], text=True).strip()
    if commit != COMMIT:
        raise ValueError('Unexpected frameworks/base baseline')
    original = subprocess.check_output(['git', '-C', str(root), 'show', 'HEAD:' + RELATIVE])
    receipt = install(root, original, args.verify)
    if args.verify:
        if json.loads(args.receipt.read_text()) != receipt:
            raise ValueError('Boot animation receipt differs')
    else:
        args.receipt.write_text(json.dumps(receipt, indent=2) + '\n')
    print(receipt['status'])


if __name__ == '__main__':
    main()
