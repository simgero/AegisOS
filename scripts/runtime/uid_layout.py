#!/usr/bin/env python3
"""Generate/check the runtime ID layout; this never writes live uid_map/gid_map files."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path, PurePosixPath
import re
import sys

PROJECT = Path(__file__).resolve().parents[2]
CONFIG = "device/aegis/qemu_arm64/runtime-ids.fs"
JAVA = "packages/aegis/identity/src/org/aegisos/identity/RuntimeUidLayout.java"
C_HEADER = "packages/aegis/identity/runtime/uid_layout.h"
HEADER = "system/core/libcutils/include/private/android_filesystem_config.h"
PARSER = "build/make/tools/fs_config/fs_config_generator.py"
# android-16.0.0_r1 android_filesystem_config.h; checked against the real parser below.
RESERVED = {"VENDOR": [(2900, 2999), (5000, 5999)], "SYSTEM_EXT": [(7500, 7999)]}


def validate(layout):
    if (not isinstance(layout, dict) or set(layout) != {"schema", "per_user_range", "ranges"}
            or type(layout["schema"]) is not int or layout["schema"] != 1
            or type(layout["per_user_range"]) is not int or layout["per_user_range"] != 100000
            or not isinstance(layout["ranges"], list) or not 1 <= len(layout["ranges"]) <= 16):
        raise ValueError("Unsupported AOSP runtime ID layout")
    inside_ids, app_ids, names = set(), set(), set()
    for row in layout["ranges"]:
        if not isinstance(row, dict) or set(row) != {"inside", "app_id", "count", "aid"}:
            raise ValueError("Malformed ID range")
        if any(type(row[key]) is not int for key in ("inside", "app_id", "count")):
            raise ValueError("ID ranges require integers")
        start, app_id, count, name = row["inside"], row["app_id"], row["count"], row["aid"]
        if (not 0 <= start <= 65534 or not 0 < count <= 1000 or start + count > 65535
                or not isinstance(name, str) or not re.fullmatch(r"[A-Z][A-Z0-9_]+", name)):
            raise ValueError("Invalid namespace ID range or AID name")
        partition = next((p for p in RESERVED if name.startswith(p + "_")), None)
        if partition is None or not any(low <= app_id <= app_id + count - 1 <= high
                                        for low, high in RESERVED[partition]):
            raise ValueError("Runtime IDs must stay within their AOSP reserved partition range")
        for offset in range(count):
            inside, outside = start + offset, app_id + offset
            aid = name if count == 1 else f"{name}_{inside:04d}"
            if len(aid) > 31:
                raise ValueError("AOSP account names must be shorter than 32 characters")
            if inside in inside_ids or outside in app_ids or aid in names:
                raise ValueError("Overlapping namespace/Android IDs or duplicate AID names")
            inside_ids.add(inside)
            app_ids.add(outside)
            names.add(aid)
    if not {0, 1000, 65534} <= inside_ids:
        raise ValueError("Runtime root, normal user and nobody must have distinct mappings")
    return sorted(layout["ranges"], key=lambda row: row["inside"])


def generated(layout):
    rows = validate(layout)
    config = ["# Generated from runtime/uid-map.json by scripts/runtime/uid_layout.py.",
              "# Numeric AOSP resource identifiers, not personal users or password accounts.", ""]
    for row in rows:
        for offset in range(row["count"]):
            inside = row["inside"] + offset
            aid = row["aid"] if row["count"] == 1 else f"{row['aid']}_{inside:04d}"
            config += [f"[AID_{aid}]", f"value: {row['app_id'] + offset}", ""]
    triples = ",\n".join(f"            {{{r['inside']}, {r['app_id']}, {r['count']}}}" for r in rows)
    java = ("// Generated from runtime/uid-map.json by scripts/runtime/uid_layout.py.\n"
            "package org.aegisos.identity;\n\n"
            "/** Layout only; AOSP authentication and runtime lifecycle checks remain mandatory. */\n"
            "final class RuntimeUidLayout {\n"
            "    static final int PER_USER_RANGE = 100000;\n"
            "    private RuntimeUidLayout() {}\n\n"
            "    /** A fresh array prevents callers from changing the process-wide layout. */\n"
            "    static int[][] ranges() {\n"
            "        return new int[][] {\n" + triples + "\n        };\n    }\n}\n")
    c_rows = ",\n".join(f"    {{{r['inside']}u, {r['app_id']}u, {r['count']}u}}" for r in rows)
    native = ("// Generated from runtime/uid-map.json by scripts/runtime/uid_layout.py.\n"
              "#ifndef AEGIS_RUNTIME_UID_LAYOUT_H\n#define AEGIS_RUNTIME_UID_LAYOUT_H\n"
              "#include <stdint.h>\n"
              "#define AEGIS_PER_USER_RANGE 100000u\n"
              "struct aegis_uid_extent { uint32_t inside, app_id, count; };\n"
              "static const struct aegis_uid_extent aegis_uid_extents[] = {\n"
              + c_rows + "\n};\n#endif\n")
    return {CONFIG: "\n".join(config), JAVA: java, C_HEADER: native}


def check_generated(layout, project=PROJECT):
    for name, expected in generated(layout).items():
        path = Path(project) / name
        if path.is_symlink() or not path.is_file() or path.read_text() != expected:
            raise ValueError("Generated ID configuration differs from runtime/uid-map.json")


def check_aosp(layout, aosp, fs_configs, project=PROJECT):
    check_generated(layout, project)
    aosp = Path(aosp).resolve()
    reference = json.loads((Path(project) / "runtime/aosp-id-reference.json").read_bytes())
    if reference["aosp_tag"] != "android-16.0.0_r1" or set(reference["sha256"]) != {HEADER, PARSER}:
        raise ValueError("Unexpected AOSP ID reference")
    for name, digest in reference["sha256"].items():
        if hashlib.sha256((aosp / name).read_bytes()).hexdigest() != digest:
            raise ValueError("AOSP ID definitions/parser changed; review mapping before a build")
    if not fs_configs or CONFIG not in fs_configs or len(set(fs_configs)) != len(fs_configs):
        raise ValueError("The actual product must include the AEGIS ID registry exactly once")
    paths = []
    for name in fs_configs:
        path = PurePosixPath(name)
        if path.is_absolute() or path.as_posix() != name or ".." in path.parts:
            raise ValueError("Unexpected product fs_config path")
        resolved = (aosp / name).resolve()
        if aosp not in resolved.parents or not resolved.is_file():
            raise ValueError("Product fs_config is missing or outside AOSP")
        paths.append(str(resolved))
    if (aosp / CONFIG).read_text() != generated(layout)[CONFIG]:
        raise ValueError("Registered AOSP product IDs differ from the project")
    # Execute only the hash-pinned, official Python metadata parser. No compiler,
    # namespace, user, ownership or permission change occurs here.
    spec = importlib.util.spec_from_file_location("aegis_aosp_fs_config", aosp / PARSER)
    parser = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(parser)
    header = parser.AIDHeaderParser(str(aosp / HEADER))
    product = parser.FSConfigFileParser(paths, header.ranges)
    expected = {row["app_id"] + i for row in validate(layout) for i in range(row["count"])}
    actual = {int(aid.value, 0) for aid in product.aids}
    if not expected <= actual:
        raise ValueError("AOSP did not register every runtime ID")
    return len(expected)


def check_base(layout, report):
    allowed = {row["inside"] + i for row in validate(layout) for i in range(row["count"])}
    if report["status"] != "VERIFIED_UPSTREAM_BASE_NOT_INSTALLED":
        raise ValueError("A verified upstream base is required")
    required = set(report["file_uids"]) | set(report["file_gids"])
    for user in report["technical_users"].values():
        required.update((user["uid"], user["gid"]))
    required.update(report["technical_groups"].values())
    if not required <= allowed:
        raise ValueError("Base contains IDs without a reserved AEGIS mapping")
    return len(required)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("action", choices=["check", "write"])
    p.add_argument("--aosp", type=Path)
    p.add_argument("--fs-config", nargs="+")
    p.add_argument("--base", type=Path, help="Existing import; hashes/content are reverified")
    args = p.parse_args()
    layout = json.loads((PROJECT / "runtime/uid-map.json").read_bytes())
    if args.action == "write":
        if args.aosp or args.fs_config or args.base:
            p.error("write only updates generated project sources")
        for name, text in generated(layout).items():
            path = PROJECT / name
            path.parent.mkdir(parents=True, exist_ok=True)
            if path.is_symlink():
                raise ValueError("Refusing to overwrite a generated-file symlink")
            path.write_text(text)
    check_generated(layout)
    print("Generated runtime ID sources match their layout; no IDs assigned to a live process.")
    if bool(args.aosp) != bool(args.fs_config):
        p.error("--aosp and the complete product --fs-config list are required together")
    if args.aosp:
        print(f"AOSP parser accepted {check_aosp(layout, args.aosp, args.fs_config)} runtime IDs.")
    if args.base:
        import base
        pin = json.loads((PROJECT / "runtime/debian-arm64.json").read_bytes())
        report = base.verify(pin, args.base)
        print(f"Verified Debian base IDs covered: {check_base(layout, report)}. No namespace test.")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, TypeError, KeyError) as error:
        print(f"Runtime ID check failed: {error}", file=sys.stderr)
        sys.exit(1)
