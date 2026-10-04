#!/usr/bin/env python3
"""Build a public-fixture terminal probe for Android; never install or run it.

The probe uses the unchanged TerminalConsole and fixed-path TerminalNative API.
It contains no JNI library, account operations or credentials. On Android it
must load the installed /system_ext/lib64/libaegis_terminal_jni.so. This is a
separate local test artifact, never a product image or a substitute for one.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import zipfile

SOURCES = (
    "packages/aegis/identity/cli/org/aegisos/identity/TerminalConsole.java",
    "packages/aegis/identity/terminal/java/org/aegisos/identity/TerminalNative.java",
    "tests/fixtures/TerminalConsoleHostProbe.java",
)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build(project, jdk, r8, output):
    project, jdk, r8 = (path.resolve(strict=True) for path in (project, jdk, r8))
    output = output.absolute()
    output.mkdir(parents=True, exist_ok=False)
    status = output / "status"
    status.write_text("BUILDING\n")
    try:
        sources = output / "sources"
        sources.mkdir()
        bindings = []
        for name in SOURCES:
            data = (project / name).read_bytes()
            copied = sources / Path(name).name
            copied.write_bytes(data)
            bindings.append({"path": name, "sha256": sha(copied)})
        api = (sources / "TerminalNative.java").read_text()
        if api.count('System.load("/system_ext/lib64/libaegis_terminal_jni.so")') != 1:
            raise ValueError("Review the changed production JNI loader before building this probe")
        classes = output / "classes"
        classes.mkdir()
        commands = [
            [str(jdk / "bin/javac"), "-J-Xmx128m", "--release", "8", "-d", str(classes),
             *(str(sources / Path(name).name) for name in SOURCES)],
        ]
        with (output / "build.log").open("w") as log:
            subprocess.run(commands[0], check=True, stdout=log, stderr=subprocess.STDOUT,
                           stdin=subprocess.DEVNULL, timeout=60)
            class_files = sorted(classes.rglob("*.class"))
            expected = {"org/aegisos/identity/" + name + ".class" for name in
                        ("TerminalConsole", "TerminalNative", "TerminalConsoleHostProbe")}
            if {str(path.relative_to(classes)) for path in class_files} != expected:
                raise ValueError("Unexpected test class inventory")
            class_archive = output / "classes.jar"
            with zipfile.ZipFile(class_archive, "x") as archive:
                for path in class_files:
                    archive.writestr(str(path.relative_to(classes)), path.read_bytes())
            artifact = output / "terminal-input-probe.zip"
            commands.append([
                str(jdk / "bin/java"), "-Xmx256m", "-cp", str(r8),
                "com.android.tools.r8.D8", "--release", "--min-api", "35",
                "--lib", str(jdk), "--thread-count", "1", "--output", str(artifact),
                str(class_archive),
            ])
            subprocess.run(commands[1], check=True, stdout=log, stderr=subprocess.STDOUT,
                           stdin=subprocess.DEVNULL, timeout=60)
        with zipfile.ZipFile(artifact) as archive:
            if archive.namelist() != ["classes.dex"] or not archive.read("classes.dex").startswith(b"dex\n"):
                raise ValueError("Expected exactly one DEX and no bundled native library")
        receipt = {
            "status": "PUBLIC_TERMINAL_PROBE_BUILT_NOT_INSTALLED",
            "project": str(project), "sources": bindings,
            "artifact": artifact.name, "artifact_sha256": sha(artifact),
            "r8_sha256": sha(r8), "jdk_release_sha256": sha(jdk / "release"),
            "commands": commands,
            "runtime_library": "/system_ext/lib64/libaegis_terminal_jni.so",
            "limits": "Compilation only. Android execution and shipped-library binding remain required.",
        }
        (output / "result.json").write_text(json.dumps(receipt, indent=2) + "\n")
        status.write_text(receipt["status"] + "\n")
        return receipt
    except BaseException:
        status.write_text("FAILED\n")
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--jdk", type=Path, required=True)
    parser.add_argument("--r8", type=Path, required=True, help="Existing local AOSP r8.jar")
    parser.add_argument("--output", type=Path, required=True, help="New local evidence directory")
    args = parser.parse_args()
    try:
        receipt = build(args.project, args.jdk, args.r8, args.output)
    except (OSError, ValueError, subprocess.SubprocessError, zipfile.BadZipFile) as error:
        print("Terminal probe build failed: " + str(error), file=sys.stderr)
        return 1
    print(receipt["status"] + " " + receipt["artifact_sha256"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
