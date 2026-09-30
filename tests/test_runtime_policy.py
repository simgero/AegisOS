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
neverallow ~dac_override_allowed self:global_capability_class_set dac_override;
neverallow ~{
  dac_override_allowed
  traced_perf
  traced_probes
  heapprofd
} self:global_capability_class_set dac_read_search;
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
neverallow {
    domain
    -appdomain
    with_asan(`-asan_extract')
    -shell
} { file_type -system_file_type -exec_type }:file execute;
neverallow * { file_type -exec_type -postinstall_file }:file entrypoint;
neverallow coredomain {
    file_type
    -system_file_type
    -postinstall_file
}:file entrypoint;
domain_auto_trans({ domain userdebug_or_eng(`-su') }, crash_dump_exec, crash_dump);
allow domain system_linker_exec:file { execute read open getattr map };
allow domain system_lib_file:file { execute read open getattr map };
allow { appdomain coredomain } system_file:file { execute read open getattr map };
allow domain system_file:file { execute read open getattr map };
allow domain vendor_file_type:file { execute read open getattr map };
allow domain vndk_sp_file:file { execute read open getattr map };
allow { domain -appdomain -rs } cgroup:dir w_dir_perms;
allow { domain -appdomain -rs } cgroup:file w_file_perms;
allow { domain -appdomain -rs } cgroup_v2:dir w_dir_perms;
allow { domain -appdomain -rs } cgroup_v2:file w_file_perms;
'''
VOLD = b'''# Only vold should ever add/remove file-based encryption keys.
neverallowxperm {
  domain
  -vold
} data_file_type:dir ioctl { FS_IOC_ADD_ENCRYPTION_KEY FS_IOC_REMOVE_ENCRYPTION_KEY FS_IOC_GET_ENCRYPTION_KEY_STATUS };
'''

class PolicySources(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.aosp = self.root / 'aosp'
        self.project = self.root / 'project'
        self.project.mkdir()
        self.originals = {'public/attributes': b'attribute domain;\n',
                          'private/attributes': b'attribute private_example;\n',
                          'private/domain.te': DOMAIN, 'private/vold.te': VOLD}
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
        with self.assertRaises(ValueError): policy.patch_domain(DOMAIN.replace(b"with_asan(`-asan_extract')", b"with_asan(`-other')"))
        with self.assertRaises(ValueError): policy.patch_vold(VOLD.replace(b'-vold', b'-changed'))
        with self.assertRaises(ValueError): policy.patch_vold(VOLD + VOLD)

    def test_new_package_subject_exception_keeps_entrypoint_guards(self):
        output = policy.patch_domain(DOMAIN)
        self.assertIn(b'neverallow {\n    domain\n    -aegis_package_exec_domain\n', output)
        for guard in (
            b'neverallow * { file_type -exec_type -postinstall_file }:file entrypoint;',
            b'neverallow coredomain {\n    file_type\n    -system_file_type\n    -postinstall_file\n}:file entrypoint;',
        ):
            self.assertIn(guard, output)
        # A changed existing subject list must not silently broaden this exception.
        with self.assertRaises(ValueError):
            policy.patch_domain(DOMAIN.replace(b"    -appdomain\n", b"    -newdomain\n"))

    def test_namespace_dac_exception_does_not_change_host_or_search_authority(self):
        output = policy.patch_domain(DOMAIN)
        self.assertIn(b'neverallow ~dac_override_allowed self:capability dac_override;', output)
        self.assertIn(b'neverallow ~{ dac_override_allowed aegis_package_dac_domain } self:cap_userns dac_override;', output)
        self.assertIn(b'neverallow ~{\n  dac_override_allowed\n  traced_perf\n  traced_probes\n  heapprofd\n} self:global_capability_class_set dac_read_search;', output)
        with self.assertRaises(ValueError):
            policy.patch_domain(DOMAIN.replace(b'self:global_capability_class_set dac_override;',
                                              b'self:global_capability_class_set { dac_override dac_read_search };'))

    def test_package_execution_does_not_inherit_android_executable_mappings(self):
        output = policy.patch_domain(DOMAIN)
        for target in ('system_linker_exec', 'system_lib_file', 'system_file', 'vendor_file_type', 'vndk_sp_file'):
            self.assertIn(f'allow {{ domain -aegis_package_exec_domain }} {target}:file execute;'.encode(), output)
            self.assertIn(f'allow domain {target}:file {{ read open getattr map }};'.encode(), output)
        self.assertIn(b'allow { appdomain coredomain -aegis_package_exec_domain } system_file:file execute;', output)
        with self.assertRaises(ValueError):
            policy.patch_domain(DOMAIN.replace(b'allow domain vndk_sp_file:file', b'allow domain changed_file:file'))

    def test_package_program_cannot_inherit_crash_dump_transition(self):
        output = policy.patch_domain(DOMAIN)
        self.assertIn(b"domain_auto_trans({ domain -aegis_package_exec_domain userdebug_or_eng(`-su') }, crash_dump_exec, crash_dump);", output)
        with self.assertRaises(ValueError):
            policy.patch_domain(DOMAIN.replace(b'crash_dump_exec, crash_dump', b'changed_exec, crash_dump'))

    def legacy(self):
        outputs = {'public/attributes': self.originals['public/attributes'] + policy.ATTRIBUTES,
                   'private/domain.te': policy.patch_domain(DOMAIN)}
        for name, data in outputs.items():
            (self.aosp / 'system/sepolicy' / name).write_bytes(data)
        marker = self.aosp / policy.MARKER
        marker.parent.mkdir(parents=True)
        marker.write_bytes(policy.encoded({'schema': 1, 'status': policy.STATUS,
            'sepolicy_commit': self.pins['sepolicy_commit'],
            'inputs': {name: self.pins['files'][name] for name in outputs},
            'outputs': {name: policy.digest(data) for name, data in outputs.items()}}))

    def test_owned_legacy_public_attributes_move_private_without_changing_public_api(self):
        self.legacy()
        receipt = self.prepare()
        self.assertEqual(self.originals['public/attributes'],
                         (self.aosp / 'system/sepolicy/public/attributes').read_bytes())
        self.assertIn(policy.ATTRIBUTES,
                      (self.aosp / 'system/sepolicy/private/attributes').read_bytes())
        policy.verify(self.aosp, receipt)

    def test_legacy_receipt_cannot_authorize_edits_to_newly_managed_files(self):
        self.legacy()
        (self.aosp / 'system/sepolicy/private/vold.te').write_bytes(VOLD + b'# unrelated\n')
        before = self.snapshot()
        with self.assertRaises(ValueError): self.prepare()
        self.assertEqual(before, self.snapshot())

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
