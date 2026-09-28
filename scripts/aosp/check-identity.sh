#!/bin/bash
# Run ONLY on aegis-build, from a clean project commit already fetched via GitHub.
# Compiles identity modules, not a bootable image; never uploads or starts a VM.
set -Eeuo pipefail
[[ $(uname -s) == Linux && $(uname -m) == x86_64 ]] || {
    echo 'Identity modules must be compiled on the Linux x86-64 builder.' >&2; exit 1;
}
[[ $(id -un) == aegis-build ]] || {
    echo 'Run this check as the existing unprivileged aegis-build account.' >&2; exit 1;
}
commit=${1:?Usage: check-identity.sh FULL_PROJECT_COMMIT}
[[ $# == 1 && "$commit" =~ ^[0-9a-f]{40}$ ]] || exit 2
script_dir=$(cd "$(dirname "$0")" && pwd)
project=$(git -C "$script_dir" rev-parse --show-toplevel)
[[ $(git -C "$project" rev-parse HEAD) == "$commit" ]] || {
    echo 'Project checkout does not match the requested commit.' >&2; exit 1;
}
[[ -z $(git -C "$project" status --porcelain --untracked-files=all) ]] || {
    echo 'Project checkout must be clean, including untracked files.' >&2; exit 1;
}
source "$script_dir/config.sh"
unset GH_TOKEN GITHUB_TOKEN CREDENTIALS_DIRECTORY
aosp=/srv/aegis/work/aosp
exec 9>/srv/aegis/work/build.lock
flock -n 9 || { echo 'Another build holds the AOSP workspace lock.' >&2; exit 1; }
[[ $(git -C "$aosp/.repo/manifests" rev-parse HEAD) == "$AOSP_MANIFEST_COMMIT" ]] || {
    echo 'The AOSP manifest checkout does not match the pinned baseline.' >&2; exit 1;
}
run=$(mktemp -d "/srv/aegis/runs/identity-$(date -u +%Y%m%dT%H%M%SZ)-${commit:0:8}-XXXXXX")
exec > >(tee -a "$run/build.log") 2>&1
state() { printf '%s\n' "$1" > "$run/status.tmp"; mv "$run/status.tmp" "$run/status"; }
trap 'state FAILED; echo "Identity check failed; see $run/build.log"' ERR
state PREPARING
printf '%s\n' "$commit" > "$run/project-commit.txt"
python3 "$script_dir/link-product.py" "$project/device/aegis/qemu_arm64" "$aosp"
python3 "$script_dir/register-identity.py" "$project/packages/aegis/identity" "$aosp"
cp "$aosp/packages/aegis/identity/.aegis-source.json" "$run/source-files.json"
cp "$aosp/device/aegis/.aegis-product-link.json" "$run/product-source-files.json"
cd "$aosp"
prebuilts/build-tools/linux-x86/bin/nsjail -Mo \
    --disable_clone_newnet --disable_clone_newcgroup -R / -- /bin/true
# AOSP envsetup does not support nounset.
set +u
source build/envsetup.sh
lunch "$AOSP_LUNCH"
[[ $(get_build_var TARGET_PRODUCT) == aegis_qemu_arm64 ]]
jobs=$(nproc)
(( jobs <= 12 )) || jobs=12
state COMPILING
m -j"$jobs" aegis aegis-identity-service AegisIdentityTests
product=$(get_build_var PRODUCT_OUT)
[[ $(get_build_var TARGET_ARCH) == arm64 ]]
mkdir "$run/modules"
# The pinned Soong android_test installs under testcases/<module>/<device arch>/.
artifacts=(
    system/bin/aegis
    system/framework/aegis.jar
    system/framework/aegis-identity-service.jar
    testcases/AegisIdentityTests/arm64/AegisIdentityTests.apk
)
for relative in "${artifacts[@]}"; do
    test -s "$product/$relative"
    install -D -m 644 "$product/$relative" "$run/modules/$relative"
done
chmod 755 "$run/modules/system/bin/aegis"
(cd "$run/modules" && sha256sum "${artifacts[@]}") > "$run/SHA256SUMS"
state IDENTITY_COMPILED_NOT_INSTALLED
echo "Identity modules compiled: $run"
echo 'Not installed, not uploaded, and not a guest or security test.'
