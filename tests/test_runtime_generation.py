"""Metadata/staging tests only. No Debian program, mount or namespace is started."""
import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import sys
import tarfile
import tempfile
import unittest
from unittest.mock import patch

from test_runtime_base import fixture, rootfs_files

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("runtime_generation", ROOT / "scripts/runtime/generation.py")
generation = importlib.util.module_from_spec(spec)
sys.path.insert(0, str(ROOT / "scripts/runtime"))
try:
    spec.loader.exec_module(generation)
finally:
    sys.path.pop(0)


def rehash(plan):
    plan = copy.deepcopy(plan)
    plan.pop("plan_sha256", None)
    plan["plan_sha256"] = hashlib.sha256(generation.encoded(plan)).hexdigest()
    return plan


def files():
    data = rootfs_files()
    data["etc/nsswitch.conf"] = b"passwd: files\ngroup: files\nshadow: files\ngshadow: files\n"
    return data


class GenerationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.layout = json.loads((ROOT / "runtime/uid-map.json").read_bytes())
        self.imported = self.root / "import"
        self.pin = None

    def plan(self, data=None, extras=()):
        self.pin, remote = fixture(files() if data is None else data, extras=extras)
        generation.base.fetch(self.pin, self.imported,
                              lambda url, path, limit: path.write_bytes(remote[url.rsplit("/", 1)[1]]))
        return generation.make_plan(self.pin, self.imported, self.layout)

    def test_generic_nss_account_is_locked_and_home_is_only_an_empty_placeholder(self):
        plan = self.plan()
        entries = generation.validate_plan(plan)
        self.assertIn("runtime:x:1000:1000:AEGIS runtime:/home/user:/bin/bash\n", entries["etc/passwd"]["text"])
        self.assertIn("root:*:", entries["etc/shadow"]["text"])
        self.assertIn("runtime:!:0:0:99999:7:::\n", entries["etc/shadow"]["text"])
        self.assertIn("runtime:x:1000:\n", entries["etc/group"]["text"])
        self.assertIn("runtime:!::\n", entries["etc/gshadow"]["text"])
        self.assertEqual(entries["home/user"], generation.directory("home/user", 1000, 1000, 0o700))
        self.assertFalse(any(name.startswith("home/user/") for name in entries))
        self.assertEqual("", entries["etc/resolv.conf"]["text"])
        self.assertEqual("", entries["etc/machine-id"]["text"])
        self.assertEqual(plan["status"], "PLANNED_NOT_BUILT")
        self.assertFalse((self.imported / "etc").exists())

    def test_planning_is_deterministic_and_configuration_covers_every_inode(self):
        plan = self.plan()
        again = generation.make_plan(self.pin, self.imported, self.layout)
        self.assertEqual(plan, again)
        rows = generation.fs_config(plan).splitlines()
        self.assertEqual(len(rows), len(plan["entries"]))
        self.assertEqual(rows[0], "/ 0 0 0755 capabilities=0")
        self.assertIn("home/user 1000 1000 0700 capabilities=0", rows)
        self.assertTrue(all(row.endswith(" capabilities=0") for row in rows))

    def test_factory_packages_are_explicit_roots_when_upstream_marks_all_automatic(self):
        data = files()
        marks = "var/lib/apt/extended_states"
        data[marks] = "".join(
            "Package: " + name + "\nArchitecture: arm64\nAuto-Installed: 1\n\n"
            for name in sorted(generation.base.REQUIRED_PACKAGES)).encode()
        plan = self.plan(data)
        original = generation.sha_file(self.imported / "rootfs.tar.gz")
        stage = self.root / "factory-roots"
        generation.materialize(plan, self.imported / "rootfs.tar.gz", stage)
        # Empty APT automatic state makes the deliberately shipped factory
        # packages manual roots, independent of Docker's upstream marks.
        self.assertEqual(b"", (stage / marks).read_bytes())
        self.assertEqual(data["var/lib/dpkg/status"],
                         (stage / "var/lib/dpkg/status").read_bytes())
        self.assertEqual(original, generation.sha_file(self.imported / "rootfs.tar.gz"))
        self.assertEqual(plan, generation.make_plan(self.pin, self.imported, self.layout))

    def test_factory_mark_replacement_rejects_linked_upstream_state(self):
        for kind in (tarfile.SYMTYPE, tarfile.LNKTYPE):
            with self.subTest(kind=kind):
                self.imported = self.root / kind.decode()
                link = tarfile.TarInfo("var/lib/apt/extended_states")
                link.type, link.linkname = kind, "var/lib/dpkg/status"
                with self.assertRaisesRegex(ValueError, "factory package marks"):
                    self.plan(extras=[link])

    def test_lost_found_is_planned_but_only_filesystem_tool_may_create_it(self):
        plan = self.plan()
        entries = generation.validate_plan(plan)
        self.assertEqual(entries["lost+found"], generation.directory("lost+found", mode=0o700))
        stage = self.root / "staged"
        generation.materialize(plan, self.imported / "rootfs.tar.gz", stage)
        self.assertFalse((stage / "lost+found").exists())
        self.assertIn("lost+found 0 0 0700 capabilities=0\n", generation.fs_config(plan))
        self.assertTrue((stage / "home/user").is_dir())

    def test_generic_account_collision_and_other_nss_authority_are_rejected(self):
        for changed in ("group-id", "group-name", "authority"):
            with self.subTest(changed=changed):
                data = files()
                if changed == "group-id":
                    data["etc/group"] += b"conflict:x:1000:\n"
                    data["etc/gshadow"] += b"conflict:*::\n"
                elif changed == "group-name":
                    data["etc/group"] += b"runtime:x:99:\n"
                    data["etc/gshadow"] += b"runtime:*::\n"
                else:
                    data["etc/nsswitch.conf"] = data["etc/nsswitch.conf"].replace(b"passwd: files", b"passwd: files ldap")
                self.imported = self.root / changed
                with self.assertRaises(ValueError):
                    self.plan(data)

    def test_private_mount_state_and_unrepresentable_paths_are_rejected(self):
        for name in ("home/someone/document", "tmp/session", "run/credential", 'usr/share/with space',
                     'usr/share/quote"file'):
            with self.subTest(name=name):
                self.imported = self.root / hashlib.sha256(name.encode()).hexdigest()[:8]
                data = files()
                data[name] = b"public negative fixture"
                with self.assertRaises(ValueError):
                    self.plan(data)

    def test_extended_archive_attributes_and_long_symlinks_need_review(self):
        attr = tarfile.TarInfo("ordinary")
        attr.pax_headers = {"SCHILY.xattr.security.capability": "fixture"}
        link = tarfile.TarInfo("long-link")
        link.type, link.linkname = tarfile.SYMTYPE, "a" * 60
        for index, entry in enumerate((attr, link)):
            self.imported = self.root / str(index)
            with self.assertRaises(ValueError):
                self.plan(extras=[entry])

    def test_staging_preserves_links_without_following_them_or_setting_host_ids(self):
        link = tarfile.TarInfo("bin")
        link.type, link.linkname = tarfile.SYMTYPE, "/usr/bin"
        hard = tarfile.TarInfo("usr/bin/bash-alias")
        hard.type, hard.linkname, hard.mode = tarfile.LNKTYPE, "usr/bin/bash", 0o644
        plan = self.plan(extras=[link, hard])
        stage = self.root / "stage"
        generation.materialize(plan, self.imported / "rootfs.tar.gz", stage)
        self.assertEqual(os.readlink(stage / "bin"), "/usr/bin")
        self.assertEqual((stage / "usr/bin/bash").stat().st_ino, (stage / "usr/bin/bash-alias").stat().st_ino)
        for path in stage.rglob("*"):
            self.assertEqual(path.lstat().st_uid, os.getuid())
            if path.is_file() and not path.is_symlink():
                self.assertEqual(stat.S_IMODE(path.stat().st_mode), 0o600)

    def test_set_id_bits_are_removed_from_file_and_directory_metadata(self):
        entry = tarfile.TarInfo("setid")
        entry.mode = 0o6755
        directory = tarfile.TarInfo("sticky")
        directory.type, directory.mode = tarfile.DIRTYPE, 0o3777
        entries = generation.validate_plan(self.plan(extras=[entry, directory]))
        self.assertEqual(entries["setid"]["mode"], 0o755)
        self.assertEqual(entries["sticky"]["mode"], 0o1777)

    def test_drift_ancestor_substitution_and_hardlink_ownership_are_rejected(self):
        plan = self.plan()
        changed = copy.deepcopy(plan)
        changed["entries"][0]["mode"] = 0o700
        with self.assertRaisesRegex(ValueError, "plan changed"):
            generation.validate_plan(changed)
        changed = copy.deepcopy(plan)
        for entry in changed["entries"]:
            if entry["path"] == "usr":
                entry.update(kind="symlink", target="/outside")
        with self.assertRaises(ValueError):
            generation.validate_plan(rehash(changed))
        hard = {"path": "alias", "kind": "hardlink", "target": "usr/bin/bash",
                "uid": 1000, "gid": 0, "mode": 0o644}
        changed = copy.deepcopy(plan)
        changed["entries"].append(hard)
        with self.assertRaises(ValueError):
            generation.validate_plan(rehash(changed))

    def test_modified_payload_and_copy_failure_never_publish_a_staging_tree(self):
        plan = self.plan()
        data = files()
        data["usr/bin/bash"] = b"changed inert bytes"
        _, remote = fixture(data)
        archive = self.root / "changed.tar.gz"
        archive.write_bytes(remote["rootfs.tar.gz"])
        stage = self.root / "stage"
        keep = self.root / "user-work"
        keep.write_text("preserve")
        with self.assertRaises(ValueError):
            generation.materialize(plan, archive, stage)
        self.assertFalse(stage.exists())
        with patch.object(generation.shutil, "copyfileobj", side_effect=OSError("fixture disk full")):
            with self.assertRaises(OSError):
                generation.materialize(plan, self.imported / "rootfs.tar.gz", stage)
        self.assertFalse(stage.exists())
        self.assertEqual(keep.read_text(), "preserve")

    def test_existing_and_symlink_destinations_are_preserved(self):
        plan = self.plan()
        stage = self.root / "stage"
        stage.mkdir()
        (stage / "keep").write_text("preserve")
        with self.assertRaises(FileExistsError):
            generation.materialize(plan, self.imported / "rootfs.tar.gz", stage)
        alias = self.root / "alias"
        alias.symlink_to(stage)
        with self.assertRaises(ValueError):
            generation.materialize(plan, self.imported / "rootfs.tar.gz", alias / "child")
        self.assertEqual((stage / "keep").read_text(), "preserve")

    def test_image_build_cannot_run_on_mac_or_an_unapproved_linux_account(self):
        with patch.object(generation.subprocess, "run", side_effect=AssertionError("Must not execute tools")):
            with patch.object(generation.platform, "system", return_value="Darwin"):
                with self.assertRaisesRegex(ValueError, "SSH builder"):
                    generation.build({}, self.root, self.root, self.root / "output")
            with patch.object(generation.platform, "system", return_value="Linux"), \
                 patch.object(generation.platform, "machine", return_value="x86_64"), \
                 patch.object(generation.pwd, "getpwuid") as identity:
                identity.return_value.pw_name = "other-account"
                with self.assertRaises(ValueError):
                    generation.require_builder()
        self.assertFalse((self.root / "output").exists())


