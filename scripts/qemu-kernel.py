#!/usr/bin/env python3
"""Probe local HVF or start an ARM64 kernel diagnostic (not a full Android boot)."""
import argparse
from pathlib import Path
import platform
import shlex
import shutil
import subprocess


def command(kernel=None, probe=False):
    args = ["qemu-system-aarch64", "-machine", "virt-11.1,gic-version=3",
            "-accel", "hvf", "-cpu", "host", "-smp", "2", "-m", "2048",
            "-nodefaults", "-display", "none", "-net", "none", "-no-reboot"]
    if probe:
        return args + ["-S", "-serial", "none", "-monitor", "stdio"]
    path = Path(kernel).resolve()
    with path.open("rb") as source:
        header = source.read(64)
    if len(header) != 64 or header[56:60] != b"ARM\x64":
        raise ValueError("Expected an uncompressed ARM64 Linux Image, not boot.img or a compressed kernel")
    return args + ["-serial", "stdio", "-monitor", "none", "-kernel", str(path),
                   "-append", "console=ttyAMA0 earlycon=pl011,0x09000000 panic=0"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--probe", action="store_true", help="Create and close a paused VM; no guest execution")
    mode.add_argument("--kernel", type=Path, help="Uncompressed ARM64 Image from our build")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()
    try:
        argv = command(args.kernel, args.probe)
    except (ValueError, OSError) as error:
        parser.error(str(error))
    if args.dry_run:
        print(shlex.join(argv))
        return
    if platform.system() != "Darwin" or platform.machine() != "arm64":
        parser.error("Run on the Apple Silicon development Mac")
    if not shutil.which(argv[0]):
        parser.error("qemu-system-aarch64 is missing")
    if args.probe:
        subprocess.run(argv, input="quit\n", text=True, check=True, timeout=30)
        print("QEMU/HVF machine creation succeeded. No kernel or Android was booted.")
    else:
        print("Kernel diagnostic only: no Android ramdisk or disks attached; a root-mount failure is expected.", flush=True)
        subprocess.run(argv, check=True)


if __name__ == "__main__":
    main()
