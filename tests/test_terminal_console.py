"""Run the real TerminalConsole and JNI input code on a private host PTY.

Needs a JDK (AEGIS_TEST_JDK or javac on PATH) and a C++ compiler. The test copy
of TerminalNative changes only its fixed Android library path to a host test
library property. No production loader, Android account, Binder service, guest
or live credential is changed. Host checks supplement, not replace, T01's
Android integration and disclosure audit.
"""
import errno
import os
from pathlib import Path
import pty
import select
import shutil
import signal
import subprocess
import tempfile
import termios
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]
IDENTITY = ROOT / "packages/aegis/identity"
PROMPT = b"PUBLIC_FIXTURE_PASSWORD> "
FIXTURE = b"OnlyPublicFixture42"


class TerminalConsoleTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        configured = os.environ.get("AEGIS_TEST_JDK")
        javac = str(Path(configured) / "bin/javac") if configured else shutil.which("javac")
        if javac is None:
            raise unittest.SkipTest("JDK required; set AEGIS_TEST_JDK")
        jdk = Path(javac).resolve().parent.parent
        cls.java = str(jdk / "bin/java")
        cls.temp = tempfile.TemporaryDirectory(prefix="aegis-terminal-host-")
        cls.addClassCleanup(cls.temp.cleanup)
        cls.root = Path(cls.temp.name)
        cls.library = cls.root / "libaegis_terminal_test.so"
        subprocess.run([
            "c++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-shared", "-fPIC",
            "-I", str(jdk / "include"), "-I", str(jdk / "include/linux"),
            str(IDENTITY / "terminal/terminal_jni.cpp"), "-o", str(cls.library),
        ], check=True, capture_output=True, text=True, timeout=30)
        api = (IDENTITY / "terminal/java/org/aegisos/identity/TerminalNative.java").read_text()
        original = 'System.load("/system_ext/lib64/libaegis_terminal_jni.so")'
        if api.count(original) != 1:
            raise AssertionError("Production loader changed; review host test adaptation")
        (cls.root / "TerminalNative.java").write_text(api.replace(
            original, 'System.load(System.getProperty("aegis.test.terminal_library"))'))
        subprocess.run([
            javac, "-d", str(cls.root), str(cls.root / "TerminalNative.java"),
            str(IDENTITY / "cli/org/aegisos/identity/TerminalConsole.java"),
            str(ROOT / "tests/fixtures/TerminalConsoleHostProbe.java"),
        ], check=True, capture_output=True, text=True, timeout=30)

    def command(self, scenario):
        return [self.java, "-XX:-UsePerfData", "-cp", str(self.root),
                "-Daegis.test.terminal_library=" + str(self.library),
                "org.aegisos.identity.TerminalConsoleHostProbe", scenario]

    def start(self, scenario):
        self.master, self.slave = pty.openpty()
        self.addCleanup(os.close, self.master)
        self.addCleanup(os.close, self.slave)
        self.before = termios.tcgetattr(self.slave)
        # Fix the test precondition: password mode must actively remove both
        # echo flags, even if echoing newline was originally enabled.
        self.before[3] |= termios.ECHO | termios.ECHONL
        termios.tcsetattr(self.slave, termios.TCSANOW, self.before)
        self.before = termios.tcgetattr(self.slave)
        self.process = subprocess.Popen(self.command(scenario), cwd=self.root,
                                        stdin=self.slave, stdout=self.slave, stderr=self.slave,
                                        start_new_session=True)
        self.addCleanup(self.close_process, self.process)
        self.output = bytearray()
        self.until(PROMPT)
        current = termios.tcgetattr(self.slave)
        self.assertFalse(current[3] & (termios.ECHO | termios.ECHONL))
        self.assertTrue(current[3] & termios.ICANON)

    @staticmethod
    def close_process(process):
        if process.poll() is None:
            process.kill()  # Only this test's own Popen child.
        process.wait(timeout=5)

    def until(self, marker):
        deadline = time.monotonic() + 10
        while marker not in self.output:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                self.fail("Timed out waiting for test marker: " + repr(marker)
                          + "; child=" + str(self.process.poll())
                          + "; public transcript=" + repr(bytes(self.output)))
            ready, _, _ = select.select([self.master], [], [], min(remaining, .2))
            if ready:
                try:
                    chunk = os.read(self.master, 4096)
                except OSError as error:
                    if error.errno != errno.EIO:
                        raise
                    chunk = b""
                self.output.extend(chunk)

    def finish(self):
        self.until(b"COMMAND> ")
        self.assertEqual(termios.tcgetattr(self.slave), self.before)
        os.write(self.master, b"AFTER\n")
        self.until(b"DONE\r\n")
        self.assertEqual(self.process.wait(timeout=5), 0)
        self.assertEqual(termios.tcgetattr(self.slave), self.before)

    def test_password_is_not_echoed_and_following_command_is_preserved(self):
        self.start("normal")
        os.write(self.master, FIXTURE + b"\n")
        self.finish()
        self.assertIn(b"PASSWORD_RETURNED", self.output)
        self.assertNotIn(FIXTURE, self.output)

    def test_maximum_input_is_accepted_without_echo(self):
        self.start("maximum")
        os.write(self.master, b"x" * 128 + b"\n")
        self.finish()
        self.assertIn(b"PASSWORD_RETURNED", self.output)
        self.assertNotIn(b"xxxx", self.output)

    def test_overflow_is_drained_without_echo_or_next_command_contamination(self):
        self.start("overflow")
        os.write(self.master, FIXTURE * 10 + b"\n")
        self.finish()
        self.assertIn(b"OVERFLOW_REFUSED", self.output)
        self.assertNotIn(FIXTURE, self.output)

    def test_eof_restores_terminal_and_preserves_next_command(self):
        self.start("eof")
        os.write(self.master, self.before[6][termios.VEOF])
        self.finish()
        self.assertIn(b"PASSWORD_RETURNED", self.output)

    def test_interruption_restores_terminal(self):
        for number in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP, signal.SIGQUIT,
                       signal.SIGTSTP):
            with self.subTest(signal=number):
                self.start("interrupt")
                self.process.send_signal(number)
                self.assertEqual(self.process.wait(timeout=5), 128 + number)
                self.assertEqual(termios.tcgetattr(self.slave), self.before)

    def test_interruption_discards_unsubmitted_password_input(self):
        for number in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP, signal.SIGQUIT,
                       signal.SIGTSTP):
            with self.subTest(signal=number):
                self.start("interrupt")
                os.write(self.master, FIXTURE)
                # Allow the PTY line discipline to receive the incomplete canonical
                # line. It cannot reach the Java reader without a delimiter.
                time.sleep(.05)
                self.process.send_signal(number)
                self.assertEqual(self.process.wait(timeout=5), 128 + number)
                self.assertEqual(termios.tcgetattr(self.slave), self.before)
                os.write(self.master, b"AFTER\n")
                ready, _, _ = select.select([self.slave], [], [], 5)
                self.assertTrue(ready, "Next terminal reader did not receive a command")
                self.assertEqual(os.read(self.slave, 4096), b"AFTER\n")

    def test_pipe_is_not_accepted_for_password_input(self):
        result = subprocess.run(self.command("pipe"), input=b"", capture_output=True,
                                check=True, timeout=10, cwd=self.root)
        self.assertEqual(result.stdout, b"PIPE_REFUSED\n")
        self.assertEqual(result.stderr, b"")