MKFS = shutil.which("mkfs.ext4") or "/opt/homebrew/opt/e2fsprogs/sbin/mkfs.ext4"
DEBUGFS = shutil.which("debugfs") or "/opt/homebrew/opt/e2fsprogs/sbin/debugfs"


@unittest.skipUnless(Path(MKFS).is_file() and Path(DEBUGFS).is_file(), "e2fsprogs metadata tools required")
class InertFilesystemMetadataTests(unittest.TestCase):
    """A tiny text/link fixture, not a runtime image. Never mounted or executed."""
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        source = self.root / "source"
        source.mkdir(mode=0o755)
        (source / "data").write_bytes(b"inert filesystem metadata fixture\n")
        (source / "data").chmod(0o600)
        (source / "[").write_bytes(b"literal bracket filename\n")
        (source / "[").chmod(0o644)
        (source / "link").symlink_to("data")
        os.link(source / "data", source / "alias")
        self.image = self.root / "fixture.ext4"
        self.size = 16 * 1024 * 1024
        self.uuid = "11111111-2222-3333-4444-555555555555"
        with self.image.open("wb") as image:
            image.truncate(self.size)
        self.env = dict(os.environ, LC_ALL="C", LANG="C")
        subprocess.run([MKFS, "-q", "-F", "-b", "4096", "-I", "256", "-N", "128",
                        "-O", "^has_journal", "-E", "root_owner=0:0", "-U", self.uuid,
                        "-d", str(source), str(self.image)], env=self.env, check=True, capture_output=True)
        uid, gid = os.getuid(), os.getgid()
        data = (source / "data").read_bytes()
        entries = [generation.directory("."), generation.directory("lost+found", mode=0o700),
                   {"path": "data", "kind": "file", "mode": 0o600, "uid": uid, "gid": gid,
                    "size": len(data), "sha256": hashlib.sha256(data).hexdigest()},
                   {"path": "link", "kind": "symlink", "mode": stat.S_IMODE((source / "link").lstat().st_mode),
                    "uid": uid, "gid": gid, "target": "data"},
                   {"path": "alias", "kind": "hardlink", "mode": 0o600, "uid": uid, "gid": gid, "target": "data"}]
        punctuation = (source / "[").read_bytes()
        entries.append({"path": "[", "kind": "file", "mode": 0o644, "uid": uid, "gid": gid,
                        "size": len(punctuation), "sha256": hashlib.sha256(punctuation).hexdigest()})
        self.plan = rehash({"entries": entries})

    def test_reads_actual_bytes_owners_links_and_directory_inventory(self):
        generation.verify_superblock(self.image, self.size, self.uuid)
        generation.verify_image(self.plan, self.image, DEBUGFS, self.env)
        with self.assertRaises(ValueError):
            generation.verify_superblock(self.image, self.size, "00000000-0000-0000-0000-000000000000")

    def test_inode_owner_and_unplanned_file_changes_are_detected(self):
        subprocess.run([DEBUGFS, "-w", "-R", "set_inode_field /data uid 65534", str(self.image)],
                       env=self.env, check=True, capture_output=True)
        with self.assertRaisesRegex(ValueError, "inode differs"):
            generation.verify_image(self.plan, self.image, DEBUGFS, self.env)
        subprocess.run([DEBUGFS, "-w", "-R", f"set_inode_field /data uid {os.getuid()}", str(self.image)],
                       env=self.env, check=True, capture_output=True)
        extra = self.root / "extra"
        extra.write_text("public extra fixture")
        subprocess.run([DEBUGFS, "-w", "-R", f"write {extra} /extra", str(self.image)],
                       env=self.env, check=True, capture_output=True)
        with self.assertRaisesRegex(ValueError, "directory differs"):
            generation.verify_image(self.plan, self.image, DEBUGFS, self.env)

    def test_same_size_content_and_symbolic_link_changes_are_detected(self):
        changed = copy.deepcopy(self.plan)
        for entry in changed["entries"]:
            if entry["path"] == "data":
                entry["sha256"] = "0" * 64
        with self.assertRaisesRegex(ValueError, "file differs"):
            generation.verify_image(rehash(changed), self.image, DEBUGFS, self.env)
        changed = copy.deepcopy(self.plan)
        for entry in changed["entries"]:
            if entry["path"] == "link":
                entry["target"] = "elsewhere"
        with self.assertRaisesRegex(ValueError, "symlink differs"):
            generation.verify_image(rehash(changed), self.image, DEBUGFS, self.env)
