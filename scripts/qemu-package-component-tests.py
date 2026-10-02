#!/usr/bin/env python3
"""Run selected package components on a matching, fresh local development guest.

Requires an already booted, authenticated QEMU and a recorded system-only
baseline. Does not create users, approve CLI transactions, boot or reset VMs.
Run selection first to upload the native bundle; then inspect each group before
starting another. Failed synthetic fixtures and all logs are retained.
"""
import argparse
import fcntl
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys
import time


GROUPS = {
    "selection": ("RuntimePackageReconciliation", [
        "PrivateRemovalReturnsCurrentCommonWithoutChangingSelections",
        "PrivateRemovalRejectsStaleBaseInsteadOfImplicitReconciliation",
        "PrivateRemovalSupportsCurrentFactoryBaseWithoutSharedStore",
        "PrivateRemovalCancellationKeepsCurrentStoresAndClosesViews",
        "ReturnsPrivatePreviousAndCurrentSharedWithoutActivation",
        "CurrentPrivateBaseNeedsNoReconciliation"]),
    "planner": ("RuntimePrivateRemovalPlanning", [
        "ReturnsToPublishedCommonVersionAndMatchingLibrary",
        "ReturnsToOlderCommonVersionAndMatchingLibrary",
        "SameVersionUnpinHasNoPackageEffects",
        "UnneededPrivateOnlyRootAndLibraryAreRemoved",
        "RemovedLibraryChoiceRemainsForAnotherExactPrivateRoot",
        "ConflictingRemainingPrivateLibraryIsNotSilentlyReplaced",
        "OwnerRetainsRemovalIntentAndCurrentBaseThroughSelectionAndReview"]),
    "execution": ("RuntimePackageExecutor", [
        "PrivateRemovalFallbackDowngradesMatchingDependencyAndPreservesConfiguration",
        "PrivateRemovalSameVersionChangesManifestWithoutRunningPackageScripts",
        "ReconciliationUpgradePreservesConfigurationServiceOwnerAndPrivateIntent",
        "ReconciliationDowngradeInstallsMatchingLibraryAndKeepsConfiguration",
        "ReconciliationWithNoEffectsChangesOnlyFullMarksWithoutPackageScripts"]),
    "publication": ("RuntimeReconciliationPlanning", [
        "SignedTriplePlanExecutesAndPublishesPreservingPrivateFilesAndOwner",
        "SignedPrivateVersionOverridePublishesNewBaseWithoutPackageChanges"]),
}
BUNDLE = {"AegisRuntimeNativeTests", "aegis-package-execute",
          "aegis-package-execute-probe", "aegis-package-network", "aegis-package-plan",
          "aegis-package-plan-probe", "aegis-package-prepare", "aegis-package-publish",
          "aegis-runtime-init", "aegis-runtime-namespace-probe", "aegis-runtime-setup"}
JAVA_CLASSES = "org.aegisos.identity.PackageBrokerProtocolTest,org.aegisos.identity.PackageTransactionTest"


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def read(path):
    return json.loads(path.read_text())


def check(condition, message):
    if not condition:
        raise ValueError(message)


def test_only_sources(base, components):
    check(base.get("version") == 1 and components.get("version") == 1, "Unknown source receipt schema")
    check(base["files"].keys() == components["files"].keys(), "Source inventory changed")
    changed = sorted(name for name in base["files"] if base["files"][name] != components["files"][name])
    check(changed == ["runtime/package_executor_tests.cpp"], "Expected only the executor test correction")
    return changed


