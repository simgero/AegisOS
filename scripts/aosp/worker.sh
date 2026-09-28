#!/bin/bash
set -Eeuo pipefail
source "$(dirname "$0")/config.sh"
script_dir=$(cd "$(dirname "$0")" && pwd)
commit=${AEGIS_SCRIPT_COMMIT:?Missing script commit}
sync_only=${AEGIS_SYNC_ONLY:-0}
[[ "$sync_only" == 0 || "$sync_only" == 1 ]] || { echo 'Invalid AEGIS_SYNC_ONLY.'; exit 2; }
repo=simgero/AegisOS
run_id="aosp-$(date -u +%Y%m%dT%H%M%SZ)-${commit:0:8}-$(head -c 4 /dev/urandom | od -An -tx1 | tr -d ' \n')"
run=/srv/aegis/runs/$run_id
mkdir -p "$run/artifacts" /srv/aegis/work/aosp
exec 9>/srv/aegis/work/build.lock
flock -n 9 || { echo 'Another build holds the workspace lock.'; exit 1; }
exec > >(tee -a "$run/build.log") 2>&1
state() { printf '%s\n' "$1" > "$run/status.tmp"; mv "$run/status.tmp" "$run/status"; }
gh_call() { GH_TOKEN=$(cat "${CREDENTIALS_DIRECTORY:?}/github-token") gh "$@"; }
release_created=false
failure_state=FAILED
# Shared with later manual restarts; a retry must not immediately repeat a 429.
cooldown=/srv/aegis/work/google-retry-after
google_step() {
    local code remaining until
    if [[ -f "$cooldown" ]]; then
        until=$(cat "$cooldown")
        [[ "$until" =~ ^[0-9]+$ ]] || { echo 'Invalid Google cooldown file.'; return 1; }
        remaining=$((until - $(date +%s)))
        if (( remaining > 0 )); then
            state RATE_LIMIT_WAIT
            echo "Google cooldown: waiting $remaining seconds before any new Google request."
            sleep "$remaining"
        fi
    fi
    state SYNCING
    if "$@" 2>&1 | tee "$run/google-step.log"; then
        return 0
    else
        code=$?
    fi
    if grep -Eiq 'HTTP[^[:alnum:]]*429|returned error: 429|Too Many Requests' "$run/google-step.log"; then
        failure_state=RATE_LIMITED
        until=$(($(date +%s) + 1800))
        printf '%s\n' "$until" > "$cooldown.tmp"
        mv "$cooldown.tmp" "$cooldown"
        echo 'Google returned HTTP 429. Stopping; no automatic retry. A manual restart waits at least 30 minutes.'
    fi
    return "$code"
}
failed() {
    code=$?
    trap - EXIT TERM INT
    state "$failure_state"
    echo "Build/upload failed (exit $code). Outputs are not verified. Logs: $run"
    cp "$run/build.log" "$run/failure.log" || true
    if $release_created; then
        GH_TOKEN=$(cat "$CREDENTIALS_DIRECTORY/github-token") timeout 90 gh release upload "$run_id" \
            "$run/failure.log" "$run/status" --repo "$repo" --clobber || true
    fi
    exit "$code"
}
trap failed EXIT
trap 'exit 143' TERM
trap 'exit 130' INT
state PREPARING
# Assert filesystem behavior using only a temporary directory.
probe=$(mktemp -d /srv/aegis/work/.case-check.XXXXXX)
touch "$probe/A"
if [[ -e "$probe/a" ]]; then rm -rf "$probe"; echo 'Case-sensitive filesystem required.'; exit 1; fi
rm -rf "$probe"
cat > "$run/notes.md" <<NOTES
AOSP baseline: $AOSP_TAG
Target: $AOSP_LUNCH
Builder commit: $commit
Kernel input run: ${AEGIS_KERNEL_RUN:-pinned AOSP prebuilts}
Shared-base input run: ${AEGIS_RUNTIME_RUN:-none selected}

Development userdebug build with AOSP test keys. Not a production security release.
Local boot on Apple Silicon has not yet been verified.
Download all images.tar.xz.part-* files and SHA256SUMS, then run:

    sha256sum -c SHA256SUMS
    cat images.tar.xz.part-* | tar -xJf -

Source pins are recorded in manifest.xml; package versions in packages.txt.
NOTES
if [[ "$sync_only" == 0 ]]; then
    gh_call release create "$run_id" --repo "$repo" --target "$commit" --draft \
        --title "$run_id" --notes-file "$run/notes.md"
    release_created=true
