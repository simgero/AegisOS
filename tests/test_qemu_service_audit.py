import importlib.util
from pathlib import Path
import unittest


SOURCE = Path(__file__).resolve().parents[1] / "scripts/qemu-service-audit.py"
SPEC = importlib.util.spec_from_file_location("qemu_service_audit", SOURCE)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)
LOGCAT = "10-04 02:17:30.650 1175 1175 I SystemServer: Entered the Android system server!\n"


class ServiceAuditTests(unittest.TestCase):
    def audit(self, text, logcat=LOGCAT):
        return MODULE.audit(text, logcat, 1175)

    def test_nonzero_oneshot_is_always_retained(self):
        report = self.audit("[ 4.1][ T1] init: Service 'misctrl' (pid 12) exited with status 1 oneshot")
        self.assertEqual(report["status"], "REVIEW_REQUIRED")
        self.assertEqual(report["nonzero_exits"][0]["service"], "misctrl")

    def test_missing_init_capture_is_not_clean(self):
        self.assertIn("missing_init_service_start_records", self.audit("")["review_reasons"])

    def test_same_generation_recent_stop_correlates(self):
        report = self.audit("\n".join([
            "[ 2.0][ T1] init: ... started service 'idmap2d' has pid 12",
            "[ 4.0][ T1] init: Control message: Processed ctl.stop for 'idmap2d' from pid: 1175 (system_server)",
            "[ 4.1][ T1] init: Service 'idmap2d' (pid 12) received signal 9"]))
        self.assertEqual(report["status"], "NO_FINDINGS_IN_SUPPLIED_CAPTURE")
        self.assertEqual(report["signal_exits"][0]["preceding_control"]["caller_pid"], 1175)

    def test_stale_control_and_pid_reuse_do_not_explain_later_exit(self):
        for middle in (
            "[ 4.05][ T1] init: ... started service 'idmap2d' has pid 12",
            "[ 4.05][ T1] init: ... started service 'idmap2d' has pid 13",
            "[ 4.05][ T1] init: Service 'idmap2d' (pid 12) exited with status 0",
        ):
            with self.subTest(middle=middle):
                report = self.audit("\n".join([
                    "[ 2.0][ T1] init: ... started service 'idmap2d' has pid 12",
                    "[ 4.0][ T1] init: Control message: Processed ctl.stop for 'idmap2d' from pid: 1175 (system_server)",
                    middle, "[ 4.1][ T1] init: Service 'idmap2d' (pid 12) received signal 9"]))
                self.assertIsNone(report["signal_exits"][0]["preceding_control"])

    def test_control_time_window_and_signal_type(self):
        for when, signal in (("5.01", 9), ("4.1", 11)):
            report = self.audit("\n".join([
                "[ 2.0][ T1] init: ... started service 'service' has pid 12",
                "[ 4.0][ T1] init: Control message: Processed ctl.stop for 'service' from pid: 1175 (system_server)",
                f"[ {when}][ T1] init: Service 'service' (pid 12) received signal {signal}"]))
            self.assertIsNone(report["signal_exits"][0]["preceding_control"])

    def test_sending_signal_alone_is_not_an_explanation(self):
        report = self.audit("\n".join([
            "[ 2.0][ T1] init: ... started service 'service' has pid 12",
            "[ 4.0][ T1] init: Sending signal 9 to service 'service' (pid 12) process group...",
            "[ 4.1][ T1] init: Service 'service' (pid 12) received signal 9"]))
        self.assertIsNone(report["signal_exits"][0]["preceding_control"])

    def test_init_stop_requires_same_process_send(self):
        for sent_pid in (12, 13):
            report = self.audit("\n".join([
                "[ 2.0][ T1] init: ... started service 'service' has pid 12",
                f"[ 4.0][ T1] init: Sending signal 9 to service 'service' (pid {sent_pid}) process group...",
                "[ 4.05][ T1] init: Command 'stop service' action=disabled=true (service.rc:1) took 10ms and succeeded",
                "[ 4.1][ T1] init: Service 'service' (pid 12) received signal 9"]))
            self.assertEqual(report["signal_exits"][0]["preceding_control"] is not None, sent_pid == 12)

    def test_framework_restart_missing_start_and_wrong_pid_require_review(self):
        for logcat in ("", LOGCAT + LOGCAT, LOGCAT.replace("1175", "1176"),
                       "unparsed Entered the Android system server!"):
            with self.subTest(logcat=logcat):
                self.assertIn("missing_ambiguous_or_unexpected_system_server_start",
                              self.audit("", logcat)["review_reasons"])

    def test_fatal_and_unknown_terminal_record_require_review(self):
        report = self.audit("[ 4.0][ T1] init: Service 'service' (pid 12) exited unexpectedly",
                            LOGCAT + "Fatal signal 11")
        self.assertIn("fatal_or_anr_marker", report["review_reasons"])
        self.assertIn("unparsed_service_terminal_record", report["review_reasons"])

    def test_clock_regression_requires_review(self):
        report = self.audit("[ 4.0][ T1] init: event\n[ 1.0][ T1] init: event")
        self.assertIn("init_log_clock_regression", report["review_reasons"])

    def shutdown_log(self, middle=None, terminal=None):
        return "\n".join([
            "[ 2.0][ T1] init: ... started service 'mdnsd' has pid 12",
            "[ 10.0][ T1] init: Reboot start, reason: shutdown, reboot_target: ",
            "[ 11.0][ T1] init: Sending signal 15 to service 'mdnsd' (pid 12) process group...",
            middle or "[ 12.0][ T1] init: waiting for services",
            terminal or "[ 17.0][ T1] init: Service 'mdnsd' (pid 12) received signal 15",
        ])

    def test_shutdown_correlation_retains_review_and_delayed_signal(self):
        report = self.audit(self.shutdown_log())
        signal = report["signal_exits"][0]
        evidence = signal["shutdown_correlation"]
        self.assertEqual(evidence["service_start_line"], 1)
        self.assertEqual(evidence["send_line"], 3)
        self.assertEqual(evidence["seconds_after_send"], 6)
        self.assertEqual(evidence["shutdown"]["line"], 2)
        self.assertIsNone(signal["preceding_control"])
        self.assertIn("signal_exit_without_matching_recent_control", report["review_reasons"])

    def test_shutdown_nonzero_exit_is_never_waived(self):
        report = self.audit(self.shutdown_log(
            terminal="[ 17.0][ T1] init: Service 'mdnsd' (pid 12) exited with status 4"))
        exit_record = report["nonzero_exits"][0]
        self.assertEqual(exit_record["status"], 4)
        self.assertEqual(exit_record["shutdown_correlation"]["sent_signal"], 15)
        self.assertIn("nonzero_service_exits_require_individual_review", report["review_reasons"])

    def test_shutdown_requires_same_instance_signal_and_timeline(self):
        original = self.shutdown_log()
        cases = [
            original.replace("init: Reboot start", "other: Reboot start"),
            original.replace("reason: shutdown", "reason: reboot"),
            original.replace("Sending signal 15", "Sending signal 9"),
            original.replace("(pid 12) process group", "(pid 13) process group"),
            original.replace("[ 11.0]", "[ 9.0]"),
            original.replace("[ 17.0]", "[ 10.5]"),
            original.replace("Sending signal", "Unparsed signal"),
            original.replace("[ 2.0][ T1] init: ... started service 'mdnsd' has pid 12", ""),
        ]
        for text in cases:
            with self.subTest(text=text):
                self.assertIsNone(self.audit(text)["signal_exits"][0]["shutdown_correlation"])

    def test_shutdown_does_not_reuse_send_after_restart_exit_or_new_episode(self):
        for middle in (
            "[ 12.0][ T1] init: ... started service 'mdnsd' has pid 12",
            "[ 12.0][ T1] init: ... started service 'mdnsd' has pid 13",
            "[ 12.0][ T1] init: Service 'mdnsd' (pid 12) exited with status 0",
            "[ 12.0][ T1] init: Reboot start, reason: shutdown, reboot_target: ",
        ):
            with self.subTest(middle=middle):
                self.assertIsNone(self.audit(self.shutdown_log(middle))["signal_exits"][0]["shutdown_correlation"])


if __name__ == "__main__":
    unittest.main()
