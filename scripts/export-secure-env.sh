#!/bin/bash
# Root bootstrap on aegis-build; all project transport is through GitHub.
set -euo pipefail
[[ $# == 2 && $1 == --token-stdin && $2 =~ ^[0-9a-f]{40}$ ]] || {
    echo 'Usage: sudo bash export-secure-env.sh --token-stdin FULL_COMMIT' >&2; exit 2;
}
[[ $(id -u) == 0 && $(uname -s) == Linux ]] || { echo 'Run on the Linux builder as root.'; exit 1; }
IFS= read -r token
[[ -n $token ]]
commit=$2
gh_call() { GH_TOKEN=$token gh "$@"; }
exec 9>/run/aegis-bootstrap.lock
flock -n 9
if systemctl is-active --quiet aegis-build.service; then
    echo 'AOSP build is active; export after it finishes.' >&2; exit 1
fi
exec 8>/srv/aegis/work/build.lock
flock -n 8
root=/opt/aegis-helper/$commit
install -d -m 755 "$root"
for file in init.c package.py; do
    gh_call api -H 'Accept: application/vnd.github.raw+json' \
        "repos/simgero/AegisOS/contents/tools/secure-env/$file?ref=$commit" > "$root/$file.tmp"
    chmod 644 "$root/$file.tmp"
    mv "$root/$file.tmp" "$root/$file"
done
tag=secure-env-$(date -u +%Y%m%dT%H%M%SZ)-${commit:0:8}
out=/srv/aegis/runs/$tag
install -d -o aegis-build -g aegis-build -m 755 "$out"
# No token is exported to the compiler or packager.
runuser -u aegis-build -- env -u GH_TOKEN -u GITHUB_TOKEN \
    python3 "$root/package.py" "$out"
printf '%s\n' "$commit" > "$out/helper-commit.txt"
cat > "$out/notes.md" <<NOTES
Original AOSP ARM64 Linux secure_env and its shared libraries from the existing
android-16.0.0_r1 build, plus a small helper init compiled on aegis-build.
For a local QEMU development helper only. In-process TPM emulation is not a
hardware-backed TEE. Guest runtime integration has not yet been verified.
Helper source commit: $commit
System-image release: aosp-20260927T181835Z-38f4c95f-e6f263b2
NOTES
gh_call release create "$tag" --repo simgero/AegisOS --target "$commit" --draft \
    --title "$tag" --notes-file "$out/notes.md"
gh_call release upload "$tag" "$out/secure-env-arm64.tar.gz" "$out/SHA256SUMS" \
    "$out/helper-commit.txt" --repo simgero/AegisOS
mkdir "$out/verify"
gh_call release download "$tag" --repo simgero/AegisOS --dir "$out/verify" \
    --pattern secure-env-arm64.tar.gz
cmp "$out/secure-env-arm64.tar.gz" "$out/verify/secure-env-arm64.tar.gz"
gh_call release edit "$tag" --repo simgero/AegisOS --draft=false
echo "HELPER_UPLOAD_VERIFIED: https://github.com/simgero/AegisOS/releases/tag/$tag"
