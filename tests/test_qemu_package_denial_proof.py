"""Credential rotation must not break or weaken the offline denial evidence gate."""
import contextlib
import copy
import importlib.util
import io
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest

spec = importlib.util.spec_from_file_location("package_denial_proof",
        Path(__file__).resolve().parents[1] / "scripts/qemu-package-denial-proof.py")
verifier = importlib.util.module_from_spec(spec)
spec.loader.exec_module(verifier)


class CredentialRotationEvidence(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.before = {
            "image_commit": "f" * 40, "profile_id": "synthetic-profile",
            "boot_id": "synthetic-boot", "system_server": {"pid": "10", "starttime": "20"},
            "ce": "CE unlocked users: [0, 11]", "foreground": "11", "users": "synthetic users",
            "selections": {"11": {"present": True, "value": "synthetic selection"}},
            "contexts": {"u11-s11": {"pid": 42, "installed_packages": {"jq": "u3"}}},
            "package_groups": {}}
        self.after = copy.deepcopy(self.before)
        self.events = [
            {"action": "package-remove-user", "output": "\nPaketplan: remove, Bereich: eigene Pakete\n"
                "Admin-Benutzer für diesen Plan (leer bricht ab): "},
            {"action": "package-approve-newbeta", "output": "Runtime Test Beta\nAdmin-Passwort für diesen Plan: \n"
                "Aktion abgelehnt.\nPaketauftrag fehlgeschlagen. Keine erfolgreiche Veröffentlichung bestätigt."},
            {"action": "status", "output": "user=11 serial=11 name=Runtime Test Beta admin=false "
                "foreground=true running=true ce=unlocked"}]

    def verify(self):
        for name, value in (("before", self.before), ("after", self.after), ("events", self.events)):
            (self.root / (name + ".json")).write_text(json.dumps(value))
        output = self.root / "proof.json"
        args = SimpleNamespace(before=self.root / "before.json", after=self.root / "after.json",
            events=self.root / "events.json", output=output, start_index=0, end_index=None,
            action="remove", scope="user", outcome="nonadmin")
        with contextlib.redirect_stdout(io.StringIO()):
            verifier.verify(args)
        return json.loads(output.read_text())

    def test_legacy_beta_credential_is_still_supported(self):
        self.events[1]["action"] = "package-approve-beta"
        self.assertEqual("nonadmin", self.verify()["outcome"])

    def test_rotated_beta_credential_preserves_the_actual_driver_event(self):
        receipt = self.verify()
        self.assertEqual("package-approve-newbeta", receipt["events"][1]["action"])
        self.assertEqual("nonadmin", receipt["outcome"])

    def test_two_credential_results_are_ambiguous(self):
        legacy = {**self.events[1], "action": "package-approve-beta"}
        self.events.insert(2, legacy)
        with self.assertRaisesRegex(ValueError, "Expected one denial outcome"):
            self.verify()

    def test_wrong_password_cannot_stand_in_for_nonadmin_authorization(self):
        self.events[1]["output"] = self.events[1]["output"].replace(
            "Aktion abgelehnt.", "AOSP hat das Passwort abgewiesen.")
        with self.assertRaisesRegex(ValueError, "No synthetic non-admin denial"):
            self.verify()

    def test_other_identity_cannot_use_the_beta_event_label(self):
        self.events[1]["output"] = self.events[1]["output"].replace("Runtime Test Beta", "Runtime Test Alpha")
        with self.assertRaisesRegex(ValueError, "No synthetic non-admin denial"):
            self.verify()

    def test_rotated_credential_does_not_relax_state_equality(self):
        self.after["contexts"]["u11-s11"]["installed_packages"]["jq"] = "u4"
        with self.assertRaisesRegex(ValueError, "Changed state: contexts"):
            self.verify()

    def test_unrecognized_credential_label_is_rejected(self):
        self.events[1]["action"] = "package-approve-someone"
        with self.assertRaisesRegex(ValueError, "Expected one denial outcome"):
            self.verify()


if __name__ == "__main__":
    unittest.main()
