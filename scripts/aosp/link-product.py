#!/usr/bin/env python3
"""Register immutable Aegis device files without replacing unrelated AOSP files."""
import argparse
import json
import hashlib
import os
import shutil
import tempfile
import uuid
from pathlib import Path

FILES = ("AndroidProducts.mk", "aegis_qemu_arm64.mk", "BoardConfig.mk")


def inventory(root):
    result = {}
    for path in root.rglob("*"):
        if path.is_symlink():
            raise ValueError("Product sources must not contain symbolic links")
        if path.is_dir():
            continue
        if not path.is_file():
            raise ValueError("Product source is not a regular file")
        result[path.relative_to(root).as_posix()] = hashlib.sha256(path.read_bytes()).hexdigest()
    return result


def directory_chain(base, parts):
    current = base
    for name in parts:
        current = current / name
        if current.is_symlink() or (current.exists() and not current.is_dir()):
            raise ValueError("Product destination ancestor is not an ordinary directory")
    return current


def verify_existing(target, marker):
    if target.exists() or target.is_symlink():
        if not marker.is_file() or marker.is_symlink():
            raise ValueError("Existing product is not managed by Aegis")
        recorded = json.loads(marker.read_text())
        if target.is_symlink():
            # Migrate the previous installer without following or deleting its source.
            if str(target.readlink()) != recorded.get("source"):
                raise ValueError("Product link changed outside the installer")
        elif target.is_dir():
            expected = recorded.get("sha256", {})
            if not set(FILES).issubset(expected) or inventory(target) != expected:
                raise ValueError("Product sources changed outside the installer")
            return expected
        else:
            raise ValueError("Existing product is not a directory")


def register(source, aosp):
    source, aosp = Path(source), Path(aosp).resolve()
    if source.is_symlink() or not source.is_dir():
        raise ValueError("Product source must be an ordinary directory")
    source = source.resolve()
    expected = inventory(source)
    if not set(FILES).issubset(expected):
        raise ValueError("Missing required product source files")
    if not (aosp / "build/envsetup.sh").is_file():
        raise ValueError("AOSP checkout is missing build/envsetup.sh")
    parent = directory_chain(aosp, ("device", "aegis"))
    target = parent / "qemu_arm64"
    marker = parent / ".aegis-product-link.json"
    if marker.is_symlink():
        raise ValueError("Product ownership record is a symbolic link")
    backups = directory_chain(aosp, ("out", "aegis-product-backups"))
    staging_parent = directory_chain(aosp, ("out", "aegis-product-staging"))
    pending = parent / ".aegis-product-link.json.tmp"
    if pending.exists() or pending.is_symlink():
        raise ValueError("Unfinished product ownership update requires inspection")
    previous = verify_existing(target, marker)
    if previous == expected:
        # Keep Make input mtimes stable when a new Git checkout has identical
        # contents, but retain the current checkout in the ownership receipt.
        with pending.open("x") as stream:
            stream.write(json.dumps({"source": str(source), "sha256": expected}) + "\n")
        pending.replace(marker)
        return
    # AOSP's file finder does not discover products through directory symlinks.
    # Install actual files, preserving the previous managed installation for recovery.
    # Stage outside device/ too: an interrupted copy must not be discovered as a product.
    parent.mkdir(parents=True, exist_ok=True)
    staging_parent.mkdir(parents=True, exist_ok=True)
    staged = Path(tempfile.mkdtemp(prefix="qemu-", dir=staging_parent))
    try:
        for name in sorted(expected):
            destination = staged / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source / name, destination)
            if previous is not None and previous.get(name) == expected[name]:
                old = (target / name).stat()
                os.utime(destination, ns=(old.st_atime_ns, old.st_mtime_ns))
        if inventory(staged) != expected:
            raise ValueError("Product sources changed while staging")
        verify_existing(target, marker)
        if target.exists() or target.is_symlink():
            backups.mkdir(parents=True, exist_ok=True)
            target.rename(backups / uuid.uuid4().hex)
        staged.rename(target)
        with pending.open("x") as stream:
            stream.write(json.dumps({"source": str(source), "sha256": expected}) + "\n")
        pending.replace(marker)
    finally:
        if staged.exists():
            shutil.rmtree(staged)  # Only this call's newly allocated staging directory.


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("aosp", type=Path)
    args = parser.parse_args()
    register(args.source, args.aosp)
