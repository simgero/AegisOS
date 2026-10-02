import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('egl_cache_hook', ROOT / 'scripts/aosp/register-egl-cache.py')
hook = importlib.util.module_from_spec(spec)
spec.loader.exec_module(hook)
ORIGINALS = {name: (ROOT / 'tests/fixtures' / ('aosp_' + name)).read_bytes()
             for name in hook.ORIGINAL_SHA256}


class EglCacheLifetimeTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for name, data in ORIGINALS.items():
            path = self.root / hook.PREFIX / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)

    def test_install_and_verify_bind_both_pinned_sources(self):
        with self.assertRaises(ValueError):
            hook.install(self.root, ORIGINALS, verify=True)
        receipt = hook.install(self.root, ORIGINALS)
        self.assertEqual(receipt, hook.install(self.root, ORIGINALS))
        self.assertEqual(receipt, hook.install(self.root, ORIGINALS, verify=True))
        self.assertEqual(set(receipt['files']), {hook.PREFIX + name for name in ORIGINALS})
        cpp = (self.root / hook.PREFIX / 'egl_cache.cpp').read_text()
        self.assertNotIn('egl_cache_t egl_cache_t::sCache;', cpp)
        self.assertIn('mSavePending(false)', cpp)  # Heap storage has no static zero initialization.
        self.assertIn('static egl_cache_t* const cache = new egl_cache_t;', cpp)
        self.assertIn('mBlobCache->writeToFile();', cpp)

    def test_unmanaged_second_file_does_not_partially_patch_first(self):
        header = self.root / hook.PREFIX / 'egl_cache.h'
        edited = ORIGINALS['egl_cache.h'] + b'// unrelated change\n'
        header.write_bytes(edited)
        with self.assertRaises(ValueError):
            hook.install(self.root, ORIGINALS)
        self.assertEqual(ORIGINALS['egl_cache.cpp'], (self.root / hook.PREFIX / 'egl_cache.cpp').read_bytes())
        self.assertEqual(edited, header.read_bytes())

    def test_changed_baseline_and_incomplete_input_are_rejected(self):
        with self.assertRaises(ValueError):
            hook.patched('egl_cache.cpp', ORIGINALS['egl_cache.cpp'] + b'\n')
        with self.assertRaises(ValueError):
            hook.install(self.root, {'egl_cache.cpp': ORIGINALS['egl_cache.cpp']})
        self.assertEqual(ORIGINALS['egl_cache.cpp'], (self.root / hook.PREFIX / 'egl_cache.cpp').read_bytes())

    def test_link_is_not_followed(self):
        header = self.root / hook.PREFIX / 'egl_cache.h'
        outside = self.root / 'outside.h'
        outside.write_bytes(ORIGINALS['egl_cache.h'])
        header.unlink()
        header.symlink_to(outside)
        with self.assertRaises(ValueError):
            hook.install(self.root, ORIGINALS)
        self.assertEqual(ORIGINALS['egl_cache.h'], outside.read_bytes())
