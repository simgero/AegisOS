#!/usr/bin/env python3
"""Package or verify component-build transport; never install or execute components."""
import argparse
import gzip
import hashlib
import json
from pathlib import Path
import re
import shutil
import stat
import sys
import tarfile
import tempfile

PROFILE = "aegis-qemu-arm64-components-v1"
SCOPE = "COMPILED_NOT_INSTALLED_OR_TESTED"
ASSETS = {"components.tar.gz", "components.json", "SHA256SUMS"}
MODULES = {
    "system_ext/bin/aegis": 0o755,
    "system_ext/framework/aegis.jar": 0o644,
    "system_ext/framework/aegis-identity-service.jar": 0o644,
    "system_ext/lib64/libaegis_terminal_jni.so": 0o644,
    "system/framework/services.jar": 0o644,
    "system/framework/framework-res.apk": 0o644,
    "vendor/etc/passwd": 0o644,
    "vendor/etc/group": 0o644,
    "system_ext/etc/passwd": 0o644,
    "system_ext/etc/group": 0o644,
    "testcases/AegisIdentityTests/arm64/AegisIdentityTests.apk": 0o644,
    "system/bin/aegis-runtime-init": 0o755,
    "system/bin/aegis-runtime-setup": 0o755,
    "data/nativetest64/AegisRuntimeNativeTests/AegisRuntimeNativeTests": 0o755,
    "data/nativetest64/AegisRuntimeNativeTests/aegis-runtime-init": 0o755,
    "data/nativetest64/AegisRuntimeNativeTests/aegis-runtime-namespace-probe": 0o755,
    "data/nativetest64/AegisRuntimeNativeTests/aegis-runtime-setup": 0o755,
}
METADATA = (
    "project-commit.txt", "status", "SHA256SUMS", "source-files.json",
    "product-source-files.json", "runtime-storage-source.json",
)
MEMBERS = {**{"modules/" + name: mode for name, mode in MODULES.items()},
           **{"metadata/" + name: 0o644 for name in METADATA}}
MAX_FILE = 1900 * 1024 * 1024  # Keep the single GitHub asset below its 2 GiB limit.
MAX_EXPANDED = 4 * 1024 * 1024 * 1024
MAX_METADATA = 4 * 1024 * 1024


def commit_id(value):
    if not isinstance(value, str) or not re.fullmatch(r"[0-9a-f]{40}", value):
        raise ValueError("A full immutable commit is required")
    return value


def run_id(value, commit):
    pattern = rf"identity-[0-9]{{8}}T[0-9]{{6}}Z-{commit[:8]}-[A-Za-z0-9]{{6}}"
    if not isinstance(value, str) or not re.fullmatch(pattern, value):
        raise ValueError("Component run does not match the requested build commit")
    return value


def ordinary_path(path, directory=False):
    path = Path(path).absolute()
    if any(parent.is_symlink() for parent in [path, *path.parents]):
        raise ValueError("Symbolic links are not component transport inputs")
    if not (path.is_dir() if directory else path.is_file()):
        raise ValueError(f"Missing ordinary {'directory' if directory else 'file'}: {path.name}")
    return path


def digest(path):
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(chunk)
    return value.hexdigest()


def small_text(path):
    ordinary_path(path)
    if path.stat().st_size > MAX_METADATA:
        raise ValueError("Component metadata is too large")
    return path.read_text(encoding="utf-8")


def checksums(text, expected_names):
    result = {}
    for line in text.splitlines():
        match = re.fullmatch(r"([0-9a-f]{64})  ([A-Za-z0-9_./-]+)", line)
        if not match:
            raise ValueError("Invalid component checksum entry")
        value, name = match.groups()
        if name not in expected_names or name in result:
            raise ValueError("Unexpected or duplicate component checksum entry")
        result[name] = value
    if set(result) != set(expected_names):
        raise ValueError("Missing component checksum entry")
    return result


