#!/bin/sh
# POSIX entry point. --token-stdin requires a saved script, not curl | sh.
set -eu
check_storage() {
    # 450 GiB leaves reserve above AOSP's 400 GB guidance while admitting a
    # formatted 500 GiB volume (about 471 GiB available with ext4 defaults).
    required_gib=450
    storage_reuse_run=${AEGIS_INCREMENTAL_FROM_RUN:-}
    if [ -n "$storage_reuse_run" ]; then
        case "$storage_reuse_run" in
            /srv/aegis/runs/aosp-*) ;;
            *) echo 'Incremental run must be a completed AOSP run under /srv/aegis/runs.' >&2; return 1;;
        esac
        case "${storage_reuse_run#/srv/aegis/runs/}" in
            *[!A-Za-z0-9_-]*) echo 'Invalid incremental run name.' >&2; return 1;;
        esac
        [ -d "$storage_reuse_run" ] && [ ! -L "$storage_reuse_run" ] || return 1
        for proof in status artifacts/config.sh artifacts/manifest.xml artifacts/builder-commit.txt; do
            [ -f "$storage_reuse_run/$proof" ] && [ -s "$storage_reuse_run/$proof" ] \
                && [ ! -L "$storage_reuse_run/$proof" ] || {
                echo 'Incremental build evidence is missing or linked.' >&2; return 1;
            }
        done
        [ "$(cat "$storage_reuse_run/status")" = UPLOAD_VERIFIED ] || {
            echo 'Incremental run has not completed verified publication.' >&2; return 1;
        }
        for image in system.img super.img; do
            image_path=/srv/aegis/work/aosp/out/target/product/qemu_arm64/$image
            [ -f "$image_path" ] && [ -s "$image_path" ] && [ ! -L "$image_path" ] || {
                echo 'Existing AOSP product output is missing.' >&2; return 1;
            }
        done
        manifest_pin=$(sed -n 's/^AOSP_MANIFEST_COMMIT=//p' "$storage_reuse_run/artifacts/config.sh")
        case "$manifest_pin" in *[!0-9a-f]*|'') return 1;; esac
        [ ${#manifest_pin} -eq 40 ] || return 1
        current_manifest=$(runuser -u aegis-build -- git -C /srv/aegis/work/aosp/.repo/manifests rev-parse HEAD) || return 1
        [ "$current_manifest" = "$manifest_pin" ] || {
            echo 'Existing AOSP manifest differs from the completed build.' >&2; return 1;
        }
        # Explicit reuse only: the existing checkout and build output are not
        # a second cold build. Retain a separate 200-GiB free-space floor.
        # The newly fetched immutable recipe must ALSO match before dispatch.
        required_gib=200
    fi
    available_kib=$(df -Pk /srv/aegis | awk 'END {print $4}')
    [ "$available_kib" -ge "$((required_gib * 1048576))" ] || {
        echo "Need at least $required_gib GiB free at /srv/aegis." >&2; return 1;
    }
    echo "Storage OK: $((available_kib / 1048576)) GiB free at /srv/aegis."
}
verify_incremental_recipe() {
    if [ -n "${storage_reuse_run:-}" ]; then
        cmp -s "$storage_reuse_run/artifacts/config.sh" "$1" || {
            echo 'Incremental build requires the identical AOSP/product recipe; use cold-build capacity for a changed recipe.' >&2
            return 1
        }
    fi
}
main() {
    case ${1:-} in
        --help) printf '%s\n' 'AegisOS AOSP builder (Ubuntu 24.04/26.04 x86-64, root).' 'Required: GH_TOKEN (Contents: read/write for simgero/AegisOS), or --token-stdin.' 'Optional: AEGIS_REF (Git commit, tag or branch; default main).' 'Optional: AEGIS_KERNEL_RUN=/srv/aegis/runs/kernel-RUN (completed matched kernel build).' 'Optional: AEGIS_RUNTIME_RUN=/srv/aegis/runs/runtime-base-RUN (completed shared-base image build).' 'Optional: AEGIS_INCREMENTAL_FROM_RUN=/srv/aegis/runs/aosp-RUN (verified prior run, existing matching manifest/product, unchanged recipe, 200 GiB free; default cold-build floor is 450 GiB).' 'Use --token-stdin COMMIT to read a token from a pipe, never command arguments.' 'Use --check-storage to check disk space without starting a build.' 'Starts a systemd service; preserves the server and source checkout.'; return ;;
        --check-storage) check_storage; return ;;
        --token-stdin)
            [ "$#" -eq 2 ] || { echo 'Usage: build.sh --token-stdin FULL_COMMIT' >&2; return 2; }
            case "$2" in *[!0-9a-f]*|'') echo 'Invalid commit.' >&2; return 2;; esac
            [ ${#2} -eq 40 ] || { echo 'A full commit is required.' >&2; return 2; }
            AEGIS_REF=$2
            IFS= read -r GH_TOKEN || { echo 'Could not read token from stdin.' >&2; return 1; }
            ;;
        '') ;;
        *) echo 'Unknown argument; use --help.' >&2; return 2 ;;
    esac
    kernel_run=${AEGIS_KERNEL_RUN:-}
    runtime_run=${AEGIS_RUNTIME_RUN:-}
    if [ -n "$runtime_run" ]; then
        case "$runtime_run" in
            /srv/aegis/runs/runtime-base-*) ;;
            *) echo 'Runtime run must be under /srv/aegis/runs/runtime-base-RUN.' >&2; return 2 ;;
        esac
        case "${runtime_run#/srv/aegis/runs/}" in
            *[!A-Za-z0-9_-]*) echo 'Invalid runtime run name.' >&2; return 2 ;;
        esac
        [ -d "$runtime_run" ] && [ ! -L "$runtime_run" ] || {
            echo 'Runtime run is missing or is a symlink.' >&2; return 2;
        }
    fi
    if [ -n "$kernel_run" ]; then
        case "$kernel_run" in
            /srv/aegis/runs/kernel-*) ;;
            *) echo 'Kernel run must be under /srv/aegis/runs/kernel-RUN.' >&2; return 2 ;;
        esac
        case "${kernel_run#/srv/aegis/runs/}" in
            *[!A-Za-z0-9_-]*) echo 'Invalid kernel run name.' >&2; return 2 ;;
        esac
        [ -d "$kernel_run" ] && [ ! -L "$kernel_run" ] || {
            echo 'Kernel run is missing or is a symlink.' >&2; return 2;
        }
    fi
    [ "$(uname -s)" = Linux ] && [ "$(uname -m)" = x86_64 ] || { echo 'Requires Linux x86-64.' >&2; return 1; }
    [ "$(id -u)" = 0 ] || { echo 'Run as root on the build server.' >&2; return 1; }
    . /etc/os-release
    [ "$ID" = ubuntu ] || { echo 'Requires Ubuntu.' >&2; return 1; }
    case "$VERSION_ID" in 24.04|26.04) ;; *) echo 'Requires Ubuntu 24.04 or 26.04.' >&2; return 1;; esac
    [ -d /run/systemd/system ] || { echo 'Requires systemd.' >&2; return 1; }
    [ -n "${GH_TOKEN:-}" ] || { echo 'Export GH_TOKEN first; never paste it into the URL.' >&2; return 1; }
    [ "$(awk '/MemTotal/ {print int($2/1024/1024)}' /proc/meminfo)" -ge 60 ] || { echo 'Provision at least 64 GB RAM.' >&2; return 1; }
    mountpoint -q /srv/aegis || { echo 'Mount the dedicated build volume at /srv/aegis first.' >&2; return 1; }
    check_storage
    # Lock before any installation/download to prevent concurrent invocations.
    exec 9>/run/aegis-bootstrap.lock
    flock -n 9 || { echo 'Another bootstrap is running.' >&2; return 1; }
    if systemctl is-active --quiet aegis-build.service; then
        echo 'Build already running: journalctl -fu aegis-build'; return 1
    fi
    umask 077
    install -d -m 700 /run/aegis-bootstrap
    printf '%s' "$GH_TOKEN" > /run/aegis-bootstrap/github-token
    unset GH_TOKEN
    gh_call() { GH_TOKEN=$(cat /run/aegis-bootstrap/github-token) gh "$@"; }
    export DEBIAN_FRONTEND=noninteractive
    apt-get update
    apt-get install -y ca-certificates curl git gh python3 build-essential flex bison zip unzip \
        zlib1g-dev libc6-dev-i386 libx11-dev lib32z1-dev libgl1-mesa-dev libxml2-utils \
        xsltproc fontconfig rsync libncurses-dev libssl-dev file xz-utils
    ref=${AEGIS_REF:-main}
    commit=$(gh_call api "repos/simgero/AegisOS/commits/$ref" --jq .sha)
    case "$commit" in *[!0-9a-f]*|'') echo 'Invalid source commit.' >&2; return 1;; esac
    [ ${#commit} -eq 40 ] || return 1
    root=/opt/aegis-builder/$commit
    install -d -m 755 /opt/aegis-builder
    # Preserve the repository layout and include all owned package/overlay/policy sources.
    # The fetcher verifies every selected file against the immutable commit's Git tree.
    gh_call api -H 'Accept: application/vnd.github.raw+json' \
        "repos/simgero/AegisOS/contents/scripts/aosp/fetch-build-inputs.py?ref=$commit" \
        > /run/aegis-bootstrap/fetch-build-inputs.py
    GH_TOKEN=$(cat /run/aegis-bootstrap/github-token) \
        python3 /run/aegis-bootstrap/fetch-build-inputs.py "$commit" "$root"
    scripts="$root/scripts/aosp"
    # Validate the exact downloaded recipe before any source sync or service.
    verify_incremental_recipe "$scripts/config.sh"
    for file in worker.sh config.sh compile.sh setup-sandbox.sh; do
        bash -n "$scripts/$file"
    done
    id aegis-build >/dev/null 2>&1 || useradd --system --create-home --home-dir /srv/aegis/home --shell /bin/bash aegis-build
    install -d -o aegis-build -g aegis-build -m 755 /srv/aegis/work /srv/aegis/runs
    bash "$scripts/setup-sandbox.sh"
    systemctl reset-failed aegis-build.service 2>/dev/null || true
    systemd-run --unit=aegis-build --collect \
        --property=User=aegis-build --property=Group=aegis-build \
        --property=RequiresMountsFor=/srv/aegis \
        --property=LoadCredential=github-token:/run/aegis-bootstrap/github-token \
        --property=RuntimeMaxSec=24h --property=TimeoutStopSec=120 \
        --property=KillMode=control-group --property=UMask=0077 \
        --setenv=HOME=/srv/aegis/home --setenv=AEGIS_SCRIPT_COMMIT="$commit" \
        --setenv=AEGIS_KERNEL_RUN="$kernel_run" \
        --setenv=AEGIS_RUNTIME_RUN="$runtime_run" \
        /bin/bash "$scripts/worker.sh"
    echo 'Build started. Follow: journalctl -fu aegis-build'
    echo 'Status and logs: /srv/aegis/runs/'
    echo '24-hour runtime limit stops the job. Server, sources and outputs are preserved.'
}
# Execute only after the complete function has arrived through the pipe.
main "$@"
