#!/usr/bin/env python3
"""Capture display and verify binary ADB transfer on a fresh local development VM.

Requires a completed boot, authenticated ADB, system user 0 only, and explicit
image/profile bindings. Does not provision ADB, create users or restart a VM.
This is a partial D1 test; virtual keyboard/mouse and visual inspection remain
separate requirements. Evidence and generated test bytes stay in the local run.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import uuid
from datetime import datetime, timezone

from qemu_control import execute


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run', type=Path, required=True)
    parser.add_argument('--prepared', type=Path, required=True)
    parser.add_argument('--commit', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if sys.flags.optimize:
        parser.error('Assertions must remain enabled; run without Python -O')
    if not re.fullmatch(r'[0-9a-f]{40}', args.commit):
        parser.error('Expected full image commit')
    if args.output.exists():
        parser.error('Preserve existing evidence; select a new output directory')
    run = args.run.resolve(strict=True)
    prepared = args.prepared.resolve(strict=True)
    address = (run / 'adb-address.txt').read_text().strip()
    if not re.fullmatch(r'127\.0\.0\.1:[0-9]{4,5}', address):
        parser.error('Expected local-only ADB endpoint')
    if not 1024 <= int(address.rsplit(':', 1)[1]) <= 65535:
        parser.error('Invalid ADB port')
    adb = ['adb', '-s', address]

    def call(argv):
        return subprocess.check_output(argv, stdin=subprocess.DEVNULL,
                                       stderr=subprocess.PIPE, timeout=120)

    def shell(*argv):
        return call(adb + ['shell', *argv]).decode().strip()

    profile = Path((run / 'profile-path.txt').read_text().strip()).resolve(strict=True)
    manifest_bytes = (profile / 'profile.json').read_bytes()
    manifest = json.loads(manifest_bytes)
    assert str(uuid.UUID(manifest['profile_id'])) == manifest['profile_id']
    assert Path(manifest['bindings']['base_disk']['path']).resolve() == prepared / 'android.raw'
    assert Path(manifest['bindings']['bootconfig']['path']).resolve() == prepared / 'runtime.bootconfig'
    receipt = json.loads((prepared / 'avb-checked.json').read_text())
    assert receipt['builder_commit'] == args.commit
    assert call(adb + ['get-state']).strip() == b'device'
    assert shell('getprop', 'ro.adb.secure') == '1'
    assert shell('getprop', 'sys.boot_completed') == '1'
    assert shell('getprop', 'ro.boot.vbmeta.digest') == receipt['vbmeta_digest']
    assert shell('getenforce') == 'Enforcing'
    assert re.findall(r'UserInfo\{([0-9]+):', shell('pm', 'list', 'users')) == ['0']
    assert set(re.findall(r'UserInfo\{([0-9]+):', shell('dumpsys', 'user'))) == {'0'}
    assert shell('am', 'get-current-user') == '0'
    boot_id = shell('cat', '/proc/sys/kernel/random/boot_id')
    pid = shell('pidof', 'system_server')
    assert pid.isdigit()
    start = shell('cat', '/proc/' + pid + '/stat').rsplit(')', 1)[1].split()[19]
    os.umask(0o077)
    output = args.output.absolute()
    output.mkdir(parents=True, exist_ok=False)
    scratch = '/data/local/tmp/aegis-adb-smoke-' + uuid.uuid4().hex
    report = {'status': 'INCOMPLETE', 'utc': datetime.now(timezone.utc).isoformat(),
              'image_commit': args.commit, 'profile_id': manifest['profile_id'],
              'profile_manifest_sha256': hashlib.sha256(manifest_bytes).hexdigest(),
              'boot_id': boot_id, 'system_server': {'pid': pid, 'starttime': start},
              'adb': address, 'scratch': scratch, 'cleanup': 'not_created',
              'limits': ['No keyboard/mouse or personal-user test.',
                         'Screenshot requires visual inspection.',
                         'Binary ADB roundtrip is not a reboot/persistence test.',
                         'Does not establish full D1 acceptance.']}
    created = False
    try:
        shell('mkdir', '-m', '700', scratch)
        created = True
        payload = os.urandom(262144)
        (output / 'sent.bin').write_bytes(payload)
        call(adb + ['push', str(output / 'sent.bin'), scratch + '/probe.bin'])
        call(adb + ['pull', scratch + '/probe.bin', str(output / 'received.bin')])
        assert (output / 'received.bin').read_bytes() == payload, 'ADB bytes differ'
        report['binary_transfer'] = {'bytes': len(payload),
                                     'sha256': hashlib.sha256(payload).hexdigest(),
                                     'byte_equal': True}
        qmp = Path((run / 'qmp-path.txt').read_text().strip())
        report['qmp_status'] = execute(qmp, 'query-status')
        assert report['qmp_status']['running']
        screenshot = output / 'screen.png'
        execute(qmp, 'screendump', {'filename': str(screenshot), 'format': 'png'})
        png = screenshot.read_bytes()
        assert png[:8] == b'\x89PNG\r\n\x1a\n' and png[12:16] == b'IHDR'
        width, height = int.from_bytes(png[16:20], 'big'), int.from_bytes(png[20:24], 'big')
        assert width > 0 and height > 0
        report['display'] = {'width': width, 'height': height,
                             'sha256': hashlib.sha256(png).hexdigest(),
                             'android_size': shell('wm', 'size')}
        assert shell('cat', '/proc/sys/kernel/random/boot_id') == boot_id
        assert shell('pidof', 'system_server') == pid
        assert shell('cat', '/proc/' + pid + '/stat').rsplit(')', 1)[1].split()[19] == start
        assert (profile / 'profile.json').read_bytes() == manifest_bytes
        report['status'] = 'DISPLAY_CAPTURED_BINARY_ADB_VERIFIED'
    finally:
        if created:
            try:
                shell('rm', '-f', scratch + '/probe.bin')
                shell('rmdir', scratch)
                report['cleanup'] = 'own_scratch_removed'
            except Exception as exc:
                report['cleanup'] = 'failed: ' + type(exc).__name__
                if report['status'] == 'DISPLAY_CAPTURED_BINARY_ADB_VERIFIED':
                    report['status'] = 'TRANSFER_VERIFIED_CLEANUP_FAILED'
        report['finished_utc'] = datetime.now(timezone.utc).isoformat()
        (output / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))
    return 0 if report['status'] == 'DISPLAY_CAPTURED_BINARY_ADB_VERIFIED' else 1


if __name__ == '__main__':
    raise SystemExit(main())
