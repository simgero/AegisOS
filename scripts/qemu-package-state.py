#!/usr/bin/env python3
"""Observe package metadata in an authenticated local development guest.

Read-only developer observation, not a substitute for ordinary GNU execution
or user isolation tests. Package workers and personal runtimes are distinct.
Run at a settled checkpoint; detected lifecycle changes fail the observation.
Existing receipts are never replaced. No credential or personal file is read.
"""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess


def check(condition, message):
    if not condition:
        raise ValueError(message)


def read(path):
    return json.loads(path.read_text())


def observe(args):
    prepared = args.prepared.resolve(strict=True)
    run = args.run.resolve(strict=True)
    check(not args.output.exists(), "Receipt already exists")
    address = (run / "adb-address.txt").read_text().strip()
    check(re.fullmatch(r"127\.0\.0\.1:[0-9]{4,5}", address)
          and 1024 <= int(address.rsplit(":", 1)[1]) <= 65535, "Expected local ADB")
    adb = ["adb", "-s", address, "shell"]

    def raw(command):
        return subprocess.check_output(adb + [command], timeout=40,
                                       stdin=subprocess.DEVNULL)

    def guest(command):
        return raw(command).decode().strip()

    def root(command):
        return guest("su 0 sh -c " + shlex.quote(command))

    def optional(path):
        result = subprocess.run(adb + ["su 0 cat " + shlex.quote(path)],
                                capture_output=True, text=True, timeout=40,
                                stdin=subprocess.DEVNULL)
        if result.returncode == 0:
            return {"present": True, "value": result.stdout.strip()}
        check(result.returncode == 1 and "No such file or directory" in result.stderr,
              "Unexpected metadata read failure: " + path)
        return {"present": False, "reason": "ENOENT"}

    avb = read(prepared / "avb-checked.json")
    check(re.fullmatch(r"[0-9a-f]{40}", avb["builder_commit"]), "Invalid image commit")
    profile = read(Path((run / "profile-path.txt").read_text().strip()) / "profile.json")
    disk = profile["bindings"]["base_disk"]
    check(Path(disk["path"]).resolve() == prepared / "android.raw", "Different profile image")
    check(disk["sha256"] == read(prepared / "android.raw.json")["sha256"], "Different disk digest")
    baseline = read(args.baseline)
    expected_ce = sorted(set(args.expected_ce))
    check(0 in expected_ce and all(n >= 0 for n in expected_ce), "Invalid expected CE users")

    def system():
        check(guest("getprop sys.boot_completed") == "1", "Boot incomplete")
        check(guest("getprop ro.adb.secure") == "1", "Require authenticated ADB")
        check(guest("getprop ro.aegis.runtime.mode") == "managed-v1", "Wrong runtime mode")
        check(guest("getprop ro.boot.vbmeta.digest") == avb["vbmeta_digest"], "Wrong guest image")
        check(guest("getenforce") == "Enforcing", "SELinux must enforce")
        boot = guest("cat /proc/sys/kernel/random/boot_id")
        pid = guest("pidof system_server")
        check(pid.isdigit(), "Expected one system_server")
        start = guest("cat /proc/" + pid + "/stat").rsplit(")", 1)[1].split()[19]
        check((boot, pid, start) == (baseline["boot_id"], baseline["system_server"],
                                    baseline["system_server_starttime"]), "System identity changed")
        ce = guest('dumpsys mount | grep "^CE unlocked users: "')
        check(sorted(map(int, re.findall(r"[0-9]+", ce))) == expected_ce, "Unexpected CE state")
        return {"boot_id": boot, "system_server": {"pid": pid, "starttime": start},
                "ce": ce, "foreground": guest("am get-current-user"),
                "users": guest("pm list users")}

    before = system()
    ids = sorted(map(int, re.findall(r"UserInfo\{([0-9]+):", before["users"])))
    check(set(expected_ce) <= set(ids), "Unknown CE user")
    def selected_generations():
        result = {"shared": optional("/data/misc/aegis-runtime/shared-packages/current")}
        for uid in ids:
            if uid:
                result[str(uid)] = (optional(f"/data/misc_ce/{uid}/aegis/packages/store/current")
                                   if uid in expected_ce else {"observed": False, "reason": "CE locked"})
        return result

    selections = selected_generations()
    contexts, package_groups = {}, {}
    parent = "/sys/fs/cgroup/aegis-runtime/contexts"
    for name in root("find " + parent + " -mindepth 1 -maxdepth 1 -type d").splitlines():
        key = Path(name).name
        match = re.fullmatch(r"([up])([0-9]+)-s([0-9]+)", key)
        check(match is not None, "Unknown context group: " + key)
        kind = match.group(1)
        uid, serial = map(int, match.groups()[1:])
        check(key == f"{kind}{uid}-s{serial}" and 10 <= uid <= 21472
              and 0 <= serial <= 2147483647 and uid in expected_ce, "Invalid group identity")
        members = root("cat " + name + "/cgroup.procs").split()
        check(all(v.isdigit() for v in members), "Invalid process list")
        if kind == "p":
            package_groups[key] = {"user": uid, "serial": serial, "pids": list(map(int, members)),
                                   "events": root("cat " + name + "/cgroup.events")}
            continue
        candidates = []
        for child in members:
            status = optional("/proc/" + child + "/status")
            if not status["present"]:
                continue
            nspid = re.findall(r"(?m)^NSpid:\s*(.*)$", status["value"])
            if nspid and nspid[0].split() == [child, "1"]:
                candidates.append(child)
        check(len(candidates) == 1, "Expected one personal init: " + key)
        child = candidates[0]
        proc = "/proc/" + child
        start = root("cat " + proc + "/stat").rsplit(")", 1)[1].split()[19]
        roots = [line for line in root("cat " + proc + "/mountinfo").splitlines()
                 if line.split()[4] == "/"]
        check(len(roots) == 1 and "ro" in roots[0].split()[5].split(","), "Expected readonly root")
        block = roots[0].split()[2]
        check(re.fullmatch(r"[0-9]+:[0-9]+", block), "Invalid root device")
        database = raw("su 0 cat " + proc + "/root/var/lib/dpkg/status")
        packages, residual_packages = {}, {}
        for paragraph in database.decode().strip().split("\n\n"):
            pairs = re.findall(r"(?m)^(Package|Version|Architecture|Status): (.*)$", paragraph)
            fields = dict(pairs)
            check(len(pairs) == len(fields) and {"Package", "Status"} <= fields.keys()
                  and fields["Package"] not in packages and fields["Package"] not in residual_packages,
                  "Unexpected package state")
            check(re.fullmatch(r"(?:install|deinstall|purge|hold|unknown) ok (?:installed|config-files|not-installed)",
                               fields["Status"]), "Incomplete or broken package state")
            if not fields["Status"].endswith(" ok installed"):
                residual_packages[fields["Package"]] = fields
                continue
            check({"Version", "Architecture"} <= fields.keys(), "Missing installed package identity")
            packages[fields["Package"]] = fields["Version"]
        contexts[key] = {"pid": int(child), "starttime": start, "root_mount": roots[0],
                         "backing_file": root("cat /sys/dev/block/" + block + "/loop/backing_file"),
                         "installed_packages": packages,
                         "residual_packages": residual_packages,
                         "dpkg_status_sha256": hashlib.sha256(database).hexdigest(),
                         "uid_map": root("cat " + proc + "/uid_map"),
                         "namespaces": {n: root("readlink " + proc + "/ns/" + n)
                                        for n in ("user", "mnt", "pid", "ipc", "uts", "net")},
                         "private_choices": optional(proc + "/root/var/lib/aegis/private-choices")}
        check(root("cat " + proc + "/stat").rsplit(")", 1)[1].split()[19] == start
              and child in root("cat " + name + "/cgroup.procs").split(), "Runtime changed")
    check(selected_generations() == selections, "Selected generations changed during observation")
    check(system() == before, "System state changed during observation")
    proof = {"utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
             "image_commit": avb["builder_commit"], "profile_id": profile["profile_id"], **before,
             "selections": selections, "contexts": contexts, "package_groups": package_groups,
             "observer_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
             "scope": "Read-only package metadata and runtime identity; GNU execution and isolation require separate proof"}
    with args.output.open("x") as stream:
        json.dump(proof, stream, indent=2)
        stream.write("\n")
    print(args.output, hashlib.sha256(args.output.read_bytes()).hexdigest())


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run", type=Path, required=True)
    parser.add_argument("--prepared", type=Path, required=True)
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--expected-ce", type=int, nargs="+", required=True)
    parser.add_argument("--output", type=Path, required=True)
    observe(parser.parse_args())
