import importlib.util
import json
import os
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def module(name, file):
    spec = importlib.util.spec_from_file_location(name, ROOT / file)
    loaded = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(loaded)
    return loaded


product = module("link_product", "scripts/aosp/link-product.py")
qemu = module("qemu_kernel", "scripts/qemu-kernel.py")


class ProductTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.aosp = self.root / "aosp"
        (self.aosp / "build").mkdir(parents=True)
        (self.aosp / "build/envsetup.sh").touch()
        self.source = self.root / "source"
        self.source.mkdir()
        for name in product.FILES:
            (self.source / name).write_text("fixture")

    def test_register_and_update_preserves_old_source(self):
        product.register(self.source, self.aosp)
        product.register(self.source, self.aosp)
        second = self.root / "second"
        second.mkdir()
        for name in product.FILES:
            (second / name).write_text("new fixture")
        product.register(second, self.aosp)
        self.assertFalse((self.aosp / "device/aegis/qemu_arm64").is_symlink())
        self.assertEqual((self.aosp / "device/aegis/qemu_arm64/AndroidProducts.mk").read_text(), "new fixture")
        self.assertEqual((self.source / product.FILES[0]).read_text(), "fixture")

    def test_product_visible_without_following_directory_symlinks(self):
        product.register(self.source, self.aosp)
        discovered = [Path(root) / name for root, _, files in os.walk(self.aosp / "device")
                      for name in files if name == "AndroidProducts.mk"]
        self.assertIn(self.aosp / "device/aegis/qemu_arm64/AndroidProducts.mk", discovered)

    def test_legacy_managed_link_migrates(self):
        parent = self.aosp / "device/aegis"
        parent.mkdir(parents=True)
        (parent / "qemu_arm64").symlink_to(self.source)
        (parent / ".aegis-product-link.json").write_text(json.dumps({"source": str(self.source)}))
        product.register(self.source, self.aosp)
        self.assertFalse((parent / "qemu_arm64").is_symlink())
        self.assertTrue((self.source / "BoardConfig.mk").exists())

    def test_modified_managed_file_preserved(self):
        product.register(self.source, self.aosp)
        path = self.aosp / "device/aegis/qemu_arm64/BoardConfig.mk"
        path.write_text("user edit")
        with self.assertRaises(ValueError):
            product.register(self.source, self.aosp)
        self.assertEqual(path.read_text(), "user edit")

    def test_existing_directory_preserved(self):
        target = self.aosp / "device/aegis/qemu_arm64"
        target.mkdir(parents=True)
        (target / "user-work").write_text("keep")
        with self.assertRaises(ValueError):
            product.register(self.source, self.aosp)
        self.assertEqual((target / "user-work").read_text(), "keep")

    def test_unmanaged_link_rejected(self):
        parent = self.aosp / "device/aegis"
        parent.mkdir(parents=True)
        (parent / "qemu_arm64").symlink_to(self.source)
        with self.assertRaises(ValueError):
            product.register(self.source, self.aosp)

    def test_incomplete_source_rejected(self):
        (self.source / "BoardConfig.mk").unlink()
        with self.assertRaises(ValueError):
            product.register(self.source, self.aosp)
        self.assertFalse((self.aosp / "device").exists())


class QemuTests(unittest.TestCase):
    def test_boot_image_is_not_a_kernel(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "boot.img"
            path.write_bytes(b"ANDROID!" + bytes(56))
            with self.assertRaises(ValueError):
                qemu.command(path)

    def test_kernel_path_remains_single_argument(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "kernel with spaces"
            path.write_bytes(bytes(56) + b"ARM\x64" + bytes(4))
            argv = qemu.command(path)
            self.assertEqual(argv[argv.index("-kernel") + 1], str(path.resolve()))
            self.assertNotIn("-drive", argv)
            self.assertEqual(argv[argv.index("-net") + 1], "none")

    def test_probe_has_no_guest_execution(self):
        argv = qemu.command(probe=True)
        self.assertIn("-S", argv)
        self.assertNotIn("-kernel", argv)
        self.assertNotIn("-drive", argv)
