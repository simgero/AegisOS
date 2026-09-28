#!/usr/bin/env python3
"""Select a verified shared base for an AOSP product; never mount or execute it."""
import argparse
import hashlib
import importlib.util
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import uuid

import base
import generation

FILES = {"runtime-base.ext4", "generation.json", "plan.json", "fs_config.txt"}
INCLUDE = "aegis-runtime-base.mk"
MARKER = ".aegis-runtime-inputs.json"
MAX_TEXT = 16 * 1024 * 1024


def sha(data):
    return hashlib.sha256(data).hexdigest()


def ordinary(path):
    path = Path(path).absolute()
    if path.resolve() != path or not path.is_dir():
        raise ValueError("Missing directory or symlink in runtime input path")
    return path


def regular(path, maximum):
    path = Path(path)
    if path.is_symlink() or not path.is_file() or not 0 < path.stat().st_size <= maximum:
        raise ValueError("Expected a bounded regular runtime input: " + path.name)
    return path


def read(path, maximum=MAX_TEXT):
    with regular(path, maximum).open("rb") as stream:
        data = stream.read(maximum + 1)
    if not 0 < len(data) <= maximum:
        raise ValueError("Runtime input changed size while reading")
    return data


def inventory(directory):
    if {p.name for p in directory.iterdir()} - {MARKER} != FILES:
        raise ValueError("Shared base must contain exactly its four checked artifacts")
    result = {}
    for name in sorted(FILES):
        path = regular(directory / name, generation.IMAGE_BYTES if name.endswith(".ext4") else MAX_TEXT)
        result[name] = {"size": path.stat().st_size, "sha256": generation.sha_file(path)}
    return result


def load_tool(project, relative, name):
    spec = importlib.util.spec_from_file_location(name, project / relative)
    tool = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(tool)
    return tool


def inspect(run, project, imported, aosp):
    run, project, imported, aosp = map(ordinary, (run, project, imported, aosp))
    if read(run / "status", 64).strip() != b"BUILT_VERIFIED_NOT_MOUNTED":
        raise ValueError("Select a completed shared-base run, not an active or failed run")
    commit = read(run / "project-commit.txt", 64).decode().strip()
    if not re.fullmatch(r"[0-9a-f]{40}", commit):
        raise ValueError("Shared-base builder commit must be immutable")
    artifacts = ordinary(run / "artifacts")
    if (artifacts / MARKER).exists() or (artifacts / MARKER).is_symlink():
        raise ValueError("Reserved marker in original shared-base run")
    files = inventory(artifacts)
    plan = base.read_json(read(artifacts / "plan.json"))
    receipt = base.read_json(read(artifacts / "generation.json"))
    pin = base.read_json(read(project / "runtime/debian-arm64.json"))
    layout = base.read_json(read(project / "runtime/uid-map.json"))
    # A self-consistent plan/hash is not sufficient: reconstruct it from the
    # pinned original import and this project's actual recipe and UID layout.
    if plan != generation.make_plan(pin, imported, layout, project):
        raise ValueError("Shared-base plan differs from the pinned import/current recipe")
    if read(artifacts / "fs_config.txt").decode() != generation.fs_config(plan):
        raise ValueError("Shared-base ownership configuration differs from the plan")
    tools = receipt["tool_sha256"]
    if set(tools) != set(generation.TOOLS) or any(
            not isinstance(value, str) or not re.fullmatch(r"[0-9a-f]{64}", value)
            for value in tools.values()):
        raise ValueError("Invalid filesystem-tool provenance")
    input_id = sha(generation.encoded({"plan": plan["plan_sha256"], "tools": tools}))
    image_uuid = str(uuid.UUID(input_id[:32]))
    image = artifacts / "runtime-base.ext4"
    if (receipt.get("schema") != 1 or receipt.get("status") != "BUILT_VERIFIED_NOT_MOUNTED"
            or receipt.get("repeated_build_identical") is not True
            or receipt.get("plan_sha256") != plan["plan_sha256"]
            or receipt.get("recipe_input_sha256") != input_id or receipt.get("uuid") != image_uuid
            or receipt.get("image_bytes") != generation.IMAGE_BYTES
            or files[image.name]["size"] != generation.IMAGE_BYTES
            or receipt.get("image_sha256") != files[image.name]["sha256"]
            or receipt.get("generation") != files[image.name]["sha256"]):
        raise ValueError("Shared-base bytes, build receipt and plan do not agree")
    generation.check_tools_sources(aosp, project)
    tool_dir = aosp / "out/host/linux-x86/bin"
    for name, digest in tools.items():
        path = tool_dir / name
        if not path.is_file() or (aosp / "out").resolve() not in path.resolve().parents \
                or generation.sha_file(path) != digest:
            raise ValueError("Filesystem tools differ from the completed base build")
    generation.verify_superblock(image, generation.IMAGE_BYTES, image_uuid)
    # Re-read every inode/content/owner using the same checked AOSP debugfs.
    # This is an image-file inspection, not a mount or a Debian execution.
    generation.verify_image(plan, image, tool_dir / "debugfs",
                            {"PATH": str(tool_dir) + ":/usr/bin:/bin", "LC_ALL": "C", "TZ": "UTC"})
    generation.check_tools_sources(aosp, project)
    generation.check_recipe(plan, project)
    if tools != {name: generation.sha_file(tool_dir / name) for name in generation.TOOLS}:
        raise ValueError("Filesystem tools changed during shared-base inspection")
    if inventory(artifacts) != files:
        raise ValueError("Shared-base inputs changed during inspection")
    record = {"schema": 1, "status": "CHECKED_BASE_INPUTS_NOT_MOUNTED",
              "builder_commit": commit, "generation": receipt["generation"],
              "upstream_pin": pin, "plan_sha256": plan["plan_sha256"], "files": files}
    return record, artifacts


