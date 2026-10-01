#!/usr/bin/env python3
"""Enable authenticated ADB over this development VM's loopback serial bridge."""
import argparse
import base64
import hashlib
from pathlib import Path
import re
import shlex
import subprocess
import time

from qemu_console import execute


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run',type=Path)
    parser.add_argument('--authorize-this-host','--authorize-this-mac',dest='authorize_this_host',action='store_true',
                        help='Explicitly provision this host public ADB key in the development guest')
    parser.add_argument('--wait-boot',type=int,default=0,
                        help='Wait at most this many seconds for Android boot before provisioning ADB (0–1800)')
    args=parser.parse_args()
    if not 0<=args.wait_boot<=1800: parser.error('--wait-boot must be 0–1800')
    address=(args.run/'adb-address.txt').read_text().strip()
    if not re.fullmatch(r'127\.0\.0\.1:[0-9]{4,5}',address):
        parser.error('Expected a local-only ADB endpoint')
    port=int(address.rsplit(':',1)[1])
    if not 1024<=port<=65535:parser.error('Invalid ADB port')
    console=(args.run/'console-path.txt').read_text().strip()

    # This dedicated development console has no interactive human editor.
    # mksh's per-character redraw of long public-key/bridge commands can
    # exceed the bounded console deadline during first boot. Keep the shell
    # and completion checks, but disable its interactive line editing.
    execute(console, 'set +o emacs; set +o vi')

    def guest(command):
        return execute(console,'su 0 sh -c '+shlex.quote('set -e; '+command))

    deadline=time.monotonic()+args.wait_boot
    while True:
        try:
            guest('test "$(getprop sys.boot_completed)" = 1; test -d /data/misc/adb')
            break
        except (RuntimeError, TimeoutError):
            if time.monotonic()>=deadline:
                raise SystemExit('Android boot is not ready for ADB. Inspect the guest logs or use --wait-boot.')
            time.sleep(2)

    # In the current legacy image the property is absent and can be set once.
    # If an image explicitly sets it to 0, refuse rather than bypassing read-only properties.
    guest('if [ -z "$(getprop ro.adb.secure)" ]; then setprop ro.adb.secure 1; fi; '
          'test "$(getprop ro.adb.secure)" = 1')
    subprocess.run(['adb','start-server'],check=True,timeout=15)
    subprocess.run(['adb','disconnect',address],capture_output=True,timeout=10)
    # A persistent profile can retain bridge.pid after power-off. The numeric
    # PID may now name an unrelated Android process: stop only a process whose
    # argv contains our exact bridge script, and discard stale PID metadata.
    guest('mkdir -p /data/local/aegis-debug; chown root:root /data/local/aegis-debug; '
          'chmod 700 /data/local/aegis-debug; '
          'if [ -f /data/local/aegis-debug/bridge.pid ]; then '
          'pid=$(cat /data/local/aegis-debug/bridge.pid); '
          'case "$pid" in ""|*[!0-9]*) exit 1;; esac; '
          'test "$pid" -gt 1; '
          'if [ -r /proc/$pid/cmdline ] && '
          'tr "\\000" "\\n" < /proc/$pid/cmdline | grep -Fxq /data/local/aegis-debug/bridge.sh; then '
          'kill "$pid"; n=0; while kill -0 "$pid" 2>/dev/null; do '
          'n=$((n+1)); test "$n" -le 30; sleep 0.1; done; fi; '
          'rm -f /data/local/aegis-debug/bridge.pid; fi')
    if args.authorize_this_host:
        key=(Path.home()/'.android/adbkey.pub').read_text().strip()
        if len(key)>4096 or '\n' in key or '\r' in key:raise ValueError('Invalid public ADB key')
        decoded=base64.b64decode(key.split()[0],validate=True)
        fingerprint=':'.join(f'{byte:02X}' for byte in hashlib.md5(decoded).digest())
        # This is the public host key, never adbkey (the private key).
        quoted=shlex.quote(key)
        guest('umask 077; if ! grep -Fxq '+quoted+' /data/misc/adb/adb_keys 2>/dev/null; '
              "then printf '%s\\n' "+quoted+' >> /data/misc/adb/adb_keys; fi; '
              'chown system:shell /data/misc/adb/adb_keys; chmod 640 /data/misc/adb/adb_keys; '
              'restorecon /data/misc/adb/adb_keys')
        print('Authorized this host public ADB key:',fingerprint)
    script=Path(__file__).resolve().parents[1]/'tools/qemu/adb-bridge.sh'
    encoded=base64.b64encode(script.read_bytes()).decode()
    guest("printf '%s' "+shlex.quote(encoded)+' | base64 -d > /data/local/aegis-debug/bridge.sh; '
          'chmod 700 /data/local/aegis-debug/bridge.sh; '
          'setprop service.adb.tcp.port 5555; setprop ctl.restart adbd')
    time.sleep(1)
    guest('nohup sh /data/local/aegis-debug/bridge.sh </dev/null '
          '>/data/local/aegis-debug/bridge.log 2>&1 &')
    time.sleep(1)
    # A newly created serial transport can retain a pending host connection
    # from the previous VM. Retry one fresh authentication on both first setup
    # and profile restart; the existing key can need the same reconnect.
    # Never infer authorization from adb connect's exit code or add a key here.
    attempts=2
    for attempt in range(attempts):
        subprocess.run(['adb','disconnect',address],capture_output=True,timeout=10)
        subprocess.run(['adb','connect',address],check=True,timeout=15)
        result=subprocess.run(['adb','-s',address,'get-state'],capture_output=True,text=True,timeout=15)
        if not result.returncode and result.stdout.strip()=='device':break
        if attempt+1<attempts:time.sleep(1)
    if result.returncode or result.stdout.strip()!='device':
        raise SystemExit('ADB is not authorized yet. Approve this Mac in the guest dialog, then reconnect.')
    state=subprocess.check_output(['adb','-s',address,'shell','getprop','ro.adb.secure'],
                                  text=True,timeout=15).strip()
    if state!='1':raise SystemExit('ADB authentication was not confirmed')
    print('Authenticated ADB ready:',address)


if __name__=='__main__':main()