def read_record(directory, build_commit):
    commit_id(build_commit)
    directory = ordinary_path(directory, directory=True)
    if {p.name for p in directory.iterdir()} != ASSETS:
        raise ValueError("Unexpected or missing component release assets")
    hashes = checksums(small_text(directory / "SHA256SUMS"), ASSETS - {"SHA256SUMS"})
    for name, expected in hashes.items():
        path = ordinary_path(directory / name)
        if not 0 < path.stat().st_size <= MAX_FILE or digest(path) != expected:
            raise ValueError(f"Component asset checksum or size mismatch: {name}")
    record = json.loads(small_text(directory / "components.json"))
    fields = {"schema", "profile", "scope", "build_commit", "export_commit", "run", "files"}
    if (not isinstance(record, dict) or set(record) != fields
            or type(record["schema"]) is not int or record["schema"] != 1
            or record["profile"] != PROFILE or record["scope"] != SCOPE
            or record["build_commit"] != build_commit):
        raise ValueError("Unexpected component transport record or build commit")
    commit_id(record["export_commit"])
    run_id(record["run"], build_commit)
    files = record["files"]
    if not isinstance(files, dict) or set(files) != set(MEMBERS):
        raise ValueError("Component inventory does not match the transport profile")
    total = 0
    for name, entry in files.items():
        limit = MAX_METADATA if name.startswith("metadata/") else MAX_FILE
        if (not isinstance(entry, dict) or set(entry) != {"sha256", "size", "mode"}
                or type(entry["mode"]) is not int or entry["mode"] != MEMBERS[name]
                or type(entry["size"]) is not int or not 0 < entry["size"] <= limit
                or not isinstance(entry["sha256"], str)
                or not re.fullmatch(r"[0-9a-f]{64}", entry["sha256"])):
            raise ValueError("Invalid component member metadata")
        total += entry["size"]
    if total > MAX_EXPANDED:
        raise ValueError("Component archive exceeds the expanded size limit")
    return record


def read_archive(directory, record, destination=None):
    """Only our fixed regular-file inventory is read or written; no extractall."""
    seen, metadata = set(), {}
    with tarfile.open(directory / "components.tar.gz", "r|gz") as archive:
        for entry in archive:
            name = entry.name
            if name not in MEMBERS or name in seen or entry.type != tarfile.REGTYPE or entry.linkname:
                raise ValueError("Unsafe, duplicate or unexpected component archive member")
            expected = record["files"][name]
            if (entry.size != expected["size"] or entry.mode != expected["mode"]
                    or entry.uid != 0 or entry.gid != 0 or entry.pax_headers):
                raise ValueError("Component archive member metadata mismatch")
            seen.add(name)
            checksum = hashlib.sha256()
            retained = bytearray() if name.startswith("metadata/") else None
            output = None
            try:
                if destination is not None:
                    target = destination / name
                    target.parent.mkdir(parents=True, exist_ok=True)
                    output = target.open("xb")
                with archive.extractfile(entry) as source:
                    for chunk in iter(lambda: source.read(1024 * 1024), b""):
                        checksum.update(chunk)
                        if retained is not None:
                            retained.extend(chunk)
                        if output is not None:
                            output.write(chunk)
                if checksum.hexdigest() != expected["sha256"]:
                    raise ValueError(f"Component member checksum mismatch: {name}")
            finally:
                if output is not None:
                    output.close()
            if destination is not None:
                target.chmod(expected["mode"])
            if retained is not None:
                metadata[name.removeprefix("metadata/")] = retained.decode("utf-8")
    if seen != set(MEMBERS):
        raise ValueError("Missing component archive member")
    if (metadata["project-commit.txt"] != record["build_commit"] + "\n"
            or metadata["status"] != "IDENTITY_COMPILED_NOT_INSTALLED\n"):
        raise ValueError("Archived run status or commit does not match the selected build")
    hashes = checksums(metadata["SHA256SUMS"], MODULES)
    for name, value in hashes.items():
        if record["files"]["modules/" + name]["sha256"] != value:
            raise ValueError("Archive does not match the builder's module checksums")
    for name in METADATA:
        if name.endswith(".json") and not isinstance(json.loads(metadata[name]), dict):
            raise ValueError("Expected a component source receipt object")


def verify(directory, build_commit):
    directory = ordinary_path(directory, directory=True)
    record = read_record(directory, build_commit)
    read_archive(directory, record)
    return record


def new_destination(output):
    output = Path(output).absolute()
    ordinary_path(output.parent, directory=True)
    if output.exists() or output.is_symlink():
        raise ValueError("Destination must be new; existing files are preserved")
    return output


