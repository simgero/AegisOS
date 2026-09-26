"""Exercise the real worker state machine with tiny local build/GitHub fixtures."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[1]
MOCK = r'''
import os, pathlib, shutil, sys
name = pathlib.Path(sys.argv[0]).name
args = sys.argv[1:]
root = pathlib.Path(os.environ['FIXTURE_ROOT'])
mode = os.environ.get('FAIL_MODE', '')
if name == 'git':
    if 'init' in args:
        (pathlib.Path(args[args.index('-C')+1])/'.git').mkdir(exist_ok=True)
elif name == 'python3':
    if 'manifest' in args:
        pathlib.Path(args[args.index('-o')+1]).write_text('<manifest/>')
elif name == 'env':
    # Intercept only the real compilation boundary, leaving the state machine intact.
    if mode == 'build': sys.exit(7)
    product = root/'product'
    product.mkdir()
    (product/'system.img').write_bytes(b'system image')
    (product/'kernel-ranchu').write_bytes(b'kernel')
    pathlib.Path(args[-1], 'product-out.txt').write_text(str(product))
elif name == 'gh':
    store = root/'remote'
    store.mkdir(exist_ok=True)
    action = args[1]
    if action == 'create': (store/'draft').touch()
    elif action == 'upload':
        if mode == 'upload': sys.exit(8)
        for value in args[3:]:
            if pathlib.Path(value).is_file(): shutil.copyfile(value, store/pathlib.Path(value).name)
    elif action == 'download':
        filename = args[args.index('--pattern')+1]
        target = pathlib.Path(args[args.index('--dir')+1])/filename
        shutil.copyfile(store/filename, target)
        if mode == 'corrupt': target.write_bytes(b'corrupted')
    elif action == 'edit':
        if mode == 'publish': sys.exit(9)
        (store/'published').touch()
elif name == 'dpkg-query': print('fixture package versions')
elif name == 'flock': pass
elif name == 'sleep': pass
elif name == 'timeout':
    os.execvp(args[1], args[1:])
'''


class WorkerTests(unittest.TestCase):
    def exercise(self, mode=''):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            scripts = root/'scripts'
            scripts.mkdir()
            for filename in ['worker.sh', 'compile.sh', 'config.sh']:
                text = (REPO/'scripts/aosp'/filename).read_text()
                (scripts/filename).write_text(text.replace('/srv/aegis', str(root/'server')))
            binaries = root/'bin'
            binaries.mkdir()
            for name in ['git', 'python3', 'env', 'gh', 'dpkg-query', 'flock', 'sleep', 'timeout']:
                path = binaries/name
                path.write_text(f'#!{sys.executable}\n'+MOCK)
                path.chmod(0o755)
            credentials = root/'credentials'
            credentials.mkdir()
            (credentials/'github-token').write_text('test-only-token')
            env = dict(os.environ, PATH=str(binaries)+':'+os.environ['PATH'],
                       FIXTURE_ROOT=str(root), FAIL_MODE=mode,
                       CREDENTIALS_DIRECTORY=str(credentials), AEGIS_SCRIPT_COMMIT='a'*40)
            result = subprocess.run(['bash', str(scripts/'worker.sh')], env=env,
                                    capture_output=True, text=True, timeout=30)
            runs = list((root/'server/runs').iterdir())
            self.assertEqual(len(runs), 1)
            self.assertTrue((runs[0]/'status').exists(), result.stdout+result.stderr)
            status = (runs[0]/'status').read_text().strip()
            self.assertNotIn('test-only-token', result.stdout+result.stderr)
            if mode:
                self.assertNotEqual(result.returncode, 0, result.stdout+result.stderr)
                self.assertEqual(status, 'FAILED')
                self.assertFalse((root/'remote/published').exists())
                self.assertNotIn('SAFE_TO_DELETE:', result.stdout)
            else:
                self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
                self.assertEqual(status, 'SAFE_TO_DELETE')
                self.assertTrue((root/'remote/published').exists())
                self.assertTrue((root/'remote/manifest.xml').exists())
                self.assertTrue(list((root/'remote').glob('images.tar.xz.part-*')))

    def test_verified_success(self): self.exercise()
    def test_failed_build(self): self.exercise('build')
    def test_failed_upload(self): self.exercise('upload')
    def test_corrupt_remote_asset(self): self.exercise('corrupt')
    def test_failed_publication(self): self.exercise('publish')


class BootstrapTests(unittest.TestCase):
    def test_pipe_help(self):
        result = subprocess.run(['sh', '-s', '--', '--help'], input=(REPO/'build.sh').read_text(),
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0)
        self.assertIn('GH_TOKEN', result.stdout)

    def test_storage_threshold(self):
        with tempfile.TemporaryDirectory() as folder:
            df = Path(folder)/'df'
            df.write_text('#!/bin/sh\nprintf "Filesystem 1024-blocks Used Available Capacity Mounted on\\n/dev/test 600000000 0 %s 0%% /srv/aegis\\n" "$TEST_FREE_KIB"\n')
            df.chmod(0o755)
            for available, expected in [(471*1048576, 0), (450*1048576, 0), (450*1048576-1, 1), (191*1048576, 1)]:
                with self.subTest(available=available):
                    env = dict(os.environ, PATH=folder+':'+os.environ['PATH'], TEST_FREE_KIB=str(available))
                    result = subprocess.run(['sh', str(REPO/'build.sh'), '--check-storage'], env=env,
                                            capture_output=True, text=True)
                    self.assertEqual(result.returncode, expected, result.stdout+result.stderr)

    def test_unknown_argument(self):
        result = subprocess.run(['sh', str(REPO/'build.sh'), '--invalid'], capture_output=True)
        self.assertEqual(result.returncode, 2)


if __name__ == '__main__':
    unittest.main()
