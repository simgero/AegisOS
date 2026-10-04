"""Exercise the real driver's shell controls with inert terminal replies.

No VM connection, AOSP identity, password, native process or isolation claim.
The actual stopped-context service regression still requires a rebuilt guest.
"""
import ast
from pathlib import Path
import re
from types import SimpleNamespace
import unittest
from unittest.mock import Mock
import shlex


ROOT = Path(__file__).resolve().parents[1]
SESSION = ('user=11 serial=11 name=Runtime Test Beta admin=false enabled=true '
           'partial=false foreground=true running=true ce=unlocked '
           'runtime=managed;check-linux-status')
STOPPED = 'user=11 serial=11 runtime=stopped ce=unlocked packages=not-active'
REJECTED = ('linux shell\r\nAktion nicht bestätigt. Tatsächlichen '
            'Benutzer-/Speicherzustand prüfen.\r\naegis> ').encode()
# The prompt shape observed in the failed b832d6c regression (event 705).
UNEXPECTED_BASH = b'linux shell\r\n\x1b[?2004hruntime@aegis-runtime:~$ '


class ShellEntryTest(unittest.TestCase):
    def setUp(self):
        source = ROOT / 'scripts/qemu-runtime-test.py'
        tree = ast.parse(source.read_text(), filename=str(source))
        names = {'enter_shell', 'start_shell', 'check_stopped_shell'}
        functions = [node for node in tree.body
                     if isinstance(node, ast.FunctionDef) and node.name in names]
        self.assertEqual(names, {node.name for node in functions})
        self.replies = [REJECTED]
        self.actions = []
        self.clock = 0
        self.states = [STOPPED, STOPPED]
        self.sessions = [SESSION, SESSION]
        self.env = {
            're': re, 'shlex': shlex,
            'os': SimpleNamespace(write=Mock(), read=Mock(side_effect=self.read)),
            'select': SimpleNamespace(select=Mock(return_value=([123], [], []))),
            'time': SimpleNamespace(monotonic=self.monotonic),
            'master': 123, 'client': SimpleNamespace(poll=lambda: None),
            'pending': bytearray(), 'shell_active': False, 'held_login': None,
            'GNU_PROMPT': 'AEGIS_TEST_GNU> ', 'events': [],
            'record': self.record, 'clean': self.clean, 'action': self.action,
            'until': Mock(return_value=b'AEGIS_TEST_GNU> '),
        }
        # Avoid importing the executable: its top level attaches to a real VM.
        exec(compile(ast.Module(body=functions, type_ignores=[]), str(source), 'exec'),
             self.env)

    @staticmethod
    def clean(value):
        return bytes(value).decode().replace('\r', '')

    def record(self, label, output):
        if isinstance(output, (bytes, bytearray)):
            output = self.clean(output)
        self.env['events'].append({'action': label, 'output': output})

    def monotonic(self):
        self.clock += 1
        return self.clock

    def read(self, descriptor, size):
        self.assertEqual(123, descriptor)
        return self.replies.pop(0)

    def action(self, label, command):
        self.assertFalse(self.env['shell_active'], 'CLI command sent into GNU')
        self.actions.append(command)
        if command == 'status':
            value = self.sessions.pop(0)
        elif command == 'linux status':
            value = self.states.pop(0)
        else:
            self.fail('Unexpected command: ' + command)
        self.record(label, command + '\n' + value + '\naegis> ')

    def check(self):
        self.env['check_stopped_shell']()

    def test_rejection_preserves_stopped_runtime_and_authenticated_session(self):
        self.check()
        self.assertFalse(self.env['shell_active'])
        self.env['os'].write.assert_called_once_with(123, b'linux shell\n')
        self.assertEqual(['status', 'linux status', 'linux status', 'status'], self.actions)
        self.assertEqual('shell-stopped-rejected', self.env['events'][-1]['action'])

    def test_recorded_implicit_start_fails_immediately_and_retains_gnu_mode(self):
        self.replies = [UNEXPECTED_BASH]
        with self.assertRaisesRegex(AssertionError, 'unexpectedly entered'):
            self.check()
        self.assertTrue(self.env['shell_active'])
        self.assertEqual(['status', 'linux status'], self.actions)
        self.assertEqual('shell-entry', self.env['events'][-1]['action'])
        self.assertIn('runtime@aegis-runtime:~$', self.env['events'][-1]['output'])
        self.env['until'].assert_not_called()

    def test_fragmented_bash_prompt_and_regular_shell_initialization(self):
        self.replies = [UNEXPECTED_BASH[:25], UNEXPECTED_BASH[25:-1], b' ']
        self.env['start_shell']()
        self.assertTrue(self.env['shell_active'])
        self.assertEqual(bytearray(), self.env['pending'])
        self.assertEqual(['shell-entry', 'gnu-prompt'],
                         [event['action'] for event in self.env['events']])

    def test_regular_shell_rejection_does_not_send_bash_configuration_to_cli(self):
        with self.assertRaisesRegex(AssertionError, 'did not enter Bash'):
            self.env['start_shell']()
        self.assertFalse(self.env['shell_active'])
        self.env['os'].write.assert_called_once_with(123, b'linux shell\n')

    def test_missing_authentication_does_not_count_as_stopped_context_rejection(self):
        self.sessions = ['terminal=unauthenticated runtime=managed']
        with self.assertRaisesRegex(AssertionError, 'authenticated foreground'):
            self.check()
        self.env['os'].write.assert_not_called()

    def test_running_context_is_rejected_as_a_bad_test_precondition(self):
        self.states = [STOPPED.replace('runtime=stopped', 'runtime=ready')]
        with self.assertRaisesRegex(AssertionError, 'stopped own runtime'):
            self.check()
        self.env['os'].write.assert_not_called()

    def test_unrelated_cli_rejection_is_not_a_pass(self):
        self.replies = [b'linux shell\nAktion abgelehnt. In diesem Terminal erneut anmelden.\naegis> ']
        with self.assertRaisesRegex(AssertionError, 'runtime state rejection'):
            self.check()
        self.assertEqual(['status', 'linux status'], self.actions)

    def test_rejection_that_changes_runtime_state_is_not_a_pass(self):
        self.states[1] = STOPPED.replace('runtime=stopped', 'runtime=ready')
        with self.assertRaisesRegex(AssertionError, 'changed the runtime state'):
            self.check()

    def test_rejection_that_changes_session_is_not_a_pass(self):
        self.sessions[1] = SESSION.replace('ce=unlocked', 'ce=locked')
        with self.assertRaisesRegex(AssertionError, 'changed the AOSP session'):
            self.check()

    def test_closed_channel_is_not_a_rejection(self):
        self.replies = [b'']
        with self.assertRaisesRegex(AssertionError, 'Channel closed'):
            self.check()
        self.assertFalse(any(e['action'] == 'shell-stopped-rejected'
                             for e in self.env['events']))

    def test_no_prompt_has_a_bounded_wait(self):
        self.env['select'].select.return_value = ([], [], [])
        with self.assertRaisesRegex(AssertionError, 'No shell prompt observed'):
            self.env['enter_shell']()
        self.assertLessEqual(self.clock, 182)
        self.env['os'].read.assert_not_called()


if __name__ == '__main__':
    unittest.main()
