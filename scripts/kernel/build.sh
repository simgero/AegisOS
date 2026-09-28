#!/bin/bash
# Run only on aegis-build. Project source arrives through a pinned GitHub commit.
set -Eeuo pipefail
[[ $(uname -s) == Linux && $(uname -m) == x86_64 && $(id -u) != 0 ]] || {
    echo 'Run as the unprivileged build account on the Linux x86_64 builder.' >&2
    exit 1
}
commit=${1:?Usage: build.sh PROJECT_COMMIT}
[[ $commit =~ ^[0-9a-f]{40}$ ]] || { echo 'Expected a full project commit.' >&2; exit 2; }
project=$(cd "$(dirname "$0")/../.." && pwd)
[[ $(git -C "$project" rev-parse HEAD) == "$commit" ]] || {
    echo 'Project checkout does not match the requested commit.' >&2; exit 1;
}
[[ -z $(git -C "$project" status --porcelain -- kernel scripts/kernel scripts/aosp/config.sh) ]] || {
    echo 'Kernel build inputs have local changes; commit and sync through GitHub first.' >&2; exit 1;
}
source "$project/scripts/aosp/config.sh"
python3 "$project/scripts/kernel/source_manifest.py" "$project/kernel/manifest.xml"
workspace=/srv/aegis/work/kernel
repo_tool=/srv/aegis/work/repo-tool/repo
[[ -f $repo_tool ]] || { echo 'Missing provisioned repo tool.' >&2; exit 1; }
[[ $(git -C "$(dirname "$repo_tool")" rev-parse HEAD) == "$REPO_COMMIT" ]] || {
    echo 'Repo tool does not match its pin.' >&2; exit 1;
}
mkdir -p "$workspace"
exec 9>/srv/aegis/work/build.lock
flock -n 9 || { echo 'Another Aegis build holds the workspace lock.' >&2; exit 1; }
run=$(mktemp -d "/srv/aegis/runs/kernel-$(date -u +%Y%m%dT%H%M%SZ)-${commit:0:8}-XXXXXX")
exec > >(tee -a "$run/build.log") 2>&1
state() { printf '%s\n' "$1" > "$run/status.tmp"; mv "$run/status.tmp" "$run/status"; }
trap 'code=$?; trap - EXIT; state FAILED; echo "Kernel build failed ($code); preserved at $run"; exit "$code"' EXIT
trap 'exit 143' TERM
trap 'exit 130' INT
state PREPARING
printf '%s\n' "$commit" > "$run/builder-commit.txt"
cp "$project/kernel/manifest.xml" "$project/kernel/aegis_runtime_defconfig" "$run/"
# Same cooldown as the AOSP source worker. Never automatically retry rate limits.
cooldown=/srv/aegis/work/google-retry-after
if [[ -f $cooldown ]]; then
    until=$(cat "$cooldown")
    [[ $until =~ ^[0-9]+$ ]] || { echo 'Invalid Google cooldown.' >&2; exit 1; }
    (( $(date +%s) >= until )) || { echo 'Google download cooldown is still active.' >&2; exit 1; }
fi
export GIT_TERMINAL_PROMPT=0
export GIT_CONFIG_COUNT=2 GIT_CONFIG_KEY_0=user.name GIT_CONFIG_VALUE_0=AegisOS
export GIT_CONFIG_KEY_1=user.email GIT_CONFIG_VALUE_1=build@aegisos.invalid
cd "$workspace"
state SYNCING
# This checkout already arrived from GitHub. Reuse its pinned manifest locally
# so the kernel sync/compiler never needs credentials for the private repository.
python3 "$repo_tool" init -u "$project" -b "$commit" \
    -m kernel/manifest.xml --depth=1 --no-use-superproject --no-clone-bundle \
    --repo-url=https://android.googlesource.com/tools/repo --repo-rev="$REPO_COMMIT"
if python3 "$repo_tool" sync -c -j1 --jobs-network=1 --jobs-checkout=1 \
    --retry-fetches=0 --no-clone-bundle --fail-fast 2>&1 | tee "$run/sync.log"; then
    :
else
    code=$?
    if grep -Eiq 'HTTP[^[:alnum:]]*429|returned error: 429|Too Many Requests' "$run/sync.log"; then
        printf '%s\n' "$(($(date +%s) + 1800))" > "$cooldown"
    fi
    exit "$code"
fi
python3 "$repo_tool" manifest -r -o "$run/resolved-manifest.xml"
python3 "$project/scripts/kernel/source_manifest.py" "$project/kernel/manifest.xml" --workspace "$workspace"
# Every project commit gets its own fragment package; never overwrite an older one.
fragment_dir="aegis/$commit"
if [[ -e $fragment_dir ]]; then
    [[ ! -L $fragment_dir && ! -L aegis ]] || { echo 'Unexpected fragment symlink.'; exit 1; }
    cmp "$project/kernel/BUILD.bazel" "$fragment_dir/BUILD.bazel"
    cmp "$project/kernel/aegis_runtime_defconfig" "$fragment_dir/aegis_runtime_defconfig"
else
    [[ ! -L aegis ]] || { echo 'Unexpected aegis symlink.'; exit 1; }
    mkdir -p "$fragment_dir"
    cp "$project/kernel/BUILD.bazel" "$project/kernel/aegis_runtime_defconfig" "$fragment_dir/"
fi
jobs=$(nproc)
(( jobs <= 12 )) || jobs=12
fragment="--defconfig_fragment=//$fragment_dir:aegis_runtime_defconfig"
state BUILDING
# Both commands apply the same fragment to the GKI base AND its module builds.
# No downloaded GKI substitution, relaxed ABI checks or prebuilt vendor modules.
env -u GH_TOKEN -u GITHUB_TOKEN -u CREDENTIALS_DIRECTORY \
    tools/bazel run --jobs="$jobs" "$fragment" //common:kernel_aarch64_dist -- --destdir="$run/gki"
env -u GH_TOKEN -u GITHUB_TOKEN -u CREDENTIALS_DIRECTORY \
    tools/bazel run --jobs="$jobs" "$fragment" \
    //common-modules/virtual-device:virtual_device_aarch64_dist -- --destdir="$run/virtual-device"
state BUILT_UNVERIFIED
trap - EXIT TERM INT
echo "Kernel outputs: $run"
echo 'Not ready for QEMU: validate configs/modules, integrate into signed AOSP images, then publish through GitHub.'
