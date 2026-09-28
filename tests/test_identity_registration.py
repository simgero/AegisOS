import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("identity_registration",
                                             ROOT / "scripts/aosp/register-identity.py")
registration = importlib.util.module_from_spec(spec)
spec.loader.exec_module(registration)


class IdentityRegistrationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / "source"
        (self.source / "src/example").mkdir(parents=True)
        (self.source / "Android.bp").write_text("module")
        (self.source / "src/example/Main.java").write_text("first")
        self.aosp = self.root / "aosp"
        (self.aosp / "build").mkdir(parents=True)
        (self.aosp / "build/envsetup.sh").touch()
        self.target = self.aosp / "packages/aegis/identity"

    def test_update_keeps_previous_tree_outside_source_discovery(self):
        registration.register(self.source, self.aosp)
        (self.source / "src/example/Main.java").write_text("second")
        registration.register(self.source, self.aosp)
        self.assertEqual((self.target / "src/example/Main.java").read_text(), "second")
        backups = list((self.aosp / "out/aegis-identity-backups").iterdir())
        self.assertEqual(len(backups), 1)
        self.assertEqual((backups[0] / "src/example/Main.java").read_text(), "first")
        self.assertEqual(len(list((self.aosp / "packages").rglob("Android.bp"))), 1)

    def test_modified_or_extra_destination_files_are_preserved(self):
        registration.register(self.source, self.aosp)
        java = self.target / "src/example/Main.java"
        java.write_text("builder's local work")
        with self.assertRaises(ValueError): registration.register(self.source, self.aosp)
        self.assertEqual(java.read_text(), "builder's local work")
        java.write_text("first")
        (self.target / "local-notes").write_text("keep")
        with self.assertRaises(ValueError): registration.register(self.source, self.aosp)
        self.assertEqual((self.target / "local-notes").read_text(), "keep")

    def test_unmanaged_destination_is_preserved(self):
        self.target.mkdir(parents=True)
        (self.target / "work").write_text("keep")
        with self.assertRaises(ValueError): registration.register(self.source, self.aosp)
        self.assertEqual((self.target / "work").read_text(), "keep")

    def test_source_symlink_is_rejected_before_destination_changes(self):
        external = self.root / "external"
        external.write_text("outside")
        (self.source / "src/secret").symlink_to(external)
        with self.assertRaises(ValueError): registration.register(self.source, self.aosp)
        self.assertFalse(self.target.exists())

    def test_destination_ancestor_symlink_cannot_redirect_writes(self):
        outside = self.root / "outside"
        outside.mkdir()
        (self.aosp / "packages").symlink_to(outside)
        with self.assertRaises(ValueError): registration.register(self.source, self.aosp)
        self.assertEqual(list(outside.iterdir()), [])

    def test_corrupt_or_symlinked_marker_does_not_authorize_replacement(self):
        registration.register(self.source, self.aosp)
        marker = self.target / registration.MARKER
        marker.write_text(json.dumps({"version": 1, "files": {}}))
        with self.assertRaises(ValueError): registration.register(self.source, self.aosp)
        marker.unlink()
        marker.symlink_to(self.source / "Android.bp")
        with self.assertRaises(ValueError): registration.register(self.source, self.aosp)
        self.assertEqual((self.target / "src/example/Main.java").read_text(), "first")
