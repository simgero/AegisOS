#!/bin/bash
# Root entry point on aegis-build. Source and checkout transport: GitHub only.
# No package installation, server configuration change, image build or VM test.
set -euo pipefail
commit=${1:-}
[[ $# == 1 && "$commit" =~ ^[0-9a-f]{40}$ ]] || {
    echo 'Usage: sudo bash start-components.sh FULL_GITHUB_COMMIT' >&2; exit 2;
}
[[ $(id -u) == 0 && $(uname -s) == Linux && $(uname -m) == x86_64 ]] || {
    echo 'Run as root on the existing Linux x86-64 aegis-build server.' >&2; exit 1;
}
for tool in git runuser flock systemd-run mountpoint; do command -v "$tool" >/dev/null; done
id aegis-build >/dev/null
mountpoint -q /srv/aegis
[[ -d /run/systemd/system ]]
for path in /srv/aegis/home /srv/aegis/work /srv/aegis/runs /srv/aegis/work/aosp; do
    runuser -u aegis-build -- test -w "$path"
done
exec 9>/run/aegis-bootstrap.lock
flock -n 9 || { echo 'Another bootstrap is active.' >&2; exit 1; }
for unit in aegis-build aegis-components; do
    if systemctl is-active --quiet "$unit.service"; then
        echo "$unit is already active; inspect its journal before starting anything else." >&2
        exit 1
    fi
done
checkout=$(mktemp -d "/srv/aegis/work/components-${commit:0:8}-XXXXXX")
chown aegis-build:aegis-build "$checkout"
chmod 755 "$checkout"
systemd-run --unit=aegis-components --collect \
    --property=User=aegis-build --property=Group=aegis-build \
    --property=WorkingDirectory="$checkout" --property=RequiresMountsFor=/srv/aegis \
    --property=RuntimeMaxSec=12h --property=TimeoutStopSec=120 \
    --property=KillMode=control-group --property=UMask=0022 \
    --setenv=HOME=/srv/aegis/home --setenv=GIT_TERMINAL_PROMPT=0 \
    /bin/bash -c '
        set -euo pipefail
        unset GH_TOKEN GITHUB_TOKEN CREDENTIALS_DIRECTORY
        git init .
        git fetch --depth=1 https://github.com/simgero/AegisOS.git "$1"
        git checkout --detach FETCH_HEAD
        test "$(git rev-parse HEAD)" = "$1"
        exec bash scripts/aosp/check-identity.sh "$1"
    ' aegis-components "$commit"
echo "Component check dispatched for $commit. This does not prove compilation success."
echo 'Follow: journalctl -fu aegis-components'
echo 'Success: IDENTITY_COMPILED_NOT_INSTALLED in the matching /srv/aegis/runs/identity-*/status.'
echo 'No images were installed or tested. Sources and outputs are retained; runtime limit: 12h.'
