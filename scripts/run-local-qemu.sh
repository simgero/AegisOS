#!/bin/bash
# Reopen the paired persistent profile of the locally booted development image.
# No automatic profile creation or migration; both Android and TPM state remain.
set -euo pipefail
cd "$(dirname "$0")/.."
base=out/full-build-2a766ab5
prepared=$base/prepared
helper=downloads/secure-env-20260928T134804Z-024354c1
disk=$prepared/android.raw
profile=out/qemu-profiles/foundation-2a766ab5
for file in "$prepared/images/kernel" "$disk" "$prepared/runtime.bootconfig" \
    "$prepared/avb-checked.json" "$profile/profile.json" "$helper/SHA256SUMS"; do
    [[ -f $file ]] || { echo "Missing local test artifact: $file" >&2; exit 1; }
done
exec python3 scripts/qemu-with-secure-env.py "$prepared/images" "$helper" \
    "$disk" "$prepared/runtime.bootconfig" \
    "$base/interactive-$(date +%Y%m%dT%H%M%S)-$$" \
    --profile "$profile" --seconds 0 --display cocoa --adb-port 15755
