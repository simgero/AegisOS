#!/usr/bin/env python3
"""Audit frozen QEMU init/logcat logs without executing any guest command.

Exit 0: no findings in the supplied capture. Exit 2: review required (including
every nonzero service exit). Neither result constitutes full D1 acceptance.
The caller must independently establish image, boot and capture continuity.
"""
import argparse
from collections import Counter
import datetime
import hashlib
import json
from pathlib import Path
import re


TIME = re.compile(r"^\[\s*([0-9.]+)\]")
START = re.compile(r"\.\.\. started service '(.+)' has pid (\d+)")
EXIT = re.compile(r"Service '(.+)' \(pid (\d+)\) exited with status (\d+)")
SIGNAL = re.compile(r"Service '(.+)' \(pid (\d+)\) received signal (\d+)")
CONTROL = re.compile(r"Control message: Processed (ctl\.stop|ctl\.restart) for '(.+)' from pid: (\d+)(.*)")
SENDING = re.compile(r"Sending signal (\d+) to service '(.+)' \(pid (\d+)\) process group")
STOP_COMMAND = re.compile(r"Command 'stop ([^']+)' action=.*succeeded")
SHUTDOWN_START = re.compile(r"\binit: Reboot start, reason: (shutdown(?:,[^,]*)?), reboot_target:")
SYSTEM_SERVER = re.compile(r"\s(\d+)\s+\d+\s+[A-Z]\s+SystemServer\s*: Entered the Android system server!")
FATAL = ("FATAL EXCEPTION", "Fatal signal", "am_crash", "am_anr",
         "FORTIFY:", "WATCHDOG KILLING SYSTEM PROCESS")


