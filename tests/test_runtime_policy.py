"""Inert source-ownership tests; production policy is compiled on aegis-build."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('runtime_policy', ROOT / 'scripts/aosp/register-runtime-policy.py')
policy = importlib.util.module_from_spec(spec)
spec.loader.exec_module(policy)

DOMAIN = b'''# Limit device node creation to these allowed domains.
neverallow {
  domain
  -init
} self:global_capability_class_set mknod;
define(`dac_override_allowed', `{
  init
}')
neverallow {
    domain
    -apexd
    -dexopt_chroot_setup
} fs_type:filesystem mount;
# Allow calls to system(3), popen(3), ...
allow {
  domain
  -init
} shell_exec:file execute;
neverallow {
    domain
    -appdomain # for oemfs
} { fs_type -rootfs }:file execute;
allow { domain -appdomain -rs } cgroup:dir w_dir_perms;
allow { domain -appdomain -rs } cgroup:file w_file_perms;
allow { domain -appdomain -rs } cgroup_v2:dir w_dir_perms;
allow { domain -appdomain -rs } cgroup_v2:file w_file_perms;
'''

class PolicySources(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.aosp = self.root / 'aosp'
        self.project = self.root / 'project'
        self.project.mkdir()
        self.originals = {'public/attributes': b'attribute domain;\n', 'private/domain.te': DOMAIN}
        self.pins = {'schema': 1, 'aosp_tag': 'android-16.0.0_r1', 'sepolicy_commit': 'a' * 40,
                     'files': {name: hashlib.sha256(data).hexdigest() for name, data in self.originals.items()}}
        for name, data in self.originals.items():
            dest = self.aosp / 'system/sepolicy' / name
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_bytes(data)

    def prepare(self):
        return policy.prepare(self.project, self.aosp, originals=self.originals, pins=self.pins)

    def snapshot(self):
        return {str(p.relative_to(self.aosp)): p.read_bytes() for p in self.aosp.rglob('*') if p.is_file()}

    def test_installs_once_and_verifies_actual_owned_sources(self):
        receipt = self.prepare()
        before = self.snapshot()
        self.assertEqual(self.prepare(), receipt)
        self.assertEqual(self.snapshot(), before)
        policy.verify(self.aosp, receipt)
        self.assertIn(b'-aegis_runtime_mount_domain',
                      (self.aosp / 'system/sepolicy/private/domain.te').read_bytes())

    def test_rejects_wrong_pin_without_writing(self):
        self.pins['files']['private/domain.te'] = '0' * 64
        before = self.snapshot()
        with self.assertRaises(ValueError): self.prepare()
        self.assertEqual(self.snapshot(), before)

    def test_rejects_unmanaged_edit_before_changing_any_destination(self):
        (self.aosp / 'system/sepolicy/private/domain.te').write_bytes(DOMAIN + b'# unrelated edit\n')
        before = self.snapshot()
        with self.assertRaises(ValueError): self.prepare()
        self.assertEqual(self.snapshot(), before)

    def test_rejects_drift_even_with_a_previous_receipt(self):
        receipt = self.prepare()
        changed = self.aosp / 'system/sepolicy/public/attributes'
        changed.write_bytes(changed.read_bytes() + b'attribute unexpected;\n')
        before = self.snapshot()
        with self.assertRaises(ValueError): self.prepare()
        with self.assertRaises(ValueError): policy.verify(self.aosp, receipt)
        self.assertEqual(self.snapshot(), before)

    def test_rejects_redirected_destination(self):
        dest = self.aosp / 'system/sepolicy/public/attributes'
        other = self.root / 'other'
        other.write_bytes(dest.read_bytes())
        dest.unlink()
        dest.symlink_to(other)
        before = other.read_bytes()
        with self.assertRaises(ValueError): self.prepare()
        self.assertEqual(other.read_bytes(), before)

    def test_missing_or_duplicate_guard_is_not_silently_patched(self):
        with self.assertRaises(ValueError): policy.patch_domain(DOMAIN.replace(b'-apexd', b'-changed'))
        with self.assertRaises(ValueError): policy.patch_domain(DOMAIN + DOMAIN)

    def test_unknown_receipt_cannot_authorize_an_overwrite(self):
        self.prepare()
        marker = self.aosp / policy.MARKER
        record = json.loads(marker.read_bytes())
        record['outputs']['extra.te'] = '0' * 64
        marker.write_text(json.dumps(record))
        before = self.snapshot()
        with self.assertRaises(ValueError): self.prepare()
        self.assertEqual(self.snapshot(), before)

if __name__ == '__main__': unittest.main()
