#!/usr/bin/env python3
"""Observe a pinned EGL cache lifetime regression in a dedicated guest process.

Compile tests/fixtures/egl_cache_exit_probe.c as an ARM64 shared object first.
No guest property, installed library, service or protection setting is changed.
A locally built replacement library can be supplied for a component comparison;
that is not a full-image boot acceptance result.
"""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import re
import secrets
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--adb', default='127.0.0.1:15871')
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--library', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--expect', choices=('clean', 'destroyed-mutex'), required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    adb = ['adb', '-s', args.adb]
    root = adb + ['shell', 'su', '0']
    def read(*command):
        return subprocess.check_output(root + list(command), text=True, timeout=30).strip()
    def digest(path):
        return hashlib.sha256(path.read_bytes()).hexdigest()
    remote = '/data/local/tmp/aegis-egl-cache-test-' + secrets.token_hex(8)
    subprocess.run(root + ['mkdir', remote], check=True, timeout=30)
    subprocess.run(adb + ['push', str(args.probe), remote + '/probe.so'], check=True, timeout=30)
    library = '/system/lib64/libEGL.so'
    if args.library is not None:
        library = remote + '/libEGL.so'
        subprocess.run(adb + ['push', str(args.library), library], check=True, timeout=30)
    library_hash = read('sha256sum', library).split()[0]
    if args.library is not None and library_hash != digest(args.library):
        raise RuntimeError('Uploaded library checksum differs')
    result = {'started_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
              'boot_id': read('cat', '/proc/sys/kernel/random/boot_id'),
              'system_server_pid': read('pidof', 'system_server'),
              'probe_sha256': digest(args.probe), 'library_sha256': library_hash,
              'library_path': library, 'expected': args.expect,
              'scope': 'Dedicated process lifetime test; no full-image acceptance claim.'}
    started = time.monotonic()
    run = subprocess.run(root + ['timeout', '30', 'env', 'LD_PRELOAD=' + remote + '/probe.so',
                         'AEGIS_EGL_TEST_LIBRARY=' + library, '/system/bin/true'],
                         capture_output=True, timeout=45)
    output = run.stdout + run.stderr
    (args.output / 'process.log').write_bytes(output)
    result.update(exit_code=run.returncode, seconds=time.monotonic() - started,
                  log_sha256=hashlib.sha256(output).hexdigest(),
                  original_system_server_survived=read('pidof', 'system_server') == result['system_server_pid'],
                  same_boot=read('cat', '/proc/sys/kernel/random/boot_id') == result['boot_id'])
    queued = re.search(rb'EGL_CACHE_REGRESSION_QUEUED pid=(\d+) cache=(0x[0-9a-f]+)', output)
    fatal = re.search(rb'FORTIFY: pthread_mutex_lock called on a destroyed mutex \((0x[0-9a-f]+)\)', output)
    callbacks = all(marker in output for marker in (b'EGL_CACHE_REGRESSION_LATE_EXIT\n',
                                                    b'EGL_CACHE_REGRESSION_EXIT_FINISHED\n'))
    expected = bool(queued and callbacks and run.returncode == 0)
    if args.expect == 'destroyed-mutex':
        # Layout of the pinned unmodified ARM64 egl_cache_t: mutex at +0x34.
        expected = expected and bool(fatal and int(fatal[1], 16) == int(queued[2], 16) + 0x34)
    else:
        expected = expected and not any(token in output for token in (b'FORTIFY:', b'Fatal signal', b'CANNOT LINK'))
    result['expected_behavior_observed'] = bool(expected and result['same_boot'] and result['original_system_server_survived'])
    if queued:
        result['fixture_pid'] = int(queued[1])
    (args.output / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))
    if not result['expected_behavior_observed']:
        raise SystemExit('Expected lifecycle result not observed; retained output must be inspected')


if __name__ == '__main__':
    main()
