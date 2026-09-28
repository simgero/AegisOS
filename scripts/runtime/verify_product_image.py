#!/usr/bin/env python3
"""Check shared-base bytes inside the delivered super image, without mounting it."""
import argparse
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile

import base
import generation
import integrate

TOOLS = ("simg2img", "lpunpack", "fsck.erofs")
MAX_IMAGE = 16 * 1024**3
INSTALLED = {"etc/aegis/runtime/base.ext4": "runtime-base.ext4",
             "etc/aegis/runtime/generation.json": "generation.json"}


def ordinary_file(path, maximum=MAX_IMAGE):
    path = Path(path).absolute()
    if any(p.is_symlink() for p in [path, *path.parents]):
        raise ValueError("Symlink in packaged-image input/output path")
    return integrate.regular(path, maximum)


def fingerprint(path, maximum=MAX_IMAGE):
    path = ordinary_file(path, maximum)
    return {"size": path.stat().st_size, "sha256": generation.sha_file(path)}


def expanded_size(image):
    """Bound conversion before invoking AOSP's complete sparse-format reader."""
    with ordinary_file(image).open("rb") as stream:
        header = stream.read(28)
    if len(header) >= 4 and struct.unpack_from("<I", header)[0] == 0xed26ff3a:
        if len(header) != 28:
            raise ValueError("Truncated Android sparse header")
        _, major, minor, file_header, chunk_header, block, count, chunks, _ = struct.unpack("<I4H4I", header)
        if (major != 1 or minor != 0 or file_header != 28 or chunk_header != 12
                or block != 4096 or not chunks or not 0 < block * count <= MAX_IMAGE):
            raise ValueError("Unexpected sparse image geometry")
        return block * count, True
    return image.stat().st_size, False


def run_tool(tool, arguments, log, cwd):
    # An inert image-file inspection. No root, mount, chroot or target execution.
    env = dict(os.environ, LC_ALL="C", LANG="C", TZ="UTC")
    for name in ("GH_TOKEN", "GITHUB_TOKEN", "CREDENTIALS_DIRECTORY"):
        env.pop(name, None)
    with log.open("ab") as output:
        try:
            subprocess.run([str(tool), *map(str, arguments)], cwd=cwd, env=env,
                           stdout=output, stderr=subprocess.STDOUT, check=True, timeout=1200,
                           umask=0o077)
        except subprocess.SubprocessError as error:
            with log.open("rb") as diagnostic:
                diagnostic.seek(max(0, log.stat().st_size - 4096))
                detail = diagnostic.read().decode(errors="replace")
            raise ValueError(f"Image reader {Path(tool).name} failed: {detail}") from error


def expected_files(receipt):
    if receipt is None:
        return None, None
    data = integrate.read(ordinary_file(receipt, integrate.MAX_TEXT))
    record = base.read_json(data)
    if record["installed"] != INSTALLED or record["inputs"]["status"] != "CHECKED_BASE_INPUTS_NOT_MOUNTED":
        raise ValueError("Unexpected selected-base receipt")
    if record["bundle_id"] != integrate.sha(generation.encoded(record["inputs"])):
        raise ValueError("Selected-base bundle does not match its inputs")
    expected = {}
    for path, name in INSTALLED.items():
        value = record["inputs"]["files"][name]
        maximum = generation.IMAGE_BYTES if name.endswith(".ext4") else integrate.MAX_TEXT
        if (type(value["size"]) is not int or not 0 < value["size"] <= maximum
                or not isinstance(value["sha256"], str)
                or not re.fullmatch(r"[0-9a-f]{64}", value["sha256"])):
            raise ValueError("Invalid selected-base file metadata")
        expected[path] = value
    return expected, {"size": len(data), "sha256": integrate.sha(data)}


def check_extracted(root, expected):
    root = integrate.ordinary(root)
    directory = root / "etc/aegis/runtime"
    if any(p.is_symlink() for p in [directory, *directory.parents]):
        raise ValueError("Symlink in packaged shared-base path")
    if expected is None:
        if directory.exists():
            raise ValueError("Unselected shared base remains in the packaged system image")
        return
    if not directory.is_dir() or {p.name for p in directory.iterdir()} != {"base.ext4", "generation.json"}:
        raise ValueError("Missing or extra shared-base files in packaged system image")
    for path, value in expected.items():
        if fingerprint(root / path, value["size"]) != value:
            raise ValueError("Packaged shared-base bytes differ from selected inputs: " + path)