def product_baseline(project, aosp, kernel_receipt):
    tool = load_tool(project, "scripts/aosp/link-product.py", "aegis_base_product")
    source = ordinary(project / "device/aegis/qemu_arm64")
    expected = tool.inventory(source)
    if INCLUDE in expected or "aegis-runtime-kernel.mk" in expected:
        raise ValueError("Project source contains a generated product selection")
    if kernel_receipt is not None:
        kernel = load_tool(project, "scripts/kernel/integrate.py", "aegis_base_kernel")
        receipt = base.read_json(read(kernel_receipt))
        bundle = receipt["bundle_id"]
        if not isinstance(bundle, str) or not re.fullmatch(r"[0-9a-f]{64}", bundle):
            raise ValueError("Invalid selected kernel bundle")
        prefix = "device/aegis/runtime-kernels/" + bundle
        selected = {"TARGET_KERNEL_PATH": prefix + "/gki/kernel-6.12",
                    "SYSTEM_DLKM_SRC": prefix + "/gki", "KERNEL_MODULES_PATH": prefix + "/vendor"}
        if receipt["selection"] != selected:
            raise ValueError("Kernel receipt has an unexpected selection")
        expected[kernel.INCLUDE] = sha(kernel.selection_text(selected).encode())
    product = ordinary(aosp / "device/aegis/qemu_arm64")
    marker = aosp / "device/aegis/.aegis-product-link.json"
    tool.verify_existing(product, marker)
    if tool.inventory(product) != expected:
        raise ValueError("Register this project's product and selected kernel before adding a base")
    return tool, product, expected


