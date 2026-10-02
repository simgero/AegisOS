#!/usr/bin/env python3
"""Fetch a checked, immutable build-input snapshot from the pinned GitHub commit."""
import argparse
import base64
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import subprocess
import tempfile

REPOSITORY = "repos/simgero/AegisOS"
PREFIXES = ("scripts/aosp/", "device/aegis/qemu_arm64/", "packages/aegis/identity/",
            "scripts/runtime/", "runtime/", "scripts/kernel/", "kernel/")
REQUIRED = {
    "scripts/aosp/worker.sh", "scripts/aosp/config.sh", "scripts/aosp/compile.sh",
    "scripts/aosp/setup-sandbox.sh", "scripts/aosp/link-product.py",
    "scripts/aosp/register-identity.py",
    "scripts/aosp/register-qemu-graphics.py",
    "scripts/aosp/register-egl-cache.py",
    "scripts/aosp/check-memory.py",
    "scripts/aosp/check-audio.py",
    "scripts/aosp/register-runtime-storage.py", "runtime/aosp-storage-hooks.json",
    "packages/aegis/identity/platform/com/android/server/aegis/AegisRuntimeStorage.java",
    "packages/aegis/identity/platform/com/android/server/aegis/AegisRemovalFiles.java",
    "packages/aegis/identity/platform/com/android/server/aegis/AegisRemovalData.java",
    "device/aegis/qemu_arm64/AndroidProducts.mk",
    "device/aegis/qemu_arm64/aegis_qemu_arm64.mk",
    "device/aegis/qemu_arm64/BoardConfig.mk", "packages/aegis/identity/Android.bp",
    "packages/aegis/identity/cli/aegis",
    "scripts/runtime/base.py", "runtime/debian-arm64.json",
    "scripts/runtime/generation.py", "scripts/runtime/build-base.sh", "runtime/filesystem-tools.json",
    "scripts/runtime/integrate.py", "scripts/runtime/verify_product_image.py",
    "scripts/runtime/uid_layout.py", "runtime/uid-map.json", "runtime/aosp-id-reference.json",
    "device/aegis/qemu_arm64/runtime-ids.fs",
    "scripts/kernel/integrate.py", "scripts/kernel/source_manifest.py",
    "kernel/manifest.xml", "kernel/aegis_runtime_defconfig",
}
MARKER = ".aegis-build-inputs.json"
MAX_FILE_BYTES = 8 * 1024 * 1024
MAX_TOTAL_BYTES = 64 * 1024 * 1024


def api(endpoint):
    response = subprocess.run(["gh", "api", "--hostname", "github.com", endpoint],
                              capture_output=True, check=False)
    if response.returncode:
        # Do not echo API bodies, credentials, or untrusted error output.
        raise RuntimeError(f"GitHub build-input request failed (exit {response.returncode})")
    return json.loads(response.stdout)


def object_id(data):
    return hashlib.sha1(b"blob " + str(len(data)).encode("ascii") + b"\0" + data).hexdigest()


def selected_files(tree):
    if tree.get("truncated") is not False:
        raise ValueError("GitHub did not return a complete source tree")
    files = {}
    for entry in tree["tree"]:
        name = entry["path"]
        if not name.startswith(PREFIXES):
            continue
        path = PurePosixPath(name)
        if (not re.fullmatch(r"[A-Za-z0-9_./-]+", name) or path.is_absolute()
                or path.as_posix() != name or ".." in path.parts):
            raise ValueError("Non-portable or escaping build-input path")
        if entry["type"] == "tree" and entry["mode"] == "040000":
            continue
        if entry["type"] != "blob" or entry["mode"] not in ("100644", "100755"):
            raise ValueError("Build inputs must be regular files, not links or submodules")
        if name in files or not re.fullmatch(r"[0-9a-f]{40}", entry["sha"]):
            raise ValueError("Invalid or duplicate source object")
        size = entry["size"]
        if type(size) is not int or not 0 <= size <= MAX_FILE_BYTES:
            raise ValueError("Build-input file exceeds the accepted size")
        files[name] = {"sha": entry["sha"], "size": size}
    if not REQUIRED.issubset(files):
        raise ValueError("Pinned commit is missing required build inputs")
    if sum(entry["size"] for entry in files.values()) > MAX_TOTAL_BYTES:
        raise ValueError("Build-input snapshot exceeds the accepted size")
    return files


def verify_snapshot(destination, record):
    if destination.is_symlink() or not destination.is_dir():
        raise ValueError("Existing source snapshot is not an ordinary directory")
    marker = destination / MARKER
    if marker.is_symlink() or not marker.is_file() or json.loads(marker.read_bytes()) != record:
        raise ValueError("Existing source snapshot has no matching ownership record")
    actual = {}
    for path in [destination, *destination.rglob("*")]:
        if path.is_symlink():
            raise ValueError("Existing source snapshot contains a symbolic link")
        metadata = path.stat()
        expected_mode = 0o755 if path.is_dir() else 0o644
        if stat.S_IMODE(metadata.st_mode) != expected_mode or metadata.st_uid != os.geteuid():
            raise ValueError("Existing source snapshot ownership or permissions changed")
        if path.is_dir():
            continue
        if not path.is_file():
            raise ValueError("Existing source snapshot contains a special file")
        name = path.relative_to(destination).as_posix()
        if name == MARKER:
            continue
        expected = record["files"].get(name)
        if expected is None or path.stat().st_size != expected["size"]:
            raise ValueError("Existing source snapshot has changed")
        data = path.read_bytes()
        actual[name] = {"sha": object_id(data), "size": len(data)}
    if actual != record["files"]:
        raise ValueError("Existing source snapshot has changed")


def fetch(commit, destination, request=api):
    if not re.fullmatch(r"[0-9a-f]{40}", commit):
        raise ValueError("A full immutable project commit is required")
    files = selected_files(request(f"{REPOSITORY}/git/trees/{commit}?recursive=1"))
    record = {"version": 1, "commit": commit, "files": files}
    destination = Path(destination)
    if destination.exists() or destination.is_symlink():
        verify_snapshot(destination, record)
        return destination
    destination.parent.mkdir(parents=True, exist_ok=True)
    staged = Path(tempfile.mkdtemp(prefix=".aegis-inputs-", dir=destination.parent))
    try:
        for name, expected in sorted(files.items()):
            blob = request(f"{REPOSITORY}/git/blobs/{expected['sha']}")
            if blob.get("encoding") != "base64" or blob.get("sha") != expected["sha"]:
                raise ValueError("GitHub returned an unexpected source blob")
            data = base64.b64decode("".join(blob["content"].splitlines()), validate=True)
            if (blob.get("size") != expected["size"] or len(data) != expected["size"]
                    or object_id(data) != expected["sha"]):
                raise ValueError("Downloaded source does not match its Git object")
            output = staged / name
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_bytes(data)
            output.chmod(0o644)
        (staged / MARKER).write_text(json.dumps(record, sort_keys=True) + "\n")
        (staged / MARKER).chmod(0o644)
        # Bootstrap runs with umask 077, but the unprivileged compiler must read sources.
        for directory in [staged, *(p for p in staged.rglob("*") if p.is_dir())]:
            directory.chmod(0o755)
        verify_snapshot(staged, record)
        if destination.exists() or destination.is_symlink():
            raise ValueError("Snapshot destination appeared during download")
        staged.rename(destination)
    finally:
        if staged.exists():
            shutil.rmtree(staged)  # Only this call's newly allocated staging directory.
    return destination


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("commit")
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    print(fetch(args.commit, args.destination))
