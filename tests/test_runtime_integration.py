"""Product/receipt failure tests with inert metadata, never an OS/mount test.

The debugfs inode-inspection boundary is simulated here. Its actual parser and
tiny inert ext4 contents are exercised separately in test_runtime_generation.
"""
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import struct
import sys
import tempfile
import unittest
import uuid
from unittest.mock import patch

from test_runtime_base import fixture
from test_runtime_generation import files, rehash

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("runtime_integration", ROOT / "scripts/runtime/integrate.py")
integration = importlib.util.module_from_spec(spec)
sys.path.insert(0, str(ROOT / "scripts/runtime"))
try:
    spec.loader.exec_module(integration)
finally:
    sys.path.pop(0)
g = integration.generation


class RuntimeIntegrationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.project = self.root / "project"
        for relative in (*g.RECIPE_SOURCES, "scripts/aosp/link-product.py", "scripts/kernel/integrate.py"):
            destination = self.project / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / relative, destination)
        shutil.copytree(ROOT / "device/aegis/qemu_arm64", self.project / "device/aegis/qemu_arm64")
        self.pin, remote = fixture(files())
        (self.project / "runtime/debian-arm64.json").write_bytes(g.encoded(self.pin))
        self.imported = self.root / "import"
        g.base.fetch(self.pin, self.imported,
                     lambda url, path, limit: path.write_bytes(remote[url.rsplit("/", 1)[1]]))
        self.aosp = self.root / "aosp"
        (self.aosp / "build").mkdir(parents=True)
        (self.aosp / "build/envsetup.sh").touch()
        source_reference = json.loads((ROOT / "runtime/filesystem-tools.json").read_bytes())
        for path in source_reference["sha256"]:
            data = ("inert source fixture " + path).encode()
            output = self.aosp / path
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_bytes(data)
            source_reference["sha256"][path] = hashlib.sha256(data).hexdigest()
        (self.project / "runtime/filesystem-tools.json").write_bytes(g.encoded(source_reference))
        tools = {}
        for name in g.TOOLS:
            path = self.aosp / "out/host/linux-x86/bin" / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("inert tool fixture " + name)
            tools[name] = g.sha_file(path)
        size = patch.object(g, "IMAGE_BYTES", 16384)
        size.start()
        self.addCleanup(size.stop)
        inspect_image = patch.object(g, "verify_image")
        self.image_check = inspect_image.start()
        self.addCleanup(inspect_image.stop)
        self.plan = g.make_plan(self.pin, self.imported,
                                json.loads((ROOT / "runtime/uid-map.json").read_bytes()), self.project)
        self.run = self.root / "runtime-base-fixture"
        self.artifacts = self.run / "artifacts"
        self.artifacts.mkdir(parents=True)
        (self.run / "status").write_text("BUILT_VERIFIED_NOT_MOUNTED\n")
        (self.run / "project-commit.txt").write_text("1" * 40 + "\n")
        self.input_id = integration.sha(g.encoded({"plan": self.plan["plan_sha256"], "tools": tools}))
        image_uuid = str(uuid.UUID(self.input_id[:32]))
        # Only an ext4 structural header, not a filesystem or executable image.
        image = bytearray(g.IMAGE_BYTES)
        struct.pack_into("<I", image, 1024 + 4, len(image) // 4096)
        struct.pack_into("<I", image, 1024 + 24, 2)
        struct.pack_into("<H", image, 1024 + 56, 0xEF53)
        struct.pack_into("<I", image, 1024 + 96, 0x40)
        image[1024 + 104:1024 + 120] = uuid.UUID(image_uuid).bytes
        (self.artifacts / "runtime-base.ext4").write_bytes(image)
        digest = integration.sha(image)
        self.generation = {"schema": 1, "status": "BUILT_VERIFIED_NOT_MOUNTED", "generation": digest,
                           "recipe_input_sha256": self.input_id, "plan_sha256": self.plan["plan_sha256"],
                           "tool_sha256": tools, "uuid": image_uuid, "image_sha256": digest,
                           "image_bytes": len(image), "repeated_build_identical": True}
        (self.artifacts / "plan.json").write_bytes(g.encoded(self.plan))
        (self.artifacts / "generation.json").write_bytes(g.encoded(self.generation))
        (self.artifacts / "fs_config.txt").write_text(g.fs_config(self.plan))
        self.product_tool = integration.load_tool(self.project, "scripts/aosp/link-product.py", "base_product_fixture")
        self.product_tool.register(self.project / "device/aegis/qemu_arm64", self.aosp)
        self.product = self.aosp / "device/aegis/qemu_arm64"
        self.receipt = self.root / "aosp-run/runtime-base-inputs.json"

    def inspect(self):
        return integration.inspect(self.run, self.project, self.imported, self.aosp)

    def prepare(self, **kwargs):
        return integration.prepare(self.run, self.project, self.imported, self.aosp, self.receipt, **kwargs)

    def test_selection_rechecks_image_and_installs_assets_before_any_runtime_start(self):
        bundle = self.prepare()
        self.image_check.assert_called_once()
        self.assertEqual(self.image_check.call_args.args[0], self.plan)
        record = json.loads(self.receipt.read_bytes())
        self.assertEqual(record["inputs"]["status"], "CHECKED_BASE_INPUTS_NOT_MOUNTED")
        self.assertEqual(record["inputs"]["generation"], self.generation["generation"])
        directory = self.aosp / "device/aegis/runtime-bases" / bundle
        self.assertEqual(integration.inventory(directory), record["inputs"]["files"])
        text = (self.product / integration.INCLUDE).read_text()
        self.assertIn("$(TARGET_COPY_OUT_SYSTEM_EXT)/etc/aegis/runtime/base.ext4", text)
        self.assertIn("ro.aegis.runtime.mode=absent", (self.product / "aegis_qemu_arm64.mk").read_text())
        for origin, name in (("plan.json", "runtime-base-plan.json"),
                             ("generation.json", "runtime-base-generation.json"),
                             ("fs_config.txt", "runtime-base-fs_config.txt")):
            self.assertEqual((self.artifacts / origin).read_bytes(), (self.receipt.parent / name).read_bytes())

    def test_registered_kernel_selection_is_preserved_and_requires_matching_receipt(self):
        kernel = integration.load_tool(self.project, "scripts/kernel/integrate.py", "base_kernel_fixture")
        prefix = "device/aegis/runtime-kernels/" + "a" * 64
        selected = {"TARGET_KERNEL_PATH": prefix + "/gki/kernel-6.12",
                    "SYSTEM_DLKM_SRC": prefix + "/gki", "KERNEL_MODULES_PATH": prefix + "/vendor"}
        stage = self.root / "selected-product"
        shutil.copytree(self.product, stage)
        text = kernel.selection_text(selected)
        (stage / kernel.INCLUDE).write_text(text)
        self.product_tool.register(stage, self.aosp)
        with self.assertRaisesRegex(ValueError, "Register this project's product"):
            self.prepare()
        receipt = self.root / "kernel-inputs.json"
        receipt.write_bytes(g.encoded({"bundle_id": "a" * 64, "selection": selected}))
        self.prepare(kernel_receipt=receipt)
        self.assertEqual(text, (self.product / kernel.INCLUDE).read_text())

    def test_failed_run_or_false_repeat_claim_cannot_be_selected(self):
        for state in ("ASSEMBLING", "FAILED", "BUILDING_TOOLS"):
            (self.run / "status").write_text(state)
            with self.subTest(state=state), self.assertRaisesRegex(ValueError, "completed"):
                self.inspect()
        (self.run / "status").write_text("BUILT_VERIFIED_NOT_MOUNTED")
        self.generation["repeated_build_identical"] = False
        (self.artifacts / "generation.json").write_bytes(g.encoded(self.generation))
        with self.assertRaisesRegex(ValueError, "do not agree"):
            self.inspect()

    def test_self_consistent_forged_plan_is_rejected_against_original_import(self):
        plan = copy.deepcopy(self.plan)
        entry = next(e for e in plan["entries"] if e["path"] == "etc/passwd")
        entry["text"] += "intruder:x:1000:1000::/home/user:/bin/bash\n"
        entry["size"] = len(entry["text"].encode())
        entry["sha256"] = integration.sha(entry["text"].encode())
        plan = rehash(plan)
        (self.artifacts / "plan.json").write_bytes(g.encoded(plan))
        with self.assertRaisesRegex(ValueError, "pinned import/current recipe"):
            self.inspect()

    def test_changed_image_and_changed_ownership_metadata_are_rejected(self):
        path = self.artifacts / "runtime-base.ext4"
        original = path.read_bytes()
        path.write_bytes(original[:-1] + b"x")
        with self.assertRaisesRegex(ValueError, "do not agree"):
            self.inspect()
        path.write_bytes(original)
        (self.artifacts / "fs_config.txt").write_text("/ 0 0 0777 capabilities=0\n")
        with self.assertRaisesRegex(ValueError, "ownership"):
            self.inspect()

    def test_changed_tool_or_failed_inode_inspection_prevents_product_mutation(self):
        before = self.product_tool.inventory(self.product)
        tool = self.aosp / "out/host/linux-x86/bin/debugfs"
        original = tool.read_bytes()
        tool.write_bytes(b"changed")
        with self.assertRaisesRegex(ValueError, "tools differ"):
            self.prepare()
        tool.write_bytes(original)
        self.image_check.side_effect = ValueError("wrong inode owner")
        with self.assertRaisesRegex(ValueError, "wrong inode owner"):
            self.prepare()
        self.assertEqual(before, self.product_tool.inventory(self.product))
        self.assertFalse(self.receipt.exists())
        self.image_check.side_effect = lambda *args: tool.write_bytes(b"changed during inspection")
        with self.assertRaisesRegex(ValueError, "tools changed during"):
            self.prepare()
        self.assertEqual(before, self.product_tool.inventory(self.product))

    def test_input_links_extras_and_symlinked_receipt_ancestors_are_refused(self):
        image = self.artifacts / "runtime-base.ext4"
        original = image.read_bytes()
        outside = self.root / "external"
        outside.write_bytes(original)
        image.unlink()
        image.symlink_to(outside)
        with self.assertRaises(ValueError): self.prepare()
        image.unlink()
        image.write_bytes(original)
        extra = self.artifacts / "unexpected"
        extra.touch()
        with self.assertRaises(ValueError): self.prepare()
        extra.unlink()
        output = self.root / "outside-output"
        output.mkdir()
        self.receipt.parent.symlink_to(output)
        with self.assertRaises(ValueError): self.prepare()
        self.assertEqual([], list(output.iterdir()))

    def test_interrupted_staging_preserves_previous_product_and_no_success_receipt(self):
        before = self.product_tool.inventory(self.product)
        with patch.object(integration.shutil, "copyfile", side_effect=OSError("interrupted")):
            with self.assertRaises(OSError): self.prepare()
        self.assertEqual(before, self.product_tool.inventory(self.product))
        self.assertFalse(self.receipt.exists())
        self.assertEqual([], list((self.aosp / "out/aegis-runtime-staging").iterdir()))

    def test_normal_registration_removes_selection_but_preserves_prior_assets(self):
        bundle = self.prepare()
        self.product_tool.register(self.project / "device/aegis/qemu_arm64", self.aosp)
        self.assertFalse((self.product / integration.INCLUDE).exists())
        self.assertTrue((self.aosp / "device/aegis/runtime-bases" / bundle).is_dir())
        self.receipt = self.root / "second-run/runtime-base-inputs.json"
        self.assertEqual(bundle, self.prepare())

    def test_changed_staged_base_or_product_is_not_silently_replaced(self):
        bundle = self.prepare()
        path = self.aosp / "device/aegis/runtime-bases" / bundle / "runtime-base.ext4"
        path.write_bytes(b"preserve my change")
        self.product_tool.register(self.project / "device/aegis/qemu_arm64", self.aosp)
        self.receipt = self.root / "second-run/runtime-base-inputs.json"
        with self.assertRaisesRegex(ValueError, "Previously staged"):
            self.prepare()
        self.assertEqual(b"preserve my change", path.read_bytes())
        local = self.product / "local-note.txt"
        local.write_text("preserve builder work")
        with self.assertRaisesRegex(ValueError, "Product sources changed outside"):
            self.prepare()
        self.assertEqual("preserve builder work", local.read_text())

    def test_product_staging_must_match_selected_bytes_and_never_reuse_unselected_assets(self):
        self.prepare()
        output = self.root / "product-out"
        target = output / "system_ext/etc/aegis/runtime"
        target.mkdir(parents=True)
        for origin, installed in (("runtime-base.ext4", "base.ext4"), ("generation.json", "generation.json")):
            shutil.copyfile(self.artifacts / origin, target / installed)
        integration.verify_installed(self.receipt, output, "system_ext")
        with self.assertRaisesRegex(ValueError, "Unselected"):
            integration.verify_absent(output, "system_ext")
        (target / "base.ext4").write_bytes(b"wrong build image")
        with self.assertRaisesRegex(ValueError, "differs"):
            integration.verify_installed(self.receipt, output, "system_ext")
        with self.assertRaises(ValueError):
            integration.verify_installed(self.receipt, output, "../system_ext")


if __name__ == "__main__":
    unittest.main()