def prepare(run, project, imported, aosp, receipt, kernel_receipt=None):
    project, aosp = ordinary(project), ordinary(aosp)
    receipt = Path(receipt).absolute()
    outputs = {"plan.json": "runtime-base-plan.json", "generation.json": "runtime-base-generation.json",
               "fs_config.txt": "runtime-base-fs_config.txt"}
    for path in [receipt, *(receipt.parent / name for name in outputs.values())]:
        if path.exists() or any(p.is_symlink() for p in [path, *path.parents]):
            raise ValueError("Runtime receipt destination exists or traverses a symlink")
    record, artifacts = inspect(run, project, imported, aosp)
    tool, current, expected = product_baseline(project, aosp, kernel_receipt)
    bundle = sha(generation.encoded(record))
    parent = tool.directory_chain(aosp, ("device", "aegis", "runtime-bases"))
    temporary = tool.directory_chain(aosp, ("out", "aegis-runtime-staging"))
    parent.mkdir(parents=True, exist_ok=True)
    temporary.mkdir(parents=True, exist_ok=True)
    destination = parent / bundle
    if destination.exists() or destination.is_symlink():
        ordinary(destination)
        if base.read_json(read(destination / MARKER)) != record or inventory(destination) != record["files"]:
            raise ValueError("Previously staged shared base changed; preserving it")
    else:
        staged = Path(tempfile.mkdtemp(prefix="base-", dir=temporary))
        try:
            for name in FILES:
                shutil.copyfile(artifacts / name, staged / name)
            if inventory(staged) != record["files"]:
                raise ValueError("Shared-base files changed while staging")
            (staged / MARKER).write_bytes(generation.encoded(record))
            if destination.exists() or destination.is_symlink():
                raise ValueError("Shared-base destination appeared during staging")
            staged.rename(destination)
        finally:
            if staged.exists():
                shutil.rmtree(staged)  # Only this invocation's fresh temporary tree.
    relative = destination.relative_to(aosp).as_posix()
    installed = {"etc/aegis/runtime/base.ext4": "runtime-base.ext4",
                 "etc/aegis/runtime/generation.json": "generation.json"}
    product = Path(tempfile.mkdtemp(prefix="product-", dir=temporary))
    try:
        for name in expected:
            target = product / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(current / name, target)
        if tool.inventory(product) != expected:
            raise ValueError("Registered product changed while adding shared base")
        text = "# Generated checked base assets; the product separately gates service activation.\n"
        for target, origin in installed.items():
            text += f"PRODUCT_COPY_FILES += {relative}/{origin}:$(TARGET_COPY_OUT_SYSTEM_EXT)/{target}\n"
        (product / INCLUDE).write_text(text)
        tool.register(product, aosp)
    finally:
        shutil.rmtree(product)
    receipt.parent.mkdir(parents=True, exist_ok=True)
    for origin, name in outputs.items():
        with (receipt.parent / name).open("xb") as target:
            target.write(read(destination / origin))
    with receipt.open("xb") as target:
        target.write(generation.encoded({"bundle_id": bundle, "inputs": record, "installed": installed}))
    return bundle


def partition_output(product_out, system_ext):
    product_out = ordinary(product_out)
    if system_ext not in {"system_ext", "system/system_ext"}:
        raise ValueError("Unexpected AOSP system_ext output location")
    return ordinary(product_out / system_ext)


def verify_absent(product_out, system_ext):
    directory = partition_output(product_out, system_ext) / "etc/aegis/runtime"
    if any(p.is_symlink() for p in [directory, *directory.parents]):
        raise ValueError("Symlink in unselected shared-base output path")
    if directory.exists() or directory.is_symlink():
        raise ValueError("Unselected runtime assets remain in product staging; clean stale build outputs first")


def verify_installed(receipt, product_out, system_ext):
    record = base.read_json(read(receipt))
    installed = {"etc/aegis/runtime/base.ext4": "runtime-base.ext4",
                 "etc/aegis/runtime/generation.json": "generation.json"}
    if record["installed"] != installed:
        raise ValueError("Unexpected installed shared-base paths")
    partition = partition_output(product_out, system_ext)
    for target, source in installed.items():
        path = partition / target
        if any(p.is_symlink() for p in [path, *path.parents]):
            raise ValueError("Symlink in installed shared-base path")
        regular(path, generation.IMAGE_BYTES)
        expected = record["inputs"]["files"][source]
        if path.stat().st_size != expected["size"] or generation.sha_file(path) != expected["sha256"]:
            raise ValueError("AOSP product staging differs from the selected shared base")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="action", required=True)
    p = sub.add_parser("prepare")
    for name in ("run", "project", "aosp", "receipt"):
        p.add_argument("--" + name, type=Path, required=True)
    p.add_argument("--kernel-receipt", type=Path)
    p = sub.add_parser("verify-installed")
    p.add_argument("--receipt", type=Path, required=True)
    p.add_argument("--product-out", type=Path, required=True)
    p.add_argument("--system-ext", required=True)
    p = sub.add_parser("verify-absent")
    p.add_argument("--product-out", type=Path, required=True)
    p.add_argument("--system-ext", required=True)
    args = parser.parse_args()
    if args.action == "prepare":
        generation.require_builder()
        pin = base.read_json(read(args.project / "runtime/debian-arm64.json"))
        imported = Path("/srv/aegis/work/runtime-imports") / pin["manifest"]["sha256"]
        print("Selected shared-base inputs:", prepare(args.run, args.project, imported, args.aosp,
                                                     args.receipt, args.kernel_receipt))
    elif args.action == "verify-installed":
        verify_installed(args.receipt, args.product_out, args.system_ext)
        print("Shared-base product staging verified; partition-image and QEMU checks remain required.")
    else:
        verify_absent(args.product_out, args.system_ext)
        print("No unselected shared-base assets in product staging.")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, TypeError, KeyError, subprocess.SubprocessError) as error:
        raise SystemExit(f"Runtime integration failed: {error}")
