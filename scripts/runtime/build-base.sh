#!/bin/bash
# Run only on aegis-build, after this exact clean project commit arrived via GitHub.
# Host filesystem tools only: never runs Debian binaries, a VM or a package script.
set -Eeuo pipefail
[[ $(uname -s) == Linux && $(uname -m) == x86_64 && $(id -un) == aegis-build ]] || {
    echo 'Build the shared base as aegis-build on the Linux x86-64 builder.' >&2; exit 1;
}
commit=${1:?Usage: build-base.sh FULL_PROJECT_COMMIT}
[[ $# == 1 && "$commit" =~ ^[0-9a-f]{40}$ ]] || exit 2
project=$(cd "$(dirname "$0")/../.." && pwd)
[[ $(git -C "$project" rev-parse HEAD) == "$commit" &&
   -z $(git -C "$project" status --porcelain --untracked-files=all) ]] || {
    echo 'The project must be clean and match the requested GitHub commit.' >&2; exit 1;
}
source "$project/scripts/aosp/config.sh"
unset GH_TOKEN GITHUB_TOKEN CREDENTIALS_DIRECTORY
aosp=/srv/aegis/work/aosp
exec 9>/srv/aegis/work/build.lock
flock -n 9 || { echo 'Another build holds the AOSP workspace lock.' >&2; exit 1; }
[[ $(git -C "$aosp/.repo/manifests" rev-parse HEAD) == "$AOSP_MANIFEST_COMMIT" ]] || {
    echo 'AOSP manifest differs from its pin.' >&2; exit 1;
}
run=$(mktemp -d "/srv/aegis/runs/runtime-base-$(date -u +%Y%m%dT%H%M%SZ)-${commit:0:8}-XXXXXX")
exec > >(tee -a "$run/build.log") 2>&1
state() { printf '%s\n' "$1" > "$run/status.tmp"; mv "$run/status.tmp" "$run/status"; }
trap 'code=$?; trap - EXIT; state FAILED; echo "Shared base failed ($code); logs: $run"; exit "$code"' EXIT
trap 'exit 143' TERM
trap 'exit 130' INT
state PREPARING
printf '%s\n' "$commit" > "$run/project-commit.txt"
python3 "$project/scripts/runtime/generation.py" check-tools --aosp "$aosp"
python3 "$project/scripts/runtime/uid_layout.py" check
# Keep the verified upstream import outside the clean project checkout.
manifest_sha=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["manifest"]["sha256"])' \
    "$project/runtime/debian-arm64.json")
imported="/srv/aegis/work/runtime-imports/$manifest_sha"
python3 "$project/scripts/runtime/base.py" fetch "$imported"
python3 "$project/scripts/runtime/uid_layout.py" check --base "$imported"
python3 "$project/scripts/aosp/link-product.py" "$project/device/aegis/qemu_arm64" "$aosp"
cd "$aosp"
prebuilts/build-tools/linux-x86/bin/nsjail -Mo \
    --disable_clone_newnet --disable_clone_newcgroup -R / -- /bin/true
set +u
source build/envsetup.sh
lunch "$AOSP_LUNCH"
[[ $(get_build_var TARGET_PRODUCT) == aegis_qemu_arm64 && $(get_build_var TARGET_ARCH) == arm64 ]]
jobs=$(nproc)
(( jobs <= 12 )) || jobs=12
state BUILDING_TOOLS
m -j"$jobs" mkuserimg_mke2fs mke2fs e2fsdroid e2fsck debugfs
state ASSEMBLING
python3 "$project/scripts/runtime/generation.py" build --aosp "$aosp" \
    --imported "$imported" --output "$run/artifacts"
[[ $(git -C "$project" rev-parse HEAD) == "$commit" &&
   -z $(git -C "$project" status --porcelain --untracked-files=all) ]] || {
    echo 'Project changed during the shared-base build; results remain unapproved.' >&2; exit 1;
}
(cd "$run/artifacts" && sha256sum runtime-base.ext4 generation.json plan.json fs_config.txt) > "$run/SHA256SUMS"
state BUILT_VERIFIED_NOT_MOUNTED
trap - EXIT TERM INT
echo "Shared base prepared: $run"
echo 'Not published, installed or mounted. Transport through GitHub; runtime tests only in local QEMU.'
