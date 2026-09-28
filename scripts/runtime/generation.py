#!/usr/bin/env python3
"""Plan a shared Debian generation; materialize/build it only on aegis-build."""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import platform
import pwd
import re
import shutil
import struct
import subprocess
import sys
import tarfile
import tempfile
import uuid

import base
import uid_layout

PROJECT = Path(__file__).resolve().parents[2]
EPOCH = 1789689600  # Fixed recipe time, 2026-09-18 UTC, not the build clock.
IMAGE_BYTES = 256 * 1024 * 1024
IMAGE_INODES = 8192
TOOLS = ("mkuserimg_mke2fs", "mke2fs", "e2fsdroid", "e2fsck", "debugfs")
RECIPE_SOURCES = ("scripts/runtime/generation.py", "scripts/runtime/base.py",
                  "scripts/runtime/uid_layout.py", "scripts/runtime/build-base.sh", "runtime/uid-map.json",
                  "runtime/filesystem-tools.json")
SAFE_PATH = re.compile(r"[A-Za-z0-9_./+@%:=\[\]-]+")
ACCOUNT_FILES = ("etc/passwd", "etc/shadow", "etc/group", "etc/gshadow")
PRIVATE_MOUNTS = ("dev", "proc", "sys", "tmp", "run", "home")


def encoded(value):
    return (json.dumps(value, sort_keys=True, separators=(",", ":")) + "\n").encode()


