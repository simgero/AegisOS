"""Packaged-base failure tests. No target code, mount or OS execution.

The LP/sparse reader boundary uses inert fixtures; Linux CI also reads a real
tiny EROFS image containing only text with the packaged filesystem utility.
"""
import copy
import importlib.util
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("runtime_product_image", ROOT / "scripts/runtime/verify_product_image.py")
image_check = importlib.util.module_from_spec(spec)
sys.path.insert(0, str(ROOT / "scripts/runtime"))
try:
    spec.loader.exec_module(image_check)
finally:
    sys.path.pop(0)


class ProductImageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.super_image = self.root / "super.img"
        self.super_image.write_bytes(b"inert super metadata fixture" + b"\0" * 4096)
        self.report = self.root / "runtime-base-image.json"
        self.receipt = self.root / "runtime-base-inputs.json"
        self.assets = {"etc/aegis/runtime/base.ext4": b"not an executable filesystem, only text\n",
                       "etc/aegis/runtime/generation.json": b'{"public_fixture":true}\n'}
        inputs = {"status": "CHECKED_BASE_INPUTS_NOT_MOUNTED", "files": {
            image_check.INSTALLED[path]: {"size": len(data), "sha256": image_check.integrate.sha(data)}
            for path, data in self.assets.items()}}
        self.selection = {"bundle_id": image_check.integrate.sha(image_check.generation.encoded(inputs)),
                          "inputs": inputs, "installed": image_check.INSTALLED}
        self.receipt.write_bytes(image_check.generation.encoded(self.selection))
        self.tools = {}
        for name in image_check.TOOLS:
            tool = self.root / name
            tool.write_text("inert tool fixture " + name)
            self.tools[name] = tool
        self.behavior = ""
        self.calls = []
        self.real_tool = image_check.run_tool
        fake = patch.object(image_check, "run_tool", side_effect=self.reader)
        fake.start()
        self.addCleanup(fake.stop)

    def reader(self, tool, args, log, cwd):
        name = Path(tool).name
        self.calls.append((name, list(map(str, args))))
        if self.behavior == "failure-" + name:
            raise subprocess.CalledProcessError(1, [str(tool)])
        if name == "simg2img":
            Path(args[1]).write_bytes(b"raw metadata fixture".ljust(8192, b"\0"))
            if self.behavior == "wrong-expansion":
                Path(args[1]).write_bytes(b"truncated")
        elif name == "lpunpack":
            if self.behavior == "empty-partition-output":
                return
            data = bytearray(2048)
            struct.pack_into("<I", data, 1024, 0xe0f5e1e2 if self.behavior != "wrong-filesystem" else 0)
            (Path(args[-1]) / "system_ext_a.img").write_bytes(data)
        elif name == "fsck.erofs":
            destination = Path(next(str(a).split("=", 1)[1] for a in args if str(a).startswith("--extract=")))
            destination.mkdir()
            if self.behavior != "absent":
                for path, data in self.assets.items():
                    target = destination / path
                    target.parent.mkdir(parents=True, exist_ok=True)
                    target.write_bytes(data)
                asset = destination / "etc/aegis/runtime/base.ext4"
                if self.behavior == "corrupt-bytes":
                    data = asset.read_bytes()
                    asset.write_bytes(b"X" + data[1:])
                elif self.behavior == "symlink-asset":
                    asset.unlink()
                    asset.symlink_to(self.receipt)
                elif self.behavior == "extra-asset":
                    (asset.parent / "stale").write_text("old input")
                elif self.behavior == "symlink-parent":
                    (destination / "etc").rename(destination / "elsewhere")
                    (destination / "etc").symlink_to(destination / "elsewhere", target_is_directory=True)
            if self.behavior == "changed-super":
                self.super_image.write_bytes(self.super_image.read_bytes() + b"changed")
            elif self.behavior == "changed-tool":
                self.tools["lpunpack"].write_text("changed tool")
            elif self.behavior == "changed-receipt":
                self.receipt.write_bytes(self.receipt.read_bytes() + b" ")

    def verify(self, selected=True):
        return image_check.verify(self.super_image, self.tools, self.report,
                                  self.receipt if selected else None)

    def sparse(self):
        self.super_image.write_bytes(struct.pack("<I4H4I", 0xed26ff3a, 1, 0, 28, 12, 4096, 2, 1, 0))

    def assert_no_success(self):
        self.assertFalse(self.report.exists())
        self.assertFalse(list(self.root.glob(".aegis-image-check-*")))

    def test_reads_partition_from_super_and_records_selected_bytes(self):
        record = self.verify()
        self.assertEqual(record["status"], "PACKAGED_BASE_BYTES_VERIFIED_NOT_BOOTED")
        self.assertEqual(record["super.img"], image_check.fingerprint(self.super_image))
        self.assertEqual(record["selected_receipt"], image_check.fingerprint(self.receipt))
        self.assertEqual(record, json.loads(self.report.read_bytes()))
        name, args = self.calls[0]
        self.assertEqual(name, "lpunpack")
        self.assertEqual(args[:3], ["--slot=0", "--partition=system_ext_a", str(self.super_image)])
        self.assertFalse(list(self.root.glob(".aegis-image-check-*")))

    def test_sparse_super_is_converted_before_logical_partition_read(self):
        self.sparse()
        self.verify()
        self.assertEqual([name for name, _ in self.calls], ["simg2img", "lpunpack", "fsck.erofs"])
        self.assertEqual(self.calls[0][1][1], self.calls[1][1][2])

    def test_missing_and_stale_base_are_not_confused_with_success(self):
        self.behavior = "absent"
        with self.assertRaisesRegex(ValueError, "Missing or extra"):
            self.verify()
        self.assert_no_success()
        self.behavior = ""
        with self.assertRaisesRegex(ValueError, "Unselected shared base"):
            self.verify(selected=False)
        self.assert_no_success()
        self.behavior = "absent"
        self.assertEqual(self.verify(selected=False)["status"], "PACKAGED_BASE_ABSENT_NOT_BOOTED")

    def test_changed_contents_extra_files_and_symlinks_prevent_success(self):
        for behavior in ("corrupt-bytes", "extra-asset", "symlink-asset", "symlink-parent"):
            with self.subTest(behavior=behavior), self.assertRaises(ValueError):
                self.behavior = behavior
                self.verify()
            self.assert_no_success()

    def test_reader_failure_or_empty_success_cannot_produce_receipt(self):
        self.sparse()
        for behavior in ("failure-simg2img", "failure-lpunpack", "failure-fsck.erofs",
                         "empty-partition-output", "wrong-expansion", "wrong-filesystem"):
            with self.subTest(behavior=behavior), self.assertRaises((ValueError, subprocess.SubprocessError)):
                self.behavior = behavior
                self.verify()
            self.assert_no_success()

    def test_changed_source_tool_or_receipt_during_read_is_rejected(self):
        for behavior in ("changed-super", "changed-tool", "changed-receipt"):
            with self.subTest(behavior=behavior), self.assertRaisesRegex(ValueError, "changed during"):
                self.behavior = behavior
                self.verify()
            self.assert_no_success()

    def test_invalid_selection_cannot_authorize_arbitrary_paths(self):
        variants = []
        wrong = copy.deepcopy(self.selection)
        wrong["installed"]["../elsewhere"] = "runtime-base.ext4"
        variants.append(wrong)
        wrong = copy.deepcopy(self.selection)
        wrong["inputs"]["files"]["runtime-base.ext4"]["sha256"] = "f" * 64
        variants.append(wrong)
        for wrong in variants:
            self.receipt.write_bytes(image_check.generation.encoded(wrong))
            with self.assertRaises(ValueError):
                self.verify()
            self.assert_no_success()
        self.assertFalse(self.calls)

    def test_existing_report_and_symlink_input_are_preserved(self):
        self.report.write_text("earlier report")
        with self.assertRaisesRegex(ValueError, "already exists"):
            self.verify()
        self.assertEqual(self.report.read_text(), "earlier report")
        self.report.unlink()
        self.super_image.unlink()
        self.super_image.symlink_to(self.receipt)
        with self.assertRaisesRegex(ValueError, "Symlink"):
            self.verify()
        self.assert_no_success()

    def test_sparse_bomb_and_truncated_header_rejected_before_readers(self):
        for data in (struct.pack("<I", 0xed26ff3a),
                     struct.pack("<I4H4I", 0xed26ff3a, 1, 0, 28, 12, 4096, 0xffffffff, 1, 0)):
            self.super_image.write_bytes(data)
            with self.assertRaises(ValueError):
                self.verify()
            self.assert_no_success()
        self.assertFalse(self.calls)


