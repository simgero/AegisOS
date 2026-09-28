#!/system/bin/sh
# Development transport only. No guest NIC and no personal authentication logic.
set -eu
test "$(getprop ro.adb.secure)" = 1
exec 3<>/dev/hvc17
# Keep fd 3 open: the driver resets termios after the last descriptor closes.
stty -F /dev/hvc17 raw -echo
echo $$ > /data/local/aegis-debug/bridge.pid
child=
cleanup() {
    trap - EXIT TERM INT
    if [ -n "$child" ]; then kill "$child" 2>/dev/null || true; fi
    rm -f /data/local/aegis-debug/bridge.pid
    exit
}
trap cleanup EXIT TERM INT
while true; do
    nc 127.0.0.1 5555 <&3 >&3 &
    child=$!
    wait "$child" || true
    child=
    sleep 1
done
