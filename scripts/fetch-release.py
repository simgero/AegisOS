#!/usr/bin/env python3
"""Download a published build from GitHub and verify every asset; never execute it."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

REPOSITORY = "simgero/AegisOS"


def verify(directory):
    directory = Path(directory)
    manifest = directory / "SHA256SUMS"
    if not manifest.is_file() or manifest.is_symlink():
        raise ValueError("Missing regular SHA256SUMS file")
    expected = {}
    for line in manifest.read_text().splitlines():
        match = re.fullmatch(r"([0-9a-f]{64}) [ *](?:\./)?([^/\\]+)", line)
        if not match:
            raise ValueError("Invalid checksum manifest entry")
        digest, name = match.groups()
        if name in (".", "..", "SHA256SUMS") or name in expected:
            raise ValueError("Unsafe or duplicate checksum entry")
        expected[name] = digest
    actual = {p.name for p in directory.iterdir()} - {"SHA256SUMS"}
    if not expected or actual != set(expected):
        raise ValueError("Downloaded assets do not match checksum manifest")
    for name, digest in expected.items():
        path = directory / name
        if path.is_symlink() or not path.is_file():
            raise ValueError(f"Not a regular file: {name}")
        checksum = hashlib.sha256()
        with path.open("rb") as source:
            for chunk in iter(lambda: source.read(1024 * 1024), b""):
                checksum.update(chunk)
        if checksum.hexdigest() != digest:
            raise ValueError(f"Checksum mismatch: {name}")
    parts = sorted(name for name in expected if re.fullmatch(r"images\.tar\.xz\.part-[0-9]{4}", name))
    if not parts or parts != [f"images.tar.xz.part-{i:04d}" for i in range(len(parts))]:
        raise ValueError("Missing or non-contiguous image archive parts")
    return len(expected)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("tag", help="Explicit published GitHub release tag")
    parser.add_argument("directory", type=Path, help="New destination directory")
    args = parser.parse_args()
    if args.tag.startswith("-"):
        parser.error("Release tag must not start with '-'")
    if args.directory.exists():
        parser.error("Destination must be a new directory; existing downloads are preserved")
    release = json.loads(subprocess.check_output([
        "gh", "release", "view", args.tag, "--repo", REPOSITORY,
        "--json", "isDraft,tagName,url",
    ], text=True))
    if release["isDraft"] or release["tagName"] != args.tag:
        raise ValueError("Only an explicitly selected published release can be downloaded")
    args.directory.mkdir(parents=True)
    subprocess.run([
        "gh", "release", "download", args.tag, "--repo", REPOSITORY,
        "--dir", str(args.directory.resolve()),
    ], check=True)
    count = verify(args.directory)
    print(f"Verified {count} assets from {release['url']}")
    print(f"Downloaded to {args.directory.resolve()}; archive has not been extracted.")
    print("Checksums confirm integrity, not QEMU compatibility or a successful boot.")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        print(f"Download NOT verified: {error}", file=sys.stderr)
        sys.exit(1)
