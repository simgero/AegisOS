#!/bin/bash
# Read-only inventory. Does not start a build or change the server.
set -euo pipefail
[[ $(uname -s) == Darwin && $(uname -m) == arm64 ]] || {
    echo 'Run this check on the Apple Silicon development Mac.' >&2; exit 1;
}
for tool in git gh python3 qemu-system-aarch64 qemu-img ssh; do
    command -v "$tool" || { echo "Missing tool: $tool" >&2; exit 1; }
done
qemu-system-aarch64 --version
qemu-system-aarch64 -accel help
df -h .
gh api user --jq .login
gh release list --repo simgero/AegisOS --limit 5
ssh -o BatchMode=yes -o StrictHostKeyChecking=yes -o ConnectTimeout=10 aegis-build '
    set -eu
    uname -m
    cat /etc/os-release
    free -h
    nproc
    lsblk -o NAME,SIZE,FSTYPE,MOUNTPOINTS
    if [ ! -d /srv/aegis ]; then
        echo "BLOCKED: /srv/aegis has not been provisioned." >&2
        exit 1
    fi
    df -h /srv/aegis
    available=$(df -Pk /srv/aegis | awk "END {print \$4}")
    [ "$available" -ge 471859200 ] || {
        echo "BLOCKED: need 450 GiB free under /srv/aegis." >&2; exit 1;
    }
    echo "Storage prerequisite met; build dependencies and QEMU target still require validation."
'
