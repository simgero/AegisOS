"""Compile the pinned AOSP program with inert host fakes; no device or property access."""
import hashlib
import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('misctrl_hook', ROOT / 'scripts/aosp/register-misctrl.py')
hook = importlib.util.module_from_spec(spec)
spec.loader.exec_module(hook)
ORIGINAL = (ROOT / 'tests/fixtures/aosp_misctrl_main.cpp').read_bytes()

# These fakes only return selected outcomes and print observations. They never
# open a misc device, change a system property, or contact Android.
FAKES = r'''
#pragma once
#include <cstdlib>
#include <iostream>
#include <string>
inline int fixture(const char* name, int fallback) {
  const char* value = std::getenv(name);
  return value ? std::atoi(value) : fallback;
}
inline int FakePageSize() { return fixture("MISCTRL_TEST_PAGE_SIZE", 4096); }
#define getpagesize FakePageSize
#define LOG(level) std::cerr
namespace android::base {
struct LogdLogger {};
inline void StderrLogger() {}
inline int TeeLogger(LogdLogger, void (*)()) { return 0; }
inline void InitLogging(char**, int) {}
inline bool SetProperty(const std::string& name, const std::string& value) {
  std::cout << "property=" << name << ":" << value << "\n";
  return fixture("MISCTRL_TEST_PROPERTY_OK", 1);
}
}
constexpr unsigned MISC_CONTROL_MAGIC_HEADER = 123;
constexpr unsigned MISC_CONTROL_MESSAGE_VERSION = 1;
constexpr unsigned MISC_CONTROL_16KB_BEFORE = 1;
struct misc_control_message { unsigned version, magic, misctrl_flags; };
inline bool ReadMiscControlMessage(misc_control_message* m, std::string* err) {
  if (!fixture("MISCTRL_TEST_READ_OK", 1)) { *err = "read fixture failure"; return false; }
  *m = {MISC_CONTROL_MESSAGE_VERSION,
        fixture("MISCTRL_TEST_VALID_HEADER", 1) ? MISC_CONTROL_MAGIC_HEADER : 0,
        static_cast<unsigned>(fixture("MISCTRL_TEST_OLD_FLAGS", 0))};
  return true;
}
inline bool WriteMiscControlMessage(const misc_control_message& m, std::string* err) {
  std::cout << "write_flags=" << m.misctrl_flags << "\n";
  if (!fixture("MISCTRL_TEST_WRITE_OK", 1)) { *err = "write fixture failure"; return false; }
  return true;
}
inline bool CheckReservedSystemSpaceEmpty(bool* empty, std::string* err) {
  std::cout << "reserved_checked\n";
  if (!fixture("MISCTRL_TEST_RESERVED_READ_OK", 1)) {
    *err = "reserved read fixture failure"; return false;
  }
  *empty = fixture("MISCTRL_TEST_RESERVED_EMPTY", 1);
  return true;
}
'''


class MisctrlStatusTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix='aegis-misctrl-host-')
        cls.addClassCleanup(cls.temp.cleanup)
        cls.root = Path(cls.temp.name)
        (cls.root / 'fakes.h').write_text(FAKES)
        for name in ('android-base/logging.h', 'android-base/properties.h',
                     'bootloader_message/bootloader_message.h', 'log/log.h'):
            path = cls.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text('#include "fakes.h"\n')
        for name, source in (('original', ORIGINAL), ('fixed', hook.patched(ORIGINAL))):
            path = cls.root / (name + '.cpp')
            path.write_bytes(source)
            subprocess.run(['c++', '-std=c++20', '-Wall', '-Wextra', '-Werror',
                            '-I', str(cls.root), str(path), '-o', str(cls.root / name)],
                           check=True, capture_output=True, text=True, timeout=30)

    def run_program(self, version='fixed', **outcomes):
        env = {key: value for key, value in os.environ.items()
               if not key.startswith('MISCTRL_TEST_')}
        env.update({'MISCTRL_TEST_' + key: str(value) for key, value in outcomes.items()})
        return subprocess.run([str(self.root / version)], env=env,
                              capture_output=True, text=True, timeout=5)

    def test_actual_upstream_inverts_property_outcome_and_fix_corrects_both(self):
        self.assertEqual(self.run_program('original').returncode, 1)
        self.assertEqual(self.run_program('original', PROPERTY_OK=0).returncode, 0)
        self.assertEqual(self.run_program().returncode, 0)
        failed = self.run_program(PROPERTY_OK=0)
        self.assertEqual(failed.returncode, 1)
        self.assertIn('Could not set ro.misctrl.16kb_before', failed.stderr)
        self.assertIn('write_flags=0', failed.stdout)
        self.assertIn('reserved_checked', failed.stdout)

    def test_real_failure_paths_remain_nonzero(self):
        for condition in ({'READ_OK': 0}, {'WRITE_OK': 0}, {'PAGE_SIZE': 8192},
                          {'RESERVED_READ_OK': 0}, {'RESERVED_EMPTY': 0}):
            for property_ok in (0, 1):
                with self.subTest(condition=condition, property_ok=property_ok):
                    result = self.run_program(PROPERTY_OK=property_ok, **condition)
                    self.assertEqual(result.returncode, 1)
                    self.assertIn('reserved_checked', result.stdout)

    def test_failed_read_does_not_publish_or_write_uninitialized_message(self):
        result = self.run_program(READ_OK=0)
        self.assertEqual(result.returncode, 1)
        self.assertNotIn('property=', result.stdout)
        self.assertNotIn('write_flags=', result.stdout)

    def test_page_size_and_historical_flags_are_preserved(self):
        for page, old, expected in ((4096, 0, 0), (16384, 0, 1), (4096, 1, 1)):
            with self.subTest(page=page, old=old):
                result = self.run_program(PAGE_SIZE=page, OLD_FLAGS=old)
                self.assertEqual(result.returncode, 0)
                self.assertIn(f'property=ro.misctrl.16kb_before:{expected}', result.stdout)
                self.assertIn(f'write_flags={expected}', result.stdout)

    def test_invalid_header_still_resets_flags(self):
        result = self.run_program(VALID_HEADER=0, OLD_FLAGS=1)
        self.assertEqual(result.returncode, 0)
        self.assertIn('misctrl message invalid, resetting it', result.stderr)
        self.assertIn('write_flags=0', result.stdout)


class MisctrlRegistrationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.path = self.root / hook.RELATIVE
        self.path.parent.mkdir(parents=True)
        self.path.write_bytes(ORIGINAL)

    def test_exact_original_idempotence_and_verify(self):
        self.assertEqual(hashlib.sha256(ORIGINAL).hexdigest(), hook.ORIGINAL_SHA256)
        with self.assertRaises(ValueError):
            hook.install(self.root, ORIGINAL, verify=True)
        receipt = hook.install(self.root, ORIGINAL)
        self.assertEqual(receipt, hook.install(self.root, ORIGINAL))
        self.assertEqual(receipt, hook.install(self.root, ORIGINAL, verify=True))

    def test_unmanaged_source_and_unknown_baseline_are_preserved(self):
        edited = ORIGINAL + b'// local change\n'
        self.path.write_bytes(edited)
        with self.assertRaises(ValueError):
            hook.install(self.root, ORIGINAL)
        self.assertEqual(self.path.read_bytes(), edited)
        with self.assertRaises(ValueError):
            hook.patched(edited)

    def test_linked_source_is_not_modified(self):
        target = self.root / 'outside.cpp'
        target.write_bytes(ORIGINAL)
        self.path.unlink()
        self.path.symlink_to(target)
        with self.assertRaises(ValueError):
            hook.install(self.root, ORIGINAL)
        self.assertEqual(target.read_bytes(), ORIGINAL)

    def test_existing_temporary_file_is_preserved(self):
        temporary = self.path.with_suffix('.aegis-tmp')
        temporary.write_text('existing work')
        with self.assertRaises(FileExistsError):
            hook.install(self.root, ORIGINAL)
        self.assertEqual(self.path.read_bytes(), ORIGINAL)
        self.assertEqual(temporary.read_text(), 'existing work')
