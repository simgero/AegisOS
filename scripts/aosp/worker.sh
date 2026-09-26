#!/bin/bash
set -Eeuo pipefail
source "$(dirname "$0")/config.sh"
script_dir=$(cd "$(dirname "$0")" && pwd)
commit=${AEGIS_SCRIPT_COMMIT:?Missing script commit}
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
failed() {
    code=$?
    trap - EXIT TERM INT
    state FAILED
    echo "Build/upload failed (exit $code). No deletion clearance. Logs: $run"
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

Development userdebug build with AOSP test keys. Not a production security release.
Local boot on Apple Silicon has not yet been verified.
Download all images.tar.xz.part-* files and SHA256SUMS, then run:

    sha256sum -c SHA256SUMS
    cat images.tar.xz.part-* | tar -xJf -

Source pins are recorded in manifest.xml; package versions in packages.txt.
NOTES
gh_call release create "$run_id" --repo "$repo" --target "$commit" --draft \
    --title "$run_id" --notes-file "$run/notes.md"
release_created=true
cp "$script_dir/"*.sh "$run/artifacts/"
printf '%s\n' "$commit" > "$run/artifacts/builder-commit.txt"
dpkg-query -W > "$run/artifacts/packages.txt"
state SYNCING
# Fetch the repo launcher from an immutable commit, rather than an unversioned script.
mkdir -p /srv/aegis/work/repo-tool
if [[ ! -d /srv/aegis/work/repo-tool/.git ]]; then git -C /srv/aegis/work/repo-tool init; fi
git -C /srv/aegis/work/repo-tool fetch --depth=1 https://android.googlesource.com/tools/repo "$REPO_COMMIT"
git -C /srv/aegis/work/repo-tool checkout --detach "$REPO_COMMIT"
repo_tool=/srv/aegis/work/repo-tool/repo
cd /srv/aegis/work/aosp
export GIT_TERMINAL_PROMPT=0
export GIT_CONFIG_COUNT=2 GIT_CONFIG_KEY_0=user.name GIT_CONFIG_VALUE_0=AegisOS \
    GIT_CONFIG_KEY_1=user.email GIT_CONFIG_VALUE_1=build@aegisos.invalid
python3 "$repo_tool" init -u https://android.googlesource.com/platform/manifest \
    -b "$AOSP_MANIFEST_COMMIT" --depth=1 \
    --repo-url=https://android.googlesource.com/tools/repo \
    --repo-rev="$REPO_COMMIT" --no-clone-bundle
python3 "$repo_tool" sync -c -j8 --no-clone-bundle --fail-fast
python3 "$repo_tool" manifest -r -o "$run/artifacts/manifest.xml"
state BUILDING
# Keep the credential directory and token out of the compiler's environment.
env -u CREDENTIALS_DIRECTORY -u GH_TOKEN bash "$script_dir/compile.sh" "$run"
state PACKAGING
product=$(cat "$run/product-out.txt")
[[ -d "$product" ]] || { echo 'Missing product output.'; exit 1; }
[[ -f "$product/system.img" && -f "$product/kernel-ranchu" ]] || { echo 'Missing emulator system image/kernel.'; exit 1; }
# Only deliver top-level runtime files, not large obj/ intermediates or unpacked trees.
(cd "$product"; find -L . -maxdepth 1 -type f -print0 | sort -z | tar -h --null -T - -cf -) \
    | xz -T2 -1 | split -b 1900M -d -a 4 - "$run/artifacts/images.tar.xz.part-"
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
state SAFE_TO_DELETE
trap - EXIT TERM INT
echo "SAFE_TO_DELETE: https://github.com/$repo/releases/tag/$run_id"
echo 'Delete the DigitalOcean server and any unwanted separate volumes yourself.'