MKFS = shutil.which("mkfs.erofs")
FSCK = shutil.which("fsck.erofs")


@unittest.skipUnless(MKFS and FSCK, "erofs-utils required; installed in Linux CI")
class InertErofsTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.source = self.root / "source"
        self.source.mkdir()
        directory = self.source / "etc/aegis/runtime"
        directory.mkdir(parents=True)
        for name in ("base.ext4", "generation.json"):
            (directory / name).write_text("inert compressed text fixture " * 100)
        self.expected = {path: image_check.fingerprint(self.source / path) for path in image_check.INSTALLED}
        self.image = self.root / "system_ext_a.img"

    def extract(self):
        subprocess.run([MKFS, "-zlz4", str(self.image), str(self.source)],
                       check=True, capture_output=True)
        destination = self.root / "extracted"
        image_check.run_tool(Path(FSCK), ["--no-preserve", "--extract=" + str(destination), self.image],
                             self.root / "reader.log", self.root)
        return destination

    def test_actual_compressed_filesystem_bytes_are_read(self):
        root = self.extract()
        image_check.check_extracted(root, self.expected)
        with self.assertRaisesRegex(ValueError, "Unselected shared base"):
            image_check.check_extracted(root, None)

    def test_actual_altered_payload_and_symlink_are_rejected(self):
        (self.source / "etc/aegis/runtime/base.ext4").write_text("different content")
        root = self.extract()
        with self.assertRaises(ValueError):
            image_check.check_extracted(root, self.expected)
        image_check.clean_temporary(root)
        asset = self.source / "etc/aegis/runtime/base.ext4"
        asset.unlink()
        asset.symlink_to("generation.json")
        root = self.extract()
        with self.assertRaisesRegex(ValueError, "Symlink"):
            image_check.check_extracted(root, self.expected)

    def test_real_reader_error_has_diagnostic_and_does_not_pass(self):
        self.image.write_bytes(b"not an EROFS filesystem")
        with self.assertRaisesRegex(ValueError, "Image reader.*failed"):
            image_check.run_tool(Path(FSCK), [str(self.image)], self.root / "reader.log", self.root)
