"""Recorded runtime evidence must survive later mutations of driver state."""
import ast
import contextlib
import io
import json
from pathlib import Path
import tempfile
import time
import unittest


class RuntimeEventSnapshot(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        source = Path(__file__).resolve().parents[1] / 'scripts/qemu-runtime-test.py'
        tree = ast.parse(source.read_text(), filename=str(source))
        # Importing the interactive executable would connect to a real VM and
        # create credentials. Exercise its actual recording functions only.
        functions = [node for node in tree.body
                     if isinstance(node, ast.FunctionDef) and node.name in ('clean', 'record')]
        self.assertEqual({'clean', 'record'}, {node.name for node in functions})
        self.namespace = {'json': json, 'time': time, 'OUT': self.root,
                          'events': [], 'credentials': {'test': bytearray(b'test-password-only')}}
        exec(compile(ast.Module(body=functions, type_ignores=[]), str(source), 'exec'),
             self.namespace)
        self.stdout = io.StringIO()

    def record(self, action, value):
        with contextlib.redirect_stdout(self.stdout):
            self.namespace['record'](action, value)

    def test_later_user_creation_cannot_rewrite_an_earlier_checkpoint(self):
        users = {'alpha': [10, 10], 'beta': [11, 11]}
        state = {'identities': users, 'observations': [{'running': [10, 11]}]}
        self.record('reboot-checkpoint', state)
        original = json.loads((self.root / 'events.json').read_text())[0]

        users['gamma'] = [12, 12]
        users['beta'][1] = 20
        state['observations'][0]['running'].append(12)
        self.record('later-checkpoint', state)

        stored = json.loads((self.root / 'events.json').read_text())
        printed = [json.loads(line) for line in self.stdout.getvalue().splitlines()]
        self.assertEqual(original, stored[0])
        self.assertEqual(original, printed[0])
        self.assertEqual(original, self.namespace['events'][0])
        self.assertEqual(state, stored[1]['output'])
        self.assertEqual(stored, printed)

    def test_terminal_bytes_are_still_cleaned(self):
        self.record('terminal', b'accepted\r\n')
        self.assertEqual('accepted\n', self.namespace['events'][0]['output'])

    def test_echoed_credential_is_not_recorded(self):
        with self.assertRaisesRegex(RuntimeError, 'Credential echo detected'):
            self.record('terminal', b'test-password-only\n')
        self.assertEqual([], self.namespace['events'])
        self.assertFalse((self.root / 'events.json').exists())
        self.assertEqual('', self.stdout.getvalue())


if __name__ == '__main__':
    unittest.main()
