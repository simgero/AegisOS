#!/bin/bash
# Root bootstrap on aegis-build. Reads a completed run; never builds or tests.
set -euo pipefail
[[ $# == 4 && $1 == --token-stdin && $2 =~ ^[0-9a-f]{40}$ && $3 =~ ^[0-9a-f]{40}$ ]] || {
    echo 'Usage: sudo bash export-components.sh --token-stdin EXPORT_COMMIT BUILD_COMMIT RUN_ID' >&2
    exit 2
}
export_commit=$2
build_commit=$3
run_id=$4
[[ $run_id =~ ^identity-[0-9]{8}T[0-9]{6}Z-${build_commit:0:8}-[A-Za-z0-9]{6}$ ]] || {
    echo 'Select the exact successful component run for the build commit.' >&2; exit 2
}
[[ $(id -u) == 0 && $(uname -s) == Linux && $(uname -m) == x86_64 ]] || {
    echo 'Run on the existing Linux x86-64 builder as root.' >&2; exit 1
}
IFS= read -r token
[[ -n $token ]] || { echo 'Missing GitHub token on stdin.' >&2; exit 1; }
gh_call() { GH_TOKEN=$token gh "$@"; }
umask 022
exec 9>/run/aegis-bootstrap.lock
flock -n 9 || { echo 'Another AEGIS bootstrap is active.' >&2; exit 1; }
exec 8>/srv/aegis/work/build.lock
flock -n 8 || { echo 'An AEGIS build is active; export after it finishes.' >&2; exit 1; }
run=/srv/aegis/runs/$run_id
[[ -d $run && ! -L $run ]]
[[ $(cat "$run/status") == IDENTITY_COMPILED_NOT_INSTALLED ]]
[[ $(cat "$run/project-commit.txt") == "$build_commit" ]]
tool_dir=$(mktemp -d "/opt/aegis-components-export-${export_commit:0:8}-XXXXXX")
chmod 755 "$tool_dir"
gh_call api -H 'Accept: application/vnd.github.raw+json' \
    "repos/simgero/AegisOS/contents/scripts/aosp/components.py?ref=$export_commit" \
    > "$tool_dir/components.py"
chmod 644 "$tool_dir/components.py"
out=$(mktemp -d "/srv/aegis/runs/components-$(date -u +%Y%m%dT%H%M%SZ)-${build_commit:0:8}-${export_commit:0:8}-XXXXXX")
chown aegis-build:aegis-build "$out"
chmod 755 "$out"
tag=${out##*/}
state() { printf '%s\n' "$1" > "$out/status.tmp"; mv "$out/status.tmp" "$out/status"; }
failed() {
    local result=$?
    trap - EXIT
    if (( result != 0 )); then
        state FAILED || true
        echo "Component export failed; no verified completion. Inspect $out." >&2
    fi
    return "$result"
}
trap failed EXIT
state PACKAGING
cd "$out"
# The packager receives no GitHub credential and runs as the existing build user.
runuser -u aegis-build -- env -u GH_TOKEN -u GITHUB_TOKEN -u CREDENTIALS_DIRECTORY \
    python3 "$tool_dir/components.py" package "$run" --build-commit "$build_commit" \
    --export-commit "$export_commit" --output "$out/assets"
cat > "$out/notes.md" <<NOTES
AEGIS ARM64 components from completed run: $run_id
Build source commit: $build_commit
Export tooling commit: $export_commit

Contains identity/runtime test artifacts and their source receipts and checksums.
This is not a bootable system image. No components were installed or executed;
password, lifecycle, namespace, SELinux and reboot tests remain to be run in the
local Android QEMU guest on the Mac. Do not copy individual framework files into
a running system. This export does not contain signing keys.
NOTES
state UPLOADING
gh_call release create "$tag" --repo simgero/AegisOS --target "$build_commit" --draft \
    --title "$tag" --notes-file "$out/notes.md"
for name in components.tar.gz components.json SHA256SUMS; do
    gh_call release upload "$tag" "$out/assets/$name" --repo simgero/AegisOS
done
state VERIFYING
mkdir "$out/verify"
for name in components.tar.gz components.json SHA256SUMS; do
    gh_call release download "$tag" --repo simgero/AegisOS --pattern "$name" --dir "$out/verify"
    cmp "$out/assets/$name" "$out/verify/$name"
done
runuser -u aegis-build -- env -u GH_TOKEN -u GITHUB_TOKEN -u CREDENTIALS_DIRECTORY \
    python3 "$tool_dir/components.py" verify "$out/verify" --build-commit "$build_commit"
gh_call release edit "$tag" --repo simgero/AegisOS --draft=false --latest=false
published=$(gh_call release view "$tag" --repo simgero/AegisOS \
    --json isDraft,tagName,targetCommitish --jq '[.isDraft,.tagName,.targetCommitish] | @tsv')
[[ $published == "$(printf 'false\t%s\t%s' "$tag" "$build_commit")" ]]
state COMPONENTS_UPLOAD_VERIFIED
trap - EXIT
echo "COMPONENTS_UPLOAD_VERIFIED: https://github.com/simgero/AegisOS/releases/tag/$tag"
echo 'Transport verified; no image installation or QEMU execution performed.'
