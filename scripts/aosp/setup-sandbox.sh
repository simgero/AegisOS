#!/bin/bash
# Root-only host setup; keep Ubuntu's global user-namespace restriction enabled.
set -euo pipefail
[[ $(id -u) == 0 ]] || { echo 'Run sandbox setup as root.' >&2; exit 1; }
if [[ $(cat /proc/sys/kernel/apparmor_restrict_unprivileged_userns 2>/dev/null || echo 0) != 1 ]]; then
    echo 'AppArmor user namespace restriction is not enabled; no exception needed.'
    exit 0
fi
command -v apparmor_parser >/dev/null
profile=/etc/apparmor.d/aegis-build-nsjail
temporary=$(mktemp)
trap 'rm -f "$temporary"' EXIT
cat > "$temporary" <<'PROFILE'
# Managed by AegisOS scripts/aosp/setup-sandbox.sh.
# This exception permits user namespaces only for the AOSP nsjail executable.
# The build account controls this source tree; it must remain a trusted account.
abi <abi/4.0>,
include <tunables/global>
profile aegis-build-nsjail /srv/aegis/work/aosp/prebuilts/build-tools/linux-x86/bin/nsjail flags=(unconfined) {
    userns,
}
PROFILE
if [[ -e "$profile" || -L "$profile" ]]; then
    [[ ! -L "$profile" ]] && cmp -s "$temporary" "$profile" || {
        echo "Refusing to overwrite a different profile: $profile" >&2; exit 1;
    }
fi
# Parse before persisting; loading is explicit and errors stop the bootstrap.
apparmor_parser --skip-kernel-load "$temporary"
install -o root -g root -m 644 "$temporary" "$profile"
apparmor_parser --replace "$profile"
echo 'AOSP nsjail AppArmor profile loaded.'