def run(args):
    run_dir, prepared = args.run.resolve(strict=True), args.prepared.resolve(strict=True)
    address = (run_dir / "adb-address.txt").read_text().strip()
    check(re.fullmatch(r"127\.0\.0\.1:[0-9]{4,5}", address), "Expected local ADB endpoint")
    check(1024 <= int(address.rsplit(":", 1)[1]) <= 65535, "Invalid local port")
    adb = ["adb", "-s", address]

    def shell(command):
        return subprocess.check_output(adb + ["shell", command], text=True,
                                       stdin=subprocess.DEVNULL, timeout=30).strip()

    def command(argv, log):
        with log.open("xb") as stream:
            return subprocess.run(argv, stdout=stream, stderr=subprocess.STDOUT,
                                  stdin=subprocess.DEVNULL).returncode

    receipt, avb = read(args.receipt), read(prepared / "avb-checked.json")
    commit = receipt["source_commit"]
    check(re.fullmatch("[0-9a-f]{40}", commit), "Expected full source commit")
    equivalence = None
    if avb["builder_commit"] != commit:
        check(args.group != "java" and args.test_only_reference is not None and args.component_sources is not None,
              "Different native test commit requires explicit source and component reference receipts")
        reference = read(args.test_only_reference)
        check(reference["source_commit"] == avb["builder_commit"], "Reference must belong to the image commit")
        check(set(reference["files"]) == BUNDLE and set(receipt["files"]) == BUNDLE, "Unexpected reference bundle")
        check(all(reference["files"][name] == receipt["files"][name] for name in BUNDLE - {"AegisRuntimeNativeTests"}),
              "Production helper bytes changed")
        check(reference["java_tests_apk_sha256"] == receipt["java_tests_apk_sha256"], "Java test artifact changed")
        base_sources = prepared / "build-receipts/identity-source-files.json"
        check(digest(base_sources) == read(prepared / "build-validation.json")["receipts"]["identity-source-files.json"],
              "Image source receipt changed")
        check(digest(args.component_sources) == receipt["source_files_sha256"], "Component source receipt changed")
        equivalence = {"changed_sources": test_only_sources(read(base_sources), read(args.component_sources)),
                       "image_source_receipt_sha256": digest(base_sources),
                       "component_source_receipt_sha256": digest(args.component_sources),
                       "reference_receipt_sha256": digest(args.test_only_reference),
                       "unchanged_helpers": sorted(BUNDLE - {"AegisRuntimeNativeTests"})}
    else:
        check(args.test_only_reference is None and args.component_sources is None,
              "Test-only reference options are only for a different native test commit")
    profile = read(Path((run_dir / "profile-path.txt").read_text().strip()) / "profile.json")
    disk = profile["bindings"]["base_disk"]
    check(Path(disk["path"]).resolve() == prepared / "android.raw", "Wrong profile base")
    check(disk["sha256"] == read(prepared / "android.raw.json")["sha256"], "Wrong base checksum")
    check(set(receipt["files"]) == BUNDLE, "Unexpected component bundle")
    check({path.name for path in args.components.iterdir()} == BUNDLE, "Unexpected bundle directory contents")
    for name, expected in receipt["files"].items():
        check((args.components / name).is_file() and not (args.components / name).is_symlink(),
              "Expected a regular component: " + name)
        check(digest(args.components / name) == expected, "Component checksum mismatch: " + name)
    if args.group == "java":
        check(args.apk is not None, "Java tests require --apk")
        check(digest(args.apk) == receipt["java_tests_apk_sha256"], "Wrong Java test APK")

    def state():
        check(shell("getprop sys.boot_completed") == "1", "Boot incomplete")
        check(shell("getprop ro.adb.secure") == "1", "ADB authentication required")
        check(shell("getprop ro.aegis.runtime.mode") == "managed-v1", "Wrong runtime mode")
        check(shell("getprop ro.boot.vbmeta.digest") == avb["vbmeta_digest"], "Wrong guest image")
        check(re.findall(r"UserInfo\{([0-9]+):", shell("pm list users")) == ["0"],
              "Use a fresh profile before creating personal users")
        check(set(re.findall(r"UserInfo\{([0-9]+):", shell("dumpsys user"))) == {"0"},
              "Partial personal users present")
        check("populated 0" in shell("su 0 cat /sys/fs/cgroup/aegis-runtime/contexts/cgroup.events").splitlines(),
              "Runtime contexts must be empty")
        pid = shell("pidof system_server")
        check(pid.isdigit(), "Expected one system_server")
        start = shell("cat /proc/" + pid + "/stat").rsplit(")", 1)[1].split()[19]
        return {"boot_id": shell("cat /proc/sys/kernel/random/boot_id"),
                "system_server": pid, "system_server_starttime": start,
                "selinux": shell("getenforce"),
                "ce": shell('dumpsys mount | grep "^CE unlocked users: "')}

    # A per-run host lock also covers the Java group and bundle upload.
    with (run_dir / "package-component-tests.lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        before = state()
        check(before == read(args.baseline), "Guest differs from recorded baseline")
        check(before["selinux"] == "Enforcing" and before["ce"] == "CE unlocked users: [0]",
              "Require Enforcing and system-only CE state")
        check(int(shell("df -k /data").splitlines()[-1].split()[3]) >= 3 * 1024 * 1024,
              "Need at least 3 GiB free for synthetic image fixtures")
        check(not shell("pidof AegisRuntimeNativeTests || true"), "Native tests already running")
        args.output.mkdir(mode=0o700, parents=True, exist_ok=False)
        target = "/data/local/tmp/aegis-package-tests-" + commit[:12]
        proof = {"source_commit": commit, "guest_image_commit": avb["builder_commit"],
                 "group": args.group, "profile_id": profile["profile_id"],
                 "vbmeta_digest": avb["vbmeta_digest"], "adb_address": address,
                 "before": before, "build_receipt_sha256": digest(args.receipt),
                 "driver_sha256": digest(Path(__file__)), "passed": False,
                 "scope": "Component tests on the matching full image; real CLI T15/T16 and the full DoD remain separate."}
        if equivalence is not None:
            proof["test_only_equivalence"] = equivalence
        started = time.monotonic()
        try:
            if args.group == "java":
                check(command(adb + ["install", "-r", "-t", str(args.apk)], args.output / "install.log") == 0,
                      "Test APK installation failed")
                check("Success" in (args.output / "install.log").read_text(), "Install success missing")
                argv = adb + ["shell", "am instrument -w -r -e class " + JAVA_CLASSES +
                              " org.aegisos.identity.tests/androidx.test.runner.AndroidJUnitRunner"]
                proof.update(classes=JAVA_CLASSES, expected_tests=38,
                             apk_sha256=receipt["java_tests_apk_sha256"])
            else:
                if args.group == "selection":
                    shell("test ! -e " + target)
                    check(command(adb + ["push", str(args.components), target], args.output / "upload.log") == 0,
                          "Bundle upload failed")
                    paths = " ".join([target] + [target + "/" + name for name in sorted(BUNDLE)])
                    shell("su 0 chown 0:2000 " + paths)
                    shell("su 0 chmod 0755 " + paths)
                for name, expected in receipt["files"].items():
                    check(shell("sha256sum " + target + "/" + name).split()[0] == expected,
                          "Guest component checksum mismatch: " + name)
                suite, cases = GROUPS[args.group]
                names = [suite + "." + name for name in cases]
                selected = ":".join(names)
                proof.update(filter=selected, expected_tests=len(names), cases=names)
                argv = adb + ["shell", "su 0 " + shlex.quote(target + "/AegisRuntimeNativeTests") +
                              " " + shlex.quote("--gtest_filter=" + selected)]
            print("Running", args.group, "on", address, "with", proof["expected_tests"], "expected tests", flush=True)
            proof["exit_code"] = command(argv, args.output / "tests.log")
            log = (args.output / "tests.log").read_text(errors="replace")
            proof["after"] = state()
            check(proof["after"] == before, "System state changed during tests")
            check(proof["exit_code"] == 0, "Test command failed")
            if args.group == "java":
                check("OK (38 tests)" in log and "FAILURES!!!" not in log and "INSTRUMENTATION_FAILED" not in log,
                      "Java tests not fully passed")
                check(not re.search(r"INSTRUMENTATION_STATUS_CODE: -(?:3|4)\b", log), "Skipped Java tests")
            else:
                check("[  SKIPPED ]" not in log and "[  FAILED  ]" not in log, "Failed or skipped tests")
                check(all("[       OK ] " + name + " (" in log for name in names), "Expected native results missing")
            proof["passed"] = True
        except Exception as error:
            proof["error"] = str(error)
            raise
        finally:
            proof["seconds"] = time.monotonic() - started
            if (args.output / "tests.log").exists():
                proof["log_sha256"] = digest(args.output / "tests.log")
            (args.output / "result.json").write_text(json.dumps(proof, indent=2) + "\n")
            print(json.dumps(proof, indent=2), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("run", "prepared", "components", "receipt", "baseline", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--group", choices=["java", *GROUPS], required=True)
    parser.add_argument("--apk", type=Path)
    parser.add_argument("--test-only-reference", type=Path,
                        help="Original component receipt from the image commit; requires --component-sources")
    parser.add_argument("--component-sources", type=Path,
                        help="Source inventory from the corrected native build; only executor test source may differ")
    try:
        run(parser.parse_args())
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        sys.exit("Package component test failed: " + str(error))