def clean_temporary(path):
    # fsck applies the image's directory modes even without --preserve. Restore
    # owner access only inside this call's private temporary extraction tree.
    for directory, names, _ in os.walk(path, topdown=True, followlinks=False):
        os.chmod(directory, 0o700)
        for name in names:
            child = Path(directory) / name
            if not child.is_symlink():
                os.chmod(child, 0o700)
    shutil.rmtree(path)


def verify(super_image, tools, output, receipt=None):
    super_image = ordinary_file(super_image)
    output = Path(output).absolute()
    parent = integrate.ordinary(output.parent)
    if output.exists() or output.is_symlink():
        raise ValueError("Packaged-image report already exists; preserving it")
    if set(tools) != set(TOOLS):
        raise ValueError("Missing AOSP image inspection tools")
    tool_records = {name: fingerprint(path) for name, path in tools.items()}
    before = fingerprint(super_image)
    expected, receipt_record = expected_files(receipt)
    size, sparse = expanded_size(super_image)
    temporary = Path(tempfile.mkdtemp(prefix=".aegis-image-check-", dir=parent))
    log = temporary / "tools.log"
    try:
        raw = super_image
        if sparse:
            raw = temporary / "super.raw"
            run_tool(tools["simg2img"], [super_image, raw], log, temporary)
            if ordinary_file(raw).stat().st_size != size:
                raise ValueError("Unsparsed super image has unexpected size")
        partitions = temporary / "partitions"
        partitions.mkdir()
        # The Mac disk/boot configuration uses slot A. Never inspect a standalone
        # system_ext.img in place of the partition that QEMU will actually boot.
        run_tool(tools["lpunpack"], ["--slot=0", "--partition=system_ext_a", raw, partitions], log, temporary)
        if {p.name for p in partitions.iterdir()} != {"system_ext_a.img"}:
            raise ValueError("Unexpected logical partition extraction")
        partition = ordinary_file(partitions / "system_ext_a.img", size)
        partition_record = fingerprint(partition)
        with partition.open("rb") as image:
            image.seek(1024)
            if image.read(4) != struct.pack("<I", 0xe0f5e1e2):
                raise ValueError("Expected this product's EROFS system_ext partition")
        extracted = temporary / "root"
        # fsck reads the raw partition and checks/decompresses all file contents.
        # Do not restore host ownership/xattrs or execute anything extracted.
        run_tool(tools["fsck.erofs"], ["--no-preserve",
                                     "--extract=" + str(extracted), partition], log, temporary)
        check_extracted(extracted, expected)
        if (fingerprint(super_image) != before or fingerprint(partition) != partition_record
                or {name: fingerprint(path) for name, path in tools.items()} != tool_records
                or expected_files(receipt) != (expected, receipt_record)):
            raise ValueError("Image, tools or selection changed during packaged-image verification")
        record = {"schema": 1, "status": "PACKAGED_BASE_BYTES_VERIFIED_NOT_BOOTED" if expected is not None
                  else "PACKAGED_BASE_ABSENT_NOT_BOOTED", "super.img": before,
                  "metadata_slot": 0, "partition": "system_ext_a", "filesystem": "erofs",
                  "partition_image": partition_record, "selected_receipt": receipt_record,
                  "installed": expected, "tools": tool_records}
        with output.open("xb") as report:
            report.write(generation.encoded(record))
        return record
    finally:
        clean_temporary(temporary)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--super-image", type=Path, required=True)
    parser.add_argument("--aosp", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--receipt", type=Path)
    args = parser.parse_args()
    generation.require_builder()
    aosp = integrate.ordinary(args.aosp)
    tools = {name: (aosp / "out/host/linux-x86/bin" / name).resolve() for name in TOOLS}
    if any(aosp / "out" not in path.parents for path in tools.values()):
        raise ValueError("Image readers must come from this AOSP build's output")
    record = verify(args.super_image, tools, args.output, args.receipt)
    print(record["status"] + ": verified files inside super.img; guest boot/isolation remain unproven.")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, TypeError, KeyError, subprocess.SubprocessError) as error:
        raise SystemExit(f"Packaged-image verification failed: {error}")
