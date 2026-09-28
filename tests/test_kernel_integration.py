import gzip
import importlib.util
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts/kernel/integrate.py"
spec = importlib.util.spec_from_file_location("kernel_integration", SCRIPT)
kernel = importlib.util.module_from_spec(spec)
spec.loader.exec_module(kernel)
RELEASE = "6.12.18-aegis-public-fixture"
VERMAGIC = RELEASE + " SMP preempt mod_unload modversions aarch64"


def config_bytes():
    return ("\n".join(f"{key}=y" for key in sorted(kernel.REQUIRED_CONFIG)) +
            "\n# CONFIG_PUBLIC_FIXTURE is not set\n").encode()


def image(config=None):
    # Structural metadata fixtures only, never executable code or bootable kernels.
    header = bytearray(64)
    header[56:60] = b"ARM\x64"
    return (header + b"Linux version " + RELEASE.encode() + b" (public fixture)\0" +
            b"IKCFG_ST" + gzip.compress(config if config is not None else config_bytes(), mtime=0) +
            b"IKCFG_ED")


def module(vermagic=VERMAGIC):
    strings = b"\0.shstrtab\0.modinfo\0"
    info = b"name=public_fixture\0vermagic=" + vermagic.encode() + b"\0"
    start = 64 + len(strings) + len(info)
    ident = b"\x7fELF\x02\x01\x01" + bytes(9)
    header = struct.pack("<16sHHIQQQIHHHHHH", ident, 1, 183, 1, 0, 0, start, 0,
                         64, 0, 0, 64, 3, 1)
    sections = bytes(64)
    sections += struct.pack("<IIQQQQIIQQ", strings.index(b".shstrtab"), 3, 0, 0,
                            64, len(strings), 0, 0, 1, 0)
    sections += struct.pack("<IIQQQQIIQQ", strings.index(b".modinfo"), 1, 0, 0,
                            64 + len(strings), len(info), 0, 0, 1, 0)
    return header + strings + info + sections


def boot_image(payload):
    header = bytearray(4096)
    header[:8] = b"ANDROID!"
    struct.pack_into("<I", header, 8, len(payload))
    struct.pack_into("<I", header, 20, 1584)
    struct.pack_into("<I", header, 40, 4)
    return header + payload


class KernelIntegrationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.run = self.root / "run"
        self.gki = self.run / "gki"
        self.vendor = self.run / "virtual-device"
        self.gki.mkdir(parents=True)
        self.vendor.mkdir()
        (self.run / "status").write_text("BUILT_UNVERIFIED\n")
        (self.run / "builder-commit.txt").write_text("1" * 40 + "\n")
        for name in ("manifest.xml", "aegis_runtime_defconfig"):
            shutil.copyfile(ROOT / "kernel" / name, self.run / name)
        shutil.copyfile(ROOT / "kernel/manifest.xml", self.run / "resolved-manifest.xml")
        for directory in (self.gki, self.vendor):
            (directory / "Image").write_bytes(image())
        (self.gki / ".config").write_bytes(config_bytes())
        (self.gki / "Module.symvers").write_text("public metadata fixture\n")
        for directory, names in ((self.gki, kernel.BOOT_GKI), (self.vendor, kernel.BOOT_VENDOR)):
            for name in names:
                (directory / name).write_bytes(module())
        self.aosp = self.root / "aosp"
        (self.aosp / "build").mkdir(parents=True)
        (self.aosp / "build/envsetup.sh").write_text("not executed\n")
        self.receipt = self.root / "image-run/kernel-inputs.json"

    def inspect(self):
        return kernel.inspect(self.run, ROOT)

    def prepare(self, receipt=None):
        return kernel.prepare(self.run, ROOT, self.aosp, receipt or self.receipt)

    def test_matched_inputs_are_inventoried_but_not_claimed_bootable(self):
        # Virtual-device dist may also contain identical GKI modules.
        name = sorted(kernel.BOOT_GKI)[0]
        shutil.copyfile(self.gki / name, self.vendor / name)
        identity, record, origins = self.inspect()
        self.assertEqual(len(identity), 64)
        self.assertEqual(record["status"], "CHECKED_INPUTS_NOT_BOOTED")
        self.assertEqual(record["kernel_release"], RELEASE)
        self.assertEqual(record["vermagic"], VERMAGIC)
        self.assertEqual(len(record["source_pins"]), 40)
        self.assertIn("gki/" + name, record["files"])
        self.assertNotIn("vendor/" + name, record["files"])
        self.assertEqual(set(origins), set(record["files"]))
        self.assertFalse((self.aosp / "device").exists())

    def test_kernel_copies_and_embedded_config_must_agree(self):
        (self.vendor / "Image").write_bytes(image() + b"different")
        with self.assertRaisesRegex(ValueError, "different kernels"):
            self.inspect()
        (self.vendor / "Image").write_bytes(image())
        (self.gki / ".config").write_bytes(config_bytes().replace(b"CONFIG_USER_NS=y", b"CONFIG_USER_NS=n"))
        with self.assertRaisesRegex(ValueError, "Sidecar"):
            self.inspect()

    def test_missing_namespace_or_security_option_cannot_be_packaged(self):
        for option in ("CONFIG_USER_NS", "CONFIG_PID_NS", "CONFIG_IPC_NS", "CONFIG_TMPFS_XATTR", "CONFIG_SECURITY_SELINUX",
                       "CONFIG_MODULE_SIG", "CONFIG_FS_ENCRYPTION", "CONFIG_DM_VERITY",
                       "CONFIG_MEMCG", "CONFIG_F2FS_FS", "CONFIG_F2FS_FS_XATTR",
                       "CONFIG_F2FS_FS_SECURITY"):
            config = config_bytes().replace((option + "=y").encode(), (option + "=n").encode())
            for directory in (self.gki, self.vendor):
                (directory / "Image").write_bytes(image(config))
            (self.gki / ".config").write_bytes(config)
            with self.subTest(option=option), self.assertRaisesRegex(ValueError, "configuration"):
                self.inspect()

    def test_old_or_differently_configured_modules_fail_before_staging(self):
        for version in ("6.1.0-old SMP", VERMAGIC.replace(" preempt", "")):
            (self.vendor / "nd_virtio.ko").write_bytes(module(version))
            with self.subTest(version=version), self.assertRaises(ValueError):
                self.prepare()
            self.assertFalse((self.aosp / "device").exists())

    def test_conflicting_duplicate_or_missing_early_boot_module_is_rejected(self):
        name = sorted(kernel.BOOT_GKI)[0]
        (self.vendor / name).write_bytes(module() + b"modified")
        with self.assertRaisesRegex(ValueError, "Conflicting GKI"):
            self.inspect()
        (self.vendor / name).unlink()
        (self.vendor / "nd_virtio.ko").unlink()
        with self.assertRaisesRegex(ValueError, "early boot"):
            self.inspect()

    def test_foreign_truncated_or_missing_module_metadata_is_rejected(self):
        wrong_machine = bytearray(module())
        wrong_machine[18:20] = (62).to_bytes(2, "little")
        wrong_offset = bytearray(module())
        struct.pack_into("<Q", wrong_offset, 40, 2**63)
        for data in (b"", module()[:63], wrong_machine, wrong_offset,
                     module().replace(b".modinfo", b".unknown"),
                     module().replace(b"vermagic=", b"invalidx=")):
            with self.subTest(length=len(data)), self.assertRaises(ValueError):
                kernel.module_vermagic(data)

    def test_non_kernel_corrupt_or_ambiguous_embedded_config_is_rejected(self):
        for data in (b"ANDROID!" + image(), image().replace(b"IKCFG_ST", b"MISSING!"),
                     image().replace(b"IKCFG_ED", b"MISSING!"), image() + b"IKCFG_ST",
                     image() + b"Linux version 6.1.0-other (fixture)"):
            with self.subTest(length=len(data)), self.assertRaises(ValueError):
                kernel.image_metadata(data)
        with patch.object(kernel, "MAX_TEXT", 32), self.assertRaises(ValueError):
            kernel.image_metadata(image())
        with self.assertRaisesRegex(ValueError, "Duplicate"):
            kernel.configuration(b"CONFIG_USER_NS=y\nCONFIG_USER_NS=n\n")

    def test_unfinished_runs_and_changed_provenance_are_rejected(self):
        for status in ("BUILDING", "FAILED", "PREPARING"):
            (self.run / "status").write_text(status)
            with self.subTest(status=status), self.assertRaisesRegex(ValueError, "completed"):
                self.inspect()
        (self.run / "status").write_text("BUILT_UNVERIFIED")
        (self.run / "aegis_runtime_defconfig").write_text("CONFIG_USER_NS=n\n")
        with self.assertRaisesRegex(ValueError, "fragment differ"):
            self.inspect()
        shutil.copyfile(ROOT / "kernel/aegis_runtime_defconfig", self.run / "aegis_runtime_defconfig")
        path = self.run / "resolved-manifest.xml"
        path.write_text(path.read_text().replace("50eb8d5d443b43f38d6e72f005f1b8601ac88a05", "0" * 40))
        with self.assertRaisesRegex(ValueError, "Resolved"):
            self.inspect()

    def test_staging_and_product_registration_are_reusable_and_reversible(self):
        identity = self.prepare()
        saved = json.loads(self.receipt.read_text())
        self.assertEqual(saved["bundle_id"], identity)
        destination = self.aosp / "device/aegis/runtime-kernels" / identity
        self.assertEqual(kernel.inventory(destination), saved["inputs"]["files"])
        product = self.aosp / "device/aegis/qemu_arm64"
        include = (product / kernel.INCLUDE).read_text()
        for name, value in saved["selection"].items():
            self.assertIn(f"{name} := {value}\n", include)
        self.assertEqual(self.prepare(self.root / "second-receipt.json"), identity)
        normal = kernel.load_tool(ROOT / "scripts/aosp/link-product.py", "test_normal_registration")
        normal.register(ROOT / "device/aegis/qemu_arm64", self.aosp)
        self.assertFalse((product / kernel.INCLUDE).exists())
        self.assertTrue(destination.is_dir())  # Preserve prior kernel outputs.
        self.assertTrue(list((self.aosp / "out/aegis-product-backups").iterdir()))

    def test_modified_installed_inputs_are_preserved_and_refused(self):
        identity = self.prepare()
        path = self.aosp / "device/aegis/runtime-kernels" / identity / "gki/kernel-6.12"
        path.write_bytes(b"keep changed bytes")
        with self.assertRaisesRegex(ValueError, "Existing kernel inputs changed"):
            self.prepare(self.root / "second-receipt.json")
        self.assertEqual(path.read_bytes(), b"keep changed bytes")
        self.assertFalse((self.root / "second-receipt.json").exists())

    def test_links_and_interrupted_copy_never_replace_existing_product(self):
        normal = kernel.load_tool(ROOT / "scripts/aosp/link-product.py", "test_product_registration")
        normal.register(ROOT / "device/aegis/qemu_arm64", self.aosp)
        product = self.aosp / "device/aegis/qemu_arm64"
        before = normal.inventory(product)
        original_copy = shutil.copyfile
        calls = 0

        def interrupted(source, target):
            nonlocal calls
            calls += 1
            if calls == 3:
                raise OSError("simulated interrupted copy")
            return original_copy(source, target)

        with patch.object(kernel.shutil, "copyfile", side_effect=interrupted), self.assertRaises(OSError):
            self.prepare()
        self.assertEqual(normal.inventory(product), before)
        self.assertEqual(list((self.aosp / "out/aegis-kernel-staging").iterdir()), [])
        self.assertFalse(self.receipt.exists())
        path = self.vendor / "nd_virtio.ko"
        path.unlink()
        path.symlink_to(self.gki / "virtio_blk.ko")
        with self.assertRaisesRegex(ValueError, "regular kernel input"):
            self.prepare()
        self.assertEqual(normal.inventory(product), before)

    def test_cli_checks_actual_build_selection_and_resulting_kernel_bytes(self):
        self.prepare()
        saved = json.loads(self.receipt.read_text())
        selection = saved["selection"]

        def call(*args):
            return subprocess.run([sys.executable, "-B", str(SCRIPT), *args,
                                   "--receipt", str(self.receipt)], capture_output=True)

        args = ("verify-selection", "--kernel", selection["TARGET_KERNEL_PATH"],
                "--system", selection["SYSTEM_DLKM_SRC"], "--vendor", selection["KERNEL_MODULES_PATH"])
        self.assertEqual(call(*args).returncode, 0)
        self.assertNotEqual(call(*args[:-1], "old/prebuilt/modules").returncode, 0)
        boot = self.root / "boot.img"
        boot.write_bytes(boot_image(image()))
        image_args = ("verify-image", "--image", str(self.gki / "Image"), "--boot-image", str(boot))
        self.assertEqual(call(*image_args).returncode, 0)
        boot.write_bytes(boot_image(b"old boot kernel"))
        self.assertNotEqual(call(*image_args).returncode, 0)
        boot.write_bytes(boot_image(image()))
        (self.gki / "Image").write_bytes(b"different kernel")
        self.assertNotEqual(call(*image_args).returncode, 0)

    def test_boot_payload_requires_complete_expected_header(self):
        data = boot_image(image())
        self.assertEqual(kernel.boot_kernel(data), image())
        wrong_version = bytearray(data)
        struct.pack_into("<I", wrong_version, 40, 3)
        wrong_size = bytearray(data)
        struct.pack_into("<I", wrong_size, 8, len(data) * 2)
        for bad in (data[:64], data[:-1], wrong_version, wrong_size):
            with self.subTest(length=len(bad)), self.assertRaises(ValueError):
                kernel.boot_kernel(bad)


if __name__ == "__main__":
    unittest.main()
