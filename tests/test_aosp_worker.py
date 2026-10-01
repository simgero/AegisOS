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
    if mode != 'missing_image_receipt':
        pathlib.Path(args[-1], 'runtime-base-image.json').write_text('public image receipt transport fixture\n')
    if mode != 'missing_storage_receipt':
        pathlib.Path(args[-1], 'runtime-storage-source.json').write_text('public framework source receipt transport fixture\n')
    if mode != 'missing_vold_receipt':
        pathlib.Path(args[-1], 'vold-source.json').write_text('public vold source receipt fixture\n')
    if os.environ.get('AEGIS_KERNEL_RUN') and mode != 'missing_kernel_receipt':
        pathlib.Path(args[-1], 'kernel-inputs.json').write_text('{"fixture":"kernel input transport only"}\n')
    if os.environ.get('AEGIS_RUNTIME_RUN'):
        for name in ('runtime-base-inputs.json', 'runtime-base-plan.json',
                     'runtime-base-generation.json', 'runtime-base-fs_config.txt'):
            if mode != 'missing_runtime_receipt' or name != 'runtime-base-plan.json':
                pathlib.Path(args[-1], name).write_text('public runtime receipt transport fixture\n')
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
        if mode == 'corrupt_runtime_receipt' and filename == 'runtime-base-plan.json':
            target.write_bytes(b'corrupted runtime receipt')
        if mode == 'corrupt_image_receipt' and filename == 'runtime-base-image.json':
            target.write_bytes(b'corrupted image receipt')
        if mode == 'corrupt_storage_receipt' and filename == 'runtime-storage-source.json':
            target.write_bytes(b'corrupted framework source receipt')
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
    def exercise(self, mode='', sync_only=False, runtime_kernel=False, runtime_base=False):
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
            env.pop('AEGIS_KERNEL_RUN', None)
            env.pop('AEGIS_RUNTIME_RUN', None)
            env.pop('AEGIS_SYNC_ONLY', None)
            if runtime_kernel:
                env['AEGIS_KERNEL_RUN'] = str(root/'server/runs/kernel-public-fixture')
            if runtime_base:
                env['AEGIS_RUNTIME_RUN'] = str(root/'server/runs/runtime-base-public-fixture')
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
                self.assertEqual((root/'remote/runtime-base-image.json').read_bytes(),
                                 (runs[0]/'artifacts/runtime-base-image.json').read_bytes())
                self.assertIn('runtime-base-image.json', (root/'remote/SHA256SUMS').read_text())
                self.assertEqual((root/'remote/runtime-storage-source.json').read_bytes(),
                                 (runs[0]/'artifacts/runtime-storage-source.json').read_bytes())
                self.assertIn('runtime-storage-source.json', (root/'remote/SHA256SUMS').read_text())
                if runtime_kernel:
                    self.assertEqual((root/'remote/kernel-inputs.json').read_bytes(),
                                     (runs[0]/'artifacts/kernel-inputs.json').read_bytes())
                    self.assertIn('kernel-inputs.json', (root/'remote/SHA256SUMS').read_text())
                if runtime_base:
                    for name in ('runtime-base-inputs.json', 'runtime-base-plan.json',
                                 'runtime-base-generation.json', 'runtime-base-fs_config.txt'):
                        self.assertEqual((root/'remote'/name).read_bytes(), (runs[0]/'artifacts'/name).read_bytes())
                        self.assertIn(name, (root/'remote/SHA256SUMS').read_text())

    def test_verified_success(self): self.exercise()
    def test_missing_image_receipt_prevents_success(self): self.exercise('missing_image_receipt')
    def test_corrupt_image_receipt_prevents_success(self): self.exercise('corrupt_image_receipt')
    def test_missing_storage_receipt_prevents_success(self): self.exercise('missing_storage_receipt')
    def test_missing_vold_receipt_prevents_success(self): self.exercise('missing_vold_receipt')
    def test_corrupt_storage_receipt_prevents_success(self): self.exercise('corrupt_storage_receipt')
    def test_selected_kernel_receipt_is_published_and_verified(self): self.exercise(runtime_kernel=True)
    def test_missing_kernel_receipt_prevents_success(self): self.exercise('missing_kernel_receipt', runtime_kernel=True)
    def test_selected_base_receipts_are_published_and_verified(self): self.exercise(runtime_base=True)
    def test_kernel_and_base_receipts_survive_together(self): self.exercise(runtime_base=True, runtime_kernel=True)
    def test_missing_base_receipt_prevents_success(self): self.exercise('missing_runtime_receipt', runtime_base=True)
    def test_corrupt_base_receipt_prevents_success(self): self.exercise('corrupt_runtime_receipt', runtime_base=True)
    def test_sources_only_without_credentials(self): self.exercise(sync_only=True)
    def test_sources_only_rate_limit(self): self.exercise('rate_limit', sync_only=True)
    def test_rate_limit_stops_before_build(self): self.exercise('rate_limit')
    def test_failed_build(self): self.exercise('build')
    def test_missing_image(self): self.exercise('missing_image')
    def test_failed_upload(self): self.exercise('upload')
    def test_corrupt_remote_asset(self): self.exercise('corrupt')
    def test_failed_publication(self): self.exercise('publish')


