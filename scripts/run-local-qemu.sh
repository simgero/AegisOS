#!/bin/bash
# Start the verified local images with the matching development TPM helper.
set -euo pipefail
cd "$(dirname "$0")/.."
base=out/qemu-first-boot
helper=downloads/secure-env-1a9e6e3f6087d572ed5d025d78f3b34d8a90c4f2
disk=out/qemu-frp/android.raw
for file in "$base/images/kernel" "$disk" "$base/runtime.bootconfig" "$helper/SHA256SUMS"; do
    [[ -f $file ]] || { echo "Missing local test artifact: $file" >&2; exit 1; }
done
exec python3 scripts/qemu-with-secure-env.py "$base/images" "$helper" \
    "$disk" "$base/runtime.bootconfig" \
    "$base/interactive-$(date +%Y%m%dT%H%M%S)-$$" --seconds 0 --display cocoa --adb-port 15555
