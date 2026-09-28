"""Inert transport fixtures only: no Android code is compiled or executed."""
import importlib.util
import io
import json
import os
from pathlib import Path
import stat
import subprocess
import sys
import tarfile
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("components", ROOT / "scripts/aosp/components.py")
components = importlib.util.module_from_spec(spec)
spec.loader.exec_module(components)
BUILD = "a" * 40
EXPORT = "b" * 40
RUN = "identity-20260928T130000Z-aaaaaaaa-Ab1Cd2"


def make_run(parent):
    run = parent / RUN
    run.mkdir(parents=True)
    (run / "status").write_text("IDENTITY_COMPILED_NOT_INSTALLED\n")
    (run / "project-commit.txt").write_text(BUILD + "\n")
    entries = []
    for name, mode in components.MODULES.items():
        path = run / "modules" / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(("INERT TRANSPORT FIXTURE: " + name).encode())
        path.chmod(mode)
        entries.append(f"{components.digest(path)}  {name}\n")
    (run / "SHA256SUMS").write_text("".join(entries))
    for name in components.METADATA:
        if name.endswith(".json"):
            (run / name).write_text('{"fixture": "transport only, not provenance proof"}\n')
    return run


class ComponentArchiveTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name).resolve()
        self.run = make_run(self.root)
        self.assets = self.root / "assets"

    def package(self):
        return components.package(self.run, BUILD, EXPORT, self.assets)

    def refresh_assets(self):
        (self.assets / "SHA256SUMS").write_text("".join(
            f"{components.digest(self.assets / name)}  {name}\n"
            for name in sorted(components.ASSETS - {"SHA256SUMS"})))

    def test_round_trip_reproducible_with_modes_and_no_source_changes(self):
        before = {p.relative_to(self.run): p.read_bytes() for p in self.run.rglob("*") if p.is_file()}
        self.package()
        record = components.verify(self.assets, BUILD)
        self.assertEqual(record["build_commit"], BUILD)
        self.assertEqual(record["export_commit"], EXPORT)
        self.assertEqual(record["scope"], "COMPILED_NOT_INSTALLED_OR_TESTED")
        second = self.root / "second"
        components.package(self.run, BUILD, EXPORT, second)
        for name in components.ASSETS:
            self.assertEqual((self.assets / name).read_bytes(), (second / name).read_bytes())
        output = self.root / "unpacked"
        components.unpack(self.assets, BUILD, output)
        for name, mode in components.MEMBERS.items():
            path = output / name
            source = self.run / (name if name.startswith("modules/") else name[9:])
            self.assertEqual(path.read_bytes(), source.read_bytes())
            self.assertEqual(stat.S_IMODE(path.stat().st_mode), mode)
        self.assertEqual(before, {p.relative_to(self.run): p.read_bytes()
                                 for p in self.run.rglob("*") if p.is_file()})

    def test_failed_or_incomplete_run_and_wrong_commit_cannot_be_exported(self):
        for status in ("COMPILING\n", "FAILED\n", "UPLOAD_VERIFIED\n", ""):
            with self.subTest(status=status):
                (self.run / "status").write_text(status)
                with self.assertRaises(ValueError):
                    self.package()
                self.assertFalse(self.assets.exists())
        (self.run / "status").write_text("IDENTITY_COMPILED_NOT_INSTALLED\n")
        (self.run / "project-commit.txt").write_text(EXPORT + "\n")
        with self.assertRaises(ValueError):
            self.package()
        self.assertFalse(self.assets.exists())

    def test_missing_extra_changed_or_wrong_mode_module_rejected(self):
        module = self.run / "modules/system_ext/bin/aegis"
        original = module.read_bytes()
        module.write_bytes(b"changed since the build")
        with self.assertRaisesRegex(ValueError, "checksum"):
            self.package()
        module.write_bytes(original)
        module.chmod(0o644)
        with self.assertRaisesRegex(ValueError, "mode"):
            self.package()
        module.chmod(0o755)
        extra = self.run / "modules/extra"
        extra.write_text("must not be uploaded")
        with self.assertRaises(ValueError):
            self.package()
        extra.unlink()
        module.unlink()
        with self.assertRaises(ValueError):
            self.package()
        self.assertFalse(self.assets.exists())

    def test_duplicate_and_missing_build_checksum_rejected(self):
        path = self.run / "SHA256SUMS"
        original = path.read_text()
        for text in (original + original.splitlines()[0] + "\n", "\n".join(original.splitlines()[1:])):
            path.write_text(text)
            with self.assertRaises(ValueError):
                self.package()
        self.assertFalse(self.assets.exists())

    def test_links_in_inputs_and_existing_outputs_preserved(self):
        module = self.run / "modules/system_ext/bin/aegis"
        module.unlink()
        module.symlink_to(self.run / "status")
        with self.assertRaises(ValueError):
            self.package()
        module.unlink()
        module.write_bytes(b"replacement")
        self.assets.mkdir()
        (self.assets / "keep").write_text("user data")
        with self.assertRaises(ValueError):
            self.package()
        self.assertEqual((self.assets / "keep").read_text(), "user data")
        redirected = self.root / "redirected"
        redirected.symlink_to(self.run, target_is_directory=True)
        with self.assertRaises(ValueError):
            components.package(redirected, BUILD, EXPORT, self.root / "never")

    def test_download_integrity_and_explicit_build_binding(self):
        self.package()
        with self.assertRaises(ValueError):
            components.verify(self.assets, EXPORT)
        original = (self.assets / "components.tar.gz").read_bytes()
        (self.assets / "components.tar.gz").write_bytes(original[:-10] + b"corrupted!")
        with self.assertRaises(ValueError):
            components.verify(self.assets, BUILD)
        (self.assets / "components.tar.gz").write_bytes(original)
        (self.assets / "extra").write_text("unexpected")
        with self.assertRaises(ValueError):
            components.verify(self.assets, BUILD)

    def test_invalid_archive_cannot_escape_or_leave_an_extracted_tree(self):
        self.package()
        archive = self.assets / "components.tar.gz"
        original = archive.read_bytes()
        with tarfile.open(fileobj=io.BytesIO(original), mode="r:gz") as source:
            members = [(entry, source.extractfile(entry).read()) for entry in source]
        for kind in ("traversal", "symlink", "duplicate", "content", "mode", "missing"):
            with self.subTest(kind=kind):
                with tarfile.open(archive, "w:gz") as target:
                    for index, (entry, data) in enumerate(members):
                        info = tarfile.TarInfo(entry.name)
                        info.size, info.mode = entry.size, entry.mode
                        if index == 0:
                            if kind == "traversal": info.name = "../escape"
                            if kind == "symlink":
                                info.type, info.linkname, info.size = tarfile.SYMTYPE, "../escape", 0
                            if kind == "content": data = b"X" * len(data)
                            if kind == "mode": info.mode = 0o4755
                            if kind == "missing": continue
                        target.addfile(info, io.BytesIO(data))
                        if index == 0 and kind == "duplicate":
                            target.addfile(info, io.BytesIO(data))
                # Recomputed outer sums must not bypass the member checks.
                self.refresh_assets()
                output = self.root / "unpacked"
                with self.assertRaises(ValueError):
                    components.unpack(self.assets, BUILD, output)
                self.assertFalse(output.exists())
                self.assertFalse((self.root / "escape").exists())
                self.assertFalse(list(self.root.glob(".aegis-components-*")))

    def test_manifest_shape_mode_and_size_are_bounded(self):
        self.package()
        manifest = self.assets / "components.json"
        original = manifest.read_bytes()
        name = next(iter(components.MEMBERS))
        for key, value in (("size", True), ("size", components.MAX_EXPANDED + 1),
                           ("mode", 0o4777), ("sha256", "invalid")):
            record = json.loads(original)
            record["files"][name][key] = value
            manifest.write_text(json.dumps(record))
            self.refresh_assets()
            with self.assertRaises(ValueError):
                components.verify(self.assets, BUILD)

    def test_cli_download_verifier_requires_commit_and_does_not_extract(self):
        self.package()
        spec = importlib.util.spec_from_file_location("fetch_components", ROOT / "scripts/fetch-release.py")
        fetch = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(fetch)
        self.assertEqual(fetch.verify(self.assets, "components", BUILD), 2)
        with self.assertRaises(ValueError):
            fetch.verify(self.assets, "components")
        result = subprocess.run([sys.executable, str(ROOT / "scripts/fetch-release.py"),
                                 "some-release", str(self.root / "download"),
                                 "--kind", "components", "--build-commit", "main"],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 2)
        self.assertFalse((self.root / "download").exists())


