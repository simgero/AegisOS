"""Real local build driver with inert compiler inputs; never invokes AOSP or a network."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class LocalBuildTests(unittest.TestCase):
    def exercise(self, missing=False):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            server = root / 'server'
            aosp = server / 'work/aosp'
            aosp.mkdir(parents=True)
            (server / 'runs').mkdir()
            repo = server / 'work/repo-tool/repo'
            repo.parent.mkdir()
            repo.write_text('import sys\nfrom pathlib import Path\nif "-o" in sys.argv: Path(sys.argv[sys.argv.index("-o")+1]).write_text("<manifest/>\\n")\n')
            scripts = root / 'scripts'
            scripts.mkdir()
            driver = (ROOT / 'scripts/aosp/build-local.sh').read_text().replace('/srv/aegis', str(server))
            (scripts / 'build-local.sh').write_text(driver)
            (scripts / 'config.sh').write_text('AOSP_MANIFEST_COMMIT='+'b'*40+'\nAOSP_LUNCH=fixture\n')
            binaries = root / 'bin'
            binaries.mkdir()
            for name, content in {
                'id': '#!/bin/sh\necho aegis-build\n',
                'git': '#!/bin/sh\necho '+'b'*40+'\n',
                'gh': '#!/bin/sh\necho UPLOAD_FORBIDDEN >&2\nexit 91\n',
            }.items():
                path = binaries / name
                path.write_text(content)
                path.chmod(0o755)
            # All images are independent inert fixture files, never actual VM disks.
            product = aosp / 'out/product'
            product.mkdir(parents=True)
            names = ('kernel boot.img init_boot.img vendor_boot.img ramdisk.img vendor-bootconfig.img '
                     'super.img userdata.img vbmeta.img vbmeta_system.img vbmeta_system_dlkm.img '
                     'vbmeta_vendor_dlkm.img system.img system_ext.img product.img vendor.img '
                     'odm.img system_dlkm.img vendor_dlkm.img odm_dlkm.img').split()
            for name in names:
                if not (missing and name == 'super.img'):
                    (product / name).write_text('fixture '+name+'\n')
            (scripts / 'compile.sh').write_text('''#!/bin/bash
set -eu
test -z "${GH_TOKEN:-}${GITHUB_TOKEN:-}${CREDENTIALS_DIRECTORY:-}"
printf 'out/product\n' > "$1/product-out.txt"
printf 'fixture\n' > "$1/product-target.txt"
''')
            env = dict(os.environ, PATH=str(binaries)+':'+os.environ['PATH'],
                       AEGIS_SCRIPT_COMMIT='a'*40, AEGIS_KERNEL_RUN='/fixture/kernel',
                       AEGIS_RUNTIME_RUN='/fixture/runtime', GH_TOKEN='must-be-removed',
                       GITHUB_TOKEN='must-be-removed', CREDENTIALS_DIRECTORY='/fixture/secret')
            result = subprocess.run(['bash', str(scripts / 'build-local.sh')], env=env,
                                    text=True, capture_output=True, timeout=30)
            runs = list((server / 'runs').iterdir())
            self.assertEqual(len(runs), 1, result.stdout+result.stderr)
            run = runs[0]
            self.assertNotIn('UPLOAD_FORBIDDEN', result.stdout+result.stderr)
            self.assertNotIn('must-be-removed', result.stdout+result.stderr)
            self.assertEqual((run / 'status').read_text().strip(),
                             'FAILED' if missing else 'LOCAL_BUILD_VERIFIED')
            if missing:
                self.assertNotEqual(result.returncode, 0)
            else:
                self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
                for name in names:
                    self.assertEqual((run / 'images' / name).read_bytes(), (product / name).read_bytes())
                self.assertTrue((run / 'images/SHA256SUMS').is_file())

    def test_local_snapshot_requires_complete_images_and_no_credentials(self): self.exercise()
    def test_missing_image_cannot_publish_local_success(self): self.exercise(missing=True)
