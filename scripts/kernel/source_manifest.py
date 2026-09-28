#!/usr/bin/env python3
"""Validate immutable kernel source pins; optionally check a synced repo tree."""
import argparse
from pathlib import Path, PurePosixPath
import re
import subprocess
import xml.etree.ElementTree as ET


def projects(manifest):
    root = ET.parse(manifest).getroot()
    if root.tag != 'manifest' or root.find('superproject') is not None:
        raise ValueError('Expected a pinned manifest without a floating superproject')
    # Includes or extend-project directives could silently override checked pins.
    if any(child.tag not in {'remote', 'default', 'project'} for child in root):
        raise ValueError('Unexpected manifest directive')
    remotes = {node.get('name'): node.get('fetch') for node in root.findall('remote')}
    default = root.find('default')
    if default is None or default.get('revision') is not None:
        raise ValueError('Do not inherit a default source revision')
    result = {}
    for node in root.findall('project'):
        path = node.get('path', node.get('name', ''))
        revision = node.get('revision', '')
        if (not path or PurePosixPath(path).is_absolute() or
                any(part in {'', '.', '..'} for part in path.split('/'))):
            raise ValueError(f'Unsafe project path: {path}')
        if path in result or not re.fullmatch(r'[0-9a-f]{40}', revision):
            raise ValueError(f'Duplicate or unpinned project: {path}')
        remote = node.get('remote', default.get('remote'))
        if remotes.get(remote) != 'https://android.googlesource.com/':
            raise ValueError(f'Unexpected upstream for {path}')
        result[path] = revision
    if not {'common', 'common-modules/virtual-device', 'build/kernel'} <= result.keys():
        raise ValueError('Missing kernel, virtual-device modules or build tools')
    return result


def check_tree(manifest, workspace):
    workspace = Path(workspace).resolve(strict=True)
    for name, revision in projects(manifest).items():
        source = workspace / name
        resolved = source.resolve(strict=True)
        if resolved != source or not source.is_dir():
            raise ValueError(f'Unexpected source directory: {name}')

        def git(*args):
            return subprocess.check_output(['git', '-C', str(source), *args], text=True).strip()

        if Path(git('rev-parse', '--show-toplevel')).resolve() != source:
            raise ValueError(f'Project is not its own Git checkout: {name}')
        if git('rev-parse', 'HEAD') != revision:
            raise ValueError(f'Source revision differs from manifest: {name}')
        if git('status', '--porcelain', '--untracked-files=all'):
            raise ValueError(f'Source has local changes: {name}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('manifest', type=Path)
    parser.add_argument('--workspace', type=Path)
    args = parser.parse_args()
    count = len(projects(args.manifest))
    if args.workspace:
        check_tree(args.manifest, args.workspace)
    print(f'Validated {count} pinned kernel projects' + (' and clean checkouts' if args.workspace else ''))
