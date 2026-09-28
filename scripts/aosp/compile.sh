#!/bin/bash
set -eo pipefail
# Called without GitHub credentials. AOSP envsetup is not nounset-compatible.
unset GH_TOKEN GITHUB_TOKEN CREDENTIALS_DIRECTORY
script_dir=$(cd "$(dirname "$0")" && pwd)
source "$script_dir/config.sh"
# Both the GitHub snapshot and a project checkout preserve the repository layout.
project=$(cd "$script_dir/../.." && pwd)
python3 "$script_dir/link-product.py" "$project/device/aegis/qemu_arm64" /srv/aegis/work/aosp
python3 "$script_dir/register-identity.py" "$project/packages/aegis/identity" /srv/aegis/work/aosp
cp /srv/aegis/work/aosp/packages/aegis/identity/.aegis-source.json "$1/identity-source-files.json"
cp /srv/aegis/work/aosp/device/aegis/.aegis-product-link.json "$1/product-source-files.json"
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
