#!/usr/bin/env python3
"""Register immutable Aegis device files without replacing unrelated AOSP files."""
import argparse
import json
import hashlib
import shutil
import tempfile
import uuid
from pathlib import Path

FILES = ("AndroidProducts.mk", "aegis_qemu_arm64.mk", "BoardConfig.mk")


def register(source, aosp):
    source, aosp = Path(source).resolve(), Path(aosp).resolve()
    for name in FILES:
        if not (source / name).is_file():
            raise ValueError(f"Missing product source: {source / name}")
    if not (aosp / "build/envsetup.sh").is_file():
        raise ValueError("AOSP checkout is missing build/envsetup.sh")
    parent = aosp / "device/aegis"
    if parent.is_symlink():
        raise ValueError("Refusing a symlinked device/aegis directory")
    parent.mkdir(parents=True, exist_ok=True)
    target = parent / "qemu_arm64"
    marker = parent / ".aegis-product-link.json"
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
            if set(expected) != set(FILES) or {p.name for p in target.iterdir()} != set(FILES):
                raise ValueError("Existing product directory contains unmanaged files")
            for name in FILES:
                path = target / name
                if path.is_symlink() or not path.is_file():
                    raise ValueError("Existing product contains a non-regular file")
                if hashlib.sha256(path.read_bytes()).hexdigest() != expected[name]:
                    raise ValueError("Product file changed outside the installer")
        else:
            raise ValueError("Existing product is not a directory")
    # AOSP's file finder does not discover products through directory symlinks.
    # Install actual files, preserving the previous managed installation for recovery.
    staged = Path(tempfile.mkdtemp(prefix=".qemu-stage-", dir=parent))
    hashes = {}
    for name in FILES:
        shutil.copyfile(source / name, staged / name)
        hashes[name] = hashlib.sha256((staged / name).read_bytes()).hexdigest()
    if target.exists() or target.is_symlink():
        # Keep old .mk files outside device/: duplicate product discovery is unsafe.
        backups = aosp / "out/aegis-product-backups"
        backups.mkdir(parents=True, exist_ok=True)
        target.rename(backups / uuid.uuid4().hex)
    staged.rename(target)
    pending = parent / ".aegis-product-link.json.tmp"
    pending.write_text(json.dumps({"source": str(source), "sha256": hashes}) + "\n")
    pending.replace(marker)



if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("aosp", type=Path)
    args = parser.parse_args()
    register(args.source, args.aosp)
