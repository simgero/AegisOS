#!/usr/bin/env python3
"""Test public terminal fixtures against the installed JNI in a fresh local guest.

No AOSP users, passwords or Binder operations are created. Each interruption
targets only this probe's child after PID/starttime/parent/argv/UID checks.
Existing profiles must contain system user 0 only. Never restart a guest.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import select
import shlex
import subprocess
import sys
import time
import uuid

CLASS = "org.aegisos.identity.TerminalConsoleHostProbe"
FIXTURE = b"OnlyPublicFixture42"
LIBRARY = "/system_ext/lib64/libaegis_terminal_jni.so"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("run", "prepared", "probe", "probe-receipt", "image-proof", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    if sys.flags.optimize:
        parser.error("Assertions must remain enabled")
    if args.output.exists():
        parser.error("Preserve previous evidence; output must be new")
    address = (args.run / "adb-address.txt").read_text().strip()
    assert re.fullmatch(r"127\.0\.0\.1:[0-9]{4,5}", address)
    assert 1024 <= int(address.rsplit(":", 1)[1]) <= 65535
    adb = ["adb", "-s", address]

    def call(*command, timeout=60):
        return subprocess.check_output(adb + list(command), stdin=subprocess.DEVNULL,
                                       stderr=subprocess.PIPE, timeout=timeout)

    def shell(command):
        return call("shell", command).decode().strip()

    profile = Path((args.run / "profile-path.txt").read_text().strip()).resolve(strict=True)
    manifest_bytes = (profile / "profile.json").read_bytes()
    manifest = json.loads(manifest_bytes)
    assert Path(manifest["bindings"]["base_disk"]["path"]).resolve() == args.prepared.resolve() / "android.raw"
    avb = json.loads((args.prepared / "avb-checked.json").read_text())
    proof = json.loads(args.image_proof.read_text())
    assert proof["status"] == "TERMINAL_PACKAGED_BYTES_VERIFIED_ANDROID_EXECUTION_PENDING"
    assert proof["build_commit"] == avb["builder_commit"]
    source_path = args.prepared / "build-receipts/identity-source-files.json"
    assert digest(source_path) == proof["source_receipt_file"]["sha256"]
    sources = json.loads(source_path.read_text())["files"]
    probe = json.loads(args.probe_receipt.read_text())
    assert probe["status"] == "PUBLIC_TERMINAL_PROBE_BUILT_NOT_INSTALLED"
    assert probe["runtime_library"] == LIBRARY
    assert digest(args.probe) == probe["artifact_sha256"]
    for entry in probe["sources"]:
        if entry["path"].startswith("packages/aegis/identity/"):
            assert sources[entry["path"][len("packages/aegis/identity/"):]] == entry["sha256"]

    def snapshot():
        assert call("get-state").strip() == b"device"
        properties = {name: shell("getprop " + name) for name in
                      ("sys.boot_completed", "ro.adb.secure", "ro.boot.vbmeta.digest",
                       "ro.crypto.state", "ro.crypto.type")}
        assert properties == {"sys.boot_completed": "1", "ro.adb.secure": "1",
                              "ro.boot.vbmeta.digest": avb["vbmeta_digest"],
                              "ro.crypto.state": "encrypted", "ro.crypto.type": "file"}
        assert shell("getenforce") == "Enforcing"
        assert re.findall(r"UserInfo\{([0-9]+):", shell("pm list users")) == ["0"]
        ce = [line.strip() for line in shell("dumpsys mount").splitlines()
              if "CE unlocked users:" in line]
        assert ce == ["CE unlocked users: [0]"]
        pid = shell("pidof system_server")
        assert pid.isdigit()
        start = shell("cat /proc/" + pid + "/stat").rsplit(") ", 1)[1].split()[19]
        library_sha = shell("sha256sum " + LIBRARY).split()[0]
        assert library_sha == proof["library"]["sha256"]
        return {"properties": properties, "boot_id": shell("cat /proc/sys/kernel/random/boot_id"),
                "system_server": {"pid": pid, "starttime": start}, "ce": ce,
                "library_sha256": library_sha, "selinux": "Enforcing", "users": [0]}

    before = snapshot()
    os.umask(0o077)
    args.output.mkdir(parents=True, exist_ok=False)
    scratch = "/data/local/tmp/aegis-terminal-" + uuid.uuid4().hex
    report = {"status": "INCOMPLETE", "started_utc": datetime.now(timezone.utc).isoformat(),
              "image_commit": avb["builder_commit"], "profile_id": manifest["profile_id"],
              "profile_manifest_sha256": hashlib.sha256(manifest_bytes).hexdigest(),
              "probe_sha256": digest(args.probe), "image_proof_sha256": digest(args.image_proof),
              "verifier_sha256": digest(Path(__file__)), "before": before, "cases": [],
              "scratch": scratch, "cleanup": "not_created",
              "limits": ["Public fixtures only; no actual authentication or password-store audit.",
                         "Does not prove complete T01, D1 or overall Phase 1 acceptance."]}

    def save():
        (args.output / "result.json").write_text(json.dumps(report, indent=2) + "\n")

    created = False
    active = None
    try:
        save()
        shell("mkdir -m 700 " + scratch)
        created = True
        call("push", str(args.probe), scratch + "/probe.zip")
        assert shell("sha256sum " + scratch + "/probe.zip").split()[0] == report["probe_sha256"]
        invocation = "CLASSPATH=" + scratch + "/probe.zip app_process /system/bin " + CLASS
        cases = [(name, None) for name in ("normal", "maximum", "overflow", "eof")]
        cases += [("interrupt", (name, number)) for name, number in
                  (("INT", 2), ("TERM", 15), ("HUP", 1), ("QUIT", 3), ("TSTP", 20))]
        for index, (scenario, interrupt) in enumerate(cases):
            token = "__AEGIS_TERMINAL_" + uuid.uuid4().hex
            wrapper = (
                "test -t 0 && test -t 1 || exit 90; "
                "stty echo echonl icanon isig; before=$(stty -g); exec 3<&0; "
                # mksh redirects background stdin to /dev/null before applying
                # the command's redirections. Preserve the parent's PTY first.
                + invocation + " " + scenario + " <&3 3<&- & child=$!; exec 3<&-; "
                + "printf '\\n" + token + "_PID:%s:%s\\n' \"$$\" \"$child\"; "
                + "wait \"$child\"; rc=$?; after=$(stty -g); "
                + "test \"$before\" = \"$after\" || exit 91; "
                + "printf '\\n" + token + "_EXIT:%s\\n' \"$rc\"; ")
            if interrupt:
                wrapper += ("printf '" + token + "_NEXT> '; IFS= read -r line; "
                            "test \"$line\" = AFTER || exit 92; ")
            wrapper += "printf '\\n" + token + "_DONE\\n'"
            active = subprocess.Popen(adb + ["shell", "-tt", "sh -c " + shlex.quote(wrapper)],
                                      stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                      stderr=subprocess.STDOUT, bufsize=0)
            transcript = bytearray()
            case = {"scenario": scenario, "signal": interrupt[0] if interrupt else None,
                    "status": "INCOMPLETE"}
            report["cases"].append(case)

            def until(marker):
                deadline = time.monotonic() + 90
                marker = marker.encode()
                while marker not in transcript:
                    if time.monotonic() >= deadline:
                        raise TimeoutError("Terminal test marker not received")
                    ready, _, _ = select.select([active.stdout], [], [], 1)
                    if ready:
                        chunk = os.read(active.stdout.fileno(), 4096)
                        if not chunk:
                            raise RuntimeError("Terminal probe ended before expected marker")
                        transcript.extend(chunk)
                        assert len(transcript) <= 1024 * 1024

            try:
                until("PUBLIC_FIXTURE_PASSWORD> ")
                match = re.search(re.escape(token.encode()) + rb"_PID:([0-9]+):([0-9]+)\r?\n", transcript)
                assert match
                parent, pid = (value.decode() for value in match.groups())
                if interrupt:
                    stat = shell("cat /proc/" + pid + "/stat").rsplit(") ", 1)[1].split()
                    assert stat[1] == parent
                    argv = call("shell", "cat /proc/" + pid + "/cmdline").split(b"\0")
                    assert any(CLASS.encode() in word for word in argv)
                    uid = [line.split()[1:] for line in shell("cat /proc/" + pid + "/status").splitlines()
                           if line.startswith("Uid:")]
                    assert uid == [["2000"] * 4]
                    case["process"] = {"pid": pid, "parent": parent, "starttime": stat[19], "uid": 2000}
                    active.stdin.write(FIXTURE)
                    # Bytes are sent before the next command on the same ordered stream.
                    # A late delivery contaminates AFTER and fails, rather than passing.
                    time.sleep(.2)
                    shell("s=$(cat /proc/" + pid + "/stat); s=${s##*) }; set -- $s; "
                          "test \"${20}\" = " + stat[19] + " && kill -" + interrupt[0] + " " + pid)
                    until(token + "_NEXT> ")
                    assert (token + "_EXIT:" + str(128 + interrupt[1])).encode() in transcript
                else:
                    payload = {"normal": FIXTURE + b"\n", "maximum": b"x" * 128 + b"\n",
                               "overflow": FIXTURE * 10 + b"\n", "eof": b"\x04"}[scenario]
                    active.stdin.write(payload)
                    until("COMMAND> ")
                    assert (b"OVERFLOW_REFUSED" if scenario == "overflow" else b"PASSWORD_RETURNED") in transcript
                active.stdin.write(b"AFTER\n")
                until(token + "_DONE")
                assert active.wait(timeout=15) == 0
                if not interrupt:
                    assert (token + "_EXIT:0").encode() in transcript
                assert FIXTURE not in transcript and b"xxxx" not in transcript
                case["status"] = "PASSED"
                print("PASSED", scenario, interrupt[0] if interrupt else "", flush=True)
            finally:
                if active.poll() is None:
                    active.terminate()  # Only this verifier's own ADB Popen child.
                    try:
                        active.wait(timeout=10)
                    except subprocess.TimeoutExpired:
                        active.kill()
                        active.wait(timeout=10)
                active.stdin.close()
                active.stdout.close()
                active = None
                log = args.output / (f"case-{index:02d}.log")
                log.write_bytes(transcript)
                case["transcript"] = log.name
                case["transcript_sha256"] = digest(log)
                save()
        pipe = call("shell", "-T", invocation + " pipe")
        assert pipe.strip() == b"PIPE_REFUSED"
        report["cases"].append({"scenario": "pipe", "status": "PASSED"})
        after = snapshot()
        assert after == before
        assert (profile / "profile.json").read_bytes() == manifest_bytes
        report["after"] = after
        report["status"] = "PUBLIC_TERMINAL_INPUT_REGRESSION_PASSED"
    except Exception as error:
        report["failure_type"] = type(error).__name__
        raise
    finally:
        if created:
            try:
                shell("rm -f " + scratch + "/probe.zip; rmdir " + scratch)
                report["cleanup"] = "own_scratch_removed"
            except Exception as error:
                report["cleanup"] = "unconfirmed:" + type(error).__name__
                report["status"] = "INCOMPLETE_CLEANUP"
        report["finished_utc"] = datetime.now(timezone.utc).isoformat()
        save()
    if report["status"] != "PUBLIC_TERMINAL_INPUT_REGRESSION_PASSED":
        raise RuntimeError("Terminal regression did not complete with confirmed cleanup")
    print(report["status"], digest(args.output / "result.json"))


if __name__ == "__main__":
    main()
