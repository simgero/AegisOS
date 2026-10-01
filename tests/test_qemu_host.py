import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('qemu_host',
        Path(__file__).resolve().parents[1] / 'scripts/qemu_host.py')
host = importlib.util.module_from_spec(spec)
spec.loader.exec_module(host)


class QemuHostTests(unittest.TestCase):
    def test_x86_linux_uses_arm_software_emulation_even_with_kvm(self):
        for kvm in (False, True):
            self.assertEqual(host.execution(system='Linux', machine='x86_64', kvm=kvm),
                    ['-machine', 'virt-10.2,gic-version=3', '-accel', 'tcg,thread=multi', '-cpu', 'max'])

    def test_native_arm_acceleration_and_no_silent_explicit_fallback(self):
        self.assertIn('hvf', host.execution(system='Darwin', machine='arm64'))
        self.assertIn('kvm', host.execution(system='Linux', machine='aarch64', kvm=True))
        self.assertIn('tcg,thread=multi', host.execution(system='Linux', machine='aarch64', kvm=False))
        for accel in ('kvm', 'hvf'):
            with self.assertRaises(ValueError):
                host.execution(accel, system='Linux', machine='x86_64', kvm=True)
