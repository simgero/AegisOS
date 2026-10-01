import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('qemu_host',
        Path(__file__).resolve().parents[1] / 'scripts/qemu_host.py')
host = importlib.util.module_from_spec(spec)
spec.loader.exec_module(host)


class QemuHostTests(unittest.TestCase):
    def test_aosp_timeout_budget_tracks_actual_accelerator(self):
        for system, machine, kvm, expected in (
                ('Linux', 'x86_64', True, 50),
                ('Linux', 'aarch64', False, 50),
                ('Linux', 'aarch64', True, 3),
                ('Darwin', 'arm64', False, 3)):
            command = host.execution(system=system, machine=machine, kvm=kvm)
            self.assertEqual(host.hardware_timeout_multiplier(command), expected)

    def test_x86_linux_uses_arm_software_emulation_even_with_kvm(self):
        for kvm in (False, True):
            self.assertEqual(host.execution(system='Linux', machine='x86_64', kvm=kvm),
                    ['-machine', 'virt-10.2,highmem-ecam=off,gic-version=3', '-accel', 'tcg,thread=multi', '-cpu', 'max'])

    def test_native_arm_acceleration_and_no_silent_explicit_fallback(self):
        self.assertIn('hvf', host.execution(system='Darwin', machine='arm64'))
        self.assertIn('kvm', host.execution(system='Linux', machine='aarch64', kvm=True))
        self.assertIn('tcg,thread=multi', host.execution(system='Linux', machine='aarch64', kvm=False))
        for accel in ('kvm', 'hvf'):
            with self.assertRaises(ValueError):
                host.execution(accel, system='Linux', machine='x86_64', kvm=True)
