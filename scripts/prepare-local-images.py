#!/usr/bin/env python3
"""Extract checked GitHub image assets for local QEMU; never boot or execute them.

This does not replace AVB verification, disk preparation or guest tests. An
explicit source commit and a NEW output directory are required. Existing
downloads, disks and persistent profiles are never changed.
"""
import argparse
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import re
import tarfile

ROOT = Path(__file__).resolve().parent
MAX_FILE = 16 * 1024**3
MAX_TOTAL = 32 * 1024**3
REQUIRED = {"kernel", "boot.img", "init_boot.img", "vendor_boot.img", "ramdisk.img",
            "vendor-bootconfig.img", "super.img", "userdata.img", "vbmeta.img",
            "vbmeta_system.img", "vbmeta_system_dlkm.img", "vbmeta_vendor_dlkm.img"}


def tool(name, path):
    spec = importlib.util.spec_from_file_location(name, ROOT / path)
    loaded = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(loaded)
    return loaded


def fingerprint(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return {"size": path.stat().st_size, "sha256": digest.hexdigest()}


def read_json(path):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError("Duplicate metadata key")
            result[key] = value
        return result
    if path.stat().st_size > 16 * 1024**2:
        raise ValueError("Oversized metadata")
    return json.loads(path.read_bytes(), object_pairs_hook=unique)


class Parts(io.RawIOBase):
    """Read contiguous archive parts without another multi-GB host copy."""
    def __init__(self, paths):
        self.paths = iter(paths)
        self.current = None

    def read(self, size=-1):
        if size < 0:
            raise ValueError("Unbounded archive read")
        result = bytearray()
        while len(result) < size:
            if self.current is None:
                path = next(self.paths, None)
                if path is None:
                    break
                self.current = path.open("rb")
            chunk = self.current.read(size - len(result))
            if chunk:
                result.extend(chunk)
            else:
                self.current.close()
                self.current = None
        return bytes(result)

    def close(self):
        if self.current is not None:
            self.current.close()
        super().close()


def extract(parts, images):
    images.mkdir(mode=0o700)
    files = {}
    total = 0
    with Parts(parts) as stream, tarfile.open(fileobj=stream, mode="r|xz") as archive:
        for member in archive:
            name = member.name.removeprefix("./")
            if (not re.fullmatch(r"[A-Za-z0-9._+-]{1,200}", name) or name in (".", "..")
                    or not member.isreg() or member.sparse is not None
                    or name in files or len(files) >= 512
                    or not 0 <= member.size <= MAX_FILE):
                raise ValueError("Unsafe, duplicate or oversized image archive entry: " + repr(member.name[:220]))
            total += member.size
            if total > MAX_TOTAL:
                raise ValueError("Expanded image archive exceeds the limit")
            # Do not restore archive ownership, modes, links or extended metadata.
            # Even ELF-looking files remain nonexecutable host data.
            path = images / name
            with archive.extractfile(member) as source, path.open("xb") as target:
                path.chmod(0o600)
                remaining = member.size
                digest = hashlib.sha256()
                while remaining:
                    data = source.read(min(1024 * 1024, remaining))
                    if not data:
                        raise ValueError("Truncated image archive member")
                    target.write(data)
                    digest.update(data)
                    remaining -= len(data)
            files[name] = {"size": member.size, "sha256": digest.hexdigest()}
    if not REQUIRED <= files.keys() or any(files[name]["size"] == 0 for name in REQUIRED):
        raise ValueError("Missing required Android images")
    return files


def check_receipts(download, images, files):
    checked = {}
    kernel = download / "kernel-inputs.json"
    if kernel.exists():
        record = read_json(kernel)
        inputs = record["inputs"]
        canonical = json.dumps(inputs, sort_keys=True, separators=(",", ":")).encode()
        if (inputs["status"] != "CHECKED_INPUTS_NOT_BOOTED"
                or hashlib.sha256(canonical).hexdigest() != record["bundle_id"]
                or files["kernel"] != inputs["files"]["gki/kernel-6.12"]):
            raise ValueError("Delivered kernel differs from its checked input set")
        inspector = tool("aegis_local_kernel", "kernel/integrate.py")
        boot_kernel = inspector.boot_kernel(inspector.read(images / "boot.img", inspector.MAX_IMAGE))
        if hashlib.sha256(boot_kernel).hexdigest() != files["kernel"]["sha256"]:
            raise ValueError("Boot image contains a different kernel")
        release, config = inspector.image_metadata(inspector.read(images / "kernel", inspector.MAX_IMAGE))
        if (release != inputs["kernel_release"]
                or any(config.get(key) != "y" for key in inspector.REQUIRED_CONFIG)):
            raise ValueError("Delivered kernel lacks required configuration")
        checked["kernel_bundle"] = record["bundle_id"]
    base = download / "runtime-base-inputs.json"
    if base.exists():
        record = read_json(base)
        report = read_json(download / "runtime-base-image.json")
        canonical = (json.dumps(record["inputs"], sort_keys=True, separators=(",", ":")) + "\n").encode()
        if (record["inputs"]["status"] != "CHECKED_BASE_INPUTS_NOT_MOUNTED"
                or hashlib.sha256(canonical).hexdigest() != record["bundle_id"]
                or record["installed"] != {"etc/aegis/runtime/base.ext4": "runtime-base.ext4",
                                           "etc/aegis/runtime/generation.json": "generation.json"}):
            raise ValueError("Invalid selected-base receipt")
        installed = {
            name: record["inputs"]["files"][source]
            for name, source in record["installed"].items()
        }
        if (report["status"] != "PACKAGED_BASE_BYTES_VERIFIED_NOT_BOOTED"
                or report["super.img"] != files["super.img"]
                or report["selected_receipt"] != fingerprint(base)
                or report["installed"] != installed
                or report["metadata_slot"] != 0 or report["partition"] != "system_ext_a"
                or report["filesystem"] != "erofs"):
            raise ValueError("Base readback receipt does not match the delivered super image")
        checked["base_generation"] = record["inputs"]["generation"]
    return checked


def prepare(download, output, commit):
    download, output = Path(download).absolute(), Path(output).absolute()
    if (not re.fullmatch(r"[0-9a-f]{40}", commit) or download.resolve() != download
            or output.resolve() != output or output.exists()):
        raise ValueError("Require an exact commit and a new, non-symlink output path")
    verifier = tool("aegis_local_release", "fetch-release.py")
    verifier.verify(download)
    manifest = fingerprint(download / "SHA256SUMS")
    if (download / "builder-commit.txt").read_text().strip() != commit:
        raise ValueError("Release belongs to another build commit")
    output.mkdir(mode=0o700)
    files = extract(sorted(download.glob("images.tar.xz.part-*")), output / "images")
    inputs = check_receipts(download, output / "images", files)
    # Detect changed downloads before publishing a success receipt. A failed
    # output remains for diagnosis, never suitable for resuming or booting.
    verifier.verify(download)
    if fingerprint(download / "SHA256SUMS") != manifest:
        raise ValueError("Download manifest changed during extraction")
    report = {"schema": 1, "status": "EXTRACTED_CHECKED_NOT_AVB_VERIFIED_NOT_BOOTED",
              "builder_commit": commit, "inputs": inputs, "files": files,
              "download_manifest": manifest}
    with (output / "prepared.json").open("x") as target:
        json.dump(report, target, sort_keys=True, indent=2)
        target.write("\n")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("download", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--build-commit", required=True)
    args = parser.parse_args()
    report = prepare(args.download, args.output, args.build_commit)
    print(report["status"] + ": " + str(args.output.resolve()))
    print("AVB verification and a fresh paired QEMU profile are still required.")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, KeyError, TypeError, tarfile.TarError) as error:
        raise SystemExit("Local image preparation failed: " + str(error))
