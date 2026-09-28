#!/usr/bin/env python3
"""Inspect a matched Kleaf output and select it for an AOSP build; never boot it."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import shutil
import struct
import tempfile
import zlib

MAX_IMAGE = 512 * 1024 * 1024
MAX_MODULE = 64 * 1024 * 1024
MAX_TEXT = 8 * 1024 * 1024
MAX_TOTAL = 2 * 1024 * 1024 * 1024
INCLUDE = "aegis-runtime-kernel.mk"
RECORD = ".aegis-kernel-inputs.json"
REQUIRED_CONFIG = {
    "CONFIG_ARM64", "CONFIG_ARM64_4K_PAGES", "CONFIG_IKCONFIG", "CONFIG_IKCONFIG_PROC",
    "CONFIG_NAMESPACES", "CONFIG_USER_NS", "CONFIG_PID_NS", "CONFIG_UTS_NS", "CONFIG_NET_NS",
    "CONFIG_SYSVIPC", "CONFIG_IPC_NS", "CONFIG_POSIX_MQUEUE", "CONFIG_SECURITY",
    "CONFIG_SECURITY_SELINUX", "CONFIG_SECCOMP", "CONFIG_SECCOMP_FILTER", "CONFIG_MODULES",
    "CONFIG_MODULE_SIG", "CONFIG_MODVERSIONS", "CONFIG_FS_ENCRYPTION", "CONFIG_DM_VERITY",
}
BOOT_GKI = {"virtio_blk.ko", "virtio_console.ko", "virtio_pci.ko",
            "vmw_vsock_virtio_transport.ko"}
BOOT_VENDOR = {"failover.ko", "nd_virtio.ko", "net_failover.ko", "virtio_dma_buf.ko",
               "virtio-gpu.ko", "virtio_input.ko", "virtio_net.ko", "virtio-rng.ko"}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def read(path, limit):
    path = Path(path)
    if path.is_symlink() or not path.is_file() or not 0 < path.stat().st_size <= limit:
        raise ValueError(f"Expected a bounded regular kernel input: {path.name}")
    with path.open("rb") as stream:
        data = stream.read(limit + 1)
    if not 0 < len(data) <= limit:
        raise ValueError("Kernel input changed size while reading")
    return data


def ordinary(path):
    path = Path(path).absolute()
    if path.resolve() != path or not path.is_dir():
        raise ValueError("Kernel input directory is missing or contains a symlink")
    return path


def configuration(data):
    values = {}
    for line in data.decode("utf-8").splitlines():
        if line.startswith("CONFIG_") and "=" in line:
            key, value = line.split("=", 1)
        elif re.fullmatch(r"# CONFIG_[A-Z0-9_]+ is not set", line):
            key, value = line[2:-11], "n"
        elif not line or line.startswith("#"):
            continue
        else:
            raise ValueError("Unexpected kernel configuration line")
        if key in values:
            raise ValueError("Duplicate kernel configuration key")
        values[key] = value
    return values


def image_metadata(data):
    if len(data) < 64 or data[56:60] != b"ARM\x64":
        raise ValueError("Expected the uncompressed ARM64 kernel Image")
    # Check the config embedded in the actual executable, not only a sidecar.
    marker = b"IKCFG_ST"
    if data.count(marker) != 1:
        raise ValueError("Kernel has no unambiguous embedded configuration")
    start = data.index(marker) + len(marker)
    decoder = zlib.decompressobj(16 + zlib.MAX_WBITS)
    config = decoder.decompress(data[start:], MAX_TEXT + 1)
    if (len(config) > MAX_TEXT or not decoder.eof
            or not decoder.unused_data.startswith(b"IKCFG_ED")):
        raise ValueError("Embedded kernel configuration is corrupt or too large")
    releases = set(re.findall(rb"Linux version ([A-Za-z0-9._+\-]{1,128}) ", data))
    if len(releases) != 1:
        raise ValueError("Kernel has no unambiguous release banner")
    return releases.pop().decode("ascii"), configuration(config)


def module_vermagic(data):
    # Read ELF metadata only. No module is loaded or executed, including in tests.
    if len(data) < 64 or data[:7] != b"\x7fELF\x02\x01\x01":
        raise ValueError("Expected a little-endian ELF64 ARM64 module")
    header = struct.unpack_from("<16sHHIQQQIHHHHHH", data)
    if header[1:4] != (1, 183, 1) or header[8] != 64 or header[11] != 64:
        raise ValueError("Kernel module has the wrong ELF type or architecture")
    offset, count, strings = header[6], header[12], header[13]
    if not 0 < strings < count <= 4096 or offset < 64 or offset + count * 64 > len(data):
        raise ValueError("Invalid module section table")
    sections = [struct.unpack_from("<IIQQQQIIQQ", data, offset + i * 64) for i in range(count)]

    def content(section):
        begin, size = section[4:6]
        if begin + size > len(data):
            raise ValueError("Module section escapes its file")
        return data[begin:begin + size]

    if sections[strings][1] != 3:
        raise ValueError("Invalid module section-name table")
    names = content(sections[strings])
    infos = []
    for section in sections:
        name_offset = section[0]
        if name_offset >= len(names) or b"\0" not in names[name_offset:]:
            raise ValueError("Invalid module section name")
        name = names[name_offset:].split(b"\0", 1)[0]
        if name == b".modinfo":
            if section[1] != 1 or section[5] > MAX_TEXT:
                raise ValueError("Invalid module information section")
            infos.append(content(section))
    if len(infos) != 1:
        raise ValueError("Missing or duplicate module information")
    values = [value[9:] for value in infos[0].split(b"\0") if value.startswith(b"vermagic=")]
    if len(values) != 1 or not values[0] or len(values[0]) > 256:
        raise ValueError("Missing or duplicate module version")
    return values[0].decode("ascii")


def boot_kernel(data):
    # This product uses the fixed 4096-byte page size of boot header v4.
    # Extract only for byte comparison; AVB/signature verification is separate.
    if (len(data) < 4096 or data[:8] != b"ANDROID!"
            or struct.unpack_from("<I", data, 40)[0] != 4
            or struct.unpack_from("<I", data, 20)[0] != 1584):
        raise ValueError("Expected this product's Android boot header v4")
    size = struct.unpack_from("<I", data, 8)[0]
    if not 0 < size <= MAX_IMAGE or 4096 + size > len(data):
        raise ValueError("Truncated or invalid boot kernel payload")
    return data[4096:4096 + size]


def load_tool(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def inspect(run, project):
    run, project = ordinary(run), ordinary(project)
    gki, vendor = ordinary(run / "gki"), ordinary(run / "virtual-device")
    if read(run / "status", 64).strip() != b"BUILT_UNVERIFIED":
        raise ValueError("Select a completed kernel build, not an active or failed run")
    commit = read(run / "builder-commit.txt", 64).decode().strip()
    if not re.fullmatch(r"[0-9a-f]{40}", commit):
        raise ValueError("Missing immutable kernel project commit")
    inputs = {}
    origins = {}

    def include(name, source, limit):
        data = read(source, limit)
        if not re.fullmatch(r"[A-Za-z0-9._/-]+", name) or ".." in Path(name).parts:
            raise ValueError("Unexpected kernel artifact name")
        inputs[name] = {"sha256": sha(data), "size": len(data)}
        origins[name] = source
        return data

    for name in ("manifest.xml", "aegis_runtime_defconfig"):
        data = include("provenance/" + name, run / name, MAX_TEXT)
        if data != read(project / "kernel" / name, MAX_TEXT):
            raise ValueError("Kernel build sources/fragment differ from this project")
    source_manifest = load_tool(project / "scripts/kernel/source_manifest.py", "aegis_kernel_sources")
    pins = source_manifest.projects(run / "manifest.xml")
    if source_manifest.projects(run / "resolved-manifest.xml") != pins:
        raise ValueError("Resolved kernel sources differ from the pinned manifest")
    include("provenance/resolved-manifest.xml", run / "resolved-manifest.xml", MAX_TEXT)
    image = include("gki/kernel-6.12", gki / "Image", MAX_IMAGE)
    if sha(read(vendor / "Image", MAX_IMAGE)) != sha(image):
        raise ValueError("Virtual-device and GKI outputs contain different kernels")
    release, embedded = image_metadata(image)
    if not release.startswith("6.12."):
        raise ValueError("The selected Android product requires the pinned 6.12 kernel family")
    config = include("provenance/gki.config", gki / ".config", MAX_TEXT)
    if configuration(config) != embedded:
        raise ValueError("Sidecar configuration does not match the executable kernel")
    if any(embedded.get(key) != "y" for key in REQUIRED_CONFIG):
        raise ValueError("Kernel lacks required namespace or Android security configuration")
    include("provenance/Module.symvers", gki / "Module.symvers", MAX_TEXT)
    module_names = set()
    gki_names = set()
    versions = set()
    for prefix, directory in (("gki", gki), ("vendor", vendor)):
        modules = sorted(directory.glob("*.ko"))
        if not modules:
            raise ValueError("Kernel output is missing its modules")
        for path in modules:
            data = read(path, MAX_MODULE)
            if not re.fullmatch(r"[A-Za-z0-9_-]+\.ko", path.name):
                raise ValueError("Non-portable kernel module name")
            version = module_vermagic(data)
            if version.split(" ", 1)[0] != release:
                raise ValueError("Module release differs from its kernel")
            versions.add(version)
            if prefix == "vendor" and path.name in gki_names:
                if sha(data) != inputs["gki/" + path.name]["sha256"]:
                    raise ValueError("Conflicting GKI module in virtual-device output")
                continue  # Same GKI bytes belong in SYSTEM_DLKM_SRC, never both sets.
            include(prefix + "/" + path.name, path, MAX_MODULE)
            module_names.add(path.name)
            if prefix == "gki":
                gki_names.add(path.name)
    if len(versions) != 1:
        raise ValueError("Module build flags/vermagic differ within the selected set")
    vendor_names = module_names - gki_names
    if not BOOT_GKI <= gki_names or not BOOT_VENDOR <= vendor_names:
        raise ValueError("Selected modules do not cover the product's early boot drivers")
    if sum(entry["size"] for entry in inputs.values()) > MAX_TOTAL:
        raise ValueError("Kernel input set exceeds its size limit")
    record = {"schema": 1, "status": "CHECKED_INPUTS_NOT_BOOTED", "project_commit": commit,
              "kernel_release": release, "vermagic": versions.pop(), "source_pins": pins,
              "files": inputs}
    bundle_id = sha(json.dumps(record, sort_keys=True, separators=(",", ":")).encode())
    return bundle_id, record, origins


def inventory(root):
    result = {}
    for path in root.rglob("*"):
        if path.is_symlink():
            raise ValueError("Kernel staging contains a symlink")
        if path.is_dir():
            continue
        relative = path.relative_to(root).as_posix()
        if relative == RECORD:
            continue
        data = read(path, MAX_IMAGE)
        result[relative] = {"sha256": sha(data), "size": len(data)}
    return result


def prepare(run, project, aosp, receipt):
    project, aosp = ordinary(project), ordinary(aosp)
    receipt = Path(receipt)
    if receipt.exists() or receipt.is_symlink():
        raise ValueError("Kernel receipt already exists; preserve it for inspection")
    bundle_id, record, origins = inspect(run, project)
    product_tool = load_tool(project / "scripts/aosp/link-product.py", "aegis_kernel_product")
    parent = product_tool.directory_chain(aosp, ("device", "aegis", "runtime-kernels"))
    temporary = product_tool.directory_chain(aosp, ("out", "aegis-kernel-staging"))
    parent.mkdir(parents=True, exist_ok=True)
    temporary.mkdir(parents=True, exist_ok=True)
    destination = parent / bundle_id
    if destination.exists() or destination.is_symlink():
        ordinary(destination)
        if json.loads(read(destination / RECORD, MAX_TEXT)) != record or inventory(destination) != record["files"]:
            raise ValueError("Existing kernel inputs changed; refusing to replace them")
    else:
        staged = Path(tempfile.mkdtemp(prefix="inputs-", dir=temporary))
        try:
            for name, source in origins.items():
                target = staged / name
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(source, target)
            if inventory(staged) != record["files"]:
                raise ValueError("Kernel inputs changed during staging")
            (staged / RECORD).write_text(json.dumps(record, sort_keys=True) + "\n")
            if destination.exists() or destination.is_symlink():
                raise ValueError("Kernel destination appeared during staging")
            staged.rename(destination)
        finally:
            if staged.exists():
                shutil.rmtree(staged)  # Only this invocation's private staging directory.
    relative = destination.relative_to(aosp).as_posix()
    # Static variable names and a hash-only generated path: no caller-supplied make syntax.
    selection = {"TARGET_KERNEL_PATH": relative + "/gki/kernel-6.12",
                 "SYSTEM_DLKM_SRC": relative + "/gki", "KERNEL_MODULES_PATH": relative + "/vendor"}
    source = project / "device/aegis/qemu_arm64"
    expected = product_tool.inventory(source)
    if INCLUDE in expected:
        raise ValueError("Project contains a generated kernel selection; review before replacing")
    product = Path(tempfile.mkdtemp(prefix="product-", dir=temporary))
    try:
        for name in expected:
            target = product / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source / name, target)
        if product_tool.inventory(product) != expected:
            raise ValueError("Product sources changed during staging")
        text = "# Generated from checked kernel inputs; never edit this installed file.\n"
        text += "TARGET_KERNEL_USE := 6.12\n"
        text += "".join(f"{name} := {value}\n" for name, value in selection.items())
        (product / INCLUDE).write_text(text)
        product_tool.register(product, aosp)
    finally:
        shutil.rmtree(product)
    receipt.parent.mkdir(parents=True, exist_ok=True)
    with receipt.open("x") as output:
        json.dump({"bundle_id": bundle_id, "selection": selection, "inputs": record}, output, sort_keys=True)
        output.write("\n")
    return bundle_id


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    prepare_args = sub.add_parser("prepare")
    prepare_args.add_argument("--run", type=Path, required=True)
    prepare_args.add_argument("--project", type=Path, required=True)
    prepare_args.add_argument("--aosp", type=Path, required=True)
    prepare_args.add_argument("--receipt", type=Path, required=True)
    verify_args = sub.add_parser("verify-selection")
    verify_args.add_argument("--receipt", type=Path, required=True)
    verify_args.add_argument("--kernel", required=True)
    verify_args.add_argument("--system", required=True)
    verify_args.add_argument("--vendor", required=True)
    image_args = sub.add_parser("verify-image")
    image_args.add_argument("--receipt", type=Path, required=True)
    image_args.add_argument("--image", type=Path, required=True)
    image_args.add_argument("--boot-image", type=Path, required=True)
    args = parser.parse_args()
    if args.command == "prepare":
        print("Kernel inputs selected:", prepare(args.run, args.project, args.aosp, args.receipt))
    else:
        record = json.loads(read(args.receipt, MAX_TEXT))
        if args.command == "verify-selection":
            actual = {"TARGET_KERNEL_PATH": args.kernel, "SYSTEM_DLKM_SRC": args.system,
                      "KERNEL_MODULES_PATH": args.vendor}
            if actual != record["selection"]:
                raise ValueError("AOSP did not select the complete checked kernel/module set")
        else:
            expected = record["inputs"]["files"]["gki/kernel-6.12"]["sha256"]
            if sha(read(args.image, MAX_IMAGE)) != expected:
                raise ValueError("Compiled product contains a different kernel")
            if sha(boot_kernel(read(args.boot_image, MAX_IMAGE))) != expected:
                raise ValueError("Compiled boot image contains a different kernel")
        print("Kernel selection check passed; boot and module-loading tests still required.")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, TypeError, KeyError, struct.error, zlib.error) as error:
        raise SystemExit(f"Kernel integration failed: {error}")
