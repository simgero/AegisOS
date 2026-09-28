#!/usr/bin/env python3
"""Fetch/verify the pinned Debian ARM64 base, without extracting or executing it."""
import argparse
import base64
import gzip
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile

REPOSITORY = "debuerreotype/docker-debian-artifacts"
DIRECTORY = "trixie/slim/oci/blobs"
FILES = {"image-manifest.json", "image-config.json", "rootfs.tar.gz"}
RECEIPT = "import-report.json"
MAX_METADATA = 512 * 1024
MAX_COMPRESSED = 128 * 1024 * 1024
MAX_EXPANDED = 1024 * 1024 * 1024
MAX_MEMBERS = 100_000
TEXT_FILES = {
    "etc/debian_version", "usr/lib/os-release", "etc/passwd", "etc/shadow",
    "etc/group", "etc/gshadow", "var/lib/dpkg/status",
    "etc/apt/sources.list.d/debian.sources",
}
ELF_FILES = {
    "usr/bin/bash", "usr/bin/apt", "usr/bin/dpkg", "usr/bin/ls",
    "usr/lib/aarch64-linux-gnu/libc.so.6",
    "usr/lib/aarch64-linux-gnu/ld-linux-aarch64.so.1",
}
KEYRING = "usr/share/keyrings/debian-archive-keyring.pgp"
REQUIRED_PACKAGES = {"apt", "bash", "coreutils", "dpkg", "libc6",
                     "debian-archive-keyring"}


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("Duplicate JSON field")
        result[key] = value
    return result


def read_json(data):
    return json.loads(data, object_pairs_hook=unique_object)


def validate_pin(pin):
    if (set(pin) != {"schema", "repository", "commit", "directory", "debian_version",
                    "official_images_commit", "official_images_tag", "manifest"}
            or type(pin["schema"]) is not int or pin["schema"] != 1
            or pin["repository"] != REPOSITORY or pin["directory"] != DIRECTORY):
        raise ValueError("Unsupported runtime base pin")
    for key in ("commit", "official_images_commit"):
        if not re.fullmatch(r"[0-9a-f]{40}", pin[key]):
            raise ValueError("The base must use immutable GitHub commits")
    if (not re.fullmatch(r"13\.[0-9]+", pin["debian_version"])
            or not re.fullmatch(r"trixie-[0-9]{8}-slim", pin["official_images_tag"])):
        raise ValueError("Expected an explicit Debian 13 slim version")
    descriptor(pin["manifest"], MAX_METADATA, pinned=True)


def descriptor(value, limit, *, pinned=False):
    digest = value["sha256"] if pinned else value["digest"].removeprefix("sha256:")
    if (not re.fullmatch(r"[0-9a-f]{64}", digest)
            or (not pinned and value["digest"] != "sha256:" + digest)
            or type(value["size"]) is not int or not 0 < value["size"] <= limit):
        raise ValueError("Invalid or oversized SHA-256 descriptor")
    return digest, value["size"]


def checked_file(path, value, limit, *, pinned=False):
    digest, size = descriptor(value, limit, pinned=pinned)
    if path.is_symlink() or not path.is_file() or path.stat().st_size != size:
        raise ValueError("Missing, linked or differently sized base artifact")
    checksum = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            checksum.update(chunk)
    if checksum.hexdigest() != digest:
        raise ValueError("Runtime base checksum mismatch")


def image_descriptors(manifest):
    if (manifest.get("schemaVersion") != 2
            or manifest.get("mediaType") != "application/vnd.oci.image.manifest.v1+json"
            or manifest["config"]["mediaType"] != "application/vnd.oci.image.config.v1+json"
            or len(manifest["layers"]) != 1
            or manifest["layers"][0]["mediaType"] != "application/vnd.oci.image.layer.v1.tar+gzip"):
        raise ValueError("Expected one complete gzip-compressed Debian base layer")
    descriptor(manifest["config"], MAX_METADATA)
    descriptor(manifest["layers"][0], MAX_COMPRESSED)
    return manifest["config"], manifest["layers"][0]


