#!/usr/bin/env python3
"""Bounded early Android boot diagnostic on the Apple Silicon Mac.

Uses original vendor and generic ramdisks. Optional test disk; no host services;
this is a porting diagnostic, not a complete Android boot or an interactive VM.
"""
import argparse
from pathlib import Path
import platform
import shlex
import struct
import subprocess


def vendor_ramdisk(path):
    data = path.read_bytes()
    if data[:8] != b"VNDRBOOT":
        raise ValueError("Expected vendor_boot.img")
    version, page = struct.unpack_from("<II", data, 8)
    if version not in (3, 4) or page not in (2048, 4096, 8192, 16384):
        raise ValueError("Unsupported vendor boot header")
    size = struct.unpack_from("<I", data, 24)[0]
    header = struct.unpack_from("<I", data, 2096)[0]
    offset = ((header + page - 1) // page) * page
    if not size or offset + size > len(data):
        raise ValueError("Truncated vendor ramdisk")
    # This product has platform and DLKM ramdisks, with no recovery fragment.
    if version == 4:
        dtb_size = struct.unpack_from("<I", data, 2100)[0]
        table_size, count, entry_size = struct.unpack_from("<III", data, 2112)
        table = offset + ((size + page - 1) // page) * page
        table += ((dtb_size + page - 1) // page) * page
        if entry_size < 108 or table + table_size > len(data) or count * entry_size > table_size:
            raise ValueError("Invalid vendor ramdisk table")
        fragments = []
        for index in range(count):
            length, start, kind = struct.unpack_from("<III", data, table + index * entry_size)
            if start + length > size:
                raise ValueError("Invalid vendor ramdisk fragment")
            if kind in (0, 1, 3):
                fragments.append(data[offset + start:offset + start + length])
        if not fragments:
            raise ValueError("No normal-boot vendor ramdisk")
        return b"".join(fragments)
    return data[offset:offset + size]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("images", type=Path)
    p.add_argument("output", type=Path, help="New directory for ramdisk, command and serial log")
    p.add_argument("--seconds", type=int, default=45)
    p.add_argument("--disk", type=Path, help="Optional GPT test disk; used in snapshot mode")
    p.add_argument("--bootconfig", type=Path, help="Additional bootloader parameters computed from the images")
    p.add_argument("--prepare-only", action="store_true", help="Write ramdisk and command without starting QEMU")
    p.add_argument("--userdata-fs", choices=["f2fs", "ext4"], default="f2fs",
                   help="Filesystem of the userdata image (AOSP release uses f2fs)")
    args = p.parse_args()
    if platform.system() != "Darwin" or platform.machine() != "arm64":
        p.error("Run on the Apple Silicon Mac")
    if not 1 <= args.seconds <= 300:
        p.error("Diagnostic duration must be 1–300 seconds")
    images = args.images.resolve()
    ramdisk = vendor_ramdisk(images / "vendor_boot.img") + (images / "ramdisk.img").read_bytes()
    config = (images / "vendor-bootconfig.img").read_bytes().rstrip(b"\0") + b"\n"
    if args.bootconfig:
        config += args.bootconfig.read_bytes().rstrip(b"\0") + b"\n"
    config += b"\0" * (-len(config) % 4)
    ramdisk += config + struct.pack("<II", len(config), sum(config)) + b"#BOOTCONFIG\n"
    args.output.mkdir(parents=True, exist_ok=False)
    output = args.output.resolve()
    (output / "initrd.img").write_bytes(ramdisk)
    command = ["qemu-system-aarch64", "-machine", "virt-11.1,gic-version=3",
               "-accel", "hvf", "-cpu", "host", "-smp", "4", "-m", "4096",
               "-nodefaults", "-display", "none", "-net", "none", "-no-reboot",
               "-serial", "stdio", "-monitor", "none", "-kernel", str(images / "kernel"),
               "-initrd", str(output / "initrd.img"), "-append",
               "console=ttyAMA0 earlycon=pl011,0x09000000 panic=1 bootconfig printk.devkmsg=on loglevel=7 "
               f"androidboot.fstab_suffix=cf.{args.userdata_fs}.hctr2 "
               "androidboot.slot_suffix=_a androidboot.boot_devices=3f000000.pcie "
               "androidboot.force_normal_boot=1"]
    if args.disk:
        command += ["-snapshot", "-drive", f"file={args.disk.resolve()},format=raw,if=none,id=android",
                    "-device", "virtio-blk-pci,drive=android"]
    (output / "command.txt").write_text(shlex.join(command) + "\n")
    if args.prepare_only:
        return
    with (output / "serial.log").open("w") as log:
        try:
            result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT,
                                    timeout=args.seconds, check=False)
            print(f"QEMU exited with status {result.returncode}; inspect the serial log.")
        except subprocess.TimeoutExpired:
            # subprocess.run kills and reaps this direct QEMU child on timeout.
            print("Diagnostic time limit reached; test VM stopped.")
    print(f"Serial log: {output / 'serial.log'}")
    print("This diagnostic does not confirm a complete Android boot.")


if __name__ == "__main__":
    main()
