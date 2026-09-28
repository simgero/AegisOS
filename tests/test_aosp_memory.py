import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('aosp_memory', ROOT / 'scripts/aosp/check-memory.py')
memory = importlib.util.module_from_spec(spec)
spec.loader.exec_module(memory)


class BuildMemoryTests(unittest.TestCase):
    @staticmethod
    def sample(total, available, swap=0):
        return f'MemTotal: {total} kB\nMemAvailable: {available} kB\nSwapFree: {swap} kB\n'

    def test_known_sufficient_memory_and_exact_boundaries(self):
        for total, available in ((96 * 1048576, 90 * 1048576),
                                 (memory.MIN_TOTAL_KIB, memory.MIN_AVAILABLE_KIB)):
            self.assertIn('preflight OK', memory.check(self.sample(total, available)))

    def test_observed_small_vm_is_rejected_even_with_large_swap(self):
        with self.assertRaisesRegex(RuntimeError, 'Swap is not counted'):
            memory.check(self.sample(int(14.7 * 1048576), 14 * 1048576, 96 * 1048576))

    def test_each_insufficient_resource_is_rejected_independently(self):
        for total, available in ((memory.MIN_TOTAL_KIB - 1, memory.MIN_AVAILABLE_KIB),
                                 (96 * 1048576, memory.MIN_AVAILABLE_KIB - 1)):
            with self.subTest(total=total, available=available):
                with self.assertRaises(RuntimeError): memory.check(self.sample(total, available))

    def test_missing_duplicate_negative_and_wrong_unit_measurements_fail_closed(self):
        for invalid in ('MemTotal: 100000000 kB\n',
                        'MemTotal: 100000000 kB\nMemAvailable: 90000000 kB\nMemAvailable: 90000000 kB\n',
                        'MemTotal: -1 kB\nMemAvailable: 0 kB\n',
                        'MemTotal: 100000000 MB\nMemAvailable: 90000000 kB\n'):
            with self.subTest(invalid=invalid):
                with self.assertRaises(ValueError): memory.check(invalid)

    def test_inconsistent_measurements_fail_closed(self):
        for total, available in ((0, 0), (60000000, 70000000)):
            with self.assertRaises(ValueError): memory.check(self.sample(total, available))


if __name__ == '__main__':
    unittest.main()
