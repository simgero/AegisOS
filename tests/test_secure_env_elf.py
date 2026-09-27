"""Regression coverage for AOSP host binaries with an embedded musl starter."""
import importlib.util
from pathlib import Path
import struct
import unittest

spec = importlib.util.spec_from_file_location(
    'secure_env_package', Path(__file__).resolve().parents[1]/'tools/secure-env/package.py')
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)


def elf(interpreter=None, trampoline=True):
    data = bytearray(64 + 56)
    data[:7] = b'\x7fELF\x02\x01\x01'
    struct.pack_into('<HHIQQQ', data, 16, 3, 183, 1, 0x1000, 64, 0)
    struct.pack_into('<HHH', data, 52, 64, 56, 1)
    payload = interpreter + b'\0' if interpreter else b'relinterp: ' if trampoline else b'ordinary program'
    struct.pack_into('<IIQQQQQQ', data, 64, 3 if interpreter else 1, 5,
                     len(data), 0x1000, 0x1000, len(payload), len(payload), 1)
    return data + payload


class StartupLayoutTests(unittest.TestCase):
    def test_aosp_dynamic_binary_without_interp(self):
        self.assertEqual(package.validate_program(elf(), {'libc_musl.so'}),
                         'AOSP embedded relinterp')

    def test_standard_musl_interpreter(self):
        self.assertEqual(package.validate_program(elf(b'/lib/ld-musl-aarch64.so.1'),
                                                  {'libc_musl.so'}), 'musl PT_INTERP')

    def test_rejects_foreign_loader(self):
        with self.assertRaisesRegex(ValueError, 'Unexpected interpreter'):
            package.validate_program(elf(b'/lib/ld-linux-aarch64.so.1'), {'libc_musl.so'})

    def test_no_interp_alone_does_not_prove_supported_startup(self):
        for data, dependencies in [(elf(trampoline=False), {'libc_musl.so'}), (elf(), set())]:
            with self.subTest(dependencies=dependencies), self.assertRaises(ValueError):
                package.validate_program(data, dependencies)

    def test_rejects_truncated_headers_or_interpreter(self):
        for data in [elf()[:100], elf(b'/lib/ld-musl-aarch64.so.1')[:-1]]:
            with self.subTest(size=len(data)), self.assertRaises(ValueError):
                package.validate_program(data, {'libc_musl.so'})

    def test_rejects_wrong_architecture(self):
        data = elf()
        struct.pack_into('<H', data, 18, 62)
        with self.assertRaisesRegex(ValueError, 'AArch64'):
            package.validate_program(data, {'libc_musl.so'})


if __name__ == '__main__':
    unittest.main()
