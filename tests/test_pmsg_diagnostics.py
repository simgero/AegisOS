"""Compile the pinned file reconstruction parser with inert read/callback fakes.

The actual PmsgRead device reader is replaced by synthetic records and terminal
errors. Tests never open pstore, write pmsg, or run any guest command.
"""
import errno
import hashlib
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('pmsg_hook', ROOT / 'scripts/aosp/register-pmsg-diagnostics.py')
HOOK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(HOOK)
ORIGINAL = (ROOT / 'tests/fixtures/aosp_pmsg_reader.cpp').read_bytes()
RECORD_BYTES = len(b'synthetic-pmsg-only')
FAKES = r'''
#include <cerrno>
#include <cctype>
#include <climits>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include "list.h"
enum log_id_t { LOG_ID_MAIN, LOG_ID_RADIO, LOG_ID_EVENTS, LOG_ID_SYSTEM,
                LOG_ID_CRASH, LOG_ID_KERNEL, LOG_ID_SECURITY };
constexpr int ANDROID_LOG_INFO = 4, ANDROID_LOG_ANY = 0;
constexpr int ANDROID_LOG_PSTORE = 1, ANDROID_LOG_NONBLOCK = 2;
constexpr unsigned ANDROID_LOG_PMSG_FILE_SEQUENCE = 1000;
constexpr unsigned ANDROID_LOG_PMSG_FILE_MAX_SEQUENCE = 1000;
struct logger_entry {
  uint16_t len, hdr_size;
  int32_t pid, tid;
  uint32_t sec, nsec, lid, uid;
};
union log_msg { logger_entry entry; char buf[5120]; };
struct logger_list { int mode; unsigned log_mask; };
using __android_log_pmsg_file_read_fn = ssize_t (*)(log_id_t, char, const char*,
                                                   const char*, size_t, void*);
static int terminal_result, callback_result, record_count, closed;
static bool emit_record, wrong_prefix;
int PmsgRead(logger_list*, log_msg* message) {
  if (!emit_record || record_count++) return terminal_result;
  std::memset(message, 0, sizeof(*message));
  message->entry.hdr_size = sizeof(message->entry);
  message->entry.lid = LOG_ID_SYSTEM;
  char* payload = message->buf + sizeof(message->entry);
  *payload++ = ANDROID_LOG_INFO;
  const char* tag = wrong_prefix ? "other:log" : "recovery:last_log";
  std::memcpy(payload, tag, std::strlen(tag) + 1);
  payload += std::strlen(tag) + 1;
  const char contents[] = "synthetic-pmsg-only";
  std::memcpy(payload, contents, sizeof(contents) - 1);
  message->entry.len = 1 + std::strlen(tag) + 1 + sizeof(contents) - 1;
  return sizeof(message->entry) + message->entry.len;
}
void PmsgClose(logger_list*) { ++closed; }
int __android_log_print(int, const char* tag, const char* format, ...) {
  std::fprintf(stderr, "%s: ", tag);
  va_list arguments;
  va_start(arguments, format);
  int result = std::vfprintf(stderr, format, arguments);
  va_end(arguments);
  std::fputc('\n', stderr);
  return result;
}
ssize_t callback(log_id_t, char, const char*, const char* contents, size_t size, void*) {
  const char expected[] = "synthetic-pmsg-only";
  if (size != sizeof(expected)-1 || std::memcmp(contents, expected, size)) std::abort();
  std::printf("CALLBACK bytes=%zu\n", size);
  return callback_result < 0 ? callback_result : static_cast<ssize_t>(size);
}
'''
MAIN = r'''
int main(int argc, char** argv) {
  if (argc != 5) return 2;
  terminal_result = std::atoi(argv[1]);
  emit_record = std::atoi(argv[2]);
  callback_result = std::atoi(argv[3]);
  wrong_prefix = std::atoi(argv[4]);
  ssize_t result = __android_log_pmsg_file_read(LOG_ID_SYSTEM, ANDROID_LOG_INFO,
                                              "recovery/", callback, nullptr);
  std::printf("RESULT=%zd CLOSED=%d\n", result, closed);
}
'''


class PmsgExecutionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix='aegis-pmsg-host-')
        cls.addClassCleanup(cls.temp.cleanup)
        cls.root = Path(cls.temp.name)
        (cls.root / 'list.h').write_bytes((ROOT / 'tests/fixtures/aosp_cutils_list.h').read_bytes())
        for name, source in (('original', ORIGINAL), ('diagnostic', HOOK.patched(ORIGINAL))):
            # Compile the entire reconstruction/aggregation function verbatim.
            # Only its lower-level device reader and output sink are substituted.
            parser = source[source.index(b'static void* realloc_or_free'):].decode()
            path = cls.root / (name + '.cpp')
            path.write_text(FAKES + parser + MAIN)
            subprocess.run(['c++', '-std=gnu++20', '-Wall', '-Wextra', '-Werror',
                            str(path), '-o', str(cls.root / name)],
                           check=True, capture_output=True, text=True, timeout=30)

    def run_parser(self, terminal, record=False, callback=0, wrong_prefix=False, version='diagnostic'):
        return subprocess.run([str(self.root / version), str(terminal), str(int(record)),
                               str(callback), str(int(wrong_prefix))],
                              check=True, capture_output=True, text=True, timeout=5)

    def test_empty_missing_denied_and_damaged_sources_are_distinguishable(self):
        for terminal in (-errno.EAGAIN, -errno.ENOENT, -errno.EACCES, -errno.EIO, -errno.EBADF):
            with self.subTest(terminal=terminal):
                original = self.run_parser(terminal, version='original')
                actual = self.run_parser(terminal)
                self.assertEqual(actual.stdout, original.stdout)
                self.assertIn('RESULT=-2 CLOSED=1', actual.stdout)
                self.assertIn(f'terminal={terminal} ', actual.stderr)
                self.assertIn('result=-2', actual.stderr)

    def test_valid_records_and_callback_failures_keep_existing_results(self):
        for terminal in (-errno.EAGAIN, -errno.EIO):
            for callback in (0, -errno.ENOSPC, -errno.EACCES):
                with self.subTest(terminal=terminal, callback=callback):
                    original = self.run_parser(terminal, True, callback, version='original')
                    actual = self.run_parser(terminal, True, callback)
                    self.assertEqual(actual.stdout, original.stdout)
                    self.assertIn(f'CALLBACK bytes={RECORD_BYTES}', actual.stdout)
                    self.assertIn(f'RESULT={RECORD_BYTES if callback == 0 else callback} CLOSED=1', actual.stdout)
                    self.assertIn(f'terminal={terminal} ', actual.stderr)

    def test_filtered_records_are_distinct_from_read_failure(self):
        actual = self.run_parser(-errno.EAGAIN, True, wrong_prefix=True)
        self.assertNotIn('CALLBACK', actual.stdout)
        self.assertIn('RESULT=-2 CLOSED=1', actual.stdout)
        self.assertIn(f'terminal={-errno.EAGAIN} ', actual.stderr)

    def test_diagnostics_never_include_filenames_or_record_contents(self):
        for terminal in (-errno.EAGAIN, -errno.EIO):
            actual = self.run_parser(terminal, True)
            self.assertRegex(actual.stderr, rf'^liblog-pmsg: pmsg file read: terminal=-\d+ aggregate={RECORD_BYTES} result={RECORD_BYTES}\n$')
            for value in ('synthetic-pmsg-only', 'recovery', 'last_log'):
                self.assertNotIn(value, actual.stderr)


class PmsgRegistrationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.path = self.root / HOOK.RELATIVE
        self.path.parent.mkdir(parents=True)
        self.path.write_bytes(ORIGINAL)

    def test_pinned_source_idempotence_and_verify(self):
        self.assertEqual(hashlib.sha256(ORIGINAL).hexdigest(), HOOK.ORIGINAL_SHA256)
        with self.assertRaises(ValueError):
            HOOK.install(self.root, ORIGINAL, verify=True)
        receipt = HOOK.install(self.root, ORIGINAL)
        self.assertEqual(receipt, HOOK.install(self.root, ORIGINAL))
        self.assertEqual(receipt, HOOK.install(self.root, ORIGINAL, verify=True))

    def test_unmanaged_and_unrecognized_sources_preserved(self):
        edited = ORIGINAL + b'// local work\n'
        self.path.write_bytes(edited)
        with self.assertRaises(ValueError):
            HOOK.install(self.root, ORIGINAL)
        self.assertEqual(self.path.read_bytes(), edited)
        with self.assertRaises(ValueError):
            HOOK.patched(edited)

    def test_linked_source_and_existing_temporary_are_preserved(self):
        target = self.root / 'original.cpp'
        target.write_bytes(ORIGINAL)
        self.path.unlink()
        self.path.symlink_to(target)
        with self.assertRaises(ValueError):
            HOOK.install(self.root, ORIGINAL)
        self.assertEqual(target.read_bytes(), ORIGINAL)
        self.path.unlink()
        self.path.write_bytes(ORIGINAL)
        temporary = self.path.with_suffix('.aegis-tmp')
        temporary.write_text('existing work')
        with self.assertRaises(FileExistsError):
            HOOK.install(self.root, ORIGINAL)
        self.assertEqual(self.path.read_bytes(), ORIGINAL)
        self.assertEqual(temporary.read_text(), 'existing work')


if __name__ == '__main__':
    unittest.main()
