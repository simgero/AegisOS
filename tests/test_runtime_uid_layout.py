import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("runtime_uid_layout", ROOT / "scripts/runtime/uid_layout.py")
ids = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ids)


class RuntimeUidLayoutTests(unittest.TestCase):
    def setUp(self):
        self.layout = json.loads((ROOT / "runtime/uid-map.json").read_bytes())
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.project = self.root / "project"
        self.aosp = self.root / "aosp"
        for name, text in ids.generated(self.layout).items():
            self.write(self.project / name, text)
        self.write(self.aosp / ids.CONFIG, ids.generated(self.layout)[ids.CONFIG])
        # Failure-path fixtures only. They must never be imported/executed.
        reference = {"aosp_tag": "android-16.0.0_r1", "sha256": {}}
        for name in (ids.HEADER, ids.PARSER):
            content = "metadata fixture: " + name
            self.write(self.aosp / name, content)
            reference["sha256"][name] = hashlib.sha256(content.encode()).hexdigest()
        self.write(self.project / "runtime/aosp-id-reference.json", json.dumps(reference))

    @staticmethod
    def write(path, text):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)

    def rejected_before_parser(self, paths=None):
        with patch.object(ids.importlib.util, "spec_from_file_location",
                          side_effect=AssertionError("Rejected inputs must never load a parser")):
            with self.assertRaises(ValueError):
                ids.check_aosp(self.layout, self.aosp,
                               [ids.CONFIG] if paths is None else paths, self.project)

    def test_checked_in_sources_and_complete_mapping_coverage(self):
        ids.check_generated(self.layout)
        rows = ids.validate(self.layout)
        mapping = {r["inside"] + i: r["app_id"] + i for r in rows for i in range(r["count"])}
        self.assertEqual(set(mapping), set(range(1001)) | {65534})
        self.assertEqual(len(set(mapping.values())), 1002)
        self.assertEqual(mapping[1000], 7500)
        self.assertNotIn(0, mapping.values())
        self.assertNotIn(1000, mapping.values())
        self.assertNotIn(2000, mapping.values())

    def test_malformed_schema_and_non_integer_fields_are_rejected(self):
        for malformed in (None, [], {}, {**self.layout, "extra": 1},
                          {**self.layout, "schema": True},
                          {**self.layout, "per_user_range": 10000},
                          {**self.layout, "ranges": [None]},
                          {**self.layout, "ranges": []}):
            with self.subTest(value=malformed), self.assertRaises(ValueError):
                ids.validate(malformed)
        for key in ("inside", "app_id", "count"):
            for value in (True, 1.5, "1", None):
                layout = copy.deepcopy(self.layout)
                layout["ranges"][0][key] = value
                with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                    ids.validate(layout)

    def test_reserved_partition_boundaries_exclude_core_and_app_ids(self):
        for app_id in (0, 1000, 2000, 2999, 5001, 6000, 10000, 99000):
            layout = copy.deepcopy(self.layout)
            layout["ranges"][0]["app_id"] = app_id
            with self.subTest(app_id=app_id), self.assertRaises(ValueError):
                ids.validate(layout)
        layout = copy.deepcopy(self.layout)
        layout["ranges"][1]["aid"] = "VENDOR_WRONG_PARTITION"
        with self.assertRaises(ValueError):
            ids.validate(layout)

    def test_overlaps_and_duplicate_names_are_rejected(self):
        mutations = ((1, "inside", 999), (2, "app_id", 7500),
                     (2, "aid", "SYSTEM_EXT_AEGIS_RUNTIME_USER"))
        for index, key, value in mutations:
            layout = copy.deepcopy(self.layout)
            layout["ranges"][index][key] = value
            with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                ids.validate(layout)

    def test_unrepresentable_linux_ids_and_aosp_names_are_rejected(self):
        for index, key, value in ((0, "inside", -1), (0, "count", 0), (0, "count", 1001),
                                  (2, "inside", 65535), (2, "count", 2),
                                  (0, "aid", "VENDOR_BAD:NAME"),
                                  (0, "aid", "VENDOR_" + "A" * 25)):
            layout = copy.deepcopy(self.layout)
            layout["ranges"][index][key] = value
            with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                ids.validate(layout)

    def test_root_normal_user_and_nobody_cannot_lose_their_mapping(self):
        for index in range(3):
            layout = copy.deepcopy(self.layout)
            layout["ranges"].pop(index)
            with self.subTest(index=index), self.assertRaises(ValueError):
                ids.validate(layout)

    def test_generated_source_drift_and_symlinks_fail_closed(self):
        for name in (ids.CONFIG, ids.JAVA):
            path = self.project / name
            text = path.read_text()
            path.write_text(text + "changed\n")
            with self.subTest(name=name), self.assertRaises(ValueError):
                ids.check_generated(self.layout, self.project)
            path.unlink()
            target = self.root / "elsewhere"
            target.write_text(text)
            path.symlink_to(target)
            with self.assertRaises(ValueError):
                ids.check_generated(self.layout, self.project)
            path.unlink()
            path.write_text(text)

    def test_changed_aosp_header_or_parser_is_rejected_before_import(self):
        for name in (ids.HEADER, ids.PARSER):
            path = self.aosp / name
            original = path.read_text()
            path.write_text(original + "changed")
            self.rejected_before_parser()
            path.write_text(original)

    def test_missing_or_duplicate_product_registry_is_rejected(self):
        for paths in ([], ["vendor/other/config.fs"], [ids.CONFIG, ids.CONFIG]):
            with self.subTest(paths=paths):
                self.rejected_before_parser(paths)

    def test_product_paths_cannot_escape_or_disagree_with_project(self):
        for name in ("/absolute", "../outside", "vendor//config.fs", "vendor/./config.fs",
                     "vendor/missing.fs"):
            with self.subTest(name=name):
                self.rejected_before_parser([ids.CONFIG, name])
        target = self.root / "outside.fs"
        target.write_text("outside")
        link = self.aosp / "vendor/outside.fs"
        link.parent.mkdir(parents=True)
        link.symlink_to(target)
        self.rejected_before_parser([ids.CONFIG, "vendor/outside.fs"])
        self.write(self.aosp / ids.CONFIG, "stale source")
        self.rejected_before_parser()

    def test_base_checks_file_user_and_group_ids_not_only_personal_user(self):
        report = {"status": "VERIFIED_UPSTREAM_BASE_NOT_INSTALLED",
                  "file_uids": [0], "file_gids": [0, 8, 42, 43],
                  "technical_users": {"root": {"uid": 0, "gid": 0},
                                      "nobody": {"uid": 65534, "gid": 65534}},
                  "technical_groups": {"nogroup": 65534}}
        self.assertEqual(ids.check_base(self.layout, report), 5)
        for section in ("file_uids", "file_gids", "user_uid", "user_gid", "group"):
            bad = copy.deepcopy(report)
            if section.startswith("file_"):
                bad[section].append(64055)
            elif section.startswith("user_"):
                bad["technical_users"]["root"][section[5:]] = 64055
            else:
                bad["technical_groups"]["new-group"] = 64055
            with self.subTest(section=section), self.assertRaises(ValueError):
                ids.check_base(self.layout, bad)
        report["status"] = "UNVERIFIED"
        with self.assertRaises(ValueError):
            ids.check_base(self.layout, report)


if __name__ == "__main__":
    unittest.main()
