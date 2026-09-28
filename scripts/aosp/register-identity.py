#!/usr/bin/env python3
"""Install an owned identity source tree into AOSP, preserving every previous tree."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import tempfile
import uuid

MARKER = ".aegis-source.json"


def inventory(root):
    result = {}
    for path in root.rglob("*"):
        if path.is_symlink():
            raise ValueError("Identity sources must not contain symlinks")
        if path.is_dir():
            continue
        if not path.is_file():
            raise ValueError("Identity source is not a regular file")
        relative = path.relative_to(root).as_posix()
        if relative == MARKER:
            continue
        result[relative] = hashlib.sha256(path.read_bytes()).hexdigest()
    return result


def owned_directory(path):
    if path.is_symlink() or not path.is_dir():
        raise ValueError("Identity destination is not an ordinary directory")
    marker = path / MARKER
    if marker.is_symlink() or not marker.is_file():
        raise ValueError("Identity destination is not managed by Aegis")
    record = json.loads(marker.read_text())
    if record.get("version") != 1 or record.get("files") != inventory(path):
        raise ValueError("Existing identity sources changed outside the installer")
    return record["files"]


def directory_chain(base, parts):
    current = base
    for name in parts:
        current = current / name
        if current.is_symlink():
            raise ValueError("Refusing a symlinked AOSP destination ancestor")
        if current.exists() and not current.is_dir():
            raise ValueError("AOSP destination ancestor is not a directory")
    return current


def register(source, aosp):
    source, aosp = Path(source), Path(aosp).resolve()
    if source.is_symlink() or not source.is_dir():
        raise ValueError("Expected an ordinary identity source directory")
    source = source.resolve()
    if (source / MARKER).exists() or (source / MARKER).is_symlink():
        raise ValueError("Input contains the reserved installer marker")
    if not (source / "Android.bp").is_file():
        raise ValueError("Identity source is missing Android.bp")
    if not (aosp / "build/envsetup.sh").is_file():
        raise ValueError("Missing AOSP checkout")
    expected = inventory(source)
    parent = directory_chain(aosp, ("packages", "aegis"))
    target = parent / "identity"
    backups = directory_chain(aosp, ("out", "aegis-identity-backups"))
    staging_parent = directory_chain(aosp, ("out", "aegis-identity-staging"))
    previous = None
    if target.exists() or target.is_symlink():
        previous = owned_directory(target)
    if previous == expected:
        return target
    parent.mkdir(parents=True, exist_ok=True)
    # Even after SIGKILL, an incomplete copy must not be discovered as another
    # Android.bp module by a later AOSP build. out/ is outside source discovery.
    staging_parent.mkdir(parents=True, exist_ok=True)
    staged = Path(tempfile.mkdtemp(prefix="identity-", dir=staging_parent))
    try:
        for relative in sorted(expected):
            destination = staged / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source / relative, destination)
            if previous is not None and previous.get(relative) == expected[relative]:
                old = (target / relative).stat()
                os.utime(destination, ns=(old.st_atime_ns, old.st_mtime_ns))
        if inventory(staged) != expected:
            raise ValueError("Identity sources changed while staging")
        (staged / MARKER).write_text(json.dumps({"version": 1, "files": expected},
                                               sort_keys=True) + "\n")
        if target.exists():
            if owned_directory(target) != previous:
                raise ValueError("Identity sources changed while staging")
            backups.mkdir(parents=True, exist_ok=True)
            target.rename(backups / uuid.uuid4().hex)
        staged.rename(target)
    finally:
        if staged.exists():
            shutil.rmtree(staged)  # Only our newly allocated temporary staging directory.
    return target


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("aosp", type=Path)
    args = parser.parse_args()
    print(register(args.source, args.aosp))