MOCK = r'''
import os, pathlib, shutil, sys
name, args = pathlib.Path(sys.argv[0]).name, sys.argv[1:]
root = pathlib.Path(os.environ['FIXTURE_ROOT'])
mode = os.environ.get('FAIL_MODE', '')
if name == 'id': print('0')
elif name == 'uname': print('Linux' if args == ['-s'] else 'x86_64')
elif name in ('chown', 'flock'): pass
elif name == 'runuser':
    split = args.index('--')
    os.execvp(args[split+1], args[split+1:])
elif name == 'python3':
    for key in ('GH_TOKEN', 'GITHUB_TOKEN', 'CREDENTIALS_DIRECTORY'):
        if key in os.environ: sys.exit('credential leaked to packager')
    os.execv(sys.executable, [sys.executable, '-B', *args])
elif name == 'gh':
    if os.environ.get('GH_TOKEN') != 'fixture-token': sys.exit('missing upload credential')
    remote = root/'remote'
    remote.mkdir(exist_ok=True)
    if args[0] == 'api':
        sys.stdout.write(pathlib.Path(os.environ['PACKAGE_TOOL']).read_text())
    elif args[1] == 'create':
        (remote/'draft').touch()
        (remote/'target').write_text(args[args.index('--target')+1])
    elif args[1] == 'upload':
        if mode == 'upload': sys.exit(5)
        source = pathlib.Path(args[3])
        shutil.copyfile(source, remote/source.name)
    elif args[1] == 'download':
        name = args[args.index('--pattern')+1]
        target = pathlib.Path(args[args.index('--dir')+1])/name
        shutil.copyfile(remote/name, target)
        if mode == 'corrupt' and name == 'components.json': target.write_text('corrupt')
    elif args[1] == 'edit':
        if mode == 'publish': sys.exit(6)
        (remote/'published').touch()
    elif args[1] == 'view':
        draft = 'true' if mode == 'readback' else 'false'
        print(draft + '\t' + args[2] + '\t' + (remote/'target').read_text())
'''


