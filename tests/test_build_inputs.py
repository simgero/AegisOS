import base64
import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("build_inputs",
                                             ROOT / "scripts/aosp/fetch-build-inputs.py")
inputs = importlib.util.module_from_spec(spec)
spec.loader.exec_module(inputs)
COMMIT = "1" * 40


class GitHubFixture:
    def __init__(self):
        self.files = {name: (name + "\n").encode() for name in inputs.REQUIRED}
        self.files["device/aegis/qemu_arm64/overlay/res/values/config.xml"] = b"<resources />"
        self.files["packages/aegis/identity/src/org/aegisos/identity/Service.java"] = b"fixture"
        self.files["unrelated/local-notes.txt"] = b"must not be copied"
        self.blobs = {}
        self.tree = {"truncated": False, "tree": []}
        self.requests = []
        for name, data in self.files.items():
            sha = inputs.object_id(data)
            self.tree["tree"].append({"path": name, "type": "blob", "mode": "100644",
                                      "sha": sha, "size": len(data)})
            self.blobs[sha] = {"sha": sha, "encoding": "base64", "size": len(data),
                               "content": base64.b64encode(data).decode()}

    def __call__(self, endpoint):
        self.requests.append(endpoint)
        if endpoint == f"{inputs.REPOSITORY}/git/trees/{COMMIT}?recursive=1":
            return copy.deepcopy(self.tree)
        prefix = f"{inputs.REPOSITORY}/git/blobs/"
        if endpoint.startswith(prefix):
            return copy.deepcopy(self.blobs[endpoint[len(prefix):]])
        raise AssertionError("Unexpected API request")


class BuildInputTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.target = self.root / COMMIT
        self.github = GitHubFixture()

    def fetch(self):
        return inputs.fetch(COMMIT, self.target, self.github)

    def test_nested_sources_are_verified_and_reused_without_redownload(self):
        self.fetch()
        for name, data in self.github.files.items():
            if name.startswith(inputs.PREFIXES):
                self.assertEqual((self.target / name).read_bytes(), data)
            else:
                self.assertFalse((self.target / name).exists())
        record = json.loads((self.target / inputs.MARKER).read_text())
        self.assertEqual(record["commit"], COMMIT)
        self.github.requests.clear()
        self.fetch()
        self.assertEqual(len(self.github.requests), 1)  # Recheck pinned tree, reuse local blobs.

    def test_corrupt_blob_never_becomes_an_installable_snapshot(self):
        name = "scripts/aosp/worker.sh"
        blob = self.github.blobs[inputs.object_id(self.github.files[name])]
        blob["content"] = base64.b64encode(b"wrong bytes").decode()
        with self.assertRaises(ValueError): self.fetch()
        self.assertFalse(self.target.exists())
        self.assertEqual(list(self.root.iterdir()), [])

    def test_interrupted_download_discards_only_its_own_staging_directory(self):
        unrelated = self.root / "keep"
        unrelated.write_bytes(b"preserve")
        requests = 0

        def interrupted(endpoint):
            nonlocal requests
            requests += 1
            if requests == 4: raise RuntimeError("simulated connection loss")
            return self.github(endpoint)

        with self.assertRaises(RuntimeError): inputs.fetch(COMMIT, self.target, interrupted)
        self.assertFalse(self.target.exists())
        self.assertEqual(list(self.root.iterdir()), [unrelated])
        self.assertEqual(unrelated.read_bytes(), b"preserve")

    def test_missing_or_truncated_tree_is_rejected_before_writes(self):
        for truncate in (True, False):
            with self.subTest(truncate=truncate):
                self.github = GitHubFixture()
                if truncate:
                    self.github.tree["truncated"] = True
                else:
                    self.github.tree["tree"] = [e for e in self.github.tree["tree"]
                                                 if e["path"] != "scripts/aosp/compile.sh"]
                with self.assertRaises(ValueError): self.fetch()
                self.assertEqual(list(self.root.iterdir()), [])
                self.assertEqual(len(self.github.requests), 1)

    def test_unsafe_paths_links_submodules_and_oversize_files_are_rejected(self):
        changes = [
            {"path": "scripts/aosp/../../outside"},
            {"path": "scripts/aosp/./worker.sh"},
            {"path": "scripts/aosp//worker.sh"},
            {"mode": "120000"},
            {"mode": "160000", "type": "commit"},
            {"size": inputs.MAX_FILE_BYTES + 1},
        ]
        for change in changes:
            with self.subTest(change=change):
                self.github = GitHubFixture()
                entry = next(e for e in self.github.tree["tree"]
                             if e["path"] == "scripts/aosp/worker.sh")
                entry.update(change)
                with self.assertRaises(ValueError): self.fetch()
                self.assertEqual(list(self.root.iterdir()), [])

    def test_changed_snapshot_is_preserved_and_not_overwritten(self):
        self.fetch()
        file = self.target / "scripts/aosp/worker.sh"
        file.write_bytes(b"local changes")
        with self.assertRaises(ValueError): self.fetch()
        self.assertEqual(file.read_bytes(), b"local changes")

    def test_changed_permissions_or_extra_file_prevent_reuse(self):
        self.fetch()
        file = self.target / "scripts/aosp/worker.sh"
        file.chmod(0o666)
        with self.assertRaises(ValueError): self.fetch()
        self.assertEqual(file.stat().st_mode & 0o777, 0o666)
        file.chmod(0o644)
        (self.target / "unmanaged").write_bytes(b"keep")
        with self.assertRaises(ValueError): self.fetch()
        self.assertEqual((self.target / "unmanaged").read_bytes(), b"keep")

    def test_symlinked_destination_cannot_redirect_snapshot(self):
        outside = self.root / "outside"
        outside.mkdir()
        self.target.symlink_to(outside)
        with self.assertRaises(ValueError): self.fetch()
        self.assertEqual(list(outside.iterdir()), [])

    def test_mutable_ref_is_rejected_before_network_access(self):
        with self.assertRaises(ValueError): inputs.fetch("main", self.target, self.github)
        self.assertEqual(self.github.requests, [])
