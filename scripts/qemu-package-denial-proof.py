#!/usr/bin/env python3
"""Verify a recorded synthetic CLI package denial and unchanged guest state.

Offline verifier for qemu-runtime-test.py and qemu-package-state.py receipts.
It performs no guest action. A receipt covers one action/scope/outcome only;
it never promotes that result to the complete authorization matrix.
"""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import re


def require(condition, message):
    if not condition:
        raise ValueError(message)


def read(path):
    return json.loads(path.read_text())


def verify(args):
    require(not args.output.exists(), "Preserve the existing receipt")
    before, after, events = read(args.before), read(args.after), read(args.events)
    if args.end_index is not None:
        require(0 < args.end_index <= len(events), "Invalid event end")
        events = events[:args.end_index]
    require(0 <= args.start_index < len(events), "Invalid event start")
    fields = ("image_commit", "profile_id", "boot_id", "system_server", "ce",
              "foreground", "users", "selections", "contexts", "package_groups")
    for field in fields:
        require(before[field] == after[field], "Changed state: " + field)
    require(before["package_groups"] == {}, "Unsettled package workers")
    current = events[args.start_index:]
    require(not any(e["action"] == "driver-failure" for e in current), "Driver failure in interval")
    plans = [e for e in current if e["action"] == f"package-{args.action}-{args.scope}"]
    require(len(plans) == 1, "Expected one complete plan")
    plan = plans[0]
    area = "gemeinsame Software" if args.scope == "all" else "eigene Pakete"
    require(f"\nPaketplan: {args.action}, Bereich: {area}\n" in plan["output"], "Wrong plan action/scope")
    require("Admin-Benutzer für diesen Plan (leer bricht ab): " in plan["output"], "No approval prompt")
    # The driver names approval events after the in-memory credential slot.
    # A password change switches Beta to newbeta without changing its identity
    # or role. Count both labels together so two outcomes remain ambiguous.
    labels = {"cancel": {"package-cancel-plan"}, "wrong": {"package-approve-wrong"},
              "nonadmin": {"package-approve-beta", "package-approve-newbeta"}}
    results = [e for e in current if e["action"] in labels[args.outcome]]
    require(len(results) == 1, "Expected one denial outcome")
    result = results[0]
    require(current.index(plan) < current.index(result), "Outcome precedes plan")
    require("Paketauftrag fehlgeschlagen. Keine erfolgreiche Veröffentlichung bestätigt." in result["output"],
            "No explicit failed-publication result")
    if args.outcome == "wrong":
        require("AOSP hat das Passwort abgewiesen." in result["output"], "No AOSP password denial")
    elif args.outcome == "nonadmin":
        require("Runtime Test Beta\nAdmin-Passwort" in result["output"]
                and "Aktion abgelehnt." in result["output"], "No synthetic non-admin denial")
    else:
        require("Admin-Passwort" not in result["output"], "Cancellation supplied an admin password")
    statuses = [e for e in current[current.index(result) + 1:] if e["action"] == "status"]
    require(statuses, "No post-denial session check")
    session = statuses[-1]["output"]
    require(re.search(r"(?m)^user=" + re.escape(after["foreground"]) + r" serial=\d+ ", session)
            and "foreground=true running=true ce=unlocked" in session,
            "Requester session was not independently confirmed")
    require(not any(e["action"].startswith("package-approval-start-") or e["action"] == "package-wait"
                    for e in current), "Execution event in denial interval")
    canonical = json.dumps(events, ensure_ascii=True, sort_keys=True, separators=(",", ":")).encode()
    receipt = {
        "status": "PACKAGE_DENIAL_WITH_UNCHANGED_SETTLED_STATE_PASSED",
        "utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "action": args.action, "scope": args.scope, "outcome": args.outcome,
        **{k: before[k] for k in ("image_commit", "profile_id", "boot_id", "system_server")},
        "before": {"path": str(args.before.resolve()), "sha256": hashlib.sha256(args.before.read_bytes()).hexdigest()},
        "after": {"path": str(args.after.resolve()), "sha256": hashlib.sha256(args.after.read_bytes()).hexdigest()},
        "unchanged_fields": list(fields), "events": current,
        "source_events": str(args.events.resolve()), "source_start_index": args.start_index,
        "source_prefix_count": len(events), "source_prefix_canonical_sha256": hashlib.sha256(canonical).hexdigest(),
        "canonical_encoding": "UTF-8 JSON ensure_ascii=true sort_keys=true separators comma/colon; first source_prefix_count entries",
        "verifier_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "limits": ["One action/scope/outcome; not the full authorization matrix or an execution-phase cancellation.",
                   "Snapshots are sequential settled observations. No package-worker removal claim is inferred from absence at both endpoints.",
                   "The synthetic non-admin identity is the existing driver's Beta account; this verifier does not create or change its AOSP role.",
                   "Generic CLI failure/relogin wording is retained, not treated as a real loss of the independently confirmed session."]
    }
    with args.output.open("x") as stream:
        json.dump(receipt, stream, indent=2, ensure_ascii=False)
        stream.write("\n")
    print(args.output, hashlib.sha256(args.output.read_bytes()).hexdigest())


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ("before", "after", "events", "output"):
        parser.add_argument("--" + option, type=Path, required=True)
    parser.add_argument("--start-index", type=int, required=True)
    parser.add_argument("--end-index", type=int, help="Exclusive immutable prefix end; default is current length")
    parser.add_argument("--action", choices=("install", "update", "remove"), required=True)
    parser.add_argument("--scope", choices=("all", "user"), required=True)
    parser.add_argument("--outcome", choices=("cancel", "wrong", "nonadmin"), required=True)
    verify(parser.parse_args())
