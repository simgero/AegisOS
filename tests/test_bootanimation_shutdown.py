import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('bootanimation_hook', ROOT / 'scripts/aosp/register-bootanimation.py')
hook = importlib.util.module_from_spec(spec)
spec.loader.exec_module(hook)
ORIGINAL = (ROOT / 'tests/fixtures/aosp_bootanimation_main.cpp').read_bytes()


class BootAnimationShutdownTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.path = self.root / hook.RELATIVE
        self.path.parent.mkdir(parents=True)
        self.path.write_bytes(ORIGINAL)

    def test_install_is_idempotent_and_verifies_exact_pinned_source(self):
        with self.assertRaises(ValueError):
            hook.install(self.root, ORIGINAL, verify=True)
        receipt = hook.install(self.root, ORIGINAL)
        self.assertEqual(receipt, hook.install(self.root, ORIGINAL))
        self.assertEqual(receipt, hook.install(self.root, ORIGINAL, verify=True))
        source = self.path.read_text()
        self.assertLess(source.index('joinThreadPool();'), source.index('boot->join();'))
        self.assertLess(source.index('boot->join();'), source.index('return 0;'))

    def test_unmanaged_edits_and_changed_baseline_are_preserved(self):
        edited = ORIGINAL + b'// unrelated change\n'
        self.path.write_bytes(edited)
        with self.assertRaises(ValueError):
            hook.install(self.root, ORIGINAL)
        self.assertEqual(edited, self.path.read_bytes())
        with self.assertRaises(ValueError):
            hook.patched(edited)

    def test_source_link_is_not_followed(self):
        target = self.root / 'outside.cpp'
        target.write_bytes(ORIGINAL)
        self.path.unlink()
        self.path.symlink_to(target)
        with self.assertRaises(ValueError):
            hook.install(self.root, ORIGINAL)
        self.assertEqual(ORIGINAL, target.read_bytes())
