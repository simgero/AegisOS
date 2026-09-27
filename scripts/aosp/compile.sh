#!/bin/bash
set -eo pipefail
# Called without GitHub credentials. AOSP envsetup is not nounset-compatible.
script_dir=$(cd "$(dirname "$0")" && pwd)
source "$script_dir/config.sh"
# Bootstrap stores immutable product files beside the worker; a Git checkout
# stores them at the repository root.
device_source="$script_dir/device"
[[ -d "$device_source" ]] || device_source="$script_dir/../../device/aegis/qemu_arm64"
python3 "$script_dir/link-product.py" "$device_source" /srv/aegis/work/aosp
cd /srv/aegis/work/aosp
# Trusty's build invokes nsjail even when Soong falls back without sandboxing.
# Check the actual executable as the build account before expensive compilation.
if ! prebuilts/build-tools/linux-x86/bin/nsjail -Mo \
    --disable_clone_newnet --disable_clone_newcgroup -R / -- /bin/true; then
    echo 'AOSP nsjail preflight failed; check the builder AppArmor profile before retrying.' >&2
    exit 1
fi
source build/envsetup.sh
lunch "$AOSP_LUNCH"
[[ $(get_build_var TARGET_PRODUCT) == aegis_qemu_arm64 ]] || {
    echo 'Unexpected AOSP product; refusing to compile.' >&2; exit 1;
}
# Conservative parallelism for the initial 64 GB host.
jobs=$(nproc)
(( jobs <= 16 )) || jobs=16
m -j"$jobs"
get_build_var PRODUCT_OUT > "$1/product-out.txt"
printf '%s\n' "$AOSP_LUNCH" > "$1/product-target.txt"
