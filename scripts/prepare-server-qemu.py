#!/usr/bin/env python3
"""Verify local development AVB images and create fresh auxiliary QEMU partitions."""
import argparse
import json
import re
from pathlib import Path
import struct
import subprocess
import sys


def prepare(root, avbtool, commit):
    if not re.fullmatch('[0-9a-f]{40}', commit):
        raise ValueError('Expected the full source commit of the images')
    root = root.resolve()
    images = root / 'images'
    if any((root / name).exists() for name in
           ('runtime.bootconfig', 'metadata.img', 'misc.img', 'frp.img', 'avb-checked.json')):
        raise ValueError('Use a new preparation directory; existing state is never replaced')
    verified = subprocess.run([sys.executable, str(avbtool), 'verify_image', '--image', 'vbmeta.img',
                              '--follow_chain_partitions'], cwd=images,
                              capture_output=True, text=True, check=True)
    digest = subprocess.check_output([sys.executable, str(avbtool), 'calculate_vbmeta_digest',
            '--image', 'vbmeta.img', '--hash_algorithm', 'sha256'], cwd=images, text=True).strip()
    size = 0
    for name in ('vbmeta', 'boot', 'init_boot', 'vbmeta_system', 'vbmeta_system_dlkm', 'vbmeta_vendor_dlkm'):
        with (images / (name + '.img')).open('rb') as stream:
            data = stream.read(256)
            if data[:4] != b'AVB0':
                stream.seek(-64, 2)
                footer = stream.read(64)
                if footer[:4] != b'AVBf':
                    raise ValueError('Expected an AVB footer')
                stream.seek(struct.unpack_from('>Q', footer, 20)[0])
                data = stream.read(256)
        if data[:4] != b'AVB0':
            raise ValueError('Expected AVB metadata')
        auth, aux = struct.unpack_from('>QQ', data, 12)
        size += 256 + auth + aux
    for name, length in (('metadata', 16 * 1024**2), ('misc', 1024**2), ('frp', 1024**2)):
        with (root / (name + '.img')).open('xb') as output:
            output.truncate(length)
    subprocess.run(['mkfs.ext4', '-q', '-F', str(root / 'metadata.img')], check=True)
    (root / 'runtime.bootconfig').write_text(
        f'androidboot.vbmeta.hash_alg=sha256\nandroidboot.vbmeta.size={size}\n'
        f'androidboot.vbmeta.digest={digest}\n'
        'androidboot.vendor.apex.com.android.hardware.keymint=com.android.hardware.keymint.rust_cf_remote\n'
        'androidboot.vendor.apex.com.android.hardware.gatekeeper=com.android.hardware.gatekeeper.cf_remote\n'
        'androidboot.vendor.apex.com.android.hardware.graphics.composer=com.android.hardware.graphics.composer.ranchu\n')
    (root / 'avb-checked.json').write_text(json.dumps({
        'builder_commit': commit,
        'status': 'TEST_KEY_AVB_CHECKED_PARTITIONS_PREPARED_NOT_BOOTED',
        'vbmeta_digest': digest, 'vbmeta_size': size, 'verification': verified.stdout,
        'trust': 'embedded AOSP development keys; direct QEMU kernel launch'}, indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--avbtool', required=True, type=Path)
    parser.add_argument('--commit', required=True)
    args = parser.parse_args()
    prepare(args.directory, args.avbtool.resolve(), args.commit)
