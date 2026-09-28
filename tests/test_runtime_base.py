import copy
import gzip
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
spec = importlib.util.spec_from_file_location("runtime_base", ROOT / "scripts/runtime/base.py")
base = importlib.util.module_from_spec(spec)
spec.loader.exec_module(base)


def encoded(value):
    return json.dumps(value, separators=(",", ":")).encode()


def digest(data):
    return "sha256:" + hashlib.sha256(data).hexdigest()


def rootfs_files():
    # Public structural fixtures, never executable programs or real credentials.
    elf = b"\x7fELF\x02\x01\x01" + bytes(11) + (183).to_bytes(2, "little") + bytes(44)
    files = {name: elf for name in base.ELF_FILES}
    files.update({
        "etc/debian_version": b"13.7\n",
        "usr/lib/os-release": b"ID=debian\nVERSION_CODENAME=trixie\n",
        "etc/passwd": b"root:x:0:0:root:/root:/bin/bash\n",
        "etc/shadow": b"root:*:20000:0:99999:7:::\n",
        "etc/group": b"root:x:0:\n",
        "etc/gshadow": b"root:*::\n",
        "etc/apt/sources.list.d/debian.sources":
            b"Signed-By: /usr/share/keyrings/debian-archive-keyring.pgp\n",
        base.KEYRING: b"non-executable keyring fixture",
        "var/lib/dpkg/status": "\n\n".join(
            "Package: " + name + "\nStatus: install ok installed\nArchitecture: arm64\nVersion: 1.0"
            for name in sorted(base.REQUIRED_PACKAGES)).encode() + b"\n",
    })
    return files


def fixture(files=None, extras=(), config_changes=None):
    buffer = io.BytesIO()
    with tarfile.open(fileobj=buffer, mode="w") as archive:
        for name, data in (rootfs_files() if files is None else files).items():
            member = tarfile.TarInfo(name)
            member.size = len(data)
            archive.addfile(member, io.BytesIO(data))
        for member in extras:
            archive.addfile(member)
    plain = buffer.getvalue()
    layer = gzip.compress(plain, mtime=0)
    config = {"os": "linux", "architecture": "arm64", "variant": "v8",
              "rootfs": {"type": "layers", "diff_ids": [digest(plain)]}}
    config.update(config_changes or {})
    config_data = encoded(config)
    manifest = {
        "schemaVersion": 2, "mediaType": "application/vnd.oci.image.manifest.v1+json",
        "config": {"mediaType": "application/vnd.oci.image.config.v1+json",
                   "digest": digest(config_data), "size": len(config_data)},
        "layers": [{"mediaType": "application/vnd.oci.image.layer.v1.tar+gzip",
                    "digest": digest(layer), "size": len(layer)}],
    }
    data = {"image-manifest.json": encoded(manifest), "image-config.json": config_data,
            "rootfs.tar.gz": layer}
    pin = json.loads((ROOT / "runtime/debian-arm64.json").read_bytes())
    pin["manifest"] = {"sha256": digest(data["image-manifest.json"])[7:],
                       "size": len(data["image-manifest.json"])}
    return pin, data


class RuntimeBaseTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.parent = Path(self.temp.name).resolve()
        self.destination = self.parent / "base"

    def fetch(self, pin, remote):
        def download(url, destination, limit):
            self.assertTrue(url.startswith("https://raw.githubusercontent.com/"
                                           + base.REPOSITORY + "/" + pin["commit"] + "/"))
            data = remote[url.rsplit("/", 1)[1]]
            destination.write_bytes(data)
        return base.fetch(pin, self.destination, download)

    def assert_import_rejected(self, pin, remote):
        with self.assertRaises(ValueError):
            self.fetch(pin, remote)
        self.assertFalse(self.destination.exists())
        self.assertEqual(list(self.parent.iterdir()), [])

    def test_verified_base_is_atomic_reusable_and_never_extracted(self):
        pin, remote = fixture()
        report = self.fetch(pin, remote)
        self.assertEqual(report["status"], "VERIFIED_UPSTREAM_BASE_NOT_INSTALLED")
        self.assertEqual(set(report["packages"]), base.REQUIRED_PACKAGES)
        self.assertEqual(set(p.name for p in self.destination.iterdir()), base.FILES | {base.RECEIPT})
        self.assertFalse((self.destination / "etc").exists())
        with patch.object(base.subprocess, "run", side_effect=AssertionError("No subprocess on reuse")):
            self.assertEqual(base.fetch(pin, self.destination), report)

    def test_corrupt_download_is_not_published(self):
        for name in base.FILES:
            with self.subTest(name=name):
                pin, remote = fixture()
                remote[name] = b"!" + remote[name][1:]
                self.assert_import_rejected(pin, remote)

    def test_platform_and_actual_elf_must_both_be_arm64(self):
        pin, remote = fixture(config_changes={"architecture": "amd64"})
        self.assert_import_rejected(pin, remote)
        files = rootfs_files()
        header = bytearray(files["usr/bin/bash"])
        header[18:20] = (62).to_bytes(2, "little")
        files["usr/bin/bash"] = bytes(header)
        self.assert_import_rejected(*fixture(files))

    def test_compressed_hash_does_not_replace_uncompressed_layer_check(self):
        self.assert_import_rejected(*fixture(config_changes={
            "rootfs": {"type": "layers", "diff_ids": ["sha256:" + "0" * 64]}}))

    def test_linux_credentials_and_personal_accounts_are_rejected(self):
        for replacement in (b"root::20000:0:99999:7:::\n",
                            b"root:public-test-hash:20000:0:99999:7:::\n"):
            files = rootfs_files()
            files["etc/shadow"] = replacement
            self.assert_import_rejected(*fixture(files))
        files = rootfs_files()
        files["etc/passwd"] += b"person:x:1000:1000::/home/person:/bin/bash\n"
        files["etc/shadow"] += b"person:*:20000:0:99999:7:::\n"
        self.assert_import_rejected(*fixture(files))

    def test_unsafe_members_hardlinks_and_parent_substitutions_are_rejected(self):
        bad = [tarfile.TarInfo("../outside"), tarfile.TarInfo("/absolute"),
               tarfile.TarInfo("usr/.wh.lib"), tarfile.TarInfo("etc/passwd")]
        link = tarfile.TarInfo("other")
        link.type, link.linkname = tarfile.LNKTYPE, "../outside"
        bad.append(link)
        link = tarfile.TarInfo("usr")
        link.type, link.linkname = tarfile.SYMTYPE, "elsewhere"
        bad.append(link)
        device = tarfile.TarInfo("dev/foreign")
        device.type = tarfile.CHRTYPE
        bad.append(device)
        for member in bad:
            with self.subTest(name=member.name):
                self.assert_import_rejected(*fixture(extras=[member]))

    def test_missing_incomplete_or_foreign_packages_are_rejected(self):
        for change in (lambda f: f.pop("usr/bin/apt"),
                       lambda f: f.update({"var/lib/dpkg/status":
                           f["var/lib/dpkg/status"].replace(b"install ok installed", b"install ok unpacked", 1)}),
                       lambda f: f.update({"var/lib/dpkg/status":
                           f["var/lib/dpkg/status"].replace(b"Architecture: arm64", b"Architecture: amd64", 1)})):
            files = rootfs_files()
            change(files)
            self.assert_import_rejected(*fixture(files))

    def test_debian_control_fields_can_start_with_empty_continuation_headers(self):
        files = rootfs_files()
        files["var/lib/dpkg/status"] += b"Conffiles:\n /etc/example public-test-checksum\n"
        pin, remote = fixture(files)
        report = self.fetch(pin, remote)
        self.assertEqual(set(report["packages"]), base.REQUIRED_PACKAGES)

    def test_expansion_limit_is_enforced_before_tar_inspection(self):
        with patch.object(base, "MAX_EXPANDED", 1024):
            self.assert_import_rejected(*fixture())

    def test_interrupted_download_preserves_other_work(self):
        pin, remote = fixture()
        keep = self.parent / "user-work"
        keep.write_text("keep")

        def interrupt(url, destination, limit):
            destination.write_bytes(b"partial download")
            raise RuntimeError("simulated interrupted connection")

        with self.assertRaises(RuntimeError):
            base.fetch(pin, self.destination, interrupt)
        self.assertFalse(self.destination.exists())
        self.assertEqual(list(self.parent.iterdir()), [keep])
        self.assertEqual(keep.read_text(), "keep")

    def test_existing_modified_import_is_preserved(self):
        pin, remote = fixture()
        self.fetch(pin, remote)
        modified = self.destination / "rootfs.tar.gz"
        modified.write_bytes(b"user edit")
        with self.assertRaises(ValueError):
            self.fetch(pin, remote)
        self.assertEqual(modified.read_bytes(), b"user edit")

    def test_changed_report_and_symlink_destinations_are_rejected(self):
        pin, remote = fixture()
        self.fetch(pin, remote)
        (self.destination / base.RECEIPT).write_text('{}')
        with self.assertRaises(ValueError):
            base.verify(pin, self.destination)
        link = self.parent / "alias"
        link.symlink_to(self.destination)
        with self.assertRaises(ValueError):
            base.fetch(pin, link)

    def test_floating_ref_or_external_source_fails_before_download(self):
        original, _ = fixture()
        for field, value in (("commit", "main"), ("repository", "someone/other"),
                             ("directory", "../../escape")):
            pin = copy.deepcopy(original)
            pin[field] = value
            with self.assertRaises(ValueError):
                base.fetch(pin, self.destination,
                           lambda *_: self.fail("Invalid source must not be contacted"))
        with self.assertRaises(ValueError):
            base.read_json(b'{"schema":1,"schema":2}')
