#!/usr/bin/env python3
"""Require the builder's real Linux RAM before starting AOSP graph generation."""
import argparse
from pathlib import Path
import re
import sys

# Match the existing build.sh floor for a provisioned 64 GB-class machine.
# MemTotal excludes some reserved memory. Swap does not satisfy either floor.
MIN_TOTAL_KIB = 60 * 1024 * 1024
MIN_AVAILABLE_KIB = 48 * 1024 * 1024


def parse_meminfo(text):
    values = {}
    for line in text.splitlines():
        key, separator, remainder = line.partition(':')
        if key not in ('MemTotal', 'MemAvailable'):
            continue
        if key in values or not separator:
            raise ValueError('Missing or duplicate Linux RAM measurement')
        match = re.fullmatch(r'\s*([0-9]+)\s+kB\s*', remainder)
        if not match:
            raise ValueError('Malformed Linux RAM measurement')
        values[key] = int(match.group(1))
    if (set(values) != {'MemTotal', 'MemAvailable'} or values['MemTotal'] <= 0
            or values['MemAvailable'] > values['MemTotal']):
        raise ValueError('Incomplete or inconsistent Linux RAM measurement')
    return values


def check(text):
    values = parse_meminfo(text)
    total, available = values['MemTotal'], values['MemAvailable']
    measured = f'{total / 1048576:.1f} GiB RAM, {available / 1048576:.1f} GiB available'
    if total < MIN_TOTAL_KIB or available < MIN_AVAILABLE_KIB:
        raise RuntimeError(f'{measured}; need at least 60 GiB total and 48 GiB available. '
                           'Provision a 64 GB-class builder VM and verify its guest RAM. '
                           'Swap is not counted. No AOSP compilation was started.')
    return 'AOSP memory preflight OK: ' + measured


if __name__ == '__main__':
    argparse.ArgumentParser(description=__doc__).parse_args()
    try:
        print(check(Path('/proc/meminfo').read_text()))
    except (OSError, ValueError, RuntimeError) as error:
        print(f'AOSP memory preflight failed: {error}', file=sys.stderr)
        sys.exit(1)
