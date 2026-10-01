#!/usr/bin/env python3
"""Authenticated, tailnet-only command jobs. Python standard library only."""
import argparse
import base64
import hashlib
import hmac
import json
import os
from pathlib import Path
import pwd
import re
import selectors
import signal
import subprocess
import sys
import threading
import time
import uuid
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlsplit

MAX_BODY = 1024 * 1024
MAX_OUTPUT = 16 * 1024 * 1024  # per stream, per job
MAX_JOBS = 1000
MAX_RUNNING = 4
ID = re.compile(r"^[0-9a-f]{32}$")


class APIError(Exception):
    def __init__(self, status, message):
        self.status, self.message = status, message


class Jobs:
    def __init__(self, root):
        self.root = Path(root)
        self.root.mkdir(mode=0o700, parents=True, exist_ok=True)
        self.lock = threading.RLock()
        self.live = {}
        # systemd kills the service's complete cgroup on stop/restart.
        for path in self.root.glob('*/job.json'):
            data = json.loads(path.read_text())
            if data['state'] in ('queued', 'running'):
                data.update(state='interrupted', finished_at=time.time())
                self.save(data)

    def save(self, data):
        directory = self.root / data['id']
        temp = directory / 'job.tmp'
        temp.write_text(json.dumps(data))
        temp.replace(directory / 'job.json')

    def read(self, ident):
        if not ID.fullmatch(ident):
            raise APIError(404, 'Unknown job')
        try:
            return json.loads((self.root / ident / 'job.json').read_text())
        except FileNotFoundError:
            raise APIError(404, 'Unknown job')

    def start(self, request):
        allowed = {'command', 'cwd', 'env', 'stdin', 'timeout', 'wait', 'request_id'}
        if not isinstance(request, dict) or set(request) - allowed:
            raise APIError(400, 'Unknown request fields')
        command = request.get('command')
        cwd = request.get('cwd', '/srv/aegis')
        env = request.get('env', {})
        stdin = request.get('stdin', '')
        timeout = request.get('timeout', 3600)
        wait = request.get('wait', 0)
        ident = request.get('request_id', uuid.uuid4().hex)
        if not isinstance(command, str) or not command.strip() or '\0' in command:
            raise APIError(400, 'command must be a nonempty string without NUL')
        if not isinstance(cwd, str) or not os.path.isabs(cwd) or not os.path.isdir(cwd):
            raise APIError(400, 'cwd must be an existing absolute directory')
        if not isinstance(stdin, str):
            raise APIError(400, 'stdin must be a string')
        if not isinstance(env, dict) or any(not isinstance(k, str) or not re.fullmatch(r'[A-Za-z_][A-Za-z0-9_]*', k) or not isinstance(v, str) or '\0' in v for k, v in env.items()):
            raise APIError(400, 'env must contain valid environment names and string values')
        if type(timeout) not in (int, float) or not 0 < timeout <= 86400:
            raise APIError(400, 'timeout must be between 0 and 86400 seconds')
        if type(wait) not in (int, float) or not 0 <= wait <= 30:
            raise APIError(400, 'wait must be between 0 and 30 seconds')
        if not isinstance(ident, str) or not ID.fullmatch(ident):
            raise APIError(400, 'request_id must be 32 lowercase hexadecimal characters')
        fingerprint = hashlib.sha256(json.dumps([command, cwd, env, stdin, timeout], sort_keys=True).encode()).hexdigest()
        with self.lock:
            if (self.root / ident).exists():
                data = self.read(ident)
                if data['fingerprint'] != fingerprint:
                    raise APIError(409, 'request_id already used for a different command')
                return ident, wait
            if len(self.live) >= MAX_RUNNING:
                raise APIError(429, 'Concurrent job limit reached')
            if len(list(self.root.iterdir())) >= MAX_JOBS:
                raise APIError(507, 'Job storage full; delete completed jobs')
            (self.root / ident).mkdir(mode=0o700)
            data = dict(id=ident, command=command, cwd=cwd, timeout=timeout,
                        state='queued', created_at=time.time(), exit_code=None,
                        fingerprint=fingerprint)
            self.save(data)
            cancel, done = threading.Event(), threading.Event()
            self.live[ident] = (cancel, done)
            threading.Thread(target=self.run, args=(data, env, stdin, cancel, done), daemon=True).start()
        return ident, wait

    def run(self, data, extra_env, stdin, cancel, done):
        directory = self.root / data['id']
        account = pwd.getpwuid(os.getuid())
        env = dict(PATH='/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin',
                   HOME=account.pw_dir, USER=account.pw_name, LOGNAME=account.pw_name,
                   LANG='C.UTF-8', TERM='dumb', GIT_TERMINAL_PROMPT='0')
        env.update(extra_env)
        process = None
        reason = None
        try:
            # Unlinked input file avoids blocking on a large stdin pipe and is not retained.
            import tempfile
            with tempfile.TemporaryFile() as input_file, \
                    (directory / 'stdout').open('wb') as stdout, \
                    (directory / 'stderr').open('wb') as stderr:
                input_file.write(stdin.encode())
                input_file.seek(0)
                process = subprocess.Popen(['/bin/bash', '-o', 'pipefail', '-c', data['command']],
                                           cwd=data['cwd'], env=env, stdin=input_file,
                                           stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                           start_new_session=True)
                with self.lock:
                    data.update(state='running', started_at=time.time(), pid=process.pid)
                    self.save(data)
                deadline = time.monotonic() + data['timeout']
                with selectors.DefaultSelector() as selector:
                    for stream, target in ((process.stdout, stdout), (process.stderr, stderr)):
                        os.set_blocking(stream.fileno(), False)
                        selector.register(stream, selectors.EVENT_READ, target)
                    sizes = {stdout: 0, stderr: 0}
                    stopped_at = None
                    while selector.get_map():
                        if cancel.is_set():
                            reason = reason or 'cancelled'
                        if time.monotonic() >= deadline:
                            reason = reason or 'timed_out'
                        if reason or process.poll() is not None:
                            if stopped_at is None:
                                self.kill(process)
                                stopped_at = time.monotonic()
                            elif time.monotonic() - stopped_at > 2:
                                # Detached services may retain inherited pipes; never hang forever.
                                break
                        for key, _ in selector.select(0.1):
                            chunk = os.read(key.fileobj.fileno(), 65536)
                            if not chunk:
                                selector.unregister(key.fileobj)
                                key.fileobj.close()
                                continue
                            target = key.data
                            remaining = MAX_OUTPUT - sizes[target]
                            target.write(chunk[:remaining])
                            target.flush()
                            sizes[target] += min(len(chunk), remaining)
                            if len(chunk) > remaining:
                                reason = reason or 'output_limit'
                    # A command can close both streams while continuing to run.
                    while process.poll() is None:
                        if cancel.is_set(): reason = reason or 'cancelled'
                        if time.monotonic() >= deadline: reason = reason or 'timed_out'
                        if reason: self.kill(process)
                        time.sleep(0.05)
                data['exit_code'] = process.wait()
                data['state'] = reason or 'finished'
        except Exception as exc:
            data.update(state='failed', error=str(exc))
        finally:
            if process:
                self.kill(process)
                process.wait()
                process.stdout.close()
                process.stderr.close()
            with self.lock:
                data['finished_at'] = time.time()
                self.save(data)
                self.live.pop(data['id'], None)
                done.set()

    @staticmethod
    def kill(process):
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            return
        except PermissionError:
            pass
        if sys.platform.startswith('linux'):
            try:
                os.killpg(process.pid, 0)
            except ProcessLookupError:
                return
            except PermissionError:
                pass
            # Match this host's existing passwordless sudo capability for root children.
            subprocess.run(['/usr/bin/sudo', '-n', '/bin/kill', '-KILL', '--', '-' + str(process.pid)],
                           stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                           stderr=subprocess.DEVNULL, timeout=3, check=False)

    def result(self, ident, offsets=None):
        with self.lock:
            data = self.read(ident)
            data.pop('fingerprint', None)
            for name in ('stdout', 'stderr'):
                offset = (offsets or {}).get(name, 0)
                try:
                    with (self.root / ident / name).open('rb') as stream:
                        size = os.fstat(stream.fileno()).st_size
                        stream.seek(offset)
                        chunk = stream.read(65536)
                except FileNotFoundError:
                    size, chunk = 0, b''
                data[name] = chunk.decode('utf-8', errors='replace')
                data[name + '_base64'] = base64.b64encode(chunk).decode()
                data[name + '_next'] = offset + len(chunk)
                data[name + '_size'] = size
            return data

    def cancel(self, ident):
        with self.lock:
            self.read(ident)
            if ident in self.live:
                self.live[ident][0].set()

    def delete(self, ident):
        with self.lock:
            self.read(ident)
            if ident in self.live:
                raise APIError(409, 'Cancel and wait for job completion before deleting')
            import shutil
            shutil.rmtree(self.root / ident)


