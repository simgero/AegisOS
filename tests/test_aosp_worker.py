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
    if 'init' in args and '--repo-url=https://android.googlesource.com/tools/repo' not in args:
        sys.exit('repo launcher must use the explicit upstream URL')
    if 'sync' in args:
        required = {'-j1', '--jobs-network=1', '--jobs-checkout=1', '--retry-fetches=0', '--fail-fast'}
        if not required.issubset(args): sys.exit('unsafe sync concurrency/retries')
        if mode == 'rate_limit':
            print('error: RPC failed; HTTP 429 curl 22 The requested URL returned error: 429')
            sys.exit(1)
    if 'manifest' in args:
        pathlib.Path(args[args.index('-o')+1]).write_text('<manifest/>')
elif name == 'env':
    if os.environ.get('AEGIS_SYNC_ONLY') == '1': sys.exit('sync-only must not compile')
    # Intercept only the real compilation boundary, leaving the state machine intact.
    if mode == 'build': sys.exit(7)
    product = root/'product'
    product.mkdir()
    for name in ('kernel', 'boot.img', 'init_boot.img', 'vendor_boot.img', 'super.img', 'userdata.img', 'vbmeta.img'):
        if mode != 'missing_image' or name != 'vendor_boot.img':
            (product/name).write_bytes(b'fixture image')
    pathlib.Path(args[-1], 'product-out.txt').write_text(str(product))
    pathlib.Path(args[-1], 'product-target.txt').write_text('aegis_qemu_arm64-bp2a-userdebug\n')
elif name == 'gh':
    if os.environ.get('AEGIS_SYNC_ONLY') == '1': sys.exit('sync-only must not call GitHub API')
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


@unittest.skipUnless(sys.platform.startswith('linux'),
                     'Worker needs Linux tools and a case-sensitive filesystem; covered by Linux CI')
class WorkerTests(unittest.TestCase):
    def exercise(self, mode='', sync_only=False):
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
            if sync_only:
                env['AEGIS_SYNC_ONLY'] = '1'
                env.pop('CREDENTIALS_DIRECTORY')
            result = subprocess.run(['bash', str(scripts/'worker.sh')], env=env,
                                    capture_output=True, text=True, timeout=30)
            runs = list((root/'server/runs').iterdir())
            self.assertEqual(len(runs), 1)
            self.assertTrue((runs[0]/'status').exists(), result.stdout+result.stderr)
            status = (runs[0]/'status').read_text().strip()
            self.assertNotIn('test-only-token', result.stdout+result.stderr)
            if mode:
                self.assertNotEqual(result.returncode, 0, result.stdout+result.stderr)
                self.assertEqual(status, 'RATE_LIMITED' if mode == 'rate_limit' else 'FAILED')
                if mode == 'rate_limit':
                    self.assertTrue((root/'server/work/google-retry-after').exists())
                    self.assertFalse((root/'product').exists())
                self.assertFalse((root/'remote/published').exists())
                self.assertNotIn('UPLOAD_VERIFIED:', result.stdout)
            elif sync_only:
                self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
                self.assertEqual(status, 'SOURCES_READY')
                self.assertTrue((runs[0]/'artifacts/manifest.xml').exists())
                self.assertFalse((root/'product').exists())
                self.assertFalse((root/'remote').exists())
            else:
                self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
                self.assertEqual(status, 'UPLOAD_VERIFIED')
                self.assertTrue((root/'remote/published').exists())
                self.assertTrue((root/'remote/manifest.xml').exists())
                self.assertTrue(list((root/'remote').glob('images.tar.xz.part-*')))

    def test_verified_success(self): self.exercise()
    def test_sources_only_without_credentials(self): self.exercise(sync_only=True)
    def test_sources_only_rate_limit(self): self.exercise('rate_limit', sync_only=True)
    def test_rate_limit_stops_before_build(self): self.exercise('rate_limit')
    def test_failed_build(self): self.exercise('build')
    def test_missing_image(self): self.exercise('missing_image')
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

    def test_token_stdin_rejects_invalid_commit_without_disclosing_token(self):
        result = subprocess.run(['sh', str(REPO/'build.sh'), '--token-stdin', 'main'],
                                input='secret-fixture-token\n', text=True, capture_output=True)
        self.assertEqual(result.returncode, 2)
        self.assertNotIn('secret-fixture-token', result.stdout + result.stderr)

    def test_token_stdin_requires_input(self):
        result = subprocess.run(['sh', str(REPO/'build.sh'), '--token-stdin', 'a'*40],
                                input='', text=True, capture_output=True)
        self.assertEqual(result.returncode, 1)
        self.assertIn('Could not read token', result.stderr)


if __name__ == '__main__':
    unittest.main()
