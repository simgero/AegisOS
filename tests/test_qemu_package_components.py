"""A corrected test binary must not disguise changed product sources."""
import copy
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("package_components",
        Path(__file__).resolve().parents[1] / "scripts/qemu-package-component-tests.py")
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


class TestOnlySources(unittest.TestCase):
    def setUp(self):
        self.base = {"version": 1, "files": {
            "Android.bp": "a" * 64,
            "runtime/package_preparer.cpp": "b" * 64,
            "runtime/package_planner_test_cases.inc": "f" * 64,
            "runtime/package_executor_tests.cpp": "c" * 64}}
        self.corrected = copy.deepcopy(self.base)
        self.corrected["files"]["runtime/package_executor_tests.cpp"] = "d" * 64

    def test_accepts_only_the_named_test_correction(self):
        self.assertEqual(["runtime/package_executor_tests.cpp"],
                         runner.test_only_sources(self.base, self.corrected))

    def test_accepts_planner_test_extension_without_product_changes(self):
        changed = copy.deepcopy(self.base)
        changed["files"]["runtime/package_planner_test_cases.inc"] = "e" * 64
        self.assertEqual(["runtime/package_planner_test_cases.inc"],
                         runner.test_only_sources(self.base, changed))

    def test_accepts_both_named_test_changes(self):
        self.corrected["files"]["runtime/package_planner_test_cases.inc"] = "e" * 64
        self.assertEqual(["runtime/package_executor_tests.cpp", "runtime/package_planner_test_cases.inc"],
                         runner.test_only_sources(self.base, self.corrected))

    def test_identical_sources_are_not_a_test_change(self):
        with self.assertRaises(ValueError):
            runner.test_only_sources(self.base, copy.deepcopy(self.base))

    def test_planner_test_does_not_authorize_product_change(self):
        changed = copy.deepcopy(self.base)
        changed["files"]["runtime/package_planner_test_cases.inc"] = "e" * 64
        changed["files"]["runtime/package_preparer.cpp"] = "e" * 64
        with self.assertRaises(ValueError):
            runner.test_only_sources(self.base, changed)

    def test_product_change_is_rejected_even_alongside_test_correction(self):
        self.corrected["files"]["runtime/package_preparer.cpp"] = "e" * 64
        with self.assertRaises(ValueError):
            runner.test_only_sources(self.base, self.corrected)

    def test_build_definition_change_is_rejected(self):
        self.corrected["files"]["Android.bp"] = "e" * 64
        with self.assertRaises(ValueError):
            runner.test_only_sources(self.base, self.corrected)

    def test_added_or_missing_source_cannot_escape_comparison(self):
        for added in (True, False):
            with self.subTest(added=added):
                changed = copy.deepcopy(self.corrected)
                if added:
                    changed["files"]["runtime/new.cpp"] = "e" * 64
                else:
                    del changed["files"]["runtime/package_preparer.cpp"]
                with self.assertRaises(ValueError):
                    runner.test_only_sources(self.base, changed)

    def test_unknown_receipt_schema_is_rejected(self):
        self.corrected["version"] = 2
        with self.assertRaises(ValueError):
            runner.test_only_sources(self.base, self.corrected)


if __name__ == "__main__":
    unittest.main()