def sha_file(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def safe_path(name):
    # Also safe as one canned-fs-config token and one debugfs path argument.
    if name != "." and (base.member_name(name) != name or not SAFE_PATH.fullmatch(name)):
        raise ValueError("Generation path cannot be represented safely")
    return name


def directory(name, uid=0, gid=0, mode=0o755):
    return {"path": name, "kind": "directory", "uid": uid, "gid": gid, "mode": mode}


def text_entry(name, text, previous=None):
    old = previous or {"uid": 0, "gid": 0, "mode": 0o644}
    data = text.encode()
    return {"path": name, "kind": "file", "uid": old["uid"], "gid": old["gid"],
            "mode": old["mode"], "size": len(data), "sha256": hashlib.sha256(data).hexdigest(),
            "text": text}


def validate_entries(entries):
    if entries.get(".") != directory(".") or len(entries) >= base.MAX_MEMBERS:
        raise ValueError("Invalid generation root or entry count")
    for name, entry in entries.items():
        safe_path(name)
        if entry["path"] != name or entry["kind"] not in {"directory", "file", "symlink", "hardlink"}:
            raise ValueError("Invalid generation entry")
        if any(type(entry[key]) is not int or not 0 <= entry[key] <= 65534 for key in ("uid", "gid")):
            raise ValueError("Invalid generation ownership")
        if type(entry["mode"]) is not int or not 0 <= entry["mode"] <= 0o1777:
            raise ValueError("Generation cannot contain set-ID bits")
        for parent in PurePosixPath(name).parents:
            if entries.get(str(parent), {}).get("kind") != "directory":
                raise ValueError("Generation ancestor is absent or not a directory")
        if entry["kind"] == "file":
            if (type(entry["size"]) is not int or not 0 <= entry["size"] <= base.MAX_EXPANDED
                    or not re.fullmatch(r"[0-9a-f]{64}", entry["sha256"])):
                raise ValueError("Invalid generation file metadata")
            if "text" in entry:
                data = entry["text"].encode()
                if len(data) != entry["size"] or hashlib.sha256(data).hexdigest() != entry["sha256"]:
                    raise ValueError("Generated text differs from its metadata")
        if entry["kind"] == "hardlink":
            target = entries.get(safe_path(entry["target"]), {})
            if target.get("kind") != "file" or any(entry[k] != target[k] for k in ("uid", "gid", "mode")):
                raise ValueError("Hardlink has incompatible target or ownership")
        if entry["kind"] == "symlink":
            target = entry["target"]
            # All links in the pinned base fit ext4's fast-symlink representation.
            # Require review before accepting a new representation/quoted target.
            if not SAFE_PATH.fullmatch(target) or len(target.encode()) >= 60:
                raise ValueError("Unsupported symbolic link representation")


def make_plan(pin, imported, layout, project=PROJECT):
    report = base.verify(pin, imported)
    uid_layout.check_base(layout, report)
    entries = {".": directory(".")}
    texts = {}
    with tarfile.open(Path(imported) / "rootfs.tar.gz", "r:gz") as tar:
        for member in tar:
            if member.name in (".", "./") and member.isdir():
                continue
            name = safe_path(base.member_name(member.name))
            # Reject hidden ACL/capability metadata rather than copying host xattrs.
            if member.pax_headers:
                raise ValueError("Review a base with extended TAR metadata before image assembly")
            entry = {"path": name, "uid": member.uid, "gid": member.gid,
                     "mode": member.mode & 0o1777, "source": member.name}
            if member.isdir():
                entry["kind"] = "directory"
            elif member.isfile():
                entry.update(kind="file", size=member.size)
                h = hashlib.sha256()
                chunks = [] if name in ACCOUNT_FILES + ("etc/nsswitch.conf",) else None
                if chunks is not None and member.size > base.MAX_METADATA:
                    raise ValueError("Oversized generation account metadata")
                with tar.extractfile(member) as stream:
                    for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                        h.update(chunk)
                        if chunks is not None:
                            chunks.append(chunk)
                entry["sha256"] = h.hexdigest()
                if chunks is not None:
                    texts[name] = b"".join(chunks).decode()
            elif member.issym():
                entry.update(kind="symlink", target=member.linkname, mode=0o777)
            elif member.islnk():
                entry.update(kind="hardlink", target=base.member_name(member.linkname))
            else:
                raise ValueError("Special files cannot enter the shared base")
            if any(name.startswith(path + "/") for path in PRIVATE_MOUNTS) and not member.isdir():
                raise ValueError("Base contains state beneath a private mount point")
            entries[name] = entry

    for entry in list(entries.values()):
        for parent in PurePosixPath(entry["path"]).parents:
            entries.setdefault(str(parent), directory(str(parent)))
    if "runtime" in report["technical_users"] or "runtime" in report["technical_groups"] \
            or 1000 in report["technical_groups"].values():
        raise ValueError("The generic runtime account conflicts with upstream metadata")
    for key in ("passwd", "group", "shadow", "gshadow"):
        matches = re.findall(r"^" + key + r":\s*([^\n#]+)", texts["etc/nsswitch.conf"], re.M)
        if len(matches) != 1 or matches[0].strip() != "files":
            raise ValueError("Personal identity must not be delegated to another NSS authority")
    additions = {
        "etc/passwd": "runtime:x:1000:1000:AEGIS runtime:/home/user:/bin/bash\n",
        "etc/shadow": "runtime:!:0:0:99999:7:::\n",
        "etc/group": "runtime:x:1000:\n",
        "etc/gshadow": "runtime:!::\n",
    }
    for name, row in additions.items():
        entries[name] = text_entry(name, texts[name].rstrip("\n") + "\n" + row, entries[name])
    for name, text in {"etc/hostname": "aegis\n", "etc/hosts": "127.0.0.1 localhost\n::1 localhost\n",
                       "etc/resolv.conf": "", "etc/machine-id": ""}.items():
        if name in entries and entries[name]["kind"] != "file":
            raise ValueError("Unexpected link at a fixed runtime configuration path")
        entries[name] = text_entry(name, text)
    if "var/lib/dbus/machine-id" in entries:
        raise ValueError("Review base machine identity before sharing it")
    for name in PRIVATE_MOUNTS + ("dev/pts", "run/user", "home/user", "run/user/1000", "lost+found"):
        if name in entries and entries[name]["kind"] != "directory":
            raise ValueError("Mount placeholder is not a directory")
        if name in {"home/user", "run/user/1000"}:
            entries[name] = directory(name, 1000, 1000, 0o700)
        elif name == "lost+found":
            entries[name] = directory(name, mode=0o700)
        else:
            entries.setdefault(name, directory(name, mode=0o1777 if name == "tmp" else 0o755))
    if any(name.startswith("lost+found/") for name in entries):
        raise ValueError("Base cannot contain recovered filesystem data")
    validate_entries(entries)
    allowed = {row["inside"] + i for row in uid_layout.validate(layout) for i in range(row["count"])}
    if any(entry[key] not in allowed for entry in entries.values() for key in ("uid", "gid")):
        raise ValueError("Generation contains an unmapped owner")
    sources = {name: sha_file(Path(project) / name) for name in RECIPE_SOURCES}
    plan = {"schema": 1, "status": "PLANNED_NOT_BUILT", "upstream_pin": pin,
            "recipe_sources": sources, "epoch": EPOCH, "image_bytes": IMAGE_BYTES,
            "image_inodes": IMAGE_INODES, "packages": report["packages"],
            "removed_setid_paths": report["setid_paths"],
            "entries": [entries[name] for name in sorted(entries)]}
    # Verify again so a changed import cannot be accepted between the two reads.
    if base.verify(pin, imported) != report:
        raise ValueError("Base import changed while planning")
    plan["plan_sha256"] = hashlib.sha256(encoded(plan)).hexdigest()
    return plan


def validate_plan(plan):
    payload = dict(plan)
    digest = payload.pop("plan_sha256")
    if hashlib.sha256(encoded(payload)).hexdigest() != digest:
        raise ValueError("Generation plan changed")
    entries = {entry["path"]: entry for entry in plan["entries"]}
    if len(entries) != len(plan["entries"]):
        raise ValueError("Duplicate generation entries")
    validate_entries(entries)
    return entries


def materialize(plan, archive, destination):
    """Write an inert, unprivileged staging tree; never chmod executable or chown.

    Production calls are builder-gated below. Host tests use inert file fixtures.
    """
    entries = validate_plan(plan)
    destination = Path(destination).absolute()
    if any(p.is_symlink() for p in [destination, *destination.parents]):
        raise ValueError("Staging path must not traverse symlinks")
    destination.mkdir(mode=0o700, exist_ok=False)
    try:
        for name in sorted(entries, key=lambda n: (len(PurePosixPath(n).parts), n)):
            if name != "." and entries[name]["kind"] == "directory":
                (destination / name).mkdir(mode=0o700)
        with tarfile.open(archive, "r:gz") as tar:
            for name in sorted(entries):
                entry = entries[name]
                if entry["kind"] != "file":
                    continue
                path = destination / name
                fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
                with os.fdopen(fd, "wb") as output:
                    if "text" in entry:
                        output.write(entry["text"].encode())
                    else:
                        member = tar.getmember(entry["source"])
                        if not member.isfile() or member.size != entry["size"]:
                            raise ValueError("Archive changed during generation assembly")
                        with tar.extractfile(member) as stream:
                            shutil.copyfileobj(stream, output, 1024 * 1024)
                if path.stat().st_size != entry["size"] or sha_file(path) != entry["sha256"]:
                    raise ValueError("Assembled file differs from checked generation input")
        # Every parent is already an actual directory; create links last, never
        # use extractall, follow a source link or apply source ownership on host.
        for name in sorted(entries):
            entry = entries[name]
            if entry["kind"] == "hardlink":
                os.link(destination / entry["target"], destination / name, follow_symlinks=False)
            elif entry["kind"] == "symlink":
                os.symlink(entry["target"], destination / name)
        for name in sorted(entries, reverse=True):
            os.utime(destination / name, (plan["epoch"], plan["epoch"]), follow_symlinks=False)
    except BaseException:
        shutil.rmtree(destination)  # Only our freshly-created private staging tree.
        raise


def fs_config(plan):
    entries = validate_plan(plan)
    return "".join(f"{'/' if name == '.' else name} {e['uid']} {e['gid']} {e['mode']:04o} capabilities=0\n"
                   for name, e in sorted(entries.items()))


def check_tools_sources(aosp, project=PROJECT):
    reference = base.read_json((Path(project) / "runtime/filesystem-tools.json").read_bytes())
    expected = {"system/extras/ext4_utils/mkuserimg_mke2fs.py",
                "system/extras/ext4_utils/mke2fs.conf", "system/core/libcutils/canned_fs_config.cpp"}
    if reference["aosp_tag"] != "android-16.0.0_r1" or set(reference["sha256"]) != expected:
        raise ValueError("Unexpected filesystem-tool source baseline")
    for name, digest in reference["sha256"].items():
        if sha_file(Path(aosp) / name) != digest:
            raise ValueError("AOSP filesystem source changed; review the recipe before building")


def require_builder():
    if (platform.system() != "Linux" or platform.machine() != "x86_64" or os.getuid() == 0
            or pwd.getpwuid(os.getuid()).pw_name != "aegis-build"):
        raise ValueError("Build filesystem images only as aegis-build on the SSH builder")


def check_recipe(plan, project=PROJECT):
    if plan["recipe_sources"] != {name: sha_file(Path(project) / name) for name in RECIPE_SOURCES}:
        raise ValueError("Generation recipe changed after planning")


def build(plan, imported, aosp, output):
    require_builder()
    check_tools_sources(aosp)
    validate_plan(plan)
    check_recipe(plan)
    output = Path(output).absolute()
    if output.exists() or any(p.is_symlink() for p in [output, *output.parents]):
        raise ValueError("Image output must be a new directory without symlink ancestors")
    output.parent.mkdir(parents=True, exist_ok=True)
    staged = Path(tempfile.mkdtemp(prefix=".runtime-image-", dir=output.parent))
    try:
        tool_dir = Path(aosp) / "out/host/linux-x86/bin"
        tools = {}
        for name in TOOLS:
            path = tool_dir / name
            if (not path.is_file() or not os.access(path, os.X_OK)
                    or (Path(aosp) / "out").resolve() not in path.resolve().parents):
                raise ValueError("Build the required AOSP filesystem tools first")
            tools[name] = sha_file(path)
        input_id = hashlib.sha256(encoded({"plan": plan["plan_sha256"], "tools": tools})).hexdigest()
        image_uuid = str(uuid.UUID(input_id[:32]))
        materialize(plan, Path(imported) / "rootfs.tar.gz", staged / "root")
        config = staged / "fs_config.txt"
        config.write_text(fs_config(plan))
        env = {"PATH": str(tool_dir) + ":/usr/bin:/bin", "LANG": "C", "LC_ALL": "C", "TZ": "UTC",
               "HOME": str(staged), "TMPDIR": str(staged), "E2FSPROGS_FAKE_TIME": str(plan["epoch"])}
        # Build twice from the same checked tree. No distro binary or maintainer
        # script is executed; only the explicitly built AOSP host tools run.
        for filename in ("runtime-base.ext4", "repeat.ext4"):
            command = [str(tool_dir / "mkuserimg_mke2fs"), str(staged / "root"),
                       str(staged / filename), "ext4", "/", str(plan["image_bytes"]),
                       "-j", "0", "-T", str(plan["epoch"]), "-C", str(config),
                       "-L", "aegis-linux", "-i", str(plan["image_inodes"]), "-I", "256",
                       "-M", "0", "-U", image_uuid, "-S", image_uuid]
            subprocess.run(command, env=env, check=True, timeout=300)
        image = staged / "runtime-base.ext4"
        for filename in ("runtime-base.ext4", "repeat.ext4"):
            path = staged / filename
            if path.is_symlink() or not path.is_file() or path.stat().st_size != plan["image_bytes"]:
                raise ValueError("Filesystem tool did not produce the requested regular raw image")
        if sha_file(image) != sha_file(staged / "repeat.ext4"):
            raise ValueError("Repeated filesystem construction produced different bytes")
        subprocess.run([str(tool_dir / "e2fsck"), "-f", "-n", str(image)],
                       env=env, check=True, timeout=180)
        verify_superblock(image, plan["image_bytes"], image_uuid)
        verify_image(plan, image, tool_dir / "debugfs", env)
        if base.verify(plan["upstream_pin"], imported)["status"] != "VERIFIED_UPSTREAM_BASE_NOT_INSTALLED":
            raise ValueError("Base import changed during image construction")
        check_tools_sources(aosp)
        check_recipe(plan)
        if tools != {name: sha_file(tool_dir / name) for name in TOOLS}:
            raise ValueError("Filesystem tools changed during generation assembly")
        image_sha = sha_file(image)
        receipt = {"schema": 1, "status": "BUILT_VERIFIED_NOT_MOUNTED", "generation": image_sha,
                   "recipe_input_sha256": input_id,
                   "plan_sha256": plan["plan_sha256"], "tool_sha256": tools, "uuid": image_uuid,
                   "image_sha256": image_sha, "image_bytes": image.stat().st_size,
                   "repeated_build_identical": True,
                   "scope": "Image contents/owners and repeat build only; no guest, mount, authorization or isolation test"}
        (staged / "plan.json").write_bytes(encoded(plan))
        (staged / "generation.json").write_bytes(encoded(receipt))
        shutil.rmtree(staged / "root")
        (staged / "repeat.ext4").unlink()
        if {p.name for p in staged.iterdir()} != {"runtime-base.ext4", "fs_config.txt", "plan.json", "generation.json"}:
            raise ValueError("Unexpected files appeared in the generation output")
        image.chmod(0o444)
        if output.exists() or output.is_symlink():
            raise ValueError("Generation output appeared during build; preserving it")
        staged.rename(output)
        return receipt
    finally:
        if staged.exists():
            shutil.rmtree(staged)


def verify_superblock(image, size, image_uuid):
    with Path(image).open("rb") as stream:
        stream.seek(1024)
        sb = stream.read(1024)
    if (len(sb) != 1024 or struct.unpack_from("<H", sb, 56)[0] != 0xEF53
            or struct.unpack_from("<I", sb, 24)[0] != 2
            or struct.unpack_from("<I", sb, 4)[0] * 4096 != size
            or sb[104:120] != uuid.UUID(image_uuid).bytes
            or struct.unpack_from("<I", sb, 92)[0] & 0x4
            or not struct.unpack_from("<I", sb, 96)[0] & 0x40):
        raise ValueError("Expected a raw, 4 KiB ext4 image with the recipe UUID and no journal")


def verify_image(plan, image, debugfs, env):
    # This check is not a mount or OS test: debugfs reads an ordinary image file.
    entries = validate_plan(plan)
    inodes = {}
    children = {name: set() for name, e in entries.items() if e["kind"] == "directory"}
    for name in entries:
        if name != ".":
            children[str(PurePosixPath(name).parent)].add(PurePosixPath(name).name)
    for name, entry in entries.items():
        path = "/" if name == "." else "/" + safe_path(name)
        result = subprocess.run([str(debugfs), "-R", "stat " + path, str(image)],
                                env=env, check=True, capture_output=True, timeout=15)
        text = result.stdout.decode()
        owner = re.search(r"User:\s*(\d+)\s+Group:\s*(\d+)", text)
        mode = re.search(r"Type:\s*(\w+)\s+Mode:\s*([0-7]+)", text)
        inode = re.search(r"\bInode:\s*(\d+)", text)
        expected_type = {"file": "regular", "hardlink": "regular", "directory": "directory", "symlink": "symlink"}[entry["kind"]]
        if (not inode or not owner or not mode or tuple(map(int, owner.groups())) != (entry["uid"], entry["gid"])
                or mode[1] != expected_type or int(mode[2], 8) != entry["mode"]):
            raise ValueError("Filesystem inode differs from generation metadata: " + name)
        inodes[name] = int(inode[1])
        if entry["kind"] == "file":
            result = subprocess.run([str(debugfs), "-R", "cat " + path, str(image)],
                                    env=env, check=True, capture_output=True, timeout=30)
            if len(result.stdout) != entry["size"] or hashlib.sha256(result.stdout).hexdigest() != entry["sha256"]:
                raise ValueError("Filesystem file differs from generation content: " + name)
        elif entry["kind"] == "symlink":
            target = re.search(r'^Fast link dest: "([^"\n]*)"$', text, re.M)
            if not target or target[1] != entry["target"]:
                raise ValueError("Filesystem symlink differs from generation metadata: " + name)
        elif entry["kind"] == "directory":
            result = subprocess.run([str(debugfs), "-R", "ls -p " + path, str(image)],
                                    env=env, check=True, capture_output=True, timeout=15)
            found = set()
            for line in result.stdout.decode().splitlines():
                if not line.strip():
                    continue
                fields = line.split("/")
                if len(fields) != 8 or fields[0] or fields[-1] or not fields[1].isdigit():
                    raise ValueError("Unexpected debugfs directory format")
                if fields[5] not in {".", ".."} and int(fields[1]) != 0:
                    if fields[5] in found:
                        raise ValueError("Duplicate filesystem directory entry")
                    found.add(fields[5])
            if found != children[name]:
                raise ValueError("Filesystem directory differs from generation metadata: " + name)
    for name, entry in entries.items():
        if entry["kind"] == "hardlink" and inodes[name] != inodes[entry["target"]]:
            raise ValueError("Filesystem lost a shared inode: " + name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("plan", "build", "check-tools"))
    parser.add_argument("--imported", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--aosp", type=Path, default=Path("/srv/aegis/work/aosp"))
    args = parser.parse_args()
    if args.action == "check-tools":
        check_tools_sources(args.aosp)
        print("Pinned filesystem-tool sources match; no image built.")
        return
    if not args.imported:
        parser.error("--imported is required")
    if args.action == "build":
        require_builder()
        if not args.output:
            parser.error("--output is required")
    pin = base.read_json((PROJECT / "runtime/debian-arm64.json").read_bytes())
    layout = base.read_json((PROJECT / "runtime/uid-map.json").read_bytes())
    plan = make_plan(pin, args.imported, layout)
    if args.action == "plan":
        if args.output:
            with args.output.open("xb") as target:
                target.write(encoded(plan))
        print(json.dumps({"status": plan["status"], "plan_sha256": plan["plan_sha256"],
                          "entries": len(plan["entries"]), "packages": len(plan["packages"])}))
    else:
        print(json.dumps(build(plan, args.imported, args.aosp, args.output)))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, TypeError, tarfile.TarError,
            subprocess.SubprocessError) as error:
        print(f"Runtime generation NOT built: {error}", file=sys.stderr)
        sys.exit(1)
