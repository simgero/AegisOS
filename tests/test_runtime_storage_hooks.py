import importlib.util
import copy
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('storage_hooks', ROOT / 'scripts/aosp/register-runtime-storage.py')
hooks = importlib.util.module_from_spec(spec)
spec.loader.exec_module(hooks)


class StorageHookSourcesTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.project = self.root / 'project'
        self.aosp = self.root / 'aosp'
        fixtures = ROOT / 'tests/fixtures/aosp_storage_hooks'
        self.originals = {
            hooks.STORAGE: (fixtures / 'StorageManagerService.fragment').read_bytes(),
            hooks.USERS: (fixtures / 'UserController.fragment').read_bytes(),
            hooks.RESILIENT: (fixtures / 'ResilientAtomicFile.fragment').read_bytes(),
        }
        self.pins = {'schema': 1, 'aosp_tag': 'android-16.0.0_r1',
                     'files': {name: hooks.digest(data) for name, data in self.originals.items()}}
        self.source = self.project / hooks.SOURCE
        self.source.parent.mkdir(parents=True)
        self.source.write_bytes(b'first inert bridge-source fixture\n')
        self.removal_source = self.project / hooks.REMOVAL_SOURCE
        self.removal_source.write_bytes(b'inert checked-files fixture\n')
        for name, data in self.originals.items():
            destination = self.target(name)
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(data)

    def target(self, name):
        return self.aosp / 'frameworks/base' / name

    def prepare(self):
        return hooks.prepare(self.project, self.aosp, originals=self.originals, pins=self.pins)

    def test_real_method_fragments_are_wrapped_and_reuse_is_idempotent(self):
        receipt = self.prepare()
        hooks.verify(self.aosp, receipt)
        storage = self.target(hooks.STORAGE).read_text()
        self.assertEqual(storage.count('try (' + hooks.PREFIX + '.Lease'), 5)
        for kind in ('CREATE', 'DESTROY', 'PROTECT', 'UNLOCK', 'LOCK'):
            self.assertEqual(storage.count('.Operation.' + kind + '))'), 1)
        self.assertLess(storage.index('super.lockCeStorage_enforcePermission();'),
                        storage.index('.Operation.LOCK'))
        self.assertLess(storage.index('.Operation.LOCK'), storage.index('if (!isCeStorageUnlocked(userId))'))
        self.assertEqual(storage.count('throw new IllegalStateException("AOSP storage-key mutation failed", e);'), 3)
        users = self.target(hooks.USERS).read_text()
        self.assertIn('catch (RemoteException | RuntimeException failure)', users)
        self.assertLess(users.index('isCeStorageUnlocked(userId)'), users.index('.keyEvicted(userId)'))
        backups = list((self.aosp / 'out/aegis-runtime-storage/backups').iterdir())
        self.assertEqual(len(backups), 1)
        self.assertEqual((backups[0] / hooks.STORAGE).read_bytes(), self.originals[hooks.STORAGE])
        self.assertEqual(self.prepare(), receipt)
        self.assertEqual(list((self.aosp / 'out/aegis-runtime-storage/backups').iterdir()), backups)

    def test_updated_owned_bridge_preserves_its_previous_bytes(self):
        self.prepare()
        self.source.write_bytes(b'second inert bridge-source fixture\n')
        receipt = self.prepare()
        self.assertEqual(self.target(hooks.BRIDGE).read_bytes(), self.source.read_bytes())
        hooks.verify(self.aosp, receipt)
        copies = list((self.aosp / 'out/aegis-runtime-storage/backups').glob('*/' + hooks.BRIDGE))
        self.assertEqual(len(copies), 1)
        self.assertEqual(copies[0].read_bytes(), b'first inert bridge-source fixture\n')

    def test_unmanaged_changes_block_all_writes(self):
        self.target(hooks.USERS).write_bytes(b'builders local work\n')
        with self.assertRaises(ValueError): self.prepare()
        self.assertEqual(self.target(hooks.STORAGE).read_bytes(), self.originals[hooks.STORAGE])
        self.assertEqual(self.target(hooks.USERS).read_bytes(), b'builders local work\n')
        self.assertFalse(self.target(hooks.BRIDGE).exists())
        self.assertFalse((self.aosp / 'out').exists())

    def test_wrong_baseline_or_changed_hook_anchor_never_installs(self):
        self.pins['files'][hooks.STORAGE] = '0' * 64
        with self.assertRaises(ValueError): self.prepare()
        self.pins['files'][hooks.STORAGE] = hooks.digest(self.originals[hooks.STORAGE])
        altered = self.originals[hooks.STORAGE].replace(b'super.lockCeStorage_enforcePermission()', b'changedPermission()')
        self.originals[hooks.STORAGE] = altered
        self.pins['files'][hooks.STORAGE] = hooks.digest(altered)
        with self.assertRaises(ValueError): self.prepare()
        self.assertFalse(self.target(hooks.BRIDGE).exists())
        self.assertFalse((self.aosp / 'out').exists())

    def test_symlinked_destination_ancestor_is_not_followed(self):
        outside = self.root / 'outside'
        outside.mkdir()
        target = self.target(hooks.BRIDGE)
        target.parent.symlink_to(outside, target_is_directory=True)
        with self.assertRaises(ValueError): self.prepare()
        self.assertEqual(list(outside.iterdir()), [])
        self.assertEqual(self.target(hooks.STORAGE).read_bytes(), self.originals[hooks.STORAGE])

    def test_corrupt_ownership_record_never_adopts_modified_sources(self):
        receipt = self.prepare()
        receipt['outputs'][hooks.USERS] = 'not-a-digest'
        (self.aosp / hooks.MARKER).write_text(json.dumps(receipt))
        self.source.write_bytes(b'updated bridge fixture')
        with self.assertRaises(ValueError): self.prepare()
        self.assertEqual(self.target(hooks.BRIDGE).read_bytes(), b'first inert bridge-source fixture\n')

    def test_interrupted_install_can_finish_only_known_exact_outputs(self):
        real_write = hooks.write_atomic
        calls = 0

        def interrupted(target, data):
            nonlocal calls
            calls += 1
            real_write(target, data)
            if calls == 1:
                raise OSError('simulated interruption after one atomic source write')

        with patch.object(hooks, 'write_atomic', side_effect=interrupted):
            with self.assertRaises(OSError): self.prepare()
        self.assertFalse((self.aosp / hooks.MARKER).exists())
        self.assertEqual(self.target(hooks.USERS).read_bytes(), self.originals[hooks.USERS])
        receipt = self.prepare()
        hooks.verify(self.aosp, receipt)
        self.assertEqual(self.target(hooks.STORAGE).read_bytes(), hooks.patch_storage(self.originals[hooks.STORAGE]))

    def test_verification_detects_post_preparation_changes(self):
        receipt = self.prepare()
        self.target(hooks.BRIDGE).write_bytes(b'changed after preparation')
        with self.assertRaises(ValueError): hooks.verify(self.aosp, receipt)
        with self.assertRaises(ValueError): self.prepare()
        self.assertEqual(self.target(hooks.BRIDGE).read_bytes(), b'changed after preparation')

    def test_receipt_verification_rejects_wrong_baseline_metadata(self):
        receipt = self.prepare()
        for field, value in (('aosp_tag', 'other'), ('inputs', []), ('outputs', None),
                             ('inputs', {hooks.STORAGE: 'bad', hooks.USERS: '0' * 64})):
            modified = copy.deepcopy(receipt)
            modified[field] = value
            with self.subTest(field=field, value=value):
                with self.assertRaises(ValueError): hooks.verify(self.aosp, modified)
        for invalid in (None, [], 'invalid'):
            with self.assertRaises(ValueError): hooks.verify(self.aosp, invalid)

    def test_exact_legacy_receipt_migrates_without_adopting_new_file_changes(self):
        legacy = {'schema': 1, 'status': 'FRAMEWORK_SOURCES_PREPARED_NOT_TESTED',
                  'aosp_tag': 'android-16.0.0_r1',
                  'inputs': {k: self.pins['files'][k] for k in (hooks.STORAGE, hooks.USERS)}}
        old_outputs = {hooks.STORAGE: hooks.patch_storage(self.originals[hooks.STORAGE]),
                       hooks.USERS: hooks.patch_users(self.originals[hooks.USERS]),
                       hooks.BRIDGE: self.source.read_bytes()}
        legacy['outputs'] = {k: hooks.digest(v) for k, v in old_outputs.items()}
        legacy['bridge_sha256'] = legacy['outputs'][hooks.BRIDGE]
        for name, data in old_outputs.items():
            self.target(name).parent.mkdir(parents=True, exist_ok=True)
            self.target(name).write_bytes(data)
        marker = self.aosp / hooks.MARKER
        marker.parent.mkdir(parents=True)
        marker.write_text(json.dumps(legacy))
        hooks.verify(self.aosp, legacy)
        self.target(hooks.RESILIENT).write_bytes(b'local unrelated atomic-file change')
        with self.assertRaises(ValueError): self.prepare()
        self.assertFalse(self.target(hooks.REMOVAL_FILES).exists())
        self.assertEqual(json.loads(marker.read_text()), legacy)
        self.target(hooks.RESILIENT).write_bytes(self.originals[hooks.RESILIENT])
        result = self.prepare()
        self.assertEqual(result['schema'], 2)
        hooks.verify(self.aosp, result)
        self.assertEqual(self.target(hooks.REMOVAL_FILES).read_bytes(), self.removal_source.read_bytes())
        self.assertEqual(self.prepare(), result)

    def test_invalid_legacy_extension_is_not_a_valid_ownership_record(self):
        receipt = self.prepare()
        receipt['schema'] = 1
        (self.aosp / hooks.MARKER).write_text(json.dumps(receipt))
        with self.assertRaises(ValueError): self.prepare()

    def test_checked_atomic_file_methods_are_installed_without_replacing_legacy_failure_path(self):
        self.prepare()
        source = self.target(hooks.RESILIENT).read_text()
        self.assertIn('AegisRemovalFiles.commit(', source)
        self.assertIn('AegisRemovalFiles.delete(', source)
        self.assertIn('FileIntegrity.setUpFsVerity(mainPfd)', source)
        for writable in ('mMainOutStream', 'mReserveOutStream'):
            self.assertLess(source.index(writable + '.close();'),
                            source.index('FileIntegrity.setUpFsVerity(mainPfd)'))
        self.assertIn('originalFailureHandling();', source)


if __name__ == '__main__':
    unittest.main()
