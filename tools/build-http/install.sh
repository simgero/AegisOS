#!/bin/bash
# Install from a GitHub-sourced checkout, as root, on the existing builder.
set -euo pipefail
[[ $(id -u) == 0 && $# == 2 ]] || { echo 'Usage: sudo bash install.sh SSH_USER TAILSCALE_IPV4' >&2; exit 2; }
account=$1
bind=$2
id "$account" >/dev/null
[[ $account =~ ^[a-z_][a-z0-9_-]*$ ]] || exit 2
[[ $(tailscale ip -4) == "$bind" ]] || { echo 'Address is not this host Tailscale IP' >&2; exit 1; }
root=$(cd -- "$(dirname -- "$0")" && pwd)
install -d -o root -g root -m 755 /opt/aegis-build-http
install -m 644 "$root/server.py" /opt/aegis-build-http/server.py
install -d -o root -g "$(id -gn "$account")" -m 750 /etc/aegis-build-http
install -d -o "$account" -g "$(id -gn "$account")" -m 700 /var/lib/aegis-build-http
python3 - "$account" "$bind" <<'PY'
import hashlib, json, os, pathlib, pwd, secrets, sys
account, bind = sys.argv[1:]
user = pwd.getpwnam(account)
root = pathlib.Path('/etc/aegis-build-http')
password_file = root / 'password'
if not password_file.exists():
    fd = os.open(password_file, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(fd, 'w') as out: out.write(secrets.token_urlsafe(32) + '\n')
password = password_file.read_text().strip()
config = dict(bind=bind, port=8787, state_dir='/var/lib/aegis-build-http',
              password_sha256=hashlib.sha256(password.encode()).hexdigest())
path = root / 'config.json'
path.write_text(json.dumps(config) + '\n')
os.chown(path, 0, user.pw_gid)
os.chmod(path, 0o640)
# root-only curl config; password never appears in a command argument.
path = root / 'client.curl'
path.write_text('user = "codex:' + password + '"\n')
os.chmod(path, 0o600)
PY
cat > /etc/systemd/system/aegis-build-http.service <<EOF
[Unit]
Description=AEGIS authenticated Tailscale command API
After=network-online.target tailscaled.service
Wants=network-online.target
Requires=tailscaled.service
StartLimitIntervalSec=0

[Service]
Type=simple
User=$account
Group=$(id -gn "$account")
ExecStart=/usr/bin/python3 /opt/aegis-build-http/server.py --config /etc/aegis-build-http/config.json
WorkingDirectory=/srv/aegis
UMask=0077
Restart=on-failure
RestartSec=5
KillMode=control-group
TimeoutStopSec=15
# This trusted administrator API intentionally retains the SSH user's sudo rights.

[Install]
WantedBy=multi-user.target
EOF
systemd-analyze verify /etc/systemd/system/aegis-build-http.service
systemctl daemon-reload
systemctl enable --now aegis-build-http.service
# Reinstallation activates new code; initial start needs no second restart.
if [[ ${AEGIS_HTTP_RESTART:-0} == 1 ]]; then systemctl restart aegis-build-http.service; fi
for attempt in {1..20}; do
    if curl --noproxy '*' --fail --silent --config /etc/aegis-build-http/client.curl "http://$bind:8787/health"; then
        echo
        echo "Ready: http://$bind:8787; credentials: /etc/aegis-build-http/password (root only)"
        exit 0
    fi
    sleep 1
done
systemctl status aegis-build-http.service --no-pager
exit 1
