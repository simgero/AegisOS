#!/bin/sh
# POSIX entry point: curl .../build.sh | sh
set -eu
main() {
    case ${1:-} in
        --help) printf '%s\n' 'AegisOS disposable AOSP builder (Ubuntu 24.04 x86-64, root).' 'Required: GH_TOKEN (Contents: read/write for simgero/AegisOS).' 'Optional: AEGIS_REF (Git commit, tag or branch; default main).' 'Starts a systemd service; does not delete or shut down the server.'; return ;;
        '') ;;
        *) echo 'Unknown argument; use --help.' >&2; return 2 ;;
    esac
    [ "$(uname -s)" = Linux ] && [ "$(uname -m)" = x86_64 ] || { echo 'Requires Linux x86-64.' >&2; return 1; }
    [ "$(id -u)" = 0 ] || { echo 'Run as root on the disposable build server.' >&2; return 1; }
    . /etc/os-release
    [ "$ID" = ubuntu ] && [ "$VERSION_ID" = 24.04 ] || { echo 'Requires Ubuntu 24.04.' >&2; return 1; }
    [ -d /run/systemd/system ] || { echo 'Requires systemd.' >&2; return 1; }
    [ -n "${GH_TOKEN:-}" ] || { echo 'Export GH_TOKEN first; never paste it into the URL.' >&2; return 1; }
    [ "$(awk '/MemTotal/ {print int($2/1024/1024)}' /proc/meminfo)" -ge 60 ] || { echo 'Provision at least 64 GB RAM.' >&2; return 1; }
    mkdir -p /srv/aegis
    [ "$(df -Pk /srv/aegis | awk 'END {print $4}')" -ge 524288000 ] || { echo 'Need at least 500 GiB free at /srv/aegis.' >&2; return 1; }
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
    install -d -m 755 "$root"
    for file in worker.sh config.sh compile.sh; do
        gh_call api -H 'Accept: application/vnd.github.raw+json' \
            "repos/simgero/AegisOS/contents/scripts/aosp/$file?ref=$commit" > "$root/$file.tmp"
        bash -n "$root/$file.tmp"
        chmod 644 "$root/$file.tmp"
        mv "$root/$file.tmp" "$root/$file"
    done
    id aegis-build >/dev/null 2>&1 || useradd --system --create-home --home-dir /srv/aegis/home --shell /bin/bash aegis-build
    install -d -o aegis-build -g aegis-build -m 755 /srv/aegis/work /srv/aegis/runs
    systemctl reset-failed aegis-build.service 2>/dev/null || true
    systemd-run --unit=aegis-build --collect \
        --property=User=aegis-build --property=Group=aegis-build \
        --property=LoadCredential=github-token:/run/aegis-bootstrap/github-token \
        --property=RuntimeMaxSec=24h --property=TimeoutStopSec=120 \
        --property=KillMode=control-group --property=UMask=0077 \
        --setenv=HOME=/srv/aegis/home --setenv=AEGIS_SCRIPT_COMMIT="$commit" \
        /bin/bash "$root/worker.sh"
    echo 'Build started. Follow: journalctl -fu aegis-build'
    echo 'Status and logs: /srv/aegis/runs/'
    echo '24-hour runtime limit stops the job, NOT DigitalOcean billing. Delete the server yourself.'
}
# Execute only after the complete function has arrived through the pipe.
main "$@"
