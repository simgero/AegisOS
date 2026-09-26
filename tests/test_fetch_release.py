import hashlib
import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location(
    "fetch_release", Path(__file__).resolve().parents[1] / "scripts/fetch-release.py")
fetch = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fetch)


class ReleaseVerificationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.name = "images.tar.xz.part-0000"
        (self.root / self.name).write_bytes(b"fixture")
        self.digest = hashlib.sha256(b"fixture").hexdigest()
        (self.root / "SHA256SUMS").write_text(f"{self.digest}  ./{self.name}\n")

    def test_valid(self):
        self.assertEqual(fetch.verify(self.root), 1)

    def test_corruption(self):
        (self.root / self.name).write_bytes(b"corrupt")
        with self.assertRaisesRegex(ValueError, "Checksum mismatch"):
            fetch.verify(self.root)

    def test_missing_asset(self):
        (self.root / self.name).unlink()
        with self.assertRaises(ValueError):
            fetch.verify(self.root)

    def test_extra_asset(self):
        (self.root / "extra").touch()
        with self.assertRaises(ValueError):
            fetch.verify(self.root)

    def test_traversal(self):
        (self.root / "SHA256SUMS").write_text(f"{self.digest}  ../outside\n")
        with self.assertRaises(ValueError):
            fetch.verify(self.root)

    def test_symlink(self):
        (self.root / self.name).unlink()
        (self.root / self.name).symlink_to("SHA256SUMS")
        with self.assertRaises(ValueError):
            fetch.verify(self.root)

    def test_gap(self):
        (self.root / self.name).rename(self.root / "images.tar.xz.part-0001")
        (self.root / "SHA256SUMS").write_text(
            f"{self.digest}  ./images.tar.xz.part-0001\n")
        with self.assertRaisesRegex(ValueError, "non-contiguous"):
            fetch.verify(self.root)