def package(run, build_commit, export_commit, output):
    commit_id(build_commit)
    commit_id(export_commit)
    run = ordinary_path(run, directory=True)
    run_id(run.name, build_commit)
    if (small_text(run / "status") != "IDENTITY_COMPILED_NOT_INSTALLED\n"
            or small_text(run / "project-commit.txt") != build_commit + "\n"):
        raise ValueError("Only the selected successfully compiled run can be packaged")
    hashes = checksums(small_text(run / "SHA256SUMS"), MODULES)
    modules = ordinary_path(run / "modules", directory=True)
    actual = set()
    for path in modules.rglob("*"):
        if path.is_symlink():
            raise ValueError("Symlinked component artifact")
        if path.is_dir():
            continue
        ordinary_path(path)
        actual.add(path.relative_to(modules).as_posix())
    if actual != set(MODULES):
        raise ValueError("Missing or unexpected files in the compiled module set")
    output = new_destination(output)
    with tempfile.TemporaryDirectory(prefix=".aegis-components-", dir=output.parent) as temporary:
        staging = Path(temporary)
        tree, assets = staging / "tree", staging / "assets"
        assets.mkdir()
        record = {"schema": 1, "profile": PROFILE, "scope": SCOPE,
                  "build_commit": build_commit, "export_commit": export_commit,
                  "run": run.name, "files": {}}
        total = 0
        for name, mode in sorted(MEMBERS.items()):
            source = run / (name if name.startswith("modules/") else name[9:])
            ordinary_path(source)
            limit = MAX_METADATA if name.startswith("metadata/") else MAX_FILE
            if not 0 < source.stat().st_size <= limit:
                raise ValueError("Component input exceeds its size limit")
            total += source.stat().st_size
            if total > MAX_EXPANDED:
                raise ValueError("Component inputs exceed the expanded size limit")
            if name.startswith("modules/") and stat.S_IMODE(source.stat().st_mode) != mode:
                raise ValueError("Compiled component mode differs from its installation mode")
            target = tree / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
            value = digest(target)
            if name.startswith("modules/") and value != hashes[name[8:]]:
                raise ValueError("Compiled component differs from its build checksum")
            record["files"][name] = {"sha256": value, "size": target.stat().st_size, "mode": mode}
        with (assets / "components.tar.gz").open("xb") as raw:
            with gzip.GzipFile(filename="", mode="wb", fileobj=raw, mtime=0, compresslevel=1) as compressed:
                with tarfile.open(fileobj=compressed, mode="w|", format=tarfile.USTAR_FORMAT) as archive:
                    for name, entry in sorted(record["files"].items()):
                        info = tarfile.TarInfo(name)
                        info.size, info.mode = entry["size"], entry["mode"]
                        with (tree / name).open("rb") as source:
                            archive.addfile(info, source)
        (assets / "components.json").write_text(json.dumps(record, sort_keys=True, indent=2) + "\n")
        (assets / "SHA256SUMS").write_text("".join(
            f"{digest(assets / name)}  {name}\n" for name in sorted(ASSETS - {"SHA256SUMS"})))
        verify(assets, build_commit)
        new_destination(output)
        assets.rename(output)
    return output


def unpack(directory, build_commit, output):
    directory = ordinary_path(directory, directory=True)
    record = read_record(directory, build_commit)
    output = new_destination(output)
    with tempfile.TemporaryDirectory(prefix=".aegis-components-", dir=output.parent) as temporary:
        target = Path(temporary) / "components"
        target.mkdir()
        read_archive(directory, record, target)
        new_destination(output)
        target.rename(output)
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="action", required=True)
    for action in ("package", "verify", "unpack"):
        child = subparsers.add_parser(action)
        child.add_argument("directory", type=Path)
        child.add_argument("--build-commit", required=True)
        if action != "verify":
            child.add_argument("--output", type=Path, required=True)
        if action == "package":
            child.add_argument("--export-commit", required=True)
    args = parser.parse_args()
    if args.action == "package":
        if not sys.platform.startswith("linux"):
            raise ValueError("Package real build outputs only on the Linux builder")
        package(args.directory, args.build_commit, args.export_commit, args.output)
    elif args.action == "verify":
        verify(args.directory, args.build_commit)
    else:
        unpack(args.directory, args.build_commit, args.output)
    print("Component transport verified; no installation, execution, boot or isolation test.")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, tarfile.TarError, EOFError) as error:
        print(f"Components NOT verified: {error}", file=sys.stderr)
        sys.exit(1)
