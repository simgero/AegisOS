#!/bin/bash
set -eo pipefail
# Called without GitHub credentials. AOSP envsetup is not nounset-compatible.
unset GH_TOKEN GITHUB_TOKEN CREDENTIALS_DIRECTORY
script_dir=$(cd "$(dirname "$0")" && pwd)
source "$script_dir/config.sh"
# Both the GitHub snapshot and a project checkout preserve the repository layout.
project=$(cd "$script_dir/../.." && pwd)
python3 "$script_dir/check-memory.py"
if [[ -n ${AEGIS_KERNEL_RUN:-} ]]; then
    python3 "$project/scripts/kernel/integrate.py" prepare --run "$AEGIS_KERNEL_RUN" \
        --project "$project" --aosp /srv/aegis/work/aosp --receipt "$1/kernel-inputs.json"
else
    python3 "$script_dir/link-product.py" "$project/device/aegis/qemu_arm64" /srv/aegis/work/aosp
fi
if [[ -n ${AEGIS_RUNTIME_RUN:-} ]]; then
    runtime_args=()
    if [[ -n ${AEGIS_KERNEL_RUN:-} ]]; then
        runtime_args+=(--kernel-receipt "$1/kernel-inputs.json")
    fi
    python3 "$project/scripts/runtime/integrate.py" prepare --run "$AEGIS_RUNTIME_RUN" \
        --project "$project" --aosp /srv/aegis/work/aosp --receipt "$1/runtime-base-inputs.json" \
        "${runtime_args[@]}"
fi
python3 "$script_dir/register-identity.py" "$project/packages/aegis/identity" /srv/aegis/work/aosp
python3 "$script_dir/register-runtime-storage.py" --project "$project" --aosp /srv/aegis/work/aosp \
    --receipt "$1/runtime-storage-source.json"
python3 "$script_dir/register-runtime-policy.py" --project "$project" --aosp /srv/aegis/work/aosp \
    --receipt "$1/runtime-policy-source.json"
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
if [[ -n ${AEGIS_KERNEL_RUN:-} ]]; then
    python3 "$project/scripts/kernel/integrate.py" verify-selection --receipt "$1/kernel-inputs.json" \
        --kernel "$(get_build_var TARGET_KERNEL_PATH)" --system "$(get_build_var SYSTEM_DLKM_SRC)" \
        --vendor "$(get_build_var KERNEL_MODULES_PATH)"
fi
read -r -a fs_configs <<< "$(get_build_var TARGET_FS_CONFIG_GEN)"
python3 "$project/scripts/runtime/uid_layout.py" check --aosp /srv/aegis/work/aosp \
    --fs-config "${fs_configs[@]}"
# Conservative parallelism for the initial 64 GB host.
jobs=$(nproc)
(( jobs <= 16 )) || jobs=16
m -j"$jobs"
python3 "$script_dir/register-runtime-storage.py" --aosp /srv/aegis/work/aosp \
    --receipt "$1/runtime-storage-source.json" --verify
python3 "$script_dir/register-runtime-policy.py" --aosp /srv/aegis/work/aosp \
    --receipt "$1/runtime-policy-source.json" --verify
# Host-side readers for the actual delivered logical partition, built only here.
m -j"$jobs" simg2img lpunpack fsck.erofs
get_build_var PRODUCT_OUT > "$1/product-out.txt"
# Every declared audio module must be configured, including the software
# Bluetooth audio endpoint in the inherited APEX even with no HCI hardware.
python3 "$script_dir/check-audio.py" \
    "$(cat "$1/product-out.txt")/vendor/etc/audio_policy_configuration.xml" \
    hardware/interfaces/audio/aidl/default/android.hardware.audio.service-aidl.xml
cmp "$project/packages/aegis/identity/cli/aegis" \
    "$(cat "$1/product-out.txt")/system_ext/bin/aegis"
if [[ -n ${AEGIS_KERNEL_RUN:-} ]]; then
    python3 "$project/scripts/kernel/integrate.py" verify-image --receipt "$1/kernel-inputs.json" \
        --image "$(cat "$1/product-out.txt")/kernel" --boot-image "$(cat "$1/product-out.txt")/boot.img"
fi
if [[ -n ${AEGIS_RUNTIME_RUN:-} ]]; then
    python3 "$project/scripts/runtime/integrate.py" verify-installed \
        --receipt "$1/runtime-base-inputs.json" --product-out "$(cat "$1/product-out.txt")" \
        --system-ext "$(get_build_var TARGET_COPY_OUT_SYSTEM_EXT)"
else
    python3 "$project/scripts/runtime/integrate.py" verify-absent \
        --product-out "$(cat "$1/product-out.txt")" --system-ext "$(get_build_var TARGET_COPY_OUT_SYSTEM_EXT)"
fi
image_args=()
if [[ -n ${AEGIS_RUNTIME_RUN:-} ]]; then
    image_args+=(--receipt "$1/runtime-base-inputs.json")
fi
python3 "$project/scripts/runtime/verify_product_image.py" \
    --super-image "$(cat "$1/product-out.txt")/super.img" --aosp /srv/aegis/work/aosp \
    --output "$1/runtime-base-image.json" "${image_args[@]}"
printf '%s\n' "$AOSP_LUNCH" > "$1/product-target.txt"