class BootstrapTests(unittest.TestCase):
    def incremental_storage_fixture(self, *, free_gib=200, mode='ok'):
        # Inert files and an isolated shell harness; no real server or build.
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            volume = root/'volume'
            prior = volume/'runs/aosp-prior'
            artifacts = prior/'artifacts'
            artifacts.mkdir(parents=True)
            (prior/'status').write_text('UPLOAD_VERIFIED\n' if mode != 'unverified' else 'BUILDING\n')
            config = (REPO/'scripts/aosp/config.sh').read_text()
            (artifacts/'config.sh').write_text(config)
            (artifacts/'manifest.xml').write_text('<manifest/>\n')
            (artifacts/'builder-commit.txt').write_text('a'*40+'\n')
            new_config = root/'new-config.sh'
            new_config.write_text(config + ('# changed recipe\n' if mode == 'recipe' else ''))
            product = volume/'work/aosp/out/target/product/qemu_arm64'
            product.mkdir(parents=True)
            for name in ('system.img', 'super.img'):
                (product/name).write_text('inert image fixture\n')
            if mode == 'missing_output': (product/'super.img').unlink()
            if mode == 'linked_evidence':
                (artifacts/'manifest.xml').unlink()
                (artifacts/'manifest.xml').symlink_to(new_config)
            bin_dir = root/'bin'
            bin_dir.mkdir()
            (bin_dir/'df').write_text('#!/bin/sh\nprintf "Filesystem 1024-blocks Used Available Capacity Mounted on\\n/dev/test 900000000 0 %s 0%% /fixture\\n" "$TEST_FREE_KIB"\n')
            (bin_dir/'runuser').write_text('#!/bin/sh\n[ "$1" = -u ] && [ "$2" = aegis-build ] && [ "$3" = -- ] && [ "$4" = git ] || exit 19\nprintf "%s\\n" "$TEST_MANIFEST"\n')
            for command in bin_dir.iterdir(): command.chmod(0o755)
            pin = next(line.split('=', 1)[1] for line in config.splitlines() if line.startswith('AOSP_MANIFEST_COMMIT='))
            source = (REPO/'build.sh').read_text()
            source = source[:source.index('# Execute only after')].replace('/srv/aegis', str(volume))
            harness = root/'check.sh'
            harness.write_text(source+'\ncheck_storage\nverify_incremental_recipe "$TEST_NEW_CONFIG"\n')
            env = dict(os.environ, PATH=str(bin_dir)+':'+os.environ['PATH'],
                       AEGIS_INCREMENTAL_FROM_RUN=str(prior), TEST_NEW_CONFIG=str(new_config),
                       TEST_FREE_KIB=str(free_gib*1048576),
                       TEST_MANIFEST='b'*40 if mode == 'manifest' else pin)
            return subprocess.run(['sh', str(harness)], env=env, capture_output=True, text=True)

    def test_explicit_completed_incremental_storage_preserves_a_free_space_floor(self):
        good = self.incremental_storage_fixture()
        self.assertEqual(good.returncode, 0, good.stdout+good.stderr)
        low = self.incremental_storage_fixture(free_gib=199)
        self.assertNotEqual(low.returncode, 0)
        self.assertIn('200 GiB', low.stderr)

    def test_incremental_storage_requires_verified_matching_existing_inputs(self):
        for mode in ('unverified', 'manifest', 'missing_output', 'linked_evidence', 'recipe'):
            with self.subTest(mode=mode):
                result = self.incremental_storage_fixture(mode=mode)
                self.assertNotEqual(result.returncode, 0, result.stdout+result.stderr)

    def test_incremental_storage_cannot_select_arbitrary_or_escaping_paths(self):
        for path in ('/tmp/aosp-any', '/srv/aegis/runs/aosp-one/../other',
                     '/srv/aegis/runs/aosp-space here', '/srv/aegis/runs/aosp-$(command)'):
            with self.subTest(path=path):
                result = subprocess.run(['sh', str(REPO/'build.sh'), '--check-storage'],
                                        env=dict(os.environ, AEGIS_INCREMENTAL_FROM_RUN=path),
                                        capture_output=True, text=True)
                self.assertNotEqual(result.returncode, 0)

    def test_runtime_run_path_cannot_escape_or_inject_shell_text(self):
        for path in ('/tmp/runtime-base-any', '/srv/aegis/runs/runtime-base-one/../other',
                     '/srv/aegis/runs/runtime-base-space here', '/srv/aegis/runs/runtime-base-$(command)'):
            with self.subTest(path=path):
                result = subprocess.run(['sh', str(REPO/'build.sh')],
                                        env=dict(os.environ, AEGIS_RUNTIME_RUN=path),
                                        capture_output=True, text=True)
                self.assertEqual(result.returncode, 2)
                self.assertIn('untime run', result.stderr)

    def test_kernel_run_path_cannot_select_arbitrary_or_escaping_locations(self):
        for path in ('/tmp/kernel-any', '/srv/aegis/runs/kernel-one/../other',
                     '/srv/aegis/runs/kernel-space here', '/srv/aegis/runs/kernel-$(command)'):
            with self.subTest(path=path):
                result = subprocess.run(['sh', str(REPO/'build.sh')],
                                        env=dict(os.environ, AEGIS_KERNEL_RUN=path),
                                        capture_output=True, text=True)
                self.assertEqual(result.returncode, 2)
                self.assertIn('ernel run', result.stderr)

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