fi
cp "$script_dir/"*.sh "$run/artifacts/"
printf '%s\n' "$commit" > "$run/artifacts/builder-commit.txt"
dpkg-query -W > "$run/artifacts/packages.txt"
state SYNCING
# Fetch the repo launcher from an immutable commit, rather than an unversioned script.
mkdir -p /srv/aegis/work/repo-tool
if [[ ! -d /srv/aegis/work/repo-tool/.git ]]; then git -C /srv/aegis/work/repo-tool init; fi
google_step git -C /srv/aegis/work/repo-tool fetch --depth=1 https://android.googlesource.com/tools/repo "$REPO_COMMIT"
git -C /srv/aegis/work/repo-tool checkout --detach "$REPO_COMMIT"
repo_tool=/srv/aegis/work/repo-tool/repo
cd /srv/aegis/work/aosp
export GIT_TERMINAL_PROMPT=0
export GIT_CONFIG_COUNT=2 GIT_CONFIG_KEY_0=user.name GIT_CONFIG_VALUE_0=AegisOS \
    GIT_CONFIG_KEY_1=user.email GIT_CONFIG_VALUE_1=build@aegisos.invalid
google_step python3 "$repo_tool" init -u https://android.googlesource.com/platform/manifest \
    -b "$AOSP_MANIFEST_COMMIT" --depth=1 \
    --repo-url=https://android.googlesource.com/tools/repo \
    --repo-rev="$REPO_COMMIT" --no-clone-bundle
google_step python3 "$repo_tool" sync -c -j1 --jobs-network=1 --jobs-checkout=1 \
    --retry-fetches=0 --no-clone-bundle --fail-fast
python3 "$repo_tool" manifest -r -o "$run/artifacts/manifest.xml"
if [[ "$sync_only" == 1 ]]; then
    state SOURCES_READY
    trap - EXIT TERM INT
    echo "SOURCES_READY: $run (no compilation or release upload performed)"
    exit 0
fi
state BUILDING
# Keep the credential directory and token out of the compiler's environment.
env -u CREDENTIALS_DIRECTORY -u GH_TOKEN bash "$script_dir/compile.sh" "$run"
state PACKAGING
product=$(cat "$run/product-out.txt")
[[ -d "$product" ]] || { echo 'Missing product output.'; exit 1; }
[[ $(cat "$run/product-target.txt") == "$AOSP_LUNCH" ]] || { echo 'Unexpected compiled product.'; exit 1; }
for required in kernel boot.img init_boot.img vendor_boot.img super.img userdata.img vbmeta.img; do
    [[ -s "$product/$required" ]] || { echo "Missing QEMU bring-up artifact: $required"; exit 1; }
done
cp "$run/product-target.txt" "$run/artifacts/product-target.txt"
cp "$run/runtime-base-image.json" "$run/artifacts/runtime-base-image.json"
cp "$run/runtime-storage-source.json" "$run/artifacts/runtime-storage-source.json"
if [[ -n ${AEGIS_KERNEL_RUN:-} ]]; then
    cp "$run/kernel-inputs.json" "$run/artifacts/kernel-inputs.json"
fi
if [[ -n ${AEGIS_RUNTIME_RUN:-} ]]; then
    for name in runtime-base-inputs.json runtime-base-plan.json runtime-base-generation.json runtime-base-fs_config.txt; do
        cp "$run/$name" "$run/artifacts/$name"
    done
fi
# Compilation has finished; use available CPUs for this separate phase, capped
# at eight. XZ level 1 has modest per-thread memory needs on the >=64 GB builder.
compression_jobs=$(nproc)
(( compression_jobs <= 8 )) || compression_jobs=8
echo "Packaging images with $compression_jobs compression threads."
# Only deliver top-level runtime files, not large obj/ intermediates or unpacked trees.
(cd "$product"; find -L . -maxdepth 1 -type f -print0 | sort -z | tar -h --null -T - -cf -) \
    | xz -T"$compression_jobs" -1 | split -b 1900M -d -a 4 - "$run/artifacts/images.tar.xz.part-"
cp "$run/build.log" "$run/artifacts/build.log"
cp "$run/notes.md" "$run/artifacts/README.md"
(cd "$run/artifacts"; sha256sum ./* > SHA256SUMS)
state UPLOADING
for asset in "$run/artifacts/"*; do
    uploaded=false
    for attempt in 1 2 3; do
        if gh_call release upload "$run_id" "$asset" --repo "$repo" --clobber; then uploaded=true; break; fi
        sleep 5
    done
    $uploaded || exit 1
done
state VERIFYING
# Verify the actual stored bytes, one asset at a time to bound disk use.
mkdir "$run/verify"
for asset in "$run/artifacts/"*; do
    name=$(basename "$asset")
    gh_call release download "$run_id" --repo "$repo" --pattern "$name" --dir "$run/verify"
    cmp "$asset" "$run/verify/$name"
    rm "$run/verify/$name"
done
printf '\nAll assets were downloaded again and compared byte for byte. Upload verified.\n' >> "$run/notes.md"
gh_call release edit "$run_id" --repo "$repo" --draft=false --latest=false --notes-file "$run/notes.md"
state UPLOAD_VERIFIED
trap - EXIT TERM INT
echo "UPLOAD_VERIFIED: https://github.com/$repo/releases/tag/$run_id"
echo 'Persistent builder: retain sources and outputs for subsequent builds. Boot is not yet verified.'