def member_name(name):
    if name.startswith("./"):
        name = name[2:]
    path = PurePosixPath(name)
    if (not name or path.is_absolute() or path.as_posix() != name
            or ".." in path.parts or "\\" in name or any(ord(c) < 32 for c in name)
            or any(part.startswith(".wh.") for part in path.parts)):
        raise ValueError("Unsafe, noncanonical or layered rootfs entry")
    return name


def package_inventory(status):
    packages = {}
    for stanza in status.strip().split("\n\n"):
        fields = {}
        for line in stanza.splitlines():
            if line.startswith((" ", "\t")):
                continue
            key, separator, value = line.partition(":")
            if not separator or not re.fullmatch(r"[A-Za-z][A-Za-z0-9-]*", key) or key in fields:
                raise ValueError("Malformed or duplicate dpkg field")
            # Debian control fields may be empty before their continuation lines
            # (notably 'Conffiles:'); a literal colon-space is not required.
            fields[key] = value.lstrip(" \t")
        name = fields["Package"]
        if (not re.fullmatch(r"[a-z0-9][a-z0-9+.-]+", name) or name in packages
                or fields["Status"] != "install ok installed"
                or fields["Architecture"] not in {"arm64", "all"} or not fields["Version"]):
            raise ValueError("Incomplete, duplicate or foreign-architecture Debian package")
        packages[name] = {"version": fields["Version"], "architecture": fields["Architecture"]}
    if not REQUIRED_PACKAGES <= packages.keys():
        raise ValueError("Debian base is missing required GNU/Linux components")
    return packages


def account_inventory(texts):
    users = {}
    for row in texts["etc/passwd"].splitlines():
        name, password, uid, gid, _, _, _ = row.split(":")
        uid, gid = int(uid), int(gid)
        if (name in users or password != "x" or not 0 <= gid <= 65534
                or not (0 <= uid < 1000 or uid == 65534)):
            raise ValueError("Base contains an unexpected personal account or credential")
        users[name] = {"uid": uid, "gid": gid}
    shadow = {}
    for row in texts["etc/shadow"].splitlines():
        fields = row.split(":")
        if len(fields) != 9 or fields[0] in shadow or not fields[1].startswith(("!", "*")):
            raise ValueError("Base Linux password entries must all be locked")
        shadow[fields[0]] = True
    if set(users) != set(shadow) or users.get("root") != {"uid": 0, "gid": 0}:
        raise ValueError("Inconsistent base account metadata")
    groups = {}
    for row in texts["etc/group"].splitlines():
        name, password, gid, members = row.split(":")
        if (name in groups or password != "x" or not 0 <= int(gid) <= 65534
                or members):
            raise ValueError("Unexpected base group metadata or memberships")
        groups[name] = int(gid)
    group_shadow = set()
    for row in texts["etc/gshadow"].splitlines():
        name, password, admins, members = row.split(":")
        if name in group_shadow or not password.startswith(("!", "*")) or admins or members:
            raise ValueError("Base group credentials must be locked and unassigned")
        group_shadow.add(name)
    if group_shadow != groups.keys():
        raise ValueError("Inconsistent base group metadata")
    # Numeric IDs are source metadata, NOT an Android UID mapping or authorization.
    return users, groups


