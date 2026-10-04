#!/usr/bin/env python3
"""Prepare a verified local build snapshot for QEMU in a new directory.

Run with read access to the completed build and write access to the destination
parent. This copies only immutable factory images and receipts, verifies the
selected kernel/runtime and AVB chain, and creates a fresh GPT base. It never
uploads, boots, creates personal users, or changes an existing profile.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import stat
import subprocess
import sys

SCRIPTS = Path(__file__).resolve().parent
IMAGES = {"kernel", "boot.img", "init_boot.img", "vendor_boot.img", "ramdisk.img",
          "vendor-bootconfig.img", "super.img", "userdata.img", "vbmeta.img",
          "vbmeta_system.img", "vbmeta_system_dlkm.img", "vbmeta_vendor_dlkm.img",
          "system.img", "system_ext.img", "product.img", "vendor.img", "odm.img",
          "system_dlkm.img", "vendor_dlkm.img", "odm_dlkm.img"}
RECEIPTS = ("status", "project-commit.txt", "product-out.txt", "product-target.txt",
            "manifest.xml", "identity-source-files.json", "product-source-files.json",
            "kernel-inputs.json", "runtime-base-inputs.json", "runtime-base-image.json",
            "runtime-base-generation.json", "runtime-policy-source.json",
            "runtime-storage-source.json", "vold-source.json", "bootanimation-source.json",
            "egl-cache-source.json", "misctrl-source.json", "pmsg-diagnostics-source.json", "build.log")


def regular(path):
    if not stat.S_ISREG(path.lstat().st_mode):
        raise ValueError(f"Expected a regular file: {path}")


def fingerprint(path):
    regular(path)
    with path.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    return {"size": path.stat().st_size, "sha256": digest}


def module(name, filename):
    spec = importlib.util.spec_from_file_location(name, SCRIPTS / filename)
    loaded = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(loaded)
    return loaded


def prepare(source, output, commit, avbtool):
    source, output = source.absolute(), output.absolute()
    if (not re.fullmatch(r"[0-9a-f]{40}", commit) or source.resolve() != source
            or output.resolve() != output or output.exists()
            or (source / "images").resolve() != source / "images"):
        raise ValueError("Require a completed local build and a new non-symlink destination")
    for name in RECEIPTS:
        regular(source / name)
    if ((source / "status").read_text().strip() != "LOCAL_BUILD_VERIFIED"
            or (source / "project-commit.txt").read_text().strip() != commit):
        raise ValueError("Build is incomplete or belongs to another commit")
    checksum_path = source / "images/SHA256SUMS"
    regular(checksum_path)
    checksum_bytes = checksum_path.read_bytes()
    expected = {}
    for line in checksum_bytes.decode("ascii").splitlines():
        match = re.fullmatch(r"([a-f0-9]{64})  \./([A-Za-z0-9._-]+)", line)
        if not match or match[2] in expected:
            raise ValueError("Invalid or duplicate snapshot checksum")
        expected[match[2]] = match[1]
    if set(expected) != IMAGES:
        raise ValueError("Snapshot must contain the complete expected image set")
    output.mkdir(mode=0o700)
    images, receipts = output / "images", output / "build-receipts"
    images.mkdir(mode=0o700)
    receipts.mkdir(mode=0o700)
    files = {}
    for name in sorted(IMAGES):
        regular(source / "images" / name)
        subprocess.run(["cp", "--reflink=auto", "--sparse=always", "--no-clobber",
                        str(source / "images" / name), str(images / name)], check=True)
        files[name] = fingerprint(images / name)
        if files[name]["sha256"] != expected[name]:
            raise ValueError(f"Snapshot checksum mismatch: {name}")
        print("Image verified:", name, flush=True)
    (images / "SHA256SUMS").write_bytes(checksum_bytes)
    receipt_hashes = {}
    for name in RECEIPTS:
        subprocess.run(["cp", "--no-clobber", str(source / name), str(receipts / name)], check=True)
        receipt_hashes[name] = fingerprint(receipts / name)["sha256"]
    inputs = module("aegis_server_inputs", "prepare-local-images.py").check_receipts(receipts, images, files)
    if set(inputs) != {"kernel_bundle", "base_generation"}:
        raise ValueError("Missing selected kernel/runtime verification")
    if (checksum_path.read_bytes() != checksum_bytes
            or (source / "status").read_text().strip() != "LOCAL_BUILD_VERIFIED"
            or (source / "project-commit.txt").read_text().strip() != commit):
        raise ValueError("Build snapshot changed during preparation")
    subprocess.run([sys.executable, str(SCRIPTS / "prepare-server-qemu.py"), str(output),
                    "--avbtool", str(avbtool.resolve(strict=True)), "--commit", commit], check=True)
    subprocess.run([sys.executable, str(SCRIPTS / "make-qemu-disk.py"), str(output)], check=True)
    report = {"source_commit": commit, "build_run": str(source),
              "status": "LOCAL_BUILD_AVB_AND_DISK_VERIFIED_NOT_BOOTED",
              "image_checksums_verified": len(files), "files": files, "inputs": inputs,
              "receipts": receipt_hashes, "avbtool": fingerprint(avbtool)}
    with (output / "build-validation.json").open("x") as stream:
        json.dump(report, stream, indent=2)
        stream.write("\n")
    print(report["status"] + ": " + str(output), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--commit", required=True)
    parser.add_argument("--avbtool", required=True, type=Path)
    args = parser.parse_args()
    prepare(args.build, args.output, args.commit, args.avbtool)