def audit(android, logcat, expected_system_server):
    """Keep every finding; a matching init control explains origin, not safety."""
    active = {}
    controls = {}
    sends = {}
    nonzero, signals, unparsed, starts = [], [], [], []
    previous_time = None
    clock_regressions = []
    service_start_count = 0
    shutdown = None
    for number, line in enumerate(android.splitlines(), 1):
        match_time = TIME.match(line)
        now = float(match_time[1]) if match_time else None
        if now is not None:
            if previous_time is not None and now < previous_time:
                clock_regressions.append(number)
            previous_time = now
        shutdown_match = SHUTDOWN_START.search(line)
        if shutdown_match:
            shutdown = {"line": number, "time": now, "reason": shutdown_match[1]}
            # A send from an earlier shutdown episode cannot explain this one.
            sends.clear()
        start = START.search(line)
        if start:
            service_start_count += 1
            name, pid = start[1], int(start[2])
            active[name] = (pid, number)
            controls.pop(name, None)
            sends.pop(name, None)
        sent = SENDING.search(line)
        if sent:
            sig, name, pid = int(sent[1]), sent[2], int(sent[3])
            if active.get(name, (None,))[0] == pid:
                sends[name] = (sig, pid, now, number, active[name])
        control = CONTROL.search(line)
        if control and control[2] in active:
            name = control[2]
            controls[name] = {"kind": control[1], "line": number,
                              "time": now, "generation": active[name],
                              "caller_pid": int(control[3])}
        command = STOP_COMMAND.search(line)
        if command and command[1] in active:
            name = command[1]
            sent = sends.get(name)
            if (sent and sent[0] == 9 and sent[1] == active[name][0]
                    and now is not None and sent[2] is not None
                    and 0 <= now - sent[2] <= 1):
                controls[name] = {"kind": "init stop command", "line": number,
                                  "time": now, "generation": active[name]}
        exited, signaled = EXIT.search(line), SIGNAL.search(line)
        shutdown_evidence = None
        if exited or signaled:
            terminal = exited or signaled
            name, pid = terminal[1], int(terminal[2])
            sent = sends.get(name)
            if (shutdown and sent and now is not None and sent[2] is not None
                    and shutdown["time"] is not None
                    and active.get(name) == sent[4] and sent[1] == pid
                    and shutdown["line"] < sent[3] < number
                    and shutdown["time"] <= sent[2] <= now
                    and sent[0] in (9, 15)
                    and (exited or sent[0] == int(signaled[3]))):
                shutdown_evidence = {
                    "shutdown": shutdown.copy(), "service_start_line": sent[4][1],
                    "send_line": sent[3], "send_time": sent[2], "sent_signal": sent[0],
                    "seconds_after_send": now - sent[2],
                    "scope": "Preceding same-instance shutdown signal; not an automatic exit waiver.",
                }
        if exited:
            name, pid, status = exited[1], int(exited[2]), int(exited[3])
            if status:
                nonzero.append({"service": name, "pid": pid, "status": status,
                                "line": number, "time": now,
                                "shutdown_correlation": shutdown_evidence})
        elif signaled:
            name, pid, sig = signaled[1], int(signaled[2]), int(signaled[3])
            item = {"service": name, "pid": pid, "signal": sig,
                    "line": number, "time": now, "preceding_control": None,
                    "shutdown_correlation": shutdown_evidence}
            candidate = controls.get(name)
            if (sig == 9 and candidate and now is not None
                    and candidate["time"] is not None
                    and active.get(name, (None,))[0] == pid
                    and candidate["generation"] == active[name]
                    and 0 <= now - candidate["time"] <= 1):
                item["preceding_control"] = candidate
            signals.append(item)
        elif "Service '" in line and ("exited" in line or "received signal" in line):
            unparsed.append(number)
        if exited or signaled:
            terminal = exited or signaled
            name, pid = terminal[1], int(terminal[2])
            if active.get(name, (None,))[0] == pid:
                active.pop(name)
                controls.pop(name, None)
                sends.pop(name, None)
    for number, line in enumerate(logcat.splitlines(), 1):
        if "Entered the Android system server!" in line:
            match = SYSTEM_SERVER.search(line)
            starts.append({"line": number, "pid": int(match[1]) if match else None})
    fatal = {name: {needle: text.count(needle) for needle in FATAL}
             for name, text in (("android", android), ("logcat", logcat))}
    reasons = []
    if not service_start_count:
        reasons.append("missing_init_service_start_records")
    if nonzero:
        reasons.append("nonzero_service_exits_require_individual_review")
    if any(item["preceding_control"] is None for item in signals):
        reasons.append("signal_exit_without_matching_recent_control")
    if len(starts) != 1 or starts[0]["pid"] != expected_system_server:
        reasons.append("missing_ambiguous_or_unexpected_system_server_start")
    if any(count for counts in fatal.values() for count in counts.values()):
        reasons.append("fatal_or_anr_marker")
    if unparsed:
        reasons.append("unparsed_service_terminal_record")
    if clock_regressions:
        reasons.append("init_log_clock_regression")
    return {"status": "REVIEW_REQUIRED" if reasons else "NO_FINDINGS_IN_SUPPLIED_CAPTURE",
            "review_reasons": reasons, "nonzero_exits": nonzero,
            "signal_exits": signals, "system_server_starts": starts,
            "fatal_marker_counts": fatal, "unparsed_terminal_lines": unparsed,
            "clock_regression_lines": clock_regressions,
            "service_start_count": service_start_count,
            "signal_counts": dict(Counter(item["service"] for item in signals))}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ("android-log", "logcat-log", "output"):
        parser.add_argument("--" + option, type=Path, required=True)
    parser.add_argument("--expected-system-server", type=int, required=True)
    args = parser.parse_args()
    if args.expected_system_server <= 0:
        parser.error("Expected SystemServer PID must be positive")
    if args.output.exists():
        parser.error("Preserve the existing receipt; choose a new output")
    raw = {"android": args.android_log.read_bytes(), "logcat": args.logcat_log.read_bytes()}
    report = audit(raw["android"].decode(errors="replace"),
                   raw["logcat"].decode(errors="replace"), args.expected_system_server)
    report.update({"utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
                   "inputs": {name: {"path": str(path.resolve()), "bytes": len(raw[name]),
                                     "sha256": hashlib.sha256(raw[name]).hexdigest()}
                              for name, path in (("android", args.android_log), ("logcat", args.logcat_log))},
                   "verifier_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                   "limits": ["Offline bounded log audit, not full D1 acceptance or proof of complete logging.",
                              "Caller must establish capture continuity and image/boot identity independently.",
                              "Matching control identifies a preceding lifecycle action; it does not prove that action was appropriate.",
                              "Shutdown correlations retain same-instance signal evidence without suppressing any review reason.",
                              "All nonzero statuses require individual review, even when expected by source analysis.",
                              "Raw log contents remain in input files; report records locations and hashes."]})
    with args.output.open("x") as stream:
        json.dump(report, stream, indent=2)
        stream.write("\n")
    print(args.output, report["status"], hashlib.sha256(args.output.read_bytes()).hexdigest())
    return 2 if report["review_reasons"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
