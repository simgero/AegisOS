#!/usr/bin/env python3
"""Integrate explicit trusted-runtime attributes into the pinned platform policy.

No allow rule is added here. Positive permissions and closed membership guards
live in the product policy. Original domains keep all original neverallows.
Compilation, enforcing guest execution and isolation tests remain mandatory.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile

spec = importlib.util.spec_from_file_location('storage_sources',
        Path(__file__).with_name('register-runtime-storage.py'))
source_io = importlib.util.module_from_spec(spec)
spec.loader.exec_module(source_io)
checked_path, write_atomic, encoded = source_io.checked_path, source_io.write_atomic, source_io.encoded
MARKER = 'out/aegis-runtime-policy/sources.json'
FILES = {'public/attributes', 'private/domain.te'}
STATUS = 'POLICY_SOURCES_PREPARED_NOT_COMPILED_OR_ENFORCED'
ATTRIBUTES = b'''
# AEGIS platform trust boundaries; membership is closed in product policy.
# No ordinary Linux process receives mount, device creation or CE DAC authority.
attribute aegis_runtime_domain;
expandattribute aegis_runtime_domain false;
attribute aegis_runtime_mount_domain;
expandattribute aegis_runtime_mount_domain false;
attribute aegis_runtime_broker_domain;
expandattribute aegis_runtime_broker_domain false;
'''

def digest(data):
    return hashlib.sha256(data).hexdigest()

def patch_domain(data):
    text = data.decode()
    # New trusted subsystem subjects only; do not remove a neverallow or add
    # an existing domain to its exception set. In particular no sdcard/fuse
    # attribute is used to bypass filesystem restrictions.
    replacements = (
        ('# Limit device node creation to these allowed domains.\nneverallow {\n  domain\n',
         '# Limit device node creation to these allowed domains.\nneverallow {\n  domain\n  -aegis_runtime_broker_domain\n'),
        ("define(`dac_override_allowed', `{\n", "define(`dac_override_allowed', `{\n  aegis_runtime_broker_domain\n"),
        ('neverallow {\n    domain\n    -apexd\n    -dexopt_chroot_setup\n',
         'neverallow {\n    domain\n    -aegis_runtime_mount_domain\n    -apexd\n    -dexopt_chroot_setup\n'),
        ('# Allow calls to system(3), popen(3), ...\nallow {\n  domain\n',
         '# Allow calls to system(3), popen(3), ...\nallow {\n  domain\n  -aegis_runtime_domain\n'),
    )
    for old, new in replacements:
        text = source_io.replace_once(text, old, new)
    return text.encode()

def prepare(project, aosp, originals=None, pins=None):
    project, aosp = Path(project).absolute(), Path(aosp).absolute()
    policy = aosp / 'system/sepolicy'
    checked_path(aosp, 'system/sepolicy/private/domain.te')
    if pins is None:
        pins = json.loads(checked_path(project, 'runtime/aosp-policy.json').read_bytes())
    if originals is None:
        head = subprocess.check_output(['git', '-C', str(policy), 'rev-parse', 'HEAD'], text=True).strip()
        if head != pins['sepolicy_commit']:
            raise ValueError('Unexpected platform SELinux revision')
        originals = {name: subprocess.check_output(['git', '-C', str(policy), 'show', 'HEAD:' + name])
                     for name in FILES}
    if (pins.get('schema') != 1 or pins.get('aosp_tag') != 'android-16.0.0_r1'
            or set(pins.get('files', {})) != FILES or set(originals) != FILES
            or any(digest(originals[name]) != pins['files'][name] for name in FILES)):
        raise ValueError('Platform policy does not match the pinned originals')
    outputs = {'public/attributes': originals['public/attributes'] + ATTRIBUTES,
               'private/domain.te': patch_domain(originals['private/domain.te'])}
    record = {'schema': 1, 'status': STATUS, 'sepolicy_commit': pins['sepolicy_commit'],
              'inputs': pins['files'], 'outputs': {name: digest(data) for name, data in outputs.items()}}
    marker = checked_path(aosp, MARKER)
    previous = json.loads(marker.read_bytes()) if marker.exists() else None
    if previous is not None:
        validate_record(previous)
        if previous['inputs'] != record['inputs'] or previous['sepolicy_commit'] != record['sepolicy_commit']:
            raise ValueError('Policy ownership record belongs to another baseline')
    changes, before = {}, {}
    for name, data in outputs.items():
        target = checked_path(aosp, 'system/sepolicy/' + name)
        current = target.read_bytes()
        if current == data:
            continue
        if current != originals[name] and not (previous and digest(current) == previous['outputs'][name]):
            raise ValueError('Unmanaged platform policy edits; preserving them')
        changes[name], before[name] = data, current
    if changes:
        backup_parent = checked_path(aosp, 'out/aegis-runtime-policy/backups/.anchor').parent
        backup_parent.mkdir(parents=True, exist_ok=True)
        backup = Path(tempfile.mkdtemp(prefix='install-', dir=backup_parent))
        for name, data in before.items():
            target = backup / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
        (backup / 'intent.json').write_bytes(encoded(record))
        for name, data in changes.items():
            write_atomic(checked_path(aosp, 'system/sepolicy/' + name), data)
    verify(aosp, record)
    write_atomic(marker, encoded(record))
    return record

def validate_record(record):
    import re
    if (record.get('schema') != 1 or record.get('status') != STATUS
            or not re.fullmatch('[0-9a-f]{40}', record.get('sepolicy_commit', ''))
            or set(record.get('inputs', {})) != FILES or set(record.get('outputs', {})) != FILES
            or any(not re.fullmatch('[0-9a-f]{64}', value)
                   for value in [*record['inputs'].values(), *record['outputs'].values()])):
        raise ValueError('Invalid platform policy source receipt')

def verify(aosp, record):
    validate_record(record)
    for name, expected in record['outputs'].items():
        if digest(checked_path(Path(aosp).absolute(), 'system/sepolicy/' + name).read_bytes()) != expected:
            raise ValueError('Platform policy changed after integration')

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project', type=Path)
    parser.add_argument('--aosp', type=Path, required=True)
    parser.add_argument('--receipt', type=Path, required=True)
    parser.add_argument('--verify', action='store_true')
    args = parser.parse_args()
    if args.verify:
        verify(args.aosp, json.loads(args.receipt.read_bytes()))
    else:
        if args.project is None:
            parser.error('--project is required')
        if args.receipt.exists() or args.receipt.is_symlink():
            raise ValueError('Build receipt already exists')
        write_atomic(args.receipt, encoded(prepare(args.project, args.aosp)))
