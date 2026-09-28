#!/bin/bash
# Run ONLY on aegis-build, from a clean project commit already fetched via GitHub.
# Compiles identity modules, not a bootable image; never uploads or starts a VM.
set -euo pipefail
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
failed() {
    local result=$?
    trap - EXIT
    if (( result != 0 )); then
        state FAILED || echo 'Could not persist failed component status.' >&2
        echo "Identity check failed; see $run/build.log" >&2
    fi
    return "$result"
}
# An inherited ERR trap polluted envsetup command substitutions with its own
# status text (for example optional vendor/product find failures). Only a
# terminal script exit records failure; diagnostics never enter captured stdout.
trap failed EXIT
state PREPARING
printf '%s\n' "$commit" > "$run/project-commit.txt"
python3 "$script_dir/check-memory.py"
python3 "$script_dir/link-product.py" "$project/device/aegis/qemu_arm64" "$aosp"
python3 "$script_dir/register-identity.py" "$project/packages/aegis/identity" "$aosp"
python3 "$script_dir/register-runtime-storage.py" --project "$project" --aosp "$aosp" \
    --receipt "$run/runtime-storage-source.json"
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
read -r -a fs_configs <<< "$(get_build_var TARGET_FS_CONFIG_GEN)"
python3 "$project/scripts/runtime/uid_layout.py" check --aosp "$aosp" \
    --fs-config "${fs_configs[@]}"
jobs=$(nproc)
(( jobs <= 12 )) || jobs=12
state COMPILING
m -j"$jobs" aegis aegis-identity-service AegisIdentityTests services framework-res selinux_policy \
    passwd_vendor group_vendor passwd_system_ext group_system_ext \
    aegis-runtime-init aegis-runtime-setup aegis-runtime-broker AegisRuntimeNativeTests
# The broker is compiled/linked here but remains absent from product startup
# and from this version of the component transport archive.
test -s "$(get_build_var PRODUCT_OUT)/system_ext/bin/aegis-runtime-broker"
python3 "$script_dir/register-runtime-storage.py" --aosp "$aosp" \
    --receipt "$run/runtime-storage-source.json" --verify
product=$(get_build_var PRODUCT_OUT)
[[ $(get_build_var TARGET_ARCH) == arm64 ]]
# A custom wrapper is necessary: pinned Soong's default hardcodes /system.
cmp "$project/packages/aegis/identity/cli/aegis" "$product/system_ext/bin/aegis"
mkdir "$run/modules"
# The pinned android_test uses testcases/<module>/<arch>; cc_test and its
# data_bins use data/nativetest64/<module>. These are collected, never executed.
artifacts=(
    system_ext/bin/aegis
    system_ext/framework/aegis.jar
    system_ext/framework/aegis-identity-service.jar
    system/framework/services.jar
    system/framework/framework-res.apk
    vendor/etc/passwd
    vendor/etc/group
    system_ext/etc/passwd
    system_ext/etc/group
    testcases/AegisIdentityTests/arm64/AegisIdentityTests.apk
    system/bin/aegis-runtime-init
    system/bin/aegis-runtime-setup
    data/nativetest64/AegisRuntimeNativeTests/AegisRuntimeNativeTests
    data/nativetest64/AegisRuntimeNativeTests/aegis-runtime-init
    data/nativetest64/AegisRuntimeNativeTests/aegis-runtime-namespace-probe
    data/nativetest64/AegisRuntimeNativeTests/aegis-runtime-setup
)
for relative in "${artifacts[@]}"; do
    test -s "$product/$relative"
    install -D -m 644 "$product/$relative" "$run/modules/$relative"
done
chmod 755 "$run/modules/system_ext/bin/aegis"
chmod 755 "$run/modules/system/bin/aegis-runtime-init" \
    "$run/modules/system/bin/aegis-runtime-setup" \
    "$run/modules/data/nativetest64/AegisRuntimeNativeTests/AegisRuntimeNativeTests" \
    "$run/modules/data/nativetest64/AegisRuntimeNativeTests/aegis-runtime-init" \
    "$run/modules/data/nativetest64/AegisRuntimeNativeTests/aegis-runtime-namespace-probe" \
    "$run/modules/data/nativetest64/AegisRuntimeNativeTests/aegis-runtime-setup"
(cd "$run/modules" && sha256sum "${artifacts[@]}") > "$run/SHA256SUMS"
state IDENTITY_COMPILED_NOT_INSTALLED
trap - EXIT
echo "Identity modules compiled: $run"
echo 'Not installed, not uploaded, and not a guest or security test.'
