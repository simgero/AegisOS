#!/usr/bin/env python3
"""Use a running AegisOS development VM's private local QMP socket."""
import argparse
import json
from pathlib import Path
import socket


def execute(path, command, arguments=None, timeout=10):
    with socket.socket(socket.AF_UNIX) as connection:
        connection.settimeout(timeout)
        connection.connect(str(path))
        with connection.makefile('rwb') as stream:
            greeting = json.loads(stream.readline())
            if 'QMP' not in greeting:
                raise RuntimeError('Expected QMP greeting')
            result = None
            for number, (name, args) in enumerate([
                ('qmp_capabilities', {}), (command, arguments or {})
            ]):
                stream.write((json.dumps({'execute': name, 'arguments': args, 'id': number})+'\n').encode())
                stream.flush()
                while True:
                    line = stream.readline()
                    if not line:
                        raise RuntimeError('QEMU closed its control socket')
                    reply = json.loads(line)
                    if reply.get('id') != number:
                        continue
                    if 'error' in reply:
                        raise RuntimeError(str(reply['error']))
                    result = reply['return']
                    break
            return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run', type=Path)
    parser.add_argument('command')
    parser.add_argument('--arguments', type=json.loads, default={})
    args = parser.parse_args()
    path = (args.run/'qmp-path.txt').read_text().strip()
    print(json.dumps(execute(path, args.command, args.arguments), indent=2))


if __name__ == '__main__':
    main()
