"""Local AVB bootconfig construction; signature verification remains avbtool's job."""
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('prepare_server',
        Path(__file__).resolve().parents[1] / 'scripts/prepare-server-qemu.py')
prepare = importlib.util.module_from_spec(spec)
spec.loader.exec_module(prepare)


class ServerQemuPrepareTests(unittest.TestCase):
    def fixture(self, root):
        images = root / 'images'
        images.mkdir()
        for name in ('vbmeta', 'boot', 'init_boot', 'vbmeta_system', 'vbmeta_system_dlkm', 'vbmeta_vendor_dlkm'):
            header = bytearray(256)
            header[:4] = b'AVB0'
            struct.pack_into('>QQ', header, 12, 64, 128)
            if name in ('boot', 'init_boot'):
                footer = struct.pack('>4sIIQQQ28x', b'AVBf', 1, 0, 4096, 4096, 448)
                data = bytes(4096) + header + bytes(192) + footer
            else:
                data = header + bytes(192)
            (images / (name + '.img')).write_bytes(data)

    def test_chained_boot_footer_metadata_counts_towards_total_size(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.fixture(root)
            with (patch.object(prepare.subprocess, 'run', return_value=subprocess.CompletedProcess([], 0, 'verified')),
                  patch.object(prepare.subprocess, 'check_output', return_value='a'*64)):
                prepare.prepare(root, Path('/fixture/avbtool.py'), 'b'*40)
            receipt = json.loads((root / 'avb-checked.json').read_text())
            self.assertEqual(receipt['vbmeta_size'], 6 * 448)
            self.assertEqual(receipt['builder_commit'], 'b'*40)
            self.assertIn('androidboot.vbmeta.size=2688', (root / 'runtime.bootconfig').read_text())
            self.assertEqual((root / 'metadata.img').stat().st_size, 16 * 1024**2)
            with self.assertRaises(ValueError):
                prepare.prepare(root, Path('/fixture/avbtool.py'), 'b'*40)

    def test_failed_avb_verification_creates_no_boot_configuration_or_disks(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.fixture(root)
            with patch.object(prepare.subprocess, 'run', side_effect=subprocess.CalledProcessError(1, 'avbtool')):
                with self.assertRaises(subprocess.CalledProcessError):
                    prepare.prepare(root, Path('/fixture/avbtool.py'), 'b'*40)
            self.assertEqual([p.name for p in root.iterdir()], ['images'])
