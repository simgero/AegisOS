#!/usr/bin/env python3
"""Small CLI for the AEGIS command API; credentials are read from a private file."""
import argparse
import base64
import json
import os
from pathlib import Path
import sys
import time
import urllib.error
import urllib.request
import uuid


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--url', default='http://100.122.101.48:8787')
    parser.add_argument('--password-file', default=str(Path.home() / '.config/aegis-build-http/password'))
    sub = parser.add_subparsers(dest='action', required=True)
    run = sub.add_parser('run')
    run.add_argument('command')
    run.add_argument('--cwd', default='/srv/aegis')
    run.add_argument('--timeout', type=float, default=3600)
    run.add_argument('--env', action='append', default=[], metavar='KEY=VALUE')
    run.add_argument('--stdin', action='store_true', help='Read job input from this CLI stdin')
    run.add_argument('--detach', action='store_true')
    run.add_argument('--request-id', default=None)
    for action in ('get', 'follow', 'cancel', 'delete'):
        sub.add_parser(action).add_argument('id')
    sub.add_parser('list')
    sub.add_parser('health')
    args = parser.parse_args()
    path = Path(args.password_file)
    if path.stat().st_mode & 0o077:
        parser.error('Password file must be accessible only by its owner (chmod 600)')
    password = path.read_text().strip()
    # Never send this administrator credential through an ambient HTTP proxy.
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))

    def request(method, endpoint, data=None):
        req = urllib.request.Request(args.url.rstrip('/') + endpoint,
            data=None if data is None else json.dumps(data).encode(), method=method,
            headers={'Authorization': 'Bearer ' + password, 'Content-Type': 'application/json'})
        try:
            with opener.open(req, timeout=40) as response: return json.load(response)
        except urllib.error.HTTPError as exc:
            raise SystemExit(f'HTTP {exc.code}: {exc.read().decode()}')

    if args.action == 'run':
        ident = args.request_id or uuid.uuid4().hex
        print('Job/request ID: ' + ident, file=sys.stderr)
        try: env = dict(item.split('=', 1) for item in args.env)
        except ValueError: parser.error('--env requires KEY=VALUE')
        data = request('POST', '/v1/jobs', dict(command=args.command, cwd=args.cwd,
            timeout=args.timeout, env=env, stdin=sys.stdin.read() if args.stdin else '',
            request_id=ident, wait=0 if args.detach else 1))
        if args.detach:
            print(json.dumps(data, indent=2))
            return
    elif args.action == 'follow':
        ident = args.id
    else:
        method = {'cancel': 'POST', 'delete': 'DELETE'}.get(args.action, 'GET')
        endpoint = '/health' if args.action == 'health' else '/v1/jobs'
        if hasattr(args, 'id'): endpoint += '/' + args.id
        if args.action == 'cancel': endpoint += '/cancel'
        print(json.dumps(request(method, endpoint), indent=2))
        return
    offsets = {'stdout': 0, 'stderr': 0}
    while True:
        query = '&'.join(k + '_offset=' + str(v) for k, v in offsets.items())
        data = request('GET', '/v1/jobs/' + ident + '?' + query)
        for name, stream in (('stdout', sys.stdout.buffer), ('stderr', sys.stderr.buffer)):
            stream.write(base64.b64decode(data[name + '_base64']))
            stream.flush()
            offsets[name] = data[name + '_next']
        more = any(offsets[k] < data[k + '_size'] for k in offsets)
        if data['state'] not in ('queued', 'running') and not more:
            if data['state'] != 'finished':
                print('Job state: ' + data['state'], file=sys.stderr)
                raise SystemExit(1)
            code = data['exit_code']
            raise SystemExit(code if code >= 0 else 128 - code)
        if not more: time.sleep(.5)


if __name__ == '__main__':
    try: main()
    except KeyboardInterrupt:
        print('\nDetached; the server job continues. Use cancel JOB_ID to stop it.', file=sys.stderr)
        sys.exit(130)
