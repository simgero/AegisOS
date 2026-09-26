#!/bin/bash
# Run on aegis-build as root. Fetch project scripts exclusively through GitHub.
set -euo pipefail
if [[ ${1:-} == --help ]]; then
    echo 'Usage: sudo bash scripts/prepare-builder.sh FULL_GITHUB_COMMIT'
    echo 'Starts an AOSP source download only. No build, token, or release upload.'
    exit 0
fi
commit=${1:-}
[[ $# == 1 && "$commit" =~ ^[0-9a-f]{40}$ ]] || { echo 'A full 40-character commit is required.' >&2; exit 2; }
[[ $(id -u) == 0 && $(uname -m) == x86_64 && $(uname -s) == Linux ]] || {
    echo 'Run as root on the x86-64 Linux builder.' >&2; exit 1;
}
for tool in curl git python3 flock systemd-run mountpoint dpkg-query; do
    command -v "$tool" >/dev/null
done
mountpoint -q /srv/aegis
id aegis-build >/dev/null
[[ -d /run/systemd/system ]]
for path in /srv/aegis/home /srv/aegis/work /srv/aegis/runs; do
    runuser -u aegis-build -- test -w "$path"
done
available=$(df -Pk /srv/aegis | awk 'END {print $4}')
(( available >= 471859200 )) || { echo 'Need 450 GiB free at /srv/aegis.' >&2; exit 1; }
exec 9>/run/aegis-bootstrap.lock
flock -n 9
if systemctl is-active --quiet aegis-build.service; then
    echo 'aegis-build is already active.' >&2; exit 1
fi
install -d -m 755 /opt/aegis-builder
root=$(mktemp -d "/opt/aegis-builder/prepare-${commit}.XXXXXX")
chmod 755 "$root"
for file in worker.sh config.sh compile.sh; do
    curl --fail --silent --show-error --location \
        "https://raw.githubusercontent.com/simgero/AegisOS/$commit/scripts/aosp/$file" \
        -o "$root/$file"
    bash -n "$root/$file"
    chmod 644 "$root/$file"
done
systemctl reset-failed aegis-build.service 2>/dev/null || true
systemd-run --unit=aegis-build --collect \
    --property=User=aegis-build --property=Group=aegis-build \
    --property=RequiresMountsFor=/srv/aegis \
    --property=RuntimeMaxSec=24h --property=TimeoutStopSec=120 \
    --property=KillMode=control-group --property=UMask=0077 \
    --setenv=HOME=/srv/aegis/home --setenv=AEGIS_SCRIPT_COMMIT="$commit" \
    --setenv=AEGIS_SYNC_ONLY=1 /bin/bash "$root/worker.sh"
echo 'Source download started. Follow with: journalctl -fu aegis-build'
echo 'Completion: SOURCES_READY in /srv/aegis/runs/*/status. Server remains running.'