@unittest.skipUnless(sys.platform.startswith("linux"), "Export shell fixtures run in Linux CI")
class ComponentExportTests(unittest.TestCase):
    def exercise(self, mode=""):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            server = root / "server"
            run = make_run(server / "runs")
            (server / "work").mkdir()
            (root / "opt").mkdir()
            if mode == "failed_build": (run / "status").write_text("FAILED\n")
            if mode == "changed_module":
                (run / "modules/system_ext/bin/aegis").write_text("changed after compile")
            script = (ROOT / "scripts/export-components.sh").read_text()
            script = script.replace("/srv/aegis", str(server)).replace("/opt/", str(root / "opt") + "/")
            script = script.replace("/run/aegis-bootstrap.lock", str(root / "bootstrap.lock"))
            entry = root / "export.sh"
            entry.write_text(script)
            binaries = root / "bin"
            binaries.mkdir()
            for name in ("id", "uname", "chown", "flock", "runuser", "python3", "gh"):
                path = binaries / name
                path.write_text(f"#!{sys.executable}\n" + MOCK)
                path.chmod(0o755)
            env = dict(os.environ, PATH=str(binaries) + ":" + os.environ["PATH"],
                       FIXTURE_ROOT=str(root), FAIL_MODE=mode,
                       PACKAGE_TOOL=str(ROOT / "scripts/aosp/components.py"),
                       GH_TOKEN="inherited-token", GITHUB_TOKEN="inherited-token",
                       CREDENTIALS_DIRECTORY="must-not-reach-packager")
            result = subprocess.run(["bash", str(entry), "--token-stdin", EXPORT, BUILD, RUN],
                                    input="fixture-token\n", env=env, capture_output=True,
                                    text=True, timeout=30)
            self.assertNotIn("fixture-token", result.stdout + result.stderr)
            self.assertNotIn("inherited-token", result.stdout + result.stderr)
            self.assertEqual((run / "status").read_text(),
                             "FAILED\n" if mode == "failed_build" else "IDENTITY_COMPILED_NOT_INSTALLED\n")
            exports = list((server / "runs").glob("components-*"))
            if mode:
                self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertNotIn("COMPONENTS_UPLOAD_VERIFIED:", result.stdout)
                if exports:
                    self.assertEqual((exports[0] / "status").read_text(), "FAILED\n")
                if mode != "readback": self.assertFalse((root / "remote/published").exists())
            else:
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertEqual(len(exports), 1)
                self.assertEqual((exports[0] / "status").read_text(), "COMPONENTS_UPLOAD_VERIFIED\n")
                self.assertTrue((root / "remote/published").exists())
                self.assertEqual((root / "remote/target").read_text(), BUILD)
                for name in components.ASSETS:
                    self.assertEqual((exports[0] / "assets" / name).read_bytes(),
                                     (exports[0] / "verify" / name).read_bytes())

    def test_export_success_verifies_all_assets(self): self.exercise()
    def test_failed_build_never_published(self): self.exercise("failed_build")
    def test_changed_build_artifact_never_published(self): self.exercise("changed_module")
    def test_upload_failure_has_no_success_state(self): self.exercise("upload")
    def test_remote_corruption_never_published(self): self.exercise("corrupt")
    def test_publish_failure_has_no_success_state(self): self.exercise("publish")
    def test_publication_readback_failure_has_no_success_state(self): self.exercise("readback")


if __name__ == "__main__":
    unittest.main()
