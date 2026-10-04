#!/usr/bin/env python3
"""Read active system-partition verity tables and mounts on a local QEMU guest.

Requires authenticated development ADB with existing su support. Reads only
the eight named system verity tables, never userdata encryption tables.
Does not change mappings, corrupt blocks, provision ADB or restart services.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run', required=True, type=Path)
    parser.add_argument('--prepared', required=True, type=Path)
    parser.add_argument('--commit', required=True)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    if sys.flags.optimize or not re.fullmatch(r'[0-9a-f]{40}', args.commit):
        parser.error('Require full image commit and enabled assertions')
    if args.output.exists():
        parser.error('Preserve previous evidence; choose a new output directory')
    address = (args.run / 'adb-address.txt').read_text().strip()
    if not re.fullmatch(r'127\.0\.0\.1:[0-9]{4,5}', address):
        parser.error('Require local-only ADB endpoint')
    if not 1024 <= int(address.rsplit(':', 1)[1]) <= 65535:
        parser.error('Invalid ADB port')
    profile = Path((args.run / 'profile-path.txt').read_text().strip())
    manifest_bytes = (profile / 'profile.json').read_bytes()
    manifest = json.loads(manifest_bytes)
    prepared = args.prepared.resolve(strict=True)
    assert Path(manifest['bindings']['base_disk']['path']).resolve() == prepared / 'android.raw'
    assert Path(manifest['bindings']['bootconfig']['path']).resolve() == prepared / 'runtime.bootconfig'
    receipt = json.loads((prepared / 'avb-checked.json').read_text())
    assert receipt['builder_commit'] == args.commit

    def read(command):
        return subprocess.check_output(
            ['adb', '-s', address, 'shell', 'su 0 sh -c ' + shlex.quote(command)],
            stdin=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True, timeout=60).strip()

    def identity():
        pid = read('pidof system_server')
        assert pid.isdecimal()
        return {'boot_id': read('cat /proc/sys/kernel/random/boot_id'),
                'system_server': {'pid': pid, 'starttime':
                    read('cat /proc/' + pid + '/stat').rsplit(')', 1)[1].split()[19]}}

    assert read('getprop ro.adb.secure') == '1'
    assert read('getprop sys.boot_completed') == '1'
    assert read('getenforce') == 'Enforcing'
    assert read('getprop ro.boot.veritymode') == 'enforcing'
    assert read('getprop ro.boot.vbmeta.digest') == receipt['vbmeta_digest']
    before = identity()
    os.umask(0o077)
    args.output.mkdir(parents=True, exist_ok=False)
    report = {'status': 'INCOMPLETE', 'utc': datetime.now(timezone.utc).isoformat(),
              'image_commit': args.commit, 'profile_id': manifest['profile_id'],
              'profile_manifest_sha256': hashlib.sha256(manifest_bytes).hexdigest(),
              'vbmeta_digest': receipt['vbmeta_digest'], **before, 'partitions': {},
              'limits': ['Read-only observation; no deliberate corruption test.',
                         'No userdata encryption tables or personal files read.',
                         'No hardware root of trust or bootloader lock claimed.',
                         'Does not establish full D1 or reboot persistence.']}
    try:
        mounts_text = read('cat /proc/mounts')
        (args.output / 'mounts.txt').write_text(mounts_text + '\n')
        mounts = [line.split() for line in mounts_text.splitlines()]
        for name in ('system', 'system_ext', 'product', 'vendor', 'odm',
                     'system_dlkm', 'vendor_dlkm', 'odm_dlkm'):
            target = '/' if name == 'system' else '/' + name
            matches = [m for m in mounts if len(m) == 6 and m[1] == target]
            assert len(matches) == 1, (name, 'missing or ambiguous mount')
            mount = matches[0]
            assert re.fullmatch(r'/dev/block/dm-[0-9]+', mount[0])
            assert mount[2] == 'erofs' and 'ro' in mount[3].split(',')
            device = mount[0].rsplit('/', 1)[1]
            dm_name = read('cat /sys/class/block/' + device + '/dm/name')
            assert dm_name == name + '-verity', (name, dm_name)
            table = read('dmctl table ' + name + '-verity')
            status = read('dmctl status ' + name + '-verity')
            verified = read('getprop partition.' + name + '.verified')
            report['partitions'][name] = {
                'table': table, 'status': status, 'mount': mount,
                'device_major_minor': read('cat /sys/class/block/' + device + '/dev'),
                'dm_name': dm_name, 'verified_property': verified}
            table_rows = table.splitlines()[1:]
            status_rows = status.splitlines()[1:]
            assert len(table_rows) == len(status_rows) == 1
            assert re.match(r'^0-[0-9]+: verity, ', table_rows[0])
            assert 'restart_on_corruption' in table_rows[0].split()
            assert not {'ignore_corruption', 'ignore_once', 'check_at_most_once'} & set(table_rows[0].split())
            assert re.fullmatch(r'0-[0-9]+: verity, V', status_rows[0])
            assert table_rows[0].split(':', 1)[0] == status_rows[0].split(':', 1)[0]
            assert verified == '2'
        assert identity() == before, 'Boot or SystemServer changed during observation'
        assert (profile / 'profile.json').read_bytes() == manifest_bytes
        assert read('cat /proc/mounts') == mounts_text, 'Mount inventory changed'
        report['status'] = 'EIGHT_ACTIVE_VERITY_MAPPINGS_AND_READ_ONLY_MOUNTS_VERIFIED'
    except Exception as exc:
        report['error'] = type(exc).__name__ + ': ' + str(exc)
        raise
    finally:
        report['finished_utc'] = datetime.now(timezone.utc).isoformat()
        report['verifier_sha256'] = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
        (args.output / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
    print(report['status'])
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
