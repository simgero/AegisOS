"""Host-only inert archives and metadata, never Android/native execution."""
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("prepare_local_images", ROOT / "scripts/prepare-local-images.py")
prepare = importlib.util.module_from_spec(spec)
spec.loader.exec_module(prepare)
COMMIT = "a" * 40


class PreparationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.download = self.root / "download"
        self.download.mkdir()
        self.output = self.root / "prepared"

    def archive(self, extra=(), missing=()):
        data = io.BytesIO()
        with tarfile.open(fileobj=data, mode="w:xz") as archive:
            for name in sorted(prepare.REQUIRED - set(missing)):
                member = tarfile.TarInfo("./" + name)
                member.size = 7
                member.mode = 0o4755  # Must never be restored on the host.
                archive.addfile(member, io.BytesIO(b"fixture"))
            for member in extra:
                archive.addfile(member, io.BytesIO(b"X" * member.size))
        content = data.getvalue()
        # Split inside arbitrary compressed bytes, not at archive boundaries.
        cut = len(content) // 2
        for i, part in enumerate((content[:cut], content[cut:])):
            (self.download / f"images.tar.xz.part-{i:04d}").write_bytes(part)
        (self.download / "builder-commit.txt").write_text(COMMIT + "\n")
        self.sums()

    def sums(self):
        manifest = ""
        for path in sorted(self.download.iterdir()):
            if path.name != "SHA256SUMS":
                manifest += hashlib.sha256(path.read_bytes()).hexdigest() + "  " + path.name + "\n"
        (self.download / "SHA256SUMS").write_text(manifest)

    def run_prepare(self):
        return prepare.prepare(self.download, self.output, COMMIT)

    def test_contiguous_parts_preserve_bytes_without_archive_permissions(self):
        self.archive()
        report = self.run_prepare()
        self.assertEqual(set(report["files"]), prepare.REQUIRED)
        self.assertEqual(report["status"], "EXTRACTED_CHECKED_NOT_AVB_VERIFIED_NOT_BOOTED")
        for path in (self.output / "images").iterdir():
            self.assertEqual(path.read_bytes(), b"fixture")
            self.assertEqual(path.stat().st_mode & 0o7777, 0o600)
        self.assertEqual(json.loads((self.output / "prepared.json").read_text()), report)
        with self.assertRaises(ValueError):
            self.run_prepare()

    def test_aosp_hidden_build_metadata_is_ordinary_nonexecutable_data(self):
        hidden = tarfile.TarInfo("./.copied_headers_list")
        hidden.size = 2
        self.archive([hidden])
        self.run_prepare()
        self.assertEqual((self.output / "images/.copied_headers_list").read_bytes(), b"XX")

    def test_wrong_commit_or_corrupt_asset_creates_no_output(self):
        self.archive()
        with self.assertRaises(ValueError):
            prepare.prepare(self.download, self.output, "b" * 40)
        self.assertFalse(self.output.exists())
        (self.download / "images.tar.xz.part-0001").write_bytes(b"corrupt")
        with self.assertRaises(ValueError): self.run_prepare()
        self.assertFalse(self.output.exists())

    def test_path_escape_and_duplicate_are_never_extracted(self):
        for name in ("../outside", "/outside", "nested/file", "./boot.img", ".", ".."):
            with self.subTest(name=name), tempfile.TemporaryDirectory() as tmp:
                self.output = Path(tmp).resolve() / "prepared"
                extra = tarfile.TarInfo(name)
                extra.size = 1
                self.archive([extra])
                with self.assertRaises(ValueError): self.run_prepare()
                self.assertFalse((self.output / "prepared.json").exists())
        self.assertFalse((self.root / "outside").exists())

    def test_links_devices_and_directories_are_rejected(self):
        for kind in (tarfile.SYMTYPE, tarfile.LNKTYPE, tarfile.CHRTYPE, tarfile.DIRTYPE):
            with self.subTest(kind=kind), tempfile.TemporaryDirectory() as tmp:
                self.output = Path(tmp).resolve() / "prepared"
                extra = tarfile.TarInfo("unexpected")
                extra.type = kind
                extra.linkname = "boot.img"
                self.archive([extra])
                with self.assertRaises(ValueError): self.run_prepare()
                self.assertFalse((self.output / "prepared.json").exists())

    def test_missing_image_has_no_success_receipt(self):
        self.archive(missing=["super.img"])
        with self.assertRaises(ValueError): self.run_prepare()
        self.assertFalse((self.output / "prepared.json").exists())

    def test_archive_byte_budget_is_enforced(self):
        self.archive()
        with patch.object(prepare, "MAX_TOTAL", 8):
            with self.assertRaises(ValueError): self.run_prepare()
        self.assertFalse((self.output / "prepared.json").exists())

    def test_changed_manifest_cannot_publish_success(self):
        self.archive()
        original = prepare.check_receipts
        def change_manifest(*args):
            # Still individually valid checksums, but another provenance claim.
            (self.download / "builder-commit.txt").write_text("b" * 40 + "\n")
            self.sums()
            return original(*args)
        with patch.object(prepare, "check_receipts", side_effect=change_manifest):
            with self.assertRaises(ValueError): self.run_prepare()
        self.assertFalse((self.output / "prepared.json").exists())

    def test_foreign_super_image_or_selected_receipt_is_rejected(self):
        self.archive()
        files = {name: {"size": 7, "sha256": hashlib.sha256(b"fixture").hexdigest()}
                 for name in prepare.REQUIRED}
        inputs = {"status": "CHECKED_BASE_INPUTS_NOT_MOUNTED", "generation": "c" * 64,
                  "files": {"runtime-base.ext4": files["super.img"], "generation.json": files["super.img"]}}
        canonical = (json.dumps(inputs, sort_keys=True, separators=(",", ":")) + "\n").encode()
        record = {"inputs": inputs, "bundle_id": hashlib.sha256(canonical).hexdigest(),
                  "installed": {"etc/aegis/runtime/base.ext4": "runtime-base.ext4",
                                "etc/aegis/runtime/generation.json": "generation.json"}}
        base = self.download / "runtime-base-inputs.json"
        base.write_text(json.dumps(record))
        report = {"status": "PACKAGED_BASE_BYTES_VERIFIED_NOT_BOOTED", "super.img": files["super.img"],
                  "selected_receipt": prepare.fingerprint(base), "metadata_slot": 0,
                  "partition": "system_ext_a", "filesystem": "erofs",
                  "installed": {k: inputs["files"][v] for k, v in record["installed"].items()}}
        image_report = self.download / "runtime-base-image.json"
        image_report.write_text(json.dumps(report))
        self.assertEqual(prepare.check_receipts(self.download, self.output, files)["base_generation"], "c" * 64)
        for key in ("super.img", "selected_receipt"):
            changed = dict(report)
            changed[key] = {"size": 7, "sha256": "d" * 64}
            image_report.write_text(json.dumps(changed))
            with self.assertRaises(ValueError): prepare.check_receipts(self.download, self.output, files)

    def test_kernel_payload_and_configuration_must_match_receipt(self):
        # Reuse inert headers/configuration fixtures, not real executable code.
        fixture_spec = importlib.util.spec_from_file_location(
            "local_kernel_fixtures", ROOT / "tests/test_kernel_integration.py")
        fixture = importlib.util.module_from_spec(fixture_spec)
        fixture_spec.loader.exec_module(fixture)
        images = self.root / "images"
        images.mkdir()
        def install(config=None):
            payload = fixture.image(config)
            (images / "kernel").write_bytes(payload)
            (images / "boot.img").write_bytes(fixture.boot_image(payload))
            files = {name: prepare.fingerprint(images / name) for name in ("kernel", "boot.img")}
            inputs = {"status": "CHECKED_INPUTS_NOT_BOOTED", "kernel_release": fixture.RELEASE,
                      "files": {"gki/kernel-6.12": files["kernel"]}}
            bundle = hashlib.sha256(json.dumps(inputs, sort_keys=True, separators=(",", ":")).encode()).hexdigest()
            (self.download / "kernel-inputs.json").write_text(json.dumps({"inputs": inputs, "bundle_id": bundle}))
            return files, bundle
        files, bundle = install()
        self.assertEqual(prepare.check_receipts(self.download, images, files)["kernel_bundle"], bundle)
        (images / "boot.img").write_bytes(fixture.boot_image(fixture.image() + b"different"))
        with self.assertRaisesRegex(ValueError, "different kernel"):
            prepare.check_receipts(self.download, images, files)
        files, _ = install(fixture.config_bytes().replace(b"CONFIG_USER_NS=y", b"CONFIG_USER_NS=n"))
        with self.assertRaisesRegex(ValueError, "configuration"):
            prepare.check_receipts(self.download, images, files)


if __name__ == "__main__":
    unittest.main()
