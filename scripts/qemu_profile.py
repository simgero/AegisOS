#!/usr/bin/env python3
"""Paired Android/TPM development storage. Never repairs or recreates missing state."""
from contextlib import contextmanager
import fcntl
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import uuid


def digest(path):
    value = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda: stream.read(4*1024**2), b''):
            value.update(chunk)
    return value.hexdigest()


def image_info(path):
    return json.loads(subprocess.check_output(
        ['qemu-img', 'info', '--output=json', str(path)], text=True))


def ext4_uuid(path):
    with path.open('rb') as stream:
        stream.seek(1024)
        block = stream.read(120)
    if len(block) != 120 or block[56:58] != b'\x53\xef':
        raise ValueError('Helper state is not an ext4 filesystem')
    return str(uuid.UUID(bytes=block[104:120]))


def bindings(images, disk, bootconfig, helper_assets):
    files = {'base_disk': disk, 'bootconfig': bootconfig,
             'helper_archive': helper_assets/'secure-env-arm64.tar.gz'}
    files.update({name: images/name for name in
                  ['kernel', 'vendor_boot.img', 'ramdisk.img', 'vendor-bootconfig.img']})
    return {name: {'path': str(path.resolve()), 'sha256': digest(path)}
            for name, path in files.items()}


def create(path, images, disk, bootconfig, helper_assets, mkfs=None):
    """Explicit provisioning only. An incomplete directory is retained on error."""
    path = path.absolute()
    path.mkdir(mode=0o700, parents=True, exist_ok=False)
    identity = str(uuid.uuid4())
    recorded = bindings(images, disk, bootconfig, helper_assets)
    android = path/'android.qcow2'
    subprocess.run(['qemu-img', 'create', '-f', 'qcow2', '-F', 'raw', '-b',
                    str(disk.resolve()), str(android)], check=True, capture_output=True)
    # This immutable creation snapshot identifies the overlay even after writes.
    # It is an identity marker, not a paired backup or a rollback facility.
    subprocess.run(['qemu-img', 'snapshot', '-c', 'aegis-profile-'+identity,
                    str(android)], check=True)
    state = path/'secure-env.ext4'
    with state.open('xb') as stream:
        stream.truncate(64*1024**2)
    mkfs = mkfs or shutil.which('mkfs.ext4')
    if not mkfs:
        candidate = Path('/opt/homebrew/opt/e2fsprogs/sbin/mkfs.ext4')
        if candidate.is_file():
            mkfs = str(candidate)
    if not mkfs:
        raise RuntimeError('mkfs.ext4 is required to provision the helper disk')
    with tempfile.TemporaryDirectory(prefix='aegis-state-seed-') as directory:
        seed = Path(directory)
        for name in ['logs', 'internal', 'instances/cvd-1/logs', 'instances/cvd-1/internal']:
            (seed/name).mkdir(parents=True, exist_ok=True)
        (seed/'profile-id').write_text(identity+'\n')
        (seed/'fresh').touch()
        (seed/'cuttlefish_config.json').write_text(json.dumps({
            'root_dir': '/state', 'instances': {'1': {
                'instance_dir': '/state', 'instance_uds_dir': '/state', 'run_as_daemon': False
            }}
        })+'\n')
        subprocess.run([mkfs, '-q', '-F', '-m', '0', '-U', identity, '-d',
                        str(seed), str(state)], check=True)
    for file in [android, state]:
        file.chmod(0o600)
    manifest = {'version': 1, 'profile_id': identity, 'bindings': recorded}
    temporary = path/'profile.json.tmp'
    temporary.write_text(json.dumps(manifest, indent=2)+'\n')
    temporary.replace(path/'profile.json')
    return manifest


def validate(path, images, disk, bootconfig, helper_assets):
    for name in ['profile.json', 'android.qcow2', 'secure-env.ext4']:
        file = path/name
        if file.is_symlink() or not file.is_file():
            raise ValueError(f'Missing or redirected profile component: {name}')
    manifest = json.loads((path/'profile.json').read_text())
    identity = manifest.get('profile_id')
    if manifest.get('version') != 1 or str(uuid.UUID(identity)) != identity:
        raise ValueError('Unsupported profile manifest')
    if manifest['bindings'] != bindings(images, disk, bootconfig, helper_assets):
        raise ValueError('Profile inputs changed; explicit migration is required')
    info = image_info(path/'android.qcow2')
    if (info.get('format') != 'qcow2'
            or Path(info.get('full-backing-filename', '')).resolve() != disk.resolve()
            or info.get('backing-filename-format') != 'raw'
            or 'aegis-profile-'+identity not in [s.get('name') for s in info.get('snapshots', [])]):
        raise ValueError('Android overlay belongs to another profile or backing image')
    if ext4_uuid(path/'secure-env.ext4') != identity:
        raise ValueError('Helper state belongs to another profile')
    return manifest


@contextmanager
def locked(path):
    """Hold for the entire lifetime of both VMs, including validation/shutdown."""
    if path.is_symlink() or not path.is_dir():
        raise ValueError('Expected an existing real profile directory')
    fd = os.open(path/'.lock', os.O_RDWR | os.O_CREAT | os.O_NOFOLLOW, 0o600)
    try:
        try:
            fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError as error:
            raise RuntimeError('This VM profile is already in use') from error
        yield
    finally:
        os.close(fd)
