"""Host file staging only; no native compilation or guest execution."""
import hashlib
import importlib.util
import os
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import uuid
import zlib

ROOT = Path(__file__).resolve().parents[1]


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, ROOT / 'scripts' / path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


image = load('local_image_io_test', 'local_image_io.py')
disk = load('local_disk_test', 'make-qemu-disk.py')
apfs = unittest.skipUnless(sys.platform == 'darwin', 'Real macOS clone API test')


class TempFiles(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()


class ImageTests(TempFiles):
    def test_complete_stream_readback_and_zero_padding(self):
        target = self.root / 'image'
        with image.ImageWriter(target, image.BLOCK * 2 + 7) as writer:
            writer.zeros(image.BLOCK)
            writer.write(b'x' * image.BLOCK)
            writer.zeros(7)
            report = writer.finish()
            self.assertEqual(writer.written, image.BLOCK)
        expected = bytes(image.BLOCK) + b'x' * image.BLOCK + bytes(7)
        self.assertEqual(target.read_bytes(), expected)
        self.assertEqual(report, {'size': len(expected), 'sha256': hashlib.sha256(expected).hexdigest()})
        self.assertEqual(target.stat().st_mode & 0o7777, 0o600)

    def test_incomplete_oversized_and_changed_output_never_finish(self):
        with image.ImageWriter(self.root / 'partial', 7) as writer:
            writer.write(b'abc')
            with self.assertRaises(ValueError): writer.finish()
            with self.assertRaises(ValueError): writer.write(b'toolong')
            writer.write(b'defg')
            os.pwrite(writer.fd, b'X', 0)
            with self.assertRaises(ValueError): writer.finish()

    def test_existing_files_and_redirected_target_are_preserved(self):
        target = self.root / 'existing'
        target.write_bytes(b'keep')
        with self.assertRaises(ValueError): image.ImageWriter(target, 0)
        link = self.root / 'link'
        link.symlink_to(target)
        with self.assertRaises(ValueError): image.ImageWriter(link, 0)
        self.assertEqual(target.read_bytes(), b'keep')

    @apfs
    def test_clone_is_independent_and_clears_stale_bytes_and_tail(self):
        seed = self.root / 'seed'
        original = b'A' * image.BLOCK + b'B' * image.BLOCK + b'stale tail'
        seed.write_bytes(original)
        target = self.root / 'copy'
        with image.ImageWriter(target, 2 * image.BLOCK, seed) as writer:
            writer.write(original[:image.BLOCK])
            writer.zeros(image.BLOCK)
            writer.finish()
            self.assertEqual(writer.reused, image.BLOCK)
            self.assertEqual(writer.written, image.BLOCK)
        self.assertNotEqual(seed.stat().st_ino, target.stat().st_ino)
        self.assertEqual(seed.read_bytes(), original)
        self.assertEqual(target.read_bytes(), b'A' * image.BLOCK + bytes(image.BLOCK))

    @apfs
    def test_seed_symlink_fifo_directory_and_device_are_refused(self):
        seed = self.root / 'seed'
        seed.write_bytes(b'keep')
        link = self.root / 'link'
        link.symlink_to(seed)
        fifo = self.root / 'fifo'
        os.mkfifo(fifo)
        for index, bad in enumerate([link, fifo, self.root, Path('/dev/null')]):
            with self.subTest(seed=bad), self.assertRaises(ValueError):
                image.ImageWriter(self.root / f'bad-{index}', 4, bad)
        self.assertEqual(seed.read_bytes(), b'keep')


def mixed_sparse():
    chunks = [struct.pack('<HHII', 0xcac1, 0, 1, 12 + 4096) + b'R' * 4096,
              struct.pack('<HHII', 0xcac2, 0, 1, 16) + b'abcd',
              struct.pack('<HHII', 0xcac2, 0, 1, 16) + bytes(4),
              struct.pack('<HHII', 0xcac3, 0, 2, 12),
              struct.pack('<HHII', 0xcac4, 0, 0, 16) + bytes(4)]
    return struct.pack('<IHHHHIIII', 0xed26ff3a, 1, 0, 28, 12, 4096, 5, len(chunks), 0) + b''.join(chunks)


class DiskTests(TempFiles):
    def fixture(self, name):
        root = self.root / name
        (root / 'images').mkdir(parents=True)
        for part in disk.IMAGE_NAMES + ['userdata', 'super']:
            (root / 'images' / (part + '.img')).write_bytes(part.encode())
        (root / 'images/super.img').write_bytes(mixed_sparse())
        for part in ['metadata', 'misc', 'frp']:
            (root / (part + '.img')).write_bytes(part.encode())
        return root

    def guids(self):
        values = iter(range(1, 100))
        return lambda: uuid.UUID(int=next(values))

    def test_gpt_and_android_sparse_expansion(self):
        root = self.fixture('fresh')
        report = disk.make_disk(root, guid_factory=self.guids())
        data = (root / 'android.raw').read_bytes()
        self.assertEqual(report['sha256'], hashlib.sha256(data).hexdigest())
        self.assertEqual(data[510:512], b'\x55\xaa')
        self.assertEqual(data[512:520], b'EFI PART')
        table = data[1024:17408]
        self.assertEqual(struct.unpack_from('<I', data, 512 + 88)[0], zlib.crc32(table))
        for offset in [512, len(data) - 512]:
            header = bytearray(data[offset:offset + 92])
            actual = struct.unpack_from('<I', header, 16)[0]
            struct.pack_into('<I', header, 16, 0)
            self.assertEqual(actual, zlib.crc32(header))
        self.assertEqual(data[-33 * 512:-512], table)
        begin = 8 * disk.MiB
        expected = b'R' * 4096 + b'abcd' * 1024 + bytes(3 * 4096)
        self.assertEqual(data[begin:begin + len(expected)], expected)
        self.assertEqual(data[begin + len(expected):begin + disk.MiB], bytes(disk.MiB - len(expected)))

    @apfs
    def test_poisoned_clone_matches_fresh_disk_byte_for_byte(self):
        fresh, reused = self.fixture('fresh'), self.fixture('reused')
        disk.make_disk(fresh, guid_factory=self.guids())
        seed = self.root / 'poisoned'
        poison = b'\xa5' * (16 * disk.MiB)
        seed.write_bytes(poison)
        disk.make_disk(reused, seed, self.guids())
        self.assertEqual((fresh / 'android.raw').read_bytes(), (reused / 'android.raw').read_bytes())
        self.assertEqual(seed.read_bytes(), poison)

    def test_truncated_sparse_chunk_fails(self):
        root = self.fixture('truncated')
        (root / 'images/super.img').write_bytes(mixed_sparse()[:-1])
        with self.assertRaises(ValueError): disk.make_disk(root)
        self.assertFalse((root / 'android.raw.json').exists())

    def test_sparse_overflow_and_trailing_bytes_fail(self):
        for index, data in enumerate([mixed_sparse() + b'X', mixed_sparse()[:16] + struct.pack('<I', 4) + mixed_sparse()[20:]]):
            root = self.fixture('bad' + str(index))
            (root / 'images/super.img').write_bytes(data)
            with self.assertRaises(ValueError): disk.make_disk(root)
