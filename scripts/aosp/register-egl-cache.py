#!/usr/bin/env python3
"""Preserve the pinned EGL cache through process exit and deferred save work."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

COMMIT = '2827a4a16b0340ecd07c2d5a6c89991799b362bb'
PREFIX = 'opengl/libs/EGL/'
ORIGINAL_SHA256 = {
    'egl_cache.cpp': 'c8109015708793b2352fa686fd776fcdc8fbe61874053c43aeef3ebc2b4fb176',
    'egl_cache.h': '057b0f59c0904e4b47875af9d92e5f9dc899a472c4b08fdec057b806ab099cc5',
}


def patched(name, original):
    if name not in ORIGINAL_SHA256 or hashlib.sha256(original).hexdigest() != ORIGINAL_SHA256[name]:
        raise ValueError('Unrecognized pinned EGL cache source')
    if name == 'egl_cache.cpp':
        return original.replace(
            b': mInitialized(false), mMultifileMode(false),',
            b': mInitialized(false), mSavePending(false), mMultifileMode(false),', 1
        ).replace(b'egl_cache_t egl_cache_t::sCache;\n\n', b'', 1).replace(
            b'    return &sCache;', b'''    // Deferred save workers retain this object after eglTerminate(). Keep the
    // documented process-lifetime singleton alive through static destruction,
    // so a late worker cannot lock an already-destroyed mMutex. The OS reclaims
    // this one object at process exit; terminate() still flushes/releases caches.
    static egl_cache_t* const cache = new egl_cache_t;
    return cache;''', 1)
    return original.replace(
        b'    // sCache is the singleton egl_cache_t object.\n    static egl_cache_t sCache;\n\n',
        b'', 1)


def install(root, originals, verify=False):
    root = Path(root)
    pending = []
    records = {}
    # Validate every source before changing either file.
    for name, original in originals.items():
        if name not in ORIGINAL_SHA256:
            raise ValueError('Unexpected EGL cache source')
        path = root / PREFIX / name
        if any(item.is_symlink() for item in (path, *path.parents)):
            raise ValueError('Refuse linked EGL cache source')
        wanted = patched(name, original)
        current = path.read_bytes()
        if current not in (original, wanted):
            raise ValueError('Unmanaged EGL cache edits; preserving them')
        if verify and current != wanted:
            raise ValueError('EGL cache lifetime fix is not installed')
        pending.append((path, current, wanted))
        records[PREFIX + name] = {'original_sha256': ORIGINAL_SHA256[name],
                                  'output_sha256': hashlib.sha256(wanted).hexdigest()}
    if set(originals) != set(ORIGINAL_SHA256):
        raise ValueError('Incomplete EGL cache source set')
    if not verify:
        for path, current, wanted in pending:
            if current != wanted:
                temporary = path.with_suffix(path.suffix + '.aegis-tmp')
                with temporary.open('xb') as stream:
                    stream.write(wanted)
                temporary.replace(path)
    return {'status': 'EGL_CACHE_SOURCE_VERIFIED_NOT_RUNTIME_TESTED',
            'upstream_commit': COMMIT, 'files': records}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--aosp', required=True, type=Path)
    parser.add_argument('--receipt', required=True, type=Path)
    parser.add_argument('--verify', action='store_true')
    args = parser.parse_args()
    root = args.aosp / 'frameworks/native'
    if subprocess.check_output(['git', '-C', str(root), 'rev-parse', 'HEAD'], text=True).strip() != COMMIT:
        raise ValueError('Unexpected frameworks/native baseline')
    originals = {name: subprocess.check_output(['git', '-C', str(root), 'show', 'HEAD:' + PREFIX + name])
                 for name in ORIGINAL_SHA256}
    receipt = install(root, originals, args.verify)
    if args.verify:
        if json.loads(args.receipt.read_text()) != receipt:
            raise ValueError('EGL cache source receipt differs')
    else:
        args.receipt.write_text(json.dumps(receipt, indent=2) + '\n')
    print(receipt['status'])


if __name__ == '__main__':
    main()
