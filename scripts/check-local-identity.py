#!/usr/bin/env python3
"""Read-only identity-service smoke check on the local Mac QEMU Android guest."""
import argparse
from datetime import datetime, timezone
import json
import re
import shutil
import subprocess
import sys


def check(serial):
    if sys.platform != "darwin":
        raise RuntimeError("Run OS checks in local QEMU on the Mac")
    if not re.fullmatch(r"127\.0\.0\.1:[0-9]{1,5}", serial):
        raise ValueError("Select the explicit loopback ADB connection for local QEMU")
    if not 1 <= int(serial.rsplit(":", 1)[1]) <= 65535:
        raise ValueError("Invalid ADB port")
    adb = shutil.which("adb")
    if adb is None:
        raise RuntimeError("ADB is not available")

    def command(*args):
        result = subprocess.run([adb, "-s", serial, *args], capture_output=True,
                                text=True, timeout=20)
        if result.returncode:
            raise RuntimeError("ADB check failed; no guest configuration was changed")
        return result.stdout.strip()

    def require(value, expected, message):
        if value != expected:
            raise RuntimeError(message)
        return value

    require(command("get-state"), "device", "ADB is not authorized and ready")
    require(command("shell", "getprop", "ro.product.device"), "qemu_arm64",
            "The selected guest is not the AEGIS QEMU product")
    require(command("shell", "getprop", "sys.boot_completed"), "1", "Android boot is incomplete")
    require(command("shell", "getprop", "ro.adb.secure"), "1", "ADB authentication is not enabled")
    require(command("shell", "getenforce"), "Enforcing", "SELinux is not enforcing")
    require(command("shell", "getprop", "ro.aegis.runtime.mode"), "absent",
            "Identity-only product mode is missing or different")
    require(command("shell", "service", "check", "aegis_identity"),
            "Service aegis_identity: found", "AEGIS Binder service is not registered")
    state = command("shell", "/system/bin/aegis", "status")
    tokens = state.split()
    fields = dict(field.split("=", 1) for field in tokens if "=" in field)
    if (len(tokens) != 3 or set(fields) != {"terminal", "runtime", "setup"}
            or fields.get("terminal") != "unauthenticated"
            or fields.get("runtime") != "not-installed"
            or fields.get("setup") not in ("available", "complete", "existing-users", "incomplete")):
        raise RuntimeError("A fresh terminal did not return the expected unauthenticated state")
    # Recheck liveness after the RPC; a successful launcher exit alone is insufficient.
    require(command("shell", "service", "check", "aegis_identity"),
            "Service aegis_identity: found", "AEGIS service disappeared during the check")
    return {
        "checked_at_utc": datetime.now(timezone.utc).isoformat(),
        "serial": serial,
        "fingerprint": command("shell", "getprop", "ro.build.fingerprint"),
        "result": "IDENTITY_SERVICE_RESPONDS",
        "fresh_terminal_state": fields,
        "scope": "boot/service/unauthenticated RPC only; no password, lifecycle or runtime test",
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("serial", help="Explicit local QEMU ADB serial, e.g. 127.0.0.1:15555")
    args = parser.parse_args()
    try:
        print(json.dumps(check(args.serial), indent=2))
    except (RuntimeError, ValueError, subprocess.TimeoutExpired) as error:
        print(f"Identity smoke check: {error}", file=sys.stderr)
        sys.exit(1)