def inspect_archive(archive, config, pin):
    if (config.get("os") != "linux" or config.get("architecture") != "arm64"
            or config.get("variant") != "v8" or config["rootfs"]["type"] != "layers"
            or len(config["rootfs"]["diff_ids"]) != 1):
        raise ValueError("Expected a Linux ARM64/v8 root filesystem")
    expanded = 0
    checksum = hashlib.sha256()
    with gzip.open(archive, "rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            expanded += len(chunk)
            if expanded > MAX_EXPANDED:
                raise ValueError("Expanded runtime base exceeds its limit")
            checksum.update(chunk)
    if config["rootfs"]["diff_ids"][0] != "sha256:" + checksum.hexdigest():
        raise ValueError("Uncompressed OCI layer digest mismatch")
    entries, texts, elf, owners, groups, setid = {}, {}, set(), set(), set(), []
    keyring_present = False
    with tarfile.open(archive, "r:gz") as tar:
        for member in tar:
            if member.name in (".", "./") and member.isdir():
                continue
            name = member_name(member.name)
            if name in entries or len(entries) >= MAX_MEMBERS or member.sparse is not None:
                raise ValueError("Duplicate, sparse or excessive rootfs entries")
            if not (member.isfile() or member.isdir() or member.issym() or member.islnk()):
                raise ValueError("Unexpected device or special node in Debian base")
            if not 0 <= member.uid <= 65534 or not 0 <= member.gid <= 65534:
                raise ValueError("Unexpected source file ownership")
            entries[name] = member
            owners.add(member.uid)
            groups.add(member.gid)
            if member.mode & 0o6000:
                setid.append(name)
            if name in TEXT_FILES | ELF_FILES | {KEYRING}:
                if not member.isfile():
                    raise ValueError("Required runtime component is not a regular file")
                with tar.extractfile(member) as stream:
                    if name in TEXT_FILES:
                        if member.size > MAX_METADATA:
                            raise ValueError("Oversized rootfs metadata")
                        texts[name] = stream.read().decode("utf-8")
                    elif name in ELF_FILES:
                        header = stream.read(64)
                        if (len(header) != 64 or header[:7] != b"\x7fELF\x02\x01\x01"
                                or int.from_bytes(header[18:20], "little") != 183):
                            raise ValueError("A required binary is not ARM64 ELF")
                        elf.add(name)
                    else:
                        keyring_present = member.size > 0
    if texts.keys() != TEXT_FILES or elf != ELF_FILES or not keyring_present:
        raise ValueError("Missing runtime tools, accounts, archive keys or metadata")
    # No extraction occurs here. Still reject link-based ancestor substitutions
    # and escaping hardlinks before these bytes reach the later image assembler.
    for name, member in entries.items():
        if any(str(parent) in entries and not entries[str(parent)].isdir()
               for parent in PurePosixPath(name).parents if str(parent) != "."):
            raise ValueError("Rootfs entry lies below a non-directory")
        if member.islnk():
            target = entries.get(member_name(member.linkname))
            if target is None or not target.isfile():
                raise ValueError("Hardlink does not target an archived regular file")
    if texts["etc/debian_version"].strip() != pin["debian_version"]:
        raise ValueError("Debian version differs from the selected base")
    os_release = texts["usr/lib/os-release"].splitlines()
    if "ID=debian" not in os_release or "VERSION_CODENAME=trixie" not in os_release:
        raise ValueError("Unexpected rootfs distribution")
    if "Signed-By: /usr/share/keyrings/debian-archive-keyring.pgp" not in texts[
            "etc/apt/sources.list.d/debian.sources"]:
        raise ValueError("Expected Debian archive-keyring configuration")
    packages = package_inventory(texts["var/lib/dpkg/status"])
    users, technical_groups = account_inventory(texts)
    return {
        "status": "VERIFIED_UPSTREAM_BASE_NOT_INSTALLED", "pin": pin,
        "expanded_bytes": expanded, "archive_entries": len(entries),
        "packages": packages, "technical_users": users, "technical_groups": technical_groups,
        "file_uids": sorted(owners), "file_gids": sorted(groups),
        "setid_paths": sorted(setid),
        "scope": "Source integrity/content only; no execution, extraction, UID mapping or isolation test",
    }


def validate_contents(pin, directory):
    validate_pin(pin)
    checked_file(directory / "image-manifest.json", pin["manifest"], MAX_METADATA, pinned=True)
    manifest = read_json((directory / "image-manifest.json").read_bytes())
    config_desc, layer_desc = image_descriptors(manifest)
    checked_file(directory / "image-config.json", config_desc, MAX_METADATA)
    config_data = (directory / "image-config.json").read_bytes()
    if "data" in config_desc and base64.b64decode(config_desc["data"], validate=True) != config_data:
        raise ValueError("Embedded OCI configuration differs from its blob")
    checked_file(directory / "rootfs.tar.gz", layer_desc, MAX_COMPRESSED)
    return inspect_archive(directory / "rootfs.tar.gz", read_json(config_data), pin)


def verify(pin, directory):
    directory = Path(directory)
    if (directory.is_symlink() or not directory.is_dir()
            or {p.name for p in directory.iterdir()} != FILES | {RECEIPT}
            or (directory / RECEIPT).is_symlink() or not (directory / RECEIPT).is_file()
            or (directory / RECEIPT).stat().st_size > MAX_METADATA):
        raise ValueError("Existing base import is incomplete or changed")
    report = validate_contents(pin, directory)
    if read_json((directory / RECEIPT).read_bytes()) != report:
        raise ValueError("Runtime base report is missing or changed")
    return report


def download(url, destination, limit):
    result = subprocess.run([
        "curl", "--fail", "--location", "--silent", "--show-error", "--proto", "=https",
        "--proto-redir", "=https", "--max-time", "180", "--max-filesize", str(limit),
        "--output", str(destination), "--url", url,
    ], capture_output=True, timeout=190)
    if result.returncode:
        raise RuntimeError(f"Pinned GitHub base download failed (exit {result.returncode})")


def fetch(pin, directory, downloader=download):
    validate_pin(pin)
    directory = Path(directory).absolute()
    # Use physical parent paths (e.g. /private/tmp on macOS). Do not redirect
    # writes through an existing symlink in a user-provided destination.
    if any(path.is_symlink() for path in [directory, *directory.parents]):
        raise ValueError("Base destination must not traverse symbolic links")
    if directory.exists():
        return verify(pin, directory)
    directory.parent.mkdir(parents=True, exist_ok=True)
    staged = Path(tempfile.mkdtemp(prefix=".aegis-base-", dir=directory.parent))
    try:
        url = f"https://raw.githubusercontent.com/{REPOSITORY}/{pin['commit']}/{DIRECTORY}/"
        downloader(url + "image-manifest.json", staged / "image-manifest.json", pin["manifest"]["size"])
        checked_file(staged / "image-manifest.json", pin["manifest"], MAX_METADATA, pinned=True)
        manifest = read_json((staged / "image-manifest.json").read_bytes())
        config, layer = image_descriptors(manifest)
        for name, desc in [("image-config.json", config), ("rootfs.tar.gz", layer)]:
            downloader(url + name, staged / name, desc["size"])
        report = validate_contents(pin, staged)
        (staged / RECEIPT).write_text(json.dumps(report, sort_keys=True, indent=2) + "\n")
        if directory.exists() or directory.is_symlink():
            raise ValueError("Destination appeared during import; preserving it")
        staged.rename(directory)
        return report
    finally:
        if staged.exists():
            shutil.rmtree(staged)  # Only this call's private stage, never extracted rootfs files.


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["fetch", "verify"])
    parser.add_argument("directory", type=Path)
    parser.add_argument("--pin", type=Path, default=Path(__file__).resolve().parents[2]
                        / "runtime/debian-arm64.json")
    args = parser.parse_args()
    pin = read_json(args.pin.read_bytes())
    report = (fetch if args.action == "fetch" else verify)(pin, args.directory)
    print(json.dumps({"status": report["status"], "directory": str(args.directory.absolute()),
                      "packages": len(report["packages"]), "scope": report["scope"]}, indent=2))


if __name__ == "__main__":
    try:
        main()
    except (ValueError, TypeError, KeyError, OSError, RuntimeError, tarfile.TarError,
            subprocess.TimeoutExpired) as error:
        print(f"Runtime base NOT verified: {error}", file=sys.stderr)
        sys.exit(1)
