#!/usr/bin/env python3
"""Register immutable Aegis device files without replacing unrelated AOSP files."""
import argparse
import json
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
        if not target.is_symlink() or not marker.is_file() or marker.is_symlink():
            raise ValueError("Existing product is not a managed Aegis symlink")
        recorded = json.loads(marker.read_text())
        if str(target.readlink()) != recorded.get("source"):
            raise ValueError("Product link changed outside the installer; refusing to replace it")
        if target.resolve() == source:
            return
        target.unlink()  # Only the verified managed link; source files remain intact.
    target.symlink_to(source, target_is_directory=True)
    marker.write_text(json.dumps({"source": str(source)}) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("aosp", type=Path)
    args = parser.parse_args()
    register(args.source, args.aosp)