class Server(ThreadingHTTPServer):
    daemon_threads = True

    def __init__(self, address, jobs, password_hash):
        self.jobs, self.password_hash = jobs, password_hash
        self.slots = threading.BoundedSemaphore(32)
        super().__init__(address, Handler)

    def process_request(self, request, client_address):
        if not self.slots.acquire(blocking=False):
            request.close()
            return
        try:
            super().process_request(request, client_address)
        except Exception:
            self.slots.release()
            raise

    def process_request_thread(self, request, client_address):
        try:
            super().process_request_thread(request, client_address)
        finally:
            self.slots.release()


class Handler(BaseHTTPRequestHandler):
    server_version = 'AegisBuildHTTP/1'

    def setup(self):
        super().setup()
        self.connection.settimeout(40)

    def log_message(self, fmt, *args):
        # Never log commands, request bodies, query strings or authentication headers.
        pass

    def reply(self, status, data):
        body = json.dumps(data, allow_nan=False).encode()
        self.send_response(status)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(body)))
        self.send_header('Cache-Control', 'no-store')
        self.send_header('X-Content-Type-Options', 'nosniff')
        if status == 401:
            self.send_header('WWW-Authenticate', 'Basic realm="AEGIS build", charset="UTF-8"')
        self.end_headers()
        self.wfile.write(body)

    def authenticate(self):
        header = self.headers.get('Authorization', '')
        password = ''
        if header.startswith('Bearer '):
            password = header[7:]
        elif header.startswith('Basic '):
            try:
                user, password = base64.b64decode(header[6:], validate=True).decode().split(':', 1)
                if user != 'codex': password = ''
            except (ValueError, UnicodeError):
                pass
        actual = hashlib.sha256(password.encode()).hexdigest()
        if not password or not hmac.compare_digest(actual, self.server.password_hash):
            raise APIError(401, 'Authentication required')
        if self.headers.get('Origin'):
            raise APIError(403, 'Browser-origin requests are not supported')

    def body(self):
        if self.headers.get('Transfer-Encoding'):
            raise APIError(400, 'Transfer-Encoding not supported')
        if self.headers.get_content_type() != 'application/json':
            raise APIError(415, 'Content-Type must be application/json')
        try: length = int(self.headers.get('Content-Length', '-1'))
        except ValueError: raise APIError(400, 'Invalid Content-Length')
        if not 0 <= length <= MAX_BODY:
            raise APIError(413, 'Body must be at most 1 MiB and include Content-Length')
        raw = self.rfile.read(length)
        if len(raw) != length:
            raise APIError(400, 'Incomplete body')
        try: return json.loads(raw)
        except (ValueError, UnicodeError): raise APIError(400, 'Invalid JSON')

    def dispatch(self):
        try:
            self.authenticate()
            url = urlsplit(self.path)
            parts = url.path.strip('/').split('/')
            jobs = self.server.jobs
            if self.command == 'GET' and url.path == '/health':
                return self.reply(200, {'status': 'ok', 'api': 1})
            if parts[:2] != ['v1', 'jobs']:
                raise APIError(404, 'Unknown endpoint')
            if len(parts) == 2:
                if self.command == 'POST':
                    ident, wait = jobs.start(self.body())
                    with jobs.lock:
                        live = jobs.live.get(ident)
                    if live: live[1].wait(wait)
                    data = jobs.result(ident)
                    return self.reply(202 if data['state'] in ('queued', 'running') else 200, data)
                if self.command == 'GET':
                    with jobs.lock:
                        records = [json.loads(p.read_text()) for p in jobs.root.glob('*/job.json')]
                    records.sort(key=lambda d: d['created_at'], reverse=True)
                    return self.reply(200, {'jobs': [{k: d.get(k) for k in ('id', 'state', 'created_at', 'exit_code')} for d in records]})
            elif len(parts) == 3:
                ident = parts[2]
                if self.command == 'GET':
                    query = parse_qs(url.query)
                    try:
                        offsets = {name: int(query.get(name + '_offset', ['0'])[0]) for name in ('stdout', 'stderr')}
                        if any(v < 0 or v > MAX_OUTPUT for v in offsets.values()): raise ValueError()
                    except ValueError:
                        raise APIError(400, 'Invalid output offset')
                    return self.reply(200, jobs.result(ident, offsets))
                if self.command == 'DELETE':
                    jobs.delete(ident)
                    return self.reply(200, {'deleted': ident})
            elif len(parts) == 4 and parts[3] == 'cancel' and self.command == 'POST':
                jobs.cancel(parts[2])
                return self.reply(202, jobs.result(parts[2]))
            raise APIError(405, 'Unsupported method or endpoint')
        except APIError as exc:
            self.reply(exc.status, {'error': exc.message})
        except (BrokenPipeError, ConnectionResetError, TimeoutError):
            pass
        except Exception:
            self.reply(500, {'error': 'Internal server error'})

    do_GET = do_POST = do_DELETE = dispatch


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--config', required=True)
    args = parser.parse_args()
    config = json.loads(Path(args.config).read_text())
    # No wildcard/LAN binding: public deployment needs a deliberate separate design.
    import ipaddress
    address = ipaddress.ip_address(config['bind'])
    if address.version != 4 or not (address.is_loopback or address in ipaddress.ip_network('100.64.0.0/10')):
        raise SystemExit('Bind must be loopback or a Tailscale IPv4 address')
    if not re.fullmatch('[0-9a-f]{64}', config['password_sha256']):
        raise SystemExit('Invalid password hash')
    os.umask(0o077)
    Server((str(address), config['port']), Jobs(config['state_dir']), config['password_sha256']).serve_forever()


if __name__ == '__main__':
    main()
