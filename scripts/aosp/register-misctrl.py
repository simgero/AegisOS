#!/usr/bin/env python3
"""Report a failed misc property write as failure, and a successful write as success."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

COMMIT = '80fbea7e9af1dd883f2046e9b299d6fe45a0f693'
RELATIVE = 'bootloader_message/misctrl_main.cpp'
ORIGINAL_SHA256 = '50ffb5d5313a18ee6968c08ad8009a514e4bc54b61cf45a3596b738cd67cbcac'


def patched(original):
    if hashlib.sha256(original).hexdigest() != ORIGINAL_SHA256:
        raise ValueError('Unrecognized pinned misctrl source')
    old = b'  res |= android::base::SetProperty("ro.misctrl.16kb_before", before_16kb ? "1" : "0");'
    if original.count(old) != 1:
        raise ValueError('misctrl property result anchor changed')
    return original.replace(old, b'''  if (!android::base::SetProperty("ro.misctrl.16kb_before", before_16kb ? "1" : "0")) {
    LOG(ERROR) << "Could not set ro.misctrl.16kb_before";
    res |= 1;
  }''', 1)


def install(root, original, verify=False):
    root = Path(root).absolute()
    path = root / RELATIVE
    if any(item.is_symlink() for item in (path, *path.parents)):
        raise ValueError('Refuse linked misctrl source')
    wanted = patched(original)
    current = path.read_bytes()
    if current not in (original, wanted):
        raise ValueError('Unmanaged misctrl edits; preserving them')
    if verify and current != wanted:
        raise ValueError('misctrl exit status fix is not installed')
    if not verify and current != wanted:
        temporary = path.with_suffix('.aegis-tmp')
        with temporary.open('xb') as stream:
            stream.write(wanted)
        temporary.replace(path)
    return {'status': 'MISCTRL_SOURCE_VERIFIED_NOT_BOOT_TESTED',
            'upstream_commit': COMMIT, 'path': RELATIVE,
            'original_sha256': ORIGINAL_SHA256,
            'output_sha256': hashlib.sha256(wanted).hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--aosp', required=True, type=Path)
    parser.add_argument('--receipt', required=True, type=Path)
    parser.add_argument('--verify', action='store_true')
    args = parser.parse_args()
    root = args.aosp / 'bootable/recovery'
    commit = subprocess.check_output(['git', '-C', str(root), 'rev-parse', 'HEAD'], text=True).strip()
    if commit != COMMIT:
        raise ValueError('Unexpected bootable/recovery baseline')
    original = subprocess.check_output(['git', '-C', str(root), 'show', 'HEAD:' + RELATIVE])
    receipt = install(root, original, args.verify)
    if args.verify:
        if json.loads(args.receipt.read_text()) != receipt:
            raise ValueError('misctrl receipt differs')
    else:
        args.receipt.write_text(json.dumps(receipt, indent=2) + '\n')
    print(receipt['status'])


if __name__ == '__main__':
    main()
