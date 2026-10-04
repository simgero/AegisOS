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
            self.assertEqual(receipt['verity_mode'], 'enforcing')
            self.assertEqual(receipt['vbmeta_flags'], dict.fromkeys(prepare.VBMETA_IMAGES, 0))
            config = (root / 'runtime.bootconfig').read_text()
            self.assertIn('androidboot.vbmeta.size=2688', config)
            self.assertEqual(config.count('androidboot.veritymode=enforcing\n'), 1)
            self.assertNotIn('androidboot.verifiedbootstate=', config)
            self.assertNotIn('androidboot.vbmeta.device_state=', config)
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

    def test_disabled_or_unknown_flags_in_any_header_prevent_preparation(self):
        for name in prepare.VBMETA_IMAGES:
            for flags in (1, 2, 3, 0x80000000):
                with self.subTest(name=name, flags=flags), tempfile.TemporaryDirectory() as directory:
                    root = Path(directory)
                    self.fixture(root)
                    path = root / 'images' / (name + '.img')
                    data = bytearray(path.read_bytes())
                    offset = 4096 if name in ('boot', 'init_boot') else 0
                    struct.pack_into('>I', data, offset + 120, flags)
                    path.write_bytes(data)
                    # Even if signature verification succeeds, disabled integrity
                    # must not produce an enforcing configuration or fresh disks.
                    with (patch.object(prepare.subprocess, 'run', return_value=subprocess.CompletedProcess([], 0, 'verified')) as run,
                          patch.object(prepare.subprocess, 'check_output', return_value='a'*64)):
                        with self.assertRaisesRegex(ValueError, 'flags must be zero'):
                            prepare.prepare(root, Path('/fixture/avbtool.py'), 'b'*40)
                    self.assertEqual(run.call_count, 1)  # No mkfs invocation.
                    self.assertEqual([p.name for p in root.iterdir()], ['images'])

    def test_truncated_header_is_rejected_before_outputs(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.fixture(root)
            path = root / 'images/vbmeta.img'
            path.write_bytes(path.read_bytes()[:64])
            with (patch.object(prepare.subprocess, 'run', return_value=subprocess.CompletedProcess([], 0, 'verified')),
                  patch.object(prepare.subprocess, 'check_output', return_value='a'*64)):
                with self.assertRaisesRegex(ValueError, 'Expected AVB metadata'):
                    prepare.prepare(root, Path('/fixture/avbtool.py'), 'b'*40)
            self.assertEqual([p.name for p in root.iterdir()], ['images'])

    def test_malformed_digest_cannot_become_boot_configuration(self):
        for digest in ('', 'a'*63, 'g'*64, 'a'*64 + '\nandroidboot.veritymode=logging'):
            with self.subTest(digest=digest), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                self.fixture(root)
                with (patch.object(prepare.subprocess, 'run', return_value=subprocess.CompletedProcess([], 0, 'verified')),
                      patch.object(prepare.subprocess, 'check_output', return_value=digest)):
                    with self.assertRaisesRegex(ValueError, 'Expected a SHA-256 AVB digest'):
                        prepare.prepare(root, Path('/fixture/avbtool.py'), 'b'*40)
                self.assertEqual([p.name for p in root.iterdir()], ['images'])
