#!/usr/bin/env python3
"""Expose the terminal pmsg read result without changing liblog return semantics."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

COMMIT = 'd78b713380007d3c0dde14712cbcbec27f491ad9'
RELATIVE = 'liblog/pmsg_reader.cpp'
ORIGINAL_SHA256 = '15c3239d82783d248bf6f229203efb25b1a47d62213f0b9a963e68511119cc54'


def patched(original):
    if hashlib.sha256(original).hexdigest() != ORIGINAL_SHA256:
        raise ValueError('Unrecognized pinned pmsg source')
    changes = (
        (b'  while (PmsgRead(&logger_list, &log_msg) > 0) {',
         b'  int read_result;\n  while ((read_result = PmsgRead(&logger_list, &log_msg)) > 0) {'),
        (b'  return (ret == SSIZE_MAX) ? -ENOENT : ret;', b'''  const ssize_t result = (ret == SSIZE_MAX) ? -ENOENT : ret;
  // Record only numeric outcomes, never filenames or retained log contents.
  // A negative read result can otherwise disappear behind the aggregate result.
  // Keep the existing API semantics; callers still see every original failure.
  __android_log_print(ANDROID_LOG_INFO, "liblog-pmsg",
                      "pmsg file read: terminal=%d aggregate=%zd result=%zd",
                      read_result, ret, result);
  return result;'''),
    )
    for old, new in changes:
        if original.count(old) != 1:
            raise ValueError('pmsg diagnostic anchor changed')
        original = original.replace(old, new, 1)
    return original


def install(root, original, verify=False):
    path = Path(root).absolute() / RELATIVE
    if any(item.is_symlink() for item in (path, *path.parents)):
        raise ValueError('Refuse linked pmsg source')
    wanted = patched(original)
    current = path.read_bytes()
    if current not in (original, wanted):
        raise ValueError('Unmanaged pmsg edits; preserving them')
    if verify and current != wanted:
        raise ValueError('pmsg diagnostics are not installed')
    if not verify and current != wanted:
        temporary = path.with_suffix('.aegis-tmp')
        with temporary.open('xb') as stream:
            stream.write(wanted)
        temporary.replace(path)
    return {'status': 'PMSG_DIAGNOSTICS_SOURCE_VERIFIED_NOT_BOOT_TESTED',
            'upstream_commit': COMMIT, 'path': RELATIVE,
            'original_sha256': ORIGINAL_SHA256,
            'output_sha256': hashlib.sha256(wanted).hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--aosp', required=True, type=Path)
    parser.add_argument('--receipt', required=True, type=Path)
    parser.add_argument('--verify', action='store_true')
    args = parser.parse_args()
    root = args.aosp / 'system/logging'
    commit = subprocess.check_output(['git', '-C', str(root), 'rev-parse', 'HEAD'], text=True).strip()
    if commit != COMMIT:
        raise ValueError('Unexpected system/logging baseline')
    original = subprocess.check_output(['git', '-C', str(root), 'show', 'HEAD:' + RELATIVE])
    receipt = install(root, original, args.verify)
    if args.verify:
        if json.loads(args.receipt.read_text()) != receipt:
            raise ValueError('pmsg diagnostic receipt differs')
    else:
        args.receipt.write_text(json.dumps(receipt, indent=2) + '\n')
    print(receipt['status'])


if __name__ == '__main__':
    main()
