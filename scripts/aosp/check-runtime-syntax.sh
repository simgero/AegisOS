#!/bin/bash
# Read existing ARM64 compiler rules; write only a separate diagnostic run.
# No source registration, AOSP output mutation, linking or guest execution.
set -euo pipefail
[[ $(uname -s) == Linux && $(uname -m) == x86_64 && $(id -un) == aegis-build ]]
commit=${1:?Usage: check-runtime-syntax.sh FULL_PROJECT_COMMIT}
[[ $# == 1 && "$commit" =~ ^[0-9a-f]{40}$ ]]
project=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
[[ $(git -C "$project" rev-parse HEAD) == "$commit" ]]
[[ -z $(git -C "$project" status --porcelain --untracked-files=all) ]]
unset GH_TOKEN GITHUB_TOKEN CREDENTIALS_DIRECTORY
run=$(mktemp -d "/srv/aegis/runs/runtime-syntax-$(date -u +%Y%m%dT%H%M%SZ)-${commit:0:8}-XXXXXX")
exec > >(tee -a "$run/check.log") 2>&1
trap 'result=$?; if (( result != 0 )); then printf "FAILED\n" > "$run/status"; fi' EXIT
printf 'CHECKING\n' > "$run/status"
printf '%s\n' "$commit" > "$run/project-commit.txt"
python3 - "$project" "$run" <<'PY'
import json
from pathlib import Path
import shlex
import subprocess
import sys

project, run = map(Path, sys.argv[1:])
aosp = Path('/srv/aegis/work/aosp')
base = 'out/soong/.intermediates/packages/aegis/identity/'
c = base + 'libaegis-runtime-context/android_arm64_armv8-a_cortex-a53_static_lto-none/obj/packages/aegis/identity/runtime/context.o'
cpp = base + 'AegisRuntimeNativeTests/android_arm64_armv8-a_cortex-a53/obj/packages/aegis/identity/runtime/setup_tests.o'
result = subprocess.run(['prebuilts/build-tools/linux-x86/bin/ninja', '-f',
        'out/combined-aegis_qemu_arm64.ninja', '-t', 'commands', c, cpp],
        cwd=aosp, check=True, capture_output=True, text=True, timeout=90)
rules = {}
for line in result.stdout.splitlines():
    args = shlex.split(line)
    for source in ('context.c', 'setup_tests.cpp'):
        if args and args[-1] == 'packages/aegis/identity/runtime/' + source:
            if source in rules:
                raise ValueError('Ambiguous compiler rule')
            compiler = 'clang++' if source.endswith('.cpp') else 'clang'
            expected = 'prebuilts/clang/host/linux-x86/clang-r547379/bin/' + compiler
            if args[:2] != ['PWD=/proc/self/cwd', expected] or '-c' not in args:
                raise ValueError('Unexpected compiler invocation')
            if any(arg in ('&&', ';', '|', '>', '<') for arg in args):
                raise ValueError('Compound compiler invocation refused')
            if args[args.index('-target') + 1] != 'aarch64-linux-android10000':
                raise ValueError('Unexpected target')
            selected = [args[1]]
            index = 2
            while index < len(args) - 1:
                arg = args[index]
                if arg in ('-o', '-MF'):
                    index += 2
                    continue
                if arg in ('-MD', '-MMD', '-c'):
                    index += 1
                    continue
                if any(arg.startswith(prefix) for prefix in
                       ('@', '-save-temps', '-ftime-trace', '-serialize-diagnostics', '-MJ')):
                    raise ValueError('Unexpected output or response-file option')
                if arg.startswith('-Ipackages/aegis/identity'):
                    arg = '-I' + str(project / arg[2:])
                selected.append(arg)
                index += 1
            selected += ['-fsyntax-only', '-Werror']
            rules[source] = selected
if set(rules) != {'context.c', 'setup_tests.cpp'}:
    raise ValueError('Expected both existing ARM64 compiler rules')
commands = {}
for source, template in (('exec.c', 'context.c'), ('context.c', 'context.c'),
                         ('exec_tests.cpp', 'setup_tests.cpp')):
    commands[source] = rules[template] + [str(project / 'packages/aegis/identity/runtime' / source)]
(run / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
for source, args in commands.items():
    print('Checking ARM64 syntax:', source, flush=True)
    subprocess.run(args, cwd=aosp, check=True, timeout=120)
(run / 'status').write_text('SYNTAX_CHECKED_NOT_LINKED_OR_EXECUTED\n')
print('ARM64 syntax checked. No objects linked, no device tests run:', run, flush=True)
PY
