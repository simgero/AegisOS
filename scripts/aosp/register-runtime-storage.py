#!/usr/bin/env python3
"""Prepare strictly pinned AOSP storage hooks; source editing only, never compilation."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

STORAGE = 'services/core/java/com/android/server/StorageManagerService.java'
USERS = 'services/core/java/com/android/server/am/UserController.java'
RESILIENT = 'services/core/java/com/android/server/pm/ResilientAtomicFile.java'
BRIDGE = 'services/core/java/com/android/server/aegis/AegisRuntimeStorage.java'
SOURCE = 'packages/aegis/identity/platform/com/android/server/aegis/AegisRuntimeStorage.java'
REMOVAL_FILES = 'services/core/java/com/android/server/aegis/AegisRemovalFiles.java'
REMOVAL_SOURCE = 'packages/aegis/identity/platform/com/android/server/aegis/AegisRemovalFiles.java'
ORIGINAL_FILES = {STORAGE, USERS, RESILIENT}
SOURCE_FILES = {BRIDGE: SOURCE, REMOVAL_FILES: REMOVAL_SOURCE}
MARKER = 'out/aegis-runtime-storage/sources.json'
PREFIX = 'com.android.server.aegis.AegisRuntimeStorage'


def digest(data):
    return hashlib.sha256(data).hexdigest()


def replace_once(text, old, new):
    if text.count(old) != 1:
        raise ValueError('Pinned AOSP hook anchor is missing or ambiguous')
    return text.replace(old, new, 1)


def patch_storage(data):
    text = data.decode('utf-8')
    for name, operation in (
            ('createUserStorageKeys', 'CREATE'), ('destroyUserStorageKeys', 'DESTROY'),
            ('setCeStorageProtection', 'PROTECT'), ('unlockCeStorage', 'UNLOCK'),
            ('lockCeStorage', 'LOCK')):
        start = f'    public void {name}('
        if text.count(start) != 1:
            raise ValueError('Storage method is missing or ambiguous')
        begin = text.index(start)
        end = text.index('\n    }\n', begin) + len('\n    }')
        method = text[begin:end]
        permission = f'        super.{name}_enforcePermission();\n'
        if method.count(permission) != 1:
            raise ValueError('Storage permission anchor changed')
        prefix, body = method.split(permission)
        body = body.removesuffix('\n    }').strip('\n')
        # A caller must not receive normal return/keyEvicted after vold failed.
        if operation in ('CREATE', 'DESTROY', 'LOCK'):
            old = '            Slog.wtf(TAG, e);\n'
            if operation == 'LOCK':
                old += '            return;\n'
            body = replace_once(body, old,
                    '            Slog.wtf(TAG, e);\n'
                    '            throw new IllegalStateException("AOSP storage-key mutation failed", e);\n')
        indented = '\n'.join('    ' + line if line else '' for line in body.splitlines())
        replacement = (prefix + permission + '\n'
                f'        try ({PREFIX}.Lease aegisStorageLease =\n'
                f'                {PREFIX}.begin(userId,\n'
                f'                        {PREFIX}.Operation.{operation})) {{\n'
                + indented + '\n        }\n    }')
        text = text[:begin] + replacement + text[end:]
    return text.encode('utf-8')


def patch_users(data):
    text = data.decode('utf-8')
    old = '''                mInjector.getStorageManager().lockCeStorage(userId);
            } catch (RemoteException re) {
                throw re.rethrowAsRuntimeException();
            }
            if (keyEvictedCallbacks == null) {'''
    new = '''                mInjector.getStorageManager().lockCeStorage(userId);
                if (mInjector.getStorageManager().isCeStorageUnlocked(userId)) {
                    Slogf.e(TAG, "CE key eviction not confirmed for user %d", userId);
                    return;
                }
            } catch (RemoteException | RuntimeException failure) {
                // Runtime quiescence or vold failed: preserve the failed state,
                // do not crash FgThread or report successful key eviction.
                Slogf.e(TAG, "CE key eviction failed for user %d", userId);
                return;
            }
            if (keyEvictedCallbacks == null) {'''
    return replace_once(text, old, new).encode('utf-8')


def patch_resilient(data):
    text = data.decode('utf-8')
    anchor = '    public void failWrite(FileOutputStream str) {\n'
    addition = '''    /** Checked commit for removal; the original UserData must remain reserved on error. */
    public void finishWriteChecked(FileOutputStream str) throws IOException {
        if (mMainOutStream != str || str == null) {
            throw new IllegalStateException("Invalid incoming stream.");
        }
        try {
            com.android.server.aegis.AegisRemovalFiles.commit(mFile, mTemporaryBackup,
                    mReserveCopy, mMainOutStream, mMainInStream, mReserveOutStream, mFileMode);
            // fs-verity rejects enabling a file with a live writable descriptor.
            // Retain only the two read descriptors, as AOSP's ordinary commit does.
            mMainOutStream.close();
            mMainOutStream = null;
            mReserveOutStream.close();
            mReserveOutStream = null;
            // Keep AOSP's best-effort fs-verity protection for both committed copies.
            try (ParcelFileDescriptor mainPfd = ParcelFileDescriptor.dup(mMainInStream.getFD());
                 ParcelFileDescriptor copyPfd = ParcelFileDescriptor.dup(mReserveInStream.getFD())) {
                FileIntegrity.setUpFsVerity(mainPfd);
                FileIntegrity.setUpFsVerity(copyPfd);
            } catch (IOException e) {
                Slog.e(LOG_TAG, "Failed to verity-protect " + mDebugName, e);
            }
        } finally {
            close();
        }
    }

    /** No ignored delete booleans and no fallback file left behind on success. */
    public void deleteChecked() throws IOException {
        if (mMainOutStream != null || mMainInStream != null || mReserveOutStream != null
                || mReserveInStream != null || mCurrentInStream != null) {
            throw new IllegalStateException("Metadata file is still in use");
        }
        com.android.server.aegis.AegisRemovalFiles.delete(mFile, mTemporaryBackup, mReserveCopy);
    }

'''
    return replace_once(text, anchor, addition + anchor).encode('utf-8')


def checked_path(root, relative):
    current = root
    if root.is_symlink() or not root.is_dir():
        raise ValueError('Expected an ordinary source root')
    for part in Path(relative).parts:
        if part in ('..', '.') or part == '/' or not part:
            raise ValueError('Invalid integration path')
        current = current / part
        if current.is_symlink():
            raise ValueError('Refusing a symlink in an integration path')
        if current.exists() and current != root / relative and not current.is_dir():
            raise ValueError('Integration ancestor is not a directory')
    if current.exists() and not current.is_file():
        raise ValueError('Integration destination is not a regular file')
    return current


def write_atomic(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix='.aegis-storage-', dir=path.parent)
    try:
        with os.fdopen(fd, 'wb') as file:
            file.write(data)
            file.flush()
            os.fsync(file.fileno())
        os.chmod(temporary, 0o644)
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)  # Only our newly allocated temporary file.


def encoded(value):
    return (json.dumps(value, sort_keys=True, indent=2) + '\n').encode()


def originals_from_git(base):
    result = {}
    for name in sorted(ORIGINAL_FILES):
        result[name] = subprocess.run(['git', '-C', str(base), 'show', 'HEAD:' + name],
                                      check=True, capture_output=True).stdout
    return result


def validate_record(record):
    legacy = isinstance(record, dict) and record.get('schema') == 1
    expected_inputs = {STORAGE, USERS} if legacy else ORIGINAL_FILES
    expected_outputs = {STORAGE, USERS, BRIDGE} if legacy else ORIGINAL_FILES | SOURCE_FILES.keys()
    if (not isinstance(record, dict) or record.get('schema') not in (1, 2)
            or record.get('status') != 'FRAMEWORK_SOURCES_PREPARED_NOT_TESTED'
            or record.get('aosp_tag') != 'android-16.0.0_r1'
            or not isinstance(record.get('inputs'), dict)
            or set(record['inputs']) != expected_inputs
            or not isinstance(record.get('outputs'), dict)
            or set(record['outputs']) != expected_outputs
            or any(not isinstance(value, str) or not re.fullmatch('[0-9a-f]{64}', value)
                   for value in [*record['inputs'].values(), *record['outputs'].values()])
            or record.get('bridge_sha256') != record['outputs'][BRIDGE]):
        raise ValueError('Invalid storage-source receipt')


def prepare(project, aosp, originals=None, pins=None):
    project, aosp = Path(project).absolute(), Path(aosp).absolute()
    # Optional in-process inputs are only for inert integration fixtures. CLI
    # always reads the committed AOSP Git blobs and repository pin, no override.
    base = aosp / 'frameworks/base'
    for name in ORIGINAL_FILES:
        checked_path(aosp, 'frameworks/base/' + name)
    if originals is None:
        originals = originals_from_git(base)
    if pins is None:
        pins = json.loads(checked_path(project, 'runtime/aosp-storage-hooks.json').read_bytes())
    if (pins.get('schema') != 1 or pins.get('aosp_tag') != 'android-16.0.0_r1'
            or set(pins.get('files', {})) != ORIGINAL_FILES
            or set(originals) != ORIGINAL_FILES
            or any(digest(originals[name]) != pins['files'][name] for name in originals)):
        raise ValueError('AOSP Git source does not match the exact pinned storage baseline')
    sources = {name: checked_path(project, source).read_bytes()
               for name, source in SOURCE_FILES.items()}
    outputs = {STORAGE: patch_storage(originals[STORAGE]),
               USERS: patch_users(originals[USERS]),
               RESILIENT: patch_resilient(originals[RESILIENT]), **sources}
    record = {'schema': 2, 'status': 'FRAMEWORK_SOURCES_PREPARED_NOT_TESTED',
              'aosp_tag': pins['aosp_tag'], 'inputs': pins['files'],
              'bridge_sha256': digest(sources[BRIDGE]),
              'outputs': {name: digest(data) for name, data in outputs.items()}}
    marker = checked_path(aosp, MARKER)
    previous = None
    if marker.exists():
        previous = json.loads(marker.read_bytes())
        validate_record(previous)
        if any(pins['files'].get(name) != value for name, value in previous['inputs'].items()):
            raise ValueError('Invalid storage-source ownership record')
    changes = {}
    before = {}
    # Validate EVERY destination before writing any of them.
    for name, data in outputs.items():
        target = checked_path(aosp, 'frameworks/base/' + name)
        current = target.read_bytes() if target.exists() else None
        if current == data:
            continue  # Also recognizes an interrupted installation of these exact bytes.
        allowed = current is None if name in SOURCE_FILES else current == originals[name]
        if previous and current is not None and name in previous['outputs']:
            allowed |= digest(current) == previous['outputs'][name]
        if not allowed:
            raise ValueError('Unmanaged or modified AOSP storage source; preserving local work')
        changes[name] = data
        before[name] = current
    if changes:
        # Keep backups outside AOSP Java-source discovery. A partially completed
        # installation is never a license to overwrite unrelated changes.
        backup_parent = checked_path(aosp, 'out/aegis-runtime-storage/backups/.anchor').parent
        backup_parent.mkdir(parents=True, exist_ok=True)
        backup = Path(tempfile.mkdtemp(prefix='install-', dir=backup_parent))
        for name, data in before.items():
            if data is not None:
                target = backup / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(data)
        (backup / 'intent.json').write_bytes(encoded(record))
        for name, data in changes.items():
            write_atomic(checked_path(aosp, 'frameworks/base/' + name), data)
    verify(aosp, record)
    write_atomic(marker, encoded(record))
    return record


def verify(aosp, record):
    aosp = Path(aosp).absolute()
    validate_record(record)
    for name, expected in record['outputs'].items():
        if digest(checked_path(aosp, 'frameworks/base/' + name).read_bytes()) != expected:
            raise ValueError('AOSP storage sources changed after preparation')


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
            parser.error('--project is required for source preparation')
        if args.receipt.exists() or args.receipt.is_symlink():
            raise ValueError('Refusing to overwrite an existing build receipt')
        result = prepare(args.project, args.aosp)
        write_atomic(args.receipt, encoded(result))
