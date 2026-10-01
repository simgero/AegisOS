#!/bin/bash
# On-server build from an immutable git archive; no GitHub commands or uploads.
set -euo pipefail
[[ $(id -un) == aegis-build ]] || { echo 'Run as aegis-build.' >&2; exit 1; }
script_dir=$(cd "$(dirname "$0")" && pwd)
source "$script_dir/config.sh"
commit=${AEGIS_SCRIPT_COMMIT:?Required source commit}
[[ $commit =~ ^[0-9a-f]{40}$ ]] || exit 2
[[ -n ${AEGIS_KERNEL_RUN:-} && -n ${AEGIS_RUNTIME_RUN:-} ]] || {
    echo 'Select the verified runtime and kernel runs explicitly.' >&2; exit 1;
}
unset GH_TOKEN GITHUB_TOKEN CREDENTIALS_DIRECTORY
exec 9>/srv/aegis/work/build.lock
flock -n 9 || { echo 'Another build owns the AOSP workspace.' >&2; exit 1; }
[[ $(git -C /srv/aegis/work/aosp/.repo/manifests rev-parse HEAD) == "$AOSP_MANIFEST_COMMIT" ]]
run=$(mktemp -d "/srv/aegis/runs/local-$(date -u +%Y%m%dT%H%M%SZ)-${commit:0:8}-XXXXXX")
exec > >(tee -a "$run/build.log") 2>&1
state() { printf '%s\n' "$1" > "$run/status.tmp"; mv "$run/status.tmp" "$run/status"; }
failed() { result=$?; trap - EXIT; state FAILED; exit "$result"; }
trap failed EXIT
state BUILDING
printf '%s\n' "$commit" > "$run/project-commit.txt"
echo "Local build: $run"
python3 /srv/aegis/work/repo-tool/repo --help >/dev/null
(cd /srv/aegis/work/aosp; python3 /srv/aegis/work/repo-tool/repo manifest -r -o "$run/manifest.xml")
bash "$script_dir/compile.sh" "$run"
state SNAPSHOTTING
product=$(cat "$run/product-out.txt")
[[ $(cat "$run/product-target.txt") == "$AOSP_LUNCH" ]]
mkdir "$run/images"
for name in kernel boot.img init_boot.img vendor_boot.img ramdisk.img vendor-bootconfig.img \
    super.img userdata.img vbmeta.img vbmeta_system.img vbmeta_system_dlkm.img vbmeta_vendor_dlkm.img \
    system.img system_ext.img product.img vendor.img odm.img system_dlkm.img vendor_dlkm.img odm_dlkm.img; do
    test -s "/srv/aegis/work/aosp/$product/$name"
    cp --reflink=auto --sparse=always "/srv/aegis/work/aosp/$product/$name" "$run/images/$name"
done
(cd "$run/images"; sha256sum ./* > SHA256SUMS; sha256sum -c SHA256SUMS)
state LOCAL_BUILD_VERIFIED
trap - EXIT
echo "LOCAL_BUILD_VERIFIED: $run (not uploaded; guest tests still required)"
